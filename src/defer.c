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
				size_t N = pstate->error.N;
				Orb_sema_t sema = state->error.sema;

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
static void core_call(Orb_cell_t c) {
	Orb_t ostate = Orb_cell_get(c);
	for(;;) {
		defer_t pstate = Orb_t_as_pointer(ostate);
		if(pstate->type == state_idle) {
			defer_t result;
			Orb_t read = to_running(c, ostate, pstate, &result);
			if(read == ostate) ostate = result;
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

			Orb_T read = Orb_cell_cas_get(c,
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

static Orb_t defer_base;
static Orb_t hfield1;

void Orb_defer_init(void) {
}

