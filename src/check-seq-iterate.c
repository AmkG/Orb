#include<liborb.h>

#include<assert.h>

/*
 * All tested sequences must be (0 1 2 3 4 5 6 7)
 */

int each_test(Orb_t s) {

	size_t i = 0;
	Orb_EACH(oi, s) {
		if(oi != Orb_t_from_integer(i)) return 0;
		++i;
	} Orb_ENDEACH;

	return 1;
}

Orb_t mysecret;
Orb_t o_iterate_test_cfunc;

Orb_t iterate_test_cfunc(Orb_t argv[], size_t* pargc, size_t argl) {
	if(*pargc != 4) {
		Orb_THROW_cc("apply",
			"Incorrect number of arguments to function given to iterate"
		);
	}
	Orb_t cur = argv[1];
	Orb_t next = argv[2];
	Orb_t at_end = argv[3];

	size_t i = 0;
	while(!Orb_bool(Orb_call0(at_end))) {
		Orb_t oi = Orb_call0(cur);
		if(oi != Orb_t_from_integer(i)) return Orb_NIL;

		Orb_call1(next, Orb_t_from_integer(1));
		++i;
	}
	if(i != 8) return Orb_NIL;

	return mysecret;
}

int iterate_test(Orb_t s) {
	Orb_t iterate = Orb_ref_cc(s, "iterate");
	Orb_safetycheck(iterate, Orb_SAFE(1));
	Orb_t rv = Orb_call1(iterate, o_iterate_test_cfunc);
	if(rv != mysecret) return 0;
	else return 1;
}

Orb_t seq1(Orb_t i) {
	return Orb_seq(&i, 1);
}

int main(void) {
	Orb_init(0, 0);

	Orb_gc_defglobal(&mysecret);
	Orb_gc_defglobal(&o_iterate_test_cfunc);

	mysecret = Orb_t_from_pointer(&mysecret);
	o_iterate_test_cfunc = Orb_bless_safety(
		Orb_t_from_cfunc(&iterate_test_cfunc),
		Orb_SAFE(1) | Orb_SAFE(2) | Orb_SAFE(3)
	);

	Orb_t tree =
		Orb_conc(
			Orb_conc(
				Orb_conc(
					seq1(Orb_t_from_integer(0)),
					seq1(Orb_t_from_integer(1))
				),
				Orb_conc(
					seq1(Orb_t_from_integer(2)),
					seq1(Orb_t_from_integer(3))
				)
			) ,
			Orb_conc(
				Orb_conc(
					seq1(Orb_t_from_integer(4)),
					seq1(Orb_t_from_integer(5))
				),
				Orb_conc(
					seq1(Orb_t_from_integer(6)),
					seq1(Orb_t_from_integer(7))
				)
			)
		)
	;
	assert(each_test(tree));
	assert(iterate_test(tree));

	Orb_t arr;
	{ Orb_t a[8]; size_t i;
		for(i = 0; i < 8; ++i) {
			a[i] = i;
		}
		arr = Orb_seq(a, 8);
	}
	assert(each_test(arr));
	assert(iterate_test(arr));

	return 0;
}

