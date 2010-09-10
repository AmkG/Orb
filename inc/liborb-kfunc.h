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
#ifndef LIBORB_KFUNC_H
#define LIBORB_KFUNC_H

#if defined(_MSC_VER) && (_MSC_VER>=1200)
# pragma once
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*kfunc's are an alternative to the standard
cfunc calling convention.  Kfunc's use an
explicitly continuation-passing style where
C-style calls are implicitly assumed to be
tail-call-optimized.  The tail-call
optimization is actually implemented by using
Baker's "Cheney on the MTA" technique.  At
each call, the function checks if the stack
is "too deep".  If the stack is indeed too
deep, objects allocated using Orb_KALLOC_*
macros are moved from the stack to
Orb_gc_malloc-allocated space, then we do a
longjmp() to reset the stack.
*/

/*kfunc's have the following disadvantages:
1.  CPS.  Eww.
2.  Most Orb_* functions are not safe to use
    in kfunc's.  You are going to use almost
    completely different functions.  In
    particular, you can't even safely use
    Orb_ref()/Orb_deref() or their _cc
    variants, you have to use
    Orb_deref_nopropobj().  You also need
    to explicitly translate methods,
    since there is no equivalent
    Orb_ref_nopropobj().  Method translation
    requires kreturn's.
3.  Local mutable state that is easily
    modelled using a C struct and an opaque
    pointer object in a cfunc must be
    translated to a kstate object, which only
    provides a mutable array of Orb_t.
    kstate's require explicit stack-type
    management (i.e. you must free them in
    the reverse order of creation).
4.  Functions that really are safe using
    cfunc that are completely unsafe in
    kfunc's must be handled using kreturn's.
5.  Probably more inefficient non-tail calls.
    cfunc non-tail calls require a direct
    procedure call, an indirect procedure
    call, a trampolining check, and two
    procedure returns.  kfunc non-tail calls
    require a (most likely stack) allocation,
    two direct procedure calls, and two
    indirect procedure calls.
kfunc's, however, have the following advantages:
1.  Thread-local, bump-a-pointer allocation.
    Woot.  Reduced GC lock contention, fast
    alloc of small objects, efficient handling
    of temporary objects, what's not to like?
2.  Definitely more efficient tail calls.
    cfunc tail calls require a procedure
    return, an equality check, and a further
    indirect procedure call.  A kfunc tail
    call requires only direct procedure call
    and an indirect procedure call.
3.  Heap-limited non-tail call recursion.
    cfunc non-tail call recursion is limited
    by stack space.  Orb is designed for
    multithreaded programming/design.  Thread
    libraries have a nasty habit of limiting
    stack space even thought they leave heap
    space system-limited.
3.1.  We can implement "threadlets", which are
    OS threads but having a smaller stack
    (e.g. 64k instead of the usual 1M).
    Nowhere near Erlang's 300 bytes starting
    but much better than typical OS threads.
    kfunc's will work fine (if less
    efficiently due to reduced size of Eden)
    with the reduced stack space.  cfunc's
    will suffer non-tail recursion depth
    problems.
*/

struct Orb_ktl_s;
typedef struct Orb_ktl_s* Orb_ktl_t;

typedef void Orb_kfunc_f(Orb_ktl_t ktl, Orb_t argv[], size_t argc, size_t argl);
typedef Orb_kfunc_f* Orb_kfunc_t;

/*kfunc calls*/
/*returns an opaque integer that identifies the type
of call.  If the returned integer is 0, the call is
a normal kcall and **ppkf is the Orb_kfunc_f to
execute.
*/
int Orb_kcall_prepare(
	Orb_ktl_t ktl, Orb_t argv[], size_t argc, size_t argl,
	Orb_kfunc_t** ppkf
);
/*Handle a non-normal kcall*/
void Orb_kcall_perform(
	int type,
	Orb_ktl_t ktl, Orb_t argv[], size_t argc, size_t argl
);

#define Orb_KCALL(ktl, argv, argc, argl)\
	do {int Orb_priv_kcall_prepare_type;\
		Orb_ktl_t Orb_priv_kcall_ktl = (ktl);\
		Orb_t* Orb_priv_kcall_argv = (argv);\
		size_t Orb_priv_kcall_argc = (argc);\
		size_t Orb_priv_kcall_argl = (argl);\
		Orb_kfunc_t* Orb_priv_kcall_pkf;\
		Orb_priv_kcall_prepare_type = Orb_kcall_prepare(\
			Orb_priv_kcall_ktl,\
			Orb_priv_kcall_argv,\
			Orb_priv_kcall_argc,\
			Orb_priv_kcall_argl,\
			&Orb_priv_kcall_pkf\
		);\
		if(Orb_priv_kcall_prepare_type == 0) {\
			/*slightly cheaper on modern branch-prediction*/\
			/*microprocessors; the indirect jump at each*/\
			/*Orb_KCALL is individually given a branch-*/\
			/*prediction slot, allowing better prediction*/\
			return (*pkf)(\
				Orb_priv_kcall_ktl,\
				Orb_priv_kcall_argv,\
				Orb_priv_kcall_argc,\
				Orb_priv_kcall_argl\
			);\
		} else {\
			/*Need to handle*/\
			return Orb_kcall_perform(\
				Orb_priv_kcall_prepare_type,\
				Orb_priv_kcall_ktl,\
				Orb_priv_kcall_argv,\
				Orb_priv_kcall_argc,\
				Orb_priv_kcall_argl\
			);\
		}\
	} while(0)

/*If you know the name of the kfunc that will be called,
better to use this to compile a direct call.
*/
#define Orb_KDIRECTCALL(fname, ktl, argv, argc, argl)\
	do {int Orb_priv_kcall_prepare_type;\
		Orb_ktl_t Orb_priv_kcall_ktl = (ktl);\
		Orb_t* Orb_priv_kcall_argv = (argv);\
		size_t Orb_priv_kcall_argc = (argc);\
		size_t Orb_priv_kcall_argl = (argl);\
		Orb_kfunc_t* Orb_priv_kcall_pkf;\
		Orb_priv_kcall_prepare_type = Orb_kcall_prepare(\
			Orb_priv_kcall_ktl,\
			Orb_priv_kcall_argv,\
			Orb_priv_kcall_argc,\
			Orb_priv_kcall_argl,\
			&Orb_priv_kcall_pkf\
		);\
		if(Orb_priv_kcall_prepare_type == 0) {\
			/*Direct call in the common case*/\
			return fname(\
				Orb_priv_kcall_ktl,\
				Orb_priv_kcall_argv,\
				Orb_priv_kcall_argc,\
				Orb_priv_kcall_argl\
			);\
		} else {\
			/*Need to handle*/\
			return Orb_kcall_perform(\
				Orb_priv_kcall_prepare_type,\
				Orb_priv_kcall_ktl,\
				Orb_priv_kcall_argv,\
				Orb_priv_kcall_argc,\
				Orb_priv_kcall_argl\
			);\
		}\
	} while(0)

#ifdef __cplusplus
}
#endif

#endif /* LIBORB_KFUNC_H */
