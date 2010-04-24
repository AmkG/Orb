#include"liborb.h"
#include"thread-pool.h"
#include"thread-support.h"

static Orb_cell_t state;
static Orb_sema_t wait_sema;

/*
 * Immutable queue
 */
struct list_s {
	Orb_t val;
	struct list_s const* next;
};
typedef struct list_s list;
typedef list const* list_t;

struct queue_s {
	list_t insert_to;
	list_t remove_from;
};
typedef struct queue_s queue;
typedef queue const* queue_t;

static queue_t queue_init(void) {
	queue* rv = Orb_gc_malloc(sizeof(queue));
	rv->insert_to = 0;
	rv->remove_from = 0;
	return rv;
}
static inline int queue_empty(queue_t q) {
	return q->insert_to == 0 && q->remove_from == 0;
}
static queue_t queue_pop(queue_t q, Orb_t* prv) {
	queue* rv = Orb_gc_malloc(sizeof(queue));
	if(q->remove_from == 0) {
		/*reverse insert_to and make it into new remove_from*/
		list_t to_reverse = q->insert_to;
		list* building = 0;
		while(to_reverse) {
			list* nn = Orb_gc_malloc(sizeof(list));
			nn->val = to_reverse->val;
			nn->next = building;
			to_reverse = to_reverse->next;
		}
		rv->insert_to = 0;
		rv->remove_from = building->next;
		*prv = building->val;
		return rv;
	} else {
		rv->insert_to = q->insert_to;
		rv->remove_from = q->remove_from->next;
		*prv = q->remove_from->val;
		return rv;
	}
}
static queue_t queue_push(queue_t q, Orb_t val) {
	queue* rv = Orb_gc_malloc(sizeof(queue));
	rv->remove_from = q->remove_from;
	list* nn = Orb_gc_malloc(sizeof(list));
	nn->val = val;
	nn->next = q->insert_to;
	rv->insert_to = nn;
	return rv;
}

/*predeclare*/
static Orb_t core_cfunc(Orb_t argv[], size_t* pargc, size_t argl);

/*
 * current thread-pool state
 */
struct tp_state_s {
	size_t waiters; /*number of worker threads waiting for work to do*/
	queue_t tasks; /*queue of tasks*/
};
typedef struct tp_state_s tp_state;
typedef tp_state const* tp_state_t;

void Orb_thread_pool_init(void) {
	state = Orb_cell_init(Orb_NOTFOUND);
	wait_sema = Orb_sema_init(0);
}

void Orb_thread_pool_add(Orb_t f) {
	size_t waiters;
	tp_state_t curstate;

	Orb_t readstate;

	tp_state* nv = Orb_gc_malloc(sizeof(tp_state));
	Orb_t nstate = Orb_t_from_pointer(nv);
	Orb_t ostate = Orb_cell_get(state);

retry:
	if(ostate == Orb_NOTFOUND) {
		nv->waiters = 0;
		nv->tasks = queue_init();
		readstate = Orb_cell_cas_get(state, ostate, nstate);
		if(readstate != ostate) {
			ostate = readstate;
			goto retry;
		}
		/*succeeded CAS, now start each thread in pool*/
		size_t i; Orb_t f = Orb_t_from_cfunc(&core_cfunc);
		for(i = Orb_num_processors(); i; --i) {
			Orb_new_thread(f);
		}
		/*set up the variables for the rest of the code*/
		ostate = nstate;
		nv = Orb_gc_malloc(sizeof(tp_state));
		nstate = Orb_t_from_pointer(nv);
	}

	curstate = Orb_t_as_pointer(ostate);
	waiters = curstate->waiters;
	if(waiters > 0) {
		nv->waiters = waiters - 1;
	} else {
		nv->waiters = 0;
	}

	nv->tasks = queue_push(curstate->tasks, f);

	readstate = Orb_cell_cas_get(state, ostate, nstate);
	if(readstate != ostate) {
		ostate = readstate;
		goto retry;
	}

	/*succeeded CAS, now check if need to wake up thread*/
	if(waiters > 0) {
		Orb_sema_post(wait_sema);
	}

}

/*
 * Thread-pool core function
 */
static Orb_t core_cfunc(Orb_t argv[], size_t* pargc, size_t argl) {
	/*ignore all arguments*/
	for(;;) {
		tp_state* nv = Orb_gc_malloc(sizeof(tp_state));
		Orb_t ostate = Orb_cell_get(state);
		for(;;) {
			tp_state_t curstate = Orb_t_as_pointer(ostate);
			if(queue_empty(curstate->tasks)) {
				/*attempt to increment waiters*/
				nv->tasks = curstate->tasks;
				nv->waiters = curstate->waiters + 1;
				Orb_t readstate = Orb_cell_cas_get(state, ostate, Orb_t_from_pointer(nv));
				if(readstate == ostate) {
					/*update successful*/
					Orb_sema_wait(wait_sema);
					break;
				} else ostate = readstate;
			} else {
				/*attempt to pop off*/
				Orb_t todo;
				queue_t nq = queue_pop(custate->tasks, &todo);
				/*now try to update state*/
				nv->tasks = nq;
				nv->waiters = curstate->waiters;
				Orb_t readstate = Orb_cell_cas_get(state, ostate, Orb_t_from_pointer(nv));
				if(readstate == ostate) {
					/*update successful*/
					Orb_TRY {
						Orb_call0(todo);
					} Orb_CATCH(E) { /*do nothing*/ }
					Orb_ENDTRY;
					break;
				} else ostate = readstate;
			}
		}
	}
}


