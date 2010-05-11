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
#ifndef LIST_H
#define LIST_H

#include"liborb.h"

/*defines a simple singly-linked list*/
struct list_s {
	Orb_t value;
	struct list_s* next;
};
typedef struct list_s list;
typedef list* list_t;

static inline list_t list_cons(Orb_t value, list_t next) {
	list_t rv = Orb_gc_malloc(sizeof(list));
	rv->value = value;
	rv->next = next;
	return rv;
}

#endif /* LIST_H */

