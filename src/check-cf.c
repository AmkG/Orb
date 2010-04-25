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
	Orb_init(0, 0);

	Orb_t test = Orb_symbol("test-symbol");

	/*check that calling the C extension is roughly equivalent
	to calling the C function directly
	*/
	Orb_t oderef = Orb_t_from_cf2(&Orb_deref); /*deref doesn't perform method binding*/
	Orb_t test1_res1 = Orb_deref_cc(test, "write");
	Orb_t test1_res2 = Orb_call2(oderef, test, Orb_symbol_cc("write"));
	assert(test1_res1 == test1_res2);

	/*Check that CELfree C extensions work too
	*/
	oderef = Orb_CELfree(oderef);
	Orb_t test2_res1 = Orb_deref_cc(test, "write");
	Orb_t test2_res2 = Orb_call2(oderef, test, Orb_symbol_cc("write"));
	assert(test2_res1 == test2_res2);

	return 0;
}
