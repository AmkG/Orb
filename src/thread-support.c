
#include"liborb.h"
#include"thread-support.h"

#define GC_THREADS
#define GC_PTHREADS
#include<gc/gc.h>

#include<stdio.h>
#include<stdlib.h>
#include<semaphore.h>
#include<errno.h>
#include<unistd.h>

#include<string.h>

#include<assert.h>

/*
 * Thread-local
 */
/*
A bit of a problem here: we want thread-locals to be considered as
GC roots.  But we don't know if the GC will consider the thread-locals
provided by pthreads as roots.

Worse, we want thread-locals to be garbage collected also.

We get around this by allocating a single "thread-local info block"
for each thread, composed of a size and an array of slots:

   +----+  +--+--+--+--+--+--+
T1 |size|  |  |  |  |  |  |  |...
   +----+  +--+--+--+--+--+--+
   +----+  +--+--+--+--+--+--+
T2 |size|  |  |  |  |  |  |  |...
   +----+  +--+--+--+--+--+--+
   +----+  +--+--+--+--+--+--+
T3 |size|  |  |  |  |  |  |  |...
   +----+  +--+--+--+--+--+--+

Each thread keeps track of this via a single pthreads thread-local.

Each slot (struct tls_slot_s) in the array is composed of a void*
and a version.  The version is used to differentiate slot indices
between the current Orb_tls_t, and a previously garbage-collected
one.

An Orb_tls_s is composed of an index and a version.  Globally,
there exists a list of free struct Orb_tls_s.  This global list is
protected by a lock.

When the free list is exhausted, we simply allocate a bunch of
indices and add them to the freelist.  The new indices are given
a version of 1.

When an Orb_tls_s is accessed, we get the index from it, then
perform the actual access to the array for each thread.  If the
array is too small, we just enlarge it.  If it doesn't exist yet,
we create it.

Finalizing an Orb_tls_s simply returns its index to the freelist,
with an incremented version.

*/

static pthread_key_t the_thread_local_key;

struct Orb_tls_s {
	size_t index;
	size_t version;
};

struct tls_slot_s {
	void* value;
	size_t version;
};

/*
 * Determines the adress of the value slot for the TLS pointed to
 * by the tls_obj
 */
static void** get_tls(Orb_tls_t tls_obj) {
	size_t i = tls_obj->index;

	Orb_t* tls_info = pthread_getspecific(the_thread_local_key);

	size_t size; /*tls_info[0]*/
	struct tls_slot_s* data; /*tls_info[1]*/

	if(tls_info == 0) {
		size = i + 1;
		data = Orb_gc_malloc(size * sizeof(struct tls_slot_s));
		tls_info = malloc(2 * sizeof(Orb_t));
		Orb_gc_defglobals(tls_info, 2);
		tls_info[0] = Orb_t_from_integer(size);
		tls_info[1] = Orb_t_from_pointer(data);
		pthread_setspecific(the_thread_local_key, tls_info);
	} else {
		size = Orb_t_as_integer(tls_info[0]);
		data = Orb_t_as_pointer(tls_info[1]);
		if(size <= i) {
			/*realloc*/
			struct tls_slot_s* ndata = Orb_gc_malloc((i + 1) * sizeof(struct tls_slot_s));
			memcpy(ndata, data, size * sizeof(struct tls_slot_s));
			data = ndata;
			size = i + 1;
			tls_info[0] = Orb_t_from_integer(size);
			tls_info[1] = Orb_t_from_pointer(data);
		}
	}
	if(data[i].version != tls_obj->version) {
		/*the index was reused after being finalized, so
		clear our slot.
		*/
		data[i].version = tls_obj->version;
		data[i].value = 0;
	}
	return &data[i].value;
}

void* Orb_tls_get(Orb_tls_t tls_obj) {
	return *get_tls(tls_obj);
}
void Orb_tls_set(Orb_tls_t tls_obj, void* p) {
	*get_tls(tls_obj) = p;
}

#define TLS_BATCH_ALLOC 8
static pthread_mutex_t tls_freelist_lock = PTHREAD_MUTEX_INITIALIZER;
static size_t* tls_freelist = 0;
static size_t* tls_versions = 0;
static size_t tls_size = 0;
static size_t tls_capacity = 0;
static size_t tls_maxindex = 0;

static void individual_tls_finalizer(void*, void*);

/*
 * Signal blocking facility
 *
 * In POSIX systems, Boehm GC uses signals to stop the world.
 * In many cases we don't want the GC to stop the world while
 * we are doing something, like holding a lock that a signal
 * handler might use.
 */
#define BLOCK_SIGNALS_DECL sigset_t Orb__priv_oldmask
#define BLOCK_SIGNALS\
	do {\
		sigset_t curmask;\
		sigfillset(&curmask);\
		/* Don't block SIGSEGV and SIGBUS*/ \
		sigdelset(&curmask, SIGSEGV);\
		sigdelset(&curmask, SIGBUS);\
		pthread_sigmask(SIG_BLOCK, &curmask, &Orb__priv_oldmask);\
	} while(0)
#define UNBLOCK_SIGNALS\
	do {\
		pthread_sigmask(SIG_SETMASK, &Orb__priv_oldmask, 0);\
	} while(0)

Orb_tls_t Orb_tls_init(void) {
	size_t rv_index;
	size_t rv_version;
	BLOCK_SIGNALS_DECL;

	BLOCK_SIGNALS;
	pthread_mutex_lock(&tls_freelist_lock);

	if(tls_size == 0) {
		/*need to refill*/
		if(tls_capacity < TLS_BATCH_ALLOC) {
			free(tls_freelist);
			free(tls_versions);
			tls_freelist = malloc(TLS_BATCH_ALLOC * sizeof(size_t));
			tls_versions = malloc(TLS_BATCH_ALLOC * sizeof(size_t));
			if(tls_freelist == 0 || tls_versions == 0) {
				free(tls_freelist); free(tls_versions);
				tls_freelist = 0; tls_versions = 0;

				/*unlock and unblock*/
				pthread_mutex_unlock(&tls_freelist_lock);
				UNBLOCK_SIGNALS;
				Orb_THROW_cc("new-tls", "Out of memory");
			}
		}
		size_t i;
		for(i = 0; i < TLS_BATCH_ALLOC; ++i) {
			/*fill indices in reverse because the freelist is
			used as a stack that grows upward.
			*/
			tls_freelist[i] = tls_maxindex + TLS_BATCH_ALLOC - i - 1;
			tls_versions[i] = 1;
		}
		tls_maxindex += TLS_BATCH_ALLOC;
		tls_size = TLS_BATCH_ALLOC;
	}

	rv_index = tls_freelist[tls_size - 1];
	rv_version = tls_versions[tls_size - 1];
	--tls_size;

	/*unlock *before* allocing
	The tls_freelist_lock is acquired by the finalizer for
	Orb_tls_t.  These finalizers might be called while the
	GC has acquired its own allocation lock, so we have
	to free the tls_freelist_lock to prevent deadlocks with
	the GC's lock.
	*/
	pthread_mutex_unlock(&tls_freelist_lock);
	UNBLOCK_SIGNALS;

	/*alloc a new Orb_tls_t*/
	Orb_tls_t rv = Orb_gc_malloc(sizeof(struct Orb_tls_s));
	rv->index = rv_index;
	rv->version = rv_version;
	/*setup finalizer*/
	Orb_gc_deffinalizer(rv, &individual_tls_finalizer, 0);

	return rv;
}

static void individual_tls_finalizer(void* vptls, void* _ignored_) {
	BLOCK_SIGNALS_DECL;

	BLOCK_SIGNALS;
	pthread_mutex_lock(&tls_freelist_lock);

	/*check if full to capacity*/
	if(tls_size == tls_capacity) {
		/*realloc*/
		size_t ncapacity = tls_capacity + TLS_BATCH_ALLOC;
		size_t* nfreelist = malloc(ncapacity * sizeof(size_t));
		size_t* nversions = malloc(ncapacity * sizeof(size_t));
		if(nfreelist == 0 || nversions == 0) {
			/*just let this index number drop T.T*/
			free(tls_freelist);
			free(tls_versions);
			goto end;
		}
		memcpy(nfreelist, tls_freelist, tls_capacity * sizeof(size_t));
		memcpy(nversions, tls_versions, tls_capacity * sizeof(size_t));

		free(tls_freelist);
		free(tls_versions);

		tls_freelist = nfreelist;
		tls_versions = nversions;
		tls_capacity = ncapacity;
	}

	Orb_tls_t tls = vptls;
	tls_freelist[tls_size] = tls->index;
	tls_versions[tls_size] = tls->version + 1; /*increment version*/
	++tls_size;

end:
	pthread_mutex_unlock(&tls_freelist_lock);
	UNBLOCK_SIGNALS;
}

/*
 * Called when a thread terminates and needs to clean up its
 * thread-local slots
 */
static void thread_local_cleanup(void* tlsdata) {
	/*undefine from globality*/
	if(tlsdata) {
		Orb_gc_undefglobals(tlsdata, 2);
		free(tlsdata);
	}
}

static void tls_init(void) {
	if(pthread_key_create(&the_thread_local_key, &thread_local_cleanup) != 0) {
		fprintf(stderr, "Unable to initialize thread-locals");
		exit(1);
	}
}


/*
 * Semaphores
 */
struct Orb_sema_s {
	sem_t core;
};

static void sema_finalizer(void* vsema, void* _ignore_) {
	Orb_sema_t sema = (Orb_sema_t) vsema;
	sem_destroy(&sema->core);
}

Orb_sema_t Orb_sema_init(unsigned int init) {
	Orb_sema_t rv = Orb_gc_malloc(sizeof(struct Orb_sema_s));
	if(sem_init(&rv->core, 0, init) != 0) {
		Orb_THROW_cc("sema", "Unable to initialize semaphore");
	}
	Orb_gc_deffinalizer(rv, &sema_finalizer, 0);
	return rv;
}

unsigned int Orb_sema_get(Orb_sema_t sema) {
	int rv;
	sem_getvalue(&sema->core, &rv);
	return rv;
}
static int wrap_sem_trywait(sem_t* sp) {
	int rv;
	do {
		errno = 0;
		rv = sem_trywait(sp);
	} while(rv != 0 && errno == EINTR);
	return rv;
}
static void wrap_sem_wait(sem_t* sp) {
	int rv;
	do {
		errno = 0;
		rv = sem_wait(sp);
	} while(rv != 0 && errno == EINTR);
}
void Orb_sema_wait(Orb_sema_t sema) {
	do {
		if(0 == wrap_sem_trywait(&sema->core)) {
			return;
		}
	} while(GC_collect_a_little());
	wrap_sem_wait(&sema->core);
}
void Orb_sema_post(Orb_sema_t sema) {
	sem_post(&sema->core);
}

#ifndef HAVE_SOME_CAS

	/*default implementation if no CAS available*/
	#define N_CAS_MUTEXES 16
	static pthread_mutex_t cas_mutexes[N_CAS_MUTEXES];

	/*CAS implementations should #define this into nothing*/
	static void cas_init(void) {
		size_t i;
		for(i = 0; i < N_CAS_MUTEXES; ++i) {
			pthread_mutex_init(&cas_mutexes[i], 0);
		}
	}

	static Orb_t cas(Orb_t* loc, Orb_t old, Orb_t newval) {
		BLOCK_SIGNALS_DECL;
		size_t i = (size_t) loc;
		i = i >> 4;
		i = i % N_CAS_MUTEXES;

		BLOCK_SIGNALS;
		pthread_mutex_lock(&cas_mutexes[i]);

		Orb_t curv = *loc;
		if(curv == old) *loc = newval;

		pthread_mutex_unlock(&cas_mutexes[i]);
		UNBLOCK_SIGNALS;

		return curv;
	}
	static Orb_t safe_read(Orb_t* loc) {
		BLOCK_SIGNALS_DECL;
		size_t i = (size_t) loc;
		i = i >> 4;
		i = i % N_CAS_MUTEXES;

		BLOCK_SIGNALS;
		pthread_mutex_lock(&cas_mutexes[i]);

		Orb_t curv = *loc;

		pthread_mutex_unlock(&cas_mutexes[i]);
		UNBLOCK_SIGNALS;

		return curv;
	}
#endif /*not HAVE_SOME_CAS*/

/*
 * Atomic Cells
 */

struct Orb_cell_s {
	Orb_t core;
};

Orb_cell_t Orb_cell_init(Orb_t init) {
	Orb_cell_t rv = Orb_gc_malloc(sizeof(struct Orb_cell_s));
	/*set its value*/
	Orb_cell_set(rv, init);
	/*In the typical case, the only way for cycles to occur
	in Orb is if a cell is modified to point indirectly
	to itself.  Thus, by having a cell automatically break
	references prior to being finalized, we can break
	the only cycles that can be formed just from standard
	Orb.
	*/
	Orb_gc_autoclear_on_finalize(rv, (void**) &rv->core);
	return rv;
}
Orb_t Orb_cell_get(Orb_cell_t c) {
	return safe_read(&c->core);
}
void Orb_cell_set(Orb_cell_t c, Orb_t val) {
	Orb_t curv, oldv;
	oldv = safe_read(&c->core);
	do {
		curv = oldv;
	} while(curv != (oldv = cas(&c->core, oldv, val)));
}
Orb_t Orb_cell_cas_get(Orb_cell_t c, Orb_t old, Orb_t newv) {
	return cas(&c->core, old, newv);
}

/*
 * C Extension Lock
 */
pthread_mutex_t the_CEL = PTHREAD_MUTEX_INITIALIZER;
Orb_t oflag_tls;

static void cel_init(void) {
	Orb_gc_defglobal(&oflag_tls);
	oflag_tls = Orb_t_from_pointer(Orb_tls_init());
}

int Orb_CEL_havelock(void) {
	Orb_tls_t flag_tls = Orb_t_as_pointer(oflag_tls);
	void* pp = Orb_tls_get(flag_tls);
	return (intptr_t) pp;
}

void Orb_CEL_lock(void) {
	pthread_mutex_lock(&the_CEL);
	Orb_tls_t flag_tls = Orb_t_as_pointer(oflag_tls);
	Orb_tls_set(flag_tls, (void*) 1);
}
void Orb_CEL_unlock(void) {
	Orb_tls_t flag_tls = Orb_t_as_pointer(oflag_tls);
	Orb_tls_set(flag_tls, (void*) 0);
	pthread_mutex_unlock(&the_CEL);
}

void Orb_thread_support_init(void) {
	/*init order is sensitive!*/
	cas_init();
	tls_init();
	cel_init();
}

/*
 * Number of processors
 */
size_t Orb_num_processors(void) {
	/*TODO: make more portable*/
	return sysconf(_SC_NPROCESSORS_ONLN);
}

/*
 * Launch a new thread
 */
struct Orb_thread_s {
	Orb_cell_t cstate;
	pthread_t tid;
};
struct threadstate_s {
	enum {
		running,
		finished
	} state;
	Orb_t ob; /*== f when running, return value when finished*/
};
typedef struct threadstate_s* threadstate_t;

static void* new_thread(void*);

Orb_thread_t Orb_priv_new_thread(Orb_t f) {
	threadstate_t ts = Orb_gc_malloc(sizeof(struct threadstate_s));
	ts->state = running;
	ts->ob = f;
	Orb_thread_t rv = Orb_gc_malloc(sizeof(struct Orb_thread_s));
	rv->cstate = Orb_cell_init(Orb_t_from_pointer(ts));

	int err = pthread_create(&rv->tid, 0, new_thread, rv);
	if(err != 0) {
		/*TODO get error message from system*/
		Orb_THROW_cc("thread", "Error launching thread");
	}
	return rv;
}

static void* new_thread(void* vpt) {
	Orb_thread_t pt = vpt;
	Orb_t ots = Orb_cell_get(pt->cstate);

	threadstate_t ts = Orb_t_as_pointer(ots);
	assert(ts->state == running);
	Orb_t f = ts->ob;

	Orb_t rv = Orb_call0(f);

	threadstate_t npts = Orb_gc_malloc(sizeof(struct threadstate_s));
	npts->state = finished;
	npts->ob = rv;

	Orb_cell_set(pt->cstate, Orb_t_from_pointer(npts));

	return 0;
}

/*
 *  Yield processor time
 */
void Orb_yield(void) {
	sched_yield();
}

