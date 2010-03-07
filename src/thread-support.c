
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
for each thread, composed of a size and an array of void*:

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

An Orb_tls_s is composed of an index and a link to another
Orb_tls_s.  Globally, there exists a list of free struct
Orb_tls_s.  This global list is accessed via an Orb_cell_t slot.

When the free list is exhausted, we simply allocate a bunch of
struct Orb_tls_s and add them to the freelist.  Their indices
are initialized to the current maxindex, incremented each time.
The new maxindex is attached to the freelist, and then we proceed
as normal.

When an Orb_tls_s is accessed, we get the index from it, then
perform the actual access to the array for each thread.  If the
array is too small, we just enlarge it.  If it doesn't exist yet,
we create it.

*/

static pthread_key_t the_thread_local_key;
static Orb_t the_tls_freelist;

struct Orb_tls_s {
	size_t index;
	Orb_tls_t next;
};

static void** get_tls(Orb_tls_t tls_obj) {
	size_t i = tls_obj->index;

	Orb_t* tls_info = pthread_getspecific(the_thread_local_key);

	size_t size; /*tls_info[0]*/
	void** data; /*tls_info[1]*/

	if(tls_info == 0) {
		size = i + 1;
		data = Orb_gc_malloc(size * sizeof(void*));
		tls_info = Orb_gc_malloc(2 * sizeof(Orb_t));
		Orb_gc_defglobals(tls_info, 2);
		tls_info[0] = Orb_t_from_integer(size);
		tls_info[1] = Orb_t_from_pointer(data);
		pthread_setspecific(the_thread_local_key, tls_info);
	} else {
		size = Orb_t_as_integer(tls_info[0]);
		data = Orb_t_as_pointer(tls_info[1]);
		if(size <= i) {
			/*realloc*/
			void** ndata = Orb_gc_malloc((i + 1) * sizeof(void*));
			memcpy(ndata, data, size * sizeof(void*));
			data = ndata;
			size = i + 1;
			tls_info[0] = Orb_t_from_integer(size);
			tls_info[1] = Orb_t_from_pointer(data);
		}
	}
	return &data[i];
}

void* Orb_tls_get(Orb_tls_t tls_obj) {
	return *get_tls(tls_obj);
}
void Orb_tls_set(Orb_tls_t tls_obj, void* p) {
	*get_tls(tls_obj) = p;
}

struct tls_freelist_s {
	struct Orb_tls_s* freelist;
	size_t maxindex;
};

#define TLS_BATCH_ALLOC 8

static void individual_tls_finalizer(void*, void*);

Orb_tls_t Orb_tls_init(void) {
	Orb_cell_t freelistcell = Orb_t_as_pointer(the_tls_freelist);
	Orb_t ofreelistdata = Orb_cell_get(freelistcell);

	struct tls_freelist_s* nfreelistdata = Orb_gc_malloc(
		sizeof(struct tls_freelist_s)
	);
	Orb_t onfreelistdata = Orb_t_from_pointer(nfreelistdata);

	struct tls_freelist_s* freelistdata;
	Orb_tls_t rv;
	Orb_t check;

top:
	freelistdata = Orb_t_as_pointer(ofreelistdata);
	rv = freelistdata->freelist;
	if(rv == 0) {
		rv = Orb_gc_malloc(TLS_BATCH_ALLOC * sizeof(struct Orb_tls_s));
		size_t i;
		size_t maxindex = freelistdata->maxindex;
		/*set up structurres and add finalizers*/
		for(i = 0; i < TLS_BATCH_ALLOC; ++i) {
			rv[i].index = maxindex + i;
			Orb_gc_deffinalizer(
				&rv[i],
				&individual_tls_finalizer,
				0
			);
		}
		/*set up list structure*/
		for(i = 0; i < TLS_BATCH_ALLOC - 1; ++i) {
			rv[i].next = &rv[i + 1];
		}
		nfreelistdata->maxindex = maxindex + TLS_BATCH_ALLOC;
	} else {
		nfreelistdata->maxindex = freelistdata->maxindex;
	}
	nfreelistdata->freelist = rv->next;
	check = Orb_cell_cas_get(freelistcell, ofreelistdata, onfreelistdata);
	if(check != ofreelistdata) {
		ofreelistdata = check;
		goto top;
	}

	rv->next = 0;

	return rv;
}

static void individual_tls_finalizer(void* vptls, void* _ignored_) {
	Orb_tls_t tls = vptls;

	Orb_cell_t freelistcell = Orb_t_as_pointer(the_tls_freelist);
	Orb_t ofreelistdata = Orb_cell_get(freelistcell);

	struct tls_freelist_s* nfreelistdata = Orb_gc_malloc(
		sizeof(struct tls_freelist_s)
	);
	Orb_t onfreelistdata = Orb_t_from_pointer(nfreelistdata);

	struct tls_freelist_s* freelistdata;
	Orb_t check;

top:
	freelistdata = Orb_t_as_pointer(ofreelistdata);

	tls->next = freelistdata->freelist;

	nfreelistdata->freelist = tls;
	nfreelistdata->maxindex = freelistdata->maxindex;

	check = Orb_cell_cas_get(freelistcell, ofreelistdata, onfreelistdata);
	if(check != ofreelistdata) {
		ofreelistdata = check;
		goto top;
	}
}

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

	Orb_gc_defglobal(&the_tls_freelist);

	struct tls_freelist_s* nfreelist = Orb_gc_malloc(
		sizeof(struct tls_freelist_s)
	);
	nfreelist->freelist = 0;
	nfreelist->maxindex = 0;

	Orb_cell_t freelistcell = Orb_cell_init(
		Orb_t_from_pointer(nfreelist)
	);
	the_tls_freelist = Orb_t_from_pointer(freelistcell);
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
	Orb_autoclear_on_finalize(rv, &rv->core);
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

