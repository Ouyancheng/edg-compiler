/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

const_ints.h -- Declarations related to manipulation of target integer
                constants.

*/

/* Avoid including these declarations more than once. */
#ifndef CONST_INTS_H
#define CONST_INTS_H 1

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */

extern a_boolean int_kind_is_signed(an_integer_kind kind);

extern a_boolean int_constant_is_signed(a_constant_ptr constant);

extern long value_of_integer_constant(a_constant *cp,
                                      a_boolean  *ovflo);

extern unsigned long unsigned_value_of_integer_constant(a_constant *cp,
                                                        a_boolean  *ovflo);

extern int cmp_integer_constants(a_constant *con1,
                                 a_constant *con2);

extern int cmplit_integer_constant(a_constant *con1,
                                   long       value2);

/* Interface to cmplit_integer_constant for the simple case of testing
   for equality. */
#define eqlit_integer_constant(con1, value2)                          \
  (cmplit_integer_constant((con1), (value2)) == 0)

/* Interface to cmplit_integer_constant for the simple case of getting
   the sign (-1, 0, +1) of an integer constant. */
#define sign_of_integer_constant(con) cmplit_integer_constant((con), 0L)

extern int cmpulit_integer_constant(a_constant    *con1,
                                    unsigned long unsigned_value2);

extern void incr_integer_constant(a_constant *cp);

extern int bits_required_to_represent_integer_constant(a_constant *cp);

extern void write_integer_constant(FILE       *f_output,
                                   a_constant *cp);

extern void set_value_of_integer_constant(a_constant *cp,
                                          long       value,
                                          a_type_ptr type);

extern void set_unsigned_value_of_integer_constant(a_constant    *cp,
                                                   unsigned long value,
                                                   a_type_ptr type);

#endif /* ifndef CONST_INTS_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1991 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
