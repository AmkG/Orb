
#include"liborb.h"
#include"thread-support.h"

#include<stdio.h>

static Orb_t o_current_eh;
#define the_current_eh ((Orb_tls_t) (Orb_t_as_pointer(o_current_eh)))

/*exception handler structure*/
struct Orb_priv_eh_s {
	jmp_buf jmpto;
	struct Orb_priv_eh_s* previous;
	Orb_t type;
	Orb_t value;
};

void* Orb_priv_eh_init(struct Orb_priv_eh_s** peh) {
	struct Orb_priv_eh_s* new_eh = Orb_gc_malloc(
		sizeof(struct Orb_priv_eh_s)
	);
	struct Orb_priv_eh_s* current_eh = Orb_tls_get(the_current_eh);

	new_eh->previous = current_eh;
	*peh = new_eh;

	Orb_tls_set(the_current_eh, new_eh);

	return new_eh->jmpto;
}

void Orb_priv_eh_end(struct Orb_priv_eh_s* eh) {
	struct Orb_priv_eh_s* current_eh = Orb_tls_get(the_current_eh);
	struct Orb_priv_eh_s* old_eh = current_eh->previous;
	Orb_tls_set(the_current_eh, old_eh);
}

void Orb_THROW(Orb_t type, Orb_t value) {
	struct Orb_priv_eh_s* current_eh = Orb_tls_get(the_current_eh);

	if(current_eh == 0) {
		/*no handler!*/
		fprintf(stderr, "Uncaught Orb exception, exiting application\n");
		/*TODO: print out type and value*/
		exit(1);
	}

	/*set up*/
	current_eh->type = type;
	current_eh->value = value;

	/*pop off*/
	struct Orb_priv_eh_s* old_eh = current_eh->previous;
	Orb_tls_set(the_current_eh, old_eh);

	longjmp(current_eh->jmpto, 1);
}

void Orb_THROW_cc(char const* type, char const* value) {
	Orb_THROW(
		Orb_symbol_cc(type),
		Orb_NIL /*TODO: convert value to Orb string*/
	);
}

Orb_t Orb_E_TYPE(struct Orb_priv_eh_s* E) { return E->type; }
Orb_t Orb_E_VALUE(struct Orb_priv_eh_s* E) { return E->type; }
void Orb_E_RETHROW(struct Orb_priv_eh_s* E) { Orb_THROW(E->type, E->value); }

void Orb_exception_init(void) {
	Orb_gc_defglobal(&o_current_eh);
	o_current_eh = Orb_t_from_pointer(Orb_tls_init());
}

