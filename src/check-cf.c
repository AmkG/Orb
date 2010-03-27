#include"liborb.h"

#include<assert.h>

int main(void) {
	Orb_init(0, 0);

	Orb_t test = Orb_symbol("test-symbol");

	/*check that calling the C extension is roughly equivalent
	to calling the C function directly
	*/
	Orb_t oderef = Orb_t_from_cf2(&Orb_deref); /*deref doesn't perform method binding*/
	Orb_t test1_res1 = Orb_deref_cc(test, "write");
	Orb_t test1_res2 = Orb_call2(oderef, test, Orb_symbol_cc("write"));
	assert(test1_res1 == test1_res2);

	/*Check that CELfree C extensions work too
	*/
	oderef = Orb_CELfree(oderef);
	Orb_t test2_res1 = Orb_deref_cc(test, "write");
	Orb_t test2_res2 = Orb_call2(oderef, test, Orb_symbol_cc("write"));
	assert(test2_res1 == test2_res2);

	return 0;
}
