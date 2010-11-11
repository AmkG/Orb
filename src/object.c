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

/*
Objects are tagged tuples, represented by simple
flat arrays.  The 0th entry is an opaque pointer
to a format, which is actually a somewhat richer
structure.  Succeeding entries of the flat array
are simply the tuple entries.

The format is another flat array.  The 0th entry
is the number of fields in the format, the 1th
entry is the name of the format.

Succeeding entries in the format form a sequence
of field-offset pairs.  The fields are arranged
in identity order.  So for example suppose we
have a constructor like the following:

  (Def-Cons (*cons a d))

Assuming that by identity order, 'd < 'a:

  format[0] = 2
  format[1] = '*cons
  format[2] = 'd
  format[3] = 2
  format[4] = 'a
  format[5] = 1
*/

Orb_t Orb_SYMBOLFORMAT;
Orb_t Orb_NUMBERFORMAT;
Orb_t Orb_POINTERFORMAT;

/*format for constructors*/
static Orb_t consf;
static Orb_t econsf; /*empty constructors*/

/*cfunc pointers for constructors*/
static Orb_t cons_cf;
static Orb_t econs_cf;

/*search through list of fields*/
static size_t binsearch(Orb_t* fields, size_t sz, Orb_t f) {
	size_t lo = 0, hi = sz;
	while(lo != hi) {
		size_t mid = lo + (hi - lo) / 2;
		Orb_t test = fields[2 * mid];
		if(f < test) {
			hi = mid;
		} else if(test < f) {
			lo = mid + 1;
		} else {
			return mid;
		}
	}
	return lo;
}

void Orb_priv_cons_init(Orb_priv_cons* d) {
	d->sz = 0;
	d->format = 0;
	d->name = Orb_NOTFOUND;
	d->reserved = 0;
}
void Orb_priv_cons_field(Orb_priv_cons* d, Orb_t f) {
	if(d->format == 0) {
		/*initialize the format pointer*/
		size_t nln = 6;
		d->format = Orb_gc_malloc(nln * sizeof(Orb_t));
		d->ln = nln;
	} else if(d->sz * 2 + 2 == d->ln) {
		/*not enough space in the format*/
		size_t nln = d->ln + d->ln / 2;
		if(nln & 1) ++nln; /*keep it even*/
		Orb_t* nformat = Orb_gc_malloc(nln * sizeof(Orb_t));
		memcpy(&nformat[2], &d->format[2],
			(d->sz * 2) * sizeof(Orb_t)
		);
		Orb_gc_free(d->format);
		d->format = nformat;
		d->ln = nln;
	}
	/*find insertion point*/
	size_t i = binsearch(&d->format[2], d->sz, f);
	if(i < d->sz) {
		/*move insertion point*/
		memmove(&d->format[(i + 1) * 2 + 2], &d->format[i * 2 + 2],
			((d->sz - i) * 2) * sizeof(Orb_t)
		);
	}
	/*insert it*/
	d->format[2 + i * 2] = f;
	d->format[2 + i * 2 + 1] = Orb_t_from_integer(d->sz);
	++d->sz;
}
Orb_t Orb_priv_cons_finish(Orb_priv_cons* d) {
	/*is it an empty format?*/
	if(d->sz == 0) {
		/*empty constructor:
			(*econsf **cfunc** format ob)
		*/
		/*The format doesn't exist yet.  Create
		one.
		*/
		Orb_t* format = Orb_gc_malloc(2 * sizeof(Orb_t));
		format[0] = Orb_t_from_integer(0);
		format[1] = d->name;
		Orb_t oformat = Orb_t_from_pointer(format);

		/*now create the object*/
		Orb_t* obj = Orb_gc_malloc(1 * sizeof(Orb_t));
		obj[0] = oformat;
		Orb_t oo = ((Orb_t) obj) + Orb_TAG_OBJECT;

		/*finally create the constructor*/
		Orb_t* rv = Orb_gc_malloc(4 * sizeof(Orb_t));
		rv[0] = econsf;
		rv[1] = econs_cf;
		rv[2] = oformat;
		rv[3] = oo;
		Orb_t orv = ((Orb_t) obj) + Orb_TAG_OBJECT;

		return orv;
	} else {
		/*non-empty constructor:
			(*consf **cfunc** format)
		*/
		d->format[0] = Orb_t_from_integer(d->sz);
		d->format[1] = d->name;
		Orb_t oformat = Orb_t_from_pointer(format);

		Orb_t* rv = Orb_gc_malloc(3 * sizeof(Orb_t));
		rv[0] = consf;
		rv[1] = cons_cf;
		rv[2] = oformat;
		Orb_t orv = ((Orb_t) obj) + Orb_TAG_OBJECT;

		return orv;
	}
}

