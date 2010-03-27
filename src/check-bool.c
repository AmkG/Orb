#include"liborb.h"

#include<assert.h>

Orb_t dummy(Orb_t argv[], size_t* pargc, size_t argl) {
	return Orb_NOTFOUND;
}

int main(void) {
	Orb_init(0,0);

	assert(Orb_bool(Orb_TRUE));
	assert(!Orb_bool(Orb_NIL));
	assert(!Orb_bool(Orb_NOTFOUND));
	assert(Orb_bool(Orb_t_from_cfunc(&dummy)));

	return 0;
}

