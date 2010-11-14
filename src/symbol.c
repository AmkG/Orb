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
#include"symbol.h"
#include"object.h"
#include"thread-support.h"
#include"tree.h"

#include<memory.h>

/*cell containing the symbol table*/
static Orb_cell_t csym;

/*comparator for string representations*/
static int symcmp(Orb_t oa, Orb_t ob) {
	char const* a = Orb_t_as_pointer(oa);
	char const* b = Orb_t_as_pointer(ob);
	while(*a && *b) {
		if(*a < *b) return 1;
		if(*b < *a) return 0;
		++a; ++b;
	}
	if(*b) return 0;
	if(*a) return 1;
}

/*cell containing the constant string->symbol table*/
static Orb_cell_t c_cc;

static int cc_cmp(Orb_t oa, Orb_t ob) {
	return oa < ob;
}

void Orb_symbol_init(void) {
	Orb_tree_t init_tree = Orb_tree_init(&symcmp);

	Orb_gc_defglobal(&csym);
	csym = Orb_cell_init(Orb_t_from_pointer(init_tree));

	Orb_tree_t init_cc_tree = Orb_tree_init(&cc_cmp);
	Orb_gc_defglobal(&c_cc);
	c_cc = Orb_cell_init(Orb_t_from_pointer(init_cc_tree));
}

/*symbol lookup
 * cs is the string to look up.
 * If the string does not have a symbol yet, copy_flag
 *   determines if a copy must be made before associating
 *   it with the symbol, or if the data is already in a
 *   buffer that won't be freed or modified by the rest
 *   of the program.
 */
static Orb_t symbol_direct(char const* cs, int copy_flag) {
	Orb_t ocs = Orb_t_from_pointer(cs);
	Orb_t ostb = Orb_cell_get(csym);
	Orb_t asym = Orb_NOTFOUND;
	for(;;) {
		Orb_tree_t stb = Orb_t_as_pointer(ostb);
		Orb_t rv = Orb_tree_lookup(stb, ocs);
		if(rv != Orb_NOTFOUND) {
			return rv;
		} else {
			/*no symbol in table, construct one*/
			/*check if a new symbol needs to be
			alloced
			*/
			if(asym == Orb_NOTFOUND) {
				/*check if cs has to be copied*/
				if(copy_flag) {
					size_t l = strlen(cs) + 1;
					char* ncs = Orb_gc_malloc(l);
					memcpy(ncs, cs, l);
					copy_flag = 0;
					cs = ncs;
					ocs = Orb_t_from_pointer(cs);
				}

				Orb_cell_t slot = Orb_cell_init(unbound);

				Orb_t* aas = Orb_gc_malloc(
					(Orb_SYMBOLSIZE + 1) * sizeof(Orb_t)
				);
				aas[0] = Orb_SYMBOLFORMAT;
				aas[1] = ocs;
				aas[2] = Orb_t_from_pointer(slot);

				asym = ((Orb_t) aas) + Orb_TAG_OBJECT;
			}

			/*insert into tree*/
			Orb_tree_t nstb = Orb_tree_insert(stb, ocs, asym);

			/*modify symbol table cell*/
			Orb_t onstb = Orb_t_from_pointer(nstb);
			Orb_t old = Orb_cell_cas_get(csym, ostb, onstb);
			if(old == ostb) {
				return asym;
			} else {
				ostb = old;
			}
		}
	}
}

/*returns a symbol, assuming that the string is **not**
a constant, global string.  Thus if a symbol must be
created, the string is copied.
*/
Orb_t Orb_symbol(char const* cs) {
	return symbol_direct(cs, 1);
}

/*returns a symbol, assuming that the string is a constant
global string that will exist for as long as the program
exists.  It should be used with string literals.
We assume that every call site of Orb_symbol_cc will always
call it with the same address.
*/
Orb_t Orb_symbol_cc(char const* cs) {
	Orb_t ocs = Orb_t_from_pointer(cs);

	Orb_t occtb = Orb_cell_get(c_cc);
	Orb_tree_t cctb = Orb_t_as_pointer(occtb);

	Orb_t rv = Orb_tree_lookup(cctb, ocs);
	if(rv != Orb_NOTFOUND) {
		return rv;
	}

	/*cs is constant, so we don't have to alloc extra
	space for it.
	*/
	rv = symbol_direct(cs, 0);

	/*now insert it into the cc tree.  We just keep inserting*/
	for(;;) {
		Orb_tree_t ncctb = Orb_tree_insert(cctb, ocs, rv);
		Orb_t oncctb = Orb_t_from_pointer(ncctb);

		Orb_t old = Orb_cell_cas_get(c_cc, occtb, oncctb);
		if(old == occtb) {
			return rv;
		} else {
			occtb = old;
			cctb = Orb_t_as_pointer(occtb);
		}
	}
}

/*returns a symbol, where the given string may contain null
characters.
Internally we use modified UTF-8, where NULL's are encoded
with 0xC0, 0x80.
*/
Orb_t Orb_symbol_sz(char const* cs, size_t sz) {
	/*degenerate case*/
	if(sz == 0) {
		return Orb_symbol(cs);
	}
	size_t ln = sz + sz / 2 + 2;

	char* ncs = Orb_gc_malloc(ln);
	size_t i, j;
	for(i = 0, j = 0; i < sz; ++i, ++j) {
		if(j + 1 == ln) {
			/*realloc*/
			size_t rln = ln + ln / 2;
			char* rcs = Orb_gc_malloc(rln);
			memmove(rcs, ncs, j);
			ncs = rcs;
			ln = rln;
		}
		if(cs[i] == 0) {
			ncs[j] = 0xC0;
			ncs[j + 1] = 0x80;
			++j;
			if(j + 1 == ln) {
				/*realloc*/
				size_t rln = ln + ln / 2;
				char* rcs = Orb_gc_malloc(rln);
				memmove(rcs, ncs, j);
				ncs = rcs;
				ln = rln;
			}
		} else {
			ncs[j] = cs[i];
		}
	}
	ncs[j] = 0;

	/*no need to copy*/
	return symbol_direct(ncs, 0);
}

