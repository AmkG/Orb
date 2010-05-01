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

Orb_t seq1(Orb_t i) {
	return Orb_seq(&i, 1);
}

int main(void) {
	Orb_init(0, 0);

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

	return 0;
}

