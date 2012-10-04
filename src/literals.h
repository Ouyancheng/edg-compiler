/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2012 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

literals.h -- Declarations relating to literals.c (having to do with 
              conversion of literal constants to and from internal form).

*/

/* Avoid including these declarations more than once. */
#ifndef LITERALS_H
#define LITERALS_H 1

#ifndef ERROR_H
#include "error.h"
#endif /* ifndef ERROR_H */
#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef LEXICAL_H
#include "lexical.h"
#endif /* ifndef LEXICAL_H */

extern void conv_integer_literal(int           radix,
                                 an_error_code *err_code,
                                 char          **err_pos);
#if FIXED_POINT_ALLOWED
extern void conv_fixed_point_literal(a_boolean      is_hexadecimal,
                                     an_error_code  *err_code,
                                     char           **err_pos);
#endif /* FIXED_POINT_ALLOWED */
extern void conv_float_literal(a_boolean	is_hexadecimal,
			       an_error_code	*err_code,
	                       char		**err_pos);

/*
Structure to maintain the current state of the processing of
conv_single_char.
*/
typedef struct a_char_conversion_state *a_char_conversion_state_ptr;
typedef struct a_char_conversion_state {
  char		**next_token_char;
			/* Points to a pointer to the next character in the
			   token string to be processed.  This can point
			   within a multibyte character (both native and
			   UTF-8) unless we are translating from UTF-8 to
			   multibyte characters; in that case, this will
			   point to the next complete UTF-8 character in
			   the token string. */
  int		remaining_char_count;
			/* Number of bytes left in the current multibyte
			   character.  conv_single_char is called multiple
			   times for a multibyte character, each call
			   returning one byte of the result.  Also used to
			   indicate that the second code unit of a
			   surrogate pair is pending and should be returned
			   by conv_single_char instead of reading a new
			   character from the token. */
  char		*next_mbc_char;
			/* When translating from UTF-8 to multibyte
			   characters and for universal-character-names
			   (except when create_surrogate_pairs is TRUE), if
			   remaining_char_count is nonzero, points to the
			   next byte from translated_char to be returned.
			   NULL for normal multibyte character processing
			   (indicating multibyte characters will be fetched
			   directly from the token string). */
  unsigned long	pending_surrogate_pair;
			/* When create_surrogate_pairs is TRUE and a
			   character or universal-character-name is
			   encountered that requires a surrogate pair, the
			   second code unit of the pair is saved here and
			   remaining_char_count is set to 1 so that the
			   next call to conv_single_char will return it
			   instead of reading another character from the
			   token. */
  a_byte_boolean
		translate_utf8_to_mbc;
			/* If TRUE and the current file is Unicode, UTF-8
			   characters appearing in the token string will be
			   replaced in the converted output by their
			   corresponding multibyte character in the system
			   default locale.  This should be set
			   appropriately by the caller, e.g., TRUE for
			   Microsoft-mode character and string literals,
			   FALSE for header names. */
  a_byte_boolean
		create_surrogate_pairs;
			/* If TRUE, the target data type is such that a
			   character designating a code point > 0xffff must
			   be represented as a surrogate pair.
			   pending_surrogate_pair and remaining_char_count
			   are used to enable conv_single_char to return
			   the second code unit of the pair in a subsequent
			   call. */
  char		translated_char[MAX_MULTIBYTE_CHAR_LENGTH];
			/* When translating from UTF-8 to multibyte
			   characters and for universal-character-names,
			   contains the translated version of the current
			   character. */
} a_char_conversion_state;

#define clear_char_conversion_state(state, ptr, translate_utf8) \
  { (state)->next_token_char = ptr;                             \
    (state)->remaining_char_count = 0;                          \
    (state)->next_mbc_char = NULL;                              \
    (state)->translate_utf8_to_mbc = translate_utf8;            \
    (state)->create_surrogate_pairs = FALSE;                    \
  }  /* clear_char_conversion_state */

extern void conv_single_char(a_char_conversion_state_ptr state,
                             a_boolean                   process_escapes,
                             unsigned long               *ch,
                             unsigned long               centity_mask,
                             a_boolean                   narrow_literal);
extern void conv_char_literal(unsigned long num_chars,
                              an_error_code *err_code,
                              char          **err_pos);
extern void conv_string_literal(unsigned long num_chars,
                                an_error_code *err_code,
                                char          **err_pos);
extern void widen_string_literal(a_constant_ptr con);
extern void concat_string_literals(a_token_cache_ptr cache,
                                   a_character_kind  kind);

#endif /* ifndef LITERALS_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2012 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
