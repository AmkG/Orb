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

#include<assert.h>

Orb_t dummy(Orb_t argv[], size_t* pargc, size_t argl) {
	return Orb_NOTFOUND;
}

int main(void) {
	Orb_init(0,0);

	assert(Orb_bool(Orb_TRUE));
	assert(!Orb_bool(Orb_NIL));
	assert(!Orb_bool(Orb_NOTFOUND));
	assert(Orb_bool(Orb_t_from_cfunc(&dummy)));

	return 0;
}

