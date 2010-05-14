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

#include"seq.h"
#include"list.h"

/*hidden fields*/
static Orb_t hfield1;
static Orb_t hfield2;

/*function to apply f onto i*/
static Orb_t o_apply_fi;
static Orb_t apply_fi_cfunc(Orb_t argv[], size_t* pargc, size_t argl) {
	if(*pargc != 1) {
		Orb_THROW_cc("apply",
			"Incorrect number of arguments to deferred "
			"apply function, expected 1"
		);
	}

	Orb_t self = argv[0];
	Orb_t f = Orb_deref(self, hfield1);
	Orb_t i = Orb_deref(self, hfield2);

	if(argl < 2) {
		return Orb_call1(f, i);
	} else {
		argv[0] = f;
		argv[1] = i;
		*pargc = 2;
		return Orb_TRAMPOLINE;
	}
}

/*maps an array onto an array.  Used by map_arr() below*/
static Orb_t map_arr_core(Orb_t narr[],
			Orb_t const arr[], size_t start, size_t sz,
			Orb_t f) {
	size_t i;
	/*prepare base*/
	Orb_t base;
	Orb_BUILDER {
		Orb_B_PARENT(o_apply_fi);
		Orb_B_FIELD(hfield1, f);
	} base = Orb_ENDBUILDER;
	/*perform deferrals in forward order*/
	for(i = 0; i < sz; ++i) {
		Orb_t f0;
		Orb_BUILDER {
			Orb_B_PARENT(base);
			Orb_B_FIELD(hfield2, arr[start + i]);
		} f0 = Orb_ENDBUILDER;
		narr[i] = Orb_defer(f0);
	}
	/*now commit defers in reverse order*/
	for(i = sz; i != 0; --i) {
		narr[i - 1] = Orb_call0(narr[i - 1]);
	}
	return Orb_seq(narr, sz);
}

/*maps an array*/
static Orb_t map_arr(Orb_t const arr[], size_t start, size_t sz, Orb_t f) {
	if(sz == 0) return Orb_seq(0, 0);
	if(sz == 1) {
		Orb_t rv = Orb_call1(f, arr[start]);
		return Orb_seq(&rv, 1);
	}
	/*As much as possible use small stack arrays*/
	switch(sz) {
#		define SIZE_CASE(N)\
		case N: {Orb_t narr[N];\
			return map_arr_core(narr, arr, start, sz, f);\
		} break;
	SIZE_CASE(2);
	SIZE_CASE(3);
	SIZE_CASE(4);
	SIZE_CASE(5);
	SIZE_CASE(6);
	SIZE_CASE(7);
	SIZE_CASE(8);
#		undef SIZE_CASE
	default: {
		Orb_t* narr = Orb_gc_malloc(sz * sizeof(Orb_t));
		Orb_t rv = map_arr_core(narr, arr, start, sz, f);
		Orb_gc_free(narr);
		return rv;
	} break;
	}
}

/*function to perform deferred core mapping*/
static Orb_t o_map_core_sf;

/*
 * core mapping function
 *
 * This function works by splitting the sequence, deferring
 * the left half and continuing working on the right.  The
 * deferred left half is kept in an explicit stack to avoid
 * stack overflow.
 *
 * When the function's main loop encounters a sequence leaf
 * (an array-backed sequence, an empty sequence, or a single
 * element sequence) it then starts working the explicit
 * stack.
 */
static Orb_t map_core(Orb_t s, Orb_t f) {
	Orb_t finalresult;
	list_t stack = 0;
	int flag = 1;

	Orb_t base = Orb_NOTFOUND;

	while(flag) {
		Orb_t const* arr; size_t start, sz;
		if(Orb_array_backed(s, &arr, &start, &sz)) {
			finalresult = map_arr(arr, start, sz, f);
			flag = 0;
		} else {
			Orb_t single, l, r;
			switch(Orb_seq_decompose(s, &single, &l, &r)) {
			case 0: { finalresult = s; flag = 0; } break;
			case 1: {
				finalresult = Orb_call1(f, single);
				finalresult = Orb_seq(&finalresult, 1);
				flag = 0;
			} break;
			case 2: {
				/*create base if necessary*/
				if(base == Orb_NOTFOUND) {
					Orb_BUILDER {
						Orb_B_PARENT(o_map_core_sf);
						Orb_B_FIELD(hfield1, f);
					} base = Orb_ENDBUILDER;
				}
				/*create function to defer for left side*/
				Orb_t f0;
				Orb_BUILDER {
					Orb_B_PARENT(base);
					Orb_B_FIELD(hfield2, l);
				} f0 = Orb_ENDBUILDER;
				Orb_t df0 = Orb_defer(f0);
				/*push on stack*/
				stack = list_cons(df0, stack);
				/*work on right*/
				s = r;
			} break;
			}
		}
	}
	/*work on remainder*/
	while(stack) {
		Orb_t df0 = stack->value;
		list_t tmp = stack; stack = stack->next; Orb_gc_free(tmp);
		finalresult = Orb_conc(Orb_call0(df0), finalresult);
	}
	return finalresult;
}

/*
 * cfunc for map_core() above, wrapping f and s
 */
static Orb_t map_core_sf_cfunc(Orb_t argv[], size_t* pargc, size_t argl) {
	if(*pargc != 1) {
		Orb_THROW_cc("apply",
			"Incorrect number of arguments to map-core, expected 0"
		);
	}

	Orb_t self = argv[0];

	Orb_t f = Orb_deref(self, hfield1);
	Orb_t s = Orb_deref(self, hfield2);

	return map_core(s, f);
}

/*cfunc for map method of sequences*/
Orb_t Orb_map_cfunc(Orb_t argv[], size_t* pargc, size_t argl) {
	if(*pargc != 3) {
		Orb_THROW_cc("apply",
			"Incorrect number of arguments to map, "
			"which expects 2 arguments"
		);
	}

	Orb_t s = argv[1];
	Orb_t f = argv[2];

	return map_core(s, f);
}

void Orb_map_init(void) {
	Orb_gc_defglobal(&o_apply_fi);
	Orb_gc_defglobal(&o_map_core_sf);

	o_apply_fi = Orb_t_from_cfunc(&apply_fi_cfunc);
	o_map_core_sf = Orb_t_from_cfunc(&map_core_sf_cfunc);
}

