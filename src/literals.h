/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1991 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

literals.h -- Declarations relating to lexicals.c (having to do with 
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
extern void conv_float_literal(an_error_code *err_code,
                               char          **err_pos);
extern void conv_char_literal(unsigned long num_chars,
                              an_error_code *err_code,
                              char          **err_pos);
extern void conv_string_literal(unsigned long num_chars,
                                an_error_code *err_code,
                                char          **err_pos);
extern void concat_string_literals(a_token_cache_ptr cache,
                                   an_integer_kind   centity_int_kind);

#endif /* ifndef LITERALS_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1991 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
