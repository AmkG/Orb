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
		} else {
			/*TODO*/
		}
	}
}

