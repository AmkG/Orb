#ifndef THREAD_SUPPORT_H
#define THREAD_SUPPORT_H

/*thread-locals*/
struct Orb_tls_s;
typedef struct Orb_tls_s* Orb_tls_t;

Orb_tls_t Orb_tls_init(void);
void* Orb_tls_get(Orb_tls_t);
void Orb_tls_set(Orb_tls_t, void*);
void Orb_tls_deinit(Orb_tls_t);

/*semaphores*/
struct Orb_sema_s;
typedef struct Orb_sema_s* Orb_sema_t;

Orb_sema_t Orb_sema_init(unsigned int);
unsigned int Orb_sema_get(Orb_sema_t);
void Orb_sema_wait(Orb_sema_t);
void Orb_sema_post(Orb_sema_t);
void Orb_sema_deinit(Orb_sema_t);

#include"liborb.h"

/*cells*/
struct Orb_cell_s;
typedef struct Orb_cell_s* Orb_cell_t;

Orb_cell_t Orb_cell_init(Orb_t);
Orb_t Orb_cell_get(Orb_cell_t);
void Orb_cell_set(Orb_cell_t, Orb_t);
int Orb_cell_cas(Orb_cell_t, Orb_t, Orb_t);
void Orb_cell_deinit(Orb_cell_t);

#endif /* THREAD_SUPPORT_H */

