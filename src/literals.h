/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2011 Edison Design Group Inc.                   [_]          *
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
extern void conv_single_char(char          **temp_ptr,
                             int           *remaining_mbc_char_count,
                             a_boolean     process_escapes,
                             unsigned long *ch,
                             unsigned long centity_mask);
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
* Copyright 1988-2011 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
