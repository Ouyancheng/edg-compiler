/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2003 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
/*

fixed_pt.c -- Routines that manipulate internal fixed-point quantities.

The versions in this file are for prototyping only, and should be replaced
for a production version.

*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

#if FIXED_POINT_EXTENSIONS_ALLOWED

void fxp_init_value(a_fixed_point_value  *value)
/*
Initialize the given fixed-point value to a safe representation.
*/
{
  set_integer_value(value, (a_host_large_integer)0);
}  /* fxp_init_value */


static void conv_integer_value_to_long_double_value(
                                               an_integer_value         *ival,
                                               an_internal_float_value  *fval,
                                               a_boolean                *err)
/*
Convert the integer value in *ival to a floating-point value (of type long
double) in *fval.  Set *err to TRUE if this does not work.

(The conversion is done through a conversion to string representation: It may
not be exact.  This routine is used for processing the representation of
fixed-point value as implicitly scaled integer values.)
*/
{
  a_constant  integer;
  char        *str;

  clear_constant(&integer, (a_constant_repr_kind)ck_integer);
#if LONG_LONG_ALLOWED
  integer.type = integer_type((an_integer_kind)ik_unsigned_long_long);
#else /* !LONG_LONG_ALLOWED */
  integer.type = integer_type((an_integer_kind)ik_unsigned_long);
#endif /* LONG_LONG_ALLOWED */
  integer.variant.integer_value = *ival;
  str = str_for_integer_constant(&integer);
  fp_string_to_float((a_float_kind)fk_long_double, str, fval, err);
}  /* conv_integer_value_to_long_double_value */


void fxp_string_to_fixed_point(a_fixed_point_type_descr  *fxp_descr,
                               char                      *str,
                               a_fixed_point_value       *value,
                               a_boolean                 *err)
/*
Convert the decimal fixed-point number in the null-terminated string str to
internal form in *value.  The number is known to be syntactically correct,
but may not be representable (it may be too large or too small); if there's
an error, return *err = TRUE.  The specific fixed-point kind is indicated by
*fxp_descr (an will typically affect the representation in *value).
The string need not have a decimal point or exponent (it can look like an
integer).  It may have a leading "-" sign.

This implementation is for demonstration purposes only: It is known to be
imprecise.  Specifically, this implementation scans the string as a floating-
point value and scales that value to obtain an integer that is used as the
fixed-point representation.  It relies on a_fixed_point_value being identical
to an_integer_value.
*/
{
  an_integer_value
              int_scale;
  char        *result_str;
  an_internal_float_value
              fp_scale, fp_value, fp_scaled_value;
  int         fract_bits;
  a_boolean   depends_on_fp_mode;
  a_boolean   pos_infinity, neg_infinity, not_a_number;

  *err = FALSE;
  fract_bits = targ_fractional_bits_for_fixed_point[fxp_descr->is_unsigned]
                                                   [(int)fxp_descr->precision]
                                                   [fxp_descr->is_fract_type];
  set_integer_value(&int_scale, (a_host_large_integer)1);
  shift_left_integer_value(&int_scale, fract_bits, err);
  check_assertion(!*err);
  conv_integer_value_to_long_double_value(&int_scale, &fp_scale, err);
  check_assertion(!*err);
  fp_string_to_float((a_float_kind)fk_long_double, str, &fp_value, err);
  if (*err) {
    goto done;
  }  /* if */
  fp_multiply((a_float_kind)fk_long_double,
              &fp_scale, &fp_value, &fp_scaled_value,
              err, &depends_on_fp_mode);
  result_str = fp_to_string((a_float_kind)fk_long_double, &fp_scaled_value,
                            &pos_infinity, &neg_infinity, &not_a_number);
  if (pos_infinity || neg_infinity || not_a_number) {
    goto done;
  }  /* if */
  conv_float_string_to_integer_value(result_str, value, /*is_signed=*/TRUE,
                                     err);
done:;
}  /* fxp_string_to_fixed_point */


void fxp_hex_string_to_fixed_point(a_fixed_point_type_descr  *fxp_descr,
                                   char                      *str,
                                   a_fixed_point_value       *value,
                                   a_boolean                 *err,
                                   a_boolean                 *inexact)
/*
Convert the hexadecimal fixed-point number in the null-terminated string str
to internal form in *value.  The number is known to be syntactically correct,
but may not be representable (it may be too large or too small); if there's
an error, return *err = TRUE.  The specific fixed-point kind is indicated by
*fxp_descr (an will typically affect the representation in *value).
*inexact is set to TRUE if *value does not exactly represent the value
indicated by the given string.  Otherwise, it is set to FALSE.

This implementation is for demonstration purposes only: It is known to be
imprecise.  Specifically, this implementation scans the string as a floating-
point value and scales that value to obtain an integer that is used as the
fixed-point representation.
*/
{
  unexpected_condition();
}  /* fxp_hex_string_to_fixed_point */


char* fxp_to_string(a_fixed_point_type_descr  *fxp_descr,
                    a_fixed_point_value       *value)
/*
Convert the given value with the given fixed-point type description to a
decimal (null-terminated) string representation in an internal static array.
Return a pointer to that array.

(This implementation assumes a_fixed_point_value is a synonym for
an_integer_value and may produce slightly inaccurate results.)
*/
{
#define BUF_LENGTH 100
  static char  str[BUF_LENGTH];

  an_integer_value
              int_scale;
  an_internal_float_value
              fp_scale, fp_value, fp_scaled_value;
  int         fract_bits;
  a_boolean   depends_on_fp_mode;
  a_boolean   pos_infinity, neg_infinity, not_a_number;
  a_boolean   err = FALSE;
  char        *result_str;
  sizeof_t    length;

  fract_bits = targ_fractional_bits_for_fixed_point[fxp_descr->is_unsigned]
                                                   [(int)fxp_descr->precision]
                                                   [fxp_descr->is_fract_type];
  set_integer_value(&int_scale, (a_host_large_integer)1);
  shift_left_integer_value(&int_scale, fract_bits, &err);
  check_assertion(!err);
  conv_integer_value_to_long_double_value(&int_scale, &fp_scale, &err);
  check_assertion(!err);

  conv_integer_value_to_long_double_value(value, &fp_value, &err);
  check_assertion(!err);
  fp_divide((a_float_kind)fk_long_double,
            &fp_value, &fp_scale, &fp_scaled_value,
            &err, &depends_on_fp_mode);
  result_str = fp_to_string((a_float_kind)fk_long_double, &fp_scaled_value,
                            &pos_infinity, &neg_infinity, &not_a_number);
  check_assertion(!(pos_infinity || neg_infinity || not_a_number));
  length = strlen(result_str);
  check_assertion(length < BUF_LENGTH - 4);
  strcpy(str, result_str);
  /* Add all the needed suffixes. */
  if (fxp_descr->is_unsigned) {
    str[length++] = 'u';
  };
  if (fxp_descr->precision == (a_fixed_point_precision)fpp_short) {
    str[length++] = 'h';
  } else if (fxp_descr->precision == (a_fixed_point_precision)fpp_long) {
    str[length++] = 'l';
  };
  str[length++] = fxp_descr->is_fract_type ? 'r' : 'k';
  str[length] = '\0';
  return str;
}  /* fxp_to_string */


unsigned int fxp_hash(a_fixed_point_value  *value)
/*
Return a hash value derived from the given fixed-point value.  This is used
in building the hash table for shareable constants.  (This implementation
assumes a_fixed_point_value is a synonym for an_integer_value.)
*/
{
  a_constant  int_constant;
  a_boolean   ovflo;

  clear_constant(&int_constant, (a_constant_repr_kind)ck_integer);
#if LONG_LONG_ALLOWED
  int_constant.type = integer_type((an_integer_kind)ik_unsigned_long_long);
#else /* !LONG_LONG_ALLOWED */
  int_constant.type = integer_type((an_integer_kind)ik_unsigned_long);
#endif /* LONG_LONG_ALLOWED */
  int_constant.variant.integer_value = *value;
  
  return (unsigned int)value_of_integer_constant(&int_constant, &ovflo);
}  /* fxp_hash */

#endif /* FIXED_POINT_EXTENSIONS_ALLOWED */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2003 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
