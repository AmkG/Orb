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

#include<assert.h>

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
	/*recorded return point for kreturn's*/
	Orb_kfunc_t retpoint;
	/*start of kstate chain*/
	Orb_kstate_t kstate;
	/*the current kstate in the thread-local pointer.
	we assume that thread-locals are not very efficient
	and should be written as rarely as we can get away
	with.  If ktl->kstate != ktl->kstate_in_tls, we
	need to update the tls.
	*/
	Orb_kstate_t kstate_in_tls;
	/*the kstate attached to the current function, if
	there is one.
	*/
	Orb_kstate_t attached;
};

/*structure used by constructed kontinuations to handle
kreturn's that require calling another function.
*/
struct kreturn_kont_s {
	Orb_kfunc_t retpoint;
	Orb_ktl_t ktl;
	Orb_t* argv;
	size_t argc;
	size_t argl;
};
typedef struct kreturn_kont_s kreturn_kont;
typedef kreturn_kont* kreturn_kont_t;

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
/*special hidden field for kstate attachments*/
static Orb_t hf_kstate;

/*get the kfunc of a kfunc-object*/
static Orb_kfunc_t get_kf(Orb_t ob) {
	Orb_t rv = Orb_deref(ob, hf_kfunc);
	if(rv == Orb_NOTFOUND) return 0;
	Orb_kfunc_t* ppf = Orb_t_as_pointer(rv);
	return *ppf;
}
/*get an attached kstate*/
static Orb_kstate_t get_ks(Orb_t ob) {
	Orb_t rv = Orb_deref(ob, hf_kstate);
	if(rv == Orb_NOTFOUND) return 0;
	return Orb_t_as_pointer(rv);
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
#define REASON_NOPROPOBJ 4 /*call to property object*/

static Orb_t handle_kf(
		Orb_ktl_t ktl, Orb_t argv[], size_t* pargc, size_t argl,
		Orb_kstate_t save_kstate) {
	Orb_t* orig_argv = argv;
	switch(setjmp(ktl->empire_state_building)) {
	case REASON_STARTED: {
		size_t argc = *pargc;

		Orb_kfunc_t kf = get_kf(argv[0]);
		ktl->attached = get_ks(argv[0]);

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
		/*get any kstate*/
		ktl->attached = get_ks(cur_argv[0]);
		/*prevent ktl from keeping a reference to argv*/
		ktl->argv = 0;
		kf(ktl, cur_argv, ktl->argc, ktl->argl);
		return Orb_NIL;
	} break;
	cfunc_call:
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
					"before returning via a cfunc "
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
	case REASON_NOPROPOBJ: {
		/*access propobj safely here*/
		Orb_kfunc_t kf = get_kf(ktl->argv[0]);
		if(kf) {
			/*kfunc's may be continuations*/
			goto kcall_call;
		} else {
			/*cfunc's should never be used for continuations*/
			goto cfunc_call;
		}
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
	if(p != Orb_NOTFOUND) {
		return REASON_NOPROPOBJ;
	}
	if(v == Orb_NOTFOUND) {
		return REASON_CFUNC;
	}

	/*search for any attached kstate*/
	Orb_t ksv;
	ksv = Orb_deref_nopropobj(argv[0], hf_kstate, &p);
	if(p != Orb_NOTFOUND) {
		return REASON_NOPROPOBJ;
	}
	if(ksv == Orb_NOTFOUND) {
		ktl->attached = 0;
	} else {
		ktl->attached = Orb_t_as_pointer(ksv);
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

/*-----------------------------------------------------------------------------
Eden Evacuation
-----------------------------------------------------------------------------*/

/*for maintaining the gray set*/
struct gray_list_s {
	Orb_t ob;
	struct gray_list_s* next;
};
typedef struct gray_list_s gray_list;

struct ee_main_s {
	/*stack limits. start < end*/
	uintptr_t start;
	uintptr_t end;
	/*remaining object to scan*/
	gray_list* gray_set;
};
typedef struct ee_main_s ee_main;

/*accepts an object, then returns the object that should
replace it, or the same object if no replacement
necessary.
*/
static Orb_t evac_value(ee_main* ee, Orb_t orig) {
	/*for non-objects, return*/
	if(!Orb_t_is_object(orig)) {
		return orig;
	}
	/*object.  So check if in stack*/
	void* vp = Orb_t_as_pointer(orig);
	uintptr_t ui = (uintptr_t) vp;
	if(ee->start < ui && ui < ee->end) {
		/*evacuate, evacuate, foop foop*/
		Orb_t rv;
		int check = Orb_evacuate_object(&rv, orig);
		if(check) {
			/*first time the object is evacuated*/
			gray_list* ngray = Orb_gc_malloc(sizeof(gray_list));
			ngray->ob = rv;
			ngray->next = ee->gray_set;
			ee->gray_set = ngray;
		}
		return rv;
	} else {
		return orig;
	}
}

/*accepts the field and value of the object, then returns a new
replacement value for that field.
*/
static Orb_t evac_field(Orb_t field, Orb_t value, void* vp) {
	ee_main* ee = (ee_main*) vp;
}

/*evacuates live objects from the stack (Eden).  Live objects
are in the argv[] vector, from argv[0] to argv[argc - 1].
Returns a newly-allocated argv vector.
*/
static Orb_t* evacuate_stack(
		Orb_ktl_t ktl, Orb_t argv[], size_t argc, size_t argl) {
	ee_main ees;
	ees.gray_set = 0;
	ee_main* ee = &ees;

	/*step 1: determine stack limits*/
	ees.start = (uintptr_t) ktl;
	ees.end = (uintptr_t) (void*) &ees;
	if(ees.start > ees.end) {
		/*swap if necessary*/
		uintptr_t tmp = ees.start;
		ees.start = ees.end;
		ees.end = tmp;
	}

	/*step 2: evacuate links of kstate chain that are on stack (Eden)*/
	{Orb_kstate_t* ppt;
		ppt = &ktl->kstate;
		/*kstate's without forwarding pointers are not yet evacuated*/
		while(*ppt && ((*ppt)->forwarding == 0)) {
			Orb_kstate_t pt = *ppt;
			Orb_kstate_t npt = Orb_gc_malloc(sizeof(Orb_kstate));

			npt->prev = pt->prev;
			npt->forwarding = npt;
			pt->forwarding = npt;

			npt->state = Orb_gc_malloc(pt->sz * sizeof(Orb_t));
			memcpy(npt->state, pt->state, pt->sz * sizeof(Orb_t));
			npt->sz = pt->sz;

			*ppt = npt;
			ppt = &npt->prev;
		}
	}

	/*step 3: evacuate objects accessible from the kstate chain*/
	{Orb_kstate_t ks;
		ks = ktl->kstate;
		while(ks) {
			size_t i;
			for(i = 0; i < ks->sz; ++i) {
				ks->state[i] = evac_value(ee, ks->state[i]);
			}

			ks = ks->prev;
		}
	}

	/*step 4: evacuate objects accessible from the argv vector*/
	Orb_t* rv = Orb_gc_malloc(argl * sizeof(Orb_t));
	{size_t i;
		for(i = 0; i < argc; ++i) {
			rv[i] = evac_value(ee, argv[i]);
		}
	}

	/*step 5: traverse and empty gray set*/
	while(ee->gray_set) {
		gray_list* g = ee->gray_set;
		Orb_t ob = g->ob;
		ee->gray_set = g->next;
		Orb_gc_free((void*) g);

		Orb_evacuate_fields(ob, &evac_field, ee);
	}

	return rv;
}

/*performs a kcall that requires a longjmp() back to the
trampoline.
*/
void Orb_kcall_perform(
		int type,
		Orb_ktl_t ktl, Orb_t argv[], size_t argc, size_t argl) {
	assert(type != 0);
	switch(type) {
	case REASON_NOPROPOBJ:
	case REASON_KCALL:
	case REASON_CFUNC: {
		Orb_t* nargv = evacuate_stack(ktl, argv, argc, argl);
		ktl->argv = nargv;
		ktl->argc = argc;
		ktl->argl = argl;
	} break;
	case REASON_RETURN: {
		/*only evacuate the actual return value if available*/
		if(argc > 1) {
			Orb_t* nargv = evacuate_stack(ktl, &argv[1], 1, 1);
			ktl->retval = *nargv;
			Orb_gc_free(nargv);
		} else {
			ktl->retval = Orb_NIL;
		}
		ktl->argv = 0;
	} break;
	default: {
		assert(type);
	} break;
	}
	longjmp(ktl->empire_state_building, type);
}

void Orb_kfunc_init(void) {
	kstate_tls = Orb_tls_init();
	CFUNC_KONTINUE = Orb_t_from_pointer(&CFUNC_KONTINUE);
	hf_kfunc = Orb_t_from_pointer(&hf_kfunc);
}



