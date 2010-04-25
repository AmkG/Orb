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

