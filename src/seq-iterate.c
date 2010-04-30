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

/*
We implement the core of Orb_EACH here, and use that same
core to implement s!iterate on sequences
*/
struct list_s {
	Orb_t value;
	struct list_s* next;
};
typedef struct list_s list;
typedef list* list_t;

struct Orb_priv_each_s {
	Orb_t one_value; /*used for singleton sequences*/
	Orb_t const* backer; /*keep a reference to the buffer*/
	Orb_t const* ptr;
	Orb_t const* end;
	list_t stack; /*remaining items to traverse*/
};

static void destructure(struct Orb_priv_each_s*, Orb_t);

static void normalize(struct Orb_priv_each_s*);

struct Orb_priv_each_s* Orb_priv_each_init(Orb_t s) {
	s = Orb_ensure_seq(s);

	struct Orb_priv_each_s* rv =
		Orb_gc_malloc(sizeof(struct Orb_priv_each_s))
	;
	rv->stack = 0;

	destructure(rv, s);
	normalize(rv);

	return rv;
}

static void normalize(struct Orb_priv_each_s* es) {
	while(es->ptr == es->end && es->stack) {
		list_t tmp = es->stack;
		es->stack = tmp->next;
		destructure(es, tmp->value);
	}
}

static void destructure(struct Orb_priv_each_s* es, Orb_t s) {
	for(;;) {
		/*first check if array-backed*/
		size_t start, sz;
		if(Orb_array_backed(s, &es->backer, &start, &sz)) {
			es->ptr = es->backer + start;
			es->end = es->backer + sz;
			return;
		}

		/*now destructure*/
		Orb_t single; Orb_t l, r;
		switch(Orb_seq_decompose(s, &single, &l, &r)) {
		case 0:
			/*nothing to scan*/
			es->backer = es->ptr = es->end = 0;
			return;
		case 1:
			/*one item, store in one_value and have the pointers
			point to it
			*/
			es->one_value = single;
			es->backer = es->ptr = &es->one_value;
			es->end = es->ptr + 1;
			return;
		case 2:
			/*two subsequences, push the right one on the stack*/
			{list_t nn = Orb_gc_malloc(sizeof(list));
				nn->value = r;
				nn->next = es->stack;
				es->stack = nn;
			}
			/*and go to the left*/
			s = l;
		}
	}
}

void Orb_priv_each_next(struct Orb_priv_each_s* es, size_t N) {
	while(N && (es->ptr != es->end)) {
		++es->ptr;
		normalize(es);
		--N;
	}
	/*at end, remove reference to buffer*/
	if(es->ptr == es->end) es->backer = 0;
}

int Orb_priv_each_atend(struct Orb_priv_each_s* es) {
	return es->ptr == es->end && !es->stack;
}

Orb_t Orb_priv_each_cur(struct Orb_priv_each_s* es) {
	if(es->ptr == es->end) {
		Orb_THROW_cc("iterate",
			"Attempt to access element past end."
		);
	}
	return *es->ptr;
}

static Orb_t hfield;

static Orb_t o_cur_cfunc;
static Orb_t o_next_cfunc;
static Orb_t o_at_end_cfunc;

#define GET_ES\
	Orb_t self = argv[0];\
	Orb_t oes = Orb_deref(self, hfield);\
	struct Orb_priv_each_s* es = Orb_t_as_pointer(oes)

static Orb_t cur_cfunc(Orb_t argv[], size_t* pargc, size_t argl) {
	if(*pargc != 1) {
		Orb_THROW_cc("apply",
			"Incorrect number of arguments to @cur"
		);
	}
	GET_ES;

	return Orb_priv_each_cur(es);
}

static Orb_t next_cfunc(Orb_t argv[], size_t* pargc, size_t argl) {
	if(*pargc != 2) {
		Orb_THROW_cc("apply",
			"Incorrect number of arguments to @next"
		);
	}
	GET_ES;
	Orb_t oN = argv[1];
	size_t N = Orb_t_as_integer(oN);

	Orb_priv_each_next(es, N);

	return Orb_NIL;
}

static Orb_t at_end_cfunc(Orb_t argv[], size_t* pargc, size_t argl) {
	if(*pargc != 1) {
		Orb_THROW_cc("apply",
			"Incorrect number of arguments to @at-end"
		);
	}
	GET_ES;

	if(Orb_priv_each_atend(es)) return Orb_TRUE;
	else return Orb_NIL;
}

Orb_t Orb_iterate_cfunc(Orb_t argv[], size_t* pargc, size_t argl) {
	if(*pargc != 3) {
		Orb_THROW_cc("apply",
			"Incorrect number of arguments to s!iterate"
		);
	}
	Orb_t s = argv[1];
	Orb_t f = argv[2];

	Orb_safetycheck(f,
		Orb_SAFE(1) | Orb_SAFE(2) | Orb_SAFE(3)
	);

	struct Orb_priv_each_s* es = Orb_priv_each_init(s);
	Orb_t oes = Orb_t_from_pointer(es);

	Orb_t cur, next, at_end;
	Orb_BUILDER {
		Orb_B_PARENT(o_cur_cfunc);
		Orb_B_FIELD(hfield, oes);
	} cur = Orb_ENDBUILDER;
	Orb_BUILDER {
		Orb_B_PARENT(o_next_cfunc);
		Orb_B_FIELD(hfield, oes);
	} next = Orb_ENDBUILDER;
	Orb_BUILDER {
		Orb_B_PARENT(o_at_end_cfunc);
		Orb_B_FIELD(hfield, oes);
	} at_end = Orb_ENDBUILDER;

	if(argl >= 4) {
		argv[0] = f;
		argv[1] = cur;
		argv[2] = next;
		argv[3] = at_end;
		*pargc = 4;
		return Orb_TRAMPOLINE;
	} else {
		return Orb_call3(f, cur, next, at_end);
	}
}

void Orb_iterate_init(void) {
	Orb_gc_defglobal(&hfield);
	Orb_gc_defglobal(&o_cur_cfunc);
	Orb_gc_defglobal(&o_next_cfunc);
	Orb_gc_defglobal(&o_at_end_cfunc);

	hfield = Orb_t_from_pointer(&hfield);
	o_cur_cfunc = Orb_t_from_cfunc(&cur_cfunc);
	o_next_cfunc = Orb_t_from_cfunc(&next_cfunc);
	o_at_end_cfunc = Orb_t_from_cfunc(&at_end_cfunc);
}


