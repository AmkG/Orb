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

#include"liborb.h"

/*
Objects are tagged tuples, represented by simple
flat arrays.  The 0th entry is an opaque pointer
to a format, which is actually a somewhat richer
structure.  Succeeding entries of the flat array
are simply the tuple entries.

The format is another flat array.  The 0th entry
is the number of fields in the format, the 1th
entry is the name of the format.

Succeeding entries in the format form a sequence
of field-offset pairs.  The fields are arranged
in identity order.  So for example suppose we
have a constructor like the following:

  (Def-Cons (*cons a d))

Assuming that by identity order, 'd < 'a:

  format[0] = 2
  format[1] = '*cons
  format[2] = 'd
  format[3] = 2
  format[4] = 'a
  format[5] = 1
*/


