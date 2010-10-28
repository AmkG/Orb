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
#include"object.h"
#include"symbol.h"
#include"bs-tree.h"

#include<string.h>

#include<assert.h>

/*----------------------------------------------------------------------------
Initializers
----------------------------------------------------------------------------*/

Orb_t Orb_NIL;
Orb_t Orb_TRUE;
Orb_t Orb_NOTFOUND;
Orb_t Orb_OBJECT;
Orb_t Orb_TRAMPOLINE;

static int still_initializing;

/*these are formats for child objects derived
from object, notfound, and symbols.
See discussion in Orb_priv_ob_build() for details
on child formats.
*/
static Orb_t formats_object;
static Orb_t formats_notfound;
static Orb_t formats_symbolbase;

void Orb_object_init_before_symbol(void) {
	still_initializing = 1;
	Orb_gc_defglobal(&Orb_NIL);
	Orb_gc_defglobal(&Orb_TRUE);
	Orb_gc_defglobal(&Orb_NOTFOUND);
	Orb_gc_defglobal(&Orb_OBJECT);
	Orb_gc_defglobal(&Orb_TRAMPOLINE);

	Orb_NIL = Orb_t_from_pointer(Orb_gc_malloc(1));
	Orb_TRUE = Orb_t_from_pointer(Orb_gc_malloc(1));
	Orb_NOTFOUND = Orb_t_from_pointer(Orb_gc_malloc(1));
	Orb_OBJECT = Orb_t_from_pointer(Orb_gc_malloc(1));
	Orb_TRAMPOLINE = Orb_t_from_pointer(Orb_gc_malloc(1));

	formats_object = Orb_NOTFOUND;
	formats_notfound = Orb_NOTFOUND;
	formats_symbolbase = Orb_NOTFOUND;
}

/*actual objects to use*/
static Orb_t bnil;
static Orb_t btrue;
static Orb_t bnotfound;
static Orb_t bobject;
static Orb_t binteger;
static Orb_t bpointer;
static Orb_t b_bound_method;

Orb_t Orb_t_from_cfunc(Orb_cfunc f) {
	Orb_cfunc* pf = Orb_gc_malloc(sizeof(Orb_cfunc));
	*pf = f;
	Orb_t rv;
	Orb_BUILDER {
		Orb_B_PARENT(Orb_OBJECT);
		Orb_B_FIELD_AS_IF_VIRTUAL_cc("**cfunc**",
			Orb_t_from_pointer(pf)
		);
	} rv = Orb_ENDBUILDER;
	return rv;
}

static Orb_t falseif(Orb_t argv[], size_t* pargc, size_t argl) {
	if(*pargc != 3) Orb_THROW_cc("apply", "incorrect number of arguments to nil!if");
	argv[0] = argv[2]; *pargc = 1;
	return Orb_TRAMPOLINE;
}
static Orb_t trueif(Orb_t argv[], size_t* pargc, size_t argl) {
	if(*pargc != 3) Orb_THROW_cc("apply", "incorrect number of arguments to t!if");
	argv[0] = argv[1]; *pargc = 1;
	return Orb_TRAMPOLINE;
}

static void write_x(Orb_t argv[], char const* s) {
	Orb_t prc = argv[1];
	for(; *s; ++s) {
		Orb_call1(prc, Orb_t_from_integer(*s));
	}
}
#define MAKE_WRITE(named, string)\
	static Orb_t named(Orb_t argv[], size_t* pargc, size_t argl) {\
		if(*pargc != 4) Orb_THROW_cc("apply", "incorrect number of arguments to write");\
		write_x(argv, string);\
		return Orb_NIL;\
	}
MAKE_WRITE(write_nil, "nil") /*#f*/
MAKE_WRITE(write_true, "t") /*#t*/
MAKE_WRITE(write_notfound, "notfound") /*#x*/
#undef MAKE_WRITE

static Orb_t bound_method_invoke(Orb_t argv[], size_t* pargc, size_t argl);
static Orb_t extend(Orb_t argv[], size_t* pargc, size_t argl) {
	if(*pargc < 2) {
		Orb_THROW_cc("apply",
			"insufficient number of arguments to extend"
		);
	}
	Orb_BUILDER {
		Orb_B_PARENT(argv[1]);
		size_t i;
		for(i = 2; i < *pargc; i += 2) {
			Orb_B_FIELD(argv[i], argv[i + 1]);
		}
	} return Orb_ENDBUILDER;
}
static Orb_t extend_v(Orb_t argv[], size_t* pargc, size_t argl) {
	if(*pargc < 2) {
		Orb_THROW_cc("apply",
			"insufficient number of arguments to "
			"extend-as-if-virtual"
		);
	}
	Orb_BUILDER {
		Orb_B_PARENT(argv[1]);
		size_t i;
		for(i = 2; i < *pargc; i += 2) {
			Orb_B_FIELD_AS_IF_VIRTUAL(argv[i], argv[i + 1]);
		}
	} return Orb_ENDBUILDER;
}

void Orb_object_init_after_symbol(void) {
	Orb_t ofalseif = Orb_bless_safety(Orb_t_from_cfunc(&falseif),
		Orb_SAFE(1) | Orb_SAFE(2)
	);
	Orb_t otrueif = Orb_bless_safety(Orb_t_from_cfunc(&trueif),
		Orb_SAFE(1) | Orb_SAFE(2)
	);

	Orb_gc_defglobal(&bnil);
	Orb_gc_defglobal(&btrue);
	Orb_gc_defglobal(&bnotfound);
	Orb_gc_defglobal(&bobject);
	Orb_gc_defglobal(&bpointer);
	Orb_gc_defglobal(&binteger);
	Orb_gc_defglobal(&b_bound_method);

	Orb_BUILDER {
		Orb_B_PARENT(Orb_NOTFOUND);
		Orb_B_FIELD_AS_IF_VIRTUAL_cc("if", ofalseif);
		Orb_B_FIELD_AS_IF_VIRTUAL_cc("write",
			Orb_bless_safety(
				Orb_t_from_cfunc(&write_nil),
				Orb_SAFE(1) | Orb_SAFE(2) | Orb_SAFE(3)
			)
		);
	} bnil = Orb_ENDBUILDER;
	Orb_BUILDER {
		Orb_B_PARENT(Orb_NOTFOUND);
		Orb_B_FIELD_AS_IF_VIRTUAL_cc("if", otrueif);
		Orb_B_FIELD_AS_IF_VIRTUAL_cc("write",
			Orb_bless_safety(
				Orb_t_from_cfunc(&write_true),
				Orb_SAFE(1) | Orb_SAFE(2) | Orb_SAFE(3)
			)
		);
	} btrue = Orb_ENDBUILDER;
	Orb_BUILDER {
		Orb_B_PARENT(Orb_NOTFOUND);
		Orb_B_FIELD_AS_IF_VIRTUAL_cc("if", ofalseif);
		Orb_B_FIELD_AS_IF_VIRTUAL_cc("write",
			Orb_bless_safety(
				Orb_t_from_cfunc(&write_notfound),
				Orb_SAFE(1) | Orb_SAFE(2) | Orb_SAFE(3)
			)
		);
	} bnotfound = Orb_ENDBUILDER;
	Orb_BUILDER {
		Orb_B_PARENT(Orb_NOTFOUND);
		Orb_B_FIELD_AS_IF_VIRTUAL_cc("if", Orb_virtual(otrueif));
		/*TODO:write*/
		Orb_B_FIELD_AS_IF_VIRTUAL_cc("extend",
			Orb_method_assured(
				Orb_t_from_cfunc(extend)
			)
		);
		Orb_B_FIELD_AS_IF_VIRTUAL_cc("extend-as-if-virtual",
			Orb_method_assured(
				Orb_t_from_cfunc(extend_v)
			)
		);
	} bobject = Orb_ENDBUILDER;
	/*TODO:bpointer*/
	/*TODO:binteger*/
	Orb_BUILDER {
		Orb_B_PARENT(Orb_NOTFOUND);
		Orb_B_FIELD_AS_IF_VIRTUAL_cc("if", otrueif);
		/*TODO: write*/
		Orb_cfunc* pf = Orb_gc_malloc(sizeof(Orb_cfunc));
		*pf = &bound_method_invoke;
		Orb_B_FIELD_AS_IF_VIRTUAL_cc("**cfunc**",
			Orb_t_from_pointer(pf)
		);
		Orb_B_FIELD_AS_IF_VIRTUAL_cc("**is-bound-method**",
			Orb_TRUE
		);
	} b_bound_method = Orb_ENDBUILDER;

	still_initializing = 0;
}

/*----------------------------------------------------------------------------
Utility
----------------------------------------------------------------------------*/

static int Orb_t_is_propertyfunction(Orb_t obj) {
	return (obj & 0x03) == 3;
}

/*translate objects in the base*/
static Orb_t translate_object(Orb_t val) {
	if(Orb_t_is_integer(val)) return binteger;
	if(val == Orb_NIL) return bnil;
	if(val == Orb_TRUE) return btrue;
	if(val == Orb_NOTFOUND) return bnotfound;
	if(val == Orb_OBJECT) return bobject;
	if(val == Orb_SYMBOLBASE) return Orb_actual_symbolbase;
	if(Orb_t_is_pointer(val)) return bpointer;
	return val;
}

static size_t binsearchfields(
		Orb_t* fields, size_t size, Orb_t field, int* pfound) {
	size_t i, lo, hi;
	lo = 0; hi = size;
	while(lo != hi) {
		i = ((hi - lo) / 2) + lo; /*get average, avoiding overflow*/
		if(fields[i] == field) {
			*pfound = 1; return i;
		} else if(fields[i] < field) {
			lo = i + 1;
		} else hi = i;
	}
	*pfound = 0;
	return lo;
}

/*----------------------------------------------------------------------------
Object Construction
----------------------------------------------------------------------------*/

struct Orb_priv_ob_s {
	Orb_t* fields;
	Orb_t* values;
	size_t size;
	size_t capacity;
	Orb_t parent;
};

/*
virtual values:
	'**is-virtual**  t
	'**virtual-value** the-virtual
virtual empty:
	'**is-virtual** t
unbound method:
	'**is-unbound-method** t
	'**unbound-function** the-function
bound method:
	'**is-bound-method** t
	'**unbound-function** the-function
	'**this** the-object
*/

struct Orb_priv_ob_s* Orb_priv_ob_start(void) {
	struct Orb_priv_ob_s* rv = Orb_gc_malloc(sizeof(struct Orb_priv_ob_s));
	rv->fields = 0;
	rv->values = 0;
	rv->size = 0;
	rv->capacity = 0;
	rv->parent = Orb_NOTFOUND;
	return rv;
}

void Orb_priv_ob_parent(struct Orb_priv_ob_s* ob, Orb_t parent) {
	ob->parent = parent;
}
void Orb_priv_ob_field_as_if_virtual(
		struct Orb_priv_ob_s* ob, Orb_t field, Orb_t value) {
	int found;
	size_t i = binsearchfields(ob->fields, ob->size, field, &found);
	if(found) {
		ob->values[i] = value;
		return;
	}
	if(ob->size == ob->capacity) {
		size_t size = ob->size;
		size_t nc = ob->capacity + ob->capacity / 2;
		if(nc < 4) nc = 4;
		Orb_t* nfields = Orb_gc_malloc(nc * sizeof(Orb_t));
		Orb_t* nvalues = Orb_gc_malloc(nc * sizeof(Orb_t));
		memcpy(nfields, ob->fields, i * sizeof(Orb_t));
		memcpy(nvalues, ob->values, i * sizeof(Orb_t));
		memcpy(&nfields[i + 1], &ob->fields[i],
			(size - i) * sizeof(Orb_t)
		);
		memcpy(&nvalues[i + 1], &ob->values[i],
			(size - i) * sizeof(Orb_t)
		);
		nfields[i] = field;
		nvalues[i] = value;
		Orb_gc_free(ob->fields); ob->fields = nfields;
		Orb_gc_free(ob->values); ob->values = nvalues;
		++ob->size;
		ob->capacity = nc;
	} else {
		size_t size = ob->size;
		memmove(&ob->fields[i + 1], &ob->fields[i],
			(size - i) * sizeof(Orb_t)
		);
		memmove(&ob->values[i + 1], &ob->values[i],
			(size - i) * sizeof(Orb_t)
		);
		ob->fields[i] = field;
		ob->values[i] = value;
		++ob->size;
	}
}
void Orb_priv_ob_field(struct Orb_priv_ob_s* ob, Orb_t field, Orb_t value) {
	Orb_t parent = ob->parent;
	Orb_t f = Orb_deref(parent, field);
	if(f != Orb_NOTFOUND) {
		Orb_t vflag = Orb_deref_cc(f, "**is-virtual**");
		if(vflag != Orb_TRUE) {
			Orb_THROW_cc("extend",
				"Attempt to extend non-virtual field"
			);
		}
	}
	Orb_priv_ob_field_as_if_virtual(ob, field, value);
}

static int format_cmp(void*, void*);
Orb_t Orb_priv_ob_build(struct Orb_priv_ob_s* ob) {
	/*
	Object:
		[0] = format
		[1..N] = field values
		[N+1] = either Orb_NOTFOUND, or an Orb_bs_tree_t
			containing child formats.
	Format:
		[0] = number of fields N
		[1..N] = fields
		[N+1] = parent object
	Notes:
		1.  Each object includes a cell of child formats.
		The child formats are bs_tree of formats that
		derive from that object.
		2.  If an object is extended when it doesn't have
		child formats yet, we initially assume it to be
		"singly-extended", i.e. it will only be extended
		once.  We add a fresh child format to its tree of
		child formats, but the new object is created with
		both its new fields and the parent's fields.

	Property-functions are special objects whose fields are
	defined by a function.  This function may use any valid
	mechanism by which it provides fields and values.
	Property-function objects have the following format:
		[0] = function-to-call
		[1] = either Orb_NOTFOUND, or an Orb_bs_tree_t
			containing child formats.
	*/
	Orb_t parent;
	Orb_t* pchildformats;

	/*set if the parent is an object with the above format*/
	int parent_is_object;

top:
	parent_is_object = 0;
	pchildformats = 0;
	parent = ob->parent;

	if(Orb_t_is_integer(parent)) {
		Orb_THROW_cc("extend", "Unexpected extension of integer");
	}
	if(Orb_t_is_pointer(parent)) {
		if(parent == Orb_OBJECT) {
			pchildformats = &formats_object;
		} else if(parent == Orb_NOTFOUND) {
			pchildformats = &formats_notfound;
		} else if(parent == Orb_SYMBOLBASE) {
			pchildformats = &formats_symbolbase;
		} else if(parent == Orb_NIL || parent == Orb_TRUE) {
			Orb_THROW_cc("extend",
				"Unexpected extension of boolean value"
			);
		} else {
			Orb_THROW_cc("extend",
				"Unexpected extension of pointer"
			);
		}
	} else if(Orb_t_is_propertyfunction(parent)) {
		pchildformats = &((Orb_t*) Orb_t_as_pointer(parent))[1];
	} else {
		Orb_t* parent_a = Orb_t_as_pointer(parent);
		Orb_t* parent_format = Orb_t_as_pointer(parent_a[0]);
		size_t N = Orb_t_as_integer(parent_format[0]);
		pchildformats = &parent_a[N+1];
		parent_is_object = 1;
	}

	if(*pchildformats == Orb_NOTFOUND) {
		/*create a new childformats tree*/
		*pchildformats = Orb_t_from_pointer(
			Orb_bs_tree_init(&format_cmp)
		);
		/*potential race condition.  Shouldn't
		matter as long as we are able to use
		*some* tree as the the child formats
		tree
		*/

		if(parent_is_object) {
			/*join the current parent to this
			object.
			*/
			Orb_t* parent_a = Orb_t_as_pointer(parent);
			Orb_t* parent_format = Orb_t_as_pointer(parent_a[0]);
			size_t N = Orb_t_as_integer(parent_format[0]);
			Orb_t parent_parent = parent_format[N+1];

			struct Orb_priv_ob_s* nob = Orb_priv_ob_start();
			/*get our direct parent's parent as our parent*/
			Orb_priv_ob_parent(nob, parent_parent);
			size_t i;
			for(i = 0; i < N; ++i) {
				/*skip virtuals checking*/
				Orb_priv_ob_field_as_if_virtual(
					nob,
					parent_format[1+i],
					parent_a[1+i]
				);
			}
			for(i = 0; i < ob->size; ++i) {
				Orb_priv_ob_field_as_if_virtual(
					nob,
					ob->fields[i],
					ob->values[i]
				);
			}
			Orb_t* fields = ob->fields;
			Orb_t* values = ob->values;
			Orb_gc_free(ob);
			Orb_gc_free(fields);
			Orb_gc_free(values);
			ob = nob;
			goto top;
		}
	}

	{ Orb_bs_tree_t childformats = Orb_t_as_pointer(*pchildformats);
		/*construct the format*/
		size_t N = ob->size;
		Orb_t* nformat = Orb_gc_malloc( sizeof(Orb_t) *
			(N + 2)
		);
		nformat[0] = Orb_t_from_integer(N);
		nformat[N+1] = ob->parent;
		memcpy(&nformat[1], ob->fields, sizeof(Orb_t) * N);

		/*add to child formats*/
		Orb_t* existing = Orb_bs_tree_insert(childformats, nformat);
		if(existing != nformat) { Orb_gc_free(nformat); nformat = 0; }

		/*now construct the object itself*/
		Orb_t* a = Orb_gc_malloc( sizeof(Orb_t) *
			(N + 2)
		);
		a[0] = Orb_t_from_pointer(existing);
		a[N+1] = Orb_NOTFOUND;
		memcpy(&a[1], ob->values, sizeof(Orb_t) * N);

		/*free*/
		Orb_t* fields = ob->fields;
		Orb_t* values = ob->values;
		Orb_gc_free(ob);
		Orb_gc_free(fields);
		Orb_gc_free(values);

		return ((Orb_t) a) + 0x01;
	}
}

static int format_cmp(void* vpa, void* vpb) {
	Orb_t* a = vpa; Orb_t* b = vpb;
	/*first compare number of fields*/
	if(a[0] < b[0]) return -1;
	if(a[0] > b[0]) return 1;
	size_t N = Orb_t_as_integer(a[0]);
	/*now compare parents*/
	if(a[N+1] < b[N+1]) return -1;
	if(a[N+1] > b[N+1]) return 1;
	/*compare fields*/
	size_t i;
	for(i = 1; i <= N; ++i) {
		if(a[i] < b[i]) return -1;
		if(a[i] > b[i]) return 1;
	}
	return 0;
}

/*----------------------------------------------------------------------------
Object referencing
----------------------------------------------------------------------------*/

Orb_t Orb_deref(Orb_t obj, Orb_t field) {
	Orb_t tmp;
	Orb_t rv = Orb_deref_nopropobj(obj, field, &tmp);
	if(field == Orb_NOTFOUND) {
		return rv;
	} else {
		return Orb_call2(
			tmp,
			Orb_t_from_integer(1),
			field
		);
	}
}

Orb_t Orb_deref_nopropobj(Orb_t obj, Orb_t field, Orb_t* p) {
	Orb_t* a; Orb_t* format;
	size_t numfields;
	Orb_t parent;
	int found; size_t index;

	*p = Orb_NOTFOUND;

top:
	obj = translate_object(obj);
	if(Orb_t_is_propertyfunction(obj)) {
		/*property-function*/
		Orb_t* innerfunc = Orb_t_as_pointer(obj);
		*p = innerfunc[0];
		return Orb_NOTFOUND;
	}
	a = Orb_t_as_pointer(obj);
	format = Orb_t_as_pointer(a[0]);
	numfields = Orb_t_as_integer(format[0]);
	index = binsearchfields(&format[1], numfields, field, &found);
	if(found) {
		return a[1+index];
	}
	parent = format[numfields+1];
	if(parent == Orb_NOTFOUND) return Orb_NOTFOUND;
	obj = parent; goto top;
}

Orb_t Orb_method(Orb_t orig) {
	/*first check for the case where:
		(def base (obj 'foo (method:fn (self) (something self))))
		(def derive (obj 'foo (method base!foo)))
	*/
	Orb_t check = Orb_deref_cc(orig, "**is-bound-method**");
	if(check == Orb_TRUE) {
		orig = Orb_deref_cc(orig, "**unbound-function**");
	}
	Orb_BUILDER {
		Orb_B_PARENT(Orb_NOTFOUND);
		Orb_B_FIELD_AS_IF_VIRTUAL_cc("**is-unbound-method**",
			Orb_TRUE
		);
		Orb_B_FIELD_AS_IF_VIRTUAL_cc("**unbound-function**",
			orig
		);
		Orb_t osafety = Orb_deref_cc(orig, "**orbsafety**");
		if(Orb_t_is_integer(osafety)) {
			size_t safety = Orb_t_as_integer(osafety);
			Orb_B_FIELD_AS_IF_VIRTUAL_cc("**orbsafety**",
				Orb_t_from_integer(safety >> 1)
			);
		}
	} return Orb_ENDBUILDER;
}
Orb_t Orb_method_assured(Orb_t orig) {
	Orb_BUILDER {
		Orb_B_PARENT(Orb_NOTFOUND);
		Orb_B_FIELD_AS_IF_VIRTUAL_cc("**is-unbound-method**",
			Orb_TRUE
		);
		Orb_B_FIELD_AS_IF_VIRTUAL_cc("**unbound-function**",
			orig
		);
	} return Orb_ENDBUILDER;
}

Orb_t Orb_virtual(Orb_t orig) {
	Orb_BUILDER {
		Orb_B_PARENT(Orb_NOTFOUND);
		Orb_B_FIELD_AS_IF_VIRTUAL_cc("**is-virtual**",
			Orb_TRUE
		);
		Orb_B_FIELD_AS_IF_VIRTUAL_cc("**virtual-value**",
			orig
		);
	} return Orb_ENDBUILDER;
}

Orb_t Orb_virtual_x(void) {
	Orb_BUILDER {
		Orb_B_PARENT(Orb_NOTFOUND);
		Orb_B_FIELD_AS_IF_VIRTUAL_cc("**is-virtual**",
			Orb_TRUE
		);
	} return Orb_ENDBUILDER;
}

Orb_t Orb_ref(Orb_t obj, Orb_t field) {
	Orb_t val = Orb_deref(obj, field);
	if(val == Orb_NOTFOUND) return val;
	Orb_t check;

	/*check for virtuality*/
	check = Orb_deref_cc(val, "**is-virtual**");
	if(check == Orb_TRUE) {
		check = Orb_deref_cc(val, "**virtual-value**");
		if(check == Orb_NOTFOUND) {
			Orb_THROW_cc("ref",
				"Attempt to reference uninitialized "
				"virtual value"
			);
		}
		val = check;
	}
	/*check for methodality*/
	check = Orb_deref_cc(val, "**is-unbound-method**");
	if(check == Orb_TRUE) {
		check = Orb_deref_cc(val, "**unbound-function**");
		/*construct the bound method*/
		Orb_BUILDER {
			Orb_B_PARENT(b_bound_method);
			Orb_B_FIELD_AS_IF_VIRTUAL_cc("**this**",
				obj
			);
			Orb_B_FIELD_AS_IF_VIRTUAL_cc("**unbound-function**",
				check
			);
			check = Orb_deref_cc(val, "**orbsafety**");
			if(Orb_t_is_integer(check)) {
				Orb_B_FIELD_AS_IF_VIRTUAL_cc("**orbsafety**",
					check
				);
			}
		} return Orb_ENDBUILDER;
	}
	return val;
}

static Orb_t bound_method_invoke(Orb_t argv[], size_t* pargc, size_t argl) {
	/*extract objects*/
	Orb_t self = argv[0];
	Orb_t this = Orb_deref_cc(self, "**this**");
	Orb_t f = Orb_deref_cc(self, "**unbound-function**");

	/*check for sufficient space to insert this into arglist*/
	if(*pargc < argl) {
		memmove(&argv[2], &argv[1], sizeof(Orb_t) * ((*pargc) - 1));
		argv[1] = this;
		argv[0] = f;
		++(*pargc);
		return Orb_TRAMPOLINE;
	} else {
		Orb_t* nargv = Orb_gc_malloc(sizeof(Orb_t) * ((*pargc) + 1));
		memcpy(&nargv[2], &argv[1], sizeof(Orb_t) * ((*pargc) - 1));
		nargv[1] = this;
		nargv[0] = f;
		return Orb_call(nargv, (*pargc) + 1);
	}
}

/*----------------------------------------------------------------------------
Object moving (used by kfunc's for evacuation).
----------------------------------------------------------------------------*/

/*
Evacuates an object, creating a precise clone of that object
elsewhere.
Return 1 if the object was moved this time, or 0 if not moved.
*/
int Orb_evacuate_object(Orb_t* ptarget, Orb_t v) {
	assert(Orb_t_is_object(v));

	/*get the memory area*/
	Orb_t* arr = Orb_t_as_pointer(v);
	/*forwarding pointer already?*/
	if(Orb_t_is_object(arr[0])) {
		*ptarget = arr[0];
		return 0;
	}
	/*have to alloc*/

	/*count the size*/
	Orb_t oformat = arr[0];
	Orb_t* farr = Orb_t_as_pointer(oformat);
	Orb_t osize = farr[0];
	size_t size = Orb_t_as_integer(osize);

	/*alloc*/
	Orb_t* rvarr = Orb_gc_malloc((size + 2) * sizeof(Orb_t));
	/*clone*/
	memcpy(rvarr, arr, (size + 1) * sizeof(Orb_t));
	/*clear child formats field*/
	rvarr[size + 1] = Orb_NOTFOUND;

	*ptarget = ((Orb_t) rvarr) + 0x01;
	return 1;
}

/* Given an object, iterate over each direct field of
the object and replace each with the result of the 
trans function, given the trans_clos parameter.
*/
void Orb_evacuate_fields(Orb_t v,
		Orb_t (*trans)(Orb_t, Orb_t, void*), void* trans_clos) {
	assert(Orb_t_is_object(v));

	/*get the memory area*/
	Orb_t* arr = Orb_t_as_pointer(v);
	/*count the size*/
	Orb_t oformat = arr[0];
	Orb_t* farr = Orb_t_as_pointer(oformat);
	Orb_t osize = farr[0];
	size_t size = Orb_t_as_integer(osize);

	/*traverse the objects*/
	size_t i;
	for(i = 0; i < size; ++i) {
		Orb_t value = arr[i + 1];
		Orb_t field = farr[i + 1];
		arr[i + 1] = trans(field, value, trans_clos);
	}
}




