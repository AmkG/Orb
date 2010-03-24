#ifndef LIBORB_H
#define LIBORB_H

#if defined(_MSC_VER) && (_MSC_VER>=1200)
#pragma once
#endif

#include<liborb-config.h>

#ifdef __cplusplus
extern "C" {
#endif

#include<stdint.h>
#include<stdlib.h>

typedef intptr_t Orb_t;

/*
Orb_t is a tagged pointer
tags:
	00 = integer
	01 = standard object (see object.c for details)
	10 = generic void* (object to point to must be word-aligned)
	11 = property-object (same as standard object, but
			treated as a lookup function in deref
			and fields)
*/

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
	return Orb_deref(v, Orb_symbol_cc(str));
}

extern Orb_t Orb_NIL;
extern Orb_t Orb_TRUE;
extern Orb_t Orb_NOTFOUND;
extern Orb_t Orb_OBJECT;

Orb_t Orb_call_ex(Orb_t argv[], size_t argc, size_t argl);
static inline Orb_t Orb_call(Orb_t argv[], size_t argc) {
	return Orb_call_ex(argv, argc, argc);
}

#define Orb_SAFE(x) (((size_t) (1)) << ((x) - 1))
void Orb_safetycheck(Orb_t, size_t);
Orb_t Orb_bless_safety(Orb_t, size_t);

static inline Orb_t Orb_call0(Orb_t f) {
	return Orb_call(&f, 1);
}
static inline Orb_t Orb_call1(Orb_t f, Orb_t a) {
	Orb_t argv[2]; argv[0] = f; argv[1] = a;
	return Orb_call(argv, 2);
}
static inline Orb_t Orb_call2(Orb_t f, Orb_t a, Orb_t b) {
	Orb_t argv[3]; argv[0] = f; argv[1] = a; argv[2] = b;
	return Orb_call(argv, 3);
}
static inline Orb_t Orb_call3(Orb_t f, Orb_t a, Orb_t b, Orb_t c) {
	Orb_t argv[4]; argv[0] = f; argv[1] = a; argv[2] = b; argv[3] = c;
	return Orb_call(argv, 4);
}

/*Create function objects from C functions.
The created function objects will acquire the
C Extension Lock (CEL) when called.
To disable acquisition of the CEL, use
Orb_CELfree() below.
*/
Orb_t Orb_t_from_cf0(Orb_t (*cf)(void));
Orb_t Orb_t_from_cf1(Orb_t (*cf)(Orb_t));
Orb_t Orb_t_from_cf2(Orb_t (*cf)(Orb_t, Orb_t));
Orb_t Orb_t_from_cf3(Orb_t (*cf)(Orb_t, Orb_t, Orb_t));
Orb_t Orb_t_from_cfv(Orb_t (*cf)(Orb_t*, size_t));

/*Pass the result of one of the above functions to return
an function object that does not acquire the CEL.
*/
Orb_t Orb_CELfree(Orb_t);

/*low-level cfunc*/
typedef Orb_t Orb_cfunc_f(Orb_t argv[], size_t* pargc, size_t argl);
typedef Orb_cfunc_f* Orb_cfunc;

Orb_t Orb_t_from_cfunc(Orb_cfunc);

/*value that may be returned by a cfunc
to specify a tail call
*/
extern Orb_t Orb_TRAMPOLINE;

/*garbage collector*/

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
/*when the GC determines that the given area is inaccessible,
call the finalizer.
Finalizers are not necessarily called if cycles more
complicated than pointers-to-self are found.
This is because the GC calls finalizers in a particular
order.
See Orb_gc_autoclear_on_finalize() for workarounds for
cycles.
client_data can be a Orb_gc_malloc()'ed area, and it
will be retained until the finalizer is called.
*/
void Orb_gc_deffinalizer(
	void* area,
	void (*finalizer)(void* area, void* client_data),
	void* client_data
);
void Orb_gc_undeffinalizer(
	void* area
);
/*when the GC determines that the given area is inaccessible
from roots, clear the given area_pointer.
The area_pointer may or may not be part of the object, and
it may or may not "normally" point to the object itself.
This is needed to handle finalization order of cycles.  If
an object can possibly cause cycles via a particular pointer,
but must be finalized even so (and the pointer is not
absolutely needed for finalization anyway) then you can
register that particular pointer via this function.  When
the object is about to be finalized, the pointer is cleared
and the cycle, if any, gets broken, allowing the GC to
determine an order for finalization.
This can also be used to implement immutable weak pointers;
the weak pointer is allocated in pointerfree space, then
registered as the area_pointer here.
*/
void Orb_gc_autoclear_on_finalize(
	void* area,
	void** area_pointer
);

/*trigger a GC*/
void Orb_gc_trigger(void);

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
#define Orb_TRY\
	do { struct Orb_priv_eh_s* Orb_priv_eh_dat;\
		if(!setjmp(Orb_priv_eh_init(&Orb_priv_eh_dat))) {\
			do
#define Orb_CATCH(E)\
			while(0);\
			Orb_priv_eh_end(Orb_priv_eh_dat)\
		} else {\
			struct Orb_priv_eh_s* E = Orb_priv_eh_dat;\
			do
#define Orb_ENDTRY\
		while(0); }\
	} while(0);

void* Orb_priv_eh_init(struct Orb_priv_eh_s**);
void Orb_priv_eh_end(struct Orb_priv_eh_s*);
void Orb_THROW(Orb_t, Orb_t);
void Orb_THROW_cc(char const*, char const*);

Orb_t Orb_E_TYPE(struct Orb_priv_eh_s*);
Orb_t Orb_E_VALUE(struct Orb_priv_eh_s*);
void Orb_E_RETHROW(struct Orb_priv_eh_s*);

/*object construction*/
/*
Orb_t rv;
Orb_BUILDER {
	Orb_B_PARENT(Orb_NOTFOUND);
	Orb_B_FIELD_cc("**val**", val);
} rv = Orb_ENDBUILDER;
*/
struct Orb_priv_ob_s;
#define Orb_BUILDER\
	do { struct Orb_priv_ob_s* Orb_priv_ob = Orb_priv_ob_start();
#define Orb_B_PARENT(v)\
		Orb_priv_ob_parent(Orb_priv_ob, v)
#define Orb_B_FIELD_AS_IF_VIRTUAL(f, v)\
		Orb_priv_ob_field_as_if_virtual(Orb_priv_ob, f, v)
#define Orb_B_FIELD(f, v)\
		Orb_priv_ob_field(Orb_priv_ob, f, v)
#define Orb_B_FIELD_AS_IF_VIRTUAL_cc(f, v)\
		Orb_priv_ob_field_as_if_virtual(Orb_priv_ob, Orb_symbol_cc(f), v)
#define Orb_B_FIELD_cc(f, v)\
		Orb_priv_ob_field(Orb_priv_ob, Orb_symbol_cc(f), v)
#define Orb_ENDBUILDER\
	Orb_priv_ob_build(Orb_priv_ob); } while(0)

struct Orb_priv_ob_s* Orb_priv_ob_start(void);
void Orb_priv_ob_parent(struct Orb_priv_ob_s*, Orb_t);
void Orb_priv_ob_field_as_if_virtual(struct Orb_priv_ob_s*, Orb_t, Orb_t);
void Orb_priv_ob_field(struct Orb_priv_ob_s*, Orb_t, Orb_t);
Orb_t Orb_priv_ob_build(struct Orb_priv_ob_s*);

Orb_t Orb_virtual(Orb_t);
Orb_t Orb_virtual_x(void);
Orb_t Orb_method(Orb_t);

#ifdef __cplusplus
}
#endif

#endif /* LIBORB_H */
