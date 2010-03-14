
#include"liborb.h"
#include"thread-support.h"

#define GC_THREADS
#define GC_PTHREADS
#include<gc/gc.h>

#include<stdio.h>
#include<stdlib.h>
#include<semaphore.h>
#include<errno.h>

#include<string.h>

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

Orb_tls_t Orb_tls_init(void) {
	size_t rv_index;
	size_t rv_version;

	/*TODO: consider blocking signals*/
	pthread_mutex_lock(&tls_freelist_lock);

	if(tls_size == 0) {
		/*need to refill*/
		if(tls_capacity < TLS_BATCH_ALLOC) {
			free(tls_freelist);
			free(tls_versions);
			tls_freelist = malloc(TLS_BATCH_ALLOC * sizeof(size_t));
			tls_versions = malloc(TLS_BATCH_ALLOC * sizeof(size_t));
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

	/*alloc a new Orb_tls_t*/
	Orb_tls_t rv = Orb_gc_malloc(sizeof(struct Orb_tls_s));
	rv->index = rv_index;
	rv->version = rv_version;
	/*setup finalizer*/
	Orb_gc_deffinalizer(rv, &individual_tls_finalizer, 0);

	return rv;
}

static void individual_tls_finalizer(void* vptls, void* _ignored_) {
	/*TODO: consider blocking signals*/
	pthread_mutex_lock(&tls_freelist_lock);

	/*check if full to capacity*/
	if(tls_size == tls_capacity) {
		/*realloc*/
		size_t ncapacity = tls_capacity + TLS_BATCH_ALLOC;
		size_t* nfreelist = malloc(ncapacity * sizeof(size_t));
		size_t* nversions = malloc(ncapacity * sizeof(size_t));
		memcpy(nfreelist, tls_freelist, tls_capacity * sizeof(size_t));
		memcpy(nversions, tls_versions, tls_capacity * sizeof(size_t));
		tls_freelist = nfreelist;
		tls_versions = nversions;
		tls_capacity = ncapacity;
	}

	Orb_tls_t tls = vptls;
	tls_freelist[tls_size] = tls->index;
	tls_versions[tls_size] = tls->version + 1; /*increment version*/
	++tls_size;

	pthread_mutex_unlock(&tls_freelist_lock);
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
		size_t i = (size_t) loc;
		i = i >> 4;
		i = i % N_CAS_MUTEXES;
		/*TODO: consider blocking signals*/
		pthread_mutex_lock(&cas_mutexes[i]);
		Orb_t curv = *loc;
		if(curv == old) *loc = newval;
		pthread_mutex_unlock(&cas_mutexes[i]);
		return curv;
	}
	static Orb_t safe_read(Orb_t* loc) {
		size_t i = (size_t) loc;
		i = i >> 4;
		i = i % N_CAS_MUTEXES;
		pthread_mutex_lock(&cas_mutexes[i]);
		Orb_t curv = *loc;
		pthread_mutex_unlock(&cas_mutexes[i]);
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
	Orb_gc_autoclear_on_finalize(rv, &rv->core);
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

void Orb_thread_support_init(void) {
	/*init order is sensitive!*/
	cas_init();
	tls_init();
}

