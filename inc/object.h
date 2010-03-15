#ifndef OBJECT_H
#define OBJECT_H

#include"liborb.h"

/*The function below is Orb_method(), but does not
check if the given function is an already-bound
method.  It is intended for use during
initialization, when it is not safe to
dereference objects just yet.

This function requires that:
	1. the input is not an already-bound method
	2. the input has no orb-safety requirements
	(i.e. add orb-safety afterwards, not before!)
*/
Orb_t Orb_method_assured(Orb_t);

void Orb_object_init_before_symbol(void);
void Orb_object_init_after_symbol(void);

#endif /* OBJECT_H */

