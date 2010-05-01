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
#include"seq.h"

#include<string.h>

/*
sequences have the following interface:
(def empty-seq
  (obj!extend
    'as-seq (method:fn (self) self)
    'len (virtual 0)
    'decompose
    (virtual:fn (@f0 @f1 @f2)
      (@f0))
    'iterate
    (method:fn (self @f)
      (@f (@fn ()  (get-current-value))
          (@fn (x) (increment-iterator-by x))
          (@fn ()  (check-for-end-of-iterator))))
    'map
    (method:fn (self f)
      (map f self))
    'mapreduce
    (method:fn (self mf rf zero)
      (mapreduce self mf rf zero))))
(empty-seq!extend
  'len N     ; length of the sequence
  'decompose
  (method:fn (self @f0 @f1 @f2)
    (if
      (is self!len 0)
        (@f0)
      (is self!len 1)
        ; give the single element of this
        ; one-item sequence
        (@f1 self!the-single-element)
      ; else
        ; give two subsequences of this sequence
        (@f2 self!left-subsequence self!right-subsequence))))
*/
/*
Internally, sequences are of the following types:
1.  empty sequences.  While there is only one canonical
    empty sequence, the user may define a sequence as empty,
    so empty checks must use characteristics of the
    sequence rather than equality.
2.  single-item sequence.
3.  array sequences, where the sequence is really backed
    by an array.  The iterate, map, and mapreduce operations
    are optimized for this case.
4.  concatenation cells, where the sequence is composed of
    two smaller sequences.

Type 3, array sequences, are, internally, treated specially by
iterate, map, and mapreduce.  Those functions use the
Orb_array_backed() function to decompose an array sequence
and work on the backing array directly.
*/

/*keys for hidden fields
hfield1 is used by singleton types for the single value
hfield1 is used by array types for the pointer to backing array,
 and hfield2 (if present) is used for the offset within the
 backing array.
hfield1, hfield2 is used by concatenation types for left and right
*/
static Orb_t o_hfield1;
static Orb_t o_hfield2;

static Orb_t o_empty;
static Orb_t o_single_base;
static Orb_t o_arr_base;
static Orb_t o_conc_base;

/*contains the method for decomposing arrays.  Used to recognize
array sequences.
*/
static Orb_t o_arr_decompose;

/*identity function for as-seq*/
static Orb_t idfn(Orb_t argv[], size_t* pargc, size_t argl) {
	if(*pargc != 2) {
		Orb_THROW_cc("apply",
			"Incorrect number of arguments to as-seq"
		);
	}
	return argv[1];
}

static Orb_t empty_decompose(Orb_t argv[], size_t* pargc, size_t argl) {
	argv[0] = argv[1];
	*pargc = 1;
	return Orb_TRAMPOLINE;
}
/*method: self @f0 @f1 @f2*/
static Orb_t single_decompose(Orb_t argv[], size_t* pargc, size_t argl) {
	Orb_t self = argv[1];
	Orb_t f1 = argv[3];

	Orb_t item = Orb_deref(self, o_hfield1);
	argv[0] = f1;
	argv[1] = item;
	*pargc = 2;
	return Orb_TRAMPOLINE;
}
/*method: self @f0 @f1 @f2*/
static Orb_t conc_decompose(Orb_t argv[], size_t* pargc, size_t argl) {
	Orb_t self = argv[1];
	Orb_t f2 = argv[4];

	Orb_t l = Orb_deref(self, o_hfield1);
	Orb_t r = Orb_deref(self, o_hfield2);
	argv[0] = f2;
	argv[1] = l;
	argv[2] = r;
	*pargc = 3;
	return Orb_TRAMPOLINE;
}

/*a bit more complex: defined at the end*/
static Orb_t arr_decompose(Orb_t argv[], size_t* pargc, size_t argl);

void Orb_seq_init(void) {
	Orb_iterate_init();
	Orb_map_init();
	Orb_mapreduce_init();

	Orb_gc_defglobal(&o_hfield1);
	Orb_gc_defglobal(&o_hfield2);

	Orb_gc_defglobal(&o_empty);
	Orb_gc_defglobal(&o_single_base);
	Orb_gc_defglobal(&o_arr_base);
	Orb_gc_defglobal(&o_conc_base);

	Orb_gc_defglobal(&o_arr_decompose);

	o_hfield1 = Orb_t_from_pointer(&o_hfield1);
	o_hfield2 = Orb_t_from_pointer(&o_hfield2);

	/*empty sequence*/
	Orb_BUILDER {
		Orb_B_PARENT(Orb_OBJECT);
		Orb_B_FIELD_cc("as-seq", Orb_method(Orb_t_from_cfunc(&idfn)));
		Orb_B_FIELD_cc("len", Orb_virtual(Orb_t_from_integer(0)));
		Orb_B_FIELD_cc("decompose",
			Orb_virtual(
				Orb_bless_safety(
					Orb_t_from_cfunc(&empty_decompose),
					Orb_SAFE(1) | Orb_SAFE(2) | Orb_SAFE(3)
				)
			)
		);
		Orb_B_FIELD_cc("iterate",
			Orb_method(
				Orb_bless_safety(
					Orb_t_from_cfunc(&Orb_iterate_cfunc),
					Orb_SAFE(2)
				)
			)
		);
		Orb_B_FIELD_cc("map",
			Orb_method(
				Orb_t_from_cfunc(&Orb_map_cfunc)
			)
		);
		Orb_B_FIELD_cc("mapreduce",
			Orb_method(
				Orb_t_from_cfunc(&Orb_mapreduce_cfunc)
			)
		);
	} o_empty = Orb_ENDBUILDER;

	/*single sequence*/
	Orb_BUILDER {
		Orb_B_PARENT(o_empty);
		Orb_B_FIELD_cc("len", Orb_t_from_integer(1));
		Orb_B_FIELD_cc("decompose",
			Orb_method(
				Orb_bless_safety(
					Orb_t_from_cfunc(&single_decompose),
					Orb_SAFE(2) | Orb_SAFE(3) | Orb_SAFE(4)
				)
			)
		);
	} o_single_base = Orb_ENDBUILDER;

	/*array sequence decomposition*/
	o_arr_decompose =
		Orb_method(
			Orb_bless_safety(
				Orb_t_from_cfunc(&arr_decompose),
				Orb_SAFE(2) | Orb_SAFE(3) | Orb_SAFE(4)
			)
		)
	;

	/*array sequence*/
	Orb_BUILDER {
		Orb_B_PARENT(o_empty);
		Orb_B_FIELD_cc("decompose", o_arr_decompose);
	} o_arr_base = Orb_ENDBUILDER;

	/*concatenation sequence*/
	Orb_BUILDER {
		Orb_B_PARENT(o_empty);
		Orb_B_FIELD_cc("decompose",
			Orb_method(
				Orb_bless_safety(
					Orb_t_from_cfunc(&conc_decompose),
					Orb_SAFE(2) | Orb_SAFE(3) | Orb_SAFE(4)
				)
			)
		);
	} o_conc_base = Orb_ENDBUILDER;
}

/*array decomposition*/
/*method: self f0 f1 f2*/
static Orb_t arr_decompose(Orb_t argv[], size_t* pargc, size_t argl) {
#define argc (*pargc)

	if(argc != 4) {
		Orb_THROW_cc("apply",
			"Incorrect number of arguments to array!decompose"
		);
	}

	Orb_t self = argv[1];
	Orb_t opbackingarr = Orb_deref(self, o_hfield1);
	Orb_t* pbackingarr = Orb_t_as_pointer(opbackingarr);

	Orb_t* parr = pbackingarr;
	Orb_t ooffset = Orb_deref(self, o_hfield2);
	size_t offset = 0;
	if(ooffset != Orb_NOTFOUND) {
		offset = Orb_t_as_integer(offset);
		parr += offset;
	}

	Orb_t olen = Orb_deref_cc(self, "len");
	size_t len = Orb_t_as_integer(olen);
	if(len == 1) {
		Orb_t f1 = argv[3];
		Orb_t val = *parr;
		argv[0] = f1;
		argv[1] = val;
		argc = 2;
		return Orb_TRAMPOLINE;
	}

	/*split into two sub-arrays*/
	/*note that this behavior means that decomposing an array is actually
	less efficient (i.e. requires more garbage collection) than simply
	using the built-in map/iterate/mapreduce.
	*/
	Orb_t l; Orb_t r;

	size_t llen = len / 2;
	size_t lstart = offset;
	size_t rlen = len - llen;
	size_t rstart = offset + llen;

	Orb_BUILDER {
		Orb_B_PARENT(o_arr_base);
		Orb_B_FIELD_cc("len", Orb_t_from_integer(llen));
		Orb_B_FIELD(o_hfield1, opbackingarr);
		Orb_B_FIELD(o_hfield2, Orb_t_from_integer(lstart));
	} l = Orb_ENDBUILDER;
	Orb_BUILDER {
		Orb_B_PARENT(o_arr_base);
		Orb_B_FIELD_cc("len", Orb_t_from_integer(rlen));
		Orb_B_FIELD(o_hfield1, opbackingarr);
		Orb_B_FIELD(o_hfield2, Orb_t_from_integer(rstart));
	} r = Orb_ENDBUILDER;

	Orb_t f2 = argv[4];

	argv[0] = f2;
	argv[1] = l;
	argv[2] = r;
	argc = 3;
	return Orb_TRAMPOLINE;

#undef argc
}

int Orb_array_backed(Orb_t seq, Orb_t const** parr, size_t* pstart, size_t* psz) {
	Orb_t decompose = Orb_deref_cc(seq, "decompose");
	if(decompose != o_arr_decompose) return 0;

	Orb_t opbackingarr = Orb_deref(seq, o_hfield1);
	if(opbackingarr == Orb_NOTFOUND) return 0;

	/*extract backing array and other components*/
	*parr = Orb_t_as_pointer(opbackingarr);
	*pstart = 0;
	Orb_t ostart = Orb_deref(seq, o_hfield2);
	if(ostart != Orb_NOTFOUND) {
		*pstart = Orb_t_as_integer(ostart);
	}
	*psz = Orb_t_as_integer(Orb_deref_cc(seq, "len"));

	return 1;
}

Orb_t Orb_seq(Orb_t* arr, size_t sz) {
	if(sz == 0) return o_empty;
	if(sz == 1) {
		Orb_BUILDER {
			Orb_B_PARENT(o_single_base);
			Orb_B_FIELD(o_hfield1, *arr);
		} return Orb_ENDBUILDER;
	}
	/*create a backing array*/
	Orb_t* narr = Orb_gc_malloc(sz * sizeof(Orb_t));
	memcpy(narr, arr, sz * sizeof(Orb_t));
	/*return an array-backed sequence*/
	Orb_BUILDER {
		Orb_B_PARENT(o_arr_base);
		Orb_B_FIELD(o_hfield1, Orb_t_from_pointer(narr));
		Orb_B_FIELD_cc("len", Orb_t_from_integer(sz));
	} return Orb_ENDBUILDER;
}

Orb_t Orb_ensure_seq(Orb_t seq) {
	Orb_t nseq;
loop:;
	Orb_t as_seq = Orb_ref_cc(seq, "as-seq");
	if(Orb_NOTFOUND == as_seq) goto error;
	nseq = Orb_call0(as_seq);
	if(seq != nseq) { seq = nseq; goto loop; }

	Orb_t decompose = Orb_deref_cc(seq, "decompose");
	if(Orb_NOTFOUND == decompose) goto error;

	return nseq;
error:
	Orb_THROW_cc("type", "Expected a sequence");
}

Orb_t Orb_conc(Orb_t l, Orb_t r) {
	l = Orb_ensure_seq(l);
	r = Orb_ensure_seq(r);

	size_t llen = Orb_len(l);
	if(llen == 0) {
		return r;
	}
	size_t rlen = Orb_len(r);
	if(rlen == 0) {
		return l;
	}

	Orb_BUILDER {
		Orb_B_PARENT(o_conc_base);
		Orb_B_FIELD_cc("len", Orb_t_from_integer(llen + rlen));
		Orb_B_FIELD(o_hfield1, l);
		Orb_B_FIELD(o_hfield2, r);
	} return Orb_ENDBUILDER;
}

Orb_t Orb_nth_o(Orb_t seq, Orb_t oi) {
	size_t i = Orb_t_as_integer(oi);
	seq = Orb_ensure_seq(seq);

loop:;
	/*check if array*/
	Orb_t const* arr; size_t start; size_t len;
	if(Orb_array_backed(seq, &arr, &start, &len)) {
		if(i < len) {
			return arr[start + i];
		} else {
			goto error;
		}
	}

	/*decompose instead*/
	Orb_t single; Orb_t l; Orb_t r;
	switch(Orb_seq_decompose(seq, &single, &l, &r)) {
	case 0: goto error;
	case 1: if(i == 0) return single;
		else goto error;
	case 2: {
		size_t llen = Orb_len(l);
		if(i < llen) {
			seq = l;
			goto loop;
		}
		seq = r;
		i -= llen;
		goto loop;
	}
	}

error:
	Orb_THROW_cc("value", "Sequence index out of range");
}

Orb_t Orb_len_o(Orb_t seq) {
	seq = Orb_ensure_seq(seq);

	return Orb_deref_cc(seq, "len");
}

/*
 * Orb_seq_decompose() implementation
 */

static Orb_t seq_decompose_f0(Orb_t argv[], size_t* pargc, size_t argl);
static Orb_t seq_decompose_f1(Orb_t argv[], size_t* pargc, size_t argl);
static Orb_t seq_decompose_f2(Orb_t argv[], size_t* pargc, size_t argl);

int Orb_seq_decompose(Orb_t seq, Orb_t* psingle, Orb_t* pl, Orb_t* pr) {
#define r (*pr)
#define l (*pl)
#define single (*psingle)

	int rv;
	Orb_t f0;
	Orb_BUILDER {
		Orb_B_PARENT(Orb_t_from_cfunc(&seq_decompose_f0));
		Orb_B_FIELD_cc("*rv", Orb_t_from_pointer(&rv));
	} f0 = Orb_ENDBUILDER;
	Orb_t f1;
	Orb_BUILDER {
		Orb_B_PARENT(Orb_t_from_cfunc(&seq_decompose_f1));
		Orb_B_FIELD_cc("*rv", Orb_t_from_pointer(&rv));
		Orb_B_FIELD_cc("*single", Orb_t_from_pointer(&single));
	} f1 = Orb_ENDBUILDER;
	Orb_t f2;
	Orb_BUILDER {
		Orb_B_PARENT(Orb_t_from_cfunc(&seq_decompose_f2));
		Orb_B_FIELD_cc("*rv", Orb_t_from_pointer(&rv));
		Orb_B_FIELD_cc("*l", Orb_t_from_pointer(&l));
		Orb_B_FIELD_cc("*r", Orb_t_from_pointer(&r));
	} f2 = Orb_ENDBUILDER;

	Orb_t seq_decompose = Orb_ref_cc(seq, "decompose");
	Orb_safetycheck(seq_decompose,
		Orb_SAFE(1) | Orb_SAFE(2) | Orb_SAFE(3)
	);

	Orb_call3(seq_decompose, f0, f1, f2);

	return rv;

#undef single
#undef l
#undef r
}

static Orb_t seq_decompose_f0(Orb_t argv[], size_t* pargc, size_t argl) {
	if(*pargc != 1) {
		Orb_THROW_cc("apply",
			"Incorrect number of arguments to seq-decompose-f0"
		);
	}
	Orb_t self = argv[0];
	Orb_t oprv = Orb_deref_cc(self, "*rv");
	int* prv = Orb_t_as_pointer(oprv);
	*prv = 0;

	return Orb_NIL;
}
static Orb_t seq_decompose_f1(Orb_t argv[], size_t* pargc, size_t argl) {
	if(*pargc != 2) {
		Orb_THROW_cc("apply",
			"Incorrect number of arguments to seq-decompose-f1"
		);
	}
	Orb_t self = argv[0];
	Orb_t oprv = Orb_deref_cc(self, "*rv");
	int* prv = Orb_t_as_pointer(oprv);
	*prv = 1;

	Orb_t opsingle = Orb_deref_cc(self, "*single");
	Orb_t* psingle = Orb_t_as_pointer(opsingle);
	*psingle = argv[1];

	return Orb_NIL;
}
static Orb_t seq_decompose_f2(Orb_t argv[], size_t* pargc, size_t argl) {
	if(*pargc != 3) {
		Orb_THROW_cc("apply",
			"Incorrect number of arguments to seq-decompose-f2"
		);
	}
	Orb_t self = argv[0];
	Orb_t oprv = Orb_deref_cc(self, "*rv");
	int* prv = Orb_t_as_pointer(oprv);
	*prv = 2;

	Orb_t opl = Orb_deref_cc(self, "*l");
	Orb_t* pl = Orb_t_as_pointer(opl);
	*pl = argv[1];

	Orb_t opr = Orb_deref_cc(self, "*r");
	Orb_t* pr = Orb_t_as_pointer(opr);
	*pr = argv[2];

	return Orb_NIL;
}

