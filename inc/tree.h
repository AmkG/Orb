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
#ifndef TREE_H
#define TREE_H

struct Orb_tree_s;
typedef struct Orb_tree_s* Orb_tree_t;

typedef int Orb_tree_comparer(Orb_t, Orb_t);
typedef Orb_tree_comparer* Orb_tree_comparer_t;

Orb_tree_t Orb_tree_init(Orb_tree_comparer_t cmp);
Orb_tree_t Orb_tree_insert(Orb_tree_t tree, Orb_t k, Orb_t v);
Orb_t Orb_tree_lookup(Orb_tree_t tree, Orb_t k);

#endif /* TREE_H */

