#ifndef SEQ_H
#define SEQ_H

void Orb_seq_init(void);

/*Determines if the given object is a sequence backed by an
array.  If so, fills in the backing array's details and
returns 1.
*/
int Orb_array_backed(Orb_t seq, Orb_t const** parr, size_t* pstart, size_t* psz);

/*decompose sequence into parts
return 0 means sequence is empty
return 1 means single-element sequence and value is in *psingle
return 2 means concatenation cell and left and right are in
 *pl and *pr respectively
*/
int Orb_seq_decompose(Orb_t seq, Orb_t* psingle, Orb_t* pl, Orb_t* pr);

/*cfunc's from various parts*/
Orb_t Orb_iterate_cfunc(Orb_t argv[], size_t* pargc, size_t argl);
Orb_t Orb_map_cfunc(Orb_t argv[], size_t* pargc, size_t argl);
Orb_t Orb_mapreduce_cfunc(Orb_t argv[], size_t* pargc, size_t argl);

/*initialization for various parts*/
void Orb_iterate_init(void);
void Orb_map_init(void);
void Orb_mapreduce_init(void);

#endif

