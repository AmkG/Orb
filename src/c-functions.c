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
#include"c-functions.h"

typedef Orb_t (*function0)(void);
typedef Orb_t (*function1)(Orb_t);
typedef Orb_t (*function2)(Orb_t, Orb_t);
typedef Orb_t (*function3)(Orb_t, Orb_t, Orb_t);
typedef Orb_t (*variadic)(Orb_t*, size_t);

/*
 * Generic function to handle various c extension functions
 *
 * Performs the specified call to the given function, given the
 * arguments and the expected number of arguments.
 */
static Orb_t core_call(void* vpf, Orb_t argv[], size_t argc, int expect) {
	if(expect < 0) {
		/*variadic*/
		variadic f = vpf;
		return f(&argv[1], argc - 1);
	} else {
		switch(expect) {
		case -1: {
			variadic f = *((variadic*)vpf);
			return f(&argv[1], argc - 1);
			}
		case 0:	{
			function0 f = *((function0*) vpf);
			if(argc != 1) {
				Orb_THROW_cc("apply",
					"incorrect number of arguments to "
					"C function expecting no argument"
				);
			}
			return f();
			}
		case 1:	{
			function1 f = *((function1*) vpf);
			if(argc != 2) {
				Orb_THROW_cc("apply",
					"incorrect number of arguments to "
					"C function expecting 1 argument"
				);
			}
			return f(argv[1]);
			}
		case 2:	{
			function2 f = *((function2*) vpf);
			if(argc != 3) {
				Orb_THROW_cc("apply",
					"incorrect number of arguments to "
					"C function expecting 2 arguments"
				);
			}
			return f(argv[1], argv[2]);
			}
		case 3:	{
			function3 f = *((function3*) vpf);
			if(argc != 4) {
				Orb_THROW_cc("apply",
					"incorrect number of arguments to "
					"C function expecting 3 arguments"
				);
			}
			return f(argv[1], argv[2], argv[3]);
			}
		default:
			Orb_THROW_cc("c-function", "unexpected number of required arguments in C function");
		}
	}
}

/*
 * Generic handling of cf.
 *
 * Locks the CEL if not locked already before calling the core code.
 */
static Orb_t cf(Orb_t argv[], size_t* pargc, size_t argl) {
	Orb_t self = argv[0];

	Orb_t oN = Orb_deref_cc(self, "**N**");
	int N = Orb_t_as_integer(oN);

	Orb_t opF = Orb_deref_cc(self, "**f**");
	void* vpf = Orb_t_as_pointer(opF);

	if(Orb_CEL_havelock()) {
		/*not locked, so just call directly*/
		return core_call(vpf, argv, *pargc, N);
	} else {
		Orb_t rv;
		Orb_CEL_lock();
		/*make sure that exceptions unlock the CEL*/
		Orb_TRY {
			rv = core_call(vpf, argv, *pargc, N);
			Orb_CEL_unlock();
		} Orb_CATCH(E) {
			Orb_CEL_unlock();
			Orb_E_RETHROW(E);
		} Orb_ENDTRY;
		return rv;
	}
}

/*
 * Generic handling of CEL-free cf
 */
static Orb_t cf_CELfree(Orb_t argv[], size_t* pargc, size_t argl) {
	Orb_t self = argv[0];

	Orb_t oN = Orb_deref_cc(self, "**N**");
	int N = Orb_t_as_integer(oN);

	Orb_t opF = Orb_deref_cc(self, "**f**");
	void* vpf = Orb_t_as_pointer(opF);

	return core_call(vpf, argv, *pargc, N);
}

static Orb_t o_cf_base;
static Orb_t o_cf_CELfree_base;

void Orb_c_functions_init(void) {
	Orb_gc_defglobal(&o_cf_base);
	Orb_gc_defglobal(&o_cf_CELfree_base);

	o_cf_base = Orb_t_from_cfunc(&cf);
	o_cf_CELfree_base = Orb_t_from_cfunc(&cf_CELfree);
}

/*the code is repetitive, so handle copy-pasta with macrology*/
#define BUILD_CONVERTER(N)\
	Orb_t Orb_t_from_cf ## N(function ## N f) {\
		function ## N* pf = Orb_gc_malloc(sizeof(function ## N));\
		*pf = f;\
		Orb_BUILDER {\
			Orb_B_PARENT(o_cf_base);\
			Orb_B_FIELD_cc("**N**", Orb_t_from_integer(N));\
			Orb_B_FIELD_cc("**f**", Orb_t_from_pointer(pf));\
		} return Orb_ENDBUILDER;\
	}

BUILD_CONVERTER(0)
BUILD_CONVERTER(1)
BUILD_CONVERTER(2)
BUILD_CONVERTER(3)

Orb_t Orb_t_from_cfv(variadic f) {
	variadic* pf = Orb_gc_malloc(sizeof(variadic));
	*pf = f;
	Orb_BUILDER {
		Orb_B_PARENT(o_cf_base);
		Orb_B_FIELD_cc("**N**", Orb_t_from_integer(-1));
		Orb_B_FIELD_cc("**f**", Orb_t_from_pointer(pf));
	} return Orb_ENDBUILDER;
}

Orb_t Orb_CELfree(Orb_t orig) {
	Orb_t N = Orb_deref_cc(orig, "**N**");
	Orb_t f = Orb_deref_cc(orig, "**f**");

	Orb_BUILDER {
		Orb_B_PARENT(o_cf_CELfree_base);
		Orb_B_FIELD_cc("**N**", N);
		Orb_B_FIELD_cc("**f**", f);
	} return Orb_ENDBUILDER;
}

