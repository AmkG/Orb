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
#include<liborb.h>

#include<assert.h>
#include<stdlib.h>

#include"thread-support.h"

Orb_cell_t common;

Orb_t increment_common_cf0(void) {
	Orb_t tmp = Orb_cell_get(common);
	for(;;) {
		int itmp = Orb_t_as_integer(tmp);
		++itmp;
		Orb_t read = Orb_cell_cas_get(common,
			tmp, Orb_t_from_integer(itmp)
		);
		if(read == tmp) break;
		tmp = read;
	}
	return Orb_NIL;
}

Orb_t secret;

Orb_t get_secret_cf0(void) {
	return secret;
}

int main(void) {
	size_t retries;

	Orb_init(0, 0);

	common = Orb_cell_init(Orb_t_from_integer(0));
	secret = Orb_t_from_pointer(&secret);

	Orb_t dont_care = Orb_defer(Orb_t_from_cf0(&increment_common_cf0));

	retries = 0;
	for(;;) {
		if(retries > 100000) {
			int increment_timed_out = 0;
			assert(increment_timed_out);
		}
		Orb_yield();
		Orb_t tmp = Orb_cell_get(common);
		if(tmp == Orb_t_from_integer(1)) break;
	}

	Orb_t f0 = Orb_t_from_cf0(&get_secret_cf0);
	Orb_t df0 = Orb_defer(f0);

	assert(Orb_call0(df0) == Orb_call0(f0));
	assert(Orb_call0(df0) == secret);

	exit(0);
}

