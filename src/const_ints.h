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

/*
Type used as the representation of an integer value.  More precisely,
this is the form used on the host to represent a target integer.
*/
/*
At the first level, one must choose between a representation using
a single value of some host integer type and one using an array of
smaller host integers.  The latter form is necessary when the front
end is used as part of a cross-compiler where the target has larger
integers than the host.
*/
#define INTEGER_VALUE_REPR_IS_A_HOST_INTEGER TRUE

#if INTEGER_VALUE_REPR_IS_A_HOST_INTEGER

/*
There is a host integer that is large enough to hold all target integers,
so the integer representation is just some host integral type.
*/
typedef long an_integer_value;
/* The printf formatting specifier to be used to print the integer type. */
#define PRINTF_FORMAT_FOR_INTEGER_VALUE "%ld"  /* long */

#else /* !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */

/*
There is no host integer that is large enough, so use an array to represent
the target integers. */
*/
/* Type of the elements of the array.  These must be at least half the
   size of a_host_large_integer (some large and efficient integer type on
   the host), and (for space reasons) preferably exactly half.
   Typically, this is a 16-bit value.  The bit size and minimum and
   maximum values indicate the range of values to be used, which may
   be smaller than the range actually available. */
typedef short an_int_value_part;
#define BITS_IN_INT_VALUE_PART (sizeof(an_int_value_part)*CHAR_BIT)
#define MAX_UINT_VALUE_PART 0xffff
#define MAX_INT_VALUE_PART 0x7fff
#define MIN_INT_VALUE_PART (-0x8000)
#define SIGN_BIT_INT_VALUE_PART 0x8000
/* Large and efficient host integer, at least twice the size of
   an_int_value_part, used in doing computations on integer values.
   The idea is that any operation involving two an_int_value_part
   values in the range MIN_INT_VALUE_PART..MAX_INT_VALUE_PART can
   be done in a_host_large_integer without special coding to deal
   with overflows. */
typedef long a_host_large_integer;
#define BITS_IN_HOST_LARGE_INTEGER (sizeof(a_host_large_integer)*CHAR_BIT)
#define MAX_HOST_LARGE_INTEGER LONG_MAX
#define MIN_HOST_LARGE_INTEGER LONG_MIN
/* The array is made up of elements of type an_int_value_part. */
#define INT_VALUE_PARTS_PER_INTEGER_VALUE 4
/* This is an array inside a struct instead of just an array so that
   its address behaves in the normal way. */
typedef struct an_integer_value {
  an_int_value_part part[INT_VALUE_PARTS_PER_INTEGER_VALUE];
} an_integer_value;

#endif /* INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */

extern void set_integer_value(an_integer_value *intval,
                              long             value);

extern void set_unsigned_integer_value(an_integer_value *intval,
                                       unsigned long    value);

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

extern void incr_integer_value(an_integer_value *intval);

extern int bits_required_to_represent_integer_constant(a_constant *cp);

extern char *str_for_integer_constant(a_constant *cp);

extern void write_integer_constant(FILE       *f_output,
                                   a_constant *cp);

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
