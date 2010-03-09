
#include"liborb.h"
#include"bs-tree.h"
#include"symbol.h"

#include<string.h>

/*string-to-symbol binary tree*/
static Orb_t str_to_sym;

static Orb_t str_field;
static Orb_t len_field;

/*fake symbol base*/
Orb_t Orb_SYMBOLBASE;
/*When Orb_deref() determines the object to be Orb_SYMBOLBASE, it
replaces the object with Orb_actual_symbolbase.  This is necessary
to properly initialize.
*/
Orb_t Orb_actual_symbolbase;

static Orb_t new_symbol(char const* string, size_t len) {
	Orb_BUILDER {
		Orb_B_PARENT(Orb_SYMBOLBASE);
		Orb_B_FIELD(str_field, Orb_t_from_pointer((void*)string));
		Orb_B_FIELD(len_field, Orb_t_from_integer(len));
	} return Orb_ENDBUILDER;
}

struct pair_s {
	char const* string;
	size_t len;
	Orb_t symbol;
};
typedef struct pair_s* pair_t;

static int str_compare(void* vpa, void* vpb) {
	pair_t a = vpa;
	pair_t b = vpb;
	if(a->len < b->len) {
		return -1;
	} else if(a->len > b->len) {
		return 1;
	}
	size_t i;
	for(i = 0; i < a->len; ++i) {
		if(a->string[i] < b->string[i]) {
			return -1;
		} else if(a->string[i] > b->string[i]) {
			return 1;
		}
	}
	return 0;
}
static int sym_compare(void* vpa, void* vpb) {
	pair_t a = vpa;
	pair_t b = vpb;
	if(a->symbol < b->symbol) return -1;
	else if(a->symbol > b->symbol) return 1;
	else return 0;
}

void Orb_symbol_init(void) {
	Orb_gc_defglobal(&str_to_sym);
	Orb_gc_defglobal(&str_field);
	Orb_gc_defglobal(&len_field);
	Orb_gc_defglobal(&Orb_SYMBOLBASE);
	Orb_gc_defglobal(&Orb_actual_symbolbase);

	Orb_SYMBOLBASE = Orb_t_from_pointer(Orb_gc_malloc(1));
	str_field = Orb_t_from_pointer(Orb_gc_malloc(1));
	len_field = Orb_t_from_pointer(Orb_gc_malloc(1));

	/*intialize map*/
	str_to_sym = Orb_t_from_pointer(Orb_bs_tree_init(&str_compare));

	Orb_BUILDER {
		Orb_B_PARENT(Orb_NOTFOUND);
		/*TODO: fill in fields*/
	} Orb_actual_symbolbase = Orb_ENDBUILDER;
}

Orb_t Orb_symbol_sz(char const* string, size_t len) {
	struct pair_s tmp;
	tmp.string = string;
	tmp.len = len;

	void* vp = Orb_bs_tree_lookup(Orb_t_as_pointer(str_to_sym), &tmp);
	if(vp) {
		pair_t rv = vp;
		return rv->symbol;
	} else {
		pair_t rv = Orb_gc_malloc(sizeof(struct pair_s));
		char* ns = Orb_gc_malloc_pointerfree(len + 1);
		memcpy(ns, string, len);
		ns[len] = 0;
		rv->string = ns;
		rv->len = len;
		rv->symbol = new_symbol(ns, len);

		vp = Orb_bs_tree_insert(Orb_t_as_pointer(str_to_sym), rv);
		if(((pair_t) vp) != rv) {
			/*someone else already inserted!*/
			Orb_gc_free(rv);
			Orb_gc_free(ns);
			ns = 0;
			rv = vp;
		}

		return rv->symbol;
	}
}


