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
#include"bool.h"

static Orb_t oftrue;
static Orb_t offalse;

int Orb_bool(Orb_t val) {
	Orb_t oif = Orb_ref_cc(val, "if");
	if(oif == Orb_NOTFOUND) {
		Orb_THROW_cc("if",
			"Value not convertible to boolean"
		);
	}
	Orb_safetycheck(oif, Orb_SAFE(1) | Orb_SAFE(2));

	Orb_t rv = Orb_call2(oif, oftrue, offalse);

	return rv == Orb_TRUE;
}

static Orb_t ftrue(Orb_t argv[], size_t* pargc, size_t argl) {
	return Orb_TRUE;
}
static Orb_t ffalse(Orb_t argv[], size_t* pargc, size_t argl) {
	return Orb_NIL;
}

void Orb_bool_init(void) {
	Orb_gc_defglobal(&oftrue);
	Orb_gc_defglobal(&offalse);

	oftrue = Orb_t_from_cfunc(&ftrue);
	offalse = Orb_t_from_cfunc(&ffalse);
}

