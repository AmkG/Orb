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
#include<stdio.h>
#include<stdlib.h>

#include"thread-support.h"

struct test {
	size_t x;
	size_t y;
};

Orb_cell_t ctest;

Orb_t test_cfunc(Orb_t argv[], size_t* pargc, size_t argl) {
	if(*pargc != 1) {
		fprintf(stderr, "Incorrect number of arguments to task function\n");
		exit(1);
	}

	for(;;) {
		Orb_t otest = Orb_cell_get(ctest);
		struct test* ptest = Orb_t_as_pointer(otest);
		assert(ptest->x > 0);
		struct test* ntest = Orb_gc_malloc(sizeof(struct test));
		ntest->x = ptest->x - 1;
		ntest->y = ptest->y + 1;
		if(Orb_cell_cas(ctest, otest, Orb_t_from_pointer(ntest))) {
			break;
		}
	}
	return Orb_NIL;
}

int main(void) {
	Orb_init(0, 0);

	struct test* tmp = Orb_gc_malloc(sizeof(struct test));
	tmp->x = 100;
	tmp->y = 0;
	ctest = Orb_cell_init(Orb_t_from_pointer(tmp));

	Orb_t f = Orb_t_from_cfunc(&test_cfunc);

	size_t i;
	for(i = 0; i < 100; ++i) {
		Orb_thread_pool_add(f);
	}

	size_t track_x;
	size_t tries = 0;
	do {
		Orb_yield();
		Orb_t otest = Orb_cell_get(ctest);
		tmp = Orb_t_as_pointer(otest);
		track_x = tmp->x;
		++tries;
		if(tries > 10000000) {
			fprintf(stderr, "Timed out!\n");
			exit(2);
		}
	} while(track_x > 0);

	Orb_t otest = Orb_cell_get(ctest);
	tmp = Orb_t_as_pointer(otest);
	assert(tmp->x == 0 && tmp->y == 100);

	exit(0);
}

