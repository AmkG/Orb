#ifndef LIBORB_H
#define LIBORB_H

#if defined(_MSC_VER) && (_MSC_VER>=1200)
#pragma once
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include<stdint.h>
#include<stdlib.h>

typedef intptr_t Orb_t;

static inline int Orb_t_is_integer(Orb_t x) {
	return ((x) & 0x03) == 0;
}
static inline int Orb_t_as_integer(Orb_t x) {
	return x >> 2;
}
static inline Orb_t Orb_t_from_integer(int x) {
	return x << 2;
}

static inline int Orb_t_is_pointer(Orb_t x) {
	return ((x) & 0x03) == 2;
}
static inline void* Orb_t_as_pointer(Orb_t x) {
	return (void*) (x & ~((Orb_t) 0x03));
}
static inline Orb_t Orb_t_from_pointer(void* x) {
	return ((Orb_t) (x)) + 2;
}

static inline int Orb_t_is_object(Orb_t x) {
	return ((x) & 0x03) == 1;
}

Orb_t Orb_symbol_sz(char const* str, size_t sz);
Orb_t Orb_symbol(char const* str);
Orb_t Orb_symbol_cc(char const* str);

Orb_t Orb_ref(Orb_t, Orb_t);
static inline Orb_t Orb_ref_cc(Orb_t v, char const* str) {
	return Orb_ref(v, Orb_symbol_cc(str));
}
Orb_t Orb_deref(Orb_t, Orb_t);
static inline Orb_t Orb_deref_cc(Orb_t v, char const* str) {
	return Orb_rdeef(v, Orb_symbol_cc(str));
}

extern Orb_t Orb_NIL;
extern Orb_t Orb_TRUE;
extern Orb_t Orb_NOTFOUND;
extern Orb_t Orb_OBJECT;

Orb_t Orb_call(Orb_t argv[], size_t argc);

static inline Orb_t Orb_call0(Orb_t f) {
	return Orb_call(&f, 1);
}
static inline Orb_t Orb_call1(Orb_t f, Orb_t a) {
	Orb_t argv[2]; argv[0] = f; argv[1] = a;
	return Orb_call(argv, 2);
}
Orb_t Orb_call2(Orb_t f, Orb_t a, Orb_t b) {
	Orb_t argv[3]; argv[0] = f; argv[1] = a; argv[2] = b;
	return Orb_call(argv, 3);
}
Orb_t Orb_call3(Orb_t f, Orb_t a, Orb_t b, Orb_t c) {
	Orb_t argv[4]; argv[0] = f; argv[1] = a; argv[2] = b; argv[3] = c;
	return Orb_call(argv, 4);
}

Orb_t Orb_t_from_cf0(Orb_t (*cf)(void));
Orb_t Orb_t_from_cf1(Orb_t (*cf)(Orb_t));
Orb_t Orb_t_from_cf2(Orb_t (*cf)(Orb_t, Orb_t));
Orb_t Orb_t_from_cf3(Orb_t (*cf)(Orb_t, Orb_t, Orb_t));

void* Orb_gc_malloc(size_t);
void* Orb_gc_malloc_pointerfree(size_t);
void Orb_gc_free(void*); /*explicit free does not check accessibility!*/
void Orb_gc_defglobals(Orb_t*, size_t);
static inline void Orb_gc_defglobal(Orb_t* p) {
	Orb_gc_defglobals(p, 1);
}
void Orb_gc_undefglobals(Orb_t*, size_t);
static inline void Orb_gc_undefglobal(Orb_t* p) {
	Orb_gc_undefglobals(p, 1);
}

void Orb_post_gc_init(int argc, char* argv[]);
void Orb_gc_only_init(void);
static inline void Orb_init(int argc, char* argv[]) {
	Orb_gc_only_init();
	Orb_post_gc_init(argc, argv);
}

/*starts a new thread, invoking the given function*/
Orb_t Orb_new_thread(Orb_t);

Orb_t Orb_new_atom(Orb_t);
Orb_t Orb_new_sema(size_t);

/*releases the C Extension Lock*/
void Orb_CEL_unlock(void);
/*re-acquires the C Extension Lock*/
void Orb_CEL_lock(void);
/*determines if we have the C Extension Lock*/
int Orb_CEL_havelock(void);

/*exception handling*/
/*
Orb_TRY(E) {
	Orb_t foo = Orb_call1(f, a);
	if(foo == Orb_NIL) {
		Orb_THROW_cc("foo", "something");
	}
	return foo;
} Orb_CATCH(E) {
	Orb_t type = Orb_E_TYPE(E);
	Orb_t val = Orb_E_VALUE(E);
	if(type != Orb_symbol_cc("foo")) {
		Orb_E_RETHROW(E);
	} else {
		return val;
	}
} Orb_ENDTRY;
*/
#include<setjmp.h>

struct Orb_priv_eh_s;
#define Orb_TRY(E)\
	do { struct Orb_priv_eh_s* E;\
		if(!setjmp(Orb_priv_eh_init(&E)))
#define Orb_CATCH(E)\
	else
#define Orb_ENDTRY\
	} while(0);

void* Orb_priv_eh_init(struct Orb_priv_eh_s**);
void Orb_THROW(Orb_t, Orb_t);
void Orb_THROW_cc(char const*, char const*);

Orb_t Orb_E_TYPE(struct Orb_priv_eh_s*);
Orb_t Orb_E_VALUE(struct Orb_priv_eh_s*);
Orb_t Orb_E_RETHROW(struct Orb_priv_eh_s*);

#ifdef __cplusplus
}
#endif

#endif /* LIBORB_H */
