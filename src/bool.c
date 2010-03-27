
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

