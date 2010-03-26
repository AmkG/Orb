
#include"exception.h"
#include"liborb.h"
#include"symbol.h"
#include"object.h"
#include"thread-support.h"
#include"c-functions.h"

void Orb_post_gc_init(int argc, char* argv[]) {
	Orb_thread_support_init();
	Orb_exception_init();
	Orb_object_init_before_symbol();
	Orb_symbol_init();
	Orb_object_init_after_symbol();
	Orb_c_functions_init();
}

