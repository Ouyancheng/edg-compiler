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

preproc.h -- Declarations related to preproc.c (having to do with
             preprocessing directives).

*/

/* Avoid including these declarations more than once: */
#ifndef PREPROC_H
#define PREPROC_H 1

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */

#ifndef PRAGMA_H
#include "pragma.h"
#endif /* ifndef PRAGMA_H */

#ifndef LEXICAL_H
#include "lexical.h"
#endif /* ifndef LEXICAL_H */

/*
If this list is updated, be sure to change pp_directive_kind_names below.
*/
typedef enum /*a_pp_directive_kind*/ {
  /* Enumeration of preprocessing directives. */
  ppd_if, ppd_ifdef, ppd_ifndef, ppd_elif, ppd_else,
  ppd_endif, ppd_include, ppd_define, ppd_undef, ppd_line,
  ppd_error, ppd_pragma, ppd_null, ppd_linedef,
#if IDENT_DIRECTIVE_AND_PRAGMA
  ppd_ident,
#endif /* IDENT_DIRECTIVE_AND_PRAGMA */
#if ALIAS_DIRECTIVE
  ppd_alias,
#endif /* ALIAS_DIRECTIVE */
#if ATT_PREPROCESSING_EXTENSIONS_ALLOWED
  ppd_assert, ppd_unassert,
#endif /* ATT_PREPROCESSING_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  ppd_import,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  ppd_include_next,
  ppd_warning,
  ppd_not_valid
} a_pp_directive_kind;

#if DEBUG
/*
Table of names of preprocessing directives, used as event kinds for PCH
processing.  This is not the definition of the preprocessing directive
keywords (see identify_dir_keyword).
*/
EXTERN char	*pp_directive_kind_names[(int)ppd_not_valid+1]
#if VAR_INITIALIZERS
= { "if",
    "ifdef",
    "ifndef",
    "elif",
    "else",
    "endif",
    "include",
    "define",
    "undef",
    "line",
    "error",
    "pragma",
    "null",
    "linedef",
#if IDENT_DIRECTIVE_AND_PRAGMA
    "ident",
#endif /* IDENT_DIRECTIVE_AND_PRAGMA */
#if ALIAS_DIRECTIVE
    "alias",
#endif /* ALIAS_DIRECTIVE */
#if ATT_PREPROCESSING_EXTENSIONS_ALLOWED
    "assert",
    "unassert",
#endif /* ATT_PREPROCESSING_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    "import",
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    "include_next",
    "warning",
    "not_valid"
  }
#endif /* VAR_INITIALIZERS */
;
#endif /* DEBUG */


/*
Preprocessor state variables.  These all have valid values at all times
(not just when other variables would indicate that it is sensible for them
to have values), except as explicitly noted.
*/
EXTERN a_boolean
		fetch_pp_tokens;
			/* TRUE if preprocessing tokens (pp-tokens) should be
			   fetched instead of normal tokens.  Also implies
			   that constants should not be converted (and
			   numeric ones should be returned as pp-number),
			   adjacent strings should not be concatenated,
			   keywords should not be recognized, and errors
			   should not be issued for malformed tokens. */
EXTERN a_boolean
		expand_macros;
			/* TRUE if preprocessor macros should be expanded. */
EXTERN a_boolean
		in_preprocessing_directive;
			/* TRUE if we are currently somewhere between the
			   opening "#" and the closing newline of a
			   preprocessing directive.  This controls the
			   interpretation of white space, makes newline
			   a token, disables recognition of keywords,
			   and enables "#" and "##" as tokens. */
EXTERN a_boolean
		caching_pragma_tokens;
			/* TRUE is we are in a pragma that is being recorded
			   as a token cache.  This disables some of
			   the special processing that is normally done when
		           is_preprocessing_directive is TRUE; specifically,
			   identifiers are looked up and the tokens
			   "#" and "##" are disabled.  Keywords are only
			   recognized if processing_C_code_in_pragma is
			   TRUE. */

EXTERN a_boolean
                recognize_keywords_in_pragma;
                        /* TRUE if we are saving the tokens of a pragma in
			   a token cache and keywords should be recognized. */

EXTERN a_boolean
		do_string_literal_concatenation;
			/* TRUE if adjacent string literal tokens should be
			   concatenated.  Considered only if fetch_pp_tokens
			   is FALSE. */
EXTERN a_boolean
		in_pp_if_expression;
			/* TRUE if we are currently inside the expression of
			   a #if.  When TRUE, integer constants get an
			   implied "L" suffix, undefined identifiers
			   are taken to be 0L, and the "defined" operator
			   is enabled. */
EXTERN a_boolean
		exp_header_name;
			/* TRUE means that a header name (name on a #include)
			   of the form "..." is expected next, and tells
			   get_token to scan accordingly.  exp_header_name
			   is valid even when in_preprocessing_directive is
			   FALSE (it's always FALSE in that case).  */
EXTERN a_boolean
		exp_system_header_name;
			/* TRUE means that a header name (name on a #include)
			   of the form <...> is expected next, and tells
			   get_token to scan accordingly.
			   exp_system_header_name is valid even when
			   in_preprocessing_directive is FALSE (it's always
			   FALSE in that case).  */
EXTERN a_boolean
		exp_digit_sequence;
			/* TRUE means that a digit-sequence
			   is expected next, and tells get_token to scan
			   accordingly.  exp_digit_sequence is valid even
			   when in_preprocessing_directive is FALSE (it's
			   always FALSE in that case).  */
EXTERN a_boolean
		do_not_put_curr_line_in_pp_output;
			/* When TRUE, the current line should not be
			   put out as preprocessing output (probably because
			   it's a directive, or skipped over by a #if).
			   Meaningful when generate_pp_output is TRUE.
			   Note that when this flag is TRUE, the line is
			   entirely deleted, rather than put out as a blank
			   line.  Also TRUE when there is no current line,
			   as at the start and end of the source. */
EXTERN a_boolean
		pass_pp_directive_to_output;
			/* When TRUE, the current preprocessing directive
			   should be passed unchanged to preprocessing output,
			   so that some later processor (like a compiler)
			   can handle it. */
EXTERN a_seq_number
		next_seq_in_pp_output;
			/* Indicates the sequence number associated with
			   the next line to be put out in preprocessing output.
			   This is used to control the output of
			   line-identifying directives.  Meaningful when
			   generate_pp_output is TRUE. */
EXTERN a_boolean
		prev_pp_output_line_was_complete;
			/* TRUE if the previous line of preprocessing
			   output ended with a newline, i.e., it was not a
			   partial line caused by macro expansion or a
			   multi-line preprocessing directive being passed
			   unchanged to output. */
EXTERN a_boolean
		currently_in_pp_if_skip;
			/* If TRUE, we are currently skipping lines
			   because of an #if or the like. */
EXTERN a_boolean
		some_error_in_curr_directive;
			/* TRUE if some error has been detected in the
			   current preprocessing directive.  This means
			   specifically some syntax error that might
			   prevent complete successful scanning of the
			   directive, not something like the directive
			   appearing out of sequence. */
EXTERN long	pp_if_stack_depth;
			/* Stack of currently active #if, #ifdef, and
			   #ifndef directives.  pp_if_stack_depth
			   is the index of the currently active entry.
			   pp_if_stack_depth == -1 for an empty stack. */
EXTERN long	base_pp_if_stack_depth;
			/* The value of pp_if_stack_depth at entry to
			   the current file; important because in ANSI C,
			   each #if must be closed within the file in
			   which it was opened.  In pcc mode, always -1. */

EXTERN sizeof_t	size_pp_dir_string_buffer;
			/* Current allocated size of
                           pp_dir_string_buffer.  Not per-file.
                           See preproc.c for the definition of
			   pp_dir_string_buffer. */

/* Scan a preprocessing directive. */
extern void pp_directive(void);
/* Verify that all #ifs are closed at end of source. */
extern void verify_that_all_pp_ifs_were_closed(void);
/* Driver for mode where compiler just does preprocessing, like cpp. */
extern void cpp_driver(void);

extern void process_macro_preinclude(void);

#if IDENT_DIRECTIVE_AND_PRAGMA
extern void ident_pragma(a_pending_pragma_ptr ppp);
#endif /* IDENT_DIRECTIVE_AND_PRAGMA */

extern a_pragma_kind_description_ptr look_up_pragma_id(
					a_source_position	*id_position);

extern
void record_pragma(a_pragma_kind_description_ptr	pkdp,
		   a_source_position			*start_of_dir_position,
		   a_source_position			*id_position);

extern void stdc_pragma(a_pending_pragma_ptr	ppp);

extern void check_for_stdc_pragmas(void);

#if UPC_EXTENSIONS_ALLOWED
extern void check_for_upc_pragmas(a_statement_ptr  sp);

extern void upc_pragma(a_pending_pragma_ptr  ppp);
#endif /* UPC_EXTENSIONS_ALLOWED */

extern void once_pragma(a_pragma_kind kind);

extern void hdrstop_or_no_pch_pragma(a_pragma_kind kind);

extern void preproc_one_time_init(void);

extern void preproc_trans_unit_init(void);

extern void preproc_init(void);

#endif /* ifndef PREPROC_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
