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
#ifndef OBJECT_H
#define OBJECT_H

#include"liborb.h"

/*The function below is Orb_method(), but does not
check if the given function is an already-bound
method.  It is intended for use during
initialization, when it is not safe to
dereference objects just yet.

This function requires that:
	1. the input is not an already-bound method
	2. the input has no orb-safety requirements
	(i.e. add orb-safety afterwards, not before!)
*/
Orb_t Orb_method_assured(Orb_t);

/*used in a simple copying collector (i.e. the kfunc calling
convention)
*/
int Orb_evacuate_object(Orb_t* ptarget, Orb_t v);
void Orb_evacuate_fields(Orb_t v,
		int pred(Orb_t,void*), void* pred_clos,
		void inform(int, Orb_t, void*), void* inform_clos);

/*initialization*/
void Orb_object_init_before_symbol(void);
void Orb_object_init_after_symbol(void);

#endif /* OBJECT_H */

