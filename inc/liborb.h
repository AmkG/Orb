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
void Orb_gc_free(void*); /*explicit free does not check accessibility!*/
void Orb_gc_defglobals(Orb_t*, size_t);
static inline void Orb_gc_defglobal(Orb_t* p) {
	Orb_gc_defglobals(p, 1);
}
void Orb_gc_undefglobals(Orb_t*, size_t);
static inline void Orb_gc_defglobal(Orb_t* p) {
	Orb_gc_undefglobals(p, 1);
}

void Orb_post_gc_init(int argc, char* argv[]);
void Orb_gc_only_init(void);
static inline void Orb_init(int argc, char* argv[]) {
	Orb_gc_only_init();
	Orb_post_gc_init(argc, argv);
}

#ifdef __cplusplus
}
#endif

#endif /* LIBORB_H */
