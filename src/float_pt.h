/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
/*

float_pt.h -- Declarations for float_pt.c (having to do with manipulation of
              internal floating-point quantities).

*/

/* Avoid including these declarations more than once: */
#ifndef FLOAT_PT_H
#define FLOAT_PT_H 1

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */


extern void fp_check_fit(a_float_kind            kind,
                         an_internal_float_value *float_value,
                         a_boolean               *err);

extern void fp_string_to_float(a_float_kind            kind,
                               char                    *str,
                               an_internal_float_value *float_value,
                               a_boolean               *err);

extern char *fp_to_string(an_internal_float_value *float_value);


extern void fp_long_to_float(a_float_kind            kind, 
                             long                    long_value,
                             an_internal_float_value *float_value,
                             a_boolean               *err);

extern void fp_unsigned_long_to_float(
                      a_float_kind            kind, 
                      unsigned long           unsigned_long_value,
                      an_internal_float_value *float_value,
                      a_boolean               *err);

extern void fp_to_long(an_internal_float_value *float_value,
                       long                    *long_value,
                       a_boolean               *err);

extern void fp_to_unsigned_long(an_internal_float_value *float_value,
                                unsigned long           *unsigned_long_value,
                                a_boolean               *err);

extern a_boolean fp_is_zero_constant(an_internal_float_value *float_value);

extern void fp_add(a_float_kind            kind,
                   an_internal_float_value *value_1,
                   an_internal_float_value *value_2,
                   an_internal_float_value *result,
                   a_boolean               *err);

extern void fp_subtract(a_float_kind            kind,
                        an_internal_float_value *value_1,
                        an_internal_float_value *value_2,
                        an_internal_float_value *result,
                        a_boolean               *err);

extern void fp_multiply(a_float_kind            kind,
                        an_internal_float_value *value_1,
                        an_internal_float_value *value_2,
                        an_internal_float_value *result,
                        a_boolean               *err);

extern void fp_divide(a_float_kind            kind,
                      an_internal_float_value *value_1,
                      an_internal_float_value *value_2,
                      an_internal_float_value *result,
                      a_boolean               *err);

extern int fp_compare(an_internal_float_value *float_value_1,
                      an_internal_float_value *float_value_2,
                      a_boolean               *unordered);

#endif /* ifndef FLOAT_PT_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
