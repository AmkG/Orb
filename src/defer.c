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
#include"thread-support.h"
#include"defer.h"

#include<assert.h>

/*
 * Structures for current state of defer object
 */
/*informational structures*/
struct waiter_s {
	Orb_sema_t sema;
	size_t N;
};
typedef struct waiter_s waiter;
struct errdata_s {
	Orb_t type;
	Orb_t value;
};
typedef struct errdata_s errdata;
/*core structure*/
struct defer_s {
	enum {
		state_idle,
		state_running,
		state_wait_on_running,
		state_finished,
		state_errored
	} type;
	union {
		Orb_t f; /*function to run in state_idle*/
		waiter waitlist; /*semaphore and number of
				waiters in state_wait_on_running
				*/
		Orb_t rv; /*return value in state_finished*/
		errdata error; /*error details in state_errored*/
	};
};
typedef struct defer_s defer;
typedef defer* defer_t;

/*given an Orb_cell_t, transition from
state_running or state_wait_on_running to the
given defer state.  Also wakes up any waiters
*/
static void post_run(Orb_cell_t c, defer_t d) {
	Orb_t nstate = Orb_t_from_pointer(d);
	Orb_t ostate = Orb_cell_get(c);
	for(;;) {
		defer_t pstate = Orb_t_as_pointer(ostate);
		assert(
			pstate->type == state_running
			|| pstate->type == state_wait_on_running
		);

		/*CAS*/
		Orb_t read = Orb_cell_cas_get(c,
			ostate, nstate
		);
		if(read == ostate) {
			/*succeeded. wake up if any waiters*/
			if(pstate->type == state_wait_on_running) {
				size_t N = pstate->waitlist.N;
				Orb_sema_t sema = pstate->waitlist.sema;

				size_t i;
				for(i = 0; i < N; ++i) {
					Orb_sema_post(sema);
				}
			}
			/*exit loop*/
			break;
		} else ostate = read;
	}
}

/*attempt to transition from an expected state_idle state to
state_running, then run the function
Returns the result of the Orb_cell_cas_get()
	== ostate if ran, otherwise did not run
Also provides the final state if ran
*/
static Orb_t to_running(Orb_cell_t c, Orb_t ostate, defer_t pstate,
			defer_t* pnstate) {
	assert(pstate->type == state_idle);
	assert(Orb_t_from_pointer(pstate) == ostate);

	defer_t rstate = Orb_gc_malloc(sizeof(defer));
	rstate->type = state_running;

	Orb_t read = Orb_cell_cas_get(c, ostate, Orb_t_from_pointer(rstate));
	if(read == ostate) {
		defer_t nstate = Orb_gc_malloc(sizeof(defer));
		Orb_TRY {
			Orb_t rv = Orb_call0(pstate->f);
			nstate->type = state_finished;
			nstate->rv = rv;
		} Orb_CATCH(E) {
			Orb_t type = Orb_E_TYPE(E);
			Orb_t value = Orb_E_VALUE(E);
			nstate->type = state_errored;
			nstate->error.type = type;
			nstate->error.value = value;
		} Orb_ENDTRY;
		post_run(c, nstate);
		*pnstate = nstate;
	}
	return read;
}

/*core function for 'try-run method*/
static void core_try_run(Orb_cell_t c) {
	Orb_t ostate = Orb_cell_get(c);
	for(;;) {
		defer_t pstate = Orb_t_as_pointer(ostate);
		if(pstate->type == state_idle) {
			defer_t dont_care;
			Orb_t read = to_running(c, ostate, pstate, &dont_care);
			if(read == ostate) return;
			else ostate = read;
		}
	}
}
/*core function for **call** method*/
static Orb_t core_call(Orb_cell_t c) {
	Orb_t ostate = Orb_cell_get(c);
	for(;;) {
		defer_t pstate = Orb_t_as_pointer(ostate);
		if(pstate->type == state_idle) {
			defer_t result;
			Orb_t read = to_running(c, ostate, pstate, &result);
			if(read == ostate) ostate = Orb_t_from_pointer(result);
			else ostate = read;
		} else if(pstate->type == state_running) {
			/*create semaphore and wait on it*/
			Orb_sema_t sema = Orb_sema_init(0);

			defer_t nstate = Orb_gc_malloc(sizeof(defer));
			nstate->type = state_wait_on_running;
			nstate->waitlist.sema = sema;
			nstate->waitlist.N = 1;

			Orb_t read = Orb_cell_cas_get(c,
				ostate, Orb_t_from_pointer(nstate)
			);
			if(read == ostate) {
				Orb_sema_wait(sema);
			} else {
				ostate = read;
				Orb_gc_free(nstate);
			}
		} else if(pstate->type == state_wait_on_running) {
			/*extract semaphore and wait on it*/
			Orb_sema_t sema = pstate->waitlist.sema;
			size_t N = pstate->waitlist.N;

			defer_t nstate = Orb_gc_malloc(sizeof(defer));
			nstate->type = state_wait_on_running;
			nstate->waitlist.sema = sema;
			nstate->waitlist.N = N + 1;

			Orb_t read = Orb_cell_cas_get(c,
				ostate, Orb_t_from_pointer(nstate)
			);
			if(read == ostate) {
				Orb_sema_wait(sema);
			} else {
				ostate = read;
				Orb_gc_free(nstate);
			}
		} else if(pstate->type == state_finished) {
			return pstate->rv;
		} else if(pstate->type == state_errored) {
			Orb_THROW(pstate->error.type, pstate->error.value);
		}
	}
}

static Orb_t hfield1;
static Orb_t defer_base;

/*method function for try-run*/
static Orb_t try_run_cfunc(Orb_t argv[], size_t* pargc, size_t argl) {
	if(*pargc != 2) {
		Orb_THROW_cc("apply",
			"Incorrect number of arguments to try-run"
		);
	}
	Orb_t this = argv[1];
	Orb_t oc = Orb_deref(this, hfield1);
	Orb_cell_t c = Orb_t_as_pointer(oc);
	core_try_run(c);
	return Orb_NIL;
}
/*method function for call*/
static Orb_t call_cfunc(Orb_t argv[], size_t* pargc, size_t argl) {
	if(*pargc != 2) {
		Orb_THROW_cc("apply",
			"Incorrect number of arguments to call"
		);
	}
	Orb_t this = argv[1];
	Orb_t oc = Orb_deref(this, hfield1);
	Orb_cell_t c = Orb_t_as_pointer(oc);
	return core_call(c);
}

void Orb_defer_init(void) {
	Orb_gc_defglobal(&hfield1);
	Orb_gc_defglobal(&defer_base);

	hfield1 = Orb_t_from_pointer(&hfield1);
	Orb_BUILDER {
		Orb_B_PARENT(Orb_OBJECT);
		Orb_B_FIELD_cc("try-run",
			Orb_method(
				Orb_t_from_cfunc(&try_run_cfunc)
			)
		);
		Orb_B_FIELD_cc("**call**",
			Orb_method(
				Orb_t_from_cfunc(&call_cfunc)
			)
		);
	} defer_base = Orb_ENDBUILDER;
}
Orb_t Orb_runonce(Orb_t f) {
	defer_t state = Orb_gc_malloc(sizeof(defer));
	state->type = state_idle;
	state->f = f;

	Orb_cell_t c = Orb_cell_init(Orb_t_from_pointer(state));

	Orb_t rv;
	Orb_BUILDER {
		Orb_B_PARENT(defer_base);
		Orb_B_FIELD(hfield1, Orb_t_from_pointer(c));
	} rv = Orb_ENDBUILDER;
	return rv;
}
Orb_t Orb_defer(Orb_t f) {
	/*exactly like runonce, but add to thread-pool*/
	Orb_t rv = Orb_runonce(f);
	Orb_t tryrun = Orb_ref_cc(rv, "try-run");
	Orb_thread_pool_add(tryrun);
	return rv;
}

/*TODO
1.  Abort for defer objects, which should also abort
    child defer's built in execution of those objects.
    Errors that occur while performing a particular
    defer object should automatically abort child
    defer objects.

2.  Consider instead use of a defer@ form:
    (defer@ (@f   (one-expression)
             @f1  (two-expression))
      (something-that-might-error @f @f1))
    '@f and '@f1 are otherwise ordinary defers.
    If 'something-that-might-error throws, then defer@
    automatically aborts @f and @f1.
*/

