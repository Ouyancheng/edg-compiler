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

#include "basics.h"
#include "literals.h"
#include "il.h"
#include "cmd_line.h"
#include "target.h"
#include "error.h"
#include "preproc.h"
#include "lexical.h"
#include "float_pt.h"
#include "const_ints.h"
#include "types.h"


/* Convert a character hex digit to the associated hex digit value. */
#define hexvalue(ch) ((ch) - (isdigit(ch) ? '0' : \
                                            (islower(ch) ? 'a'-0xa : 'A'-0xA)))


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
*/
{
  unsigned long temp = 0;
  a_boolean     has_u_suffix = FALSE, has_l_suffix = FALSE;
  char          *temp_ptr;
  a_boolean     ovflo = FALSE;
  a_boolean     non_arith = (radix != 10);
  char          *real_end_pos = end_of_curr_token;
  int           digit;
  an_integer_kind
		kind;
  /* The ULTRIX C compiler has trouble with the type of folded compile-time
     unsigned expressions, so we use a variable for this value. */
  unsigned long ULONG_MAX_div_10 = ULONG_MAX;
  ULONG_MAX_div_10 /= (unsigned long)10;

  *err_code = ec_no_error;
  /* Locate and logically remove the suffix, if any.  The suffix is "u"
     for unsigned or "l" for long, or both, in upper or lower case. */
  if (real_end_pos >= start_of_curr_token) {
    if (*real_end_pos == 'u' || *real_end_pos == 'U') {
      has_u_suffix = TRUE;
      real_end_pos--;
      if (real_end_pos >= start_of_curr_token) {
        if (*real_end_pos == 'l' || *real_end_pos == 'L') {
          has_l_suffix = TRUE;
          real_end_pos--;
        }  /* if */
      }  /* if */
    } else if (*real_end_pos == 'l' || *real_end_pos == 'L') {
      has_l_suffix = TRUE;
      real_end_pos--;
      if (real_end_pos >= start_of_curr_token) {
        if (*real_end_pos == 'u' || *real_end_pos == 'U') {
          has_u_suffix = TRUE;
          real_end_pos--;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */

  /* Evaluate the literal as an unsigned long. */
  if (radix == 10) {
    /* Decimal. */
    temp = *start_of_curr_token - '0';
    for (temp_ptr = start_of_curr_token+1;
         temp_ptr <= real_end_pos; temp_ptr++) {
      digit = *temp_ptr - '0';
      /* Multiply previous value by 10, checking for overflow. */
      if (temp > ULONG_MAX_div_10) ovflo = TRUE;
      temp *= 10;
      /* Add in digit, checking for overflow. */
      if (temp > ULONG_MAX-(unsigned long)digit) ovflo = TRUE;
      temp += digit;
    }  /* for */
  } else if (radix == 8) {
    /* Octal.*/
    for (temp_ptr = start_of_curr_token+1;
         temp_ptr <= real_end_pos; temp_ptr++) {
      digit = *temp_ptr - '0';
      if (C_dialect != C_dialect_pcc && (digit >= 8)) {
        /* Digits 8 and 9 are allowed by K&R/pcc, but not by ANSI. */
        *err_pos = temp_ptr;
        *err_code = ec_bad_octal_digit;
        goto wrapup;
      }  /* if */
      /* Multiply previous value by 8, checking for overflow. */
      if (temp > ULONG_MAX>>3) ovflo = TRUE;
      temp <<= 3;
      /* Or in digit. */
      temp |= digit;
    }  /* for */
  } else {
    /* radix == 16 (hexadecimal). */
    for (temp_ptr = start_of_curr_token+2;
         temp_ptr <= real_end_pos; temp_ptr++) {
      digit = hexvalue(*temp_ptr);
      /* Multiply previous value by 16, checking for overflow. */
      if (temp > ULONG_MAX>>4) ovflo = TRUE;
      temp <<= 4;
      /* Or in digit. */
      temp |= digit;
    }  /* for */
  }  /* if */
  /* Determine the type based on the value and the suffixes.  See standard,
     3.1.3.2.  In pcc compatibility mode, overflow is ignored, and
     the constant is either int or long (see K&R, reference manual section,
     2.4.1 and 2.4.2). */
  if (C_dialect == C_dialect_pcc && !has_u_suffix) {
    /* Non-ANSI (pcc) checking. */
    if (ovflo) {
      /* A warning is generated for overflow, but the overflow is then
         ignored.  The conversions above produce the same value that pcc
         does. */
      /* Convert the character position into an error position. */
      conv_line_loc_to_source_pos(start_of_curr_token, &error_position);
      warning(ec_integer_too_large);
      ovflo = FALSE;
    }  /* if */
    if (has_l_suffix) {
      /* An explicit "L" suffix makes the constant long. */
      kind = (an_integer_kind)ik_long;
    } else if (radix == 10 && temp <= TARG_INT_MAX) {
      /* A decimal constant that is no larger than the largest signed int
        is an int. */
      kind = (an_integer_kind)ik_int;
    } else if (radix != 10 && temp <= TARG_UINT_MAX) {
      /* A hexadecimal or octal constant that is no larger than the largest
         unsigned int is treated as an int (there are no unsigned int
         constants in K&R/pcc). */
      kind = (an_integer_kind)ik_int;
    } else {
      /* Anything else is a long. */
      kind = (an_integer_kind)ik_long;
      /* A value that is larger than LONG_MAX is tagged as non-arithmetic
         because the source looks positive but the internal value is negative.
         This helps in avoiding an error when converting the smallest
         integer. */
      if (temp > TARG_LONG_MAX) non_arith = TRUE;
    }  /* if */
  } else if (!ovflo) {
    /* ANSI C constant checking. */
    if (!has_l_suffix && !has_u_suffix && temp <= TARG_INT_MAX) {
      kind = (an_integer_kind)ik_int;
    } else if (!has_l_suffix &&
               (has_u_suffix || radix != 10) &&
               temp <= TARG_UINT_MAX) {
      kind = (an_integer_kind)ik_unsigned_int;
    } else if (!has_u_suffix && temp <= TARG_LONG_MAX) {
      kind = (an_integer_kind)ik_long;
    } else if (temp <= TARG_ULONG_MAX) {
      kind = (an_integer_kind)ik_unsigned_long;
    } else {
      /* Doesn't fit in target integers. */
      ovflo = TRUE;
    }  /* if */
  }  /* if */
  if (ovflo) {
    *err_pos = start_of_curr_token;
    *err_code = ec_integer_too_large;
  } else {
    /* Build a constant with the right type and value. */
    clear_constant(&const_for_curr_token, (a_constant_repr_kind)ck_integer);
    const_for_curr_token.type                  = integer_type(kind);
    const_for_curr_token.variant.integer_value = (long)temp;
    const_for_curr_token.non_arithmetic        = non_arith;
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
               temp;
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
  fp_string_to_float(kind, start_of_curr_token, &temp, &err);
  *(actual_end+1) = old_next_char;
  *(actual_end+2) = old_next2_char;
  if (err) {
    *err_code = ec_bad_float_value;
    *err_pos = start_of_curr_token;
  } else {
    /* Build a constant with the right type and value. */
    clear_constant(&const_for_curr_token, (a_constant_repr_kind)ck_float);
    const_for_curr_token.type = float_type(kind);
    memcpy((char *)&const_for_curr_token.variant.float_value, (char *)&temp,
           sizeof(an_internal_float_value));
  }  /* if */
  if (*err_code != ec_no_error) {
    /* Return an error constant. */
    set_error_constant(&const_for_curr_token);
  }  /* if */
}  /* conv_float_literal */


static void conv_single_char(char          **temp_ptr,
                             unsigned long *ch,
                             unsigned long centity_mask,
                             unsigned long centity_sign_bit,
                             a_boolean     centity_is_signed)
/*
Fetch one character of a character constant or string literal.  The current
position in the token is *temp_ptr (it is incremented appropriately
for what is taken).  The character gotten is returned (sign-extended
if necessary) in ch.  centity_mask, centity_sign_bit, and centity_is_signed
define the character entity into which this character is going (char or
wchar_t).
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
        if (!isxdigit(*lptr)) {
          unrecognized = TRUE;
        } else {
          targ_ch = hexvalue(*lptr);  /* First digit. */
          while (isxdigit(tch = *(++lptr))) {
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
        if (isdigit(tch = *lptr) && tch != '8' && tch != '9') {
          /* Second digit. */
          lptr++;
          targ_ch = (targ_ch << 3) | (tch - '0');
          if (isdigit(tch = *lptr) && tch != '8' && tch != '9') {
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
  /* Drop sign extension, then add it again if the target wants it. */
  targ_ch &= centity_mask;
  if (centity_is_signed) {
    /* Sign-extend the value (the char or wchar_t type is signed). */
    if (targ_ch & centity_sign_bit) targ_ch |= ~centity_mask;
  }  /* if */
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
    if (targ_ch > centity_mask) range_error = TRUE;
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
                                  unsigned long centity_mask,
                                  unsigned long centity_sign_bit,
                                  a_boolean     centity_is_signed)
/*
Fetch one wide character of a wide character constant or string literal.
The current position in the token is *temp_ptr (it is incremented
appropriately for what is taken).  More than one source character
may be taken to produce one wide character as output.  The number of
source characters taken is returned in *chars_taken.  The wide character
gotten is returned (sign-extended if necessary) in ch.  centity_mask,
centity_sign_bit, and centity_is_signed define the attributes of wchar_t.
This routine works like mbtowc (see 4.10.7.2 and 3.1.3.4 in the ANSI C
standard).
*/
{
  /* Simple version: one character in means one wchar_t out. */
  conv_single_char(temp_ptr, ch, centity_mask, centity_sign_bit,
                   centity_is_signed);
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
  *constant_size = (*num_elems)*TARG_SIZEOF_WCHAR_T;
}  /* determine_wide_char_constant_size */


/*
Set variables describing the attributes of the character entity to be
used to match the type "char".
*/
#define set_centity_attributes_for_char()                             \
{ centity_mask = TARG_UCHAR_MAX;                                      \
  centity_sign_bit = (unsigned long)TARG_SCHAR_MAX + 1;               \
  centity_is_signed = targ_has_signed_chars;                          \
}  /* set_centity_attributes_for_char */


/*
Set variables describing the attributes of the character entity to be
used to match the type "wchar_t".
*/
#define set_centity_attributes_for_wchar_t()                          \
{ /* Make the sign bit. */                                            \
  centity_sign_bit = (unsigned long)1 <<                              \
                              ((TARG_SIZEOF_WCHAR_T*TARG_CHAR_BIT)-1);\
  /* Combine the sign bit with all the bits below the sign bit to     \
     get the full mask. */                                            \
  centity_mask = (centity_sign_bit) | ((centity_sign_bit) - 1);       \
  centity_is_signed = int_kind_is_signed(                             \
                              (an_integer_kind)TARG_WCHAR_T_INT_KIND);\
}  /* set_centity_attributes_for_wchar_t */


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
  unsigned long   i;
  unsigned long   ch;
  unsigned long   temp;
  char            *temp_ptr;
  a_boolean       is_wide = FALSE;
  a_type_ptr      con_type;
  an_integer_kind int_kind;
  sizeof_t        constant_size;
  a_targ_size_t   num_elems;
  unsigned long   chars_taken;
  unsigned long   centity_mask;
  unsigned long   centity_sign_bit;
  a_boolean       centity_is_signed;
  int             centity_bits;

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
    int_kind = (an_integer_kind)TARG_WCHAR_T_INT_KIND;
    determine_wide_char_constant_size(temp_ptr, num_chars, /*add_null=*/FALSE,
                                      &constant_size, &num_elems);
    set_centity_attributes_for_wchar_t();
    centity_bits = TARG_SIZEOF_WCHAR_T * TARG_CHAR_BIT;
  } else {
     /* Normal character constant. */
    if (C_dialect == C_dialect_cplusplus && num_chars == 1) {
      int_kind = (an_integer_kind)ik_char;
    } else {
      int_kind = (an_integer_kind)ik_int;
    }  /* if */
    constant_size = num_chars;
    set_centity_attributes_for_char();
    centity_bits = TARG_CHAR_BIT;
  }  /* if */
  con_type = integer_type(int_kind);
  /* See if the characters we have will fit in the size we've determined. */
  if (constant_size > con_type->size) {
    /* Too many characters to fit. */
    *err_code = ec_too_many_characters;
    *err_pos = start_of_curr_token;
  } else {
    /* Accumulate the characters. */
    temp = 0;
    for (i = 0; i < num_chars; i += chars_taken) {
      /* Convert one character of the char constant. */
      if (!is_wide) {
        conv_single_char(&temp_ptr, &ch, centity_mask, centity_sign_bit,
                         centity_is_signed);
        chars_taken = 1;
      } else {
        conv_single_wide_char(&temp_ptr, &ch, &chars_taken,
                              centity_mask, centity_sign_bit,
                              centity_is_signed);
      }  /* if */
      /* Put the character in the right place. */
#if TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT
      /* 'ab' == 0x6162. */
      /* Drop any sign extension on the new character if it's not the first. */
      if (i != 0 && centity_is_signed) ch &= centity_mask;
      temp <<= centity_bits;
#else
      /* 'ab' == 0x6261. */
      if (i != 0) {
        /* Drop any sign extension on the previous value if this isn't the
           first character. */
        if (centity_is_signed) {
          temp &= ~(~(unsigned long)0 << (i*centity_bits));
        }  /* if */
        ch <<= (i*centity_bits);
      } /* if */
#endif /* TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT */
      temp |= ch;
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
    const_for_curr_token.variant.integer_value = temp;
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
  int  i;
  char *p = *pstr;

  /* This is basically a copy of an integer to an array of characters;
     we must allow for the target endian-ness. */
  for (i = 0; i < TARG_SIZEOF_WCHAR_T; i++) {
#if TARG_LITTLE_ENDIAN
    *p++ = ch & UCHAR_MAX;
    ch >>= TARG_CHAR_BIT;
#else /* !TARG_LITTLE_ENDIAN */
    *p++ = (ch >> ((TARG_SIZEOF_WCHAR_T - i - 1) * TARG_CHAR_BIT)) & UCHAR_MAX;
#endif /* TARG_LITTLE_ENDIAN */
  }  /* for */
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
  unsigned long centity_sign_bit;
  a_boolean     centity_is_signed;
  
  *err_code = ec_no_error;
  *err_pos = NULL;  /* To make lint happy. */
  temp_ptr = start_of_curr_token+1;
  /* See if this is a wide string literal. */
  if (*start_of_curr_token == 'L') {
    /* Wide string literal. */
    is_wide = TRUE;
    /* Skip over the "L". */
    temp_ptr++;
    determine_wide_char_constant_size(temp_ptr, num_chars, /*add_null=*/TRUE,
                                      &constant_size, &num_elems);
    set_centity_attributes_for_wchar_t();
  } else {
    /* Normal string literal. */
    constant_size = num_elems = num_chars+1;  /* "+1" is space for the null. */
    set_centity_attributes_for_char();
  }  /* if */
  /* Allocate enough space to hold the final string, including the null
     added to it. */
  str_start = pstr = alloc_text_of_string_literal(constant_size);
  /* Accumulate the characters. */
  for (i = 0; i < num_chars; i += chars_taken) {
    /* Convert one character of the string literal. */
    if (!is_wide) {
      conv_single_char(&temp_ptr, &ch, centity_mask, centity_sign_bit,
                       centity_is_signed);
      /* Put the character in the right place. */
      *pstr++ = (char)ch;
      chars_taken = 1;
    } else {
      conv_single_wide_char(&temp_ptr, &ch, &chars_taken,
                            centity_mask, centity_sign_bit,
                            centity_is_signed);
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


void concat_string_literals(a_constant *first_string,
                            a_constant *second_string)
/*
Concatenate the two string literals (or wide string literals) indicated
by *first_string and *second_string, and place the result in *second_string.
This routine implements the lexical concatenation of section 2.1.1.2, phase
6, of the standard.  The null from the first string is discarded in
doing the concatenation; the one from the second string is copied as
the final null of the concatenated string; see 3.1.4.
*/
{
  a_targ_size_t s1_len, s2_len, new_len;
  char          *new_str;
  a_boolean     wide_strings;

  if (is_error_constant(first_string) || is_error_constant(second_string)) {
    /* One or the other of the strings had an error, leave the second
       string as is. */
  }  else {
    /* We want wide_strings FALSE if wchar_t and char are the same type,
       so test for not char array type rather than testing explicitly
       for wchar_t array. */
    wide_strings = !is_char_array_type(first_string->type);
    /* Get string lengths. */
    s1_len = first_string->variant.string.length;
    /* Remove null from length of first string. */
    if (!wide_strings) {
      s1_len--;
    } else {
      s1_len -= TARG_SIZEOF_WCHAR_T;
    }  /* if */
    if (s1_len > 0) {
      s2_len = second_string->variant.string.length;
      /* Allocate space for the concatenation. */
      new_len = s1_len + s2_len;
      new_str = alloc_text_of_string_literal((sizeof_t)new_len);
      /* Copy the two strings into the new space. */
      memcpy(new_str, first_string->variant.string.value, (int)s1_len);
      memcpy(&new_str[s1_len], second_string->variant.string.value,
                                                          (int)s2_len);
      /* Note that the space for the old strings is just lost; that's
         judged to be acceptable, since lexical concatenation will probably
         not be done excessively.  If we wanted to free the old strings:

         free_il(first_string->variant.string.value);
         free_il(second_string->variant.string.value);

         But there is currently no routine to do that kind of freeing.
      */
      /* Adjust the *second_string constant to be the new string. */
      second_string->variant.string.length = new_len;
      second_string->variant.string.value  = new_str;
      if (!wide_strings) {
        second_string->type = string_type((a_targ_size_t)new_len);
      } else {
        second_string->type = wide_string_type(
                               (a_targ_size_t)(new_len / TARG_SIZEOF_WCHAR_T));
      }  /* if */
    }  /* if */
  }  /* if */
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
