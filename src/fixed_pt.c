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
Initialize the given fixed-point value to zero.
*/
{
  set_integer_value(value, (a_host_large_integer)0);
}  /* fxp_init_value */


a_boolean fxp_value_is_zero(a_fixed_point_value  *value)
/*
Return TRUE if and only if the given fixed-point value is zero.
*/
{
  an_integer_value  zero;

  set_integer_value(&zero, (a_host_large_integer)0);
  /* Use the integer comparison routine.  Note that signedness doesn't
     matter for the zero case. */
  return (cmp_integer_values(value, /*op_1_signed=*/FALSE,
                             &zero, /*op_2_signed=*/FALSE) == 0);
}  /* fxp_value_is_zero */


static void conv_integer_value_to_long_double_value(
                                           an_integer_value         *ival,
                                           a_boolean                is_signed,
                                           an_internal_float_value  *fval,
                                           a_boolean                *err)
/*
Convert the integer value in *ival to a floating-point value (of type long
double) in *fval.  Set *err to TRUE if this does not work.

(The conversion is done through a conversion to string representation: It may
not be exact.  This routine is used for processing the representation of
fixed-point values as implicitly scaled integer values.)
*/
{
  a_constant  integer;
  char        *str;

  clear_constant(&integer, (a_constant_repr_kind)ck_integer);
#if LONG_LONG_ALLOWED
  integer.type =
              integer_type(is_signed ? (an_integer_kind)ik_long_long
                                     : (an_integer_kind)ik_unsigned_long_long);
#else /* !LONG_LONG_ALLOWED */
  integer.type = integer_type(is_signed ? (an_integer_kind)ik_long
                                        : (an_integer_kind)ik_unsigned_long);
#endif /* LONG_LONG_ALLOWED */
  integer.variant.integer_value = *ival;
  str = str_for_integer_constant(&integer);
  fp_string_to_float((a_float_kind)fk_long_double, str, fval, err);
}  /* conv_integer_value_to_long_double_value */


static void construct_fxp_scale_factor(a_fixed_point_type_descr  *fxp_descr,
                                       an_internal_float_value   *fp_scale)
/*
Construct a long double scaling factor 2^F where F is the number of fractional
bits in the fixed-point type represented by fxp_descr.  Place the result in
fp_scale.
*/
{
  int               fract_bits;
  an_integer_value  int_scale;  
  a_boolean         err = FALSE;

  fract_bits = targ_fractional_bits_for_fixed_point[fxp_descr->is_unsigned]
                                                   [(int)fxp_descr->precision]
                                                   [fxp_descr->is_fract_type];
  /* Shift the value "1" fract_bits to the left and convert the result to
     type "long double" using a string as an intermediate representation. */
  set_integer_value(&int_scale, (a_host_large_integer)1);
  shift_left_integer_value(&int_scale, fract_bits, &err);
  check_assertion(!err);
  conv_integer_value_to_long_double_value(&int_scale, /*is_signed=*/FALSE,
                                          fp_scale, &err);
  check_assertion(!err);
}  /* construct_fxp_scale_factor */


void fxp_string_to_fixed_point(a_fixed_point_type_descr  *fxp_descr,
                               char                      *str,
                               a_fixed_point_value       *value,
                               a_boolean                 *err)
/*
Convert the decimal fixed-point number in the null-terminated string str to
internal form in *value.  The number is known to be syntactically correct,
but may not be representable (it may be too large or too small); if there's
an error, return *err = TRUE.  The specific fixed-point kind is indicated by
*fxp_descr (and will typically affect the representation in *value).
The string need not have a decimal point or exponent (it can look like an
integer).  It may have a leading "-" sign.

This implementation is for demonstration purposes only: It is known to be
imprecise.  Specifically, this implementation scans the string as a floating-
point value and scales that value to obtain an integer that is used as the
fixed-point representation.  It relies on a_fixed_point_value being identical
to an_integer_value.
*/
{
  char        *result_str;
  an_internal_float_value
              fp_scale, fp_value, fp_scaled_value;
  a_boolean   depends_on_fp_mode;
  a_boolean   pos_infinity, neg_infinity, not_a_number;

  *err = FALSE;
  construct_fxp_scale_factor(fxp_descr, &fp_scale);

  /* Convert the given string to a floating point value. */
  fp_string_to_float((a_float_kind)fk_long_double, str, &fp_value, err);
  if (*err) {
    goto done;
  }  /* if */
  /* Multiply the floating-point representation of the given string by the
     scaling factor. */
  fp_multiply((a_float_kind)fk_long_double,
              &fp_scale, &fp_value, &fp_scaled_value,
              err, &depends_on_fp_mode);
  if (*err) {
    goto done;
  }  /* if */
  /* Extract the integer part of the result using a string as an intermediate
     representation. */
  result_str = fp_to_string((a_float_kind)fk_long_double, &fp_scaled_value,
                            &pos_infinity, &neg_infinity, &not_a_number);
  if (pos_infinity || neg_infinity || not_a_number) {
    *err = TRUE;
    goto done;
  }  /* if */
  conv_float_string_to_integer_value(result_str, value,
                                     !fxp_descr->is_unsigned, err);
done:;
}  /* fxp_string_to_fixed_point */


static int value_bits_for_fixed_point(a_fixed_point_type_descr	*fxp_descr)
/*
Return the number of data bits in a fixed point value (i.e., the number of
bits excluding the sign bit).
*/
{
  int	bits;

  bits = targ_sizeof_fixed_point[fxp_descr->is_unsigned]
                                [(int)fxp_descr->precision]
                                [fxp_descr->is_fract_type] * CHAR_BIT;
  if (!fxp_descr->is_unsigned) bits--;
  return bits;
}  /* value_bits_for_fixed_point */


static int sizeof_fixed_point(a_fixed_point_type_descr	*fxp_descr)
/*
Return the number of bytes in a fixed point value.
*/
{
  int	size;

  size = targ_sizeof_fixed_point[fxp_descr->is_unsigned]
                                [(int)fxp_descr->precision]
                                [fxp_descr->is_fract_type];
  return size;
}  /* sizeof_fixed_point */


static int non_fractional_bits_for_fixed_point(
				a_fixed_point_type_descr	*fxp_descr)
/*
Return the number of bits in the non-fractional part of a fixed point value.
The sign bit (if any) is included in the non-fractional bits.
*/
{
  int	fract_bits;
  int	total_bits;

  fract_bits = targ_fractional_bits_for_fixed_point[fxp_descr->is_unsigned]
                                                   [(int)fxp_descr->precision]
                                                   [fxp_descr->is_fract_type];
  total_bits = targ_sizeof_fixed_point[fxp_descr->is_unsigned]
                                      [(int)fxp_descr->precision]
                                      [fxp_descr->is_fract_type] * CHAR_BIT;
  return total_bits - fract_bits;
}  /* non_fractional_bits_for_fixed_point */


static void set_mantissa_to_saturated_value(
				a_mantissa_ptr			mp,
				a_fixed_point_type_descr	*fxp_descr)
/*
Set the mantissa to the value used to represent a saturated fixed-point
value.  This is all bits set to 1, except for the sign bit.
*/
{
  int i;

  for (i = 0; i < MANTISSA_PARTS; i++) mp->parts[i] = 0xffffffff;
  if (!fxp_descr->is_unsigned) mp->parts[0] = 0x7fffffff;
}  /* set_mantissa_to_saturated_value */


static void store_hex_fxp_value(
				a_mantissa_ptr			mp,
				a_fixed_point_type_descr	*fxp_descr,
				a_fixed_point_value		*value)
/*
Store the value represented by mp in the fixed point value "value".
fxp_descr describes the format of the value being stored.
*/
{
  int			parts_to_copy;
  int			source_size;

  /* Zero the memory so that all of the space occupied by "value"
     is cleared, even if we are not storing all of the bytes of the
     value. */
  memzero((char *)value, sizeof(a_fixed_point_value));
  source_size = sizeof_fixed_point(fxp_descr);
  parts_to_copy = source_size / sizeof(an_fp_value_part);
  /* The source value is in the upper source_size bytes of the mantissa.
     This needs to be copied to the low order bytes of the fixed point
     value. */
  if (host_little_endian) {
    int i;
    for (i = 0; i < source_size; ++i) {
      char	*source;
      char	*dest;
      dest = &((char*)value)[i];
      source = (char*)&(mp->parts[(parts_to_copy - 1) -
                                  (i / sizeof(an_fp_value_part))]) +
                       (i % sizeof(an_fp_value_part));
      *dest = *source;
    }  /* for */
  } else {
    /* Copy the value from the mantissa to the low order bytes of the
       fixed point value. */
    memcpy((char*)value + sizeof(a_fixed_point_value) - source_size,
           (char*)&mp->parts[0], size_t_arg(source_size));
  }  /* if */
}  /* store_hex_fxp_value */


static void conv_mantissa_to_fixed_point(
				a_mantissa_ptr			mp,
				long				exponent,
				a_fixed_point_type_descr	*fxp_descr,
				a_boolean			overflow,
				a_fixed_point_value		*value,
				a_boolean			*err,
				a_boolean			*inexact)
/*
Given a mantissa (mp) and exponent that represent a fixed point value, check
that the value is representable in the destination type specified by
fxp_descr and shift the value as needed so that it contains the correct
number of value bits for the destination type.  overflow is TRUE if
the value is already known to be too large.  Set *err on overflow.  Set
*inexact if any bits are lost because of scaling or rounding.
*/
{
  int		nonfract_bits;
  int		value_bits;
  int		shift_count;

  *err = FALSE;
  *inexact = FALSE;
  if (!overflow) {
    /* Compute the number of bits to shift the mantissa so that it contains
       the right number of fractional and non-fractional bits. */
    nonfract_bits = non_fractional_bits_for_fixed_point(fxp_descr);
    value_bits = value_bits_for_fixed_point(fxp_descr);
    shift_count = nonfract_bits - exponent;
    if (shift_count > 0) {
      shift_right_mantissa(mp, shift_count);
      /* See if the result value has more bits of precision than fit int
         the destination type. */
      if (number_of_bits_in_mantissa(mp) > value_bits) *inexact = TRUE;
      /* Round the value to the nearest representable value. */
      round_hex_fp_value(mp, &exponent, value_bits, inexact);
    } else {
      /* We would be shifting bits out of the high end of this mantissa. */
      overflow = TRUE;
    }  /* if */
  }  /* if */
#if DEBUG
  if (db_flag_is_set("fxp_conv")) {
    fprintf(f_debug, "fxp hex value: ");
    db_mantissa(mp);
    fprintf(f_debug, "exponent=%ld, nonfract=%d, shift=%d\n",
            exponent, nonfract_bits, shift_count);
  }  /* if */
#endif /* DEBUG */
  if (overflow) {
    /* On overflow, return an error flag and set the result value to a
       saturated value. */
    *err = TRUE;
    set_mantissa_to_saturated_value(mp, fxp_descr);
  }  /* if */
  /* Store the result in the appropriate form. */
  store_hex_fxp_value(mp, fxp_descr, value);
  /* If an underflow occurred, set the flag that indicates that the resulting
     value is not an exact representation of the specified value. */
  if (mp->underflow) *inexact = TRUE;
}  /* conv_mantissa_to_fixed_point */


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
*/
{
  a_boolean	overflow = FALSE;
  long		exponent = 0;
  a_mantissa	mantissa;
  a_boolean	any_digits = FALSE;

  conv_hex_string_to_mantissa_and_exponent(str, &mantissa, &exponent,
                                           &any_digits, &overflow);
  conv_mantissa_to_fixed_point(&mantissa, exponent, fxp_descr,
                               overflow, value, err, inexact);
}  /* fxp_hex_string_to_fixed_point */


char* fxp_to_string(a_fixed_point_type_descr  *fxp_descr,
                    a_fixed_point_value       *value)
/*
Convert the given value with the given fixed-point type description to a
decimal (null-terminated) string representation in an internal static array.
Return a pointer to that array.  Suffixes are appended as needed.

(This implementation assumes a_fixed_point_value is a synonym for
an_integer_value and may produce slightly inaccurate results.)
*/
{
#define BUF_LENGTH 100
  static char  str[BUF_LENGTH];

  an_internal_float_value
              fp_scale, fp_value, fp_scaled_value;
  a_boolean   depends_on_fp_mode;
  a_boolean   pos_infinity, neg_infinity, not_a_number;
  a_boolean   err = FALSE;
  char        *result_str;
  sizeof_t    length;

  construct_fxp_scale_factor(fxp_descr, &fp_scale);
  /* Treat the given fixed-point as an integer (not yet scaled) and convert
     is to type "long double". */
  conv_integer_value_to_long_double_value(value, !fxp_descr->is_unsigned,
                                          &fp_value, &err);
  check_assertion(!err);
  /* Scale the result and convert it to a string. */
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
#undef BUF_LENGTH
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
