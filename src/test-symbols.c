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

int main(void) {
	assert(Orb_symbol("<foo>foo") == Orb_symbol_cc("<foo>foo"));
	Orb_t tmp = Orb_symbol_cc("<foo>foo");
	Orb_gc_trigger();
	Orb_t tmp2 = Orb_symbol_cc("<foo>foo");
	assert(tmp == tmp2);

	Orb_t tmp3 = Orb_symbol_cc("<bar>bar");
	Orb_gc_trigger();
	assert(tmp3 != tmp);
	assert(tmp3 != tmp2);
	assert(tmp2 == tmp);
}

