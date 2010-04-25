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

#include<stdio.h>
#include<string.h>

#include<assert.h>

struct stringbuilder {
	char* string;
	size_t size;
	size_t capacity;
};

void prebuild(struct stringbuilder* sb) {
	sb->string = 0;
	sb->size = 0;
	sb->capacity = 0;
}
void build(struct stringbuilder* sb, char c) {
	if(sb->size == sb->capacity) {
		size_t nc = sb->capacity + sb->capacity / 2;
		if(nc < 4) nc = 4;
		char* ns = malloc(nc);
		memcpy(ns, sb->string, sb->size);
		free(sb->string);
		sb->string = ns; sb->capacity = nc;
	}
	sb->string[sb->size] = c;
	sb->string[sb->size + 1] = 0;
	++sb->size;
}

Orb_t prc(Orb_t argv[], size_t* pargc, size_t argl) {
	if(*pargc != 2) {
		Orb_THROW_cc("apply", "incorrect number of arguments to prc");
	}
	Orb_t self = argv[0];

	Orb_t osb = Orb_deref_cc(self, "*sb");
	struct stringbuilder* sb = Orb_t_as_pointer(osb);

	Orb_t oc = argv[1];
	char c = Orb_t_as_integer(oc);

	build(sb, c);

	return Orb_NIL;
}
Orb_t prs(Orb_t argv[], size_t* pargc, size_t argl) {
	if(*pargc != 1) {
		Orb_THROW_cc("apply", "incorrect number of arguments to prs");
	}
	Orb_t self = argv[0];

	Orb_t osb = Orb_deref_cc(self, "*sb");
	struct stringbuilder* sb = Orb_t_as_pointer(osb);

	build(sb, ' ');

	return Orb_NIL;
}
Orb_t pro(Orb_t argv[], size_t* pargc, size_t argl) {
	if(*pargc != 2) {
		Orb_THROW_cc("apply", "incorrect number of arguments to pro");
	}
	Orb_t opro = argv[0];

	Orb_t osb = Orb_deref_cc(opro, "*sb");

	Orb_t oprc; Orb_t oprs;
	Orb_BUILDER {
		Orb_B_PARENT(Orb_t_from_cfunc(&prc));
		Orb_B_FIELD_cc("*sb", osb);
	} oprc = Orb_ENDBUILDER;
	Orb_BUILDER {
		Orb_B_PARENT(Orb_t_from_cfunc(&prs));
		Orb_B_FIELD_cc("*sb", osb);
	} oprs = Orb_ENDBUILDER;

	Orb_t owrite = Orb_ref_cc(argv[1], "write");

	Orb_safetycheck(owrite, Orb_SAFE(1) | Orb_SAFE(2) | Orb_SAFE(3));
	Orb_call3(owrite, oprc, oprs, opro);

	return Orb_NIL;
}

int main(void) {
	Orb_init(0, 0);
	Orb_t x = Orb_symbol("hello");
	Orb_t y = Orb_symbol_cc("hello");
	assert(x == y);
	Orb_gc_trigger();
	Orb_t z = Orb_symbol_cc("goodbye");
	Orb_gc_trigger();
	Orb_t w = Orb_symbol("goodbye");
	assert(z == w);
	assert(x != z);

	Orb_t xw = Orb_deref_cc(x, "write");
	Orb_t zw = Orb_deref_cc(z, "write");
	assert(zw == xw);

	xw = Orb_ref_cc(x, "write");
	zw = Orb_ref_cc(z, "write");
	assert(xw != zw);

	struct stringbuilder sb;
	prebuild(&sb);

	Orb_t opro;
	Orb_BUILDER {
		Orb_B_PARENT(Orb_t_from_cfunc(&pro));
		Orb_B_FIELD_cc("*sb", Orb_t_from_pointer(&sb));
	} opro = Orb_ENDBUILDER;

	Orb_t rv = Orb_call1(opro, x);
	assert(rv == Orb_NIL);
	assert(strcmp(sb.string, "hello") == 0);

	free(sb.string);

	return 0;
}

