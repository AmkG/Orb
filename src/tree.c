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
#include"tree.h"

enum color_e {color_red, color_black};
typedef enum color_e color_t;

struct node_s {
	color_t c;
	struct node_s* l;
	Orb_t k;
	Orb_t v;
	struct node_s* r;
};
typedef struct node_s node;
typedef node* node_t;

struct Orb_tree_s {
	Orb_tree_comparer_t cmp;
	node_t top;
};
typedef struct Orb_tree_s Orb_tree;

Orb_tree_t Orb_tree_init(Orb_tree_comparer_t cmp) {
	Orb_tree_t rv = Orb_gc_malloc(sizeof(Orb_tree));
	rv->cmp = cmp;
	rv->top = 0;
	return rv;
}

static Orb_t lookup(node_t n, Orb_tree_comparer_t cmp, Orb_t k);

Orb_t Orb_tree_lookup(Orb_tree_t tree, Orb_t k) {
	return lookup(tree->top, tree->cmp, k);
}

static Orb_t lookup(node_t n, Orb_tree_comparer_t cmp, Orb_t k) {
	for(;;) {
		if(n == 0) {
			return Orb_NOTFOUND;
		} else {
			Orb_t nk = n->k;
			if(cmp(k, nk)) {
				n = n->l;
			} else if(cmp(nk, k)) {
				n = n->r;
			} else {
				return n->v;
			}
		}
	}
}

/*Okasaki insert balancing*/
static node_t balance(node_t);

/*constructor*/
static node_t cnode(color_t c, node_t l, Orb_t k, Orb_t v, node_t r) {
	node_t rv = Orb_gc_malloc(sizeof(node));
	rv->c = c;
	rv->l = l;
	rv->k = k;
	rv->v = v;
	rv->r = r;
	return rv;
}

static node_t ins(node_t n, Orb_tree_comparer_t cmp, Orb_t k, Orb_t v) {
	if(n == 0) {
		return cnode(color_red, 0, k, v, 0);
	} else {
		Orb_t nk = n->k;
		if(cmp(k, nk)) {
			return cnode(
				n->c, balance(ins(n->l, cmp, k, v)),
				n->k, n->v, n->r
			);
		} else if(cmp(nk, k)) {
			return cnode(
				n->c, n->l, n->k, n->v,
				balance(ins(n->r, cmp, k, v))
			);
		} else {
			return cnode(
				n->c, n->l, k, v, n->r
			);
		}
	}
}

/*Okasaki-style insertion*/
static node_t cbalance(
		node_t a,
		Orb_t xk, Orb_t xv,
		node_t b,
		Orb_t yk, Orb_t yv,
		node_t c,
		Orb_t zk, Orb_t zv,
		node_t d) {
	return cnode(
		color_red,
		cnode(color_black,
			a,
			xk, xv,
			b
		),
		yk, yv,
		cnode(color_black,
			c,
			zk, zv,
			d
		)
	);
}

static node_t balance(node_t n) {
	if(n->c == color_red) return n;
	/*no pattern matching, do it ourself!*/

#define RET_CBAL \
	return cbalance(a, xk, xv, b, yk, yv, c, zk, zv, d)

	/* (T 'b a x (T 'r b y (T 'r c z d))) */
	{
		node_t a = n->l;
		Orb_t xk = n->k, xv = n->v;
		node_t tmp = n->r;

		if(tmp == 0 || tmp->c == color_black) {
			goto skip2; /*also skip the next pattern*/
		}
		node_t b = tmp->l;
		Orb_t yk = tmp->k, yv = tmp->v;
		node_t tmp2 = tmp->r;

		if(tmp2 == 0 || tmp2->c == color_black) {
			goto skip1;
		}
		node_t c = tmp2->l;
		Orb_t zk = tmp2->k, zv = tmp2->v;
		node_t d = tmp2->r;

		RET_CBAL;
	}
skip1:

	/* (T 'b a x (T 'r (T 'r b y c) z d)) */
	{
		node_t a = n->l;
		Orb_t xk = n->k, xv = n->v;
		node_t tmp = n->r;

		/*check of tmp was already done before*/
		node_t tmp2 = tmp->l;
		Orb_t zk = tmp->k, zv = tmp->v;
		node_t d = tmp->r;

		if(tmp2 == 0 || tmp2->c == color_black) {
			goto skip2;
		}
		node_t b = tmp2->l;
		Orb_t yk = tmp2->k, yv = tmp2->v;
		node_t c = tmp2->r;

		RET_CBAL;
	}
skip2:

	/* (T 'b (T 'r a x (T 'r b y c)) z d) */
	{
		node_t tmp = n->l;
		Orb_t zk = n->k, zv = n->v;
		node_t d = n->r;

		if(tmp == 0 || tmp->c == color_black) {
			goto skip4; /*also skip next pattern*/
		}
		node_t a = tmp->l;
		Orb_t xk = tmp->k, xv = tmp->v;
		node_t tmp2 = tmp->r;

		if(tmp2 == 0 || tmp2->c == color_black) {
			goto skip3;
		}
		node_t b = tmp2->l;
		Orb_t yk = tmp2->k, yv = tmp2->v;
		node_t c = tmp2->r;

		RET_CBAL;
	}
skip3:

	/* (T 'b (T 'r (T 'r a x b) y c) z d) */
	{
		node_t tmp = n->l;
		Orb_t zk = n->k, zv = n->v;
		node_t d = n->r;

		/*tmp was already checked before*/
		node_t tmp2 = tmp->l;
		Orb_t yk = tmp->k, yv = tmp->v;
		node_t c = tmp->r;

		if(tmp2 == 0 || tmp2->c == color_black) {
			goto skip4;
		}
		node_t a = tmp2->l;
		Orb_t xk = tmp2->k, xv = tmp2->v;
		node_t b = tmp2->r;

		RET_CBAL;
	}
skip4:

#undef RET_CBAL

	return n;

}

/*insert routine*/
Orb_tree_t Orb_tree_insert(Orb_tree_t tree, Orb_t k, Orb_t v) {
	node_t nrv = ins(tree->top, tree->cmp, k, v);
	nrv->c = color_black;

	Orb_tree_t rv = Orb_gc_malloc(sizeof(Orb_tree));
	rv->cmp = tree->cmp;
	rv->top = nrv;

	return rv;
}

