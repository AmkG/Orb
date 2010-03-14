
#include"liborb.h"
#include"symbol.h"
#include"object.h"
#include"thread-support.h"

void Orb_post_gc_init(int argc, char* argv[]) {
	Orb_thread_support_init();
	Orb_object_init_before_symbol();
	Orb_symbol_init();
	Orb_object_init_after_symbol();
}

