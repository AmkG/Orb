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

/*TODO Orb_iterate_cfunc(), Orb_iterate_init()*/
