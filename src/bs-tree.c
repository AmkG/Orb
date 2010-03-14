
#include"liborb.h"
#include"bs-tree.h"
#include"thread-support.h"

struct Orb_bs_tree_s {
	Orb_bs_tree_comparer_t cf;
	Orb_cell_t cell;
};

struct node_s;
typedef struct node_s* node_t;
struct node_s {
	void* p;
	node_t l;
	node_t r;
};

Orb_bs_tree_t Orb_bs_tree_init(Orb_bs_tree_comparer_t cf) {
	Orb_bs_tree_t rv = Orb_gc_malloc(sizeof(struct Orb_bs_tree_s));
	rv->cf = cf;
	rv->cell = Orb_cell_init(Orb_t_from_pointer((void*) 0));
	return rv;
}

void* Orb_bs_tree_lookup(Orb_bs_tree_t tree, void* p) {
	node_t n = Orb_t_as_pointer(Orb_cell_get(tree->cell));
	Orb_bs_tree_comparer_t cf = tree->cf;
	int rv;
top:
	if(n == 0) return 0;
	rv = cf(p, n->p);
	if(rv == 0) return n->p;
	else if(rv < 0) n = n->l;
	else if(rv > 0) n = n->r;
	goto top;
}

static node_t node_ctor(void* p, node_t l, node_t r) {
	node_t n = Orb_gc_malloc(sizeof(struct node_s));
	n->p = p;
	n->l = l;
	n->r = r;
}

static node_t node_inserted(node_t n, void* p, Orb_bs_tree_comparer_t cf, void** found) {
	if(n == 0) {
		return node_ctor(p, 0, 0);
	}

	int rv = cf(p, n->p);
	/*TODO: tree balancing*/
	if(rv == 0) {
		*found = n->p;
		return n;
	} else if(rv < 0) {
		node_t new_n = node_inserted(n->l, p, cf, found);
		if(new_n == n->l) return n;
		else return node_ctor(n->p, new_n, n->r);
	} else if(rv > 0) {
		node_t new_n = node_inserted(n->r, p, cf, found);
		if(new_n == n->r) return n;
		else return node_ctor(n->p, n->l, new_n);
	}
}

void* Orb_bs_tree_insert(Orb_bs_tree_t tree, void* p) {
	Orb_t on = Orb_cell_get(tree->cell);
	node_t n;
	void* found;
top:
	n = Orb_t_as_pointer(on);
	node_t new_n = node_inserted(n, p, tree->cf, &found);
	if(new_n == n) {
		return found;
	} else {
		Orb_t ocheck = Orb_cell_cas_get(tree->cell, on,
			Orb_t_from_pointer(new_n)
		);
		if(ocheck != on) {
			on = ocheck;
			goto top;
		} else {
			return p;
		}
	}
}

