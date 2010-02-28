
#include"liborb.h"

#define GC_THREADS
#include<gc/gc.h>

void Orb_gc_only_init(void) {
	GC_INIT();
	GC_all_interior_pointers = 0;
	GC_REGISTER_DISPLACEMENT(3);
}

void* Orb_gc_malloc(size_t sz) {
	void* rv = GC_MALLOC(sz);
	if(rv == 0) {
		Orb_THROW_cc("alloc", "out of memory!");
	}
	return rv;
}
void* Orb_gc_malloc_pointerfree(size_t sz) {
	void* rv = GC_MALLOC_ATOMIC(sz);
	if(rv == 0) {
		Orb_THROW_cc("alloc", "out of memory!");
	}
	return rv;
}
void Orb_gc_free(void* p) {
	GC_FREE(p);
}

void Orb_gc_defglobals(Orb_t* p, size_t n) {
	GC_add_roots((void*)p, (void*)p + n);
}
void Orb_gc_undefglobals(Orb_t* p, size_t n) {
	GC_remove_roots((void*)p, (void*)p + n);
}

