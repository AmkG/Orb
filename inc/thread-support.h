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

