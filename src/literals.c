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


static void conv_single_char(char **temp_ptr,
                             long *ch)
/*
Fetch one character of a character constant or string literal.  The current
position in the token is *temp_ptr (it is incremented appropriately
for what is taken).  The character gotten is returned (sign-extended
if necessary) in ch.
*/
{
  register long c;
  register char tch;
  register char *lptr;
  int           digit;
  a_boolean     range_error = FALSE;
  a_boolean     unrecognized;

  lptr = *temp_ptr;
  c = *(lptr++);
  if (c == '\\') {
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
          c = TARG_ALERT_CHAR;
        }  /* if */
        break;
      case 'b':
        c = TARG_BACKSPACE_CHAR;
        break;
      case 'f':
        c = TARG_FORM_FEED_CHAR;
        break;
      case 'n':
        c = TARG_NEWLINE_CHAR;
        break;
      case 'r':
        c = TARG_CARR_RETURN_CHAR;
        break;
      case 't':
        c = TARG_HORIZ_TAB_CHAR;
        break;
      case 'v':
        /* \v is not in K&R, but is recognized by pcc. */
        c = TARG_VERT_TAB_CHAR;
        break;
      case 'x':
        /* Hexadecimal escape.  There can be many digits, but there must be
           at least one.  If not, treat as just "x". */
        if (!isxdigit(*lptr)) {
          unrecognized = TRUE;
        } else {
          c = hexvalue(*lptr);  /* First digit. */
          while (isxdigit(tch = *(++lptr))) {
            if (c > ((unsigned long)LONG_MAX)>>4) {
              /* Error will be processed below.  We must keep going and take
                 all the digits. */
              c &= (1<<TARG_CHAR_BIT)-1;
              range_error = TRUE;
            }  /* if */
            digit = hexvalue(tch);
            c = (c << 4) | digit;
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
        c = tch - '0';  /* First digit. */
        if (isdigit(tch = *lptr) && tch != '8' && tch != '9') {
          /* Second digit. */
          lptr++;
          c = (c << 3) | (tch - '0');
          if (isdigit(tch = *lptr) && tch != '8' && tch != '9') {
            /* Third digit. */
            lptr++;
            c = (c << 3) | (tch - '0');
          }  /* if */
        }  /* if */
        goto range_check;
      default:
        /* Other characters, left alone.  Specifically, standard requires
           that \', \", \?, and \\ be reduced to just the escaped character. */
        if (tch == '\'' || tch == '"' || tch == '?' || tch == '\\') {
          c = tch;
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
      c = tch;
    }  /* if */
  }  /* if */
return_point:
  *ch = c;
  *temp_ptr = lptr;
  return;

range_check:
  /* Check that the value of c is legal for a character.  The standard
     (3.1.3.4) requires that this be diagnosed, but also says that it's
     implementation-defined.  That means it has to be a warning rather
     than an error. */
  if (range_error || c > TARG_UCHAR_MAX) {
    conv_line_loc_to_source_pos(*temp_ptr, &error_position);
    warning(ec_bad_character_value);
    /* Truncate the character value so it fits in a target character. */
    c &= (1<<TARG_CHAR_BIT)-1;
  }  /* if */
  if (targ_has_signed_chars) {
    /* Sign-extend the value (target has signed chars). */
    if (c > TARG_SCHAR_MAX) {
      c |= ~((1<<TARG_CHAR_BIT)-1);
    }  /* if */
  }  /* if */
  goto return_point;
}  /* conv_single_char */


void conv_char_literal(long          num_chars,
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
  long            i;
  long            ch;
  long            temp;
  char            *temp_ptr;
  a_boolean       is_wide = FALSE;
  a_type_ptr      con_type;
  an_integer_kind int_kind;

  *err_code = ec_no_error;
  *err_pos = NULL;
  temp_ptr = start_of_curr_token;
  /* If this is a wide char constant, skip the "L". */
  if (*temp_ptr == 'L') {
    temp_ptr++;
    is_wide = TRUE;
  }  /* if */
  /* Advance past the opening quote. */
  temp_ptr++;
  /* Determine the constant type:
       Wide character constant (L'x'): wchar_t
       Single character constant ('x'): int in C, char in C++
       Multi-character constant ('xy'): int
  */
  if (is_wide) {
    int_kind = (an_integer_kind)TARG_WCHAR_T_INT_KIND;
  } else if (C_dialect == C_dialect_cplusplus && num_chars == 1) {
    int_kind == (an_integer_kind)ik_char;
  } else {
    int_kind = (an_integer_kind)ik_int;
  }  /* if */
  con_type = integer_type(int_kind);
  /* See if the characters we have will fit in the size we've chosen. */
  if (num_chars > con_type->size) {
    /* Too many characters to fit. */
    *err_code = ec_too_many_characters;
    *err_pos = start_of_curr_token;
  } else {
    temp = 0;
    for (i = 0; i < num_chars; i++) {
      /* Convert one character of the char constant. */
      conv_single_char(&temp_ptr, &ch);
      /* Put the character in the right place. */
#if TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT
      /* 'ab' == 0x6162. */
      /* Drop any sign extension on the new character if it's not the first. */
      if (targ_has_signed_chars && i != 0) ch &= (1<<TARG_CHAR_BIT)-1;
      temp <<= TARG_CHAR_BIT;
#else
      /* 'ab' == 0x6261. */
      /* Drop any sign extension on the previous value if this isn't the
         first character. */
      if (targ_has_signed_chars && i != 0) temp &= (1<<(i*TARG_CHAR_BIT))-1;
      ch <<= i*TARG_CHAR_BIT;
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


void conv_string_literal(long          num_chars,
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
  long i;
  long ch;
  char *temp_ptr;
  char *pstr;
  
  *err_code = ec_no_error;
  *err_pos = NULL;  /* To make lint happy. */
  /* Allocate enough space to hold the final string, including the null
     added to it. */
  pstr = alloc_text_of_string_literal((sizeof_t)(num_chars+1));
  temp_ptr = start_of_curr_token+1;
  /* If this is a wide string literal, skip the "L". */
  if (*start_of_curr_token == 'L') temp_ptr++;
  for (i = 0; i < num_chars; i++) {
    /* Convert one character of the string literal. */
    conv_single_char(&temp_ptr, &ch);
    /* Put the character in the right place. */
    pstr[i] = (char)ch;
  }  /* for */
#if CHECKING
  /* Make sure the whole string was taken.  If not, the character count
     from accum_quoted_string is wrong. */
  if (temp_ptr != end_of_curr_token) {
    internal_error("conv_string_literal: length miscalculated");
  }  /* if */
#endif /* CHECKING */
  pstr[num_chars] = '\0';
  clear_constant(&const_for_curr_token, (a_constant_repr_kind)ck_string);
  const_for_curr_token.type = string_type((a_targ_size_t)(num_chars+1));
  const_for_curr_token.variant.string.length = num_chars+1;
  const_for_curr_token.variant.string.value  = pstr;
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
  long s1_len, s2_len, new_len;
  char *new_str;

  if (first_string ->kind == (a_constant_repr_kind)ck_error ||
      second_string->kind == (a_constant_repr_kind)ck_error) {
    /* One or the other of the strings had an error, leave the second
       string as is. */
  }  else {
    /* Get string lengths. */
    s1_len = first_string->variant.string.length - 1;  /* Remove null. */
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
      second_string->type = string_type((a_targ_size_t)new_len);
    }  /* if */
  }  /* if */
}  /* concat_string_literals */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
