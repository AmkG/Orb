
#include"liborb.h"
#include"thread-support.h"

#include<stdlib.h>

#define GC_THREADS
#define GC_PTHREADS
#include<gc/gc.h>



#ifndef HAVE_SOME_CAS

	/*default implementation if no CAS available*/
	#define CAS_MUTEXES 16
	static pthread_mutex_t cas_mutexes[CAS_MUTEXES];

	/*CAS implementations should #define this into nothing*/
	static void cas_init(void) {
		size_t i;
		for(i = 0; i < CAS_MUTEXES; ++i) {
			pthread_mutex_init(&cas_mutexes[i], 0);
		}
	}

	static Orb_t cas(Orb_t* loc, Orb_t old, Orb_t newval) {
		size_t i = (size_t) loc;
		i = i >> 4;
		i = i % CAS_MUTEXES;
		pthread_mutex_lock(&cas_mutexes[i]);
		Orb_t curv = *loc;
		if(curv == old) *loc = newval;
		pthread_mutex_unlock(&cas_mutexes[i]);
		return curv;
	}
	static Orb_t safe_read(Orb_t* loc) {
		size_t i = (size_t) loc;
		i = i >> 4;
		i = i % CAS_MUTEXES;
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

