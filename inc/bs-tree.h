#ifndef BS_TREE_H
#define BS_TREE_H

struct Orb_bs_tree_s;
typedef struct Orb_bs_tree_s* Orb_bs_tree_t;

/*return
	-1 if a < b
	0  if a == b
	+1 if a > b
*/
typedef int Orb_bs_tree_comparer_f(void* a, void* b);
typedef Orb_bs_tree_comparer_f* Orb_bs_tree_comparer_t;

Orb_bs_tree_t Orb_bs_tree_init(Orb_bs_tree_comparer_t);
/*returns the input if it was inserted into the tree, or
the existing value if it already exists.
*/
void* Orb_bs_tree_insert(Orb_bs_tree_t, void*);
/*returns 0 if not found*/
void* Orb_bs_tree_lookup(Orb_bs_tree_t, void*);

#endif /* BS_TREE_H */

