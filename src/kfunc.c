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

#include"kfunc.h"
#include"liborb-kfunc.h"
#include"thread-support.h"

#include<stdio.h>
#include<string.h>

/*stack limit for Cheney on the MTA recursion*/
#define STACK_LIMIT 262144 /*256k*/

/*stack-based thread-local for kfunc*/

struct Orb_ktl_s {
	/*Cheney on the MTA*/
	jmp_buf empire_state_building;
	/*used by both kreturn's and return-to-cfunc*/
	Orb_t retval;
	/*used by kreturn's and by minor GC's*/
	Orb_t* argv;
	size_t argc;
	size_t argl;
	/*start of kstate chain*/
	Orb_kstate_t kstate;
	/*the current kstate in the thread-local pointer.
	we assume that thread-locals are not very efficient
	and should be written as rarely as we can get away
	with.  If ktl->kstate != ktl->kstate_in_tls, we
	need to update the tls.
	*/
	Orb_kstate_t kstate_in_tls;
};

/*thread-local pointer to most recent kstate from
a kfunc-to-cfunc call.
*/
static Orb_tls_t kstate_tls;

/*cfunc-to-kfunc calls*/
static Orb_t handle_kf(Orb_ktl_t, Orb_t argv[], size_t* pargc, size_t argl,
	Orb_kstate_t save_kstate
);
static Orb_t kfunc_cf(Orb_t argv[], size_t* pargc, size_t argl) {
	struct Orb_ktl_s sktl;
	memset((void*)&sktl, 0, sizeof(struct Orb_ktl_s));
	Orb_kstate_t save_kstate = (Orb_kstate_t) Orb_tls_get(kstate_tls);
	sktl.kstate = save_kstate;
	sktl.kstate_in_tls = save_kstate;

	Orb_t rv;

	Orb_TRY {
		rv = handle_kf(&sktl, argv, pargc, argl, save_kstate);
		Orb_tls_set(kstate_tls, save_kstate);
	} Orb_CATCH(E) {
		Orb_tls_set(kstate_tls, save_kstate);
		Orb_RETHROW(E);
	} Orb_ENDTRY;

	return rv;
}

/*special code for "return-to-cfunc" kontinuation*/
static Orb_t CFUNC_KONTINUE;
/*special hidden field for kfunc pointers*/
static Orb_t hf_kfunc;

/*get the kfunc of a kfunc-object*/
static Orb_kfunc_t get_kf(Orb_t ob) {
	Orb_kfunc_t* ppf = Orb_t_as_pointer(Orb_deref(ob, hf_kfunc));
	return *ppf;
}
/*update the kstate in the TLS, but only if needed*/
static void update_kstate(Orb_ktl_t ktl) {
	if(ktl->kstate != ktl->kstate_in_tls) {
		Orb_tls_set(kstate_tls, ktl->kstate);
		ktl->kstate_in_tls = ktl->kstate;
	}
}

/*various reasons for why we've been bounced back to the trampoline*/
#define REASON_STARTED 0 /*we're just getting started*/
#define REASON_KCALL 1 /*minor GC triggered, bounce back in*/
#define REASON_CFUNC 2 /*need to bounce to a cfunc being called*/
#define REASON_RETURN 3 /*return to the calling cfunc*/

static Orb_t handle_kf(
		Orb_ktl_t ktl, Orb_t argv[], size_t* pargc, size_t argl,
		Orb_kstate_t save_kstate) {
	Orb_t* orig_argv = argv;
	switch(setjmp(ktl->empire_state_building)) {
	case REASON_STARTED: {
		size_t argc = *pargc;

		Orb_kfunc_t kf = get_kf(argv[0]);

		/*need space for extra CFUNC_KONTINUE.  Create space if
		needed.
		*/
		if(argl == argc) {
			argv = Orb_gc_malloc((argl + 1) * sizeof(Orb_t));
			argv[0] = orig_argv[0];
			memcpy(&argv[2], &orig_argv[1],
				(argc - 1) * sizeof(Orb_t)
			);
			argv[1] = CFUNC_KONTINUE;
			kf(ktl, argv, argc + 1, argl + 1);
		} else {
			memmove(&argv[2], &orig_argv[1],
				(argc - 1) * sizeof(Orb_t)
			);
			argv[1] = CFUNC_KONTINUE;
			kf(ktl, argv, argc + 1, argl);
		}
		/*don't expect to return*/
		return Orb_NIL;
	} break;
	kcall_call:
	case REASON_KCALL: {
		Orb_t* cur_argv = ktl->argv;
		/*just bounce back*/
		Orb_kfunc_t kf = get_kf(cur_argv[0]);
		/*prevent ktl from keeping a reference to argv*/
		ktl->argv = 0;
		kf(ktl, cur_argv, ktl->argc, ktl->argl);
		return Orb_NIL;
	} break;
	case REASON_CFUNC: {
		Orb_t* cur_argv = ktl->argv;
		size_t cur_argc = ktl->argc;
		/*see if the return kontinuation is CFUNC_KONTINUE*/
		if(cur_argv[1] == CFUNC_KONTINUE) {
			/*it is! so make sure that the current kstate
			has been freed to the saved kstate.
			*/
			if(ktl->kstate != save_kstate) {
				Orb_THROW_cc("kfunc-kstate-1",
					"a kstate was not freed "
					"before returning to a cfunc "
					"in tail position."
				);
			}
			update_kstate(ktl);
			/*now see if we have enough space
			on the original argv to use Orb_TRAMPOLINE.
			*/
			if(cur_argc - 1 <= argl) {
				/*we do! now copy it*/
				orig_argv[0] = cur_argv[0];
				memcpy(&orig_argv[1], &cur_argv[2],
					(cur_argc - 2) * sizeof(Orb_t)
				);
				*pargc = cur_argc - 1;
				return Orb_TRAMPOLINE;
			} else {
				/*no we don't, so fool around with
				Orb_call_ex()
				*/
				cur_argv[1] = cur_argv[0];
				return Orb_call_ex(&cur_argv[1], cur_argc - 1,
					ktl->argl - 1
				);
			}
		} else {
			/*update kstate before we try calling the
			cfunc.
			*/
			update_kstate(ktl);
			/*re-arrange the argv*/
			Orb_t kont = cur_argv[1];
			cur_argv[1] = cur_argv[0];
			Orb_t rv = Orb_call_ex(
				&cur_argv[1], cur_argc - 1,
				ktl->argl - 1
			);
			/*pass it to the kontinuation*/
			cur_argv[0] = kont;
			cur_argv[1] = rv;
			ktl->argc = 2;
			goto kcall_call;
		}
	} break;
	case REASON_RETURN: {
		/*make sure that the current kstate is the same
		as the saved kstate.
		*/
		if(ktl->kstate != save_kstate) {
			Orb_THROW_cc( "kfunc-kstate-2",
				"a kstate was not freed "
				"before returning a value."
			);
		}
		update_kstate(ktl);
		return ktl->retval;
	} break;
	}
}

int Orb_kcall_prepare(
		Orb_ktl_t ktl, Orb_t argv[], size_t argc, size_t argl,
		Orb_kfunc_t** ppkf) {
	/*first check for a return to cfunc*/
	if(argv[0] == CFUNC_KONTINUE) {
		return REASON_RETURN;
	}
	/*now check if it's a straightforward kfunc call.  if not, REASON_CFUNC*/
	Orb_t v;
	Orb_t p;
	v = Orb_deref_nopropobj(argv[0], hf_kfunc, &p);
	if(p != Orb_NOTFOUND || v == Orb_NOTFOUND) {
		return REASON_CFUNC;
	}

	/*now determine stack size*/
	void* start;
	void* end;
	start = (void*) ktl;
	end = (void*) &end;
	char* cstart = (char*) start;
	char* cend = (char*) end;
	size_t stack_size;
	if(cstart < cend) {
		stack_size = cend - cstart;
	} else {
		stack_size = cstart - cend;
	}

	if(stack_size >= STACK_LIMIT) {
		return REASON_KCALL;
	}

	Orb_kfunc_t* pkf = Orb_t_as_pointer(v);
	*ppkf = pkf;
	return 0;
}

void Orb_kfunc_init(void) {
	kstate_tls = Orb_tls_init();
	CFUNC_KONTINUE = Orb_t_from_pointer(&CFUNC_KONTINUE);
	hf_kfunc = Orb_t_from_pointer(&hf_kfunc);
}



