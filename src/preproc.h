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

preproc.h -- Declarations related to preproc.c (having to do with
             preprocessing directives).

*/

/* Avoid including these declarations more than once: */
#ifndef PREPROC_H
#define PREPROC_H 1

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */

#ifndef LEXICAL_H
#include "lexical.h"
#endif /* ifndef LEXICAL_H */

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
		processing_C_code_in_pragma;
			/* TRUE is we are in a preprocessing directive
			   (typically a pragma) but we are processing what
			   is actually C/C++ code.  This disables some of
			   the special processing that is normally done when
		           is_preprocessing_directive is TRUE; specifically,
			   keyword recognition is enabled and the tokens
			   "#" and "##" are disabled. */
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
			   is expected next, and tells get_token to scan
			   accordingly.  exp_header_name is valid even
			   when in_preprocessing_directive is FALSE (it's
			   always FALSE in that case).  */
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
EXTERN int	pp_if_stack_depth;
			/* Stack of currently active #if, #ifdef, and
			   #ifndef directives.  pp_if_stack_depth
			   is the index of the currently active entry.
			   pp_if_stack_depth == -1 for an empty stack. */
EXTERN int	base_pp_if_stack_depth;
			/* The value of pp_if_stack_depth at entry to
			   the current file; important because in ANSI C,
			   each #if must be closed within the file in
			   which it was opened.  In pcc mode, always -1. */


/* Scan a preprocessing directive. */
extern void pp_directive(void);
/* Verify that all #ifs are closed at end of source. */
extern void verify_that_all_pp_ifs_were_closed(void);
/* Driver for mode where compiler just does preprocessing, like cpp. */
extern void cpp_driver(void);

#endif /* ifndef PREPROC_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1991 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
