/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
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

extern a_host_fp_value fetch_host_fp_value(
				a_float_kind            kind,
				an_internal_float_value *float_value);

#if TARG_HAS_IEEE_FLOATING_POINT
extern void make_fp_nan(an_internal_float_value *value);

extern void make_fp_infinity(an_internal_float_value *value);
#endif /* TARG_HAS_IEEE_FLOATING_POINT */

extern void fp_change_kind(an_internal_float_value *old_value,
                           a_float_kind            old_kind,
                           an_internal_float_value *new_value,
                           a_float_kind            new_kind,
                           a_boolean               *err,
                           a_boolean               *depends_on_rounding_mode);

extern
void fp_hex_string_to_float(a_float_kind		kind,
	                    char			*str,
	                    an_internal_float_value	*float_value,
	                    a_boolean			*err,
			    a_boolean			*inexact);

extern void fp_string_to_float(a_float_kind            kind,
                               char                    *str,
                               an_internal_float_value *float_value,
                               a_boolean               *err);

extern char *fp_to_string(a_float_kind            kind,
                          an_internal_float_value *float_value,
                          a_boolean               *pos_infinity,
                          a_boolean               *neg_infinity,
                          a_boolean               *not_a_number);

extern
void fp_host_large_integer_to_float(a_float_kind            kind,
		                    a_host_large_integer    int_value,
                                    an_internal_float_value *float_value,
                                    a_boolean               *err);

#ifdef CFE
extern void fp_host_large_unsigned_to_float(
                      a_float_kind            kind, 
                      a_host_large_unsigned   unsigned_value,
                      an_internal_float_value *float_value,
                      a_boolean               *err);
#endif /* ifdef CFE */

extern void fp_to_host_large_integer(
			a_float_kind            kind,
			an_internal_float_value *float_value,
			a_host_large_integer    *int_value,
			a_boolean               *err,
			a_boolean               *depends_on_rounding_mode);

#ifdef CFE
extern void fp_to_host_large_unsigned(
			a_float_kind            kind,
			an_internal_float_value *float_value,
			a_host_large_unsigned   *unsigned_value,
			a_boolean               *err,
			a_boolean               *depends_on_rounding_mode);
#endif /* ifdef CFE */

extern a_boolean fp_is_zero_constant(a_float_kind            kind,
                                     an_internal_float_value *float_value);

extern void fp_add(a_float_kind            kind,
                   an_internal_float_value *value_1,
                   an_internal_float_value *value_2,
                   an_internal_float_value *result,
                   a_boolean               *err,
                   a_boolean               *depends_on_rounding_mode);

extern void fp_subtract(a_float_kind            kind,
                        an_internal_float_value *value_1,
                        an_internal_float_value *value_2,
                        an_internal_float_value *result,
                        a_boolean               *err,
                        a_boolean               *depends_on_rounding_mode);

extern void fp_negate(a_float_kind            kind,
                      an_internal_float_value *value_1,
                      an_internal_float_value *result,
                      a_boolean               *err);

extern void fp_multiply(a_float_kind            kind,
                        an_internal_float_value *value_1,
                        an_internal_float_value *value_2,
                        an_internal_float_value *result,
                        a_boolean               *err,
                        a_boolean               *depends_on_rounding_mode);

extern void fp_divide(a_float_kind            kind,
                      an_internal_float_value *value_1,
                      an_internal_float_value *value_2,
                      an_internal_float_value *result,
                      a_boolean               *err,
                      a_boolean               *depends_on_rounding_mode);

extern int fp_compare(a_float_kind            kind,
                      an_internal_float_value *float_value_1,
                      an_internal_float_value *float_value_2,
                      a_boolean               *unordered);

extern unsigned int fp_hash(an_internal_float_value *value);

extern void float_pt_init(void);

#endif /* ifndef FLOAT_PT_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/

