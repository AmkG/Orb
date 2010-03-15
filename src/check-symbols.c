
#include<liborb.h>

#include<stdio.h>
#include<string.h>

#include<assert.h>

struct stringbuilder {
	char* string;
	size_t size;
	size_t capacity;
};

void build(struct stringbuilder* sb, char c) {
	if(sb->size == sb->capacity) {
		size_t nc = sb->capacity + sb->capacity / 2;
		if(nc < 4) nc = 4;
		char* ns = malloc(nc);
		memcpy(ns, sb->string, sb->size);
		free(sb->string);
		sb->string = ns; sb->capacity = nc;
	}
	sb->string[sb->size] = c;
	++sb->size;
}

int main(void) {
	Orb_init(0, 0);
	Orb_t x = Orb_symbol("hello");
	Orb_t y = Orb_symbol_cc("hello");
	assert(x == y);
	Orb_gc_trigger();
	Orb_t z = Orb_symbol_cc("goodbye");
	Orb_gc_trigger();
	Orb_t w = Orb_symbol("goodbye");
	assert(z == w);
	assert(x != z);

	Orb_t xw = Orb_deref_cc(x, "write");
	Orb_t zw = Orb_deref_cc(z, "write");
	assert(zw == xw);

	return 0;
}

