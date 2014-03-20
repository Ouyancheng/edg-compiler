/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2013 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

literals.c -- Literal constant conversion to and from internal form.

*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Additional header files. */
#include "literals.h"
#include "preproc.h"


void conv_integer_literal(int           radix,
                          an_error_code *err_code,
                          a_const_char  **err_pos)
/*
Convert an integer of base indicated by radix (2, 8, 10, or 16) from
external form to internal form.  start_of_curr_token and
end_of_curr_token point to the two ends of the external form.  The
internal form is placed in const_for_curr_token.  If there is no error,
*err_code is set to ec_no_error (which is 0); otherwise, *err_code is
set to an appropriate error code and *err_pos is set to the character
position of the error.  A zero-length number is converted as zero.
Other than the zero-length pathology, the input number is guaranteed
to be syntactically correct (except for digits 8 and 9 in octal 
constants or digits above 1 for binary constants).  The number may have
a "u" or "l" suffix, or both. (Or a "ll" or "ull" suffix, if long long
is allowed.) (Or a suffix like "i32", if Microsoft extensions are enabled.)
*/
{
  an_integer_value number, ten, digit, mask;
  a_boolean        has_u_suffix = FALSE, has_l_suffix = FALSE;
#if LONG_LONG_ALLOWED
  a_boolean        has_ll_suffix = FALSE;
  char		   l_char_used = '\0';
#endif /* LONG_LONG_ALLOWED */
  a_const_char     *temp_ptr;
  a_boolean        err, ovflo = FALSE, do_sign_extension = FALSE;
  a_boolean        non_arith = (radix != 10);
  a_const_char     *real_end_pos = end_of_curr_token;
  unsigned long    intdigit;
  an_integer_kind  kind;
#if MICROSOFT_EXTENSIONS_ALLOWED
  an_integer_kind  isuffix_kind = (an_integer_kind)ik_none;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

  *err_code = ec_no_error;
  /* Locate and logically remove the suffix, if any.  The suffix is "u"
     for unsigned or "l" for long, or both, in upper or lower case. */
#if LONG_LONG_ALLOWED
  /* "ll" means long long, "ull" means unsigned long long. */
#endif /* LONG_LONG_ALLOWED */
  if (real_end_pos >= start_of_curr_token) {
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode) {
      /* The Microsoft compiler allows a suffix like "i32" indicating a
         32-bit integer.  "ui32" indicates an unsigned 32-bit integer. */
      /* Look for an "i" or "I" anywhere in the number. */
      /*lint --e{850} temp_ptr modified in loop */
      for (temp_ptr = start_of_curr_token;
           temp_ptr <= real_end_pos;
           temp_ptr++) {
        if (*temp_ptr == 'i' || *temp_ptr == 'I') {
          /* Yes, we have a suffix like "i32". */
          /* scan_number has ensured that there is at least one digit
             following the "i". */
          a_const_char  *suffix_loc = temp_ptr;
          unsigned long isuffix = 0;
          unsigned long ndigits = 0;

          real_end_pos = temp_ptr-1;
          temp_ptr++;
          /* Accumulate the size. */
          do {
            isuffix *= 10;
            isuffix += *temp_ptr++ - '0';
            ndigits++;
          } while (isdigit((unsigned char)(*temp_ptr)));
          /* Check that the size is valid. */
          if (ndigits <= 3) {
            if (isuffix == 8 &&
                targ_int8_int_kind != (an_integer_kind)ik_none) {
              isuffix_kind = targ_int8_int_kind;
            } else if (isuffix == 16 &&
                       targ_int16_int_kind != (an_integer_kind)ik_none) {
              isuffix_kind = targ_int16_int_kind;
            } else if (isuffix == 32 &&
                       targ_int32_int_kind != (an_integer_kind)ik_none) {
              isuffix_kind = targ_int32_int_kind;
            } else if (isuffix == 64 &&
                       targ_int64_int_kind != (an_integer_kind)ik_none) {
              isuffix_kind = targ_int64_int_kind;
            }  /* if */
          }  /* if */
          if (isuffix_kind == (an_integer_kind)ik_none) {
            /* Bad size. */
            *err_pos = suffix_loc;
            *err_code = ec_bad_suffix;
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    for (;;) {
      if (*real_end_pos == 'u' || *real_end_pos == 'U') {
        has_u_suffix = TRUE;
        real_end_pos--;
      } else if (*real_end_pos == 'l' || *real_end_pos == 'L') {
#if LONG_LONG_ALLOWED
        if (has_l_suffix) {
          has_l_suffix = FALSE;
          has_ll_suffix = TRUE;
          if (*real_end_pos != l_char_used && strict_ansi_mode) {
            /* An invalid suffix such as "Ll" or "lL".  Give an error but
               still treat it as a long long. */
            *err_pos = real_end_pos;
            *err_code = ec_bad_suffix;
          }  /* if */
        } else
#endif /* LONG_LONG_ALLOWED */
        {
          has_l_suffix = TRUE;
#if LONG_LONG_ALLOWED
          l_char_used = *real_end_pos;
#endif /* LONG_LONG_ALLOWED */
        }  /* if */
        real_end_pos--;
      } else {
        /* Not an "l" or "u"; exit loop. */
        break;
      }  /* if */
    }  /* for */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode && isuffix_kind != (an_integer_kind)ik_none &&
        has_u_suffix) {
      /* The number has a suffix like "ui32".  Adjust the kind to the
         corresponding unsigned integral kind. */
      isuffix_kind = unsigned_int_kind_of[(int)isuffix_kind];
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */

  /* Evaluate the literal as an unsigned long. */
  if (radix == 10) {
    /* Decimal. */
    set_unsigned_integer_value(&ten, (a_host_large_unsigned)10);
    intdigit = *start_of_curr_token - '0';
    set_unsigned_integer_value(&number, (a_host_large_unsigned)intdigit);
    for (temp_ptr = start_of_curr_token+1;
         temp_ptr <= real_end_pos; temp_ptr++) {
      intdigit = *temp_ptr - '0';
      /* Multiply previous value by 10, checking for overflow. */
      multiply_integer_values(&number, &ten, /*is_signed=*/FALSE, &err);
      if (err) ovflo = TRUE;
      /* Add in digit, checking for overflow. */
      set_unsigned_integer_value(&digit, (a_host_large_unsigned)intdigit);
      add_integer_values(&number, &digit, /*is_signed=*/FALSE, &err);
      if (err) ovflo = TRUE;
    }  /* for */
  } else if (radix == 8) {
    /* Octal.*/
    set_unsigned_integer_value(&number, (a_host_large_unsigned)0);
    for (temp_ptr = start_of_curr_token+1;
         temp_ptr <= real_end_pos; temp_ptr++) {
      intdigit = *temp_ptr - '0';
      if (C_dialect != C_dialect_pcc && (intdigit >= 8)) {
        /* Digits 8 and 9 are allowed by K&R/pcc, but not by ANSI. */
        *err_pos = temp_ptr;
        *err_code = ec_bad_octal_digit;
        goto wrapup;
      }  /* if */
      /* Multiply previous value by 8, checking for overflow. */
      shift_left_integer_value(&number, 3, &err);
      if (err) ovflo = TRUE;
      /* Or in digit. */
      set_unsigned_integer_value(&digit, (a_host_large_unsigned)intdigit);
      or_integer_values(&number, &digit);
    }  /* for */
  } else if (radix == 2) {
    /* Binary.*/
    set_unsigned_integer_value(&number, (a_host_large_unsigned)0);
    for (temp_ptr = start_of_curr_token+2;
         temp_ptr <= real_end_pos; temp_ptr++) {
      intdigit = *temp_ptr - '0';
      if (intdigit >= 2) {
        /* Digits over 1 are not allowed. */
        *err_pos = temp_ptr;
        *err_code = ec_bad_binary_digit;
        goto wrapup;
      }  /* if */
      /* Multiply previous value by 2, checking for overflow. */
      shift_left_integer_value(&number, 1, &err);
      if (err) ovflo = TRUE;
      /* Or in digit. */
      set_unsigned_integer_value(&digit, (a_host_large_unsigned)intdigit);
      or_integer_values(&number, &digit);
    }  /* for */
  } else {
    /* radix == 16 (hexadecimal). */
    set_unsigned_integer_value(&number, (a_host_large_unsigned)0);
    for (temp_ptr = start_of_curr_token+2;
         temp_ptr <= real_end_pos; temp_ptr++) {
      intdigit = hexvalue(*temp_ptr);
      /* Multiply previous value by 16, checking for overflow. */
      shift_left_integer_value(&number, 4, &err);
      if (err) ovflo = TRUE;
      /* Or in digit. */
      set_unsigned_integer_value(&digit, (a_host_large_unsigned)intdigit);
      or_integer_values(&number, &digit);
    }  /* for */
  }  /* if */
  /* Determine the type based on the value and the suffixes.  See standard,
     3.1.3.2 (for C89). */
  if (in_pp_if_expression && (c99_mode || gnu_mode)) {
    /* C99 was amended with DR 265 to the effect that the conversion of an
       integer literal in a #if control expression should treat all integer
       types as having the same representation as intmax_t or uintmax_t
       (depending on their signedness). */
    if (has_u_suffix) {
      /* A "u" suffix is always mapped onto an unsigned type. */
      kind = targ_uintmax_kind;
    } else if (radix == 10 ||
               le_max_integer_value_of_kind(&number, /*is_signed=*/FALSE,
                                      targ_intmax_kind)) {
      /* Decimal literals without a "u" suffix are always signed.  Nondecimal
         literals are signed if they can be represented by the signed type. */
      kind = targ_intmax_kind;
    } else {
      kind = targ_uintmax_kind;
    }  /* if */
    goto kind_established;
  }  /* if */
  /* In pcc compatibility mode, overflow is ignored, and the constant is
     either int or long (see K&R, reference manual section, 2.4.1 and 2.4.2).
     Since the "u" suffix does not exist in pcc C, treat constants with that
     suffix according to the ANSI rules. */
#if LONG_LONG_ALLOWED
  /* Likewise for "ll". */
#endif /* LONG_LONG_ALLOWED */
  if (C_dialect == C_dialect_pcc && !has_u_suffix
#if LONG_LONG_ALLOWED
      && !has_ll_suffix
#endif /* LONG_LONG_ALLOWED */
                                                 ) {
    /* Non-ANSI (pcc) checking. */
    if (has_l_suffix) goto pcc_l_check;
    if (radix == 10 &&
        le_max_integer_value_of_kind(&number, /*is_signed=*/FALSE,
                                     (an_integer_kind)ik_int)) {
      /* A decimal constant that is no larger than the largest signed int
         is an int. */
      kind = (an_integer_kind)ik_int;
      goto pcc_kind_established;
    }  /* if */
    if (radix != 10 &&
        le_max_integer_value_of_kind(&number, /*is_signed=*/FALSE,
                                     (an_integer_kind)ik_unsigned_int)) {
      /* A hexadecimal or octal constant that is no larger than the largest
         unsigned int is treated as an int (there are no unsigned int
         constants in K&R/pcc). */
      kind = (an_integer_kind)ik_int;
      do_sign_extension = TRUE;
      goto pcc_kind_established;
    }  /* if */
pcc_l_check:
    if (le_max_integer_value_of_kind(&number, /*is_signed=*/FALSE,
                                     (an_integer_kind)ik_unsigned_long)) {
      /* A constant that is no larger than the largest unsigned long is
         treated as a long (there are no unsigned long constants in
         K&R/pcc). */
      kind = (an_integer_kind)ik_long;
      do_sign_extension = TRUE;
      /* A decimal constant that is greater than the largest long is considered
         a long, but tagged as non-arithmetic because the source looks
         positive but the internal value is negative.  This helps in
         avoiding an error when converting the smallest integer. */
      if (!non_arith &&
          !le_max_integer_value_of_kind(&number, /*is_signed=*/FALSE,
                                        (an_integer_kind)ik_long)) {
        non_arith = TRUE;
      }  /* if */
      goto pcc_kind_established;
    }  /* if */
#if LONG_LONG_ALLOWED
    if (le_max_integer_value_of_kind(&number, /*is_signed=*/FALSE,
                                     (an_integer_kind)ik_unsigned_long_long)) {
      /* long long. */
      kind = (an_integer_kind)ik_long_long;
      do_sign_extension = TRUE;
      /* A decimal constant that is larger than LONG_LONG_MAX is tagged as
         non-arithmetic because the source looks positive but the internal
         value is negative.  This helps in avoiding an error when
         converting the smallest integer. */
      if (!non_arith &&
          !le_max_integer_value_of_kind(&number, /*is_signed=*/FALSE,
                                        (an_integer_kind)ik_long_long)) {
        non_arith = TRUE;
      }  /* if */
      goto pcc_kind_established;
    }  /* if */
#endif /* LONG_LONG_ALLOWED */
    /* Doesn't fit in target integers.  This can only happen when the
       host representation for integer values can hold values larger
       than the largest target integer. */
    ovflo = TRUE;
#if LONG_LONG_ALLOWED
    kind = (an_integer_kind)ik_long_long;
#else /* !LONG_LONG_ALLOWED */
    kind = (an_integer_kind)ik_long;
#endif /* LONG_LONG_ALLOWED */
pcc_kind_established:
    if (ovflo) {
      /* A warning is generated for overflow, but the overflow is then
         ignored.  The conversions above produce the same value that pcc
         does. */
      /* Convert the character position into an error position. */
      conv_line_loc_to_source_pos(start_of_curr_token, &error_position);
      warning(ec_integer_too_large);
      do_sign_extension = int_kind_is_signed[kind];
      /* Mask off any bits past the end of the largest target integer. */
      make_integer_value_mask(&mask,
                             (int)(TARG_SIZEOF_LARGEST_INTEGER*targ_char_bit));
      and_integer_values(&number, &mask);
      ovflo = FALSE;
    }  /* if */
  } else if (!ovflo) {
    /* Non-pcc-mode constant checking. */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode && isuffix_kind != (an_integer_kind)ik_none) {
      /* The number has a suffix like "i32".  The kind has already been
         determined. */
      kind = isuffix_kind;
      /* If necessary, truncate the constant to the size specified. */
      if (!le_max_integer_value_of_kind(&number, /*is_signed=*/FALSE, kind)) {
        a_targ_size_t    size;
        a_targ_alignment alignment;

        /* The constant doesn't fit in the integer kind. */
        /* Convert the character position into an error position. */
        conv_line_loc_to_source_pos(start_of_curr_token, &error_position);
        warning(ec_integer_too_large);
        /* Mask off any bits past the end of the integer. */
        do_sign_extension = int_kind_is_signed[kind];
        get_integer_size_and_alignment(kind, &size, &alignment);
        make_integer_value_mask(&mask, (int)(size*targ_char_bit));
        and_integer_values(&number, &mask);
      }  /* if */
      goto kind_established;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* ANSI C constant checking. */
#if LONG_LONG_ALLOWED
    if (has_ll_suffix) goto ll_check;
#endif /* LONG_LONG_ALLOWED */
    if (has_l_suffix) goto l_check;
    if (!has_u_suffix &&
        le_max_integer_value_of_kind(&number, /*is_signed=*/FALSE,
                                     (an_integer_kind)ik_int)) {
      kind = (an_integer_kind)ik_int;
      goto kind_established;
    } else if ((has_u_suffix || radix != 10) &&
               le_max_integer_value_of_kind(&number, /*is_signed=*/FALSE,
                                           (an_integer_kind)ik_unsigned_int)) {
      kind = (an_integer_kind)ik_unsigned_int;
      goto kind_established;
    }  /* if */
l_check:
    if (!has_u_suffix &&
        le_max_integer_value_of_kind(&number, /*is_signed=*/FALSE,
                                     (an_integer_kind)ik_long)) {
      kind = (an_integer_kind)ik_long;
      goto kind_established;
    } else if ((has_u_suffix || radix != 10 || !long_long_promotion_allowed) &&
               le_max_integer_value_of_kind(&number, /*is_signed=*/FALSE,
                                          (an_integer_kind)ik_unsigned_long)) {
      /* When long long is not a standard type (including the case when long
         long does not exist) a signed constant promotes to unsigned long
         before (possibly) considering long long. */
      kind = (an_integer_kind)ik_unsigned_long;
      goto kind_established;
    }  /* if */
#if LONG_LONG_ALLOWED
ll_check:
    if (strict_ansi_mode && !long_long_is_standard && !has_ll_suffix) {
      /* long long is not supported, so skip these range checks to force
         a constant-too-large error.  Don't skip if there is an explicit
         "ll" suffix, to avoid two errors. */
    } else if (!has_u_suffix &&
        le_max_integer_value_of_kind(&number, /*is_signed=*/FALSE,
                                     (an_integer_kind)ik_long_long)) {
      kind = (an_integer_kind)ik_long_long;
      goto kind_established;
    } else if ((!c99_mode || has_u_suffix || radix != 10) &&
               le_max_integer_value_of_kind(
                                     &number, /*is_signed=*/FALSE,
                                     (an_integer_kind)ik_unsigned_long_long)) {
      /* Note that in C99 a constant that is too large for long long
         but is decimal with no U suffix does not get unsigned long long
         type (that's designed to allow C99 implementations to have
         integral types larger than long long). */
      kind = (an_integer_kind)ik_unsigned_long_long;
      goto kind_established;
    } else if (c99_mode &&
#if INT128_EXTENSIONS_ALLOWED
               !int128_extensions_enabled &&
#endif /* INT128_EXTENSIONS_ALLOWED */
               le_max_integer_value_of_kind(
                                     &number, /*is_signed=*/FALSE,
                                     (an_integer_kind)ik_unsigned_long_long)) {
      /* In C99 mode, give the kind of constant described above
         unsigned long long type, with a warning.  Note that if the
         implementation has extended integer types beyond unsigned long long
         this test should be eliminated.  This should be an error in
         strict mode, but both Plum Hall and Perennial have constants
         like this; a warning counts as a "diagnostic" so it's a
         reasonable compromise until the C committee rules on it. */
      conv_line_loc_to_source_pos(start_of_curr_token, &error_position);
      warning(ec_c99_constant_in_unsigned_long_long_range);
      kind = (an_integer_kind)ik_unsigned_long_long;
      goto kind_established;
    }  /* if */
#endif /* LONG_LONG_ALLOWED */
#if INT128_EXTENSIONS_ALLOWED
    if (int128_extensions_enabled) {
      /* 128-bit integers are supported. */
      if (!has_u_suffix &&
          le_max_integer_value_of_kind(&number, /*is_signed=*/FALSE,
                                       (an_integer_kind)ik_int128)) {
        kind = (an_integer_kind)ik_int128;
        goto kind_established;
      } else if (le_max_integer_value_of_kind(
                                       &number, /*is_signed=*/FALSE,
                                       (an_integer_kind)ik_unsigned_int128)) {
        kind = (an_integer_kind)ik_unsigned_int128;
        goto kind_established;
      }  /* if */
    }  /* if */
#endif /* INT128_EXTENSIONS_ALLOWED */
    /* Doesn't fit in target integers.  This can only happen when the
       host representation for integer values can hold values larger
       than the largest target integer. */
    ovflo = TRUE;
#if LONG_LONG_ALLOWED
    kind = (an_integer_kind)ik_long_long;
#else /* !LONG_LONG_ALLOWED */
    kind = (an_integer_kind)ik_long;
#endif /* LONG_LONG_ALLOWED */
kind_established:;
  }  /* if */
  if (ovflo) {
    *err_pos = start_of_curr_token;
    *err_code = ec_integer_too_large;
  } else {
    /* Build a constant with the right type and value. */
    clear_constant(&const_for_curr_token, (a_constant_repr_kind)ck_integer);
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode && microsoft_version == 1200 &&
        isuffix_kind != (an_integer_kind)ik_none) {
      const_for_curr_token.type = microsoft_sized_integer_type(kind);
    } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* Do not insert code here. */
    {
      const_for_curr_token.type = integer_type(kind);
    }  /* if */
    /* For values that might be negative (possible in pcc mode), do
       sign extension. */
    if (do_sign_extension) {
      sign_extend_integer_value(&number,
                                (int)(const_for_curr_token.type->size *
                                                               targ_char_bit));
    }  /* if */
    const_for_curr_token.variant.integer_value = number;
    const_for_curr_token.non_arithmetic        = non_arith;
    /* is_simple_zero is TRUE if the constant is simply "0".  It's useful to
       know that when the constant is used in a virtual function pure specifier
       in C++. */
    const_for_curr_token.is_simple_zero        = (start_of_curr_token ==
                                                  end_of_curr_token &&
                                                  *start_of_curr_token == '0');
  }  /* if */
wrapup:
  if (*err_code != ec_no_error) {
    /* Return an error constant. */
    set_error_constant(&const_for_curr_token);
  }  /* if */
}  /* conv_integer_literal */

#if FIXED_POINT_ALLOWED

void conv_fixed_point_literal(a_boolean      is_hexadecimal,
                              an_error_code  *err_code,
                              a_const_char   **err_pos)
/*
Convert a fixed-point constant from external form to internal form.
start_of_curr_token and end_of_curr_token point to the two ends of the
external form.  is_hexadecimal is TRUE if the external form is specified
as a hexadecimal value.

The internal form is placed in const_for_curr_token.  If there is no
error, *err_code is set to ec_no_error (which is 0); otherwise,
*err_code is set to an appropriate error code and *err_pos is set to
the character position of the error.

This function is modeled after conv_float_literal (see below).
*/
{
  a_fixed_point_type_descr
               fxp_descr;
  a_fixed_point_value
               value;
  a_const_char *actual_end = end_of_curr_token;
  char         old_next_char;
  a_boolean    err;
  a_boolean    inexact = FALSE;

  *err_code = ec_no_error;
  /* Check the suffixes. */
  check_assertion(*actual_end == 'r' || *actual_end == 'R' ||
                  *actual_end == 'k' || *actual_end == 'K');
  if (*actual_end == 'r' || *actual_end == 'R') {
    /* "R" suffix, indicates "_Fract" fixed-point type. */
    fxp_descr.is_fract_type = TRUE;
  } else {
    /* "K" suffix, indicates "_Accum" fixed-point type. */
    fxp_descr.is_fract_type = FALSE;
  }  /* if */
  --actual_end;
  fxp_descr.precision = (a_fixed_point_precision)fpp_default;
  fxp_descr.is_unsigned = FALSE;
  fxp_descr.saturating = FALSE;
  for (;;) {
    if (*actual_end == 'u' || *actual_end == 'U') {
      fxp_descr.is_unsigned = TRUE;
    } else if (*actual_end == 'h' || *actual_end == 'H') {
      fxp_descr.precision = (a_fixed_point_precision)fpp_short;
    } else if (*actual_end == 'l' || *actual_end == 'L') {
      fxp_descr.precision = (a_fixed_point_precision)fpp_long;
    } else {
      /* Not a suffix: This should be the actual end of the number. */
      break;
    }  /* if */
    --actual_end;
  }  /* for */
  /* Place a null after the number to guarantee stopping at the right
     point.  */
  old_next_char = *(actual_end+1);
  *(char *)(actual_end+1) = '\0';
  /* Do the conversion. */
  if (is_hexadecimal) {
    fxp_hex_string_to_fixed_point(&fxp_descr, start_of_curr_token, &value,
                                  &err, &inexact);
  } else {
    fxp_string_to_fixed_point(&fxp_descr, start_of_curr_token, &value, &err);
  }  /* if */
  /* Restore the character that was replaced by a null. */
  *(char *)(actual_end+1) = old_next_char;
  if (err) {
    *err_code = ec_bad_fixed_point_value;
    *err_pos = start_of_curr_token;
  } else {
    /* Build a constant with the right type and value. */
    clear_constant(&const_for_curr_token,
                   (a_constant_repr_kind)ck_fixed_point);
    const_for_curr_token.type = fixed_point_type(fxp_descr);
    const_for_curr_token.variant.fixed_point_value = value;
    if (inexact) {
      /* The hex value could not be exactly represented in the specified
         fixed-point format. */
      a_source_position	pos;
      conv_line_loc_to_source_pos(start_of_curr_token, &pos);
      pos_warning(ec_inexact_fxp_conversion, &pos);
    }  /* if */
  }  /* if */
  if (*err_code != ec_no_error) {
    /* Return an error constant. */
    set_error_constant(&const_for_curr_token);
  }  /* if */
}  /* conv_fixed_point_literal */

#endif /* FIXED_POINT_ALLOWED */

void conv_float_literal(a_boolean	is_hexadecimal,
			an_error_code	*err_code,
                        a_const_char	**err_pos)
/*
Convert a floating constant from external form to internal form.
start_of_curr_token and end_of_curr_token point to the two ends of the
external form.  is_hexadecimal is TRUE if the external form is specified
as a hexadecimal value.

The internal form is placed in const_for_curr_token.  If there is no
error, *err_code is set to ec_no_error (which is 0); otherwise,
*err_code is set to an appropriate error code and *err_pos is set to
the character position of the error.
*/
{
  a_float_kind kind;
  an_internal_float_value
               number;
  a_const_char *actual_end = end_of_curr_token;
  char         old_next_char, old_next2_char;
  a_boolean    err;
  a_boolean    inexact = FALSE;
#if GNU_EXTENSIONS_ALLOWED || C99_IL_EXTENSIONS_SUPPORTED
  a_boolean    is_imaginary_literal = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED || C99_IL_EXTENSIONS_SUPPORTED */

  *err_code = ec_no_error;
  /* See if there is a suffix (or two). */
#if GNU_EXTENSIONS_ALLOWED
  if (*actual_end == 'i' || *actual_end == 'I' ||
      *actual_end == 'j' || *actual_end == 'J') {
    /* GNU accepts imaginary literals like "1.0i", "2.0fj", and "3.0jL".
       So we have to check for a 'i', 'I', 'j', or 'J' both here and
       after a potential "precision suffix" like 'f' or 'L'. */
    is_imaginary_literal = TRUE;
    --actual_end;
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (*actual_end == 'f' || *actual_end == 'F') {
    /* "F" suffix, indicates float type. */
    kind = (a_float_kind)fk_float;
    --actual_end;
  } else if (*actual_end == 'l' || *actual_end == 'L') {
    /* "L" suffix, indicates long double. */
    kind = (a_float_kind)fk_long_double;
    --actual_end;
  } else {
    /* No suffix.  Default is double. */
    kind = (a_float_kind)fk_double;
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (*actual_end == 'i' || *actual_end == 'I' ||
      *actual_end == 'j' || *actual_end == 'J') {
    /* Check for a suffix indicating an imaginary literal appearing after a
       suffix denoting the floating-point precision.  Note that scan_number
       will have diagnosed cases where two "imaginary literal" suffixes
       appeared. */
    is_imaginary_literal = TRUE;
    --actual_end;
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (microsoft_bugs &&
      start_of_curr_token[0] == '.' &&
      isdigit((unsigned char)start_of_curr_token[1]) &&
      start_of_curr_token[2] == '.') {
    /* Microsoft accepts constants like .1.234, and ignores the second
       decimal point and everything after it. */
    actual_end = start_of_curr_token+1;
  }  /* if */
  /* Place a null after the number to guarantee stopping at the right
     point.  If the number has a missing exponent, place a zero exponent
     at the end (this is for the pcc case).  There's always room for at
     least two characters after the floating number, because the number 
     is always followed by at least a newline and null. */
  old_next_char = *(actual_end+1);
  old_next2_char = *(actual_end+2);
  if (*actual_end == 'E' || *actual_end == 'e' ||
      ((*actual_end == '+' || *actual_end == '-') &&
       actual_end != start_of_curr_token &&
       (*(actual_end-1) == 'E' || *(actual_end-1) == 'e'))) {
    /* Missing exponent digits (pcc case); add 0 exponent. */
    *(char *)(actual_end+1) = '0';
    *(char *)(actual_end+2) = '\0';
  } else {
    *(char *)(actual_end+1) = '\0';
  }  /* if */
  /* Do the conversion. */
  if (is_hexadecimal) {
    fp_hex_string_to_float(kind, start_of_curr_token, &number, &err,
                           &inexact);
  } else {
    fp_string_to_float(kind, start_of_curr_token, &number, &err);
  }  /* if */
  *(char *)(actual_end+1) = old_next_char;
  *(char *)(actual_end+2) = old_next2_char;
  if (err) {
    *err_code = ec_bad_float_value;
    *err_pos = start_of_curr_token;
  } else {
    /* Build a constant with the right type and value. */
#if C99_IL_EXTENSIONS_SUPPORTED
    if (is_imaginary_literal) {
      clear_constant(&const_for_curr_token, (a_constant_repr_kind)ck_complex);
      const_for_curr_token.type = complex_type(kind);
      fp_host_large_integer_to_float(
                     kind, (a_host_large_integer)0,
                     &const_for_curr_token.variant.complex_value->real, &err);
      const_for_curr_token.variant.complex_value->imag = number;
    } else
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    /* Do not insert code here. */
    {
      clear_constant(&const_for_curr_token, (a_constant_repr_kind)ck_float);
      const_for_curr_token.type = float_type(kind);
      const_for_curr_token.variant.float_value = number;
    }  /* if */
    if (inexact) {
      /* The hex value could not be exactly represented in the specified
         floating point format. */
      a_source_position	pos;
      conv_line_loc_to_source_pos(start_of_curr_token, &pos);
      pos_warning(ec_inexact_fp_conversion, &pos);
    }  /* if */
  }  /* if */
  if (*err_code != ec_no_error) {
    /* Return an error constant. */
    set_error_constant(&const_for_curr_token);
  }  /* if */
}  /* conv_float_literal */


static unsigned long conv_unicode_literal_char(
                                      a_char_conversion_state_ptr state,
                                      unsigned long               unicode_char,
                                      a_boolean                   utf8_literal)
/*
Convert the Unicode character unicode_char to the appropriate
representation in a literal.  Return the first byte of the converted
character and set up state for scanning through the second and following
bytes (if any).  If utf8_literal is TRUE, the character is part of a UTF-8
string literal and is to be converted to UTF-8 rather than being truncated
to a Latin-1 byte.
*/
{
  int           translated_len;
#if UNICODE_SOURCE_SUPPORTED
  a_boolean     is_unicode_source =
                                   (curr_file_unicode_source_kind != usk_none);
#else /* !UNICODE_SOURCE_SUPPORTED */
  a_boolean     is_unicode_source = FALSE;
#endif /* UNICODE_SOURCE_SUPPORTED */

#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
  if (state->translate_utf8_to_mbc ||
      (!gnu_mode && !is_unicode_source && !utf8_literal)) {
    /* If the emulation (such as Microsoft mode) requires it, we translate
       Unicode characters to the system default multibyte character set.
       Except in GNU mode, we also do that translation if the source is not
       Unicode (so that a universal-character-name will have the same
       encoding as the surrounding native characters). */
    a_boolean err;
    translated_len = unicode_to_multibyte_char(unicode_char,
                                               state->translated_char, &err);
    if (err) {
      /* The code point could not be converted to a suitable
         representation.  Issue a diagnostic. */
      char buf[30];
      (void)sprintf(buf, "%lx", unicode_char);
      conv_line_loc_to_source_pos(*state->next_token_char, &error_position);
      str_warning(ec_bad_unicode_char_in_string, buf);
    }  /* if */
  } else
#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
  /* Do not insert code here. */
  if (gnu_mode || is_unicode_source || utf8_literal) {
    /* Translate the Unicode character into UTF-8. */
    translated_len = unicode_to_utf8(unicode_char, state->translated_char);
  } else {
    /* Assuming that the target character set is Latin-1, which shares the
       first 256 code points with Unicode, we truncate the character to the
       low-order eight bits and issue a warning if the character is not
       Latin-1. */
    translated_len = 1;
    state->translated_char[0] = (unsigned char)unicode_char;
    if (unicode_char > 0xff) {
      conv_line_loc_to_source_pos(*state->next_token_char, &error_position);
      warning(ec_character_not_latin_1);
    }  /* if */
  }  /* if */
  /* Set up for scanning the remaining translated characters and return the
     first. */
  state->remaining_char_count = translated_len - 1;
  state->next_mbc_char = state->translated_char + 1;
  return state->translated_char[0];
}  /* conv_unicode_literal_char */


static unsigned long create_surrogate_pair(unsigned long               ch,
                                           a_char_conversion_state_ptr state)
/*
If ch is a valid Unicode character requiring a surrogate pair in its UTF-16
encoding, set up *state to buffer the second code unit of the pair and
return the first; otherwise, return ch and leave *state unmodified.
*/
{
  unsigned short encoding[2];
  int            num_code_units;

  num_code_units = ucn_to_utf16(ch, encoding);
  if (num_code_units == 2) {
    /* The character was valid Unicode and resulted in a surrogate pair.
       Return the first code unit now and set up to return the second one
       as the next character.  (If the value was invalid, an error was
       already reported when the character was scanned, so we just return
       the original value.) */
    state->pending_surrogate_pair = encoding[1];
    state->next_mbc_char = NULL;
    state->remaining_char_count = 1;
    ch = encoding[0];
  }  /* if */
  return ch;
}  /* create_surrogate_pair */


void conv_single_char(a_char_conversion_state_ptr state,
                      a_boolean                   process_escapes,
                      unsigned long               *ch,
                      unsigned long               centity_mask,
                      a_boolean                   narrow_literal,
                      a_boolean                   utf8_literal)
/*
Fetch one character of a character constant or string literal.  The current
position in the token is *state->next_token_char (it is incremented
appropriately for what is taken).  Escapes (beginning with "\") are
recognized and processed if process_escapes is TRUE.  The character gotten
is returned (not sign-extended) in ch.  centity_mask defines the size of
the character entity into which this character is going (char, wchar_t,
char16_t, or char32_t); narrow_literal is TRUE for narrow-character string
and character literals, and utf8_literal is TRUE for a UTF-8 string
literal.  When multibyte characters are enabled and for
universal-character-names, each byte of the multibyte character is returned
on a separate call of this routine.  state->remaining_char_count is set to
the number of characters remaining to be extracted on subsequent calls, and
serves to disable recognition of escapes, etc., on bytes after the first in
a multibyte character.  The caller must set state->remaining_char_count to
zero before the first call of this routine in a given string, even if
multibyte characters are not enabled.  When
NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE and
state->translate_utf8_to_mbc are TRUE, the bytes returned for a UTF-8
character will be those of the corresponding character in the system
default locale.  When state->create_surrogate_pairs is TRUE and a character
or universal-character-name is encountered that requires a surrogate pair,
the first code unit is returned by this call and the second code unit is
saved in state->pending_surrogate_pair to be returned on the next call.
When processing C++11 raw string literals, state->next_orig_line_modif may
be non-NULL; if it points to a modification for the current position, the
original character(s) are returned instead of the modified version and
state->next_orig_line_modif is advanced to point to the next modification.
*/
{
  unsigned long targ_ch;
  unsigned char tch;
  a_const_char  *lptr;
  int           digit;
  a_boolean     range_error = FALSE;
  a_boolean     numeric_escape = FALSE;
  a_boolean     unrecognized;

  lptr = *state->next_token_char;
  if (state->remaining_char_count != 0) {
    /* We are in the middle of a multibyte character sequence or surrogate
       pair started on a previous call of this routine.  Return another
       character (or the second code unit) and decrement the count of
       remaining characters. */
    if (state->next_mbc_char != NULL) {
      /* The Unicode character that was seen was translated into a
         multibyte character, or a trigraph or line splice was reverted in
         a raw string literal; state->next_mbc_char points to the
         translated or reverted byte to return on this call. */
      targ_ch = (unsigned char)*state->next_mbc_char;
      ++state->next_mbc_char;
    } else if (state->create_surrogate_pairs) {
      /* The previous call returned the first code unit of a surrogate
         pair.  Return the second code unit now. */
      check_assertion(state->remaining_char_count == 1);
      targ_ch = state->pending_surrogate_pair;
    } else {
      targ_ch = (unsigned char)*lptr;
      lptr++;
    }  /* if */
    --state->remaining_char_count;
    goto return_point;
  }  /* if */
get_another:
  targ_ch = (unsigned char)*lptr;
  if (state->next_orig_line_modif != NULL &&
      state->next_orig_line_modif->line_loc == lptr) {
    /* This is a character that must be restored to its original form
       because it appeared in a raw string literal. */
    an_orig_line_modif_ptr olmp = state->next_orig_line_modif;
    state->next_orig_line_modif = olmp->next;
    switch (olmp->kind) {
      case olm_trigraph:
        /* Reconstruct the original trigraph.  The first '?' will be
           returned on this call, while the remaining two characters are
           put into the translated_char array for future calls. */
        targ_ch = '?';
        state->remaining_char_count = 2;
        state->translated_char[0] = '?';
        state->translated_char[1] = olmp->variant.trigraph_orig_char;
        state->next_mbc_char = state->translated_char;
        ++lptr;
        break;
      case olm_line_splice:
        /* A line splice is not represented in the source string
           characters, so we don't increment lptr, but we return the '\'
           now and the newline on the next call. */
        targ_ch = '\\';
        state->remaining_char_count = 1;
        state->translated_char[0] = TARG_NEWLINE_CHAR;
        state->next_mbc_char = state->translated_char;
        break;
      case olm_multiline_string_splice:
        /* scan_multiline_string inserted the two characters '\' and 'n'
           into the source string to represent the newline.  Skip over
           those characters and just return a newline character. */
        targ_ch = TARG_NEWLINE_CHAR;
        lptr += 2;
        break;
      case olm_null:
        /* A null (0) character in the source was replaced by an LE_NULL
           lexical escape.  Skip over it and just return the null, except
           in Microsoft mode, where the character is ignored. */
        lptr += LE_ESCAPE_LEN;
        if (microsoft_mode) {
          goto get_another;
        } else {
          targ_ch = 0;
        }  /* if */
        break;
      default:
        unexpected_condition();
    }  /* switch */
  } else if (targ_ch == LE_ESCAPE) {
    check_assertion(lptr[1] == LE_NULL);
    /* Null (zero) character, represented as an escape. */
    targ_ch = 0;
    lptr += LE_ESCAPE_LEN;
    /* In Microsoft mode, such characters are thrown away. */
    if (microsoft_mode) goto get_another;
  } else if (targ_ch != '\\' || !process_escapes) {
    /* Normal character (not escaped). */
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
    if (multibyte_chars_in_source_enabled) {
      /* Determine the size of the multibyte character sequence that begins at
         the current character.  Since we're returning one character on this
         call, the remaining count is one less than the size. */
      a_boolean     err;
      unsigned long wc;
      state->remaining_char_count = lex_mbc_to_wide_char(lptr, &wc, &err) - 1;
      if (err) {
        /* Invalid multibyte character sequence.  Report the error, skip
           over the invalid sequence, and return a single (null)
           character. */
        conv_line_loc_to_source_pos(lptr, &error_position);
        diagnostic(es_discretionary_error, ec_bad_multibyte_char);
        lptr += state->remaining_char_count;
        state->remaining_char_count = 0;
        targ_ch = 0;
#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
      } else if (curr_file_unicode_source_kind != usk_none &&
                 state->translate_utf8_to_mbc) {
        /* The UTF-8 character in the token must be translated to the system
           default multibyte character set.  Get the wide character Unicode
           character corresponding to the UTF-8 bytes and pass it to
           conv_unicode_literal_char, which will convert the Unicode
           to the appropriate character set and set up the conversion state
           to return subsequent bytes of the multibyte character. */
        (void)mbc_to_wide_char(lptr, &wc, (a_boolean *)NULL,
                               /*is_native=*/FALSE);
        lptr += state->remaining_char_count;
        targ_ch = conv_unicode_literal_char(state, uc, /*utf8_literal=*/FALSE);
#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
      } else if (utf8_literal) {
        /* This is a character in a UTF-8 literal, which could be a
           multibyte character, either UTF-8 or a native character set:
           convert it to Unicode and then to UTF-8, returning the first (or
           only) byte. */
        lptr += state->remaining_char_count;
        targ_ch = conv_unicode_literal_char(state, wc, /*utf8_literal=*/TRUE);
      }  /* if */
    } else
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
    /* Do not insert code here. */
    if (utf8_literal) {
      /* This is a Latin-1 character (one byte) in a UTF-8 literal.
         Convert it to UTF-8 and return the first (or only) byte. */
      targ_ch = conv_unicode_literal_char(state, targ_ch,
                                          /*utf8_literal=*/TRUE);
    }  /* if */
    lptr++;
  } else {
    /* Backslash, escaped character.  Can be an octal escape, a hexadecimal
       escape, a simple escape sequence (like \n), a
       universal-character-name, or something unrecognized, in which case
       the character is left alone. */
    unrecognized = FALSE;
    lptr++;
    switch ((int)(tch = (unsigned char)*(lptr++))) {
      case 'a':
        if (C_dialect == C_dialect_pcc) {
          /* pcc does not recognize \a. */
          unrecognized = TRUE;
        } else {
          targ_ch = (unsigned char)TARG_ALERT_CHAR;
        }  /* if */
        break;
      case 'b':
        targ_ch = (unsigned char)TARG_BACKSPACE_CHAR;
        break;
#if GNU_EXTENSIONS_ALLOWED
      case 'e':
      case 'E':
        if (gnu_mode) {
          /* GNU mode \e or \E stands for the ASCII "ESC" character. */
          targ_ch = (unsigned char)TARG_ESC_CHAR;
        } else {
          /* Most modes do not recognize this escape sequence. */
          unrecognized = TRUE;
        }  /* if */
        break;
#endif /* GNU_EXTENSIONS_ALLOWED */
      case 'f':
        targ_ch = (unsigned char)TARG_FORM_FEED_CHAR;
        break;
      case 'n':
        targ_ch = (unsigned char)TARG_NEWLINE_CHAR;
        break;
      case 'r':
        targ_ch = (unsigned char)TARG_CARR_RETURN_CHAR;
        break;
      case 't':
        targ_ch = (unsigned char)TARG_HORIZ_TAB_CHAR;
        break;
      case 'v':
        /* \v is not in K&R, but is recognized by pcc. */
        targ_ch = (unsigned char)TARG_VERT_TAB_CHAR;
        break;
      case 'u':
      case 'U':
        /* A universal character name.  Back the pointer up to the position
           of the backslash. */
        /* Universal characters are allowed in C++ and C99. */
        if (!universal_character_names_allowed) goto other_chars;
        lptr -= 2;
        targ_ch = scan_universal_character(&lptr,
                                           /*is_identifier=*/FALSE,
					   /*is_identifier_start=*/FALSE,
                                           /*issue_diagnostics=*/TRUE);
        if (!narrow_literal) {
          /* This is for a wide character or wide string literal.  Return
             the value directly, subject to the range constraints implied
             by centity_mask. */
          goto range_check;
        } else {
          /* Convert the Unicode character specified by the
             universal-character-name to either UTF-8 or the system default
             multibyte character set as appropriate and set up the
             conversion state to return subsequent bytes of the resulting
             character. */
          targ_ch = conv_unicode_literal_char(state, targ_ch, utf8_literal);
        }  /* if */
        break;
      case 'x':
        /* Hexadecimal escape.  There can be many digits, but there must be
           at least one.  If not, treat as just "x". */
        if (!isxdigit((unsigned char)*lptr)) {
          conv_line_loc_to_source_pos(*state->next_token_char+2,
                                      &error_position);
          if (C_dialect == C_dialect_pcc || SVR4_C_mode) {
            warning(ec_bad_hex_digit);
          } else {
            error(ec_bad_hex_digit);
          }  /* if */
          targ_ch = (unsigned char)'x';
        } else {
          numeric_escape = TRUE;
          targ_ch = hexvalue(*lptr);  /* First digit. */
          while (tch = *(++lptr), isxdigit(tch)) {
            if (targ_ch > (((unsigned long)LONG_MAX)>>4)) {
              /* Error will be processed below.  We must keep going and take
                 all the digits. */
              range_error = TRUE;
            }  /* if */
            digit = hexvalue(tch);
            targ_ch = (targ_ch << 4) | digit;
          }  /* while */
          goto range_check;
        }  /* if */
        break;
      case '0':  case '1':  case '2':  case '3':
      case '4':  case '5':  case '6':  case '7':
        /* Octal escape.  Note that neither ANSI nor pcc recognizes 8 and 9
           as "octal" in this context.  Up to three octal digits may appear.
           Note that there is code in accum_quoted_string that must match
           this code. */
        numeric_escape = TRUE;
        targ_ch = tch - '0';  /* First digit. */
        tch = (unsigned char)*lptr;
        if (isdigit(tch) && tch != '8' && tch != '9') {
          /* Second digit. */
          targ_ch = (targ_ch << 3) | (tch - '0');
          lptr++;
          tch = (unsigned char)*lptr;
          if (isdigit(tch) && tch != '8' && tch != '9') {
            /* Third digit. */
            lptr++;
            targ_ch = (targ_ch << 3) | (tch - '0');
          }  /* if */
        }  /* if */
        goto range_check;
      default:
other_chars:
        /* Other characters, left alone.  Specifically, standard requires
           that \', \", \?, and \\ be reduced to just the escaped character. */
        if (tch == '\'' || tch == '"' || tch == '?' || tch == '\\') {
          targ_ch = tch;
        } else {
          unrecognized = TRUE;
        }  /* if */
        break;
    }  /* switch */
    /* Unrecognized escapes cause a warning but translate to the escaped
       character. */
    if (unrecognized) {
      conv_line_loc_to_source_pos(*state->next_token_char, &error_position);
      warning(ec_unrecognized_char_escape);
      targ_ch = tch;
    }  /* if */
  }  /* if */
return_point:
  /* Drop out-of-range bits. */
  targ_ch &= centity_mask;
  *ch = targ_ch;
  if (microsoft_mode) {
    /* Throw away any null characters following the character in Microsoft
       mode.  This makes it easier for the caller to recognize the end of the
       string. */
    while (*lptr == LE_ESCAPE && lptr[1] == LE_NULL) lptr += 2;
  }  /* if */
  *state->next_token_char = lptr;
  return;

range_check:
  /* Check that the value of c is legal for a character.  The standard
     (3.1.3.4) requires that this be diagnosed.  Range is different for
     wide characters. */
  if (!range_error) {
    /* The comparison here is always done as unsigned, even if char or
       wchar_t are signed.  That's because octal and hexadecimal escapes
       are always treated as unsigned.  See 3.1.3.4 constraints. */
    /* Use masking for the check so that this will work when the target
       char is larger than the host.  In that case, with the current limited
       implementation, there can be "holes" in the middle of wide character
       constants, and those holes shouldn't contain any "1" bits. */
    if ((targ_ch & ~centity_mask) != 0) {
      if (state->create_surrogate_pairs && !numeric_escape) {
        /* The target character type is such that an overflow should be
           handled by creating a UTF-16 surrogate pair rather than as a
           warning or error. */
        targ_ch = create_surrogate_pair(targ_ch, state);
      } else {
        range_error = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (range_error) {
    /* A range error occurring in an octal or hexadecimal escape is
       classified as an error by the C Standard.  Other contexts, and in
       all cases in C++, produce implementation-defined behavior.  We thus
       issue a strict-ANSI diagnostic for numeric escapes in C and a
       warning in all other cases. */
    conv_line_loc_to_source_pos(*state->next_token_char, &error_position);
    if (C_mode() && strict_ansi_mode && numeric_escape) {
      diagnostic(strict_ansi_error_severity, ec_bad_character_value);
    } else {
      warning(ec_bad_character_value);
    }  /* if */
    /* Value is truncated by the normal return processing. */
  }  /* if */
  goto return_point;
}  /* conv_single_char */


static void conv_single_wide_char(a_char_conversion_state_ptr state,
                                  a_boolean                   process_escapes,
                                  unsigned long               *ch,
                                  unsigned long               centity_mask)
/*
Fetch one wide character of a wide character constant or string literal
(here, a "wide character" can be a wchar_t, a char16_t, or a char32_t).
The current position in the token is *state->next_token_char (it is
incremented appropriately for what is taken).  More than one source
character may be taken to produce one wide character as output.  The wide
character gotten is returned (not sign-extended) in ch.  centity_mask
defines the size of character.
*/
{
#if !MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
  /* Simple version: no multibyte characters to consider. */
  conv_single_char(state, process_escapes, ch, centity_mask,
                   /*narrow_literal=*/FALSE, /*utf8_literal=*/FALSE);
#else /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
  /* Multibyte character processing may be needed. */
  if (!multibyte_chars_in_source_enabled ||
      (process_escapes && **state->next_token_char == '\\') ||
      **state->next_token_char == LE_ESCAPE ||
      state->remaining_char_count > 0 ||
      (state->next_orig_line_modif != NULL &&
       state->next_orig_line_modif->line_loc == *state->next_token_char)) {
    /* Use simple routine if multibyte characters are disabled or if
       the character is an escape or the result of a modification. */
    conv_single_char(state, process_escapes, ch, centity_mask,
                     /*narrow_literal=*/FALSE, /*utf8_literal=*/FALSE);
  } else {
    unsigned  long wc;
    int       numch;
    a_boolean err;
    /* Convert a multibyte character sequence to a wide character. */
    numch = lex_mbc_to_wide_char(*state->next_token_char, &wc, &err);
    if (err) {
      /* Invalid multibyte character sequence. */
      conv_line_loc_to_source_pos(*state->next_token_char, &error_position);
      diagnostic(es_discretionary_error, ec_bad_multibyte_char);
      wc = 0;
    }  /* if */
    if ((wc & ~centity_mask) != 0 && state->create_surrogate_pairs) {
      /* The character does not fit into a single code unit.  Create a
         UTF-16 surrogate pair, buffering the second code unit in *state
         and returning the first as the result of this call. */
      wc = create_surrogate_pair(wc, state);
    }  /* if */
    *ch = wc;
    *state->next_token_char += numch;
  }  /* if */
#endif /* !MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
}  /* conv_single_wide_char */


void conv_char_literal(unsigned long num_chars,
                       an_error_code *err_code,
                       a_const_char  **err_pos)
/*
Convert a character constant from external form to internal form.
start_of_curr_token and end_of_curr_token point to the two ends of the
external form.  The internal form is placed in const_for_curr_token.  If
there is no error, *err_code is set to ec_no_error (which is 0); otherwise,
*err_code is set to an appropriate error code and *err_pos is set to the
character position of the error.  num_chars indicates the number of
characters contained within the quotes (after escape processing, and in
wide characters if the constant is wide).  If the literal contains a UCN,
the actual number of converted characters may be less than num_chars.  */
{
  unsigned long           i, ch;
  an_integer_value        number, ch_int_val;
  a_const_char            *temp_ptr;
  a_boolean               err, too_many_chars = FALSE, bad_character = FALSE;
  a_type_ptr              con_type;
  unsigned int            char_size;
  unsigned long           centity_mask;
  a_boolean               centity_is_signed;
  int                     centity_bits;
  int                     encoding_length;
  a_character_kind        character_kind;
  a_char_conversion_state conv_state;

  /* Determine the constant type as follows:
       Single-character constant     ('x'): int in C, char in C++
       Multi-character constant     ('xy'): int
       Wide character constant      (L'x'): wchar_t
       char16_t character constant  (u'x'): char16_t
       char32_t character constant  (U'x'): char32_t
     Multi-character wide-character literals don't really make sense, but we
     do allow them (with a warning) for wchar_t literals because the C
     standard says it is implementation-defined, and several test suites have
     something like L'ab' in them.
  */
  switch (*start_of_curr_token) {
    case '\'':
      /* Normal character literal (single or multi). */
      character_kind = (a_character_kind)chk_char;
      char_size = 1;
      centity_bits = targ_char_bit;
      centity_is_signed = targ_has_signed_chars; 
      temp_ptr = start_of_curr_token+1;
      if (C_mode() || num_chars > 1) {
        /* Character constants in C have type int, as do multi-character
           literals in C++.  In the case of a C++ literal containing a UCN
           that translates to a single character, the type will be adjusted
           to char below after the value is known. */
        con_type = integer_type((an_integer_kind)ik_int);
      } else {
        /* A single-character constant in C++. */
        con_type = integer_type((an_integer_kind)ik_char);
      }  /* if */
      break;
    case 'L':
      /* Wide character literal. */
      character_kind = (a_character_kind)chk_wchar_t;
      char_size = (unsigned int)targ_sizeof_wchar_t;
      centity_bits = char_size*targ_char_bit;
      centity_is_signed = int_kind_is_signed[(int)targ_wchar_t_int_kind];
      con_type = eff_wchar_t_type();
      temp_ptr = start_of_curr_token+2;
      break;
    case 'U':
      /* char32_t character literal. */
      character_kind = (a_character_kind)chk_char32_t;
      char_size = (unsigned int)targ_sizeof_char32_t;
      centity_bits = char_size*targ_char_bit;
      centity_is_signed = FALSE; 
      con_type = eff_char32_t_type();
      temp_ptr = start_of_curr_token+2;
      break;
    case 'u':
      /* char16_t character literal. */
      character_kind = (a_character_kind)chk_char16_t;
      char_size = (unsigned int)targ_sizeof_char16_t;
      /* Do not use a mask for char16_t characters at this time.  Any masking
         operation is the responsibility of the encoding (invoked through the
         encode_in_char16_t macro). */
      centity_bits = sizeof(unsigned long)*CHAR_BIT;
      centity_is_signed = FALSE; 
      con_type = eff_char16_t_type();
      temp_ptr = start_of_curr_token+2;
      break;
    default:
      unexpected_condition();
  }  /* switch */
  centity_mask = (unsigned long)1 << (centity_bits-1);
  centity_mask = centity_mask | (centity_mask - 1);
  /* UTF-8 characters should be translated to multibyte characters only
     for narrow-character literals in Microsoft mode. */
  clear_char_conversion_state(&conv_state, &temp_ptr,
                              (character_kind == (a_character_kind)chk_char &&
                               microsoft_mode));
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
  /* Initialize for scanning multibyte characters in the string. */
  mbc_scan_init_if_multibyte_chars_in_source_enabled();
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
  set_unsigned_integer_value(&number, (a_host_large_unsigned)0);
  /* Accumulate the characters.  A wide literal with no characters (L'')
     is possible in Microsoft mode and must produce a zero value. */
  for (i = 0;
       temp_ptr < end_of_curr_token || conv_state.remaining_char_count > 0;
       ++i)  /*lint !e440*/ {
    /* Convert one character of the char constant. */
    switch (character_kind) {
      case chk_char:
        conv_single_char(&conv_state, /*process_escapes=*/TRUE, &ch,
                         centity_mask, /*narrow_literal=*/TRUE,
                         /*utf8_literal=*/FALSE);
        if (i >= targ_sizeof_int && !gnu_mode) {
          /* GNU compilers accept overlong literals, simply discarding any
             leading characters that do not fit.  Otherwise, flag this as
             an error. */
          too_many_chars = TRUE;
        }  /* if */
        break;
      case chk_wchar_t:
        conv_single_wide_char(&conv_state, /*process_escapes=*/TRUE, &ch,
                              centity_mask);
        /* The value of a multi-character L'...' literal is truncated to
           the first character. */
        if (i != 0) continue;
        break;
      case chk_char16_t:
        conv_single_wide_char(&conv_state, /*process_escapes=*/TRUE, &ch,
                              centity_mask);
        if (i != 0 && !C_mode()) {
          too_many_chars = TRUE;
        } else {
          unsigned short char16_t_vals[MAX_CHAR16_T_ENCODING_LENGTH];
          encoding_length = encode_in_char16_t(ch, char16_t_vals);
          if (encoding_length == 1) {
            /* Normal case. */
            ch = (unsigned long)char16_t_vals[0];
          } else {
            /* ch contained a character code that cannot be encoded in a
               single char16_t character. */
            bad_character = TRUE;
          }  /* if */
        }  /* if */
        break;
      case chk_char32_t:
        conv_single_wide_char(&conv_state, /*process_escapes=*/TRUE, &ch,
                              centity_mask);
        if (i != 0 && !C_mode()) {
          too_many_chars = TRUE;
        }  /* if */
        break;
      default:
        unexpected_condition();
    }  /* switch */
    if (i != 0 && (character_kind == (a_character_kind)chk_char16_t ||
                   character_kind == (a_character_kind)chk_char32_t)) {
      /* Ignore any preceding characters and just take the last one. */
      set_unsigned_integer_value(&number, (a_host_large_unsigned)0);
    }  /* if */
    /* Put the character in the right place. */
    set_unsigned_integer_value(&ch_int_val, (a_host_large_unsigned)ch);
    if (targ_char_constant_first_char_most_significant) {
      /* 'ab' == 0x6162. */
      /* Do sign extension if necessary, but only on the first character. */
      if (i == 0 && centity_is_signed) {
        sign_extend_integer_value(&ch_int_val, centity_bits);
      }  /* if */
      shift_left_integer_value(&number, centity_bits, &err);
    } else {
      /* 'ab' == 0x6261. */
      /* Do sign extension on the new character if necessary. */
      if (centity_is_signed) {
        sign_extend_integer_value(&ch_int_val, centity_bits);
      }  /* if */
      if (i != 0) {
        /* Drop any sign extension on the previous value if this isn't the
           first character. */
        if (centity_is_signed) {
          an_integer_value mask;
          make_integer_value_mask(&mask, (int)i*centity_bits);
          and_integer_values(&number, &mask);
        }  /* if */
        shift_left_integer_value(&ch_int_val, (int)i*centity_bits, &err);
      } /* if */
    } /* if */
    or_integer_values(&number, &ch_int_val);
  }  /* for */
  if (character_kind != (a_character_kind)chk_char32_t &&
      num_chars > 1 && i == 1) {
    /* A universal-character-name might potentially represent a number of
       bytes, so num_chars was set conservatively to allow for that case.
       If it actually turned out to represent a single character, update
       the character count and constant type accordingly. */
    if (character_kind == (a_character_kind)chk_char && !C_mode()) {
      con_type = integer_type((an_integer_kind)ik_char);
    }  /* if */
    num_chars = 1;
  }  /* if */
  if (bad_character) {
    *err_code = ec_no_char16_t_representation;
    *err_pos = start_of_curr_token + 2;
    /* Return an error constant. */
    set_error_constant(&const_for_curr_token);
  } else if (too_many_chars) {
    *err_code = ec_too_many_characters;
    *err_pos = start_of_curr_token;
    /* Return an error constant. */
    set_error_constant(&const_for_curr_token);
  } else {
    *err_code = ec_no_error;
    *err_pos = NULL;
    if (num_chars > 1) {
      /* A character literal with more than one character produces an
         implementation-defined value.  Issue a warning.  The "too many
         characters" message is used for wide characters as this is
         unlikely to produce a meaningful result. */
      an_error_code  wcode = (character_kind != (a_character_kind)chk_char) ?
                               ec_too_many_characters : ec_multi_char_literal;
      if (gnu_mode && i > targ_sizeof_int) {
        /* Truncate the value and warn about discarded characters. */
        an_integer_value int_mask;
        make_integer_value_mask(&int_mask,
                                (int)(targ_sizeof_int * targ_char_bit));
        and_integer_values(&number, &int_mask);
        wcode = ec_leading_character_ignored_in_char_literal;
      }  /* if */
      conv_line_loc_to_source_pos(start_of_curr_token, &error_position);
      warning(wcode);
    }  /* if */
    clear_constant(&const_for_curr_token, (a_constant_repr_kind)ck_integer);
    const_for_curr_token.type = con_type;
    const_for_curr_token.variant.integer_value = number;
    const_for_curr_token.character_kind = character_kind;
  }  /* if */
}  /* conv_char_literal */


static void put_wide_char_into_string(unsigned long  ch,
                                      char           **pstr,
                                      unsigned int   char_size)
/*
Put the wide character (wchar_t, char16_t, or char32_t) ch into the string
pointed to by *pstr, and increment *pstr by the proper amount.  char_size
specifies the number of bytes in a wide character.
*/
{
  unsigned int  i;
  char          *p = *pstr;

  /* This is basically a copy of an integer to an array of characters;
     we must allow for the target endian-ness. */
  if (targ_little_endian) {
    for (i = 0; i < char_size; i++) {
      *p++ = (char)(ch & UCHAR_MAX);
      ch >>= targ_char_bit;
    }  /* for */
  } else {
    for (i = 0; i < char_size; i++) {
      *p++ = (char)((ch >> ((char_size - i - 1) * targ_char_bit)) & UCHAR_MAX);
    }  /* for */
  }  /* if */
  *pstr = p;
}  /* put_wide_char_into_string */


void conv_string_literal(a_const_char                  *start_of_string_value,
                         a_const_char                  *end_of_string_value,
                         a_string_or_char_literal_kind lit_kind,
                         unsigned long                 num_chars,
                         an_error_code                 *err_code,
                         a_const_char                  **err_pos)
/*
Convert a string literal from external form to internal form.
start_of_string_value and end_of_string_value point to the first character
of the value and to the terminating character of the external form (i.e.,
following the opening quote and to the closing quote in an ordinary string
literal, or following the '(' and to the ')' in a raw string literal), and
lit_kind describes the kind of literal.  The internal form is placed in
const_for_curr_token.  If there is no error, *err_code is set to
ec_no_error (which is 0); otherwise, *err_code is set to an appropriate
error code and *err_pos is set to the character position of the error.
num_chars indicates the number of characters contained within the quotes
(after escape processing, and in wide characters if the string is wide).
If the string is a char16_t string of the form u"...", num_chars may be
larger (but not smaller) than the number of characters needed to represent
the string.
*/
{
  unsigned long                 i, ch, centity_mask;
  a_const_char                  *temp_ptr;
  char                          *pstr;
  char                          *str_start;
  sizeof_t                      constant_size;
  a_targ_size_t                 num_elems;
  unsigned int                  char_size;
  a_character_kind              character_kind;
  a_char_conversion_state       conv_state;
  a_boolean                     raw_string_end_in_trigraph = FALSE;
  a_string_or_char_literal_kind prefix_kind =
                                             literal_encoding_prefix(lit_kind);

  /* The number of array elements is one more than the number of characters,
     to leave space for the terminating null.  (For char16_t strings, this
     may need to be adjusted below.) */
  num_elems = (a_targ_size_t)num_chars + 1;
  temp_ptr = start_of_string_value;
  /* Set the character kind and size. */
  check_assertion(lit_kind & SCLK_STRING_LITERAL);
  switch (prefix_kind) {
    case SCLK_ORDINARY_LITERAL:
    case SCLK_UTF8_LITERAL:
      character_kind = (a_character_kind)chk_char;
      char_size = 1;
      break;
    case SCLK_WIDE_LITERAL:
      character_kind = (a_character_kind)chk_wchar_t;
      char_size = (unsigned int)targ_sizeof_wchar_t;
      break;
    case SCLK_CHAR32_T_LITERAL:
      character_kind = (a_character_kind)chk_char32_t;
      char_size = (unsigned int)targ_sizeof_char32_t;
      break;
    case SCLK_CHAR16_T_LITERAL:
      character_kind = (a_character_kind)chk_char16_t;
      char_size = (unsigned int)targ_sizeof_char16_t;
      break;
    default:
      unexpected_condition();
  }  /* switch */
  /* Build a mask used to mask individual characters. */
  centity_mask = (unsigned long)1 << (targ_host_string_char_bit-1);
  centity_mask = centity_mask | (centity_mask-1);
  constant_size = (sizeof_t)num_elems;
  if (char_size != 1) {
    constant_size *= char_size;
    /* Replicate the mask for one character as many times as there are chars
       in the wide character.  This "inefficient" method is used because it
       works right even when the target character is larger than the host
       character.  In that case, there are "holes" in the bit pattern where a
       "1" bit cannot be represented. */
    for (i = 1; i < char_size; ++i) {
      centity_mask |= (centity_mask << targ_char_bit);
    }  /* for */
#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
  } else if (curr_file_unicode_source_kind != usk_none && microsoft_mode) {
    /* UTF-8 characters will be translated to multibyte characters, so
       there is no fixed correspondence between the number of bytes in the
       token and the number of bytes in the constant.  In the worst case,
       each single-byte Unicode character could require
       MAX_MULTIBYTE_CHAR_LENGTH bytes in the translated character, so we
       assume that constant size to be safe.  (The actual length of the
       constant will be calculated below based on the number of bytes in
       the translated string.) */
    constant_size *= MAX_MULTIBYTE_CHAR_LENGTH;
#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
  }  /* if */
  /* Allocate enough space to hold the final string, including the null
     added to it.  (This may be more than strictly needed in the case of
     char16_t strings or when translating a string in a Unicode-encoded
     file to native multibyte characters.) */
  str_start = pstr = alloc_text_of_string_literal(constant_size);
  /* UTF-8 characters should be translated to multibyte characters only
     for narrow-character literals in Microsoft mode. */
  clear_char_conversion_state(&conv_state, &temp_ptr,
                              (prefix_kind == SCLK_ORDINARY_LITERAL &&
                               microsoft_mode));
  conv_state.create_surrogate_pairs = (prefix_kind == SCLK_WIDE_LITERAL ||
                                       prefix_kind == SCLK_CHAR16_T_LITERAL);
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
  /* Initialize for scanning multibyte characters in the string. */
  mbc_scan_init_if_multibyte_chars_in_source_enabled();
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
  if (lit_kind & SCLK_RAW_STRING_LITERAL) {
    /* Set up to reverse any original line modifications (trigraphs, line
       splices) that appear in the raw string. */
    for (conv_state.next_orig_line_modif = orig_line_modif_list;
         conv_state.next_orig_line_modif != NULL &&
                          conv_state.next_orig_line_modif->line_loc < temp_ptr;
         conv_state.next_orig_line_modif =
                                      conv_state.next_orig_line_modif->next) {}
    if (*end_of_string_value == ']') {
      /* This is the pathological case in which the two characters
         preceding the terminating ')' of the raw string literal were both
         '?' characters, which was interpreted as a trigraph for ']'.  Set
         up the loop control accordingly. */
      raw_string_end_in_trigraph = TRUE;
    }  /* if */
  }  /* if */
  /* Accumulate the characters.  Loop until we reach the indicated end of
     the string value.  The loop is extended while characters are pending,
     either because a multibyte character is in process, or because of the
     pathological ']' trigraph case mentioned above, or because a raw
     string literal ended with a line splice that must be expanded. */
  while (temp_ptr < end_of_string_value + raw_string_end_in_trigraph ||
         conv_state.remaining_char_count > raw_string_end_in_trigraph ||
         (conv_state.next_orig_line_modif != NULL &&
          conv_state.next_orig_line_modif->kind == olm_line_splice &&
          conv_state.next_orig_line_modif->line_loc == temp_ptr)) {
    check_assertion(pstr < str_start + constant_size);
    /* Convert one character of the string literal. */
    switch (character_kind) {
      case chk_char:
        conv_single_char(
                 &conv_state,
                 /*process_escapes=*/(lit_kind & SCLK_RAW_STRING_LITERAL) == 0,
                 &ch, centity_mask, /*narrow_literal=*/TRUE,
                 (prefix_kind == SCLK_UTF8_LITERAL));
        *pstr++ = (char)ch;
        break;
      case chk_wchar_t:
      case chk_char16_t:
      case chk_char32_t:
        conv_single_wide_char(
                 &conv_state,
                 /*process_escapes=*/(lit_kind & SCLK_RAW_STRING_LITERAL) == 0,
                 &ch, centity_mask);
        put_wide_char_into_string(ch, &pstr, char_size);
        break;
      default:
        unexpected_condition();
    }  /* switch */
  }  /* for */
  /* Add the final null. */
  check_assertion(pstr < str_start + constant_size);
  switch (character_kind) {
    case chk_char:
      /* Normal string literal. */
      *(pstr++) = '\0';
      /* The actual length of the string may be less than was originally
         calculated due to translation of universal-character-names into
         UTF-8 or, if enabled, translation of Unicode characters to
         multibyte characters. */
      constant_size = pstr - str_start;
      num_elems = (a_targ_size_t)constant_size;
      break;
    case chk_char16_t:
    case chk_wchar_t:
      /* The allocated number of bytes may be too large due to a conservative
         estimate for encoding length.  Update the size and character count to
         reflect the actual encoding. */
      constant_size = (pstr - str_start) + char_size;
      num_elems = (a_targ_size_t)(constant_size / char_size);
      /*FALLTHROUGH*/
    case chk_char32_t:
      /* L"...", u"...", or U"...": */
      ch = 0;
      put_wide_char_into_string(ch, &pstr, char_size);
      break;
    default:
      unexpected_condition();
  }  /* switch */
#if CHECKING
  /* Check that the length calculation was correct. */
  check_assertion_str((sizeof_t)(pstr - str_start) == constant_size,
                      "conv_string_literal: length miscalculated");
#endif /* CHECKING */
  /* Make the constant entry for the string. */
  clear_constant(&const_for_curr_token, (a_constant_repr_kind)ck_string);
  const_for_curr_token.type = string_literal_type(character_kind, num_elems);
  const_for_curr_token.variant.string.length = (a_targ_size_t)constant_size;
  const_for_curr_token.variant.string.value  = str_start;
  const_for_curr_token.character_kind = character_kind;
  /* Currently, no error is returned through err_code or err_pos. */
  *err_code = ec_no_error;
  *err_pos = NULL;  /* To make lint happy. */
}  /* conv_string_literal */


static void widening_copy(a_const_char      *src,
                          char              *dst,
                          a_targ_size_t     len,
                          a_character_kind  kind)
/*
Copy an ordinary character string of length len pointed to by src to a
character string pointed to by dst.  dst already points to storage that is
sufficient to hold len characters of the indicated kind.
*/
{
  a_targ_size_t  k = 0;
  unsigned int   char_size = (unsigned int)character_size[kind];

  for (k = 0; k < len; ++k) {
    unsigned long ch = (unsigned long)(unsigned char)src[k];
    put_wide_char_into_string(ch, &dst, char_size);
  }  /* for */
}  /* widening_copy */


void widen_string_literal(a_constant_ptr con)
/*
Change the indicated narrow string literal into a wide string (wchar_t)
literal.
*/
{
  a_targ_size_t  narrow_str_len = con->variant.string.length;
  a_const_char   *narrow_str = con->variant.string.value;

  clear_constant(con, (a_constant_repr_kind)ck_string);
  con->type = string_literal_type((a_character_kind)chk_wchar_t,
                                  narrow_str_len);
  con->character_kind = (a_character_kind)chk_wchar_t;
  con->variant.string.length = narrow_str_len * targ_sizeof_wchar_t;
  con->variant.string.value = alloc_text_of_string_literal(
                                        (sizeof_t)con->variant.string.length);
  widening_copy(narrow_str, (char *)con->variant.string.value,
                narrow_str_len, (a_character_kind)chk_wchar_t);
}  /* widen_string_literal */


void concat_string_literals(a_token_cache_ptr cache,
                            a_character_kind  character_kind)
/*
Concatenate two or more string literals contained in the indicated token
cache, and replace the constant in the first cached string token with the
constant for the concatenation.  (The rest of the cached tokens are left as
they are; the caller removes and frees them.)  The result string will have
characters of the given kind.  Some of the constants may be error constants if
there were malformed string literals in the input; in that case, the output is
an error constant.  Some of the entries in the token cache may be for pragmas;
they are ignored.  This routine implements the lexical concatenation of
section 2.1.1.2, phase 6, of the C standard.  The nulls from the initial
strings are discarded in doing the concatenation, and the one from the last
string is copied as the final null of the concatenated string; see ANSI C
3.1.4.  The cached strings either all have the given character kind, or a
mix of the given kind and chk_char.
*/
{
  a_targ_size_t      total_len = 0, str_len, null_len;
  a_cached_token_ptr ctp, first_string_token = NULL;
  a_boolean          produce_error_constant = FALSE;
  a_constant_ptr     concat_con, con;
  char               *new_str;

  db_enter(4, "concat_string_literals");
  /* Determine the length of the terminating null on strings.  It's usually
     1, but it may be bigger for wide string literals. */
  null_len = character_size[character_kind];
  /* Determine the length of the concatenation. */
  for (ctp = cache->first_token; ctp != NULL; ctp = ctp->next) {
    /* Ignore pragma entries. */
    if (ctp->extra_info_kind == (a_token_extra_info_kind)teik_pragma) continue;
    check_assertion_str((a_token_kind)ctp->token == tok_string_literal &&
                        ctp->extra_info_kind ==
                                        (a_token_extra_info_kind)teik_constant,
                       "concat_string_literals: cached token is not a string");
    if (first_string_token == NULL) first_string_token = ctp;
    con = ctp->variant.constant;
    if (is_error_constant(con)) {
      /* If any constant is an error constant, the overall concatenation
         will be an error constant. */
      produce_error_constant = TRUE;
      break;
    } else {
      /* String constant. */
      check_assertion_str(con->kind == (a_constant_repr_kind)ck_string,
                          "concat_string_literals: constant not ck_string");
      /* Determine the length of this string literal. */
      str_len = con->variant.string.length;
      if (con->character_kind != character_kind) {
        if (con->character_kind != (a_character_kind)chk_char) {
          /* An attempt to concatenate two different string kinds, neither of
             which is a plain (narrow) string.  This is an error. */
          produce_error_constant = TRUE;
        } else {
          /* This string will need widening. */
          str_len *= null_len;
        }  /* if */
      }  /* if */
      /* Except on the last constant, subtract out the space for the
         final null in the string. */
      if (ctp->next != NULL) str_len -= null_len;
      /* Add the length of this string to the accumulated length. */
      total_len += str_len;
    }  /* if */
  }  /* for */
  /* Here, we either have the length of the concatenation in total_len, or
     produce_error_constant is set. */
  /* Build the concatenation and record it in the constant in the first
     string token in the cache. */
  check_assertion(first_string_token != NULL); /* For Coverity. */
  concat_con = first_string_token->variant.constant;
  if (produce_error_constant) {
    /* There is at least one error constant in the concatenation or the
       strings were of incompatible kinds (e.g., L"a" U"b"), so return
       an error constant. */
    set_error_constant(concat_con);
  } else {
    /* No error constants, so do the concatenation. */
    /* Allocate enough space for the concatenation. */
    new_str = alloc_text_of_string_literal((sizeof_t)total_len);
    total_len = 0;
    /* Copy the constants into the concatenation. */
    for (ctp = first_string_token; ctp != NULL; ctp = ctp->next) {
      /* Ignore pragma entries. */
      if (ctp->extra_info_kind == (a_token_extra_info_kind)teik_pragma) {
        continue;
      }  /* if */
      con = ctp->variant.constant;
      /* Determine the length of this string literal. */
      str_len = con->variant.string.length;
      /* Except on the last constant, subtract out the space for the
         final null in the string. */
      if (ctp->next != NULL) str_len -= character_size[con->character_kind];
      /* Copy the string text (including the final null, if that's
         appropriate). */
      if (con->character_kind != character_kind) {
        /* A string like "xyz" in L"abc" "xyz" needs widening. */
        check_assertion(con->character_kind == (a_character_kind)chk_char);
        widening_copy(con->variant.string.value, new_str+total_len, 
                      str_len, character_kind);
        str_len *= null_len;
      } else {
        (void)memcpy(new_str+total_len, con->variant.string.value,
                     size_t_arg(str_len));
      }  /* if */
      /* Keep track of the total length so far, which is also the offset for
         storing into the concatenation. */
      total_len += str_len;
    }  /* for */
    /* Overwrite the first string with the concatenation.  Note this is
       done late because the information in the first string is used in the
       concatenation loop above. */
    /* The string currently associated with the first constant (and, for that
       matter, the strings for all the constants) are just lost. */
    /* Get rid of any information specific to the old constant; in particular,
       get rid of its source correspondence (possible when the first constant
       comes from a macro). */
    clear_constant(concat_con, (a_constant_repr_kind)ck_string);
    concat_con->variant.string.length = total_len;
    concat_con->variant.string.value  = new_str;
    /* Adjust the constant type to match the new length. */
    concat_con->type = string_literal_type(character_kind,
                                           (a_targ_size_t)total_len/null_len);
    concat_con->character_kind = character_kind;
  }  /* if */
  db_exit();
}  /* concat_string_literals */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2013 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
