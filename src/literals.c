/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
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


/* Convert a character hex digit to the associated hex digit value. */
#define hexvalue(ch) ((ch) - (isdigit((unsigned char)ch) ? '0' : \
                             (islower((unsigned char)ch) ? 'a'-0xa : 'A'-0xA)))


void conv_integer_literal(int           radix,
                          an_error_code *err_code,
                          char          **err_pos)
/*
Convert an integer of base indicated by radix (8, 10, or 16) from
external form to internal form.  start_of_curr_token and
end_of_curr_token point to the two ends of the external form.  The
internal form is placed in const_for_curr_token.  If there is no error,
*err_code is set to ec_no_error (which is 0); otherwise, *err_code is
set to an appropriate error code and *err_pos is set to the character
position of the error.  A zero-length number is converted as zero.
Other than the zero-length pathology, the input number is guaranteed
to be syntactically correct (except for digits 8 and 9 in octal 
constants).  The number may have a "u" or "l" suffix, or both.
(Or a "ll" or "ull" suffix, if long long is allowed.)
*/
{
  an_integer_value number, ten, digit, mask;
  a_boolean        has_u_suffix = FALSE, has_l_suffix = FALSE;
#if LONG_LONG_ALLOWED
  a_boolean        has_ll_suffix = FALSE;
#endif /* LONG_LONG_ALLOWED */
  char             *temp_ptr;
  a_boolean        err, ovflo = FALSE, do_sign_extension = FALSE;
  a_boolean        non_arith = (radix != 10);
  char             *real_end_pos = end_of_curr_token;
  unsigned long    intdigit;
  an_integer_kind  kind;

  *err_code = ec_no_error;
  /* Locate and logically remove the suffix, if any.  The suffix is "u"
     for unsigned or "l" for long, or both, in upper or lower case. */
#if LONG_LONG_ALLOWED
  /* "ll" means long long, "ull" means unsigned long long. */
#endif /* LONG_LONG_ALLOWED */
  if (real_end_pos >= start_of_curr_token) {
    for (;;) {
      if (*real_end_pos == 'u' || *real_end_pos == 'U') {
        has_u_suffix = TRUE;
        real_end_pos--;
      } else if (*real_end_pos == 'l' || *real_end_pos == 'L') {
#if LONG_LONG_ALLOWED
        if (has_l_suffix) {
          has_l_suffix = FALSE;
          has_ll_suffix = TRUE;
        } else
#endif /* LONG_LONG_ALLOWED */
        {
          has_l_suffix = TRUE;
        }  /* if */
        real_end_pos--;
      } else {
        /* Not an "l" or "u"; exit loop. */
        break;
      }  /* if */
    }  /* for */
  }  /* if */

  /* Evaluate the literal as an unsigned long. */
  if (radix == 10) {
    /* Decimal. */
    set_unsigned_integer_value(&ten, 10L);
    intdigit = *start_of_curr_token - '0';
    set_unsigned_integer_value(&number, intdigit);
    for (temp_ptr = start_of_curr_token+1;
         temp_ptr <= real_end_pos; temp_ptr++) {
      intdigit = *temp_ptr - '0';
      /* Multiply previous value by 10, checking for overflow. */
      multiply_integer_values(&number, &ten, /*is_signed=*/FALSE, &err);
      if (err) ovflo = TRUE;
      /* Add in digit, checking for overflow. */
      set_unsigned_integer_value(&digit, intdigit);
      add_integer_values(&number, &digit, /*is_signed=*/FALSE, &err);
      if (err) ovflo = TRUE;
    }  /* for */
  } else if (radix == 8) {
    /* Octal.*/
    set_unsigned_integer_value(&number, 0L);
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
      set_unsigned_integer_value(&digit, intdigit);
      or_integer_values(&number, &digit);
    }  /* for */
  } else {
    /* radix == 16 (hexadecimal). */
    set_unsigned_integer_value(&number, 0L);
    for (temp_ptr = start_of_curr_token+2;
         temp_ptr <= real_end_pos; temp_ptr++) {
      intdigit = hexvalue(*temp_ptr);
      /* Multiply previous value by 16, checking for overflow. */
      shift_left_integer_value(&number, 4, &err);
      if (err) ovflo = TRUE;
      /* Or in digit. */
      set_unsigned_integer_value(&digit, intdigit);
      or_integer_values(&number, &digit);
    }  /* for */
  }  /* if */
  /* Determine the type based on the value and the suffixes.  See standard,
     3.1.3.2.  In pcc compatibility mode, overflow is ignored, and
     the constant is either int or long (see K&R, reference manual section,
     2.4.1 and 2.4.2). */
  /* Since the "u" suffix does not exist in pcc C, treat constants with
     that suffix according to the ANSI rules. */
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
    } else if (le_max_integer_value_of_kind(&number, /*is_signed=*/FALSE,
                                          (an_integer_kind)ik_unsigned_long)) {
      kind = (an_integer_kind)ik_unsigned_long;
      goto kind_established;
    }  /* if */
#if LONG_LONG_ALLOWED
ll_check:
    if (!has_u_suffix &&
        le_max_integer_value_of_kind(&number, /*is_signed=*/FALSE,
                                     (an_integer_kind)ik_long_long)) {
      kind = (an_integer_kind)ik_long_long;
      goto kind_established;
    } else if (le_max_integer_value_of_kind(&number, /*is_signed=*/FALSE,
                                     (an_integer_kind)ik_unsigned_long_long)) {
      kind = (an_integer_kind)ik_unsigned_long_long;
      goto kind_established;
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
kind_established:;
  }  /* if */
  if (ovflo) {
    *err_pos = start_of_curr_token;
    *err_code = ec_integer_too_large;
  } else {
    /* Build a constant with the right type and value. */
    clear_constant(&const_for_curr_token, (a_constant_repr_kind)ck_integer);
    const_for_curr_token.type                  = integer_type(kind);
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


void conv_float_literal(an_error_code *err_code,
                        char          **err_pos)
/*
Convert a floating constant from external form to internal form.
start_of_curr_token and end_of_curr_token point to the two ends of the
external form.  The internal form is placed in const_for_curr_token.  If
there is no error, *err_code is set to ec_no_error (which is 0);
otherwise, *err_code is set to an appropriate error code and *err_pos
is set to the character position of the error.
*/
{
  a_float_kind kind;
  an_internal_float_value
               number;
  char         *actual_end;
  char         old_next_char, old_next2_char;
  a_boolean    err;

  *err_code = ec_no_error;
  /* See if there is a suffix. */
  if (*end_of_curr_token == 'f' || *end_of_curr_token == 'F') {
    /* "F" suffix, indicates float type. */
    kind = (a_float_kind)fk_float;
    actual_end = end_of_curr_token-1;
  } else if (*end_of_curr_token == 'l' || *end_of_curr_token == 'L') {
    /* "L" suffix, indicates long double. */
    kind = (a_float_kind)fk_long_double;
    actual_end = end_of_curr_token-1;
  } else {
    /* No suffix.  Default is double. */
    kind = (a_float_kind)fk_double;
    actual_end = end_of_curr_token;
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
    *(actual_end+1) = '0';
    *(actual_end+2) = '\0';
  } else {
    *(actual_end+1) = '\0';
  }  /* if */
  /* Do the conversion. */
  fp_string_to_float(kind, start_of_curr_token, &number, &err);
  *(actual_end+1) = old_next_char;
  *(actual_end+2) = old_next2_char;
  if (err) {
    *err_code = ec_bad_float_value;
    *err_pos = start_of_curr_token;
  } else {
    /* Build a constant with the right type and value. */
    clear_constant(&const_for_curr_token, (a_constant_repr_kind)ck_float);
    const_for_curr_token.type = float_type(kind);
    const_for_curr_token.variant.float_value = number;
  }  /* if */
  if (*err_code != ec_no_error) {
    /* Return an error constant. */
    set_error_constant(&const_for_curr_token);
  }  /* if */
}  /* conv_float_literal */


static void conv_single_char(char          **temp_ptr,
                             unsigned long *ch,
                             unsigned long centity_mask)
/*
Fetch one character of a character constant or string literal.  The current
position in the token is *temp_ptr (it is incremented appropriately
for what is taken).  The character gotten is returned (not sign-extended)
in ch.  centity_mask defines the size of the character entity into which
this character is going (char or wchar_t).
*/
{
  register unsigned long targ_ch;
  register char          src_ch, tch;
  register char          *lptr;
  int                    digit;
  a_boolean              range_error = FALSE;
  a_boolean              unrecognized;

  lptr = *temp_ptr;
  targ_ch = src_ch = *(lptr++);
  if (src_ch == '\\') {
    /* Backslash, escaped character.  Can be an octal escape, a hexadecimal
       escape, a simple escape sequence (like \n), or something unrecognized,
       in which case the character is left alone.  See standard, 2.2.2,
       3.1.3.4, and 3.1.4. */
    unrecognized = FALSE;
    switch ((int)(tch = *(lptr++))) {
      case 'a':
        if (C_dialect == C_dialect_pcc) {
          /* pcc does not recognize \a. */
          unrecognized = TRUE;
        } else {
          targ_ch = TARG_ALERT_CHAR;
        }  /* if */
        break;
      case 'b':
        targ_ch = TARG_BACKSPACE_CHAR;
        break;
      case 'f':
        targ_ch = TARG_FORM_FEED_CHAR;
        break;
      case 'n':
        targ_ch = TARG_NEWLINE_CHAR;
        break;
      case 'r':
        targ_ch = TARG_CARR_RETURN_CHAR;
        break;
      case 't':
        targ_ch = TARG_HORIZ_TAB_CHAR;
        break;
      case 'v':
        /* \v is not in K&R, but is recognized by pcc. */
        targ_ch = TARG_VERT_TAB_CHAR;
        break;
      case 'x':
        /* Hexadecimal escape.  There can be many digits, but there must be
           at least one.  If not, treat as just "x". */
        if (!isxdigit((unsigned char)*lptr)) {
          unrecognized = TRUE;
        } else {
          targ_ch = hexvalue(*lptr);  /* First digit. */
          while (isxdigit((unsigned char)(tch = *(++lptr)))) {
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
        targ_ch = tch - '0';  /* First digit. */
        tch = *lptr;
        if (isdigit((unsigned char)tch) && tch != '8' && tch != '9') {
          /* Second digit. */
          targ_ch = (targ_ch << 3) | (tch - '0');
          lptr++;
          tch = *lptr;
          if (isdigit((unsigned char)tch) && tch != '8' && tch != '9') {
            /* Third digit. */
            lptr++;
            targ_ch = (targ_ch << 3) | (tch - '0');
          }  /* if */
        }  /* if */
        goto range_check;
      default:
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
      conv_line_loc_to_source_pos(*temp_ptr, &error_position);
      warning(ec_unrecognized_char_escape);
      targ_ch = tch;
    }  /* if */
  }  /* if */
return_point:
  /* Drop sign extension (from host C compiler with signed characters)
     and out-of-range bits. */
  targ_ch &= centity_mask;
  *ch = targ_ch;
  *temp_ptr = lptr;
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
    if ((targ_ch & ~centity_mask) != 0) range_error = TRUE;
  }  /* if */
  if (range_error) {
    conv_line_loc_to_source_pos(*temp_ptr, &error_position);
    if (strict_ansi_mode) {
      diagnostic(strict_ansi_error_severity, ec_bad_character_value);
    } else {
      warning(ec_bad_character_value);
    }  /* if */
    /* Value is truncated by the normal return processing. */
  }  /* if */
  goto return_point;
}  /* conv_single_char */


static void conv_single_wide_char(char          **temp_ptr,
                                  unsigned long *ch,
                                  unsigned long *chars_taken,
                                  unsigned long centity_mask)
/*
Fetch one wide character of a wide character constant or string literal.
The current position in the token is *temp_ptr (it is incremented
appropriately for what is taken).  More than one source character
may be taken to produce one wide character as output.  The number of
source characters taken is returned in *chars_taken.  The wide character
gotten is returned (not sign-extended) in ch.  centity_mask defines
the size of wchar_t.  This routine works like mbtowc (see 4.10.7.2 and
3.1.3.4 in the ANSI C standard).
*/
{
  /* Simple version: one character in means one wchar_t out. */
  conv_single_char(temp_ptr, ch, centity_mask);
  *chars_taken = 1;
}  /* conv_single_wide_char */


/*ARGSUSED*/ /* <-- Because temp_ptr is not used in this simple version. */
static void determine_wide_char_constant_size(char          *temp_ptr,
                                              unsigned long num_chars,
                                              a_boolean     add_null,
                                              sizeof_t      *constant_size,
                                              a_targ_size_t *num_elems)
/*
Determine the size of a wide character constant or string literal.
temp_ptr points to the source characters; there are num_chars of them.
A null should be considered appended to the constant if add_null is TRUE.
Return *constant_size set to the size in bytes for the constant and
*num_elems set to the number of wchar_t elements.  This routine should
provide results consistent with the functioning of mbtowc (see 4.10.7.2
and 3.1.3.4 in the ANSI C standard).
*/
{
  /* Simple version: one character in means one wchar_t out. */
  /* Fancier versions may have to scan the text of the literal here to
     determine the proper size after allowing for escape sequences and
     the like. */
  *num_elems = num_chars;
  if (add_null) (*num_elems)++;
  *constant_size = (sizeof_t)((*num_elems)*targ_sizeof_wchar_t);
}  /* determine_wide_char_constant_size */


void conv_char_literal(unsigned long num_chars,
                       an_error_code *err_code,
                       char          **err_pos)
/*
Convert a character constant from external form to internal form.
start_of_curr_token and end_of_curr_token point to the two ends of the
external form.  The internal form is placed in const_for_curr_token.  If
there is no error, *err_code is set to ec_no_error (which is 0);
otherwise, *err_code is set to an appropriate error code and *err_pos
is set to the character position of the error.  num_chars indicates
the number of characters contained within the quotes (after escape
processing).
*/
{
  unsigned long    i;
  unsigned long    ch;
  an_integer_value number, ch_int_val;
  char             *temp_ptr;
  a_boolean        is_wide = FALSE, err;
  a_type_ptr       con_type;
  an_integer_kind  int_kind;
  sizeof_t         constant_size;
  a_targ_size_t    num_elems;
  unsigned long    chars_taken;
  unsigned long    centity_mask;
  a_boolean        centity_is_signed;
  int              centity_bits;

  *err_code = ec_no_error;
  *err_pos = NULL;
  temp_ptr = start_of_curr_token+1;
  /* See if this is a wide character constant. */
  /* Determine the constant type:
       Wide character constant  (L'x'): wchar_t
       Single character constant ('x'): int in C, char in C++
       Multi-character constant ('xy'): int
  */
  if (*start_of_curr_token == 'L') {
    /* Wide character constant. */
    is_wide = TRUE;
    /* Skip over the "L". */
    temp_ptr++;
    int_kind = targ_wchar_t_int_kind;
    determine_wide_char_constant_size(temp_ptr, num_chars, /*add_null=*/FALSE,
                                      &constant_size, &num_elems);
    centity_mask = (unsigned long)1 << ((targ_sizeof_wchar_t*targ_char_bit)-1);
    centity_mask = centity_mask | (centity_mask - 1);
    centity_bits = targ_sizeof_wchar_t*targ_char_bit;
    centity_is_signed = int_kind_is_signed[(int)targ_wchar_t_int_kind];
  } else {
     /* Normal character constant. */
    if (C_dialect == C_dialect_cplusplus && num_chars == 1) {
      int_kind = (an_integer_kind)ik_char;
    } else {
      int_kind = (an_integer_kind)ik_int;
    }  /* if */
    constant_size = (sizeof_t)num_chars;
    centity_mask = (unsigned long)1 << (targ_char_bit-1);
    centity_mask = centity_mask | (centity_mask - 1);
    centity_bits = targ_char_bit;
    centity_is_signed = targ_has_signed_chars; 
  }  /* if */
  con_type = integer_type(int_kind);
  /* See if the characters we have will fit in the size we've determined. */
  if (constant_size > con_type->size) {
    /* Too many characters to fit. */
    *err_code = ec_too_many_characters;
    *err_pos = start_of_curr_token;
    /* For wide character literals, make this a warning because the C standard
       says it is implementation-defined, and several test suites have
       something like L'ab' in them. */
    if (is_wide) {
      conv_line_loc_to_source_pos(*err_pos, &error_position);
      warning(*err_code);
      *err_code = ec_no_error;
      *err_pos = NULL;
    }  /* if */
  }  /* if */
  if (*err_code == ec_no_error) {
    /* Accumulate the characters. */
    set_unsigned_integer_value(&number, 0L);
    for (i = 0; i < num_chars; i += chars_taken) {
      /* Convert one character of the char constant. */
      if (!is_wide) {
        conv_single_char(&temp_ptr, &ch, centity_mask);
        chars_taken = 1;
      } else {
        conv_single_wide_char(&temp_ptr, &ch, &chars_taken, centity_mask);
      }  /* if */
      /* For wide character constants with too many characters, ignore
         characters that don't fit. */
      if (is_wide && i > 0) continue;
      /* Put the character in the right place. */
      set_unsigned_integer_value(&ch_int_val, ch);
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
#if CHECKING
    /* Make sure the whole constant was taken.  If not, the character count
       from accum_quoted_string is wrong. */
    if (temp_ptr != end_of_curr_token) {
      internal_error("conv_char_literal: length miscalculated");
    }  /* if */
#endif /* CHECKING */
    clear_constant(&const_for_curr_token, (a_constant_repr_kind)ck_integer);
    const_for_curr_token.type = con_type;
    const_for_curr_token.variant.integer_value = number;
  }  /* if */
  if (*err_code != ec_no_error) {
    /* Return an error constant. */
    set_error_constant(&const_for_curr_token);
  }  /* if */
}  /* conv_char_literal */


static void put_wide_char_into_string(unsigned long ch,
                                      char     **pstr)
/*
Put the wide character ch into the string pointed to by *pstr, and increment
*pstr by the proper amount.
*/
{
  unsigned int  i;
  char *p = *pstr;

  /* This is basically a copy of an integer to an array of characters;
     we must allow for the target endian-ness. */
  if (targ_little_endian) {
    for (i = 0; i < targ_sizeof_wchar_t; i++) {
      *p++ = (char) (ch & UCHAR_MAX);
      ch >>= targ_char_bit;
    }  /* for */
  } else {
    for (i = 0; i < targ_sizeof_wchar_t; i++) {
      *p++ = (char) ((ch >> ((targ_sizeof_wchar_t - i - 1) *
                                               targ_char_bit)) & UCHAR_MAX);
    }  /* for */
  }  /* if */
  *pstr = p;
}  /* put_wide_char_into_string */


void conv_string_literal(unsigned long num_chars,
                         an_error_code *err_code,
                         char          **err_pos)
/*
Convert a string literal from external form to internal form.
start_of_curr_token and end_of_curr_token point to the two ends of the
external form.  The internal form is placed in const_for_curr_token.  If
there is no error, *err_code is set to ec_no_error (which is 0);
otherwise, *err_code is set to an appropriate error code and *err_pos
is set to the character position of the error.  num_chars indicates
the number of characters contained within the quotes (after escape
processing).
*/
{
  unsigned long i;
  unsigned long ch;
  char          *temp_ptr;
  char          *pstr, *str_start;
  a_boolean     is_wide = FALSE;
  sizeof_t      constant_size;
  a_targ_size_t num_elems;
  unsigned long chars_taken;
  unsigned long centity_mask;

  *err_code = ec_no_error;
  *err_pos = NULL;  /* To make lint happy. */
  /* Build a mask used to mask individual characters. */
  centity_mask = (unsigned long)1 << (targ_host_string_char_bit-1);
  centity_mask = centity_mask | (centity_mask-1);
  temp_ptr = start_of_curr_token+1;
  /* See if this is a wide string literal. */
  if (*start_of_curr_token == 'L') {
    /* Wide string literal. */
    is_wide = TRUE;
    /* Skip over the "L". */
    temp_ptr++;
    determine_wide_char_constant_size(temp_ptr, num_chars, /*add_null=*/TRUE,
                                      &constant_size, &num_elems);
    /* Replicate the mask for one character as many times as there are
       characters in the wide character.  This "inefficient" method is used
       because it works right even when the target character is larger than
       the host character.  In that case, there are "holes" in the bit
       pattern where a "1" bit cannot be represented. */
    for (i = 1; i < targ_sizeof_wchar_t; i++) {
      centity_mask |= (centity_mask << targ_char_bit);
    }  /* for */
  } else {
    /* Normal string literal.  The "+1" is space for the null. */
    constant_size = (sizeof_t)(num_elems = num_chars+1);
    /* centity_mask is already set. */
  }  /* if */
  /* Allocate enough space to hold the final string, including the null
     added to it. */
  str_start = pstr = alloc_text_of_string_literal(constant_size);
  /* Accumulate the characters. */
  for (i = 0; i < num_chars; i += chars_taken) {
    /* Convert one character of the string literal. */
    if (!is_wide) {
      conv_single_char(&temp_ptr, &ch, centity_mask);
      /* Put the character in the right place. */
      *pstr++ = (char)ch;
      chars_taken = 1;
    } else {
      conv_single_wide_char(&temp_ptr, &ch, &chars_taken, centity_mask);
      put_wide_char_into_string(ch, &pstr);
    }  /* if */
  }  /* for */
#if CHECKING
  /* Make sure the whole string was taken.  If not, the character count
     from accum_quoted_string is wrong. */
  if (temp_ptr != end_of_curr_token) {
    internal_error("conv_string_literal: length miscalculated");
  }  /* if */
#endif /* CHECKING */
  /* Add the final null. */
  if (!is_wide) {
    *pstr = '\0';
  } else {
    ch = 0;
    put_wide_char_into_string(ch, &pstr);
  }  /* if */
  /* Make the constant entry for the string. */
  clear_constant(&const_for_curr_token, (a_constant_repr_kind)ck_string);
  const_for_curr_token.type = is_wide ? wide_string_type(num_elems) :
                                        string_type(num_elems);
  const_for_curr_token.variant.string.length = constant_size;
  const_for_curr_token.variant.string.value  = str_start;
  if (*err_code != ec_no_error) {
    /* Return an error constant. */
    set_error_constant(&const_for_curr_token);
  }  /* if */
}  /* conv_string_literal */


void concat_string_literals(a_token_cache_ptr cache,
                            an_integer_kind   centity_int_kind)
/*
Concatenate two or more string literals (or wide string literals) contained
in the indicated token cache, and replace the constant in the first
cached string token with the constant for the concatenation.  (The rest of the
cached tokens are left as they are; the caller removes and frees them.)
Some of the constants may be error constants if there were malformed
string literals in the input; in that case, the output is an error constant.
Some of the entries in the token cache may be for pragmas; they are ignored.
This routine implements the lexical concatenation of section 2.1.1.2, phase
6, of the C standard.  The nulls from the initial strings are discarded in
doing the concatenation, and the one from the last string is copied as
the final null of the concatenated string; see ANSI C 3.1.4.
All the strings will be wide or not wide, or rather, all will have the
same underlying character type (wchar_t and char might be the same type);
centity_int_kind indicates the underlying character type.
*/
{
  a_targ_size_t      total_len = 0, str_len, null_len;
  a_cached_token_ptr ctp, first_string_token = NULL;
  a_boolean          any_error_constant = FALSE;
  a_constant_ptr     concat_con, con;
  char               *new_str;

  db_enter(4, "concat_string_literals");
  /* Determine the length of the terminating null on strings.  It's usually
     1, but it may be bigger for wide string literals. */
  if (centity_int_kind == targ_wchar_t_int_kind) {
    /* Wide string literal -- the null is the size of a wchar_t. */
    null_len = targ_sizeof_wchar_t;
  } else {
    /* Non-wide string literal -- the null is one byte. */
    null_len = 1;
  }  /* if */
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
      any_error_constant = TRUE;
      break;
    } else {
      /* String constant. */
      check_assertion_str(con->kind == (a_constant_repr_kind)ck_string,
                          "concat_string_literals: constant not ck_string");
      /* Determine the length of this string literal. */
      str_len = con->variant.string.length;
      /* Except on the last constant, subtract out the space for the
         final null in the string. */
      if (ctp->next != NULL) str_len -= null_len;
      /* Add the length of this string to the accumulated length. */
      total_len += str_len;
    }  /* if */
  }  /* for */
  /* Here, we either have the length of the concatenation in total_len, or
     any_error_constant is set. */
  /* Build the concatenation and record it in the constant in the first
     string token in the cache. */
  concat_con = first_string_token->variant.constant;
  if (any_error_constant) {
    /* There is at least one error constant in the concatenation, so return
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
      if (ctp->next != NULL) str_len -= null_len;
      /* Copy the string text (including the final null, if that's
         appropriate). */
      (void)memcpy(new_str+total_len, con->variant.string.value,
                   size_t_arg(str_len));
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
    if (centity_int_kind != targ_wchar_t_int_kind) {
      concat_con->type = string_type((a_targ_size_t)total_len);
    } else {
      concat_con->type = wide_string_type(
                             (a_targ_size_t)(total_len / targ_sizeof_wchar_t));
    }  /* if */
  }  /* if */
  db_exit();
}  /* concat_string_literals */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
