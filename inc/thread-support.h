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
#ifndef THREAD_SUPPORT_H
#define THREAD_SUPPORT_H

/*thread-locals*/
struct Orb_tls_s;
typedef struct Orb_tls_s* Orb_tls_t;

Orb_tls_t Orb_tls_init(void);
void* Orb_tls_get(Orb_tls_t);
void Orb_tls_set(Orb_tls_t, void*);

/*semaphores*/
struct Orb_sema_s;
typedef struct Orb_sema_s* Orb_sema_t;

Orb_sema_t Orb_sema_init(unsigned int);
unsigned int Orb_sema_get(Orb_sema_t);
void Orb_sema_wait(Orb_sema_t);
void Orb_sema_post(Orb_sema_t);

#include"liborb.h"

/*cells*/
struct Orb_cell_s;
typedef struct Orb_cell_s* Orb_cell_t;

Orb_cell_t Orb_cell_init(Orb_t);
Orb_t Orb_cell_get(Orb_cell_t);
void Orb_cell_set(Orb_cell_t, Orb_t);
/*compare and swap, returning current value of cell*/
Orb_t Orb_cell_cas_get(Orb_cell_t, Orb_t, Orb_t);
/*compare and swap, returning 0 if cas failed and non-0 if
cas succeeded.
*/
static inline int Orb_cell_cas(Orb_cell_t c, Orb_t o, Orb_t n) {
	return o == Orb_cell_cas_get(c, o, n);
}

/*new threads*/
struct Orb_thread_s;
typedef struct Orb_thread_s* Orb_thread_t;
Orb_thread_t Orb_priv_new_thread(Orb_t);

/*general*/
void Orb_thread_support_init(void);
size_t Orb_num_processors(void);

#endif /* THREAD_SUPPORT_H */

