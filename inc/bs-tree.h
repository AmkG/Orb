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

