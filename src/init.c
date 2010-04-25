/*
Copyright 2010 Alan Manuel K. Gloria

This file is part of Orb C Implementation

Orb C Implementation is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Orb C Implementation is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Orb C Implementation.  If not, see <http://www.gnu.org/licenses/>.
*/

#include"exception.h"
#include"liborb.h"
#include"symbol.h"
#include"object.h"
#include"thread-support.h"
#include"c-functions.h"
#include"bool.h"

void Orb_post_gc_init(int argc, char* argv[]) {
	Orb_thread_support_init();
	Orb_exception_init();
	Orb_object_init_before_symbol();
	Orb_symbol_init();
	Orb_object_init_after_symbol();
	Orb_c_functions_init();
	Orb_bool_init();
	Orb_thread_pool_init();
}

