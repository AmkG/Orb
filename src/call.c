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

Orb_t Orb_call_ex(Orb_t argv[], size_t argc, size_t argl) {
	Orb_t rv;
	Orb_t f;
	do {
	top:
		f = argv[0];
		/*check for **call** field*/
		Orb_t check = Orb_ref_cc(f, "**call**");
		if(check != Orb_NOTFOUND) {
			argv[0] = check; goto top;
		}
		/*check for **cfunc** field*/
		check = Orb_deref_cc(f, "**cfunc**");
		if(check != Orb_NOTFOUND) {
			/*extract the cfunc*/
			Orb_cfunc* pf = Orb_t_as_pointer(check);
			Orb_cfunc cf = *pf;
			rv = cf(argv, &argc, argl);
		} else {
			Orb_THROW_cc("apply", "Call to object that cannot be called");
		}
	} while(rv == Orb_TRAMPOLINE);
	return rv;
}

void Orb_safetycheck(Orb_t f, size_t safety) {
	if(safety != 0) {
		Orb_t ofsafety = Orb_deref_cc(f, "**orbsafety**");
		if(ofsafety != Orb_NOTFOUND) {
			size_t fsafety = Orb_t_as_integer(ofsafety);
			/*check*/
			if((fsafety & safety) == safety) return;
		}
		Orb_THROW_cc("apply",
			"Attempt to call non-orb-safe function with orbs"
		);
	}
}
Orb_t Orb_bless_safety(Orb_t f, size_t safety) {
	Orb_BUILDER {
		Orb_B_PARENT(f);
		Orb_B_FIELD_AS_IF_VIRTUAL_cc("**orbsafety**",
			Orb_t_from_integer(safety)
		);
	} return Orb_ENDBUILDER;
}

