/*
Copyright 2010 Alan Manuel K. Gloria

This file is part of Orb C Implementation

Orb C Implementation is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Orb C Implementation is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Orb C Implementation.  If not, see <http://www.gnu.org/licenses/>.
*/

#include"liborb.h"

#define GC_THREADS
#include<gc/gc.h>

void Orb_gc_only_init(void) {
	GC_INIT();
	GC_all_interior_pointers = 0;

	/*need to register each possible displacement*/
	GC_REGISTER_DISPLACEMENT(3);
	GC_REGISTER_DISPLACEMENT(2);
	GC_REGISTER_DISPLACEMENT(1);
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

struct findat {
	void (*finalizer)(void*, void*);
	void* client_data;
};
static void my_finalizer(GC_PTR obj, GC_PTR tofindat) {
	struct findat* pfindat = (struct findat*) (void*) tofindat;
	pfindat->finalizer((void*) obj, pfindat->client_data);
}
void Orb_gc_deffinalizer(
		void* area,
		void (*finalizer)(void*, void*),
		void* client_data) {
	struct findat* dat = GC_MALLOC(sizeof(struct findat));
	dat->finalizer = finalizer;
	dat->client_data = client_data;
	GC_REGISTER_FINALIZER_IGNORE_SELF(
		(GC_PTR) area, &my_finalizer, (GC_PTR) dat,
		0, 0
	);
}
void Orb_gc_undeffinalizer(void* area) {
	GC_REGISTER_FINALIZER_IGNORE_SELF((GC_PTR) area, 0, 0, 0, 0);
}
void Orb_gc_autoclear_on_finalize(void* area, void** area_pointer) {
	GC_GENERAL_REGISTER_DISAPPEARING_LINK(
		(GC_PTR) area, (GC_PTR) area_pointer
	);
}

void Orb_gc_trigger(void) {
	GC_gcollect();
}

