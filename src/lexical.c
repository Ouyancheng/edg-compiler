/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

lexical.c -- Source input and lexical scanning routines.

These routines and data structures handle reading of source lines
and parsing of them into tokens.

*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Additional header files. */
#include "class_decl.h"
#include "decls.h"
#include "disambig.h"
#include "literals.h"
#include "macro.h"
#include "pch.h"
#include "pragma.h"
#include "preproc.h"
#include "symbol_ref.h"
#include "templates.h"
#if INCLUDE_COMMENTS_IN_ASM_FUNC_BODY
#include "func_def.h"
#endif /* INCLUDE_COMMENTS_IN_ASM_FUNC_BODY */
#ifdef lint
/* Include the definition of an_arg_operand to suppress lint errors. */
#include "exprutil.h"
#endif /* ifdef lint */


/*
Return TRUE if tok is a token kind that is a literal constant.
*/
#define is_literal_constant_token(tok)                                \
  (tok == tok_float_constant || tok == tok_int_constant ||            \
   tok == tok_char_constant  || tok == tok_string_literal ||	      \
   tok == tok_false          || tok == tok_true)


/*
Macro that returns TRUE if the curr_token_pragma list should not be
processed because we are currently processing tokens as part of a
preprocessing operation.
*/
#define suppress_pragma_processing					\
  (fetch_pp_tokens || in_preprocessing_directive)


/*
Macro that determines whether any of the initial special case tests
at the beginning of get_token need to be done.
*/
#define recalc_any_initial_get_token_tests_needed()			\
  (any_initial_get_token_tests_needed = curr_token_pragmas != NULL ||	\
                                        cached_token_rescan_list != NULL || \
                                        reusable_cache_stack != NULL)


/* Macro to check prevent calling the error checking function unless some
   error flags have been specified. */
#define check_for_generalized_identifier_errors(options, pos)		\
  (((options & GID_ERROR_FLAGS) != 0) ?					\
	f_check_for_generalized_identifier_errors(options, pos) : FALSE)

/* Macro to call the routine to check for template declarator errors, but
   only when the appropriate options have been specified. */
#define check_for_template_declarator_errors(options, pos)		\
  (((options &								\
     (GID_IS_TEMPLATE_DECLARATION | GID_IS_TEMPLATE_SPECIALIZATION)) != 0) ? \
	f_check_for_template_declarator_errors(options, pos) : FALSE)


/*
Entry used to represent the table of universal character names that
may be used as part of an identifier.

A sorted array of these entries is used when checking the validity of
a given universal character.
*/
typedef struct a_UCN_range *a_UCN_range_ptr;
typedef struct a_UCN_range {
  unsigned long
		start;
			/* The first character in the range. */
  unsigned long
		end;
			/* The last character in the range. */
  a_boolean	is_digit;
			/* TRUE if this is a digit, FALSE if it is some other
			   valid identifier character. */
} a_UCN_range;

/*
Sorted table of valid UCN ranges.  This is extracted from Annex D
of the C99 standard.  This table is used in both C++ and C99 modes
as it is an updated version of the table specified in the Annex E
of the C++ standard.
*/
static a_UCN_range
		UCN_table[] = {
  { 0x00aa, 0x00aa, FALSE }, 
  { 0x00b5, 0x00b5, FALSE }, 
  { 0x00b7, 0x00b7, FALSE }, 
  { 0x00ba, 0x00ba, FALSE }, 
  { 0x00c0, 0x00d6, FALSE }, 
  { 0x00d8, 0x00f6, FALSE }, 
  { 0x00f8, 0x01f5, FALSE }, 
  { 0x01fa, 0x0217, FALSE }, 
  { 0x0250, 0x02a8, FALSE }, 
  { 0x02b0, 0x02b8, FALSE }, 
  { 0x02bb, 0x02bb, FALSE }, 
  { 0x02bd, 0x02c1, FALSE }, 
  { 0x02d0, 0x02d1, FALSE }, 
  { 0x02e0, 0x02e4, FALSE }, 
  { 0x037a, 0x037a, FALSE }, 
  { 0x0386, 0x0386, FALSE }, 
  { 0x0388, 0x038a, FALSE }, 
  { 0x038c, 0x038c, FALSE }, 
  { 0x038e, 0x03a1, FALSE }, 
  { 0x03a3, 0x03ce, FALSE }, 
  { 0x03d0, 0x03d6, FALSE }, 
  { 0x03da, 0x03da, FALSE }, 
  { 0x03dc, 0x03dc, FALSE }, 
  { 0x03de, 0x03de, FALSE }, 
  { 0x03e0, 0x03e0, FALSE }, 
  { 0x03e2, 0x03f3, FALSE }, 
  { 0x0401, 0x040c, FALSE }, 
  { 0x040e, 0x044f, FALSE }, 
  { 0x0451, 0x045c, FALSE }, 
  { 0x045e, 0x0481, FALSE }, 
  { 0x0490, 0x04c4, FALSE }, 
  { 0x04c7, 0x04c8, FALSE }, 
  { 0x04cb, 0x04cc, FALSE }, 
  { 0x04d0, 0x04eb, FALSE }, 
  { 0x04ee, 0x04f5, FALSE }, 
  { 0x04f8, 0x04f9, FALSE }, 
  { 0x0531, 0x0556, FALSE }, 
  { 0x0559, 0x0559, FALSE }, 
  { 0x0561, 0x0587, FALSE }, 
  { 0x05b0, 0x05b9, FALSE }, 
  { 0x05bb, 0x05bd, FALSE }, 
  { 0x05bf, 0x05bf, FALSE }, 
  { 0x05c1, 0x05c2, FALSE }, 
  { 0x05d0, 0x05ea, FALSE }, 
  { 0x05f0, 0x05f2, FALSE }, 
  { 0x0621, 0x063a, FALSE }, 
  { 0x0640, 0x0652, FALSE }, 
  { 0x0660, 0x0669, TRUE }, 
  { 0x0670, 0x06b7, FALSE }, 
  { 0x06ba, 0x06be, FALSE }, 
  { 0x06c0, 0x06ce, FALSE }, 
  { 0x06d0, 0x06dc, FALSE }, 
  { 0x06e5, 0x06e8, FALSE }, 
  { 0x06ea, 0x06ed, FALSE }, 
  { 0x06f0, 0x06f9, TRUE }, 
  { 0x0901, 0x0903, FALSE }, 
  { 0x0905, 0x0939, FALSE }, 
  { 0x093d, 0x093d, FALSE }, 
  { 0x093e, 0x094d, FALSE }, 
  { 0x0950, 0x0952, FALSE }, 
  { 0x0958, 0x0963, FALSE }, 
  { 0x0966, 0x096f, TRUE }, 
  { 0x0981, 0x0983, FALSE }, 
  { 0x0985, 0x098c, FALSE }, 
  { 0x098f, 0x0990, FALSE }, 
  { 0x0993, 0x09a8, FALSE }, 
  { 0x09aa, 0x09b0, FALSE }, 
  { 0x09b2, 0x09b2, FALSE }, 
  { 0x09b6, 0x09b9, FALSE }, 
  { 0x09be, 0x09c4, FALSE }, 
  { 0x09c7, 0x09c8, FALSE }, 
  { 0x09cb, 0x09cd, FALSE }, 
  { 0x09dc, 0x09dd, FALSE }, 
  { 0x09df, 0x09e3, FALSE }, 
  { 0x09e6, 0x09ef, TRUE }, 
  { 0x09f0, 0x09f1, FALSE }, 
  { 0x0a02, 0x0a02, FALSE }, 
  { 0x0a05, 0x0a0a, FALSE }, 
  { 0x0a0f, 0x0a10, FALSE }, 
  { 0x0a13, 0x0a28, FALSE }, 
  { 0x0a2a, 0x0a30, FALSE }, 
  { 0x0a32, 0x0a33, FALSE }, 
  { 0x0a35, 0x0a36, FALSE }, 
  { 0x0a38, 0x0a39, FALSE }, 
  { 0x0a3e, 0x0a42, FALSE }, 
  { 0x0a47, 0x0a48, FALSE }, 
  { 0x0a4b, 0x0a4d, FALSE }, 
  { 0x0a59, 0x0a5c, FALSE }, 
  { 0x0a5e, 0x0a5e, FALSE }, 
  { 0x0a66, 0x0a6f, TRUE }, 
  { 0x0a74, 0x0a74, FALSE }, 
  { 0x0a81, 0x0a83, FALSE }, 
  { 0x0a85, 0x0a8b, FALSE }, 
  { 0x0a8d, 0x0a8d, FALSE }, 
  { 0x0a8f, 0x0a91, FALSE }, 
  { 0x0a93, 0x0aa8, FALSE }, 
  { 0x0aaa, 0x0ab0, FALSE }, 
  { 0x0ab2, 0x0ab3, FALSE }, 
  { 0x0ab5, 0x0ab9, FALSE }, 
  { 0x0abd, 0x0ac5, FALSE }, 
  { 0x0ac7, 0x0ac9, FALSE }, 
  { 0x0acb, 0x0acd, FALSE }, 
  { 0x0ad0, 0x0ad0, FALSE }, 
  { 0x0ae0, 0x0ae0, FALSE }, 
  { 0x0ae6, 0x0aef, TRUE }, 
  { 0x0b01, 0x0b03, FALSE }, 
  { 0x0b05, 0x0b0c, FALSE }, 
  { 0x0b0f, 0x0b10, FALSE }, 
  { 0x0b13, 0x0b28, FALSE }, 
  { 0x0b2a, 0x0b30, FALSE }, 
  { 0x0b32, 0x0b33, FALSE }, 
  { 0x0b36, 0x0b39, FALSE }, 
  { 0x0b3d, 0x0b3d, FALSE }, 
  { 0x0b3e, 0x0b43, FALSE }, 
  { 0x0b47, 0x0b48, FALSE }, 
  { 0x0b4b, 0x0b4d, FALSE }, 
  { 0x0b5c, 0x0b5d, FALSE }, 
  { 0x0b5f, 0x0b61, FALSE }, 
  { 0x0b66, 0x0b6f, TRUE }, 
  { 0x0b82, 0x0b83, FALSE }, 
  { 0x0b85, 0x0b8a, FALSE }, 
  { 0x0b8e, 0x0b90, FALSE }, 
  { 0x0b92, 0x0b95, FALSE }, 
  { 0x0b99, 0x0b9a, FALSE }, 
  { 0x0b9c, 0x0b9c, FALSE }, 
  { 0x0b9e, 0x0b9f, FALSE }, 
  { 0x0ba3, 0x0ba4, FALSE }, 
  { 0x0ba8, 0x0baa, FALSE }, 
  { 0x0bae, 0x0bb5, FALSE }, 
  { 0x0bb7, 0x0bb9, FALSE }, 
  { 0x0bbe, 0x0bc2, FALSE }, 
  { 0x0bc6, 0x0bc8, FALSE }, 
  { 0x0bca, 0x0bcd, FALSE }, 
  { 0x0be7, 0x0bef, TRUE }, 
  { 0x0c01, 0x0c03, FALSE }, 
  { 0x0c05, 0x0c0c, FALSE }, 
  { 0x0c0e, 0x0c10, FALSE }, 
  { 0x0c12, 0x0c28, FALSE }, 
  { 0x0c2a, 0x0c33, FALSE }, 
  { 0x0c35, 0x0c39, FALSE }, 
  { 0x0c3e, 0x0c44, FALSE }, 
  { 0x0c46, 0x0c48, FALSE }, 
  { 0x0c4a, 0x0c4d, FALSE }, 
  { 0x0c60, 0x0c61, FALSE }, 
  { 0x0c66, 0x0c6f, TRUE }, 
  { 0x0c82, 0x0c83, FALSE }, 
  { 0x0c85, 0x0c8c, FALSE }, 
  { 0x0c8e, 0x0c90, FALSE }, 
  { 0x0c92, 0x0ca8, FALSE }, 
  { 0x0caa, 0x0cb3, FALSE }, 
  { 0x0cb5, 0x0cb9, FALSE }, 
  { 0x0cbe, 0x0cc4, FALSE }, 
  { 0x0cc6, 0x0cc8, FALSE }, 
  { 0x0cca, 0x0ccd, FALSE }, 
  { 0x0cde, 0x0cde, FALSE }, 
  { 0x0ce0, 0x0ce1, FALSE }, 
  { 0x0ce6, 0x0cef, TRUE }, 
  { 0x0d02, 0x0d03, FALSE }, 
  { 0x0d05, 0x0d0c, FALSE }, 
  { 0x0d0e, 0x0d10, FALSE }, 
  { 0x0d12, 0x0d28, FALSE }, 
  { 0x0d2a, 0x0d39, FALSE }, 
  { 0x0d3e, 0x0d43, FALSE }, 
  { 0x0d46, 0x0d48, FALSE }, 
  { 0x0d4a, 0x0d4d, FALSE }, 
  { 0x0d60, 0x0d61, FALSE }, 
  { 0x0d66, 0x0d6f, TRUE }, 
  { 0x0e01, 0x0e3a, FALSE }, 
  { 0x0e47, 0x0e4e, FALSE }, 
  { 0x0e50, 0x0e59, TRUE }, 
  { 0x0e81, 0x0e82, FALSE }, 
  { 0x0e84, 0x0e84, FALSE }, 
  { 0x0e87, 0x0e88, FALSE }, 
  { 0x0e8a, 0x0e8a, FALSE }, 
  { 0x0e8d, 0x0e8d, FALSE }, 
  { 0x0e94, 0x0e97, FALSE }, 
  { 0x0e99, 0x0e9f, FALSE }, 
  { 0x0ea1, 0x0ea3, FALSE }, 
  { 0x0ea5, 0x0ea5, FALSE }, 
  { 0x0ea7, 0x0ea7, FALSE }, 
  { 0x0eaa, 0x0eab, FALSE }, 
  { 0x0ead, 0x0eae, FALSE }, 
  { 0x0eb0, 0x0eb9, FALSE }, 
  { 0x0ebb, 0x0ebd, FALSE }, 
  { 0x0ec0, 0x0ec4, FALSE }, 
  { 0x0ec6, 0x0ec6, FALSE }, 
  { 0x0ec8, 0x0ecd, FALSE }, 
  { 0x0ed0, 0x0ed9, TRUE }, 
  { 0x0edc, 0x0edd, FALSE }, 
  { 0x0f00, 0x0f00, FALSE }, 
  { 0x0f18, 0x0f19, FALSE }, 
  { 0x0f20, 0x0f33, TRUE }, 
  { 0x0f35, 0x0f35, FALSE }, 
  { 0x0f37, 0x0f37, FALSE }, 
  { 0x0f39, 0x0f39, FALSE }, 
  { 0x0f3e, 0x0f47, FALSE }, 
  { 0x0f49, 0x0f69, FALSE }, 
  { 0x0f71, 0x0f84, FALSE }, 
  { 0x0f86, 0x0f8b, FALSE }, 
  { 0x0f90, 0x0f95, FALSE }, 
  { 0x0f97, 0x0f97, FALSE }, 
  { 0x0f99, 0x0fad, FALSE }, 
  { 0x0fb1, 0x0fb7, FALSE }, 
  { 0x0fb9, 0x0fb9, FALSE }, 
  { 0x10a0, 0x10c5, FALSE }, 
  { 0x10d0, 0x10f6, FALSE }, 
  { 0x1e00, 0x1e9b, FALSE }, 
  { 0x1ea0, 0x1ef9, FALSE }, 
  { 0x1f00, 0x1f15, FALSE }, 
  { 0x1f18, 0x1f1d, FALSE }, 
  { 0x1f20, 0x1f45, FALSE }, 
  { 0x1f48, 0x1f4d, FALSE }, 
  { 0x1f50, 0x1f57, FALSE }, 
  { 0x1f59, 0x1f59, FALSE }, 
  { 0x1f5b, 0x1f5b, FALSE }, 
  { 0x1f5d, 0x1f5d, FALSE }, 
  { 0x1f5f, 0x1f7d, FALSE }, 
  { 0x1f80, 0x1fb4, FALSE }, 
  { 0x1fb6, 0x1fbc, FALSE }, 
  { 0x1fbe, 0x1fbe, FALSE }, 
  { 0x1fc2, 0x1fc4, FALSE }, 
  { 0x1fc6, 0x1fcc, FALSE }, 
  { 0x1fd0, 0x1fd3, FALSE }, 
  { 0x1fd6, 0x1fdb, FALSE }, 
  { 0x1fe0, 0x1fec, FALSE }, 
  { 0x1ff2, 0x1ff4, FALSE }, 
  { 0x1ff6, 0x1ffc, FALSE }, 
  { 0x203f, 0x2040, FALSE }, 
  { 0x207f, 0x207f, FALSE }, 
  { 0x2102, 0x2102, FALSE }, 
  { 0x2107, 0x2107, FALSE }, 
  { 0x210a, 0x2113, FALSE }, 
  { 0x2115, 0x2115, FALSE }, 
  { 0x2118, 0x211d, FALSE }, 
  { 0x2124, 0x2124, FALSE }, 
  { 0x2126, 0x2126, FALSE }, 
  { 0x2128, 0x2128, FALSE }, 
  { 0x212a, 0x2131, FALSE }, 
  { 0x2133, 0x2138, FALSE }, 
  { 0x2160, 0x2182, FALSE }, 
  { 0x3005, 0x3007, FALSE }, 
  { 0x3021, 0x3029, FALSE }, 
  { 0x3041, 0x3093, FALSE }, 
  { 0x309b, 0x309c, FALSE }, 
  { 0x30a1, 0x30f6, FALSE }, 
  { 0x30fb, 0x30fc, FALSE }, 
  { 0x3105, 0x312c, FALSE }, 
  { 0x4e00, 0x9fa5, FALSE }, 
  { 0xac00, 0xd7a3, FALSE }
};

/*
Variables pertaining to the input stack (for include files and the
primary source file) and the current input file (the top entry on the
stack).
*/
static an_input_stack_entry_ptr
		input_stack = NULL;
			/* Input stack, one entry for each active source 
			   file.  Entry [0] is for the primary source file.
			   Dynamically allocated, reallocated if necessary;
			   size_input_stack gives the current allocated
			   size.  Not per-file. */
static int	size_input_stack = 0;
			/* Allocated size of input_stack, in elements not
			   bytes.  Not per-file. */
#define INPUT_STACK_INCREMENTAL_ALLOCATION 30
			/* The number of elements added to input_stack each
			   time it is reallocated; also the initial
			   allocation. */
static int	depth_input_stack;
			/* Depth of the input stack, minus 1.
			   input_stack[depth_input_stack] is the active
			   entry.  -1 if the stack is completely empty. */
static FILE
		*curr_input_stream;
			/* The currently active source input stream. */
static a_boolean
		eof_read_on_curr_input_stream;
			/* TRUE if end of file has been reached on the
			   current source stream.  This can indicate the
			   end of an include file or the end of the primary
			   source file.  This is an internal variable
			   indicating a physical end of file, and should not
			   be confused with the logical end of file variables
			   like after_end_of_all_source. */
/*
Variables related to the current source line (see lexical.h):
*/
static a_boolean
		at_end_of_source_file;
			/* If TRUE, there is no current logical source line;
			   the last attempt to read one ran into an end of
			   file instead.  This may, however, be only the end
			   of an include file, and not of the entire source
			   sequence (see pop_input_stack).  The current
			   position is at the end of the current input file,
			   but still within it -- the stack has not yet been
			   popped.  curr_source_line still contains the line
			   most recently read. */
static a_boolean
		after_end_of_all_source;
			/* If TRUE, there is no current logical source line;
			   all source has been read, all files popped off the
			   input stack.  The current sequence number indicates
			   a position one line beyond the last line actually
			   read.  We are no longer inside any source file; we
			   are past the end of the primary file. */
static a_boolean
		any_tokens_gotten_from_curr_source_line;
			/* If TRUE, one or more tokens (normal or
			   preprocessor) have been gotten from the current
			   source line.  If FALSE, nothing (except possibly
			   white space) has been gotten from that line.
			   This is used in determining whether or not a "#"
			   is the start of a preprocessing directive. */
static a_boolean
		no_modifs_to_curr_source_line;
			/* TRUE if source_line_modif_list == NULL and
			   orig_line_modif_list == NULL.  Used as a speed
			   optimization in deciding whether or not one can
			   use the fast method of converting a line location
			   into a source position. */
static a_boolean
		init_do_not_put_curr_line_in_pp_output;
			/* The value of do_not_put_curr_line_in_pp_output
			   that applies to any text inserted at the beginning
			   of the current source line.  Such text may have to
			   be put out even if the main line is suppressed
			   (e.g., if the inserted text is an inert macro
			   identifier and the new line is a preprocessing
			   directive). */
static char	curr_raw_listing_line_code;
			/* When a raw listing file is being generated,
			   this indicates the code for the current line:
			   'S' for a line skipped by an if-skip, 'N' for
			   a normal line, or '\0' if there is no current 
			   line. */
static	char	*raw_listing_buffer = NULL;
			/* Buffer used in writing the macro-expanded versions
			   of source lines to the raw listing file.  Space is
			   dynamically allocated, and its upper bound is given
			   by after_end_of_raw_listing_buffer.
			   See lexical_init for the initial allocation. */
#define RAW_LISTING_BUFFER_INITIAL_ALLOCATION 3000
#define RAW_LISTING_BUFFER_INCREMENTAL_ALLOCATION 5000
			/* Initial and incremental allocation sizes for
			   raw_listing_buffer.  Should probably match the
			   corresponding constants for curr_source_line. */
static char	*after_end_of_raw_listing_buffer = NULL;
			/* Address past the last element of raw_listing_buffer,
			   as an aid to checking for overflow, etc.  A variable
			   because raw_listing_buffer can be reallocated larger
			   if needed. */
static char	*loc_in_raw_listing_buffer;
			/* Current output location in raw_listing_buffer. */
static a_boolean
		must_display_raw_listing_buffer;
			/* TRUE if raw_listing_buffer contains a line or lines
			   modified in nontrivial ways (comments are considered
			   trivial modifications).  If so, it must be
			   displayed. */


/*
Information about cached tokens, i.e., tokens saved for later rescanning.
*/
static a_cached_token_ptr
		cached_token_rescan_list;
			/* If non-NULL, points to a list of cached tokens
			   that should be rescanned by get_token before going
			   forward in the input stream. */
static a_cached_token_ptr
		avail_cached_tokens;
			/* List of cached token entries (allocated in front
			   end storage) freed and available for reuse. */
static a_constant_ptr
		avail_cached_constants;
			/* List of constant entries (allocated in front end
			   storage) for use with token caching entries,
			   freed and available for reuse. */

/*
Information about data structures used for working with persistent token
caches.
*/

static a_reusable_cache_entry_ptr
		avail_reusable_cache_entries;
			/* List of reusable cache stack entries (allocated
                           in front end storage) freed and available for
                           reuse. */

static a_reusable_cache_entry_ptr
                reusable_cache_stack;
                        /* The stack of reusable caches that are currently
                           active. */

static a_stop_token_stack_entry_ptr
		avail_stop_token_stack_entries;
			/* List of stop token stack entries that have been
			   freed and are available for reuse. */

#if MICROSOFT_EXTENSIONS_ALLOWED
static a_boolean
		scanning_microsoft_asm;
			/* TRUE while scanning a Microsoft asm. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Flag that indicates whether a dollar sign was found in any identifiers.
Used in strict ANSI mode to make sure that this diagnostic is only given
once per compilation unit.
*/
static a_boolean
		dollar_in_id_diagnostic_issued;

/*
Head of a list of history information about include files that have
been processed.  Used to suppress subsequence inclusions of the same
file.
*/
static an_include_file_history_ptr
		include_file_history_list;

/*
Array of identifier lookup options indexed by identifier lookup mode.  Used
to translate the lookup mode into a set of identifier lookup options.
*/
static an_id_lookup_options_set idl_options_for_lookup_mode[(int)ilm_last+1]= {
  /* ilm_normal */		IDL_NO_OPTIONS,
  /* ilm_tag */			IDL_MUST_BE_TAG,
  /* ilm_tentative_type */	IDL_TENTATIVE_TYPE_LOOKUP,
  /* ilm_ctor_initializer_name */
                                IDL_SKIP_CURR_SCOPE,
  /* ilm_qualified_ctor_initializer_name */
                                IDL_SKIP_CURR_SCOPE | IDL_MUST_BE_CLASS,
  /* ilm_namespace */           IDL_MUST_BE_NAMESPACE,
  /* ilm_typename */            IDL_TYPENAME_LOOKUP,
  /* ilm_class */  	        IDL_MUST_BE_CLASS,
  /* ilm_template_linkage */	IDL_LINKAGE_LOOKUP | IDL_TREAT_AS_TEMPLATE_ID,
  /* ilm_using_declaration */  	IDL_USING_DECLARATION,
  /* ilm_using_typename */      IDL_USING_DECLARATION | IDL_TYPENAME_LOOKUP,
  /* ilm_expr */		IDL_IS_EXPR_CONTEXT,
  /* ilm_last */		IDL_NO_OPTIONS
};

/*
Data structure used to represent a list of file suffixes.
*/
typedef struct a_file_suffix *a_file_suffix_ptr;
typedef struct a_file_suffix {
  a_file_suffix_ptr
                next;
                        /* Pointer to the next entry on the list. */
  char
                *suffix;
			/* Pointer to a null terminated string containing
			   the suffix.  It does not contain any delimiter
			   to be used between the filename and the suffix. */
} a_file_suffix;

#if INSTANTIATION_BY_IMPLICIT_INCLUSION

static a_file_suffix_ptr
		 implicit_instantiation_file_suffix_list = NULL;

#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */

static a_file_suffix_ptr
		 include_file_suffix_list = NULL;
			/* List of file suffixes used when searching for a
			   header file whose name does not include a suffix. */

#if DEBUG
/*
Counts of tables allocated, to track total use of memory.
*/
static unsigned long
		num_orig_line_modifs_allocated,
		num_source_line_modifs_allocated,
		num_cached_tokens_allocated,
                num_cached_tokens_in_reusable_caches,
                num_pragmas_in_reusable_caches,
		num_cached_constants_allocated,
                num_file_suffixes_allocated,
		num_include_file_histories_allocated,
		cached_pp_token_string_space,
                num_stop_token_stack_entries_allocated,
		num_reusable_cache_entries_allocated;
#endif /* DEBUG */


static void unimplemented_keyword_diagnostic(a_symbol_ptr  sym)
/*
Issue a diagnostic on unimplemented keywords.
*/
{
  if (sym->variant.keyword.diagnostic_issued_if_used != ec_no_error) {
    an_error_severity severity;
    severity = strict_ansi_mode ? strict_ansi_error_severity : es_remark;
    sym_diagnostic(severity,
		   sym->variant.keyword.diagnostic_issued_if_used, sym);
    sym->variant.keyword.diagnostic_issued_if_used = ec_no_error;
  }  /* if */
}  /* unimplemented_keyword_diagnostic */


void clear_token_cache(a_token_cache *cache,
		       a_boolean     reusable)
/*
Initialize a token cache, presumably so tokens can be added to it.
*/
{
  cache->first_token = NULL;
  cache->last_token  = NULL;
  cache->is_reusable = reusable;
#if DEBUG
  cache->token_count = 0;
  cache->pragma_count = 0;
#endif /* DEBUG */
}  /* clear_token_cache */


#if DEBUG
#define incr_num_cached_tokens_allocated() num_cached_tokens_allocated++;
#else /* !DEBUG */
#define incr_num_cached_tokens_allocated() /* Nothing */
#endif /* DEBUG */

/*
Macro to allocate a cached token entry or, if possible, to reuse a freed
entry.  Note that some of the fields of the allocated entry are initialized
by the caller (including token, source_position, and extra_info_kind).
*/
#define alloc_cached_token(ctp)                                         \
{ if (avail_cached_tokens != NULL) {                                    \
    /* Reuse a freed entry. */                                          \
    ctp = avail_cached_tokens;                                          \
    avail_cached_tokens = avail_cached_tokens->next;                    \
  } else {                                                              \
    /* Allocate a new entry. */                                         \
    ctp = (a_cached_token_ptr)alloc_fe(sizeof(a_cached_token));         \
    incr_num_cached_tokens_allocated();                                 \
  }  /* if */                                                           \
  ctp->next = NULL;                                                     \
}  /* alloc_cached_token */


static a_reusable_cache_entry_ptr alloc_reusable_cache_entry(void)
/*
Allocate a reusable cache entry.  Reuse a freed entry if possible.
*/
{
  a_reusable_cache_entry_ptr rsep;

  if (avail_reusable_cache_entries != NULL) {
    /* Reuse a freed entry. */
    rsep = avail_reusable_cache_entries;
    avail_reusable_cache_entries = avail_reusable_cache_entries->next;
  } else {
    /* Allocate a new entry. */
    rsep = (a_reusable_cache_entry_ptr)
                                 alloc_fe(sizeof(a_reusable_cache_entry));
#if DEBUG
    num_reusable_cache_entries_allocated++;
#endif /* DEBUG */
  }  /* if */
  rsep->next = NULL;
  rsep->previous_token_rescan_list = NULL;
  rsep->next_cached_token = NULL;
  return rsep;
}  /* alloc_reusable_cache_entry */


static a_constant_ptr alloc_cached_constant(void)
/*
Allocate a cached constant entry.  Reuse a freed entry if possible.
*/
{
  a_constant_ptr cp;

  if (avail_cached_constants != NULL) {
    /* Reuse a freed entry. */
    cp = avail_cached_constants;
    avail_cached_constants = avail_cached_constants->next;
  } else {
    /* Allocate a new entry. */
    cp = (a_constant_ptr)alloc_fe(sizeof(a_constant));
#if DEBUG
    num_cached_constants_allocated++;
#endif /* DEBUG */
  }  /* if */
  return cp;
}  /* alloc_cached_constant */


/*
Macro used to update the counter of tokens used in reusable caches
and the number of tokens used in the given cache.
When debugging code is not being generated the macro expands to nothing.
*/
#if DEBUG
#define incr_tokens_in_cache(cache)					\
  if (cache->is_reusable) {						\
    num_cached_tokens_in_reusable_caches++;				\
  }  /* if */								\
  cache->token_count++;
#else /* !DEBUG */
#define incr_tokens_in_cache(cache)  /* */
#endif /* DEBUG */

/*
Add the cached token pointed to by ctp to the end of the token cache
pointed to by cache.  
*/
#define add_cached_token_to_cache(ctp, cache)                         \
{ if (cache->first_token == NULL) {                                   \
    cache->first_token = ctp;                                         \
  } else {                                                            \
    cache->last_token->next = ctp;                                    \
  }  /* if */                                                         \
  cache->last_token = ctp;                                            \
  incr_tokens_in_cache(cache);					      \
}  /* add_cached_token_to_cache */


static void add_pragma_entry_to_cache(a_token_cache *cache)
/*
Add a pragma entry to the end of the indicated token cache for the pragmas
associated with the current token.
*/
{
  a_cached_token_ptr ctp;

  alloc_cached_token(ctp);
  check_assertion(curr_token_pragmas != NULL);
  ctp->extra_info_kind = (a_token_extra_info_kind)teik_pragma;
  ctp->variant.pragmas = curr_token_pragmas;
  ctp->source_position = curr_token_pragmas->pragma_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  /* An ending position is not maintained for pragmas so the start position
     is used. */
  ctp->end_source_position = ctp->source_position;
#endif /*  EXTRA_SOURCE_POSITIONS_IN_IL */
  ctp->token = (a_byte_token_kind)tok_error;
  ctp->token_sequence_number = NO_TOKEN_SEQUENCE_NUMBER;
  add_cached_token_to_cache(ctp, cache);
#if DEBUG
  /* Increment the number of pragmas in reusable caches and the number of
     pragmas in this particular cache. */
  { a_pending_pragma_ptr	ppp = curr_token_pragmas;
    unsigned long		count = 0;
    while (ppp != NULL) {
      count++;
      ppp = ppp->next;
    }  /* while */
    if (cache->is_reusable) num_pragmas_in_reusable_caches += count;
    cache->pragma_count += count;
  }
#endif /* DEBUG */
}  /* add_pragma_entry_to_cache */


/*
Macro to free a cached token entry, i.e., to put it on the avail list to be
reused.  If the entry points to a cached constant entry, free it, too.
It is expected that no pragma entries will be pointed to at the time
the cached token is freed.
*/
#define free_cached_token(ctp)                                          \
{ if (ctp->extra_info_kind == (a_token_extra_info_kind)teik_constant) { \
    /* The entry points to a constant entry; free it. */                \
    a_constant_ptr con = ctp->variant.constant;                         \
    con->next = avail_cached_constants;                                 \
    avail_cached_constants = con;                                       \
  }  /* if */                                                           \
  ctp->next = avail_cached_tokens;                                      \
  avail_cached_tokens = ctp;                                            \
}  /* free_cached_token */


#if !DEBUG
/*ARGSUSED*/ /* <-- because "token_cache" is only used in debug code. */
#endif /* !DEBUG */
static void free_cached_token_from_reusable_cache(
				a_token_cache_ptr  token_cache,
                                a_cached_token_ptr ctp,
                                a_boolean	   keep_pragma_tokens)
/*
Free an individual token from a reusable cache.  keep_pragma_tokens is TRUE
when the token caches associated with pragma entries should be retained.
This is needed when freeing tokens from the original copies of member function
bodies of class templates.
*/
{
  /* Free any pragmas associated with this token.  Cached constants will
     be freed by free_cached_token. */
  if (ctp->extra_info_kind == (a_token_extra_info_kind)teik_pragma) {
    /* Free any pragma entries associated with this token. */
    a_pending_pragma_ptr	ppp = ctp->variant.pragmas;
    while (ppp != NULL) {
      a_pending_pragma_ptr	next_ppp = ppp->next;
#if DEBUG
      num_pragmas_in_reusable_caches--;
#endif /* DEBUG */
      if (keep_pragma_tokens) ppp->discard_cache_when_done = FALSE;
      free_pending_pragma(ppp);
      ppp = next_ppp;
    }  /* while */
    ctp->variant.pragmas = NULL;
  }  /* if */
#if DEBUG
  num_cached_tokens_in_reusable_caches--;
  token_cache->token_count--;
#endif /* DEBUG */
  free_cached_token(ctp);
}  /* free_cached_token_from_reusable_cache */


void free_tokens_from_reusable_cache(a_cached_token_ptr	ctp,
				     a_token_cache	*cache)
/*
Free a list of cached tokens from the reusable cache specified by
cache.
*/
{
  while (ctp != NULL) {
    a_cached_token_ptr	next_ctp = ctp->next;
    free_cached_token_from_reusable_cache(cache, ctp,
                                         /*keep_pragma_tokens=*/TRUE);
    ctp = next_ctp;
  }  /* while */
}  /* free_tokens_from_reusable_cache */


void terminate_token_cache(a_token_cache *cache)
/*
Save an end-of-source token on the end of the list of tokens saved in *cache.
*/
{
  a_cached_token_ptr ctp;

  /* Build an entry for the end-of-source token. */
  alloc_cached_token(ctp);
  ctp->source_position = pos_curr_token;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  ctp->end_source_position = pos_curr_token;
#endif /*  EXTRA_SOURCE_POSITIONS_IN_IL */
  ctp->token = (a_byte_token_kind)tok_end_of_source;
  ctp->token_sequence_number = NO_TOKEN_SEQUENCE_NUMBER;
  ctp->extra_info_kind = (a_token_extra_info_kind)teik_none;
  /* Add the end-of-source token to the end of the cache. */
  add_cached_token_to_cache(ctp, cache);
}  /* terminate_token_cache */


void remove_cache_terminator(a_token_cache *cache)
/*
Remove the terminator token from the token cache so that additional
tokens may be added to it.
*/
{ 
  a_cached_token_ptr	ctp = cache->first_token;
  a_cached_token_ptr	prev_ctp = NULL;

  for (; ctp->next != NULL; ctp = ctp->next) {
    prev_ctp = ctp;
  }  /* for */
  cache->last_token = prev_ctp;
  prev_ctp->next = NULL;
  /* Free the terminator token. */
  free_cached_token_from_reusable_cache(cache, ctp,
                                        /*keep_pragma_tokens=*/FALSE);
}  /* remove_cache_terminator */


a_cached_token_ptr build_cached_token(a_token_kind	      kind,
                                      a_token_sequence_number sequence_number,
                                      a_source_position	      *position)
/*
Create a cached token of the specified token kind, with the specified
token sequence number and source position.  The token kind must be one
for which there is no associated extra information.  Return a pointer to
the newly created token.
*/
{
  a_cached_token_ptr ctp;

  /* Build an entry for the end-of-source token. */
  alloc_cached_token(ctp);
  ctp->token = (a_byte_token_kind)kind;
  ctp->token_sequence_number = sequence_number;
  ctp->source_position = *position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  /* This is a synthesized token, for which we are really using the position
     of another token.  Just use the same position for the start and end. */
  ctp->end_source_position = *position;
#endif /*  EXTRA_SOURCE_POSITIONS_IN_IL */
  ctp->extra_info_kind = (a_token_extra_info_kind)teik_none;
#if DEBUG
  /* For accounting purposes, assume that this token will be used in a
     reusable cache. */
  num_cached_tokens_in_reusable_caches++;
#endif /* DEBUG */
  return ctp;
}  /* build_cached_token */


static void make_copy_of_pp_token(a_pp_token_descr_ptr pptdp)
/*
Allocates memory and copies the token string indicated by
start_of_curr_token and end_of_curr_token to the newly allocated
memory.
*/
{
  char		*new_start;
  char		*new_end;
  sizeof_t	length;

  /* Compute the length of the string not including a null terminator
     that will be added.  The terminator is not required for most
     processing, but it is used by add_token_to_string. */
  length = end_of_curr_token - start_of_curr_token + 1;
  /* Allocate the string, adding space for the terminator. */
  new_start = (char *)alloc_fe(length+1);
#if DEBUG
  /* Track the amount of space used for pp token strings. */
  cached_pp_token_string_space += length+1;
#endif /* DEBUG */
  new_end = new_start + length - 1;
  strncpy(new_start, start_of_curr_token, size_t_arg(length));
  /* Add the terminator to the new string. */
  new_start[length] = '\0';
  pptdp->token_start = new_start;
  pptdp->token_end = new_end;
}  /* make_copy_of_pp_token */


void cache_curr_token(a_token_cache *cache)
/*
Save the current token on the end of the list of tokens saved in *cache.
This is used to save tokens for later rescanning.
*/
{
  a_cached_token_ptr	ctp;

  /* If there are any pragmas associated with the current token, create
     a token cache entry to preserve the pragma information before adding
     the token cache entry for the current token. */
  if (curr_token_pragmas != NULL && !suppress_pragma_processing) {
    add_pragma_entry_to_cache(cache);
    curr_token_pragmas = NULL;
  }  /* if */
  /* Build an entry for the current token itself. */
  alloc_cached_token(ctp);
  ctp->token = (a_byte_token_kind)curr_token;
  ctp->source_position = pos_curr_token;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  ctp->end_source_position = end_pos_curr_token;
#endif /*  EXTRA_SOURCE_POSITIONS_IN_IL */
  ctp->token_sequence_number = curr_token_sequence_number;
  if (fetch_pp_tokens) {
    /* The token being saved is a pp token.  Save this by copying the token
       string. */
    ctp->extra_info_kind = (a_token_extra_info_kind)teik_pp_token;
    make_copy_of_pp_token(&ctp->variant.pp_token_descr);
  } else if (curr_token == tok_identifier || curr_token == tok_ptr_to_member) {
    /* Identifier -- save information about it. */
    ctp->extra_info_kind = (a_token_extra_info_kind)teik_identifier;
    ctp->variant.locator = locator_for_curr_id;
  } else if (curr_token == tok_microsoft_asm) {
    ctp->extra_info_kind = (a_token_extra_info_kind)teik_asm_string;
    ctp->variant.asm_string = curr_token_asm_string;
  } else if (is_literal_constant_token(curr_token)) {
    /* Literal constant -- save the constant's value. */
    ctp->extra_info_kind = (a_token_extra_info_kind)teik_constant;
    ctp->variant.constant = alloc_cached_constant();
    /* Copy the constant.  Note that anything pointed to by the constant
       (e.g., a string) has been allocated in the file scope and doesn't
       need to be copied. */
    copy_constant(&const_for_curr_token, ctp->variant.constant);
  } else {
    /* No extra information needed for this token. */
    ctp->extra_info_kind = (a_token_extra_info_kind)teik_none;
  }  /* if */
  add_cached_token_to_cache(ctp, cache);
}  /* cache_curr_token */


static a_boolean is_template_reference(void)
/*
The current token is "<" and the previous token is an identifier.  Look
up the identifier as a class name.  If it is a class template name,
return TRUE, otherwise return FALSE.

This routine is used by cache_token_stream and flush_tokens when
caching (or flushing) until a matching token is found.  Template
references must be recognized so that matching tokens within
the template reference can be ignored.  Template references are
more difficult to recognize that other matching token pairs
(such as () or []) because not all less-than signs begin template
references.
*/
{
  a_boolean	result = FALSE;
  a_symbol_ptr  sym;

  if (!fetch_pp_tokens) {
    /* This test can only be done when not fetching preprocessing
       tokens.  It is not possible to do the ID lookup in fetch_pp_tokens
       mode. */
    sym = normal_id_lookup(&locator_for_curr_id,
                           IDL_DO_NOT_ADD_TO_NONREAL_CLASS);
    if (sym != NULL && is_class_template_or_injected_template_symbol(sym)) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_template_reference */


/*
Get a token and, if it is a tok_identifier, call
is_generalized_identifier_start to coalesce it in case it is the
beginning of something like a qualified name.  When coalescing identifiers,
make sure that we are not fetching tokens beyond the end of the cache
that contains the entire statement.  If an attempt is made to do so,
return tok_end_of_source.  When tok_end_of_source is returned, the
current token sequence number is set to the largest possible value to
ensure that, when the tokens are copied from the source cache to the
new cache, all tokens up to the end-of-cache marker will be copied.
*/
#define get_token_and_coalesce_if_needed(coalesce_ids, last_tsn_in_cache) \
  if (coalesce_ids) {						\
    if (curr_token_sequence_number >= last_tsn_in_cache) {		\
      curr_token = tok_end_of_source;					\
      curr_token_sequence_number = MAX_TOKEN_SEQUENCE_NUMBER;		\
    } else {								\
      (void)get_token();						\
      (void)is_generalized_identifier_start(GID_TEMPLATE_ARGS_OPTIONAL |\
					    GID_IS_EXPR_CONTEXT);	\
    }  /* if */								\
  } else {								\
    (void)get_token();							\
  }


static void copy_cached_token(a_cached_token_ptr	from_ctp,
			      a_cached_token_ptr	to_ctp)
/*
Make a copy of a cached token, including any constant or pragmas pointed
to by the token.
*/
{
  a_token_extra_info_kind	extra_info_kind;

  *to_ctp = *from_ctp;
  extra_info_kind = from_ctp->extra_info_kind;
  if (extra_info_kind == (a_token_extra_info_kind)teik_constant) {
    to_ctp->variant.constant = alloc_cached_constant();
    copy_constant(from_ctp->variant.constant, to_ctp->variant.constant);
  } else if (extra_info_kind == (a_token_extra_info_kind)teik_pragma) {
    to_ctp->variant.pragmas =
                           make_copy_of_pragma_list(from_ctp->variant.pragmas);
  }  /* if */
  to_ctp->next = NULL;
}  /* copy_cached_token */


static
void copy_tokens_from_cache(a_token_cache_ptr	       src_cache,
                            a_token_sequence_number    first_tsn,
                            a_token_sequence_number    last_tsn,
                            a_token_cache_ptr	       dest_cache)
/*
Copy the tokens from src_cache to dest_cache that are in the range
of token sequence numbers from first_tsn to last_tsn.  last_tsn
is actually the first token to not be included in the cache.
*/
{
  a_cached_token_ptr		ctp;
  a_cached_token_ptr		first_ctp_to_copy;
  a_cached_token_ptr		last_ctp_to_copy;

  /* first_ctp_to_copy is set for each non-pragma token in the cache, and
     points to the cache entry that follows it (which may be a pragma entry
     the precedes the next token). */
  first_ctp_to_copy = src_cache->first_token;
  for (ctp = src_cache->first_token; ctp != NULL; ctp = ctp->next) {
    if (ctp->token_sequence_number == first_tsn) break;
    if (ctp->extra_info_kind != (a_token_extra_info_kind)teik_pragma) {
      /* The token sequence looks something like:
		 pragma-n0 token-n1 pragma-n2 token-n3 token-n4
         where token-n3 is the first token to be copied.  We also want to
         copy any pragmas that precede token-n3 to the destination cache.
         When we break out of the loop first_ctp_to_copy will point to
         pragma-n2. */
      first_ctp_to_copy = ctp->next;
    }  /* if */
  }  /* for */
  check_assertion_str(ctp != NULL,
                      "copy_tokens_from_cache: first_tsn missing");
  last_ctp_to_copy = ctp;
  for (; ctp != NULL; ctp = ctp->next) {
    /* Stop when we find the specified token, or if we reach an end of
       source token marking the end of the cache. */
    if (ctp->token_sequence_number == last_tsn ||
        (a_token_kind)ctp->token == tok_end_of_source) break;
    if (ctp->extra_info_kind != (a_token_extra_info_kind)teik_pragma) {
      /* The token sequence looks something like:
		 pragma-n0 token-n1 pragma-n2 token-n3 token-n4
         where token-n3 is the first token to be copied.  We also want to
         copy any pragmas that precede token-n3 to the destination cache.
         When we break out of the loop first_ctp_to_copy will point to
         pragma-n2. */
      last_ctp_to_copy = ctp->next;
    }  /* if */
  }  /* for */
  check_assertion_str(ctp != NULL, "copy_tokens_from_cache: last_tsn missing");
  /* Copy the specified range of tokens to the destination cache. */
  for (ctp = first_ctp_to_copy; ctp != last_ctp_to_copy; ctp = ctp->next) {
    a_cached_token_ptr	copy_ctp;
    /* Make a copy of the token to be added. */
    alloc_cached_token(copy_ctp);
    copy_cached_token(ctp, copy_ctp);
    add_cached_token_to_cache(copy_ctp, dest_cache);
  }  /* for */
}  /* copy_tokens_from_cache */


static
a_boolean cache_token_stream_until_matching_token(
				a_token_cache		*cache,
                                a_boolean		coalesce_ids,
				a_token_sequence_number last_tsn_in_cache)
/*
Given curr_token of '(', '[', or '{', copy tokens into the token cache
specified by cache up to but not including the corresponding closing token,
')', ']', or '}', respectively.  Return immediately if end of source is
reached.  (This routine is similar to flush_until_matching_token, but instead
of throwing tokens away it adds them to the specified token cache.)

Normally returns FALSE.  Returns TRUE if it returns without finding
the desired token (i.e., on end-of-source or a zero level right brace).

coalesce_ids is TRUE if identifiers found in the token stream should
be coalesced.  This should be done when the stop token set includes
tokens that can appear in an expression, which means that the
caching process must be able to determine whether a "<" starts
a template argument list or is just a less-than sign.
*/
{
  a_token_kind  closing_token;
  int           paren_count = 0, bracket_count = 0, brace_count = 0;
  a_boolean	done = FALSE;
  a_boolean	err = FALSE;

  db_enter(4, "cache_token_stream_until_matching_token");
  /* Determine the closing token that corresponds to curr_token. */
  switch (curr_token) {
    case tok_lparen:    closing_token = tok_rparen;   break;
    case tok_lbracket:  closing_token = tok_rbracket; break;
    case tok_lbrace:    closing_token = tok_rbrace;   break;
#if CHECKING
    default:
      internal_error("cache_token_stream_until_matching_token: bad token");
#endif /* CHECKING */
  }  /* switch */
  /* Cache the current token, and advance to its successor. */
  if (!coalesce_ids) cache_curr_token(cache);
  get_token_and_coalesce_if_needed(coalesce_ids, last_tsn_in_cache)
  /* Keep looping through successive tokens until the corresponding closing
     token is found at level zero (i.e., not within a nesting of parens,
     brackets, or braces). */
  while (!done && (curr_token != closing_token ||
                   paren_count != 0 || bracket_count != 0 ||
		   brace_count != 0)) {
    /* Never scan past a zero level right brace.  This prevents
       caching past the end of a class or function in the event of
       a mismatched paren or bracket. */
    if (curr_token == tok_rbrace && brace_count == 0) {
      err = TRUE;
      break;
    }  /* if */
    /* Count paired tokens within the skip. */
    if (closing_token == tok_rbrace) { /*lint !e539*/
      /* When looking for a right brace, don't consider any other
         delimiters.  Braces can't be nested inside parens, brackets,
         etc. */
      switch (curr_token) {
        case tok_lbrace:                         brace_count++;   break;
        case tok_rbrace:    if (brace_count > 0) brace_count--;   break;
        default:;
      }  /* switch */
    } else {
      switch (curr_token) {
        case tok_lparen:                           paren_count++;   break;
        case tok_rparen:    if (paren_count > 0)   paren_count--;   break;
        case tok_lbracket:                         bracket_count++; break;
        case tok_rbracket:  if (bracket_count > 0) bracket_count--; break;
        case tok_lbrace:                           brace_count++;   break;
        case tok_rbrace:    if (brace_count > 0) brace_count--;     break;
        default:;
      }  /* switch */
    }  /* if */
    /* Always stop the flush on end of source. */
    if (curr_token == tok_end_of_source) break;
    /* None of the conditions was satisfied, so keep going. */
    if (!coalesce_ids) cache_curr_token(cache);
    get_token_and_coalesce_if_needed(coalesce_ids, last_tsn_in_cache);
  }  /* while */
  db_exit();
  return err;
}  /* cache_token_stream_until_matching_token */


static
void cache_token_stream_with_coalesce_flag(a_token_cache_ptr  cache,
                                           a_token_set_array  stop_tokens,
                                           a_boolean	      coalesce_ids,
                                           a_token_cache_ptr  src_cache)
/*
Copy the current token and succeeding tokens into the token cache specified
by cache up to but not including the first token that matches a member of
the stop tokens array.  Return immediately if end of source is reached.
(This routine is similar to flush_tokens, but instead of throwing tokens
away it adds them to the specified token cache.)

coalesce_ids is TRUE if identifiers found in the token stream should
be coalesced.  This should be done when the stop token set includes
tokens that can appear in an expression, which means that the
caching process must be able to determine whether a "<" starts
a template argument list or is just a less-than sign.
src_cache must be provided when coalesce_ids is TRUE, and points to
a token cache containing the tokens that are being coalesced.  Once the
end of the token stream has been found, the tokens from that cache will
be copies to the new cache.
*/
{
  a_token_sequence_number	first_tsn = curr_token_sequence_number;
  a_token_sequence_number	last_tsn;
  a_token_sequence_number	last_tsn_in_cache = NO_TOKEN_SEQUENCE_NUMBER;

  db_enter(4, "cache_token_stream_with_coalesce_flag");
  if (coalesce_ids) {
    a_cached_token_ptr	ctp = src_cache->first_token;
    /* Find the last token in the cache to make sure that we don't scan past
       it while coalescing identifiers. */
    for (; ctp != NULL; ctp = ctp->next) {
      if (ctp->token_sequence_number > last_tsn_in_cache) {
        last_tsn_in_cache = ctp->token_sequence_number;
      }  /* if */
    }  /* for */
    if (first_tsn > last_tsn_in_cache) {
      /* We are already at the end, or past the end of the source cache.
         Don't attempt to get any more tokens.  Unget the current token so
         that it will be made the current token by the get_token done at
         the end of this routine. */
      unget_token();
      /* See get_token_and_coalesce_if_needed for more information. */
      curr_token = tok_end_of_source;
      curr_token_sequence_number = MAX_TOKEN_SEQUENCE_NUMBER;
    } else {
      /* Attempt to coalesce this token in case it begins an identifier. */
      (void)is_generalized_identifier_start(GID_TEMPLATE_ARGS_OPTIONAL);
    }  /* if */
  }  /* if */
  /* Loop through the tokens, beginning with the current token and stopping
     when a token in the stop token array is found.  Whenever a '(', '[', or
     '{' is encountered, ignore the stop token array until the corresponding
     ')', ']', or '}' is reached. */
  while (stop_tokens[(int)curr_token] == 0) {
    a_boolean	err;
    if (curr_token == tok_lparen || curr_token == tok_lbracket ||
        curr_token == tok_lbrace) {
      err = cache_token_stream_until_matching_token(
                                       cache, coalesce_ids, last_tsn_in_cache);
      if (err) break;
    }  /* if */
    /* Stop immediately when end of source is reached. */
    if (curr_token == tok_end_of_source) break;
    /* Add the current token to the cache and advance to its successor. */
    if (!coalesce_ids) cache_curr_token(cache);
    get_token_and_coalesce_if_needed(coalesce_ids, last_tsn_in_cache);
  }  /* while */
  /* Leave error_position associated with what is now curr_token. */
  set_err_pos_to_curr_token();
  if (coalesce_ids) {
    /* Make a copy of the specified range of tokens from the source cache. */
    last_tsn = curr_token_sequence_number;
    if (first_tsn > last_tsn_in_cache) {
      /* The starting token is not in the cache (possible in error cases).
         Don't try to extract them from the source cache. */
    } else {
      copy_tokens_from_cache(src_cache, first_tsn, last_tsn, cache);
    }  /* if */
    if (curr_token == tok_end_of_source && last_tsn >= last_tsn_in_cache) {
      (void)get_token();
    }  /* if */
  }  /* if */
  db_exit();
}  /* cache_token_stream_with_coalecse_flag */


void cache_token_stream(a_token_cache      *cache,
                        a_token_set_array  stop_tokens)
/*
Interface to cache_token_stream_with_coalesce_flag that does not
cause identifiers to be coalesced.
*/
{
  cache_token_stream_with_coalesce_flag(cache, stop_tokens,
                                        /*coalesce_ids=*/FALSE,
                                        (a_token_cache_ptr)NULL);
}  /* cache_token_stream */


void cache_token_stream_coalesce_identifiers(a_token_cache_ptr  cache,
                                             a_token_set_array  stop_tokens,
                                             a_token_cache_ptr	src_cache)
/*
Interface to cache_token_stream_with_coalesce_flag that causes
identifiers to be coalesced.
*/
{
  check_assertion_str2(src_cache != NULL,
                       "cache_token_stream_coalesce_identifiers:",
                       "no source cache specified");
  cache_token_stream_with_coalesce_flag(cache, stop_tokens,
                                        /*coalesce_ids=*/TRUE, src_cache);
}  /* cache_token_stream_coalesce_identifiers */


void cache_rest_of_declaration(a_token_cache_ptr	cache,
                               a_boolean		stop_on_colon,
                               a_boolean		stop_on_lbrace)
/*
Enter the remaining tokens of the current declaration into a reusable
token cache, and scan those tokens from a copy of the cache.  This is
used to create a cache that can be used while caching a token stream and
coalescing identifiers.  stop_on_colon is TRUE if a colon should
be in the set of stop tokens.
*/
{
  /* Initialize a local stop token set. */
  a_token_set_array  stop_tokens;
  clear_token_set_array(stop_tokens);
  /* Cache all tokens up to the ";" that follows a declaration, the "{" that
     begins a definition, or a ":" that begins a ctor initializer list. */
  if (stop_on_lbrace) incr_token_set_array_element(stop_tokens, tok_lbrace);
  if (stop_on_colon) incr_token_set_array_element(stop_tokens, tok_colon);
  incr_token_set_array_element(stop_tokens, tok_semicolon);
  cache_token_stream(cache, stop_tokens);
  /* Add an end-of-source token to the end of the token cache to
     assure that we don't scan past the end of the cache in the actual
     scan. */
  terminate_token_cache(cache);
  /* Rescan the cached tokens from a copy of this token cache. */
  rescan_copy_of_cache(cache);
}  /* cache_rest_of_declaration */


void rescan_cached_tokens(a_token_cache *cache)
/*
Put the tokens saved in *cache onto the rescan list so that they will be
re-fetched by get_token.  On return, the current token is the first
token of the cache.  The token that was the current token on entry is
placed at the end of the rescan list so that it will be fetched again
after the rescanned tokens have been gotten.  If there are no tokens
in the cache, nothing is done.
*/
{
  db_enter(4, "rescan_cached_tokens");
#if DEBUG
  /* This cache was marked as reusable but is now being destructively
     rescanned.  Update the count of reusable cached tokens. */
  if (cache->is_reusable) {
    /* Reset the flag so that any tokens added to the cache (as is done
       later in this routine) won't be considered reusable. */
    cache->is_reusable = FALSE;
    num_cached_tokens_in_reusable_caches -= cache->token_count;
    num_pragmas_in_reusable_caches -= cache->pragma_count;
  }  /* if */
#endif /* DEBUG */
  if (cache->first_token != NULL) {
    /* Add the current token to the cache, so that it is not lost. */
    cache_curr_token(cache);
    /* Put the tokens in the cache onto the front of the rescan list. */
    cache->last_token->next = cached_token_rescan_list;
    cached_token_rescan_list = cache->first_token;
    /* Clear the cache to be neat. */
    cache->first_token = cache->last_token = NULL;
    /* Indicate that the special case code at the beginning of get_token
       is needed to check for cached token rescanning. */
    any_initial_get_token_tests_needed = TRUE;
    /* Fetch the first cached token. */
    (void)get_token();
  }  /* if */
  db_exit();
}  /* rescan_cached_tokens */


void rescan_reusable_cache(a_token_cache *cache)
/*
This routine is similar to rescan_cached_tokens except that the token
cache provided by the caller is not destroyed while it is scanned.
This is used by template processing for instantiation processing.
The token cache passed by the caller is put on the top of a stack of
reusable caches being scanned.  The cached token rescan list
pointer is saved in the entry for the reusable cache so that
it can be restored when the cache has been exhausted.  The current
token is cached so that it will be fetched again after the reusable
tokens have been rescanned.
*/
{
  a_token_cache               cache_for_curr_token;
  a_reusable_cache_entry_ptr  rcep;

  db_enter(4, "rescan_reusable_cache");
  /* Make sure that this cache is reusable. */
  check_assertion(cache->is_reusable);
  if (cache->first_token != NULL) {
    /* Create a token cache for the current token so that it is
       not lost. */
    clear_token_cache(&cache_for_curr_token, /*reusable=*/FALSE);
    cache_curr_token(&cache_for_curr_token);
    /* Append the current rescan list to the end of the cache just created
       for the current token. */
    cache_for_curr_token.last_token->next = cached_token_rescan_list;
    cached_token_rescan_list = cache_for_curr_token.first_token;
    /* Create a new reusable cache entry for the new cache and put on the
       front of the list of active reusable caches. */
    rcep = alloc_reusable_cache_entry();
    rcep->next = reusable_cache_stack;
    reusable_cache_stack = rcep;
    /* Save and clear the current value of the regular token rescan list. */
    rcep->previous_token_rescan_list = cached_token_rescan_list;
    cached_token_rescan_list = NULL;
    /* Set the next token pointer of the reusable cache entry to the front
       of the cache. */
    rcep->next_cached_token = cache->first_token;
    /* Indicate that the special case code at the beginning of get_token
       is needed to cached token rescanning. */
    any_initial_get_token_tests_needed = TRUE;
    /* Fetch the first cached token. */
    (void)get_token();
  }  /* if */
  db_exit();
}  /* rescan_reusable_cache */


void rescan_copy_of_cache(a_token_cache *cache)
/*
This routine makes a copy of the cache provided by the caller and rescans
the tokens from the copy of the cache.  The source cache must be terminated
by a tok_end_of_source.  The copy of the cache will not include the
end of source token.  This routine is used to allow a series of tokens
to be cached into a reusable cache and the rescanned as if part of
the original source with no need to worry about detection of the
tok_end_of_source later.
*/
{
  a_token_cache	      temp_token_cache;

  clear_token_cache(&temp_token_cache, /*reusable=*/FALSE);
  rescan_reusable_cache(cache);
  while (curr_token != tok_end_of_source) {
    cache_curr_token(&temp_token_cache);
    (void)get_token();
  }  /* while */
  /* Skip past the end-of-source token. */
  (void)get_token();
  /* Rescan the tokens from the temporary cache. */
  rescan_cached_tokens(&temp_token_cache);
}  /* rescan_copy_of_cache */


static void free_reusable_cache_entry(a_reusable_cache_entry_ptr rsep)
/*
Free a token cache stack entry, i.e., put it on the avail list to be reused.
*/
{
  rsep->next = avail_reusable_cache_entries;
  avail_reusable_cache_entries = rsep;
}  /* free_reusable_cache_entry */


/*ARGSUSED*/ /* <-- "okay_if_not_found" is only used by checking code. */
void split_token_cache(a_token_cache	       *cache1,
                       a_token_cache	       *cache2,
                       a_token_sequence_number split_location,
                       a_boolean	       include_prev_token,
                       a_boolean	       okay_if_not_found)
/*
Split cache1 into two pieces.  cache1 will contain all the tokens up
to the one that precedes the token number specified by split location.
cache2 will contain all the tokens that follow.  If incldue_prev_token
is TRUE, we should include the token before the split location in the tokens
that are moved to cache2.  okay_if_not_found is TRUE if it is okay
if the split location is not found.  This suppresses an internal
error.
*/
{
  a_cached_token_ptr		ctp;
  a_cached_token_ptr		first_ctp_to_move = NULL;
  a_cached_token_ptr		before_first_ctp_to_move = NULL;
  a_cached_token_ptr		prev_first_ctp_to_move;
  a_cached_token_ptr		prev_before_first_ctp_to_move;

  check_assertion_str2(cache1->is_reusable && cache2->is_reusable,
                       "split_token_cache:",
                       "cache not reusable");
  for (ctp = cache1->first_token; ctp != NULL; ctp = ctp->next) {
    if (ctp->token_sequence_number == split_location) break;
    if (ctp->extra_info_kind != (a_token_extra_info_kind)teik_pragma) {
      /* The token sequence looks something like:
		 pragma-n0 token-n1 pragma-n2 token-n3 token-n4
         where token-n3 is the split location.  We also want to move any
         pragma that precede token-n3 to cache 2.  When we break out of
         the loop first_ctp_to_move will point to pragma-n2 and
         before_first_ctp_to_move will point to token-n1.  Save the previous
         values of these fields.  If include_prev_token is TRUE we want
         to split the cache before pragma-n0. */
      prev_first_ctp_to_move = first_ctp_to_move;
      prev_before_first_ctp_to_move = before_first_ctp_to_move;
      first_ctp_to_move = ctp->next;
      before_first_ctp_to_move = ctp;
    }  /* if */
  }  /* for */
  if (ctp == NULL && okay_if_not_found) goto exit;
  check_assertion_str2(ctp != NULL, "split_token_cache:",
                       "specified token not found");
  if (include_prev_token) {
    /* The token before the split location should be the first to be put
       in the new cache. */
    first_ctp_to_move = prev_first_ctp_to_move;
    before_first_ctp_to_move = prev_before_first_ctp_to_move;
  }  /* if */
#if DEBUG
  /* Adjust the token and pragma counts in the caches. */
  for (ctp = first_ctp_to_move; ctp != NULL; ctp = ctp->next) {
    if (ctp->extra_info_kind == (a_token_extra_info_kind)teik_pragma) {
      /* Adjust the pragma count for the two caches. */
      a_pending_pragma_ptr	ppp = ctp->variant.pragmas;
      while (ppp != NULL) {
        cache1->pragma_count--;
        cache2->pragma_count++;
        ppp = ppp->next;
      }  /* while */
    }  /* if */
    /* Adjust the token counts for the two caches. */
    cache1->token_count--;
    cache2->token_count++;
  }  /* for */
#endif /* DEBUG */
  /* Set the first and last tokens of cache2 to the appropriate values. */
  cache2->first_token = first_ctp_to_move;
  cache2->last_token = cache1->last_token;
  /* Break the links in cache1. */
  cache1->last_token = before_first_ctp_to_move;
  cache1->last_token->next = NULL;
  /* Add a new terminator to the end of the original. */
  terminate_token_cache(cache1);
exit:
  return;
}  /* split_token_cache */


void move_cached_tokens(a_cached_token_ptr	first_token,
			a_token_cache		*from_cache,
                        a_token_cache		*to_cache)
/*
Given two token caches, move the specified list of tokens from the
first token cache to the second.  The list of tokens will already
have been removed from the list indicated by from_cache, but will
still be counted in the token and pragma count fields used for
debugging purposes.  This routine may only be used for reusable token caches.
*/
{
  a_cached_token_ptr		ctp;
  a_cached_token_ptr		last_ctp;

  check_assertion_str2(from_cache->is_reusable && to_cache->is_reusable,
                       "move_cached_tokens:",
                       "cache not reusable");
  to_cache->first_token = first_token;
  for (ctp = first_token; ctp != NULL; last_ctp = ctp, ctp = ctp->next) {
#if DEBUG
    if (ctp->extra_info_kind == (a_token_extra_info_kind)teik_pragma) {
      /* Adjust the pragma count for the two caches. */
      a_pending_pragma_ptr	ppp = ctp->variant.pragmas;
      while (ppp != NULL) {
        from_cache->pragma_count--;
        to_cache->pragma_count++;
        ppp = ppp->next;
      }  /* while */
    }  /* if */
    /* Adjust the token counts for the two caches. */
    from_cache->token_count--;
    to_cache->token_count++;
#endif /* DEBUG */
  }  /* for */
  /* Set the last token pointer and terminate the token cache. */
  to_cache->last_token = last_ctp;
  terminate_token_cache(to_cache);
}  /* move_cached_tokens */


void discard_token_cache(a_token_cache *cache)
/*
The token cache *cache has been built but is not needed; free the cached
tokens therein and clear the cache.
*/
{
  a_cached_token_ptr ctp, ctp_next;

#if DEBUG
  /* This cache was marked as reusable but is now being discarded.
     Update the count of reusable cached tokens. */
  if (cache->is_reusable) {
    /* Reset the flag just to be neat. */
    cache->is_reusable = FALSE;
    num_cached_tokens_in_reusable_caches -= cache->token_count;
    num_pragmas_in_reusable_caches -= cache->pragma_count;
  }  /* if */
#endif /* DEBUG */
  for (ctp = cache->first_token; ctp != NULL; ctp = ctp_next) {
    ctp_next = ctp->next;
    free_cached_token(ctp);
  }  /* for */
  clear_token_cache(cache, /*reusable=*/(a_boolean)cache->is_reusable);
}  /* discard_token_cache */


static a_token_kind get_token_from_cached_token_rescan_list(void)
/*
Remove the first token from cached_token_rescan_list, establish it as the
current token, and return its token kind.  This routine and
get_token_from_reusable_cache_stack are very similar.  If a change is
made to one routine the other should be checked to see if it needs
an equivalent change.
*/
{
  a_token_kind       ctoken;
  a_cached_token_ptr ctp;

  db_enter(4, "get_token_from_cached_token_rescan_list");
  for (;;) {
    /* Remove the first entry from the list. */
    ctp = cached_token_rescan_list;
    cached_token_rescan_list = cached_token_rescan_list->next;
    /* If it is a special entry indicating a pragma, process it and
       take another entry.  Otherwise, exit the loop. */
    if (ctp->extra_info_kind != (a_token_extra_info_kind)teik_pragma){
      break;
    }  /* if */
    /* Set the current token pragma list to point to the pragmas associated
       with the cached token. */
    check_assertion_str(!suppress_pragma_processing,
                  "get_token_from...: pragma found in suppress_pragma mode");
    curr_token_pragmas = ctp->variant.pragmas;
    free_cached_token(ctp);
  }  /* for */
  /* When fetch_pp_tokens is FALSE, make sure that the token being retrieved
     is not a cached pp-token.  If it is, flush any cached pp-tokens and
     issue an error at the point at which we resume scanning normal tokens.
     It should not be possible to run out of tokens while flushing the
     pp-tokens. */
  if (ctp->extra_info_kind == (a_token_extra_info_kind)teik_pp_token &&
      !fetch_pp_tokens) {
    while (ctp != NULL &&
           ctp->extra_info_kind == (a_token_extra_info_kind)teik_pp_token) {
      ctp = ctp->next;
    }  /* while */
    check_assertion_str2(ctp != NULL, "get_token_from_reusable_cache_stack:",
                         "pp-token flush consumed all tokens");
    pos_error(ec_end_of_flush, &ctp->source_position);
    cached_token_rescan_list = ctp->next;
  }  /* if */
  /* Entry is for a token (normal case). */
  ctoken = (a_token_kind)ctp->token;
  pos_curr_token = ctp->source_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  end_pos_curr_token = ctp->end_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  error_position = pos_curr_token;
  curr_token_sequence_number = ctp->token_sequence_number;
  start_of_curr_token = end_of_curr_token = NULL;
  len_of_curr_token = 0;
  if (ctp->extra_info_kind == (a_token_extra_info_kind)teik_pp_token) {
    /* This is a cached pp-token.  Restore the starting and ending token
       positions. */
    start_of_curr_token = ctp->variant.pp_token_descr.token_start;
    end_of_curr_token = ctp->variant.pp_token_descr.token_end;
  } else if (ctp->extra_info_kind ==
                                    (a_token_extra_info_kind)teik_identifier) {
    /* For an identifier, restore the locator. */
    locator_for_curr_id = ctp->variant.locator;
  } else if (ctp->extra_info_kind == 
                                    (a_token_extra_info_kind)teik_asm_string) {
    /* For a Microsoft asm token, restore the asm string pointer. */
    curr_token_asm_string = ctp->variant.asm_string;
  } else if (ctp->extra_info_kind == (a_token_extra_info_kind)teik_constant) {
    /* For a literal constant, restore const_for_curr_token. */
    copy_constant(ctp->variant.constant, &const_for_curr_token);
  }  /* if */
  free_cached_token(ctp);
  if (cached_token_rescan_list == NULL) {
    recalc_any_initial_get_token_tests_needed();
  }  /* if */
  db_exit();
  return ctoken;
}  /* get_token_from_cached_token_rescan_list */


static a_token_kind get_token_from_reusable_cache_stack(void)
/*
Return a token from the current entry on the reusable cache stack,
establish it as the current token, and return its token kind.  This routine
and get_token_from_cached_token_rescan_list are very similar.  If a change is
made to one routine the other should be checked to see if it needs
an equivalent change.
*/
{
  a_token_kind       ctoken;
  a_cached_token_ptr ctp;

  db_enter(4, "get_token_from_reusable_cache_stack");
  for (;;) {
    /* Remove the first entry from the list. */
    ctp = reusable_cache_stack->next_cached_token;
    reusable_cache_stack->next_cached_token = ctp->next;
    /* If it is a special entry indicating a pragma, process it and
       take another entry.  Otherwise, exit the loop. */
    if (ctp->extra_info_kind != (a_token_extra_info_kind)teik_pragma){
      break;
    }  /* if */
    /* Set the current token pragma list to point to the pragmas associated
       with the cached token. */
    check_assertion_str(!suppress_pragma_processing,
                  "get_token_from...: pragma found in suppress_pragma mode");
    curr_token_pragmas = make_copy_of_pragma_list(ctp->variant.pragmas);
  }  /* for */
  /* When fetch_pp_tokens is FALSE, make sure that the token being retrieved
     is not a cached pp-token.  If it is, flush any cached pp-tokens and
     issue an error at the point at which we resume scanning normal tokens.
     It should not be possible to run out of tokens while flushing the
     pp-tokens. */
  if (ctp->extra_info_kind == (a_token_extra_info_kind)teik_pp_token &&
      !fetch_pp_tokens) {
    while (ctp != NULL &&
           ctp->extra_info_kind == (a_token_extra_info_kind)teik_pp_token) {
      ctp = ctp->next;
    }  /* while */
    check_assertion_str2(ctp != NULL, "get_token_from_reusable_cache_stack:",
                         "pp-token flush consumed all tokens");
    pos_error(ec_end_of_flush, &ctp->source_position);
    reusable_cache_stack->next_cached_token = ctp->next;
  }  /* if */
  /* Entry is for a token (normal case). */
  ctoken = (a_token_kind)ctp->token;
  pos_curr_token = ctp->source_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  end_pos_curr_token = ctp->end_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  error_position = pos_curr_token;
  curr_token_sequence_number = ctp->token_sequence_number;
  start_of_curr_token = end_of_curr_token = NULL;
  len_of_curr_token = 0;
  if (ctp->extra_info_kind == (a_token_extra_info_kind)teik_pp_token) {
    /* This is a cached pp-token.  Restore the starting and ending token
       positions. */
    start_of_curr_token = ctp->variant.pp_token_descr.token_start;
    end_of_curr_token = ctp->variant.pp_token_descr.token_end;
  } else if (ctp->extra_info_kind ==
                                    (a_token_extra_info_kind)teik_identifier) {
    /* For an identifier, restore the locator. */
    locator_for_curr_id = ctp->variant.locator;
  } else if (ctp->extra_info_kind == 
                                    (a_token_extra_info_kind)teik_asm_string) {
    /* For a Microsoft asm token, restore the asm string pointer. */
    curr_token_asm_string = ctp->variant.asm_string;
  } else if (ctp->extra_info_kind == (a_token_extra_info_kind)teik_constant) {
    /* For a literal constant, restore const_for_curr_token. */
    copy_constant(ctp->variant.constant, &const_for_curr_token);
  }  /* if */
  /* Check whether we have reached the end of this cache. */
  if (reusable_cache_stack->next_cached_token == NULL) {
    a_reusable_cache_entry_ptr  rcep = reusable_cache_stack;
    /* Restore the cached token rescan list to the state before the
       current reusable cache was pushed onto the stack.  These tokens
       should be rescanned before we resume use of the next entry on the
       reusable stack. */
    cached_token_rescan_list = rcep->previous_token_rescan_list;
    reusable_cache_stack = rcep->next;
    free_reusable_cache_entry(rcep);
    recalc_any_initial_get_token_tests_needed();
  }  /* if */
  db_exit();
  return ctoken;
}  /* get_token_from_reusable_cache_stack */

/*
Macro that is TRUE when tokens are being rescanned from a cache.
*/
#define rescanning_cached_tokens()				\
  (cached_token_rescan_list != NULL || reusable_cache_stack != NULL)


static an_orig_line_modif_ptr add_orig_line_modif(
                                an_orig_line_modif_kind kind,
                                char                    *line_loc)
/*
Allocate an original line modification entry, set its kind to "kind",
set its line location to "line_loc", set its fields to default values,
add it to the end of the orig_line_modif_list, and return a pointer to it.
Entries of this kind indicate the locations of modifications to the
original source line because of trigraphs and line splices.
*/
{
  an_orig_line_modif_ptr olmp;

  if (avail_orig_line_modifs != NULL) {
    /* Reuse a freed entry. */
    olmp = avail_orig_line_modifs;
    avail_orig_line_modifs = avail_orig_line_modifs->next;
  } else {
    /* Allocate a new entry. */
    olmp = (an_orig_line_modif_ptr)alloc_fe(sizeof(an_orig_line_modif));
#if DEBUG
    num_orig_line_modifs_allocated++;
#endif /* DEBUG */
  }  /* if */
  /* Set the fixed fields. */
  olmp->next     = NULL;
  olmp->line_loc = line_loc;
  olmp->kind     = kind;
  /* Set the variant fields. */
  switch (kind) {
    case olm_trigraph:
      olmp->variant.trigraph_orig_char = ' ';  /* To be neat. */
      break;
    case olm_line_splice:
    case olm_multiline_string_splice:
      olmp->variant.line_splice_seq_number = 0;  /* To be neat. */
      break;
#if CHECKING
    default:
      internal_error("add_orig_line_modif: bad kind");
#endif /* CHECKING */
  }  /* switch */
  /* Add the entry to the end of the list. */
  if (orig_line_modif_list == NULL) {
    /* This is the first entry on the list. */
    orig_line_modif_list = olmp;
  } else {
    /* Link the current last entry to this new last entry. */
    end_orig_line_modif_list->next = olmp;
  }  /* if */
  /* The new entry is the new end of list. */
  end_orig_line_modif_list = olmp;
  no_modifs_to_curr_source_line = FALSE;

  return(olmp);
}  /* add_orig_line_modif */


static void free_orig_line_modif(an_orig_line_modif_ptr *olmp)
/*
Free the original line modification pointed to by *olmp, and set *olmp 
to NULL.
*/
{
  /* Add the entry to the front of the list of available entries. */
  (*olmp)->next = avail_orig_line_modifs;
  avail_orig_line_modifs = *olmp;
  *olmp = NULL;
}  /* free_orig_line_modif */


a_source_line_modif_ptr add_source_line_modif(
                          char                      *line_loc,
                          sizeof_t                  num_chars_to_delete,
                          char                      *inserted_text,
                          char                      *end_inserted_text)
/*
Allocate a source line modification entry, put "line_loc",
"num_chars_to_delete", "inserted_text", and "end_inserted_text" in its
like-named fields, set its other fields to default values, add it to
the front of the source_line_modif_list, and return a pointer to it.
line_loc is NULL to indicate an insertion before the first character
of curr_source_line.  Inserted_text and end_inserted_text are passed in
as NULL when the caller plans to set those fields explicitly later.
Entries of this kind are used to record changes made during
preprocessing, such as deletion of comments and expansion of macro
invocations.
*/
{
  a_source_line_modif_ptr slmp;

  if (avail_source_line_modifs != NULL) {
    /* Reuse a freed entry. */
    slmp = avail_source_line_modifs;
    avail_source_line_modifs = avail_source_line_modifs->next;
  } else {
    /* Allocate a new entry. */
    slmp = (a_source_line_modif_ptr)alloc_fe(sizeof(a_source_line_modif));
#if DEBUG
    num_source_line_modifs_allocated++;
#endif /* DEBUG */
  }  /* if */
  slmp->next                = source_line_modif_list;
  slmp->line_loc            = line_loc;
  slmp->parent_modif        = NULL;
  slmp->num_chars_to_delete = num_chars_to_delete;
  slmp->is_isolated_text    = FALSE;
  slmp->is_for_comment      = FALSE;
  slmp->parent_modif_determined
                            = FALSE;
  slmp->being_rescanned_for_token_pasting
                            = FALSE;
  slmp->inserted_text       = inserted_text;
  slmp->end_inserted_text   = end_inserted_text;
  slmp->assoc_macro         = (a_macro_def_ptr)NULL;
  /* Give this entry a sequence id indicating the "time" at which it was
     added. */
  slmp->sequence_id         = ++sequence_id_for_source_line_modifs;
  slmp->assoc_copy_modif    = (a_source_line_modif_ptr)NULL;
  slmp->source_position.seq = 0;
  slmp->source_position.column
                            = SP_COL_UNKNOWN;
  slmp->text_from_primary_source_line
                            = NULL;
  if (line_loc != NULL) {
    /* Normal case: line_loc points to the point of insertion.  Save the
       original character, and replace it with a marker that will call
       attention to the source modification. */
    slmp->orig_char         = *line_loc;
    *line_loc               = ATTENTION_MARKER;
  } else {
    /* Insertion before the first character of curr_source_line. */
    slmp->orig_char         = ' ';
    /* Save a pointer to this entry so that it can be found easily. */
#if CHECKING
    if (line_start_source_line_modif != NULL) {
      /* There should only be one of these. */
      internal_error(
          "add_source_line_modif: more than one line_start_source_line_modif");
    }  /* if */
#endif /* CHECKING */
    line_start_source_line_modif = slmp;
  }  /* if */
  /* Add the entry to the front of the list.  (Adding to the front helps
     keep those entries most likely to be referenced where they will be
     found most quickly.) */
  source_line_modif_list = slmp;
  no_modifs_to_curr_source_line = FALSE;

  return(slmp);
}  /* add_source_line_modif */


/*
Add a source line modification entry to indicate deletion of num_chars
characters starting at line_loc.  The inserted_chars area is used for
the zero-length replacement string.  for_comment is TRUE if the
modification is due to a comment.
*/
#define add_deletion_source_line_modif(line_loc, num_chars, for_comment) \
{ a_source_line_modif_ptr dslmp; \
  dslmp = add_source_line_modif(line_loc, num_chars, \
                                (char *)NULL, (char *)NULL); \
  *dslmp->inserted_chars   = LE_ESCAPE; \
  dslmp->inserted_chars[1] = LE_END_OF_INSERTION; \
  dslmp->inserted_text = dslmp->end_inserted_text = dslmp->inserted_chars; \
  dslmp->is_for_comment = for_comment; \
}  /* add_deletion_source_line_modif */


/*
Add a source line modification to indicate replacement of num_chars
characters starting at line_loc by a space.  This is used for deletion
of comments.  A different space string must be used for each comment
in the current line so that one can get back from each one to the
right place based only on line position; for this reason, the
space/end-insertion string is placed in the a_source_line_modif entry.
*/
#define replace_source_string_by_space(line_loc, num_chars) \
{ a_source_line_modif_ptr rslmp; \
  rslmp = add_source_line_modif(line_loc, num_chars, \
                                (char *)NULL, (char *)NULL); \
  *rslmp->inserted_chars   = ' '; \
  rslmp->inserted_chars[1] = LE_ESCAPE; \
  rslmp->inserted_chars[2] = LE_END_OF_INSERTION; \
  rslmp->inserted_text     = rslmp->inserted_chars; \
  rslmp->end_inserted_text = rslmp->inserted_chars+1; \
  rslmp->is_for_comment    = TRUE; \
}  /* replace_source_string_by_space */


void free_source_line_modif(a_source_line_modif_ptr *slmp)
/*
Free the source line modification entry pointed to by *slmp, and set *slmp
to NULL.
*/
{
  /* Add the entry to the front of the list of available entries. */
  (*slmp)->next = avail_source_line_modifs;
  avail_source_line_modifs = *slmp;
  /* Clear the parameter so that the pointer cannot be inadvertently
     used again. */
  *slmp = NULL;
}  /* free_source_line_modif */


void rem_source_line_modif(a_source_line_modif_ptr slmp)
/*
Remove the source line modification pointed to by *slmp from the
source_line_modif_list.  The entry is not freed.
*/
{
  a_source_line_modif_ptr prev_slmp;

  /* Remove this entry from the source_line_modif_list. */
  if (slmp == source_line_modif_list) {
    /* The entry is first on the list, so removing it is easy. */
    source_line_modif_list = slmp->next;
  } else {
    /* Find the entry preceding this one, and link around this entry. */
    for (prev_slmp = source_line_modif_list;
         prev_slmp->next != slmp;
         prev_slmp = prev_slmp->next) {};
    prev_slmp->next = slmp->next;
  }  /* if */
  slmp->next = NULL;
  /* If this entry was line_start_source_line_modif (a special entry
     inserted preceding the first character of curr_source_line), clear
     that pointer now, because there is no longer such an entry. */
  if (line_start_source_line_modif == slmp) {
    line_start_source_line_modif = NULL;
  } else {
#if CHECKING
    if (slmp->line_loc == NULL) {
      internal_error("rem_source_line_modif: line_loc NULL");
    }  /* if */
#endif /* CHECKING */
    /* Restore the original character (thus removing the attention marker
       character put in when this entry was added). */
    *(slmp->line_loc) = slmp->orig_char;
  }  /* if */
}  /* rem_source_line_modif */


a_source_line_modif_ptr assoc_source_line_modif(char *loc_in_line)
/*
Find the source line modification entry that defines the character location
loc_in_line.  The location must be one that appears in a source modification
(i.e., the caller probably has had to check already that
within_curr_source_line(loc_in_line) == FALSE).
*/
{
  register a_source_line_modif_ptr slmp, prev_slmp;

  for (prev_slmp = NULL, slmp = source_line_modif_list;
       ;
       prev_slmp = slmp, slmp = slmp->next) {
#if CHECKING
    if (slmp == NULL) {
      internal_error("assoc_source_line_modif: bad address");
    }  /* if */
#endif /* CHECKING */
    if (ptr_in_range(loc_in_line, slmp->inserted_text,
                                  slmp->end_inserted_text+1)) {
      /* loc_in_line falls within this entry's text.  Move the entry to 
         the front of the list to speed up other searches. */
      if (prev_slmp != NULL) {
        prev_slmp->next = slmp->next;
        slmp->next = source_line_modif_list;
        source_line_modif_list = slmp;
      }  /* if */
      break;
    }  /* if */
  }  /* for */

  return(slmp);
}  /* assoc_source_line_modif */


a_source_line_modif_ptr f_parent_source_line_modif(
                                                  a_source_line_modif_ptr slmp)
/*
Find the parent of the source line modification entry slmp, and return
a pointer to it.  The parent modification is the modification that inserts
the text that slmp modifies.  Return NULL if slmp has no parent (i.e., it
modifies the primary source line).  This routine should not be called
directly: call the macro parent_source_line_modif instead, which checks
the parent_modif_determined flag to see if the parent is already known.
*/
{
  a_source_line_modif_ptr parent_slmp;
  char                    *line_loc;

  line_loc = slmp->line_loc;
  if (line_loc == NULL) {
    /* This is line_start_source_line_modif, the special entry to insert at 
       the beginning of the current source line.  Its parent is NULL. */
    parent_slmp = NULL;
  } else if (within_curr_source_line(line_loc)) {
    /* slmp modifies the primary source line; it has no parent. */
    parent_slmp = NULL;
  } else {
    /* slmp modifies the text inserted by some other modification.  Find
       out which. */
    parent_slmp = assoc_source_line_modif(line_loc);
  }  /* if */
  /* Call a macro to store the value and set the parent_modif_determined
     flag. */
  set_parent_modif(slmp, parent_slmp);
  return parent_slmp;
}  /* f_parent_source_line_modif */


a_source_line_modif_ptr nested_source_line_modif(char *loc_in_line)
/*
*loc_in_line contains an ATTENTION_MARKER, indicating that the text at that
point is altered by a source line modification entry.  Find the entry,
and return a pointer to it.
*/
{
  register a_source_line_modif_ptr slmp;

  for (slmp = source_line_modif_list; ; slmp = slmp->next) {
#if CHECKING
    if (slmp == NULL) {
      internal_error("nested_source_line_modif: bad address");
    }  /* if */
#endif /* CHECKING */
    if (slmp->line_loc == loc_in_line) break;
  }  /* for */

  return(slmp);
}  /* nested_source_line_modif */


/*ARGSUSED*/ /* <-- because "kind" is not used in some versions. */
void gen_pp_line_info(char      kind,
                      a_boolean next_line)
/*
Write a line-identification directive to f_pp_output as part of preprocessor
output.  It should identify the current line, or the line following the
current line if next_line is TRUE.  The kind character is the third operand:
'1' for entry into a file, '2' for exit from a file, and ' ' for anything
else.  This routine should only be called when generate_pp_output is TRUE.
*/
{
  a_line_number eff_line_number;

  if (gen_line_info_in_pp_output) {
    /* Put out a line-identifying directive.  The form is
         #line line-number "file-name"
       or, in pcc mode,
         # line-number "file-name" kind
       This is similar to, but is not, a #line directive.  The "kind"
       is not always wanted (the SUN cc generates it, but not all pcc-based
       compilers do); the flag GEN_EXTRA_LINE_ID_INFO controls whether or
       not it is generated. */
    if (!pcc_preprocessing_mode) {
      /* ANSI version. */
      fputs("#line", f_pp_output);
    } else {
      /* pcc version. */
      fputc('#', f_pp_output);
    }  /* if */
    /* Put out the line number. */
    eff_line_number = curr_ise->line_number;
    if (next_line) {
      /* Identify the line after the current one. */
      eff_line_number++;
    } else {
      /* Identify the current line.  Note that curr_ise->line_number is the
         number of the last line read, which will be the number of the last
         physical line when a logical line is continued using backslashes.
         In such a case, get the number of the first line. */
      eff_line_number -= seq_number_last_read - curr_seq_number;
    }  /* if */
    fprintf(f_pp_output, " %lu \"",  (unsigned long)eff_line_number);
    /* Put out the file name.  For ANSI/ISO output, add escapes as
       necessary. */
    (void)write_file_name(curr_ise->file_name, f_pp_output,
                          /*process_escapes=*/!pcc_preprocessing_mode);
    fputc('"', f_pp_output);
#if GEN_EXTRA_LINE_ID_INFO
    if (pcc_preprocessing_mode) {
      if (kind != ' ') {
        fputc(' ', f_pp_output);
        fputc(kind, f_pp_output);
      }  /* if */
    }  /* if */
#endif /* GEN_EXTRA_LINE_ID_INFO */
    fputc('\n', f_pp_output);
    /* Remember the sequence number associated with the current pp output
       position.  When the current logical source line is continued using
       backslashes, curr_seq_number gives the sequence number of the
       first line and seq_number_last_read gives the sequence number of
       the last line. */
    if (next_line) {
      next_seq_in_pp_output = seq_number_last_read + 1;
    } else {
      next_seq_in_pp_output = curr_seq_number;
    }  /* if */
  }  /* if */
}  /* gen_pp_line_info */


/*
Execute "statement" if a blank should be put out before the first character
of a new token to separate it from the previous token.  This is used
in producing preprocessing output and raw listing output, in cases
where token confusion might result without the blank.  token_start is TRUE
if this is the start of a new token; if so, ch is the first character
of the new token, and prev_ch is the previous character.  prev_ch is
updated on return (always).  token_start is reset to FALSE if it was TRUE.
In some modes (pcc mode, #include directives for stdarg.h) no blank is ever
put out.

NOTE:  This check needs to be pretty fast.  It is only run when
preprocessing or raw listing output is being created, and then
only when there are expansions.  Still, it gets done a lot.

If this is the first character following a token mark, see if the
previous character (prev_ch) and the current character (ch) could
appear next to each other in a token.  If so, a blank is put out
to separate them.  The decision is made conservatively -- if there
is any doubt, or if it's too hard to figure out, the blank is put out.
Start by getting the lexical category of the two characters.
If either is a "singleton", i.e., a character that is always its own
token and not part of another, then no space is needed.
*/
#define token_separator_blank_if_needed(ch, prev_ch, token_start, statement) \
{ a_byte cat_ch, cat_prev_ch;                                         \
  if (token_start) {                                                  \
    token_start = FALSE;                                              \
    if (!no_token_separators_in_this_line_of_pp_output) {             \
      if ((cat_prev_ch = pp_lexical_category[prev_ch-CHAR_MIN]) ==    \
                                                         PLC_SINGLETON || \
          (cat_ch = pp_lexical_category[ch-CHAR_MIN]) == PLC_SINGLETON) { \
        /* At least one is a singleton, so no space is needed. */     \
      } else if (cat_prev_ch != cat_ch &&                             \
        /* The two characters have different categories (i.e., one is \
           a character that can appear in identifiers or pp-numbers,  \
           and the other is not).  We probably do not need the space. \
           However, check for some bizarre special cases having to do \
           with pp-numbers (3.1.8; "e" or "E" followed by "+" or "-"  \
           can appear in a pp-number) and wide literals ("L" followed \
           by a single or double quote). */                           \
                 ((prev_ch != 'e' && prev_ch != 'E') ||               \
                  (ch != '+' && ch != '-')) &&                        \
                  (prev_ch != 'L' || (ch != '\'' && ch != '"'))) {    \
        /* No space is needed. */                                     \
      } else {                                                        \
        /* An extra space to separate tokens is needed. */            \
        statement;                                                    \
      }  /* if */                                                     \
    }  /* if */                                                       \
  }  /* if */                                                         \
  prev_ch = ch;                                                       \
}  /* token_separator_blank_if_needed */


void gen_pp_output_for_curr_line(void)
/*
Write out the current line to f_pp_output (preprocessing output), if
necessary.  This routine should be called only if generate_pp_output
is TRUE.
*/
{
  register char                    *loc_in_line;
  register char                    ch;
  register a_source_line_modif_ptr slmp;
           a_source_line_modif_ptr ins_slmp;
           char                    prev_ch;
           a_boolean               token_start;

  /* do_not_put_curr_line_in_pp_output is TRUE if there is no current
     source line (as at the start of source), or if the line should not
     be put out (for example, because it's a preprocessing directive). */
  /* There's a strange special case that happens when some text that
     should be output is inserted at the front of a line that should not
     be output, e.g., an inert macro identifier is inserted at the beginning
     of a line that is a preprocessing directive.  The flag
     init_do_not_put_curr_line_in_pp_output controls output of the inserted
     text independently. */
  if (!do_not_put_curr_line_in_pp_output ||
      (line_start_source_line_modif != NULL &&
       !init_do_not_put_curr_line_in_pp_output)) {
    /* See if the new line immediately follows the line previously written.
       If not, put out a directive to indicate the new line's position.
       If the previous output line did not end with a newline,
       line numbering can't be adjusted at this point. */
    if (curr_seq_number != next_seq_in_pp_output &&
        prev_pp_output_line_was_complete) {
      if (curr_seq_number <= next_seq_in_pp_output+5 &&
          /* Following line is needed for some cases involving reinsertion
             of a macro id at the beginning of a line.  The reinserted id
             is followed by a newline, which bumps up the next_seq_in_pp_output
             past curr_seq_number. */
          curr_seq_number > next_seq_in_pp_output) {
        /* Optimization -- For changes of a small number of lines,
           it is more efficient to put out one or more blank lines to
           move up to the desired line number.  This is smaller in the
           output file, and also more efficient to process on the
           receiving end.  Note that if line position information is not
           wanted in the output, the blank lines are not put out, since
           they serve no real purpose. */
        while (curr_seq_number != next_seq_in_pp_output) {
          if (gen_line_info_in_pp_output) putc('\n', f_pp_output);
          next_seq_in_pp_output++;
        }  /* while */
      } else {
        /* Larger skip; put out a line-identifying directive. */
        gen_pp_line_info(' ', /*next_line=*/FALSE);
      }  /* if */
    }  /* if */
    /* Put out the previous line itself.  The text in curr_source_line
       has trigraphs and line splices processed, but is not correct for
       preprocessing output if there are in it any comments to be deleted
       or macros to be expanded.  For those cases, the preprocessed line
       must be constructed by using the information in
       source_line_modif_list. */
    if (source_line_modif_list == NULL) {
      /* For the common case, output the line quickly. */
      /* We count on the fact that an LE_ESCAPE sequence will end the
         string. */
      if (fputs(curr_source_line, f_pp_output) == EOF) {
        /* Error in writing the pp output file.  This check is done on 
           most lines and supplements the check done when the file is closed.
           Checking here is so that a disk full error is caught fairly
           quickly. */
        str_catastrophe(ec_file_write_error, "preprocessing output");
      }  /* if */
      /* The newline at the end of the source line is represented by
         an LE_ESCAPE/LE_NEWLINE lexical escape sequence, so no newline
         was printed.  Print one now. */
      putc('\n', f_pp_output);      
      next_seq_in_pp_output++;
      prev_pp_output_line_was_complete = TRUE;
    } else {
      /* Construct the preprocessing output from the source line and the
         list of modifications.  Start at the beginning, and scan through
         characters, processing escapes appropriately. */
      /* The logic here is very similar to that in
         gen_expanded_raw_listing_output_for_curr_line.  If you change
         this routine, change the other too. */
      set_up_for_walk_of_source_line(loc_in_line, slmp);
      prev_ch = '\n';
      token_start = FALSE;
      for (;;) {
        /* Fetch the next character, stepping into and out of macro
           expansions. */
        ch = *loc_in_line;
        if (ch == ATTENTION_MARKER) {
          /* Attention marker.  Find the associated source line modification
             and process it. */
          walk_into_insertion(slmp, ins_slmp, loc_in_line);
          if (loc_in_line[0] == LE_ESCAPE &&
              loc_in_line[1] == LE_INERT_MACRO) {
            /* If the insertion starts with an inert macro indication, do not
               consider it the start of a new token.  This makes some
               undefined-behavior token-pasting cases work slightly better
               in the output. */
          } else {
            token_start = TRUE;
          }  /* if */
        } else if (ch == LE_ESCAPE) {
          /* Lexical escape. */
          ch = loc_in_line[1];
          if (ch == LE_END_OF_TOKEN) {
            /* Do not output end-of-token markers. */
            token_start = TRUE;
            loc_in_line += LE_ESCAPE_LEN;
          } else if (ch == LE_INERT_MACRO) {
            /* Do not output inert-macro markers. */
            loc_in_line += LE_ESCAPE_LEN;
          } else if (ch == LE_END_OF_INSERTION) {
            /* End of a macro expansion. */
            /* If we've just finished the inserted text at the start of the
               source line, and the main part of the line is not supposed to
               be displayed (see comment above), stop here. */
            if (slmp == line_start_source_line_modif &&
                do_not_put_curr_line_in_pp_output) break;
            /* End of a macro.  Pick up after the invocation text. */
            walk_out_of_insertion(slmp, loc_in_line);
            token_start = TRUE;
          } else if (ch == LE_NEWLINE) {
            /* Newline character. */
            putc('\n', f_pp_output);
            prev_ch = '\n';
            /* Count newlines.  This is done inside the loop because there are
               cases where no newline is output (two lines are joined in the
               output), and cases where one line has two newlines, and we
               don't want to miscount. */
            prev_pp_output_line_was_complete = TRUE;
            next_seq_in_pp_output++;
            loc_in_line += LE_ESCAPE_LEN;
          } else if (ch == LE_END_OF_LINE) {
            /* End of the whole line. */
            break;
          } else {
            unexpected_condition_str(
                            "gen_pp_output_for_curr_line: bad lexical escape");
          }  /* if */
        } else {
          /* Normal character. */
          /* Output a blank to separate the character from the previous
             character if necessary to prevent tokenizing confusion. */
          token_separator_blank_if_needed(ch, prev_ch, token_start,
                                          putc(' ', f_pp_output));
          /* Output the character. */
          putc(ch, f_pp_output);
          prev_pp_output_line_was_complete = FALSE;
          loc_in_line++;
        }  /* if */
      }  /* for */
    }  /* if */
    /* Make sure this line is written only once. */
    init_do_not_put_curr_line_in_pp_output = TRUE;
    do_not_put_curr_line_in_pp_output = TRUE;
  }  /* if */
}  /* gen_pp_output_for_curr_line */


void gen_rlisting_line_info(char kind)
/*
Generate line information for the raw listing file (which is optionally
generated as input to a program that will generate an interspersed listing).
kind is '1' when going into an include file, '2' after having left an include
file, and ' ' for any other cases (like for the primary file and for #line
directives).  This routine should only be called when f_raw_listing
is non-NULL.
The slightly convoluted name of this routine is due to 8-char external
uniqueness requirements.  Sometimes there's no good way to get a unique name.
*/
{
  /* Generate line information for the raw listing file.  The form is
       L line-number "file-name" kind
  */
  fprintf(f_raw_listing, "L %lu \"%s\"",
                         (a_line_number)(curr_ise->line_number+1),
                         curr_ise->file_name);
  if (kind != ' ') {
    putc(' ', f_raw_listing);
    putc(kind, f_raw_listing);
  }  /* if */
  putc('\n', f_raw_listing);
}  /* gen_rlisting_line_info */


static void write_orig_line_piece(char *loc_in_line,
                                  char *stop_loc)
/*
Write the characters from *loc_in_line to just before *stop_loc in
the current source line to the raw listing file.  If stop_loc is NULL,
write to the end of the source line.  Write the characters in their original
source form, i.e., if there are macro expansions replace the attention
markers with the original characters.  The caller guarantees that no
orig_line_modif_list modifications apply to the indicated text.
*/
{
  char                    *local_stop_loc;
  a_source_line_modif_ptr slmp;

  for (;;) {
    if (source_line_modif_list == NULL) {
      /* There are no macro modifications, so the text can just be written. */
      local_stop_loc = stop_loc;
    } else if (stop_loc == NULL) {
      /* Writing to end of line, and there are macro modifications to the
         current line.  See if there are any attention markers in the rest
         of the line.  Note that this scan will stop at the
         LE_ESCAPE/LE_NEWLINE lexical escape sequence at the end of the
         line if no attention marker is found. */
      local_stop_loc = strchr(loc_in_line, ATTENTION_MARKER);
    } else {
      /* Writing part of the line, and there are macro modifications to the
         current line.  See if there are any attention markers in the part
         of the line we want to write. */
      for (local_stop_loc = loc_in_line;
           local_stop_loc < stop_loc && *local_stop_loc != ATTENTION_MARKER;
           local_stop_loc++) {}
    }  /* if */
    /* Now write whatever is just raw text. */
    if (local_stop_loc == NULL) {
      /* Write to the end of the line.  The end of every line will be written
         by this code, and in most cases the entire line. */
      if (fputs(loc_in_line, f_raw_listing) == EOF) {
        /* Error in writing the raw listing file.  This check supplements 
           the check done when the file is closed.  Checking here is done so
           that a disk full error is caught fairly quickly. */
        str_catastrophe(ec_file_write_error, "raw listing");
      }  /* if */
      /* Write the final newline (the source line contains an
         LE_ESCAPE/LE_NEWLINE escape rather than an actual newline
         character). */
      putc('\n', f_raw_listing);
    } else {
      /* Write a piece of the line. */
      fprintf(f_raw_listing, "%.*s", (int)(local_stop_loc-loc_in_line),
              loc_in_line);
    }  /* if */
    loc_in_line = local_stop_loc;
    /* If the whole requested piece has now been written out, exit the loop. */
    if (loc_in_line == stop_loc) break;
    /* *loc_in_line must be an attention marker.  Find and write the original
       character for that position. */
    slmp = nested_source_line_modif(loc_in_line);
    putc(slmp->orig_char, f_raw_listing);
    loc_in_line++;
    /* If the whole requested piece has now been written out, exit the loop. */
    if (loc_in_line == stop_loc) break;
  }  /* for */
}  /* write_orig_line_piece */


/*
Clear the raw_listing_buffer.
*/
#define clear_raw_listing_buffer()                                    \
{ loc_in_raw_listing_buffer = raw_listing_buffer;                     \
  must_display_raw_listing_buffer = FALSE;                            \
}  /* clear_raw_listing_buffer */


static void expand_raw_listing_buffer(void)
/*
We have run into a source line that won't fit in raw_listing_buffer;
reallocate raw_listing_buffer to make it bigger.
*/
{
  sizeof_t old_size, new_size, loc_offset;
  char     *new_raw_listing_buffer;

  db_enter(4, "expand_raw_listing_buffer");
  old_size = after_end_of_raw_listing_buffer - raw_listing_buffer;
  /* Increase the size of raw_listing_buffer. */
  new_size = old_size + RAW_LISTING_BUFFER_INCREMENTAL_ALLOCATION;
  new_raw_listing_buffer = realloc_general(raw_listing_buffer,
                                           old_size, new_size);
  loc_offset = loc_in_raw_listing_buffer - raw_listing_buffer;
  raw_listing_buffer = new_raw_listing_buffer;
  after_end_of_raw_listing_buffer = raw_listing_buffer + new_size;
  loc_in_raw_listing_buffer = raw_listing_buffer + loc_offset;
  db_exit();
}  /* expand_raw_listing_buffer */


/*
Add a character to the raw listing buffer.  Expand the buffer if necessary.
*/
#define add_char_to_raw_listing_buffer(ch)                            \
{ if (loc_in_raw_listing_buffer == after_end_of_raw_listing_buffer) { \
    expand_raw_listing_buffer();                                      \
  }  /* if */                                                         \
  *loc_in_raw_listing_buffer++ = ch;                                  \
}  /* add_char_to_raw_listing_buffer. */


void gen_expanded_raw_listing_output_for_curr_line(a_boolean do_inserted_text)
/*
Generate the raw listing file output for the macro-expanded version of
the current source line.  This routine should only be called if
f_raw_listing != NULL.  If do_inserted_text is TRUE, there must be text
inserted at the beginning of the current source line, and the
expanded information for that inserted text (only) will be output.
If do_inserted_text is FALSE, there must be a current source line, and
the expanded information for it (and not any inserted text) will be output.
This routine shouldn't be called more than once with each value of
do_inserted_text for a given line, since it doesn't maintain its own flag
indicating that the expanded version has already been put out.
See gen_raw_listing_output_for_curr_line and cpp_driver, which control
the calls to this routine.
*/
{
  register char                    *loc_in_line;
  register char                    ch;
  register a_source_line_modif_ptr slmp;
           a_source_line_modif_ptr ins_slmp;
           char                    prev_ch;
           a_boolean               token_start;

  /* The output line is generated in raw_listing_buffer first, then
     written to output.  This expensive and unfortunate technique is
     required because of cases like
       ? ?=pragma hello / *
       * / there 
     (Where the / * and * / are really comment delimiters, of course,
     and the two ?s are not separated by a space.)
     For that case, we'll get here with "#pragma hello " with no newline,
     and then later with " there".  The whole line is written when the
     piece with the newline arrives.  That's because the raw listing
     output has to look like
       N? ?=pragma hello / *
       N* / there
       X#pragma hello   there
     (Again, the comment delimiters and ?s have to be imagined altered.)
     Without this trick, the two parts of the "X" line could not be written
     next to one another.
     Another issue:  Lines that contain only comment modifications are not
     written.  However, we could get a first segment of a line (not ending
     in a newline) that contains only comment modifications, and then a
     second segment (ending in a newline) that contains macro modifications.
     The entire line (both segments) must be written out, but we can't
     determine that while processing the first segment, so we have to save it.
  */
  /* One easy speed optimization:  If the line has no modifications of
     any kind (which implies it ends with a newline), and the buffer
     contains no nontrivial modifications (which implies that there is
     no previous line segment that would force the printing of this line),
     just throw the line away and clear the buffer.  This deals with
     a substantial percentage of source lines. */
  if (no_modifs_to_curr_source_line && !must_display_raw_listing_buffer) {
    clear_raw_listing_buffer();
  } else {
    /* The logic here is very similar to that in gen_pp_output_for_curr_line.
       If you change this routine, change the other too. */
    if (do_inserted_text) {
      /* Do the text inserted at the front of the source line. */
      slmp = line_start_source_line_modif;
      loc_in_line = slmp->inserted_text;
    } else {
      /* Do the source line itself. */
      slmp = NULL;
      loc_in_line = curr_source_line;
      /* A line containing trigraphs or line splices is considered modified
         and must be displayed. */
      if (orig_line_modif_list != NULL) must_display_raw_listing_buffer = TRUE;
    }  /* if */
    prev_ch = '\n';
    token_start = FALSE;
    for (;;) {
      /* Fetch the next character, stepping into and out of macro
         expansions. */
      ch = *loc_in_line;
      if (ch == ATTENTION_MARKER) {
        /* Attention marker.  Find the associated source line modification
           and process it. */
        walk_into_insertion(slmp, ins_slmp, loc_in_line);
        token_start = TRUE;
        if (!ins_slmp->is_for_comment) {
          /* Keep track of whether or not there are noncomment (i.e., macro)
             modifications in the lines in the buffer. */
          must_display_raw_listing_buffer = TRUE;
        }  /* if */
      } else if (ch == LE_ESCAPE) {
        /* Lexical escape. */
        ch = loc_in_line[1];
        if (ch == LE_END_OF_TOKEN ||
            ch == LE_INERT_MACRO) {
          /* Do not output end-of-token or inert-macro markers. */
          token_start = TRUE;
          loc_in_line += LE_ESCAPE_LEN;
        } else if (ch == LE_END_OF_INSERTION) {
          /* End of a macro expansion. */
          /* If we've just finished the inserted text at the start of the
             source line, stop. */
          if (slmp == line_start_source_line_modif) break;
          /* End of a macro.  Pick up after the invocation text. */
          walk_out_of_insertion(slmp, loc_in_line);
          token_start = TRUE;
        } else if (ch == LE_NEWLINE) {
          /* Newline character. */
          add_char_to_raw_listing_buffer('\n');
          prev_ch = '\n';
          /* We have a complete line and we should output it or throw it
             away now. */
          if (must_display_raw_listing_buffer) {
            *loc_in_raw_listing_buffer = '\0';
            putc('X', f_raw_listing);
            fputs(raw_listing_buffer, f_raw_listing);
          }  /* if */
          clear_raw_listing_buffer();
          loc_in_line += LE_ESCAPE_LEN;
        } else if (ch == LE_END_OF_LINE) {
          /* End of the whole line. */
          break;
        } else {
          unexpected_condition_str(
                           "gen_expanded_raw_listing_...: bad lexical escape");
        }  /* if */
      } else {
        /* Normal character. */
        /* Output a blank to separate the character from the previous
           character if necessary to prevent tokenizing confusion. */
        token_separator_blank_if_needed(ch, prev_ch, token_start,
                                        add_char_to_raw_listing_buffer(' '));
        /* Output the character. */
        add_char_to_raw_listing_buffer(ch);
        loc_in_line++;
      }  /* if */
    }  /* for */
  }  /* if */
}  /* gen_expanded_raw_listing_output_for_curr_line */


static void gen_raw_listing_output_for_curr_line(void)
/*
Generate raw listing output for the current source line.  This routine should
only be called when f_raw_listing is non-NULL.
*/
{
  an_orig_line_modif_ptr olmp, olmp_next;
  char                   *loc_in_line;

  /* No output if there is no current line. */
  if (curr_raw_listing_line_code != '\0') {
    /* Write the macro-expanded form of the text inserted at the beginning
       of the source line if there is any and if there are modifications. */
    if (line_start_source_line_modif != NULL) {
      gen_expanded_raw_listing_output_for_curr_line(/*do_inserted_text=*/TRUE);
    }  /* if */
    /* If we are now (at the end of the line) not in an if-skip, force
       the line to be an "N" (normal) line.  This gets #else and #endif
       lines out as "N" lines rather than "S" (skipped) lines. */
    if (!currently_in_pp_if_skip) curr_raw_listing_line_code = 'N';
    /* Put out the start of the line. */
    putc(curr_raw_listing_line_code, f_raw_listing);
    /* Reconstruct the original line and output it. */
    loc_in_line = curr_source_line;
    for (olmp = orig_line_modif_list; olmp != NULL; olmp = olmp->next) {
      /* Process each modification in order. */
      /* Write unaffected text that precedes this modification. */
      write_orig_line_piece(loc_in_line, olmp->line_loc);
      switch (olmp->kind) {
        case olm_trigraph:
          fprintf(f_raw_listing, "??%c", olmp->variant.trigraph_orig_char);
          /* If the trigraph is "? ? /", which turns into "\", and it's at the
             end of a line, the "\" will indicate a line splice.  In that
             case, the "\" for the line splice should not be put out.
             (The extra spaces in the trigraph above are to avoid complaints
             from compilers compiling this comment.) */
          olmp_next = olmp->next;
          if (olmp_next != NULL && olmp_next->kind == olm_line_splice &&
              olmp_next->line_loc == olmp->line_loc) {
            /* Partially process the line-splice entry. */
            olmp = olmp_next;
            loc_in_line = olmp->line_loc;
            goto partially_process_line_splice;
          }  /* if */
          /* Normal trigraph. */
          loc_in_line = olmp->line_loc + 1;
          break;
        case olm_multiline_string_splice:
          /* A line splice induced by a multiline string.  Skip the
             artificial \n and don't add a trailing backslash.  */
          loc_in_line = olmp->line_loc + 2;
          goto partially_process_line_splice;
        case olm_line_splice:
          loc_in_line = olmp->line_loc;
          putc('\\', f_raw_listing);
partially_process_line_splice:
          putc('\n', f_raw_listing);
          putc(curr_raw_listing_line_code, f_raw_listing);
          break;
#if CHECKING
        default:
          internal_error(
       "gen_raw_listing_output_for_curr_line: bad orig_modif_list entry kind");
#endif /* CHECKING */
      }  /* switch */
    }  /* for */
    /* Write out the last piece of unaffected text, including the newline.
       In the most common case (no modifications), this writes the entire
       line. */
    write_orig_line_piece(loc_in_line, (char *)NULL);
    /* Write the macro-expanded form of the line if there are modifications. */
    gen_expanded_raw_listing_output_for_curr_line(/*do_inserted_text=*/FALSE);
    /* Make sure this line is written only once. */
    curr_raw_listing_line_code = '\0';
  }  /* if */
}  /* gen_raw_listing_output_for_curr_line */


void finish_raw_listing_file(void)
/*
Called on abnormal termination of the compilation to do cleanup
on the raw listing file, e.g., force out the last source line.
*/
{
  if (f_raw_listing != NULL) {
    gen_raw_listing_output_for_curr_line();
  }  /* if */
}  /* finish_raw_listing_file */


/*
The routines that follow are used to detect idioms used to guard against
multiple inclusions of a given file.  If such idiom is found, the
front end will completely suppress any subsequent attempts to re-include
the file.

The idioms supported are #ifndef guard code, and "#pragma once" directives.
Files using #ifndef guards have the form:

	... Optional comments ...
	#ifndef NAME
	#define NAME
	... Body of include file ...
	#endif

Note that #ifndef and #ifdef forms of this are supported, but similar
mechanisms using
 
	#if !defined(NAME)

are not supported.

This include guard detection mechanism is implemented using a simple
state machine.  The state information is recorded in the input stack
entry.

            (*)                      (*)
      ---------------> FAIL <-------------------
      |                STATE                    |
      |                  ^                      |
      |          level-0 | #else/#elif          |
      |                  |                      |
      |                  |         #endif       |
    START  #ifdef   INTERMEDIATE------------>ACCEPT
    STATE---------->   STATE   ---+           STATE
      |    #ifndef       | ^      |             |
      |                  | | (*)  |             |
      |                  | -------|             |
      |                  |                      |
      |                  |                      |
      |          #pragma | once                 |
      |                  v                      |
      |  #pragma       ONCE      #pragma        |
      |--------------> STATE <------------------|
          once         ^   |       once
                       |   |
                       |(*)|
                       -----



In the above diagram, comments do not count as tokens at all, while
(*) refers to any token other than those marking the other transitions.

In the case of the INTERMEDIATE state (which is what the file is in during
the body, if an opening #ifndef has been seen), if a matching #else is
seen, we know that subsequent inclusions cannot be suppressed.

Once the file has reached the ACCEPT state, if any token is seen, then
the file goes into the FAIL state.

Also, once the file gets into the ONCE state, it will never move out.

Most of this determination is done in get_token(), and the
routines that handle the #ifdef, #ifndef and #pragma directives (in
preproc.c).

The final accept-state check is performed in pop_input_stack().

On subsequent includes, the file history information is checked to
determine whether the file contained a multiple inclusion guard.  If
so, the include is suppressed.
*/

static an_include_file_history_ptr alloc_include_file_history(void)
/*
Allocate an_include_file_history structure, initialize it, and return
a pointer.
*/
{
  an_include_file_history_ptr	ifhp;
  ifhp = (an_include_file_history_ptr)
                           alloc_fe(sizeof(an_include_file_history));
#if DEBUG
  num_include_file_histories_allocated++;
#endif /* DEBUG */
  ifhp->full_name = NULL;
  ifhp->next = NULL;
  ifhp->suppress_subsequent_include = FALSE;
  ifhp->pragma_once = FALSE;
  ifhp->ifdef_guard = FALSE;
  ifhp->ifndef_guard = FALSE;
  ifhp->controlling_macro_name = NULL;
  return ifhp;
}  /* alloc_include_file_history */


a_boolean find_include_history(char                        *full_name,
	    		       an_include_file_history_ptr *ifhp_ptr,
			       a_boolean		   create)
/*
Examine the file history to see if "full_name" has been seen before. If
it has, return a pointer to its history record in ret_hist, otherwise
if create is TRUE, create a new history record, attach it to the file
history chain, and return a pointer to the new entry in ifhp. Also, set
first_time if the latter case.  Return TRUE if the file was
found in the list.
*/
{
  an_include_file_history_ptr	ifhp;
  an_include_file_history_ptr	prev_ifhp;
  a_boolean			found = FALSE;
  sizeof_t			name_length = strlen(full_name);

  /* Loop through the file history list and try to find a entry that
     matches the file passed by the caller. */
  for (ifhp = include_file_history_list, prev_ifhp = NULL;
       ifhp != NULL;
       prev_ifhp = ifhp, ifhp = ifhp->next) {
    if (compare_file_names(full_name, ifhp->full_name) == 0) {
      /* We've found a match. */
      found = TRUE;
      break;
    }  /* if */
  }  /* for */
  if (ifhp != NULL || !create) {
    /* An entry was found -- this file has been included before. */
  } else {
    /* This file has not been included before.  Create a new file history
       entry. */
    /* Append to the tail of the list */
    ifhp = alloc_include_file_history();
    ifhp->full_name = full_name;
    ifhp->name_length = name_length;
    ifhp->next = NULL;
    if (prev_ifhp) {
      prev_ifhp->next = ifhp;
    } else {
      include_file_history_list = ifhp;
    }  /* if */
  }  /* if */
  *ifhp_ptr = ifhp;
  return found;
}  /* find_include_history */


a_byte get_ifg_state(void)
/*
Get the current include file guard state from the current input stack entry.
The state is first normalized by taking into account the
any_tokens_fetched_from_curr_input_file global variable.
*/
{
  /* If we are in the START or ACCEPT state and we have encountered some
     tokens, move to the fail state.  This means there were tokens before
     the opening #ifndef or after the #endif. */
  a_byte state;
  if (curr_ise == NULL) {
    state = IFG_STATE_FAIL;
  } else {
    if (curr_ise->ifg_state < IFG_STATE_FAIL &&
        any_tokens_fetched_from_curr_input_file) {
      curr_ise->ifg_state = IFG_STATE_FAIL;
    }  /* if */
    state = curr_ise->ifg_state;
  }  /* if */
  return state;
}  /* get_ifg_state */


void set_ifg_state(a_byte	new_state)
/*
Update the include file guard state.  This routine resets the
any_tokens_fetched_from_curr_input_file when appropriate.
*/
{
  if (curr_ise != NULL) {
    curr_ise->ifg_state = new_state;
    if (new_state == IFG_STATE_ACCEPT) {
      /* We've just seen the closing #endif, there should be no more tokens in
         this file. */
      any_tokens_fetched_from_curr_input_file = FALSE;
    }  /* if */
  }  /* if */
}  /* set_ifg_state */


static a_boolean suppress_subsequent_include
				(an_include_file_history_ptr ifhp)
/*
Return TRUE if the specified include file contained include guard
code that makes it possible to suppress subsequent re-inclusions.
*/
{
  a_boolean		result = FALSE;
  a_symbol_ptr		assoc_symbol;
  a_symbol_locator	locator;


  if (ifhp->pragma_once) {
    result = TRUE;
  } else if (!ifhp->suppress_subsequent_include) {
    /* No need to check further. */
  } else if (ifhp->ifdef_guard || ifhp->ifndef_guard) {  
    /* See whether the controlling macro is currently defined. */
    a_symbol_header_ptr	sym_hdr;
    locator = cleared_locator;
    sym_hdr = find_symbol_header(
                              ifhp->controlling_macro_name,
			      (sizeof_t)(strlen(ifhp->controlling_macro_name)),
                              &locator);
    assoc_symbol = find_defined_macro(sym_hdr);
    /* If the macro is undefined, then an #ifdef NAME guard would cause the
       included file to be ignored, so we should return TRUE (meaning it is
       OK to suppress the inclusion). */
    result = assoc_symbol == NULL;
    /* If this is an #ifndef instead of an #ifdef, negate the current value
       of result. */
    if (ifhp->ifndef_guard) result = !result;
  }  /* if */
  return result;
}  /* suppress_subsequent_include */


a_boolean suppress_subsequent_include_of_file
				(char                        *full_name,
				 an_include_file_history_ptr *ifhp_ptr)
/*
Determine whether the specified file has already been included, and if so,
whether a subsequent include should be suppressed because it will have
no effect.
*/
{
  a_boolean	result;
  /* Find an existing include file history record for this file, or create
     one if none exists. */
  (void)find_include_history(full_name, ifhp_ptr, /*create=*/TRUE);
  result = suppress_subsequent_include(*ifhp_ptr);
  return result;
} /* suppress_subsequent_include_of_file */


#if DEBUG
static void db_include_guard_info(void)
/*
Display the include guard information associated with the current input
stack entry, for debugging purposes.
*/
{
  char *idemp_name;
  char *idemp_text;
  db_enter(5, "db_include_guard_info");
  switch (curr_ise->ifg_state) {
    case IFG_STATE_START:
      fprintf(f_debug, "Pop: File %s is (essentially) empty\n",
              curr_ise->file_name);
      break;
    case IFG_STATE_FAIL:
      fprintf(f_debug, "Pop: File %s is not guarded\n", curr_ise->file_name);
      break;
    case IFG_STATE_ACCEPT:
      if (curr_ise->include_history->ifdef_guard) {
        idemp_name = "#ifdef";
        idemp_text = curr_ise->include_history->controlling_macro_name;
      } else if (curr_ise->include_history->ifndef_guard) {
        idemp_name = "#ifndef";
        idemp_text = curr_ise->include_history->controlling_macro_name;
      } else {
        unexpected_condition();
      }  /* if */
      fprintf(f_debug,
              "Pop: File %s is guarded (kind = %s, macro name = \"%s\")\n",
              curr_ise->file_name, idemp_name, idemp_text);
      break;
    case IFG_STATE_ONCE:
      if (curr_ise->include_history->pragma_once) {
        fprintf(f_debug,
                "Pop: File %s is guarded (kind = #pragma once).\n",
                curr_ise->file_name);
      } else {
        unexpected_condition();
      }
      break;
    default:
      unexpected_condition();
  }  /* switch */
  db_exit();
}  /* db_include_guard_info */
#endif /* DEBUG */


void check_for_generation_of_pch_on_return_to_primary_file(void)
/*
Generate a PCH file if we are at the top of the input stack and the
flag is set indicating that a PCH file should be generated on return
to the primary source file.  This routine is called when an include
file is popped off of the input stack to check whether a PCH file
should be generated upon return to the primary source file.  This
routine is also called when an include directive is processed but the
include is suppressed for some reason.
*/
{
  if (depth_input_stack == 0 && 
      generate_pch_on_return_to_primary_source_file) {
    /* We are returning to the primary source file and the flag is set
       indicating that a PCH file should be generated at this point. */
    generate_pch_on_return_to_primary_source_file = FALSE;
    generate_precompiled_header();
    header_stop_no_longer_pending();
  }  /* if */
}  /* check_for_generation_of_pch_on_return_to_primary_file */


static void display_included_file_name(int	depth,
				       char	*file_name)
/*
When using the option to list the included files, this routine is called
to actually output the include file name.  depth is the stack depth
to be used for indentation purposes.  file_name is the name of the file
to be displayed.
*/
{
  /* Indent the output by the input stack depth. */
  unsigned long indent = (unsigned long)(depth - 1);
  for (; indent > 0; indent--) fputc(' ', f_pp_output);
  fprintf(f_pp_output, "%s\n", file_name);
}  /* display_included_file_name */
  

void open_file_and_push_input_stack(char      *file_name,
                                    a_boolean use_search_path,
				    a_boolean is_include_file,
                                    a_boolean is_system_include,
                                    a_boolean is_preinclude,
                                    a_boolean is_implicit_include,
                                    a_boolean is_include_next)
/*
Push the indicated file onto the input stack, so that the next time a line
is read, it will come from that file.  If the file cannot be opened,
generate a catastrophic error and do not return.  use_search_path
is TRUE if the search path of include directories should be used
when trying the open.  file_name must be allocated in IL storage.
is_include_file is TRUE if the file is being read as the result of a
#include directive or a --preinclude command-line-option.  It is
FALSE for implicitly included files.  is_system_include is TRUE for
files included with the #include <file.h> notation and FALSE for
all other files.  is_preinclude is TRUE for files included via the
--preinclude command-line option.  is_implicit_include is TRUE for
files included for template implicit inclusion.  is_include_next is
TRUE if the file is being pushed for an #include_next directive.
*/
{
  char				*full_file_name;
  char				*display_name;
  FILE 				*input_file;
  an_include_file_history_ptr	ifhp;
  a_directory_name_entry_ptr    dir_entry;

  db_enter(2, "open_file_and_push_input_stack");
  input_file = open_file_for_input(file_name, use_search_path,
                                   is_system_include, is_include_next,
                                   /*replace_suffix=*/FALSE, &full_file_name,
                                   &display_name, &dir_entry);
  check_assertion(input_file != NULL);
  if (suppress_subsequent_include_of_file(full_file_name, &ifhp)) {
    /* This file contains include guard code.  An inclusion here would
       have no effect, so it should be suppressed. */
    (void)fclose(input_file);
#if DEBUG
    if (debug_level >= 4) {
      fprintf(f_debug,
          "open_file_and_push_input_stack: skipping guarded include file %s\n",
          file_name);
    }  /* if */
#endif /* DEBUG */
    /* If generating a list of include files (-H option), put out the
       file name. */
    if (list_included_files) {
      display_included_file_name(depth_input_stack + 1, display_name);
    }  /* if */
    /* Check whether a PCH file should be generated at the end of the
       execution of this include directive. */
    check_for_generation_of_pch_on_return_to_primary_file();
    goto done;
  }  /* if */
  push_input_stack(input_file, file_name, display_name, full_file_name,
                   is_include_file, is_system_include, is_preinclude,
                   is_implicit_include, dir_entry, ifhp);
done:
  db_exit();
}  /* open_file_and_push_input_stack */


static FILE *try_to_open_source_file(char	*name_to_try,
				     char	*file_name)
/*
Try to open the source file specified by name_to_try.  file_name is
the name to be used in an error message.
*/
{
  FILE		*new_input_file;
  a_boolean	not_found = FALSE;
  a_boolean	bad_format = FALSE;
  a_boolean	bad_name = FALSE;

  new_input_file = open_source_file(name_to_try, &not_found,
                                    &bad_format, &bad_name);
  /* If not_found is FALSE then either name_to_try is non-NULL
     (i.e., the input file was opened) or else there was an error
     on the open.  In either case, stop searching. */
  if (not_found) {
    /* Issue a catastrophic error if the file could not be opened
       because of an error.  Bad format errors are simply ignored as
       this is typically a result of finding a directory with
       the specified name. */
    if (bad_name) {
      str_catastrophe(ec_illegal_source_file_name, file_name);
    }  /* if */
  }  /* if */
  return new_input_file;
}  /* try_to_open_source_file */


static FILE *search_for_input_file(
			char				*file_name,
			a_boolean			use_search_path,
			a_directory_name_entry_ptr	search_path,
			a_file_suffix_ptr		suffix_list,
			a_boolean			replace_suffix,
			char				**name_found,
			a_directory_name_entry_ptr	*dir_entry)
/*
Look for file_name in the list of directories specified by search path.

If replace_suffix is TRUE, the file suffix of file_name is replaced with
each entry in suffix_list for each directory in search_path.  This is
used when searching for a source file for implicit inclusion.  When
replace_suffix is FALSE a suffix_list may still be specified, in which
case the suffix list is only used if file_name has no suffix.  This
is used to supply a default suffix for headers specified without a
suffix.  The path name of the file found is returned in name_found.
*dir_entry is set to point to the directory name entry on the search
path in which the file was found, or NULL if the search path was not
used.
*/
{
  a_file_suffix_ptr		fsp;
  a_boolean			done = FALSE;
  a_directory_name_entry_ptr	curr_directory_name_entry;
  char				*name_to_try;
  FILE				*new_input_file = NULL;
  char				*prev_dir_name = NULL;
  a_text_buffer_ptr		buffer = NULL;

  *dir_entry = NULL;
  /* Determine whether we need to do the suffix replacement processing.
     This is done when replace_suffix is TRUE or when when file name
     supplied has no suffix. */
  replace_suffix = replace_suffix || *suffix_of(file_name) == '\0';
  if (!use_search_path || is_absolute_file_name(file_name)) {
    /* File name is absolute, so search path is not used. */
    name_to_try = file_name;
    new_input_file = try_to_open_source_file(name_to_try, file_name);
  } else if (search_path == NULL) {
    /* No search path, so file can't be found.  Issue a catastrophic error.
       Use special message to make it clearer, since problem may be that
       there are no -I options on the command line. */
    str_catastrophe(ec_empty_include_search_path, file_name);
  } else {
    /* Loop through the directory name entries.  The "done" flag will be
       set if the loop should not be repeated (i.e., an absolute path name
       was specified). */
    for (curr_directory_name_entry = search_path;
         !done && curr_directory_name_entry != NULL;
         curr_directory_name_entry = curr_directory_name_entry->next) {
      if (curr_directory_name_entry->dir_name == prev_dir_name) {
        /* Two directories with the same name are adjacent in the stack.
          No need to try to open the same file a second time. */
        continue;
      }  /* if */
      prev_dir_name = curr_directory_name_entry->dir_name;
      /* We need to traverse the search path.  Merge the current entry in
         the path with the file name and use that name as the base for
         replacing the suffixes. */
      buffer = combine_dir_and_file_name(
                                      curr_directory_name_entry->dir_name,
                                      file_name, (a_text_buffer_ptr)NULL);
      name_to_try = buffer->buffer;
      /* Now try to open the modified file. */
      if (!replace_suffix) {
        /* We don't need to replace the suffix.  Just try the
           file/directory combination just constructed. */
        new_input_file = try_to_open_source_file(name_to_try, file_name);
      } else {
        /* We need to replace the suffix.  Go through the list of
           suffixes. */
        /* Loop through the linked list of suffixes. */
        for (fsp = suffix_list;
             fsp != NULL;
             fsp = fsp->next) {
          /* Replace the existing suffix with a new one. */
          replace_file_name_suffix(fsp->suffix, buffer);
          /* Get the current buffer pointer in case it was reallocated. */
          name_to_try = buffer->buffer;
          /* Now try to open the modified file. */
          new_input_file = try_to_open_source_file(name_to_try, file_name);
          if (new_input_file != NULL) break;
        }  /* for */
      }  /* if */
      if (new_input_file != NULL) {
        done = TRUE;
        *dir_entry = curr_directory_name_entry;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  if (new_input_file != NULL) {
    /* If a file was found, return the name in name_found.  If the name
       is currently in the temporary buffer, make a copy and return a
       pointer to the copy. */
    if (buffer != NULL && name_to_try == buffer->buffer) {
      name_to_try = alloc_primary_file_scope_il(buffer->size);
      (void)strcpy(name_to_try, buffer->buffer);
    }  /* if */
    *name_found = name_to_try;
  }  /* if */
  return new_input_file;
#undef FILE_NAME_BUFFER_SIZE
}  /* search_for_input_file */


#if !INSTANTIATION_BY_IMPLICIT_INCLUSION
/*ARGSUSED*/ /* <-- replace_suffix is used only if instantiation may use
                    implicit inclusion. */
#endif /* !INSTANTIATION_BY_IMPLICIT_INCLUSION */
FILE *open_file_for_input(char                       *file_name,
                          a_boolean                  use_search_path,
                          a_boolean                  is_system_include,
                          a_boolean                  is_include_next,
                          a_boolean                  replace_suffix,
                          char                       **full_file_name,
                          char                       **display_name,
                          a_directory_name_entry_ptr *dir_entry)
/*
Try to open file_name, and return a pointer to the file if the open is
successful.  file_name must be allocated in IL storage.  use_search_path is
TRUE if the search path of include directories should be used when trying
the open.  is_system_include is TRUE if the included file name was
specified in <...>.  If the open is successful, the full name of the file
that is opened is returned in *full_file_name, the name intended for use in
diagnostics and other output is returned in *display_name.  *dir_entry
is set to point to the entry on the search path in which the file was
found, or NULL if the search path was not used.  is_include_next is
TRUE if the file is being opened for an #include_next directive.
replace_suffix is TRUE when this routine is used to search for an
implicitly included template definition file.  When replace_suffix is
used, each suffix in the implicit_instantiation_file_suffix_list is
used to search for a template definition file.  When replace_suffix is
FALSE, the open must be successful and a catastrophic error will be
issued if it is not; otherwise, a NULL file pointer will be returned.
*/
{
  char                        *temp_file_name;
  FILE                        *new_input_file;
  a_directory_name_entry_ptr  search_path;

  db_enter(2, "open_file_for_input");
  *dir_entry = NULL;
  search_path = NULL;
  if (use_search_path) {
    /* Determine the list of directories to be searched when opening
       the file. */
    if (is_include_next) {
      /* For #include_next, start at the search path entry after the one
         in which the current file was found. */
      search_path = curr_ise->dir_entry;
      if (search_path != NULL) search_path = search_path->next;
    } else if (is_system_include) {
      search_path = sys_incl_search_path;
    } else {
      search_path = incl_search_path;
    } /* if */
  }  /* if */
  new_input_file = NULL;
  *full_file_name = NULL;
  check_assertion((curr_ise == NULL) == (depth_input_stack == -1));
  /* Open the new file. */
  if (curr_ise == NULL && strcmp(file_name, FILE_NAME_FOR_STDIN) == 0) {
    /* Special code for stdin; no open needed. */
    temp_file_name = file_name;
    new_input_file = stdin;
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
  } else if (replace_suffix) {
    new_input_file = search_for_input_file(
                                       file_name, use_search_path, search_path,
                                       implicit_instantiation_file_suffix_list,
                                       replace_suffix, &temp_file_name,
                                       dir_entry);
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
  } else {
    new_input_file = search_for_input_file(
                                       file_name, use_search_path, search_path,
                                       include_file_suffix_list,
                                       /*replace_suffix=*/FALSE,
                                       &temp_file_name, dir_entry);
    if (new_input_file == NULL) {
      /* The file could not be opened. */
      str_catastrophe(ec_source_file_could_not_be_opened, file_name);
    }  /* if */
  }  /* if */
  if (new_input_file != NULL) {
    /* If the name is in "buffer", allocate it now.  The names are generated 
       there first because many directory/file name combinations might be tried
       before the right one is found.  We don't allocate space for the name 
       until we find a file of that name. */
    *full_file_name = temp_file_name;
    /* Note that *display_name gets the same name as full name.  This is
       a matter of taste. */
    *display_name = temp_file_name;
  }  /* if */
  db_exit();
  return new_input_file;
}  /* open_file_for_input */


static
int look_for_file_on_input_stack(char	*file_name)
/*
Look for the file specified by file_name in the input stack and return
the number of times that the file appears there.
*/
{
  int	times_name_appears;
  int	isnum;

  times_name_appears = 0;
  for (isnum = depth_input_stack; isnum >= 0; isnum--) {
    if (compare_file_names(input_stack[isnum].full_name, file_name) == 0) {
      times_name_appears++;
    }  /* if */
  }  /* for */
  return times_name_appears;
}  /* look_for_file_on_input_stack */


void push_input_stack(
		FILE     			*new_input_file,
                char    			*name_as_written,
                char     			*display_name,
                char     			*full_file_name,
		a_boolean			is_include_file,
		a_boolean		 	is_system_include,
                a_boolean                       is_preinclude,
                a_boolean			is_implicit_include,
                a_directory_name_entry_ptr      dir_entry,
		an_include_file_history_ptr	ifhp)
/*
Push the indicated file onto the input stack.  name_as_written,
display_name, and full_file_name are various forms of the file name.
is_include_file is TRUE if the file is being read as the result of a
#include directive or a --preinclude command-line-option.  It is FALSE
for implicitly included files.  is_system_include is TRUE for files
included with the #include <file.h> notation and FALSE for all other
files.  is_preinclude is TRUE for files included via the --preinclude
command-line option.  is_implicit_include is TRUE for files included
for template implicit inclusion.  dir_entry points to the entry on
the search path that was used to find this file.
*/
{
  int                times_name_appears;
  a_source_file_ptr  parent_file;

  db_enter(2, "push_input_stack");
#if DEBUG
  if (debug_level >= 2) {
    fprintf(f_debug, "file_name = %s\n", full_file_name);
  }  /* if */
#endif /* DEBUG */
  /* Check for recursion of #includes.  This is done by looking through the
     stack for the file name we just opened. */
  times_name_appears = look_for_file_on_input_stack(full_file_name);
  /* The entry in the input stack has the same file name as the
     file we just opened.  This is okay once (it has to be), but
     if it happens several times, it probably means recursion. */
  if (times_name_appears >= 10 /* Arbitrary, must be > 1 */) {
    str_catastrophe(ec_include_recursion, full_file_name);
  }  /* if */
  /* If preprocessing output is being generated, force out the previous
     source line before the input stack information is changed. */
  if (generate_pp_output) {
    gen_pp_output_for_curr_line();
  }  /* if */
  /* If a raw listing file is being generated, force out the previous
     line before the input stack information is changed. */
  if (f_raw_listing != NULL) {
    gen_raw_listing_output_for_curr_line();
  }  /* if */
  /* Reset the per-line state of whether token separators should be emitted
     in the generated output. */
  no_token_separators_in_this_line_of_pp_output =
                                             no_token_separators_in_pp_output;
  /* Check for the need to expand the input stack. */
  if (depth_input_stack+1 == size_input_stack) {
    /* Expand the input stack by reallocating it. */
    int new_size = size_input_stack + INPUT_STACK_INCREMENTAL_ALLOCATION;
    input_stack = (an_input_stack_entry_ptr)realloc_general(
                   (char *)input_stack,
                   (sizeof_t)(size_input_stack*sizeof(an_input_stack_entry)),
                   (sizeof_t)(new_size*sizeof(an_input_stack_entry)));
    size_input_stack = new_size;
    if (depth_input_stack >= 0) curr_ise = &input_stack[depth_input_stack];
  }  /* if */
  /* If the maximum number of files has already been opened, close the
     top-most file in the input stack after remembering its current
     position for later re-opening.  This prevents hogging of system
     resources, and works efficiently for moderate nesting depths. */
  if (depth_input_stack >= MAX_INCLUDE_FILES_OPEN_AT_ONCE) {
    curr_ise->position = ftell(curr_ise->file);
    (void)fclose(curr_ise->file);
    curr_ise->file = NULL;
  }  /* if */
  /* Push the new input stack entry. */
  curr_ise = &input_stack[++depth_input_stack];
  curr_ise->file        = new_input_file;
  curr_ise->line_number = 0;
  curr_ise->position    = 0;
  curr_ise->actual_line = 0;
  /* Update other variables describing the current state. */
  eof_read_on_curr_input_stream = FALSE;
  curr_input_stream = curr_ise->file;
  /* Save the "display" form of the name and the full name. */
  curr_ise->full_name = full_file_name;
  curr_ise->file_name = display_name;
  curr_ise->dir_name = directory_of(full_file_name);
  curr_ise->dir_entry = dir_entry;
  curr_ise->is_include_file = is_include_file;
  curr_ise->nested_inclusion = (times_name_appears != 0);
  curr_ise->include_history   = ifhp;
  curr_ise->ifg_state = IFG_STATE_START;
  curr_ise->saved_any_tokens_fetched =
				      any_tokens_fetched_from_curr_input_file;
  any_tokens_fetched_from_curr_input_file = FALSE;
#if CHECKING
  curr_ise->avoid_codecenter_warnings = 0;
#endif /* CHECKING */
  /* Create an intermediate file record describing this file.  It is
     useful later in converting sequence numbers into file name/line
     information. */
  if (depth_input_stack == 0) {
    if (!is_implicit_include) {
      parent_file = NULL;
    } else {
      /* A file included for template implicit inclusion is put under
         the primary file. */
      parent_file = il_header.primary_source_file;
      /* after_end_of_all_source was set TRUE when we reached the end of the
         primary source file.  Reset it so that we can continue accepting
         input from the implicitly included template definition files. */
      after_end_of_all_source = FALSE;
    }  /* if */
  } else {
    parent_file = input_stack[depth_input_stack-1].assoc_il_file;
  }  /* if */
  record_start_of_source_file(parent_file,
                              (a_seq_number)seq_number_last_read+1,
                              (a_line_number)1, display_name,
                              full_file_name, name_as_written,
                              &(curr_ise->assoc_il_file), is_include_file,
                              is_system_include, is_preinclude,
			      (dir_entry != NULL &&
                                               dir_entry->system_include_dir));
  /* The two il file pointers start out the same.  They will be made to
     point to distinct entries if a #line directive is processed:
     assoc_il_file will point to the entry for the #line, and
     assoc_actual_il_file will stay as it is (pointing to the entry
     for the file actually being read). */
  curr_ise->assoc_actual_il_file = curr_ise->assoc_il_file;
  /* Initialize the source file index used by the diagnostic routines
     to reread source lines when needed. */
  curr_ise->next_index_point = initialize_file_index(
                                            curr_ise->assoc_actual_il_file);
  /* If generating preprocessing output, put out a line-identifying
     directive for the new file. */
  if (generate_pp_output) {
    /* The entry into the primary source file should not be tagged with
       "1"; that's the way cpp does it. */
    if (depth_input_stack == 0) {
      gen_pp_line_info(' ', /*next_line=*/TRUE);
    } else {
      gen_pp_line_info('1', /*next_line=*/TRUE);
    }  /* if */
  }  /* if */
  /* If generating raw listing output (for input to a program that will
     generate an interspersed listing), put out a line-information record. */
  if (f_raw_listing != NULL) {
    /* The entry into the primary source file should not be tagged with "1". */
    if (depth_input_stack == 0) {
      gen_rlisting_line_info(' ');
    } else {
      gen_rlisting_line_info('1');
    }  /* if */
  }  /* if */
  /* If generating makefile dependency information (-M option), write a
     line of the form

     primaryfile.o: includefile.h

     Note that the code here is executed also for the primary source
     file, and we do want that dependency line as well. */
  if (list_makefile_dependencies) {
    fprintf(f_pp_output, "%s: %s\n", object_file_name, curr_ise->file_name);
  }  /* if */
  /* If generating a list of include files (-H option), put out the
     file name.  Do not put out the name of the primary source file. */
  if (list_included_files && depth_input_stack != 0) {
    display_included_file_name(depth_input_stack, curr_ise->file_name);
  }  /* if */
  if (!curr_ise->assoc_actual_il_file->top_level_file) {
    /* Modify the search rules for #include directives found within this source
       file, so that the directory containing the current include file will be
       searched first.  Note that this is not done for the primary source
       file.  The include search entry for the primary source file is
       managed by the routines in cmd_line.c. */
    push_primary_include_search_dir(curr_ise->dir_name);
  }  /* if */
  if (C_dialect != C_dialect_pcc) {
    /* If not in pcc mode, keep the base of the preprocessing if stack
       up to date.  Each file's #ifs are kept separate; an #if must
       be ended in the same file in which it began. */
    curr_ise->base_pp_if_stack_depth = base_pp_if_stack_depth =
                                                          pp_if_stack_depth;
  }  /* if */
#if DEBUG
  if (debug_level >= 5) {
    a_directory_name_entry_ptr	dnep = incl_search_path;
    fprintf(f_debug, "Include search path after pushing %s:\n",
            full_file_name);
    while (dnep != NULL) {
      fprintf(f_debug, "  %s\n", dnep->dir_name);
      dnep = dnep->next;
    }  /* while */
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* push_input_stack */


void pop_input_stack(void)
/*
Pop the input stack, and correctly prepare for input from the file
at the next level down.
*/
{
  a_boolean	is_end_of_primary_source_file = TRUE;
  a_byte	ifg_state;

  db_enter(2, "pop_input_stack");
#if DEBUG
  if (debug_level >= 4) {
    db_include_guard_info();
  }  /* if */
#endif /* DEBUG */
  /* This is where we do a final check to see if subsequent inclusions of
     the file can potentially be suppressed. If we are in the "accept" or
     "once" state, then this file satisfies the criteria, otherwise it
     does not.  Unless the file contained an explicit #pragma once, the
     final determination of whether an actual subsequent include of this
     file can be suppressed can only be determined at the point of the
     include because the controlling macro must be tested at that point. */
  ifg_state = get_ifg_state();
  if (ifg_state != IFG_STATE_ACCEPT &&
      ifg_state != IFG_STATE_ONCE &&
      ifg_state != IFG_STATE_START) {
    /* Not a candidate for include suppression. */
  } else {
    /* This file does meet the criteria for suppression of subsequent
       includes. */
    curr_ise->include_history->suppress_subsequent_include = TRUE;
  }
  /* Restore the previous value of the any_tokens_fetched flag. */
  any_tokens_fetched_from_curr_input_file =
					  curr_ise->saved_any_tokens_fetched;
  /* Remember the final sequence number in the file, for sequence number
     mapping purposes. */
  record_end_of_source_file(curr_ise->assoc_actual_il_file,
                            seq_number_last_read);
  /* If a #line is in effect, also enter the final sequence number in the
     entry for that. */
  if (curr_ise->assoc_actual_il_file != curr_ise->assoc_il_file) {
    record_end_of_source_file(curr_ise->assoc_il_file, seq_number_last_read);
  }  /* if */
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
  if (depth_input_stack == 0) {
    /* The depth of the input stack can be zero for two reasons; we may
       have reached the end of the primary source file or we may have
       reached the end of a file implicitly included during template
       instantiation processing.  If this is the end of an implicitly
       included template definition file then update the last sequence
       number of the primary source file to include the sequence numbers
       of the statements read from the template definition file. */
    if (!curr_ise->assoc_actual_il_file->top_level_file) {
      record_end_of_source_file(il_header.primary_source_file,
                                seq_number_last_read);
      is_end_of_primary_source_file = FALSE;
    }  /* if */
  }  /* if */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
  /* Close the current input file. */
  (void)fclose(curr_input_stream);
  eof_read_on_curr_input_stream = FALSE;
  /* If preprocessing output is being generated, force out the previous
     source line before the input stack information is changed. */
  if (generate_pp_output) {
    gen_pp_output_for_curr_line();
  }  /* if */
  /* If a raw listing file is being generated, force out the previous
     line before the input stack information is changed. */
  if (f_raw_listing != NULL) {
    gen_raw_listing_output_for_curr_line();
  }  /* if */
  /* Reset the per-line state of whether token separators should be emitted
     in the generated output. */
  no_token_separators_in_this_line_of_pp_output =
                                             no_token_separators_in_pp_output;
  /* Check that all #ifs were closed.  In ANSI, this has to happen at
     the end of each source file.  In pcc, this has to happen only at
     the end of the whole compilation. */
  if (depth_input_stack == 0 || C_dialect != C_dialect_pcc) {
    verify_that_all_pp_ifs_were_closed();
  }  /* if */
  /* Pop the input stack. */
  /* If the stack is empty, there is no current input file any more.
     This happens at the end of the primary source file. */
  if (--depth_input_stack < 0) {
    curr_ise = NULL;
    curr_input_stream = NULL;
    if (!is_end_of_primary_source_file) {
      /* When a top-level implicitly included source file is popped,
         reset the primary include search directory to the directory of
         the primary source file. */
      pop_primary_include_search_dir(dir_name_of_primary_source_file);
    }  /* if */
  } else {
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
    an_input_stack_entry_ptr  prev_ise = curr_ise;
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
    curr_ise = &input_stack[depth_input_stack];
    if (curr_ise->file == NULL) {
#if DEBUG
      if (debug_level >= 2) {
        fprintf(f_debug, "re-opening at level %d, name = \"%s\", pos = %ld\n",
                         depth_input_stack, curr_ise->full_name,
                         curr_ise->position);
      }  /* if */
#endif /* DEBUG */
      /* This include file was closed on a push_input_stack to keep down
         the number of open files.  Re-open it and reposition it now. */
      if ((curr_ise->file = reopen_source_file(curr_ise->full_name)) == NULL) {
        /* File could not be re-opened; it was probably deleted since the
           compilation started. */
        str_catastrophe(ec_source_file_could_not_be_opened,
                        curr_ise->full_name);
      }  /* if */
      if (fseek(curr_ise->file, curr_ise->position, SEEK_SET) != 0) {
        /* The seek could not be done.  Again, this implies some change
           in the file since last it was opened. */
        str_catastrophe(ec_source_file_could_not_be_opened,
                        curr_ise->full_name);
      }  /* if */
#if __VMS__ && 0
      /* This change was only necessary for some of the later 4.n versions
	 of VMS, before a library bug was corrected. */
      /* On VMS, ftell returns the position of the start of the previous
         record (i.e., the line containing the #include), so advance to
         the next record. */
      while (fgetc(curr_ise->file) != '\n') {};
#endif /* __VMS__ */
    }  /* if */
    curr_input_stream = curr_ise->file;
    /* If generating preprocessing output, put out a line-identifying
       directive for the new file. */
    if (generate_pp_output) {
      gen_pp_line_info('2', /*next_line=*/TRUE);
    }  /* if */
    /* If generating raw listing information (for input to a program that
       will generate an interspersed listing), put out line information. */
    if (f_raw_listing != NULL) {
      gen_rlisting_line_info('2');
    }  /* if */
    /* Modify the search rules for #include directives found within this
       source file, so that the directory containing the current include
       file will be searched first. */
    pop_primary_include_search_dir(curr_ise->dir_name);
    if (C_dialect != C_dialect_pcc) {
      /* If not in pcc mode, keep the base of the preprocessing if stack
         up to date.  Each file's #ifs are kept separate; an #if must
         be ended in the same file in which it began. */
      base_pp_if_stack_depth = curr_ise->base_pp_if_stack_depth;
    }  /* if */
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
    if (list_makefile_dependencies && do_preprocessing_only &&
	prev_ise->is_include_file &&
        !prev_ise->assoc_actual_il_file->included_by_preinclude &&
        !prev_ise->nested_inclusion && !C_mode() &&
        implicit_template_inclusion_mode) {
      /* When generating makefile dependency information in C++ mode, and
         if implicit inclusion is enabled, look for a source file related
         to the current include file and include it if found.  This may include
         some files that would not be included in a full compilation
         that uses implicit inclusion because the preprocessor cannot
         determine whether or not the include file defined any
         templates and, if so, whether the templates were used in a
         way that requires the related source file to be read. */
      char		*full_file_name;
      char		*display_name;
      FILE		*f_source;
      a_directory_name_entry_ptr
                        dir_entry;
      a_source_file_ptr	sfp = prev_ise->assoc_actual_il_file;
      f_source = open_file_for_input(
                              sfp->name_as_written, /*use_search_path=*/TRUE,
                              (a_boolean)sfp->included_by_system_include,
                              /*is_include_next=*/FALSE,
 			      /*replace_suffix=*/TRUE,
			      &full_file_name, &display_name,
                              &dir_entry);
      if (f_source != NULL) {
        /* A related source file was found.  Make sure that the name of the
           file found is not the same as the file we started with.  This
           could occur if the user included a .c file that contains a
           template declaration. */
        if (compare_file_names(full_file_name, sfp->full_name) != 0) {
	  an_include_file_history_ptr	ifhp;
#if DEBUG
          if (debug_level >= 3) {
            fprintf(f_debug, "  Including text from '%s'\n", full_file_name);
          }  /* if */
#endif /* DEBUG */
          /* Push the new file onto the input stack and scan it.  There is
             no "name as written" so a NULL pointer is passed in. */
	  if (suppress_subsequent_include_of_file(full_file_name, &ifhp) ||
              (implicit_template_inclusion_mode &&
               look_for_file_on_input_stack(full_file_name) > 0)) {
            /* This file contains include guard code or is already on the input
               stack more than once.  An inclusion of a guarded file would
	       have no effect and so, is suppressed.  A file that is already on
               the stack more than once is probably an include loop caused
	       by looking for a file that can be implicitly included in a
	       context in which no implicit include would actually be done
	       in a real compilation. */
	    (void)fclose(f_source);
#if DEBUG
	    if (debug_level >= 3) {
	      fprintf(f_debug,
		      "pop_input_stack: skipping include file %s\n",
		      full_file_name);
            }  /* if */
#endif /* DEBUG */
	  } else {
            push_input_stack(f_source, (char *)NULL, display_name,
                             full_file_name, /*is_include_file=*/FALSE,
                             (a_boolean)sfp->included_by_system_include,
			     /*is_preinclude=*/FALSE,
                             /*is_implicit_include=*/TRUE,
                             dir_entry, ifhp);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
#endif  /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
    /* Check whether a PCH file should be generated at the end of the
       execution of this include directive. */
    check_for_generation_of_pch_on_return_to_primary_file();
  }  /* if */
#if DEBUG
  if (debug_level >= 5) {
    a_directory_name_entry_ptr	dnep = incl_search_path;
    fprintf(f_debug, "Include search path after popping:\n");
    while (dnep != NULL) {
      fprintf(f_debug, "  %s\n", dnep->dir_name);
      dnep = dnep->next;
    }  /* while */
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* pop_input_stack */


static void expand_curr_source_line(void)
/*
We have run into a source line that won't fit in curr_source_line;
reallocate curr_source_line to make it bigger.
*/
{
  sizeof_t old_size, new_size;
  char     *new_curr_source_line;

  db_enter(4, "expand_curr_source_line");
  old_size = after_end_of_curr_source_line - curr_source_line;
  /* Increase the size of curr_source_line. */
  new_size = old_size + CURR_SOURCE_LINE_INCREMENTAL_ALLOCATION;
  /* Allocate one more byte than required, so that a pointer past the end
     will not have the same address as a pointer to the next object in
     memory. */
  new_curr_source_line = realloc_general(curr_source_line,
                                         (sizeof_t)(old_size+1),
                                         (sizeof_t)(new_size+1));
  /* Update any pointers to the old curr_source_line in the
     curr_source_line data structure. */
  adjust_curr_source_line_structure_after_realloc(curr_source_line,
                                                 after_end_of_curr_source_line,
                                                  new_curr_source_line);
  curr_source_line = new_curr_source_line;
  after_end_of_curr_source_line = curr_source_line + new_size;
  db_exit();
}  /* expand_curr_source_line */


void ensure_min_curr_source_line_length(sizeof_t  min_len)
/*
Make sure the curr source line can hold at least min_len characters.
*/
{
  while (min_len >
               (sizeof_t)(after_end_of_curr_source_line - curr_source_line)) {
    expand_curr_source_line();
  }  /* while */
}  /* ensure_min_curr_source_line_length */


void conv_line_loc_to_source_pos(char              *loc_in_line,
                                 a_source_position *position_var)
/*
Convert a pointer to somewhere in curr_source_line or macro_buffer
(loc_in_line) into the corresponding source sequence number and column
(in position_var).  This is used to develop a position for later use
in error reporting.  This function version is for cases where the processing
need not be very fast; macro_line_loc_to_source_pos should be used
when speed is critical.
*/
{
  char                    *adj_loc_in_line;
  a_source_line_modif_ptr slmp, parent_slmp, orig_slmp;
  an_orig_line_modif_ptr  olmp                     = orig_line_modif_list;
  char                    *start_of_curr_phys_line = curr_source_line;
  a_seq_number            seq_number               = curr_seq_number;
  int                     trigraph_adjustment      = 0;

  adj_loc_in_line = loc_in_line;
  orig_slmp = NULL;
  if (!within_curr_source_line(adj_loc_in_line)) {
    /* If loc_in_line is not in curr_source_line, it must be in a macro
       expansion or macro argument.  Find the location in curr_source_line
       that begins the macro expansion that ultimately generates
       adj_loc_in_line.  This gives us a source line position we can
       convert.  Remember the innermost modification entry in orig_slmp
       so the position can be put into it once determined. */
    orig_slmp = slmp = assoc_source_line_modif(adj_loc_in_line);
    for (;;) {
      /* If a source line modification includes a source position, we
         are done. */
      if (slmp->source_position.seq != 0) {
        *position_var = slmp->source_position;
        goto have_position;
      }  /* if */
      parent_slmp = parent_source_line_modif(slmp);
      if (parent_slmp == NULL) break;
      slmp = parent_slmp;
    }  /* for */
    /* The topmost modification is not for a macro, so we will have to
       work out the position from the line location. */
    adj_loc_in_line = loc_of_insert(slmp);
  }  /* if */
  /* We now have in adj_loc_in_line a position within curr_source_line, which
     must be converted to the corresponding source position. */
  if (olmp != NULL) {
    /* There are trigraphs and/or line splices, so the position
       must be determined by finding where adj_loc_in_line falls relative
       to those modifications in the line. */
    do {
      if (adj_loc_in_line < olmp->line_loc) {
        /* This position precedes the current entry, so we now know
           which physical line adj_loc_in_line is in. */
        break;
      } else if (olmp->kind == olm_line_splice ||
                 olmp->kind == olm_multiline_string_splice) {
        /* In the case that a line splice is followed by the end of the
           logical source line, use the position of the "\" on the current
           line as the error position.  This is useful when the last line
           of a file ends with a backslash. */
        if (*adj_loc_in_line   == LE_ESCAPE &&
            adj_loc_in_line[1] == LE_NEWLINE) break;
        /* Keep track of the current physical line. */
        start_of_curr_phys_line = olmp->line_loc;
        if (olmp->kind == olm_multiline_string_splice) {
          start_of_curr_phys_line += 2;
        }  /* if */
        seq_number              = olmp->variant.line_splice_seq_number;
        trigraph_adjustment     = 0;
      } else {
#if CHECKING
        if (olmp->kind != olm_trigraph) {
          internal_error(
                 "conv_line_loc_to_source_pos: bad orig line modification");
        }  /* if */
#endif /* CHECKING */
        /* Keep a column adjustment to compensate for trigraphs. */
        trigraph_adjustment += 2;
      }  /* if */
    } while ((olmp = olmp->next) != NULL);
  }  /* if */
  /* Set the source position now that we know what physical line we 
     are in and how many trigraphs appear before adj_loc_in_line on this
     line. */
  position_var->seq    = seq_number;
  position_var->column = adj_loc_in_line - start_of_curr_phys_line +
                         trigraph_adjustment + 1;
have_position:
  /* Save the position determined in the innermost source line modification
     that covers this location.  That will make succeeding calls of
     this routine faster. */
  if (orig_slmp != NULL) orig_slmp->source_position = *position_var;
}  /* conv_line_loc_to_source_pos */


/*
Convert a pointer to somewhere in curr_source_line or macro_buffer
(loc_in_line) into the corresponding source sequence number and column
(in position_var).  This is used to develop a position for later use
in error reporting.  This macro version is for cases where the processing
must be very fast; conv_line_loc_to_source_pos should be used
when speed is not so critical.

If the position is within curr_source_line (i.e., it's not in a macro
expansion), and there is no modification information of any kind,
curr_source_line is all one line with no trigraphs to perturb the
column numbers.  The position can be determined directly.  This is the
most common case.
*/
#define macro_line_loc_to_source_pos(loc_in_line, position_var) \
{ if (no_modifs_to_curr_source_line || \
      (within_curr_source_line(loc_in_line) && \
       orig_line_modif_list == NULL)) { \
    (position_var).seq    = curr_seq_number; \
    (position_var).column = (loc_in_line) - curr_source_line + 1; \
  } else { \
    conv_line_loc_to_source_pos((loc_in_line), &(position_var)); \
  }  /* if */ \
}  /* macro_line_loc_to_source_pos */


static void diagnostic_at_line_pos(an_error_severity  severity,
                                   an_error_code      error_code,
                                   char               *loc_in_line)
/*
Record the occurrence of the indicated diagnostic at the indicated character
position of the current logical source line.
*/
{
  /* Convert the character position into an error position. */
  conv_line_loc_to_source_pos(loc_in_line, &error_position);
  diagnostic(severity, error_code);
}  /* diagnostic_at_line_pos */


/*
Record the occurrence of the indicated error at the indicated character
position of the current logical source line.
*/
#define error_at_line_pos(error_code, loc_in_line)                    \
  diagnostic_at_line_pos(es_error, (error_code), (loc_in_line));


/*
Record the occurrence of the indicated warning at the indicated character
position of the current logical source line.
*/
#define warning_at_line_pos(error_code, loc_in_line)                  \
  diagnostic_at_line_pos(es_warning, (error_code), (loc_in_line));


/*
Macro to temporarily add newline/end-line to the current contents of the
source buffer.  Needed when an error is detected while building the
source line.  Without the newline/end-line, the partial line could not
be properly displayed with the error message.  Fortunately, the errors
that there are occur at the end of lines, so the "partial" line is really
the full line.
*/
#define finish_off_source_line_so_it_can_be_displayed_in_error()      \
{ *loc_in_line   = LE_ESCAPE; loc_in_line[1] = LE_NEWLINE; \
  loc_in_line[2] = LE_ESCAPE; loc_in_line[3] = LE_END_OF_LINE; }


/*
Test a character to see if it is an end-of-file character.
*/
#if !READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS
#define is_eof_char(ch) ((ch) == EOF)
#else /* READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS */
/* On MS-DOS when reading source files in binary mode, a control-Z
   acts as an EOF. */
#define is_eof_char(ch) ((ch) == EOF || (ch) == CONTROL_Z)
#endif /* !READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS */

#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
#if BACKSLASH_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR
#define MBC_CHECKING_NEEDED_IN_LINE_READING TRUE
#else /* !BACKSLASH_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR */
#if QUESTION_MARK_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR
#define MBC_CHECKING_NEEDED_IN_LINE_READING TRUE
#else /* !QUESTION_MARK_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR */
#define MBC_CHECKING_NEEDED_IN_LINE_READING FALSE
#endif /* QUESTION_MARK_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR */
#endif /* BACKSLASH_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR */
#else /* !MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
#define MBC_CHECKING_NEEDED_IN_LINE_READING FALSE
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */

#if MBC_CHECKING_NEEDED_IN_LINE_READING

static void find_offset_for_source_line_mbc_including(
                                                     char          *new_char,
                                                     unsigned long *mbc_offset)
/*
*mbc_offset is the 0-origined offset to a position in curr_source_line that
is on a multibyte character boundary.  Advance from that position over whole
multibyte character sequences to find the start position of the multibyte
character sequence that contains new_char.  Return *mbc_offset set to
the offset of that start position.  An offset is used for mbc_offset,
rather than a pointer, to avoid the need to remap the pointer if
curr_source_line is resized.
*/
{
  unsigned long offset = *mbc_offset;
  char          *ptr;
  int           numch;

  /* If we're already too far in the line, start over. */
  if (curr_source_line+offset > new_char) offset = 0;
  /* If we're starting at the beginning of the line, make sure any shift
     states are reset. */
  if (offset == 0) mbc_scan_init();

  /* Step through the characters of the source line, stepping over
     multibyte character sequences. */
  for (ptr = curr_source_line+offset;; ptr += numch, offset += numch) {
    numch = mbc_length(ptr, (a_boolean *)NULL);
    if (ptr + numch > new_char) break;
  }  /* for */

  *mbc_offset = offset;
}  /* find_offset_for_source_line_mbc_including */

#endif /* MBC_CHECKING_NEEDED_IN_LINE_READING */

a_boolean read_logical_source_line(a_boolean do_pop_on_end_of_file,
                                   a_boolean extend_current_line)
/*
Read the next logical source line into curr_source_line and
the related variables.  A "logical source line" is what results after
trigraph characters (see standard, 2.2.1.1) have been replaced, and
lines ending in newline-backslash have been spliced with the lines
immediately following (see standard, 2.1.1.2).  The logical source
line thus formed is returned in curr_source_line, terminated
with an LE_NEWLINE and an LE_END_OF_LINE lexical escape sequence.
The input is read from curr_input_stream.  Information that allows
the mapping of characters in curr_source_line back to the corresponding
source sequence number and column is maintained in orig_line_modif_list.

If end of file is not encountered, curr_char_loc is pointed at the first
character of the line just read.

If the end of the current input file is reached, then:

1)  If do_pop_on_end_of_file is TRUE, pop_input_stack is called (perhaps
more than once) to get to the next line of input, and that is returned
in the usual way.  If the end of the primary source file is reached
and that file is popped, then after_end_of_all_source is set to TRUE,
an empty line (just an LE_END_OF_LINE lexical escape) is placed in
curr_source_line, and curr_char_loc is pointed at the line-end escape.

2)  If do_pop_on_end_of_file is FALSE, then the current line and the
current position within it are left unchanged, and at_end_of_source_file is
set to TRUE.

The return value from this function is at_end_of_source_file ||
after_end_of_all_source -- i.e., TRUE if no current source line was read.

If extend_current_line is TRUE, read more characters onto the end of
the current source line instead of beginning a new line.  This is used
for the GNU C multiline string extension.
*/
{
  int             ch;
  char            *loc_in_line;
  a_boolean       return_value;
  unsigned long   curr_column;
#if MBC_CHECKING_NEEDED_IN_LINE_READING
  unsigned long   mbc_offset = 0;
#endif /* MBC_CHECKING_NEEDED_IN_LINE_READING */
  int             next_ch;
  a_boolean       char_is_trapped = FALSE, has_invalid_char = FALSE;
  an_orig_line_modif_ptr
		  olmp;
  a_source_line_modif_ptr
		  slmp;
  sizeof_t        offset_in_line, offset_to_invalid_char;
  char		  *after_curr_source_line_minus_term =
                               after_end_of_curr_source_line - 2*LE_ESCAPE_LEN;
		       /* For checking of buffer overflow -- to leave
                          room for the newline and line-end lexical escapes. */

  /* This routine handles translation phases 1 (trigraphs, newlines) and
     2 (line splices) from the description of translation phases in
     2.1.1.2 of the standard. */
  /* If we are being asked to extend the current line, go straight to
     the slow loop.  */
  if (extend_current_line) goto entry_for_extend_current_line;
  /* If the compiler is being run just to produce preprocessing output,
     and not to compile (i.e., it's supposed to act like cpp), dump the
     previous line of input (possibly modified since being read in) to the
     preprocessing output file. */
  if (generate_pp_output) {
    gen_pp_output_for_curr_line();
  }  /* if */
  /* If a raw listing file is being generated, write out the previous
     line. */
  if (f_raw_listing != NULL) {
    gen_raw_listing_output_for_curr_line();
  }  /* if */
  /* Reset the per-line state of whether token separators should be emitted
     in the generated output. */
  no_token_separators_in_this_line_of_pp_output =
                                             no_token_separators_in_pp_output;
  /* Get the first character of the line, checking for end of file in
     doing so.  If eof_read_on_curr_input_stream is already TRUE,
     the end of file has already been read (this handles the case
     where the previous call of this routine read an incomplete last
     line; this call needs to return the end of file indication). */
  while (eof_read_on_curr_input_stream ||
         (ch = getc(curr_input_stream), is_eof_char(ch))) {
    /* End of file encountered in the expected way, i.e., before a line
       has started. */
    eof_read_on_curr_input_stream = TRUE;
    at_end_of_source_file = TRUE;
    if (!do_pop_on_end_of_file) {
      /* We're asked not to do the pop, so just return things as they
         are (at_end_of_source_file is TRUE). */
      goto simple_return;
    }  /* if */
    /* We are supposed to pop the input stack and attempt again to
       read the next line. */
    pop_input_stack();
    at_end_of_source_file = FALSE;
    if (depth_input_stack < 0) {
      /* We have popped out of the primary source file; this is the real
         end of file. */
      after_end_of_all_source = TRUE;
      break;
    }  /* if */
    /* Loop to try reading from the file reopened by pop_input_stack. */
  }  /* while */
  /* Either the end of all source, or a real line to read.  For the
     end of source case, a line with just a line-end lexical escape
     is placed in curr_source_line and the sequence number is incremented
     to an "after all source" position. */
  loc_in_line = curr_source_line;
  curr_seq_number = ++seq_number_last_read;
  /* If there are entries on either of the lists indicating modifications
     to the current source line, clear those lists now, since they are for the
     old source line.  We also want the lists empty to start building
     them for the new line. */
  if (orig_line_modif_list != NULL) {
    do {
      olmp = orig_line_modif_list;
      orig_line_modif_list = orig_line_modif_list->next;
      free_orig_line_modif(&olmp);
    }  while (orig_line_modif_list != NULL);
  }  /* if */
  if (source_line_modif_list != NULL) {
    do {
      slmp = source_line_modif_list;
      rem_source_line_modif(slmp);
      free_source_line_modif(&slmp);
    } while (source_line_modif_list != NULL);
  }  /* if */
  no_modifs_to_curr_source_line = TRUE;
  if (after_end_of_all_source) {
    /* End of all source.  Go end the line with a line-end sequence and
       return. */
    goto return_with_line;
  } else {
    /* Not end of file, read the line. */
    curr_ise->line_number++;
    /* Check if this line being read is that next needed for the file index
       table.  Remember that the first character has already been read into
       ch. */
    if (++(curr_ise->actual_line) == curr_ise->next_index_point) {
      curr_ise->next_index_point = update_file_index(
                                        curr_ise->assoc_actual_il_file,
                                        curr_ise->actual_line,
                                        ftell(curr_ise->file) - 1);
    }  /* if */
    /* Read characters until the newline indicating end of line. */
    /* Every attempt is made to make this FAST, since every character of
       the source program passes through this loop.  The assumption is
       that trigraphs will almost never appear, and that line splices will
       not appear too often (mostly in long macro definitions).
       In general, we try to do tests that are cheap in the normal case,
       and we exit out to a general-purpose expensive version of this
       algorithm on anything out of the ordinary.  The exits out are
       by simple goto, to keep even non-executed code out of the inner
       loop, to improve pipelining. */
    if (ch != '\n') {
      /* Use local variables in the inner loop, because some compilers
         have trouble optimizing this otherwise. */
      register char *local_loc_in_line = loc_in_line;
      register int local_ch = ch;
      do {
        /* Check for question marks.  Presence of 2 in a row suggests there
           may be a trigraph in the line. */
        if (local_ch == '?') {
          /* One "?", check previous character to see if it is also a "?". */
          if (local_loc_in_line != curr_source_line &&
              *(local_loc_in_line-1) == '?') {
            /* On reasonable suspicion of a trigraph, exit to more expensive
               processing code.  Note that this is done before the second
               "?" is stored, so that we do not ever store more characters
               than ultimately required, and therefore avoid spurious
               buffer overflows on trigraphs at the ends of very long lines. */
            ch = local_ch;
            loc_in_line = local_loc_in_line;
            goto possible_trigraph;
          }  /* if */
        } else if (local_ch == LE_ESCAPE) {
          /* The zero character is reserved for internal use.  Replace it
             by a blank and save the error position for later display. */
          local_ch = ' ';
          if (!has_invalid_char) {
            has_invalid_char = TRUE;
            offset_to_invalid_char = local_loc_in_line - curr_source_line;
          }  /* if */
        }  /* if */
        /* Check that there is still room in the line buffer.  We have to
           leave room for both the final newline and line-end escapes. */
        if (local_loc_in_line == after_curr_source_line_minus_term) {
          /* The line is too long; the buffer must be expanded.  Note that
             after the buffer is expanded we do not return to this loop for
             the current line; the rest of the line is processed in the
             more expensive loop. */
          ch = local_ch;
          loc_in_line = local_loc_in_line;
          goto expand_buffer;
        }  /* if */
        /* Put the character into curr_source_line. */
        *local_loc_in_line++ = (char)local_ch;
        /* Get next character, check for end of file without newline. */
        if (local_ch = getc(curr_input_stream), is_eof_char(local_ch)) {
          ch = local_ch;
          loc_in_line = local_loc_in_line;
          goto partial_final_line;
        }  /* if */
        /* Check for newline, which ends loop. */
      } while (local_ch != '\n');
      ch = local_ch;
      loc_in_line = local_loc_in_line;
#if IGNORE_CARRIAGE_RETURN_IN_SOURCE
      /* Ignore carriage return right before newline.  Ignore several if
         they are present (there are Microsoft header files that have this). */
      while (*(loc_in_line-1) == '\r') {
        loc_in_line--;
        /* Avoid the line splice test if the line is empty except for the
           carriage return. */
        if (loc_in_line == curr_source_line) {
          goto add_newline_and_line_end_and_return;
        }  /* if */
      }  /* while */
#endif /* IGNORE_CARRIAGE_RETURN_IN_SOURCE */
      /* End of a line containing at least one character.  Check to see
         if the last character is a backslash.  If so, the current line
         should be spliced with the line following. */
      if (*(loc_in_line-1) == '\\') goto line_splice;
    }  /* if */
  }  /* if */

add_newline_and_line_end_and_return:
  /* Store the final LE_NEWLINE lexical escape sequence. */
  *loc_in_line++ = LE_ESCAPE;
  *loc_in_line++ = LE_NEWLINE;

return_with_line:
  /* Store the final LE_END_OF_LINE lexical escape sequence. */
  *loc_in_line++ = LE_ESCAPE;
  *loc_in_line = LE_END_OF_LINE;
  if (has_invalid_char) {
    /* Put out an error if the line contains any invalid characters.  Only the
       position of the first one is identified. */
    error_at_line_pos(ec_invalid_char,
                      curr_source_line + offset_to_invalid_char);
  }  /* if */
  /* Set the input character position to the start of the line. */
  if (!extend_current_line) {
    curr_char_loc = curr_source_line;
    any_tokens_gotten_from_curr_source_line = FALSE;
  }  /* if */

simple_return:
  /* The return value is TRUE on any end-of-file case. */
  return_value = at_end_of_source_file || after_end_of_all_source;
  if (!extend_current_line) {
    /* The current line should not be put out as preprocessing output
       if there isn't a current line, if it's being skipped in an #if
       or the like, or if it is a line after the first in a preprocessing
       directive (that happens when there are multi-line comments in a
       preprocessing directive).  The flag will be set later for other
       preprocessing-related cases.  If this is a line after the first
       in a directive being passed unchanged to preprocessing output,
       put the line out. */
    if (generate_pp_output) {
      do_not_put_curr_line_in_pp_output = currently_in_pp_if_skip ||
                                          (in_preprocessing_directive &&
                                           !pass_pp_directive_to_output);
      /* Maintain a separate flag for any text inserted at the beginning of
         the line. */
      init_do_not_put_curr_line_in_pp_output =
                                             do_not_put_curr_line_in_pp_output;
      if (return_value) do_not_put_curr_line_in_pp_output = TRUE;
    }  /* if */
    if (f_raw_listing != NULL) {
      /* If a raw listing file is being generated, save the line type:
         "N" indicating that this is a source line, "S" if this line
         is part of an #if-skip, or '\0' if there is no source line. */
      curr_raw_listing_line_code = return_value ? '\0' :
                                         (currently_in_pp_if_skip ? 'S' : 'N');
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 1) {
    /* Display the line just read. */
    fprintf(f_debug, "Returning from read_logical_source_line, ");
    fprintf(f_debug, "return value = %d, ", return_value);
    if (after_end_of_all_source) {
      fprintf(f_debug, "\nafter_end_of_all_source = TRUE.\n");
    } else {
      fprintf(f_debug, "seq = %lu\n%s\n", curr_seq_number, curr_source_line);
      if (debug_level >= 4) {
        /* Dump out the modification list, which shows the location
           of the trigraphs and line splices. */
        for (olmp = orig_line_modif_list; olmp != NULL; olmp = olmp->next) {
          /* Put a caret under the proper character of the source line. */
          fprintf(f_debug, "%*c ",
                  (int)(olmp->line_loc-curr_source_line+1), '^');
          switch (olmp->kind) {
            case olm_trigraph:
              fprintf(f_debug, "trigraph: ??%c\n",
                               olmp->variant.trigraph_orig_char);
              break;
            case olm_line_splice:
              fprintf(f_debug, "line splice: seq = %lu\n",
                               olmp->variant.line_splice_seq_number);
              break;
            case olm_multiline_string_splice:
              fprintf(f_debug, "multiline string splice: seq = %lu\n",
                               olmp->variant.line_splice_seq_number);
              break;
#if CHECKING
            default:
              internal_error(
               "read_logical_source_line: bad orig_modif_list entry kind (2)");
#endif /* CHECKING */
          }  /* switch */
        }  /* for */
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* DEBUG */

  return (return_value);

expand_buffer:
  /* The curr_source_line buffer is too small to fit the next character,
     so go into the expensive loop and expand it. */
  curr_column = loc_in_line - curr_source_line + 1;
  goto entry_for_expand_buffer;

partial_final_line:
  /* The final line of a file does not end with a newline.  Issue a warning
     (or an error in strict mode), add a newline and line-end to the line, and
     return. */
  eof_read_on_curr_input_stream = TRUE;
  finish_off_source_line_so_it_can_be_displayed_in_error();
  diagnostic_at_line_pos(strict_ansi_mode ?
                           strict_ansi_error_severity : es_warning,
                         ec_last_line_incomplete, loc_in_line);
  goto add_newline_and_line_end_and_return;

possible_trigraph:
  /* Two "?"s in a row were detected in the first line.  Go into the
     general-purpose algorithm.  Note that we don't actually know that
     the two "?"s are followed by something that makes them a trigraph,
     but that should happen seldom enough that, from an efficiency point
     of view, we don't care. */
  curr_column = loc_in_line - curr_source_line + 1;
  goto entry_for_possible_trigraph;

line_splice:
  /* A backslash has been detected at the end of the first line.
     Go into the general-purpose algorithm. */
  curr_column = loc_in_line - curr_source_line;
  goto entry_for_line_splice;

  /* The general-purpose expensive algorithm.  Handles trigraphs, tracks
     mapping to original source characters.  Efficiency is less important
     in this version. */
line_loop:
  /* Start of a line after the first, first character already read into ch. */
  seq_number_last_read++;
  curr_ise->line_number++;
  curr_column = 0;
  /* Check if this line being read is that next needed for the file index
     table.  Remember that the first character has already been read into
     ch. */
  if (++(curr_ise->actual_line) == curr_ise->next_index_point) {
    curr_ise->next_index_point = update_file_index(
                                        curr_ise->assoc_actual_il_file,
                                        curr_ise->actual_line,
                                        ftell(curr_ise->file) - 1);
  }  /* if */
  /* Check for an empty line. */
  if (ch != '\n') {
    /* Process characters until a newline is read. */
    do {
      /* Process one character (ch). */
      curr_column++;
      /* Check for trigraphs.  A trigraph is two "?"s followed by another
         character. */
      if (ch == '?') {
        /* One "?", check previous character to see if it is also a "?".
           Note the use of curr_column rather than the start of buffer, since
           a "?" at the end of the previous line should not be counted
           as part of a trigraph in this line (trigraphs are handled in
           translation phase 1, line splices in phase 2). */
        if (curr_column != 1 && *(loc_in_line-1) == '?') {
entry_for_possible_trigraph:
          /* Trigraphs are disabled if the C dialect being compiled is
             pcc, but they are recognized in C++ and ANSI C modes. */
          if (C_dialect != C_dialect_pcc
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
#if QUESTION_MARK_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR
              /* See whether the first question mark is actually a question
                 mark, or a character after the first in a multibyte
                 sequence. */
              && (!multibyte_chars_in_source_enabled ||
                  (find_offset_for_source_line_mbc_including(loc_in_line-1,
                                                             &mbc_offset),
                   mbc_offset == loc_in_line-1-curr_source_line))
#endif /* QUESTION_MARK_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR */
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
                                                ) {
            /* Get the next character, the one following the two "?"s. */
            next_ch = getc(curr_input_stream);
            /* Check for the possible third characters of trigraphs.  If one
               is found, replace the three characters by the new one.  If not,
               pass the "?" through, and trap the next character for processing
               on the next time around the loop. */
            /* See standard, 2.2.1.1. */
            switch (next_ch) {
              case '=':  ch = '#'; break;
              case '(':  ch = '['; break;
              case '/':  ch = '\\'; break;
              case ')':  ch = ']'; break;
              case '\'': ch = '^'; break;
              case '<':  ch = '{'; break;
              case '!':  ch = '|'; break;
              case '>':  ch = '}'; break;
              case '-':  ch = '~'; break;
              default:   char_is_trapped = TRUE;
            }  /* switch */
            if (!char_is_trapped) {
              /* Trigraph detected.  Add a modification entry indicating
                 the position of the trigraph, remove the first "?" from the
                 buffer, then go on to store the remapped character. */
              loc_in_line--;
              curr_column++;
              olmp = add_orig_line_modif(olm_trigraph, loc_in_line);
              olmp->variant.trigraph_orig_char = next_ch;
            }  /* if */
          }  /* if */
        }  /* if */
      } else if (ch == LE_ESCAPE) {
        /* The zero character is reserved for internal use.  Replace it
           by a blank and save the error position for later display. */
        *loc_in_line = ch = ' ';
        if (!has_invalid_char) {
          has_invalid_char = TRUE;
          offset_to_invalid_char = loc_in_line - curr_source_line;
        }  /* if */
      }  /* if */
      /* Check that there is still room in the line buffer.  We have to
         leave room for both the final newline and line-end escapes. */
      if (loc_in_line == after_curr_source_line_minus_term) {
entry_for_expand_buffer:
        /* The line is too long; the buffer must be expanded. */
        offset_in_line = loc_in_line - curr_source_line;
        expand_curr_source_line();
        loc_in_line = curr_source_line + offset_in_line;
        after_curr_source_line_minus_term = after_end_of_curr_source_line -
                                            2*LE_ESCAPE_LEN;
      }  /* if */
      /* Put the character into curr_source_line. */
      *loc_in_line++ = (char)ch;
      /* Get next character, check for end of file without newline. */
      if (char_is_trapped) {
        ch = next_ch;
        char_is_trapped = FALSE;
      } else {
        ch = getc(curr_input_stream);
      }  /* if */
      if (is_eof_char(ch)) goto partial_final_line;
    } while (ch != '\n');
#if IGNORE_CARRIAGE_RETURN_IN_SOURCE
    /* Ignore carriage return right before newline.  Ignore several if
       they are present (there are Microsoft header files that have this). */
    while (*(loc_in_line-1) == '\r') {
      loc_in_line--;
      curr_column--;
      /* Avoid the line splice test if the line is empty except for the
         carriage return. */
      if (curr_column == 0) {
        goto add_newline_and_line_end_and_return;
      }  /* if */
    }  /* while */
#endif /* IGNORE_CARRIAGE_RETURN_IN_SOURCE */
    /* Check for backslash indicating line-splice.  Go add trailing newline
       and end-of-line, and then exit, if no backslash is present. */
    if (*(loc_in_line-1) == '\\') {
entry_for_line_splice:
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
#if BACKSLASH_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR
      if (multibyte_chars_in_source_enabled) {
        /* See whether the backslash is actually a backslash, or a character
           after the first in a multibyte sequence. */
        find_offset_for_source_line_mbc_including(loc_in_line-1, &mbc_offset);
        if (mbc_offset != loc_in_line-1-curr_source_line) {
          /* The backslash is not really a backslash.  But it is followed by a
             newline. */
          goto add_newline_and_line_end_and_return;
        }  /* if */
      }  /* if */
#endif /* BACKSLASH_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR */
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
      /* Remove the backslash in the buffer. */
      loc_in_line--;
      /* Add a modification entry recording the position of the line splice. */
      olmp = add_orig_line_modif(olm_line_splice, loc_in_line);
      olmp->variant.line_splice_seq_number = seq_number_last_read+1;
      /* Begin reading the next line.  It is an error if end of file is
         encountered. */
      if (ch = getc(curr_input_stream), !is_eof_char(ch)) goto line_loop;
      eof_read_on_curr_input_stream = TRUE;
      /* Backslash at end of last line in a file -- error. */
      finish_off_source_line_so_it_can_be_displayed_in_error();
      diagnostic_at_line_pos(microsoft_mode ? es_warning : es_error,
                             ec_last_line_backslash, loc_in_line);
      /* Ignore the backslash, end the logical line at this point. */
    }  /* if */
  }  /* if */
  goto add_newline_and_line_end_and_return;

entry_for_extend_current_line:
  /* Add more characters to the current source line instead of beginning
     a new source line. */
  loc_in_line = curr_char_loc;
  if (!eof_read_on_curr_input_stream &&
      (ch = getc(curr_input_stream), !is_eof_char(ch))) goto line_loop;
  eof_read_on_curr_input_stream = TRUE;
  at_end_of_source_file = TRUE;
  goto add_newline_and_line_end_and_return;
}  /* read_logical_source_line */


a_boolean is_nonstandard_character(char ch)
/*
Return TRUE if the indicated character is one not required by 5.2.1 of the
ISO C standard.
*/
{
  a_boolean is_nonstd;

  switch (ch) {
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
    case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
    case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
    case 'v': case 'w': case 'x': case 'y': case 'z':
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
    case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
    case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
    case 'V': case 'W': case 'X': case 'Y': case 'Z':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    case '!': case '"': case '#': case '%': case '&':
    case'\'': case '(': case ')': case '*': case '+':
    case ',': case '-': case '.': case '/': case ':':
    case ';': case '<': case '=': case '>': case '?':
    case '[': case'\\': case ']': case '^': case '_':
    case '{': case '|': case '}': case '~':
    case '\f':
    case VERTICAL_TAB_CHARACTER:
    case '\t':
    case ' ':
    case '\n':
      is_nonstd = FALSE;
      break;
    default:
      is_nonstd = TRUE;
      break;
  }  /* switch */
  return is_nonstd;
}  /* is_nonstandard_character */


/*
Flag that is TRUE while scanning a pcc-mode half-comment (see routine
below).
*/
static a_boolean in_pcc_mode_half_comment = FALSE;

static void skip_pcc_mode_half_comment(void)
/*
In pcc mode, a comment can be begun by "/" followed by "*" as a result
of macro expansion.  This is a strange sort of comment, however: the
preprocessor does not see it as a comment, and the compiler does.
That's because in pcc, the preprocessor writes out the characters
"/" and "*" and continues processing, and the compiler sees those
later as a comment delimiter.  This is a strange mode.  To implement
it, we skip tokens until the closing delimiter.
*/
{
  a_boolean         saved_fetch_pp_tokens = fetch_pp_tokens;
  a_source_position comment_start_pos;

  in_pcc_mode_half_comment = TRUE;
  /* Determine the start position of the comment in case it's needed
     for an unclosed-comment error. */
  conv_line_loc_to_source_pos(curr_char_loc, &comment_start_pos);
  /* Advance past the "/" and "*". */
  curr_char_loc += 2;
  /* Flush raw pp tokens until the end of the comment. */
  fetch_pp_tokens = TRUE;
  for (;;) {
    /* The "*" and "/" must be fetched as tokens, because they may come
       from macro expansions. */
    (void)get_token();
    while (curr_token == tok_star) {
      /* No white space is allowed between the tokens. */
      skip_white_space();
      (void)get_token();
      if (kind_of_white_space_skipped == 0 && *start_of_curr_token == '/') {
        /* End of comment.  Skip the "*" and "/". */
        curr_char_loc = start_of_curr_token + 1;
        goto end_of_comment;
      }  /* if */
    }  /* while */
    if (curr_token == tok_end_of_source) {
      /* Comment unclosed at end of source. */
      if (!building_pch_prefix) {
        /* Only issue this warning during the real compilation, not
           during the PCH prefix scan. */
        error_position = comment_start_pos;
        error(ec_comment_unclosed_at_eof);
      }  /* if */
      /* Consider the comment closed. */
      goto end_of_comment;
    }  /* if */
  }  /* for */
end_of_comment:
  in_pcc_mode_half_comment = FALSE;
  fetch_pp_tokens = saved_fetch_pp_tokens;
}  /* skip_pcc_mode_half_comment */


/*
Test whether curr_char_loc is the start of a comment.  It is already
known that *curr_char_loc == '/'.  The test can be tricky if
*curr_char_loc and *(curr_char_loc+1) are "/" and "*" but those
resulted from pasting of tokens in a macro expansion; that case should
not be considered to be the start of a comment.  In pcc mode and
Microsoft mode, the token pasting rules are different, and the start
of a comment is assumed.  Also tests for "//" in C++ mode.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#define or_microsoft_mode_slash_slash() \
  || (microsoft_mode && *(curr_char_loc+1) == '/')
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define or_microsoft_mode_slash_slash() /* Nothing */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#define start_of_comment()                                            \
  ((*(curr_char_loc+1) == '*' ||                                      \
    (end_of_line_comments_allowed && *(curr_char_loc+1) == '/')) &&   \
   (within_curr_source_line(curr_char_loc) ||                         \
    (pcc_preprocessing_mode && !in_pcc_mode_half_comment &&           \
     *(curr_char_loc+1) == '*')                                       \
    or_microsoft_mode_slash_slash() ))


void skip_white_space(void)
/*
Skip over any white space in the input.  White space includes blanks,
horizontal tabs, and comments.  Ordinarily, newline, vertical tab, and
form feed are also considered white space; however, when the global
variable in_preprocessing_directive indicates that we are inside such
a directive, newline is not white space, and vertical tab and form
feed are flagged as errors.  See standard sections 2.1.1.2
(translation phases; this routine handles part of phase 3), 3.1, and
3.8.  The global variable kind_of_white_space_skipped is set to indicate
the kinds of white space that were skipped, if any (this is useful for 
scanning of #defines, where white space is significant).
This routine also handles the special marker characters that appear in
source text (end of token, start of expansion, end of expansion).
*/
{
  register char      ch;
  register int	     kind_skipped;
  char               *comment_start_loc;
  a_boolean          comment_pos_determined;
  a_source_position  comment_start_pos;
  char               *delete_from;
  char               *delete_to;
  a_source_line_modif_ptr
		     slmp;
  a_boolean          delete_only_for_comment;

/* Macro used later to test if comments must be deleted.  Except for the
   keep_comments_in_pp_output switch, this is basically a time optimization --
   we avoid doing the comment deletion if we will not be outputting the
   modified line text in some way (as preprocessing or raw listing output). */
/* If asm functions are allowed, also delete comments if inside an asm
   function body.  The comments are copied to the asm string before they
   are deleted. */
#if ASM_SUPPORT_NEEDED
#define or_in_asm_function_body() || in_asm_function_body
#else /* !ASM_SUPPORT_NEEDED */
#define or_in_asm_function_body() /* Nothing */
#endif /* ASM_SUPPORT_NEEDED */
#define need_to_delete_comment()                                      \
  ((((generate_pp_output && !do_not_put_curr_line_in_pp_output) ||    \
     f_raw_listing != NULL) &&                                        \
    !keep_comments_in_pp_output)                                      \
   or_in_asm_function_body())
/* Macro used to check whether the comment start position has been determined
   and to determine it if not already done. */
#define determine_comment_pos_if_not_yet_done()				\
{ if (!comment_pos_determined) {					\
    macro_line_loc_to_source_pos(comment_start_loc, comment_start_pos);	\
    comment_pos_determined = TRUE;					\
  }  /* if */								\
}

  /* Forget that we know where the current token's characters are. */
  start_of_curr_token = NULL;
  kind_skipped = 0;  /* No white space skipped so far. */
white_space_loop:
  /* Examine the current character to see if it is white space.  Throw away
     spaces and horizontal tabs quickly, since they are always white space
     and there are lots of them (so we want that to be fast). */
  if ((ch = *curr_char_loc) == ' ' || ch == '\t') {
    kind_skipped |= WHITE_SPACE_OTHER;
    do {} while ((ch = *(++curr_char_loc)) == ' ' || ch == '\t');
  }  /* if */
  switch (ch) {
    case LE_ESCAPE:
      /* Lexical escape.  Can be end of line, end of insertion, etc.
         Second character indicates which. */
      ch = curr_char_loc[1];
      if (ch == LE_NEWLINE) {
        /* Newline is white space ordinarily, but a token to be returned if
           in a preprocessing directive. */
        /* Newline is also a token in asm functions. */
        if (in_preprocessing_directive ||
            treat_newline_as_token) goto end_skip;
        /* The newline character is white space, and is being thrown away. */
        kind_skipped |= WHITE_SPACE_OTHER;
        curr_char_loc += LE_ESCAPE_LEN;
      } else if (ch == LE_END_OF_LINE || ch == LE_END_OF_INSERTION) {
        /* End of source line or end of macro insertion. */
        /* Check to see if the hanging deletion flag is set. */
        if ((delete_from = delete_source_from_loc) != NULL) {
          /* Source from the indicated position to the end of the line or
             macro is to be deleted.  This is typically because a macro
             invocation argument list is being scanned.  Keep the final
             line-end lexical escape, but delete the newline lexical
             escape. */
          delete_to = curr_char_loc - 1;
          if (delete_from <= delete_to) {
            add_deletion_source_line_modif(delete_from,
                                         (sizeof_t)(delete_to-delete_from+1),
                                         /*for_comment=*/FALSE);
          }  /* if */
          /* Clear the flag to be sure it is cleared for error exit cases.
             It will be set later to the location of further text if there is
             any. */
          delete_source_from_loc = NULL;
        }  /* if */
        if (ch == LE_END_OF_LINE) {
          /* End of the source line. */
          /* We have to read a new logical source line now. 
             read_logical_source_line will pop the input stack if end of
             file is encountered.  On the final end of file, TRUE is returned,
             and we want to exit this routine.  If we are already at the
             final end of file, don't try reading again, just exit. */
          if (after_end_of_all_source ||
              read_logical_source_line(/*do_pop_on_end_of_file=*/TRUE,
                                       /*extend_current_line=*/FALSE)) {
            /* End of file, end the white-space skip. */
            goto end_skip;
          } /* if */
          /* Not end of file, keep checking for white space in the new line. */
        } else {
          /* End of the expansion text for a macro.  Find the character
             location of the character following the macro invocation, and
             continue there. */
          slmp = assoc_source_line_modif(curr_char_loc);
          if (slmp->being_rescanned_for_token_pasting) {
            /* We're rescanning a macro expansion in order to do old-style
               token pasting (e.g., in pcc mode).  We've reached the end of
               the top-level macro.  See whether we should continue
               into the surrounding line, which is true if we're inside
               the argument list for a macro call. */
            if (macro_depth <= 1) {
              /* Do not continue into the primary source line;
                 a tok_end_of_source will be returned eventually. */
              goto end_skip;
            }  /* if */
            /* Continue into the primary source line.  Clear the flag to
               indicate that we went off the end. */
            slmp->being_rescanned_for_token_pasting = FALSE;
            slmp->is_isolated_text = FALSE;
          } else {
            /* See if the current position is part of the text of a macro
               argument being macro-expanded; such text is expanded in
               isolation from the rest of the source file (see 3.8.3.1).
               In that case, the end-of-insertion is returned to the caller. */
            if (slmp->is_isolated_text) goto end_skip;
          }  /* if */
          /* Normal case; continue with the text following the macro
             invocation. */
          leave_insertion(slmp, curr_char_loc);
        }  /* if */
        /* If the hanging deletion flag was set, reset it to the new
           current position. */
        if (delete_from != NULL) {
          delete_source_from_loc = curr_char_loc;
        }  /* if */
      } else if (ch == LE_END_OF_TOKEN) {
        /* Marker put into text by preprocessing of macros, to force the same
           interpretation of token boundaries as during the macro definition.
           At this level, should be ignored.  Note that kind_skipped is not
           set, since this is not white space. */
        curr_char_loc += LE_ESCAPE_LEN;
      } else if (ch == LE_INERT_MACRO) {
        /* Marker put into text to indicate that the following macro name
           should not be expanded.  Return to caller. */
        goto end_skip;
      } else {
        unexpected_condition_str("skip_white_space: bad lexical escape");
      }  /* if */
      goto white_space_loop;
    case '\f':  /* Form feed. */
    case VERTICAL_TAB_CHARACTER:
      /* These are white space if not in a preprocessing directive.  Inside
         a preprocessing directive, it is implementation-defined whether
         or not they are replaced by a space (conceptually), and that's
         what this implementation does, so they are allowed there as well. */
      curr_char_loc++;
      kind_skipped |= WHITE_SPACE_OTHER;
      goto white_space_loop;
    case ATTENTION_MARKER:
      /* Marker placed into source text to provide a cue to the fact that
         a source modification (probably a text replacement due to a
         macro expansion) begins here. */
      if (delete_source_from_loc != NULL) {
        /* Source from the indicated position to the current position
           is to be deleted.  This is typically because a macro
           invocation argument list is being scanned. */
        delete_to = curr_char_loc - 1;
        if (delete_source_from_loc <= delete_to) {
          add_deletion_source_line_modif(delete_source_from_loc,
                                (sizeof_t)(delete_to-delete_source_from_loc+1),
                                         /*for_comment=*/FALSE);
        }  /* if */
      }  /* if */
      /* Find the appropriate source line modification entry, and begin
         scanning text in that entry.  kind_skipped is not set, since this
         is not white space. */
      go_into_insertion(slmp, curr_char_loc);
      /* If the hanging deletion flag is set, reset it to the new
         current position. */
      if (delete_source_from_loc != NULL) {
        delete_source_from_loc = curr_char_loc;
      }  /* if */
      goto white_space_loop;
    case '/':
      /* Possible start of a comment.  Check for "*" following the "/".
         (Or another "/" in C++ mode.) */
      if (!start_of_comment()) goto end_skip;
      /* This is the start of a comment. */
      if (pcc_preprocessing_mode &&
          !within_curr_source_line(curr_char_loc) &&
          *(curr_char_loc+1) == '*') {
        /* We're in pcc mode, and the "/ *" token opening characters came from
           token pasting inside a macro.  They are considered to start a
           strange sort of comment: it's a comment to the compiler but not a
           comment to the preprocessor.  Therefore, we have to skip tokens
           until the closing delimiter.  This only comes up if the user
           is taking terrible liberties with macro processing. */
        /* If the text we're looking at was generated by combining the
           text of a top-level macro expansion and the text following
           it on the primary source line to allow for token pasting
           (see expand_top_level_pcc_macro), and the "/" is in
           the part of the text from the primary source line, we have
           a real comment, not a half comment. */
        slmp = assoc_source_line_modif(curr_char_loc);
        if (slmp->text_from_primary_source_line != NULL &&
            curr_char_loc >= slmp->text_from_primary_source_line) {
          goto normal_comment;
        }  /* if */
        skip_pcc_mode_half_comment();
      } else if (*(curr_char_loc+1) == '/') {
        /* C++ comment -- //.  Skip to end of line. */
        comment_start_loc = curr_char_loc;
        /* Advance past the first "/". */
        curr_char_loc++;
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (microsoft_mode) {
          if (!within_curr_source_line(curr_char_loc)) {
            /* In Microsoft mode, a // comment delimiter can appear in a macro:
                 #define startcomment() /##/
                 startcomment() This is ignored
               Work outward to the primary source line.
            */
            do {
              slmp = assoc_source_line_modif(curr_char_loc);
              /* If the comment delimiter appears in the expansion of a
                 macro argument, don't consider it the start of a comment.
                 This is disallowed partly because you get in trouble with
                 copy_modif_list later if you allow it (the modification
                 entries are in the wrong order). */
              if (slmp->is_isolated_text) goto end_skip;
              leave_insertion(slmp, curr_char_loc);
            } while (!within_curr_source_line(curr_char_loc));
            if (need_to_delete_comment()) {
              /* The text of the comment needs to be deleted. */
              curr_char_loc = comment_start_loc;
              do {
                slmp = assoc_source_line_modif(curr_char_loc);
                /* Find the end of the insertion. */
                while (*curr_char_loc   != LE_ESCAPE ||
                       curr_char_loc[1] != LE_END_OF_INSERTION) {
                  curr_char_loc++;
                }  /* while */
#if INCLUDE_COMMENTS_IN_ASM_FUNC_BODY
                /* Before deleting the comment, see if it's part of an asm
                   function body -- if so, make a copy of it. */
                if (in_asm_function_body && !in_preprocessing_directive) {
                  copy_from_source_to_asm_func_buffer(comment_start_loc,
                                                      curr_char_loc);
                }  /* if */
#endif /* INCLUDE_COMMENTS_IN_ASM_FUNC_BODY */
                /* Delete the comment entirely, but leave the end-of-insertion
                   escape. */
                add_deletion_source_line_modif(comment_start_loc,
                                   (sizeof_t)(curr_char_loc-comment_start_loc),
                                               /*for_comment=*/TRUE);
                leave_insertion(slmp, curr_char_loc);
                comment_start_loc = curr_char_loc;
              } while (!within_curr_source_line(curr_char_loc));
            }  /* if */
          }  /* if */
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Advance to the end of line. */
        /* Note that no special processing is required for multibyte characters
           because the LE_ESCAPE/LE_NEWLINE line terminator cannot occur in
           multibyte character sequences. */
        while (*curr_char_loc   != LE_ESCAPE ||
               curr_char_loc[1] != LE_NEWLINE) curr_char_loc++;
#if MICROSOFT_EXTENSIONS_ALLOWED
        /* With the trick of a // comment inside a macro, it's possible
           that there are no characters of the comment in the primary
           source line. */
        if (curr_char_loc != comment_start_loc)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not insert code here; this is the "then" of an "if". */
        {        
          if (need_to_delete_comment()) {
#if INCLUDE_COMMENTS_IN_ASM_FUNC_BODY
            /* Before deleting the comment, see if it's part of an asm function
               body -- if so, make a copy of it. */
            if (in_asm_function_body && !in_preprocessing_directive) {
              copy_from_source_to_asm_func_buffer(comment_start_loc,
                                                  curr_char_loc);
            }  /* if */
#endif /* INCLUDE_COMMENTS_IN_ASM_FUNC_BODY */
            /* Delete the comment entirely, but leave the newline escape. */
            add_deletion_source_line_modif(comment_start_loc,
                                   (sizeof_t)(curr_char_loc-comment_start_loc),
                                           /*for_comment=*/TRUE);
          }  /* if */
        }  /* if */
      } else {
normal_comment:
        /* C-style comment. */
        /* Save the position of the start of the comment.  Do not determine
           the source position yet (it may be costly, and it's only needed
           if there is an error). */
        comment_start_loc = curr_char_loc;
        comment_pos_determined = FALSE;
        /* Advance past the "/" and "*". */
        curr_char_loc += 2;
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
#if STAR_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR
        /* Initialize for scanning multibyte characters in the comment. */
        mbc_scan_init_if_multibyte_chars_in_source_enabled();
#endif /* STAR_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR */
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
        if (!in_preprocessing_directive && !currently_in_pp_if_skip) {
          /* Look for the special lint comments "notreached", "argsused", and
             "varargs" (all of those in caps -- written here in lower case 
             letters because UNIX lint finds the keywords even in the middle of
             comments).  Ignore blanks or tabs before the keyword (lint
             actually allows any white space, and even other identifiers,
             before the keyword; this compiler doesn't). */
          if ((ch = *curr_char_loc) == ' ' || ch == '\t') {
            do {} while ((ch = *(++curr_char_loc)) == ' ' || ch == '\t');
          }  /* if */
          if (ch == 'N' && curr_char_loc[1] == 'O' &&
              strncmp(curr_char_loc+2, "TREACHED", 8) == 0 &&
              !isalpha((unsigned char)curr_char_loc[10])) {
            /* The special lint comment "notreached" (in caps), asserting that
               the code following is unreachable. */
#if DEBUG
            if (debug_level >= 3) {
              fprintf(f_debug, "lint NOTREACHED comment\n");
            }  /* if */
#endif /* DEBUG */
            determine_comment_pos_if_not_yet_done();
            (void)add_curr_token_pseudo_pragma
                     ((a_pragma_kind)pk_lint_notreached, &comment_start_pos);
            curr_char_loc += 10;
          } else if (ch == 'A' && curr_char_loc[1] == 'R' &&
                     strncmp(curr_char_loc+2, "GSUSED", 6) == 0 &&
                     !isalpha((unsigned char)curr_char_loc[8])) {
            /* The special lint comment "argsused" (in caps), asserting that
               the arguments of the function following are all used (or more
               accurately, that it's okay that they aren't all used). */
#if DEBUG
            if (debug_level >= 3) {
              fprintf(f_debug, "lint ARGSUSED comment\n");
            }  /* if */
#endif /* DEBUG */
            determine_comment_pos_if_not_yet_done();
            (void)add_curr_token_pseudo_pragma((a_pragma_kind)pk_lint_argsused,
                                               &comment_start_pos);
            curr_char_loc += 8;
          } else if (ch == 'V' && curr_char_loc[1] == 'A' &&
                     strncmp(curr_char_loc+2, "RARGS", 5) == 0 &&
                     !isalpha((unsigned char)curr_char_loc[7])) {
            /* The special lint comment "varargs" (in caps), asserting that
               the function following takes a variable number of arguments.
               If a number follows the keyword, it is the number of arguments
               that are fixed and should always be present and checked; 0 is
               assumed if the number is omitted. */
            int				varargs_count = 0;
            a_pending_pragma_ptr	ppp;
            curr_char_loc += 7;
            varargs_count = 0;
            /* Skip any blanks, then scan a decimal number.  lint itself only
               looks at one digit. */
            while (*curr_char_loc == ' ') curr_char_loc++;
            while (isdigit((unsigned char)(ch = *curr_char_loc))) {
              if (varargs_count > LINT_VARARGS_COUNT_MAX / 10) {
                varargs_count = 0;
                break;
              }  /* if */
              varargs_count *= 10;
              if (varargs_count > LINT_VARARGS_COUNT_MAX - (ch - '0')) {
                varargs_count = 0;
                break;
              }  /* if */
              varargs_count += ch - '0';
              curr_char_loc++;
            }  /* while */
#if DEBUG
            if (debug_level >= 3) {
              fprintf(f_debug, "lint VARARGS comment, count = %d\n",
                               varargs_count);
            }  /* if */
#endif /* DEBUG */
            determine_comment_pos_if_not_yet_done();
            ppp = add_curr_token_pseudo_pragma
		   ((a_pragma_kind)pk_lint_varargs_count, &comment_start_pos);
            ppp->variant.lint_varargs_count = varargs_count;
          }  /* if */
        }  /* if */
        /* Scan to the * / marking the end.  This may involve reading extra
           lines, and it is interesting to note that the newlines passed in
           those cases do not terminate preprocessing directives. */
        /* Again, a speed note:  All text inside comments goes through this
           loop, so it should be very fast. */
        while ((ch = *curr_char_loc) != '*' || *(curr_char_loc+1) != '/') {
          if (ch == LE_ESCAPE) {
            /* End of a line of the comment. */
            ch = curr_char_loc[1];
            check_assertion_str(ch == LE_NEWLINE ||
                                ch == LE_END_OF_LINE,
                            "skip_white_space: bad lexical escape in comment");
            /* Determine the source position for the start of the comment
               now, before we lose the current source line. */
            determine_comment_pos_if_not_yet_done();
#if INCLUDE_COMMENTS_IN_ASM_FUNC_BODY
            if (in_asm_function_body && !in_preprocessing_directive) {
              /* Copy the comment text if it's part of an asm function
                 body. */
              if (ch == LE_NEWLINE) {
                curr_char_loc += LE_ESCAPE_LEN;
                ch = LE_END_OF_LINE;
              }  /* if */
              copy_from_source_to_asm_func_buffer(comment_start_loc,
                                                  curr_char_loc);
            }  /* if */
#endif /* INCLUDE_COMMENTS_IN_ASM_FUNC_BODY */
            /* We are supposed to delete the characters of the source line
               from delete_source_from_loc on, if it is non-NULL.  This would
               be, for example, because we are scanning a macro invocation. */
            delete_from = delete_source_from_loc;
            if (delete_from != NULL) {
              /* Reset the "from" position for subsequent lines. */
              delete_source_from_loc = curr_source_line;
              delete_only_for_comment = FALSE;
            } else if (need_to_delete_comment()) {
              /* Delete the characters of the comment if writing preprocessor
                 output with the comments deleted.  The newline will be kept,
                 and therefore the comment is deleted entirely rather than
                 replaced by one blank -- since there is already white space
                 here in the form of the newline. */
              delete_from = comment_start_loc;
              delete_only_for_comment = TRUE;
            }  /* if */
            if (delete_from != NULL) {
              /* Delete the required characters.  Leave the newline and
                 line-end lexical escape sequences.  If this comment is
                 part of a preprocessing directive, delete the newline as
                 well so that multi-line directives will become one-line
                 directives. */
              if (in_preprocessing_directive && ch == LE_NEWLINE) {
                delete_to = curr_char_loc+LE_ESCAPE_LEN-1;
              } else {
                delete_to = curr_char_loc-1;
              }  /* if */
              if (delete_from <= delete_to) {
                add_deletion_source_line_modif(delete_from,
                                           (sizeof_t)(delete_to-delete_from+1),
                                      /*for_comment=*/delete_only_for_comment);
              }  /* if */
            }  /* if */
            /* Read a new line.  Note the parameter asking that the input
               stack not be popped, since we need to know about ends of files
               (it is an error for a comment to be unclosed at the end of the
               file in which it was opened).  If we are processing command-
               line macros, we shouldn't attempt to read another line. */
            if (curr_command_line_macro_def != NULL ||
                read_logical_source_line(/*do_pop_on_end_of_file=*/FALSE,
                                         /*extend_current_line=*/FALSE)) {
              /* End of file encountered, unclosed comment. */
              if (!building_pch_prefix) {
                /* Only issue this warning during the real compilation, not
                   during the PCH prefix scan. */
                error_position = comment_start_pos;
                error(ec_comment_unclosed_at_eof);
              }  /* if */
              /* Consider the comment closed. */
              goto end_of_comment;
            }  /* if */
            /* Reset the start of comment location for subsequent lines. */
            comment_start_loc = curr_source_line;
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
#if STAR_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR
            /* Initialize for scanning multibyte characters in the comment. */
            mbc_scan_init_if_multibyte_chars_in_source_enabled();
#endif /* STAR_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR */
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
          } else {
            /* Not an escape, i.e., a normal character. */
            /* Check for possible nested comment, issue a warning.  This
               helps catch unclosed comments. */
            if (ch == '/' && *(curr_char_loc+1) == '*') {
              if (!building_pch_prefix) {
                /* Only issue this warning during the real compilation, not
                   during the PCH prefix scan. */
                warning_at_line_pos(ec_nested_comment, curr_char_loc);
              }  /* if */
            }  /* if */
            /* Advance to the next character position. */
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
#if STAR_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR
            if (multibyte_chars_in_source_enabled) {
              /* Advance to the next character, dealing with multibyte
                 characters. */
              curr_char_loc += mbc_length(curr_char_loc, (a_boolean *)NULL);
            } else
#endif /* STAR_CAN_OCCUR_AS_PART_OF_MULTIBYTE_CHAR */
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
            /* Do not insert code here -- this is the "else" of an "if". */
            {
              /* Advance to the next character without worrying about
                 multibyte characters. */
              curr_char_loc++;
            }  /* if */
          }  /* if */
        }  /* while */
        /* End of comment.  Take the "*" and "/", go back to throw away more
           white space. */
        curr_char_loc += 2;
        if (need_to_delete_comment() && delete_source_from_loc == NULL) {
#if INCLUDE_COMMENTS_IN_ASM_FUNC_BODY
          /* Before deleting the comment, see if it's part of an asm function
             body -- if so, make a copy of it. */
          if (in_asm_function_body && !in_preprocessing_directive) {
            copy_from_source_to_asm_func_buffer(comment_start_loc,
                                                curr_char_loc);
          }  /* if */
#endif /* INCLUDE_COMMENTS_IN_ASM_FUNC_BODY */
          /* Delete the characters of the comment if writing preprocessor
             output with the comments deleted.  Under ANSI rules, the comment
             must be replaced by one space if there is no other white space
             here (2.1.1.2, phase 3); we replace it with a blank always.
             Under pcc rules, the comment is always deleted entirely.
             The deletion here is suppressed if we are deleting everything 
             up to this point anyway (delete_source_from_loc != NULL). */
          if (pcc_preprocessing_mode) {
            /* pcc mode; delete the comment entirely. */
            add_deletion_source_line_modif(comment_start_loc,
                                   (sizeof_t)(curr_char_loc-comment_start_loc),
                                           /*for_comment=*/TRUE);
          } else {
            /* ANSI mode; replace the comment with a space. */
            replace_source_string_by_space(comment_start_loc,
                                  (sizeof_t)(curr_char_loc-comment_start_loc));
          }  /* if */
        }  /* if */
end_of_comment:;
      }  /* if */
      /* Remember that a comment was skipped in white space.  Note that
         this must be done after the test of kind_skipped above. */
      kind_skipped |= WHITE_SPACE_COMMENTS;
      goto white_space_loop;
    default:
      /* For all non-white-space characters, do nothing and return. */
      ;
  }  /* switch */
end_skip:
  /* "Return" the mask of kinds of white space skipped. */
  kind_of_white_space_skipped = kind_skipped;
#undef determine_comment_pos_if_not_yet_done
}  /* skip_white_space */


static a_token_kind scan_number(void)
/*
Scan a numeric token (integer or floating constant).  Return the kind of
token.
*/
{
  register char	ch;
  register enum {k_decimal, k_octal, k_hex, k_float} kind;
  register a_token_kind 
		ctoken;
  a_boolean     err = FALSE;
  char		*err_pos;
  an_error_code	err_code;
  a_boolean	is_hex_fp_value = FALSE;
  a_boolean	any_hex_digits = FALSE;

  /* Collect the characters of the constant, and figure out where it
     ends.  In the process, figure out what kind of token it is.
     This is done according to the syntax for integer constants (3.1.3.2)
     and floating constants (3.1.3.1), rather than according to the
     pp-number syntax (3.1.8).  */
  kind = k_decimal;
  if (*curr_char_loc == '0') {
    /* First digit is a zero.  This is probably an octal constant (like
       0777) or a hex constant (like 0xfff), but it could also be a
       solitary 0 (which is octal, though that doesn't matter), or
       a floating constant that begins with 0. */
    if ((ch = *(curr_char_loc+1)) == 'x' || ch == 'X') {
      /* 0x... or 0X..., hexadecimal. */
      kind = k_hex;
      /* The hex constant stops on a non-hex digit. */
      curr_char_loc++;
      while (isxdigit((unsigned char)*(++curr_char_loc))) {
        any_hex_digits = TRUE;
      }  /* while */
      /* Check for floating point. */
      if (hex_floating_point_constants_allowed) {
        /* C99 permits floating point constants specified in hexadecimal. */
        if ((ch = *curr_char_loc) == '.') goto float_accum_1;
        if (ch == 'p' || ch == 'P')       goto float_accum_2;
      }  /* if */
      /* Check for just "0x" by itself.  pcc allows this and interprets
         it as zero. */
      if (curr_char_loc == start_of_curr_token+2 && !fetch_pp_tokens) {
        if (C_dialect != C_dialect_pcc) {
          error_at_line_pos(ec_bad_hex_digit, start_of_curr_token);
          err = TRUE;
        } else {
          warning_at_line_pos(ec_bad_hex_digit, start_of_curr_token);
        }  /* if */
      }  /* if */
    } else {
      /* Octal or floating point.  Accumulate digits.  Digits 8 and 9
         are valid in floating point, but in octal they are only valid in
         pcc mode.  That is checked later.  For now, the "8" and "9"
         are accumulated. */
      do {} while (isdigit((unsigned char)*(++curr_char_loc)));
      /* Check for floating point. */
      if ((ch = *curr_char_loc) == '.') goto float_accum_1;
      if (ch == 'e' || ch == 'E')       goto float_accum_2;
      /* Definitely octal. */
      kind = k_octal;
    }  /* if */
  } else if (*curr_char_loc == '.') {
    /* Number starting with ".".  The character following the "." must be
       a digit (already checked by get_token). */
    if (microsoft_bugs &&
        isdigit((unsigned char)curr_char_loc[1]) &&
        curr_char_loc[2] == '.') {
      /* The Microsoft compiler accepts constants like ".1.234". */
      curr_char_loc += 2;
      warning_at_line_pos(ec_extra_chars_on_number, curr_char_loc);
    }  /* if */
    goto float_accum_1;
  } else {
    /* Number not starting with "0" or ".".  Could be a decimal integer or
       a floating-point number.  Accumulate the initial digit sequence. */
    do {} while (isdigit((unsigned char)*(++curr_char_loc)));
    /* A ".", "e", or "E" now indicates a floating-point constant. */
    if ((ch = *curr_char_loc) == '.') goto float_accum_1;
    if (ch == 'e' || ch == 'E')       goto float_accum_2;
    /* Definitely a decimal integer. */
    kind = k_decimal;
  }  /* if */
  /* Integer constant of some kind.  Check for final suffix of "u" or
     "l", or both, in upper or lower case. */
#if LONG_LONG_ALLOWED
  /* Or "ll" for long long. */
#endif /* LONG_LONG_ALLOWED */
  { a_boolean u_seen = FALSE;
    int       l_seen = 0;
    for (;; curr_char_loc++) {
      ch = *curr_char_loc;
      if ((ch == 'u' || ch == 'U') && !u_seen) {
        u_seen = TRUE;
      } else if ((ch == 'l' || ch == 'L') &&
#if LONG_LONG_ALLOWED
                 l_seen < 2
#else /* !LONG_LONG_ALLOWED */
                 l_seen < 1
#endif /* LONG_LONG_ALLOWED */
                           ) {
        l_seen++;
      } else {
        break;
      }  /* if */
    }  /* for */
#if LONG_LONG_ALLOWED
    if (strict_ansi_mode && !long_long_is_standard &&
        !fetch_pp_tokens && l_seen == 2) {
      /* "long long" type is nonstandard. */
      diagnostic_at_line_pos(strict_ansi_discretionary_severity,
                             ec_nonstd_long_long, start_of_curr_token);
    }  /* if */
#endif /* LONG_LONG_ALLOWED */
    if (microsoft_mode && l_seen == 0 &&
        (*curr_char_loc == 'i' || *curr_char_loc == 'I') &&
        isdigit((unsigned char)curr_char_loc[1])) {
      /* The Microsoft compiler allows a suffix like "i32" indicating a
         32-bit integer.  "ui32" indicates an unsigned 32-bit integer (for
         that case, the "u" was scanned above). */
      do {
        curr_char_loc++;
      } while (isdigit((unsigned char)(*curr_char_loc)));
    }  /* if */
  }
  goto constant_accumulated;

float_accum_1:
  /* At the decimal point in a floating constant.  Take whatever digits
     follow it.  The kind variable indicates the kind of digits that
     are being used (hex or decimal). */
  if (kind == k_hex) {
    while (isxdigit((unsigned char)*(++curr_char_loc))) {
      any_hex_digits = TRUE;
    }  /* while */
    if (!any_hex_digits) {
      /* No hex digits were specified.  Something like "0x.". */
      error_at_line_pos(ec_bad_float_constant, curr_char_loc);
      any_hex_digits = TRUE;
    }  /* if */
  } else {
    do {} while (isdigit((unsigned char)*(++curr_char_loc)));
  }  /* if */
  /* Check for the presence of an exponent. */
  if (kind == k_hex) {
    if ((ch = *curr_char_loc) != 'p' && ch != 'P') {
      /* A missing binary suffix -- this is an error. */
      error_at_line_pos(ec_bad_float_constant, curr_char_loc);
      err = TRUE;
      goto end_float_accum;
    }  /* if */
  } else {
    if ((ch = *curr_char_loc) != 'e' && ch != 'E') goto end_float_accum;
  }  /* if */
float_accum_2:
  /* At the "e" or "E" indicating the start of the exponent of a floating
     constant (or the "p" or "P" for a floating point constant specified
     as a hexadecimal value).  Take an optional sign, then decimal digits
     of the exponent. */
  if (kind == k_hex && !any_hex_digits) {
    /* No hex digits were specified.  Something like "0xp0". */
    error_at_line_pos(ec_bad_float_constant, curr_char_loc);
  }  /* if */
  if ((ch = *(curr_char_loc+1)) == '+' || ch == '-') curr_char_loc++;
  if (!isdigit((unsigned char)*(curr_char_loc+1)) && !fetch_pp_tokens) {
    /* No digits of the exponent are present. pcc treats this as an exponent
       of zero. */
    if (C_dialect != C_dialect_pcc) {
      error_at_line_pos(ec_bad_float_constant, curr_char_loc+1);
      err = TRUE;
    } else {
      warning_at_line_pos(ec_bad_float_constant, curr_char_loc+1);
    }  /* if */
  }  /* if */
  do {} while (isdigit((unsigned char)*(++curr_char_loc)));
end_float_accum:
  is_hex_fp_value = kind == k_hex;
  kind = k_float;
  /* Check for a final suffix of "f" or "l", in upper or lower case. */
  if ((ch = *curr_char_loc) == 'f' || ch == 'F' || ch == 'l' || ch == 'L' ) {
     curr_char_loc++;
  }  /* if */

constant_accumulated:
  /* Here, start_of_curr_token marks the beginning, and curr_char_loc one
     past the end of the constant.  kind and ctoken are set correctly.
     The suffix (if any) has been accumulated. */
  end_of_curr_token = curr_char_loc - 1;

#if DEBUG
  if (debug_level >= 4) {
    char *ks;
    switch (kind) {
      case k_decimal:   ks = "decimal"; break;
      case k_octal:     ks = "octal";   break;
      case k_hex:       ks = "hex";     break;
      case k_float:     ks = "float";   break;
#if CHECKING
      default:          ks = "<bad kind>";
#endif /* CHECKING */
    }  /* switch */
    fprintf(f_debug, "Numeric token = \"%.*s\", kind = %s\n",
                     (int)(end_of_curr_token - start_of_curr_token + 1),
                     start_of_curr_token, ks);
  }  /* if */
#endif /* DEBUG */
  /* See if there are more characters that would have been part of
     this number had it been scanned as a pp-number.  The pp-number
     syntax is (3.1.8):

     pp-number:
            digit
            . digit
            pp-number digit
            pp-number nondigit
            pp-number e sign
            pp-number E sign
            pp-number p sign    (C99)
            pp-number P sign    (C99)
            pp-number .

     digit is any decimal digit (0-9).
     nondigit is an alphabetic or "_".
  */
  /* Do this only in strict ANSI mode, or in default mode when scanning
     preprocessing tokens, but never in pcc mode. */
  if ((strict_ansi_mode || fetch_pp_tokens) && C_dialect != C_dialect_pcc) {
    while (is_id_char[(ch = *curr_char_loc)-CHAR_MIN] || ch == '.' ||
           ((ch == '+' || ch == '-') &&
            ((ch = *(curr_char_loc-1)) == 'e' || ch == 'E' ||
             (hex_floating_point_constants_allowed &&
              (ch == 'p' || ch == 'P'))))) {
      /* 0-9, a-z, A-Z, "_", ".", or sign preceded by "e" or "E" or "p"
         or "P".  Keep accumulating. */
      curr_char_loc++;
    }  /* while */
#if DEBUG
    if (debug_level >= 4) {
      if (curr_char_loc != (end_of_curr_token + 1)) {
        fprintf(f_debug, "Extra pp-number text: \"%.*s\"\n",
                         (int)(curr_char_loc - end_of_curr_token - 1),
                         end_of_curr_token+1);
      }  /* if */
    }  /* if */
#endif /* DEBUG */
  } else if (C_dialect == C_dialect_pcc && fetch_pp_tokens) {
    /* pcc mode has its own variant of this.  Consider
         #define a 7
         int i = 1a;
       The "a" in "1a" should not be treated as a separate token. */
    while (is_id_char[(ch = *curr_char_loc)-CHAR_MIN]) {
      /* Alphanumeric character.  Keep accumulating. */
      curr_char_loc++;
    }  /* while */
  }  /* if */

  if (fetch_pp_tokens) {
    /* Raw pp-tokens are wanted, do not convert.  Take the whole pp-number. */
    end_of_curr_token = curr_char_loc - 1;
    ctoken = tok_pp_number;
  } else {
    /* Preprocessing number is not wanted. */
    /* Check for extra pp-number characters of token to be converted.
       This is an error. */
    if (curr_char_loc != (end_of_curr_token + 1) && !err) {
      error_at_line_pos(ec_extra_chars_on_number, end_of_curr_token+1);
      /* Note that the extra characters just get thrown away. */
    }  /* if */
    /* Convert the constant.  Errors are still possible, since the checking
       above allows certain cases by. */
    switch (kind) {
      case k_decimal:
        conv_integer_literal(10, &err_code, &err_pos);
        ctoken = tok_int_constant;
        break;
      case k_octal:
        conv_integer_literal(8, &err_code, &err_pos);
        ctoken = tok_int_constant;
        break;
      case k_hex:
        conv_integer_literal(16, &err_code, &err_pos);
        ctoken = tok_int_constant;
        break;
      case k_float:
        conv_float_literal(is_hex_fp_value, &err_code, &err_pos);
        ctoken = tok_float_constant;
        break;
#if CHECKING
      default:
        internal_error("scan_number: bad kind");
#endif /* CHECKING */
    }  /* switch */
    /* Check for errors detected. */
    if (err_code != ec_no_error) {
      error_at_line_pos(err_code, err_pos);
    }  /* if */
  }  /* if */
#if DEBUG
  if (db_flag_is_set("scan_number")) {
    db_constant(&const_for_curr_token);
    if (const_for_curr_token.type != NULL) {
      fprintf(f_debug, ", type: ");
      db_type(const_for_curr_token.type);
    }  /* if */
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  return (ctoken);
}  /* scan_number */


static void scan_boolean_constant(a_token_kind ctoken)
/*
Scan the "true" and "false" tokens.  ctoken is tok_true or tok_false.
Creates an integer constant of the appropriate kind (except when scanning
pp tokens).
*/
{
  if (fetch_pp_tokens) {
    /* When fetching preprocessing tokens, just return tok_false or tok_true.
       Nothing else needs to be done here for this case. */
  } else {
    /* Create a boolean constant with the appropriate value. */
    clear_constant(&const_for_curr_token, (a_constant_repr_kind)ck_integer);
    const_for_curr_token.type = bool_type();
    /* The integer value is either 1 (true) or 0 (false). */
    set_integer_value(&const_for_curr_token.variant.integer_value,
                      (a_host_large_integer)(ctoken == tok_true));
    const_for_curr_token.non_arithmetic = TRUE;
  }  /* if */
}  /* scan_boolean_constant */

#if BSEARCH_QSORT_FUNCTION_IS_EXTERN_C
BEGIN_EXTERN_C_BLOCK
#endif /* BSEARCH_QSORT_FUNCTION_IS_EXTERN_C */

static a_boolean UCN_char_is_in_range(const void* char_ptr,
                                      const void* table_entry_ptr)
/*
Comparison function used by bsearch to test whether a given UCN
value is within the specified range.  char_ptr points to the UCN
value being looked up, and table_entry_ptr points to the entry in the
UCN range table.

Return -1 if the character precedes the range, zero if it is in the range,
or +1 if the character follows the range.
*/
{
  unsigned long		uchar = *(unsigned long*)char_ptr;
  a_UCN_range_ptr	range = (a_UCN_range_ptr)table_entry_ptr;
  return (uchar < range->start ? -1 : uchar <= range->end ? 0 : 1);
}  /* UCN_char_is_in_range */

#if BSEARCH_QSORT_FUNCTION_IS_EXTERN_C
END_EXTERN_C_BLOCK
#endif /* BSEARCH_QSORT_FUNCTION_IS_EXTERN_C */

static an_error_code is_valid_UCN_identifier_char(
					unsigned long	uchar,
					a_boolean	is_identifier_start)
/*
Determine whether uchar is a value that is designated as a universal character
name that may appear in an identifier.  If is_identifier_start is TRUE,
the specified character must not be a digit.  If the identifier is
invalid, return an error code that gives the reason the identifier
character is invalid.
*/
{
  an_error_code	result = ec_no_error;

  a_UCN_range *range;
  range = (a_UCN_range *)bsearch(
			  (a_bsearch_arg_type)&uchar,
			  (a_bsearch_arg_type)UCN_table,
                          size_t_arg(sizeof(UCN_table) / sizeof(a_UCN_range)),
                          sizeof(a_UCN_range), UCN_char_is_in_range);
  if (range == NULL) {
    /* Not a valid identifier character. */
    result = ec_invalid_identifier_UCN;
  } else if (is_identifier_start && range->is_digit) {
    /* This is the first character of an identifier and the character is
       a digit. */
    result = ec_invalid_identifier_start_UCN;
  }  /* if */
  return result;
}  /* is_valid_UCN_identifier_char */


static void check_for_invalid_cplusplus_ucn(
					unsigned long	ucn,
				        char		**start_pos,
					a_boolean	is_identifier,
					a_boolean	is_identifier_start)
/*
Determine whether "ucn" is a valid universal character name in C++.
Issue a diagnostic if it is not.
*/
{
  an_error_code	err_code = ec_no_error;
  if (ucn <= 255 && !is_nonstandard_character((char)ucn)) {
    /* A UCN cannot be used to name a character in the basic character
       set. */
    err_code = ec_UCN_names_basic_char;
  } else if (ucn < 0x20 || (ucn >= 0x7f && ucn <= 0x9f)) {
    /* These characters are disallowed by the standard. */
    err_code = ec_invalid_UCN;
  } else if (is_identifier) {
    /* Check whether this is a valid identifier character. */
    err_code = is_valid_UCN_identifier_char(ucn, is_identifier_start);
  }  /* if */
  if (err_code != ec_no_error) {
    /* Get the source position that corresponds to this character. */
    conv_line_loc_to_source_pos(*start_pos, &error_position);
    diagnostic(strict_ansi_error_severity, err_code);
  }  /* if */
}  /* check_for_invalid_cplusplus_ucn */


static void check_for_invalid_c99_ucn(unsigned long	ucn,
				      char		**start_pos,
				      a_boolean		is_identifier,
				      a_boolean		is_identifier_start)
/*
Determine whether "ucn" is a valid universal character name in C99.
Issue a diagnostic if it is not.
*/
{
  an_error_code	err_code = ec_no_error;
  if (ucn == '$' && is_identifier && allow_dollar_in_id_chars) {
    /* A "$" specified as a UCN when dollar signs are allowed in
       identifiers.  A dollar sign is normally allowed as a UCN in
       C99 mode, but is disallowed when dollar signs are permitted in
       identifiers. */
    err_code = ec_UCN_names_basic_char;
  } else if (ucn < 0xa0 && ucn != 0x24 && ucn != 0x40 && ucn != 0x60) {
    /* A UCN cannot name a character less than 0xa0 except for
       "$" (0x24), "@" (0x40), and "`" (0x60).  Note that the dollar
       sign may be prohibited by the test above. */
    err_code = ec_UCN_names_basic_char;
  } else if (ucn >= 0xd800 && ucn <= 0xdfff) {
    /* A UCN cannot name a character in the range of 0xd800-0xdfff. */
    err_code = ec_invalid_UCN;
  } else if (is_identifier) {
    /* Check whether this is a valid identifier character. */
    err_code = is_valid_UCN_identifier_char(ucn, is_identifier_start);
  }  /* if */
  if (err_code != ec_no_error) {
    /* Get the source position that corresponds to this character. */
    conv_line_loc_to_source_pos(*start_pos, &error_position);
    diagnostic(strict_ansi_error_severity, err_code);
  }  /* if */
}  /* check_for_invalid_c99_ucn */


unsigned long scan_universal_character(char		**start_pos,
				       a_boolean	is_identifier,
				       a_boolean	is_identifier_start,
				       a_boolean	issue_diagnostics)
/*
Scan the universal character name starting at start_pos.  The
character specified by the universal character name is returned.  If
is_identifier is TRUE, an error is issued if the character is not one
of those designated as a valid identifier character.  If is_identifier_start
is TRUE, it must be one of those characters that is not designated as a
digit.  If issue_diagnostics is TRUE, diagnostic messages are
produced if the universal character is improperly formed, or if it
names an invalid character.  start_pos is updated by this routine to
point to the character after the universal character name.
*/
{
  char		*pos = *start_pos;
  a_boolean	err = FALSE;
  unsigned long	result = 0;
  int		digits;

  /* The current position must be the start of a universal character name. */
  check_assertion_str2(*pos == '\\' && (*(pos+1) == 'u' || *(pos+1) == 'U'),
                       "scan_universal_character:",
                       "curr pos not universal character");
  /* Suppress diagnostics if we are skipping over this string because of
     some kind of preprocessor "if" directive, or when doing the initial
     PCH prefix scan of a file. */
  if (currently_in_pp_if_skip || building_pch_prefix) {
    issue_diagnostics = FALSE;
  }  /* if */
  /* Skip past the "\u" or "\U", and determine whether we are processing a
     four or eight character name.  "\u" is followed by four hex digits,
     "\U" is followed by eight hex digits. */
  pos++;
  digits = *pos++ == 'u' ? 4 : 8;
  /* Scan the digits and calculate the character value.  Stop scanning if
     we encounter an invalid character. */
  for (; digits > 0; --digits) {
    char	ch;
    ch = *pos++;
    if (!isxdigit((unsigned char)ch)) {
      if (issue_diagnostics) {
        /* Get the source position that corresponds to this character. */
        conv_line_loc_to_source_pos(pos-1, &error_position);
        error(ec_malformed_universal_character);
      }  /* if */
      err = TRUE;
      break;
    }  /* if */
    result = (result << 4) | hexvalue(ch);
  }  /* for */
  /* Check whether the specified character is a valid universal character. */
  if (!err && issue_diagnostics) {
    if (!C_mode()) {
      check_for_invalid_cplusplus_ucn(result, start_pos, is_identifier,
                                      is_identifier_start);
    } else {
      check_for_invalid_c99_ucn(result, start_pos, is_identifier,
                                is_identifier_start);
    }  /* if */
  }  /* if */
  if (is_identifier) {
    /* Record the fact that an identifier containing a UCN has been
       encountered. */
   il_header.UCN_identifiers_used = TRUE;
  }  /* if */
  *start_pos = pos;
  return result;
}  /* scan_universal_character */


static a_boolean accum_quoted_string(unsigned long *num_chars,
                                     a_boolean     is_header_name,
                                     a_boolean     is_wide,
                                     char          quoting_char)
/*
Scan a quoted construct, of kind indicated by ctoken.  This routine is
used for character constants and string literals, in both the "wide"
and normal forms (is_wide indicates which), and for header names in
#include and #line directives (is_header_name is TRUE for the #include
case).  curr_char_loc is just past the initial quote.  Scan to the
matching closing quote (indicated by quoting_char), and do not be
confused by escaped characters and multibyte character sequences.
Increment *num_chars by the number of (possibly wide) characters
contained in the string, after escape processing.  curr_char_loc and
end_of_curr_token are set to point just before the closing quote of
the string.  The return value is TRUE if the string was not terminated
before the end of the line, FALSE if it was.  The caller is
responsible for issuing error messages.
*/
{
  register char ch;
  unsigned long	nchars;
  a_boolean     unterminated = FALSE;

  nchars = 0;
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
  /* Initialize for scanning multibyte characters in the string. */
  mbc_scan_init_if_multibyte_chars_in_source_enabled();
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
  /* Scan through the characters of the string looking for the closing quoting
     character. */
  while ((ch = *curr_char_loc) != quoting_char) {
    if (ch == '\\' && !is_header_name) {
      /* Backslash, escapes the next character.  If followed by "0" or
         "x", an octal or hexadecimal value must be scanned.  We recognize
         those digits so we can accurately count characters, but we do
         not convert them at this point. */
      ch = *(++curr_char_loc);
      if (ch == LE_ESCAPE) {
        /* Token ends after the "\" -- this is an unclosed string.  This can
           happen because of macro definitions on the command line, e.g.,
           -DX="\ */
        unterminated = TRUE;
        goto return_point;
      } else if ((ch == 'u' || ch == 'U') &&
                 universal_character_names_allowed) {
        /* A universal character name escape sequence.  Skip past the
           characters that make up the universal character.  Ignore any
           errors at this point -- they will be issued when the escape
           is converted to a character. */
        /* Back up one character because the routine expects the opening
           backslash to be the current character. */
        curr_char_loc--;
        (void)scan_universal_character(&curr_char_loc,
                                       /*is_identifier=*/FALSE,
				       /*is_identifier_start=*/FALSE,
                                       /*issue_diagnostics=*/FALSE);
        nchars++;
      } else {
        curr_char_loc++;
        nchars++;
        if (isdigit((unsigned char)ch) && ch != '8' && ch != '9') {
          /* Octal escape, one to three digits.  Note that neither ANSI nor
             pcc allows 8 and 9 as octal digits in this case.  Note that
             there is code in conv_single_char that must match this code.*/
          ch = *curr_char_loc;
          if (isdigit((unsigned char)ch) && ch != '8' && ch != '9') {
            curr_char_loc++;
            ch = *curr_char_loc;
            if (isdigit((unsigned char)ch) && ch != '8' && ch != '9') {
              curr_char_loc++;
            }  /* if */
          }  /* if */
        } else if (ch == 'x') {
          /* Hex escape, any number of digits. */
          while (isxdigit((unsigned char)*curr_char_loc)) curr_char_loc++;
        }  /* if */
      }  /* if */
    } else if (ch == LE_ESCAPE) {
      /* Lexical escape, e.g., newline.  The string is unterminated at
         end of line or in some other strange way that comes up with
         preprocessing. */
      unterminated = TRUE;
      goto return_point;
    } else {
      /* Normal character. */
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
      if (multibyte_chars_in_source_enabled) {
        /* Advance to the next character, dealing with multibyte characters. */
        int numch = mbc_length(curr_char_loc, (a_boolean *)NULL);
        curr_char_loc += numch;
        if (is_wide) {
          nchars++;
        } else {
          nchars += (unsigned long)numch;
        }  /* if */
      } else
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
      /* Do not insert code here -- this is the "else" of an "if". */
      {
        /* Advance to the next character without worrying about multibyte
           characters. */
        curr_char_loc++;
        nchars++;
      }  /* if */
    }  /* if */
  }  /* while */
return_point:
  end_of_curr_token = curr_char_loc;
  if (unterminated) end_of_curr_token--;
  *num_chars += nchars;
  return unterminated;
}  /* accum_quoted_string */


static a_token_kind scan_char_constant(void)
/*
Scan a character constant token, return the token kind or tok_error.
The token can be a normal or wide character constant.
*/
{
  a_token_kind  ctoken = tok_char_constant;
  unsigned long num_chars = 0;
  a_boolean     is_wide = FALSE;
  an_error_code err_code;
  char          *err_pos;

  if (*curr_char_loc == 'L') {
    is_wide = TRUE;
    curr_char_loc++;
  }  /* if */
  check_assertion(*curr_char_loc == '\'');
  curr_char_loc++;
  if (accum_quoted_string(&num_chars, /*is_header_name=*/FALSE,
                          is_wide, '\'')) {
    /* Error, character constant is unclosed. */
    /* Similar error for other strange cases of incomplete strings, which
       can come up with preprocessing. */
    /* Message is generic -- "Missing closing quote". */
    ctoken = tok_error;
    err_code_for_error_token = ec_unclosed_string;
  } else {
    /* Advance past closing quote. */
    check_assertion(*curr_char_loc == '\'');
    curr_char_loc++;
    /* Character constants may not be zero length. */
    if (num_chars == 0) {
      ctoken = tok_error;
      err_code_for_error_token = ec_zero_length_string;
    }  /* if */
  }  /* if */
  if (!fetch_pp_tokens) {
    /* Convert the constant to internal form. */
    if (ctoken == tok_error) {
      ctoken = tok_char_constant;
      set_error_constant(&const_for_curr_token);
      error_at_line_pos(err_code_for_error_token, start_of_curr_token);
    } else {
      conv_char_literal(num_chars, &err_code, &err_pos);
      /* Check for errors detected. */
      if (err_code != ec_no_error) {
        error_at_line_pos(err_code, err_pos);
      }  /* if */
    }  /* if */
  }  /* if */
  return ctoken;
}  /* scan_char_constant */

#if GNU_EXTENSIONS_ALLOWED

static a_boolean scan_multiline_string(unsigned long *num_chars,
                                       a_boolean     is_wide)
/*
Process the second and subsequent lines of a multi-line string.
Return TRUE if the string turns out to be well-formed, FALSE
otherwise.
*/
{
  an_orig_line_modif_ptr olmp;
  a_boolean              result = FALSE;

  while (curr_char_loc[0] == LE_ESCAPE &&
         curr_char_loc[1] == LE_NEWLINE) {
    /* Inject the characters \ n on top of the NEWLINE escape,
       and add an entry to the orig_line_modif_list so that this
       can be undone. */
    olmp = add_orig_line_modif(olm_multiline_string_splice,
                               curr_char_loc);
    olmp->variant.line_splice_seq_number = seq_number_last_read + 1;
    *curr_char_loc++ = '\\';
    *curr_char_loc++ = 'n';
    /* Read the next line of the input file, extending the current
       logical source line. */
    if (read_logical_source_line(/*do_pop_on_end_of_file=*/FALSE,
                                 /*extend_current_line=*/TRUE)) {
      /* End of file, report an error. */
      break;
    }  /* if */
    /* Back up over the \n added above and resume scanning.  */
    curr_char_loc -= 2;
    if (!accum_quoted_string(num_chars, /*is_header_name=*/FALSE,
                             is_wide, '"')) {
      /* End of string, done. */
      result = TRUE;
      break;
    }  /* if */
  }  /* while */
  return result;
}  /* scan_multiline_string */

#endif /* GNU_EXTENSIONS_ALLOWED */

static a_token_kind scan_string_literal(void)
/*
Scan a string literal token, return the token kind or tok_error.
The token can be a normal or wide string literal.
*/
{
  a_token_kind           ctoken = tok_string_literal;
  a_boolean              is_wide = FALSE;
  unsigned long          num_chars = 0;
  an_error_code          err_code;
  char                   *err_pos;

  if (*curr_char_loc == 'L') {
    is_wide = TRUE;
    curr_char_loc++;
  }  /* if */
  check_assertion(*curr_char_loc == '"');
  curr_char_loc++;
  if (accum_quoted_string(&num_chars, /*is_header_name=*/FALSE,
                          is_wide, '"')
#if GNU_EXTENSIONS_ALLOWED
      /* GCC permits a string literal to extend over multiple lines. */
      && (!gcc_mode || curr_command_line_macro_def != NULL ||
          !scan_multiline_string(&num_chars, is_wide))
#endif  /* GNU_EXTENSIONS_ALLOWED */
                                                      ) {
    /* Error, string is unclosed. */
    /* Similar error for other strange cases of incomplete strings, which
       can come up with preprocessing. */
    /* Message is generic -- "Missing closing quote". */
    ctoken = tok_error;
    err_code_for_error_token = ec_unclosed_string;
  } else {
    /* Advance past closing quote. */
    check_assertion(*curr_char_loc == '"');
    curr_char_loc++;
  }  /* if */
  if (!fetch_pp_tokens) {
    /* Convert the constant to internal form. */
    if (ctoken == tok_error) {
      ctoken = tok_string_literal;
      set_error_constant(&const_for_curr_token);
      error_at_line_pos(err_code_for_error_token, start_of_curr_token);
    } else {
      conv_string_literal(num_chars, &err_code, &err_pos);
      /* Check for errors detected. */
      if (err_code != ec_no_error) {
        error_at_line_pos(err_code, err_pos);
      }  /* if */
    }  /* if */
  }  /* if */
  return ctoken;
}  /* scan_string_literal */


static a_token_kind scan_header_name(void)
/*
Scan a header name token, return the token kind or tok_error.
*/
{
  a_token_kind	ctoken = tok_header_name;
  unsigned long	num_chars = 0;
  char		quoting_char;

  quoting_char = *curr_char_loc++;
  /* Angle-bracket includes are closed by a different character than
     opens them. */
  if (quoting_char == '<') quoting_char = '>';
  check_assertion(quoting_char == '"' || quoting_char == '>');
  if (accum_quoted_string(&num_chars,
                          /*is_header_name=*/TRUE,
                          /*is_wide=*/FALSE,
                          quoting_char)) {
    /* Error, header name is unclosed. */
    ctoken = tok_error;
    err_code_for_error_token = ec_unclosed_string;
  } else {
    /* Advance past closing quote. */
    check_assertion(*curr_char_loc == quoting_char);
    curr_char_loc++;
    /* Header names may not be zero length. */
    if (num_chars == 0) {
      ctoken = tok_error;
      err_code_for_error_token = ec_zero_length_string;
    }  /* if */
  }  /* else */
  if (!fetch_pp_tokens && ctoken == tok_error) {
    error_at_line_pos(err_code_for_error_token, start_of_curr_token);
    ctoken = tok_header_name;
  }  /* if */
  return ctoken;
}  /* scan_header_name */

#if ASM_SUPPORT_NEEDED

#define ASM_FUNC_BODY_BUFFER_INCREMENTAL_ALLOCATION 1024
			/* Initial and incremental allocation size for
			   asm_func_body_buffer.  The initial allocation
			   should be such that almost all cases can be
			   accepted (so that the realloc is hardly ever
			   needed). */


static void expand_asm_func_body_buffer(sizeof_t size_needed)
/*
Expand the asm_func_body_buffer by reallocating it, so that its total size
is at least size_needed.  Called by ensure_asm_func_body_buffer_space.
*/
{
  sizeof_t new_size;

  new_size = size_asm_func_body_buffer +
                      ASM_FUNC_BODY_BUFFER_INCREMENTAL_ALLOCATION;
  if (new_size < size_needed) new_size  = size_needed;
  asm_func_body_buffer = realloc_general(asm_func_body_buffer,
                                         size_asm_func_body_buffer, new_size);
  size_asm_func_body_buffer = new_size;
}  /* expand_asm_func_body_buffer */


static void add_to_asm_func_buffer(char      *start_char,
                                   sizeof_t  len)
/*
Add len characters to the asm function body buffer, beginning at start_char
(a pointer to a piece of text in the source program).
*/
{
  /* Ensure that asm_func_body_buffer has enough space left to accommodate
     "len" bytes.  If not, expand asm_func_body_buffer by reallocating it. */
  if (size_asm_func_body_buffer - pos_in_asm_func_body_buffer < len) {
    expand_asm_func_body_buffer((sizeof_t)(pos_in_asm_func_body_buffer + len));
  }  /* if */
  memcpy(&asm_func_body_buffer[pos_in_asm_func_body_buffer],
         start_char, size_t_arg(len));
  pos_in_asm_func_body_buffer += len;
}  /* add_to_asm_func_buffer */

/*
These two remember where the previous copy_from_source_to_asm_func_buffer()
left off.  They are set by scan_asm_function_body() to start
just after the initial left brace.
*/
static char *prev_stop_char;
static a_seq_number prev_seq_number;

#if !ASM_FUNCTION_ALLOWED
static
#endif /* !ASM_FUNCTION_ALLOWED */
void reset_asm_buffer(void)
/*
Reset the static variables used while building the string representation
of an asm function or Microsoft asm block.
*/
{
  pos_in_asm_func_body_buffer = 0;
  prev_stop_char = NULL;
  prev_seq_number = curr_seq_number;
}  /* reset_asm_buffer */

#if !INCLUDE_COMMENTS_IN_ASM_FUNC_BODY
/*ARGSUSED*/ /* after_comment_stop_char is unused. */
#endif /* !INCLUDE_COMMENTS_IN_ASM_FUNC_BODY */
#if !ASM_FUNCTION_ALLOWED
static
#endif /* !ASM_FUNCTION_ALLOWED */
void copy_from_source_to_asm_func_buffer(char *stop_char,
                                         char *after_comment_stop_char)
/*
The buffer in which to collect the characters comprising the asm function is
asm_func_body_buffer.  Append to it all the characters in the source beginning
at *prev_stop_char through, but not including, *stop_char.  Then, if
INCLUDE_COMMENTS_IN_ASM_FUNC_BODY is TRUE and after_comment_stop_char is
non-NULL, also append the characters in the comment, through but not including
*after_comment_stop_char.
*/
{
  a_source_line_modif_ptr  slmp;
  char                     *curr_char, *next_char;
  sizeof_t                 len;
  char                     ch;
  a_boolean                ends_with_newline = FALSE;

  if (prev_seq_number != curr_seq_number) {
    /* We've advanced to a new source line.  Update prev_stop_char to point to
       the start of the new line. */
    if (line_start_source_line_modif != NULL) {
      prev_stop_char = line_start_source_line_modif->inserted_text;
    } else {
      prev_stop_char = curr_source_line;
    }  /* if */
    prev_seq_number = curr_seq_number;
  }  /* if */
  /* curr_char is the pointer into the source line.  Its initial value is
     usually prev_stop_char, which is usually the character following the
     last character that was copied into the buffer. */
  if (prev_stop_char != NULL) {
    curr_char = prev_stop_char;
  } else {
    /* prev_stop_char is NULL, which is the case on the first call to this
       routine for a given asm function.  Use the initial character of the
       current token. */
    curr_char = start_of_curr_token;
    if (curr_char == NULL) {
      /* Rare case -- there is no start_of_curr_token pointer. */
      curr_char = stop_char;
    }  /* if */
    prev_stop_char = curr_char;
  }  /* if */
  while (curr_char != stop_char) {
    switch (*curr_char) {
      case LE_ESCAPE:
        ch = curr_char[1];
        if (ch == LE_END_OF_TOKEN ||
            ch == LE_INERT_MACRO) {
          /* Marker put into text by preprocessing of macros, to force the same
             interpretation of token boundaries as during the macro definition.
             Or, marker that indicates that a macro name should not be
             expanded.  Skip over the escape and don't put it out. */
          next_char = curr_char + LE_ESCAPE_LEN;
        } else if (ch == LE_END_OF_INSERTION) {
          /* End of the expansion text for a macro.  Find the character
             location of the character following the macro invocation, and
             continue there. */
          slmp = assoc_source_line_modif(curr_char);
          next_char = curr_char;
          leave_insertion(slmp, next_char);
        } else if (ch == LE_NEWLINE) {
          /* Newline character. */
          ends_with_newline = TRUE;
          next_char = curr_char + LE_ESCAPE_LEN;
        } else {
          unexpected_condition_str(
                    "copy_from_source_to_asm_func_buffer: bad lexical escape");
        }  /* if */
        break;
      case ATTENTION_MARKER:
        /* Marker placed into source text to provide a cue to the fact that
           a source modification (probably a text replacement due to a
           macro expansion) begins here.  next_char will be adjusted by the
           macro to point to the first character in the insertion text. */
        next_char = curr_char;
        go_into_insertion(slmp, next_char);
        break;
      default:
        /* Normal case:  bump curr_char and keep looping. */
        curr_char++;
        continue;
    }  /* switch */
    /* Falling through to here mean one of the special characters was seen.
       Copy the characters from prev_stop_char through (but not including)
       curr_char into the buffer, and then reset prev_stop_char and curr_char
       to next_char. */
    if ((len = curr_char - prev_stop_char) != 0) {
      /* Add "len" characters to the buffer, starting at prev_stop_char. */
      add_to_asm_func_buffer(prev_stop_char, len);
    }  /* if */
    curr_char = prev_stop_char = next_char;
    if (ends_with_newline) {
      ends_with_newline = FALSE;
      len = 1;
      add_to_asm_func_buffer("\n", len);
    }  /* if */
  }  /* while */
  if (curr_char > prev_stop_char) {
    /* Copy the characters from prev_stop_char through (but not including)
       curr_char into the buffer. */
    len = curr_char - prev_stop_char;
    /* Add "len" characters to the buffer, starting at prev_stop_char. */
    add_to_asm_func_buffer(prev_stop_char, len);
    prev_stop_char = curr_char;
  }  /* if */
#if INCLUDE_COMMENTS_IN_ASM_FUNC_BODY
  if (after_comment_stop_char != NULL) {
    /* Append text of commentary, too. */
    ends_with_newline = FALSE;
    check_assertion(after_comment_stop_char > prev_stop_char);
    len = after_comment_stop_char - prev_stop_char;
    if (len >= LE_ESCAPE_LEN &&
        after_comment_stop_char[-LE_ESCAPE_LEN  ] == LE_ESCAPE &&
        after_comment_stop_char[-LE_ESCAPE_LEN+1] == LE_NEWLINE) {
      /* The comment ends with a newline.  Put it out separately below (the
         lexical escape in the line is not the '\n' character we want in
         the string). */
      ends_with_newline = TRUE;
      len -= LE_ESCAPE_LEN;
    }  /* if */
    /* Add "len" characters to the buffer, starting at prev_stop_char. */
    add_to_asm_func_buffer(prev_stop_char, len);
    if (ends_with_newline) {
      len = 1;
      add_to_asm_func_buffer("\n", len);
    }  /* if */
    /* Reset prev_stop_char. */
    prev_stop_char = after_comment_stop_char;
  }  /* if */
#endif /* INCLUDE_COMMENTS_IN_ASM_FUNC_BODY */
}  /* copy_from_source_to_asm_func_buffer */

#endif /* ASM_SUPPORT_NEEDED */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void skip_asm_comment(a_boolean	include_newline)
/*
A semicolon in a Microsoft asm begins an asm comment.  Process the rest
of the tokens on the line specially so that they are not included in
brace counts (for example).  include_newline is TRUE if the terminating
newline should be included in the asm string.
*/
{
  while (curr_token != tok_newline && curr_token != tok_end_of_source) {
    (void)get_token();
    if (curr_token != tok_newline || include_newline) {
      copy_from_source_to_asm_func_buffer(end_of_curr_token + 1, (char *)NULL);
    }  /* if */
  }  /* while */
}  /* skip_asm_comment */


static void build_microsoft_asm_string(void)
/*
When an __asm token is encountered in Microsoft mode, the string that
follows is immediately scanned and stored in curr_token_asm_string.
This value is saved and restored as needed by the token caching
mechanism.  This routine scans and builds the asm string.
*/
{
  unsigned int		nbrace = 0;
  char			*body;
  a_boolean		is_asm_block;
  a_boolean		save_token = FALSE;
  a_source_position	saved_pos_curr_token;

  /* Set a flag that indicates we are scanning a Microsoft asm.  This
     prevents this routine from being called recursively. */
  scanning_microsoft_asm = TRUE;
  /* Save the position of this token so that it can be restored later. */
  saved_pos_curr_token = pos_curr_token;
  /* Advance past the __asm token. */
  (void)get_token();
  /* Initialize variables used for building the string. */
  reset_asm_buffer();
  /* Initialize global variables used by lexical routines. */
  in_asm_function_body = TRUE;
  treat_newline_as_token = TRUE;
  fetch_pp_tokens = TRUE;
  is_asm_block = curr_token == tok_lbrace;
  if (is_asm_block) {
    /* Loop through the tokens and build the string token by token. */
    while (curr_token != tok_end_of_source) {
      /* Special handling for a left brace embedded within the assembler
         code: assume it has a matching right brace. */
      if (curr_token == tok_lbrace) ++nbrace;
      /* Copy characters from the source line to the buffer, from
         last_stop_char through the end of the current token. */
      copy_from_source_to_asm_func_buffer(end_of_curr_token + 1, (char *)NULL);
      /* Stop when a zero-level right brace is reached.
         Keep track of braces. */
      if (curr_token == tok_rbrace && --nbrace == 0) {
        /* This right brace matches the opening left brace, marking the end of
           the asm function body. */
        break;
      }  /* if */
      /* Advance to the next token. */
      (void)get_token();
      /* If this is the start of an asm comment, process the rest of the line
         specially. */
      if (curr_token == tok_semicolon) {
        skip_asm_comment(/*include_newline=*/TRUE);
      }  /* if */
    }  /* while */
  } else {
    /* Not an asm block.  Just take tokens up to the end of the line or up
       to an opening brace. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    a_source_position	end_pos;
    end_pos = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    while (curr_token != tok_end_of_source) {
      if (curr_token == tok_newline || curr_token == tok_rbrace) {
        save_token = curr_token == tok_rbrace;
#if EXTRA_SOURCE_POSITIONS_IN_IL
        if (curr_token == tok_newline) {
          /* Restore the ending position to that of the token before the
             newline. */
          end_pos_curr_token = end_pos;
        }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        break;
      }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
      end_pos = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      /* Copy characters from the source line to the buffer, from
         last_stop_char through the end of the current token. */
      copy_from_source_to_asm_func_buffer(end_of_curr_token + 1, (char *)NULL);
      /* If this is the start of an asm comment, process the rest of the line
         specially. */
      if (curr_token == tok_semicolon) {
        skip_asm_comment(/*include_newline=*/FALSE);
      } else {
        /* Advance to the next token. */
        (void)get_token();
      }  /* if */
    }  /* while */
  }  /* if */
  fetch_pp_tokens = FALSE;
  in_asm_function_body = FALSE;
  treat_newline_as_token = FALSE;
  if (save_token) {
    /* The current token is not part of the asm string.  Cache it so that
       it can will be rescanned after the tok_microsoft_asm is fetched. */
    a_token_cache	cache;
    clear_token_cache(&cache, /*reusable=*/FALSE);
    cache_curr_token(&cache);
    rescan_cached_tokens(&cache);
  }  /* if */
  /* Allocate a block of the current IL memory region (the one established
     for the asm function) -- the asm buffer will be copied into it, along
     with a trailing null character. */
  body = alloc_asm_function_body(pos_in_asm_func_body_buffer + 1);
  (void)memcpy(body, asm_func_body_buffer,
               size_t_arg(pos_in_asm_func_body_buffer));
  /* Add a null terminator. */
  body[pos_in_asm_func_body_buffer] = '\0';
  curr_token_asm_string = body;
  curr_token = tok_microsoft_asm;
  /* Restore the token start position of the __asm token. */
  pos_curr_token = saved_pos_curr_token;
  /* Reset the token start pointer because we may have moved to another
     source line. */
  start_of_curr_token = NULL;
#if DEBUG
  if (debug_level >= 3 || db_flag_is_set("asm_string")) {
    fprintf(f_debug, "asm string: %s\n", body);
  }  /* if */
#endif /* DEBUG */
  scanning_microsoft_asm = FALSE;
}  /* build_microsoft_asm_string */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */


static void adjust_pp_int_constant(void)
/*
The current token is an integer constant scanned within a preprocessing
#if expression.  It should be made long if it is not already so (or intmax_t
in C99 mode).  See C89 standard, 3.8.1.
*/
{
  an_integer_kind ik;

  ik = const_for_curr_token.type->variant.integer.int_kind;
  if (c99_mode) {
    /* In C99 mode, use intmax_t for signed types, uintmax_t for unsigned
       types. */
    if (ik == targ_intmax_kind || ik == targ_uintmax_kind) {
      /* The type is already right, so leave it alone. */
    } else {
      if (!int_kind_is_signed[(int)ik]) {
        ik = targ_uintmax_kind;
      } else {
        ik = targ_intmax_kind;
      }  /* if */
      const_for_curr_token.type = integer_type(ik);
    }  /* if */
  } else {
    /* C++ or C89.  Use long or unsigned long. */
    if (ik == (an_integer_kind)ik_long ||
        ik == (an_integer_kind)ik_unsigned_long) {
      /* The type is long, so leave it alone. */
#if LONG_LONG_ALLOWED
    } else if (ik == (an_integer_kind)ik_long_long ||
               ik == (an_integer_kind)ik_unsigned_long_long) {
      /* The type is long long, so leave it alone. */
#endif /* LONG_LONG_ALLOWED */
    } else {
      /* The type is smaller than long, so change it.  It's changed to
         unsigned long if the current type is unsigned, otherwise to long. */
      if (!int_kind_is_signed[(int)ik]) {
        ik = (an_integer_kind)ik_unsigned_long;
      } else {
        ik = (an_integer_kind)ik_long;
      }  /* if */
      const_for_curr_token.type = integer_type(ik);
    }  /* if */
  }  /* if */
}  /* adjust_pp_int_constant */


static void concat_adjacent_string_literals(void)
/*
The current token (not in curr_token yet, but in const_for_curr_token) is
a string literal (tok_string_literal), and in the current lexical mode
normal (not pp) tokens should be fetched, and concatenation of adjacent
string literals should be done.  Look to see if the next token of input
is a string literal, and if so, concatenate it with the current token.
Loop to pick up all the adjacent string literals.
*/
{
  a_boolean          wide_strings;
  a_token_cache      cache;
  a_cached_token_ptr ctp, ctp_next, first_string_token = NULL;
  a_boolean          more_than_one_string = FALSE;

  db_enter(5, "concat_adjacent_string_literals");
  check_assertion_str(!fetch_pp_tokens && do_string_literal_concatenation,
                      "concat_adjacent_string_literals: bad mode");
  /* See if the string is a wide string. */
  wide_strings = FALSE;
  /* Watch out for the case where the constant is an error constant. */
  if (!is_error_constant(&const_for_curr_token)) {
    /* Use the negative test because in C mode, and in C++ mode when wchar_t
       is not a keyword, wchar_t and char could be the same type. */
    wide_strings = !is_char_array_type(const_for_curr_token.type);
  }  /* if */
  /* Start a token cache in which we will accumulate all the adjacent
     string literals.  Usually, this will be just a single string literal. */
  clear_token_cache(&cache, /*reusable=*/FALSE);
  /* Set up the first string literal as the current token, so it can be
     cached.  This routine is called from get_token at a point where
     there is, in effect, no current token, so we're just anticipating the
     action that would be done on return from get_token here.
     const_for_curr_token is already set; start_of_curr_token and
     end_of_curr_token are already set; len_of_curr_token does not need
     to be set. */
  curr_token = tok_string_literal;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  check_assertion(end_of_curr_token != NULL);
  /* Determine the source position of the end of the string. */
  macro_line_loc_to_source_pos(end_of_curr_token, end_pos_curr_token);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Loop as long as the next token is a string literal. */
  for (;;) {
    /* Save the current token (a string literal) by adding it to the token
       cache. */
    cache_curr_token(&cache);
    /* Remember the first string token (there may be pragma entries preceding
       it in the cache). */
    if (first_string_token == NULL) {
      first_string_token = cache.last_token;
    } else {
      /* There is more than one string literal, so concatenation will have
         to be done below. */
      more_than_one_string = TRUE;
    }  /* if */
    /* Scan the next token.  Suppress string literal concatenation so that
       when scanning something like
         "aaa" "bbb" "ccc"
       the get_token call at "bbb" does not fetch "bbbccc" (which would cause
       undesirable behavior in terms of memory use). */
    do_string_literal_concatenation = FALSE;
    (void)get_token();
    do_string_literal_concatenation = TRUE;
    /* End the loop if the new token is not a string literal. */
    if (curr_token != tok_string_literal) break;
    /* Also end the loop if the new string is wide and the old is not, or
       vice-versa.  In C mode, and in C++ mode when wchar_t is not a keyword,
       if wchar_t is char, wide and non-wide string literals will be
       concatenated.  An error constant is accepted in all modes. */
    if (!is_error_constant(&const_for_curr_token) &&
        wide_strings != !is_char_array_type(const_for_curr_token.type)) break;
    /* This string literal is okay, and will be added to the concatenation
       in the token cache. */
  }  /* for */
  /* Here, all the adjacent string literals have been captured in a token
     cache.  Concatenate them into a single string literal. */
  if (!more_than_one_string) {
    /* The common degenerate case of a single string literal requires no
       concatenation. */
  } else {
    a_cached_token_ptr last_token;
    /* More than one string literal -- concatenate. */
    concat_string_literals(&cache, wide_strings);
    /* The constants have been concatenated into the first constant in the
       token cache (which might not be the first entry in the cache, if there
       are pragma entries first).  Discard the token cache entries for the
       string literal tokens after that first one. */
    last_token = first_string_token;
    for (ctp = first_string_token->next; ctp != NULL; ctp = ctp_next) {
      ctp_next = ctp->next;
      if (ctp->extra_info_kind == (a_token_extra_info_kind)teik_pragma) {
        /* Keep a pragma entry (this is a pragma entry that appeared between
           string literals). */
        last_token->next = ctp;
        last_token = ctp;
      } else {
#if EXTRA_SOURCE_POSITIONS_IN_IL
        /* Update the end of token position of the resulting string with the
           end position of the subsequent segment that is about to be freed.
           This results in the ending position of the last string segment
           being used as the ending position of the concatenated string. */
        first_string_token->end_source_position = ctp->end_source_position;
#endif /*  EXTRA_SOURCE_POSITIONS_IN_IL */
        /* Free a string literal token entry. */
        free_cached_token(ctp);
#if DEBUG
        cache.token_count--;
#endif /* DEBUG */
      }  /* if */
    }  /* for */
    last_token->next = NULL;
    cache.last_token = last_token;
  }  /* if */
  /* Stick the remaining single string literal back onto the input token
     stream (ahead of the non-string-literal token that stopped the loop). */
  rescan_cached_tokens(&cache);
  db_exit();
}  /* concat_adjacent_string_literals */


/*
Remember that a token has been gotten from the current source line.
Remember the start position (sequence number, column) of the token,
and also put that into error_position.
*/
#define remember_token_start()                                        \
{ any_tokens_gotten_from_curr_source_line = TRUE;                     \
  macro_line_loc_to_source_pos(start_of_curr_token, pos_curr_token);  \
  error_position = pos_curr_token;                                    \
}  /* remember_token_start */

/*
Macro that is TRUE if digraph tokens should be recognized.  Note that
alternative_tokens_allowed is only TRUE in C++ mode.
*/
#define digraphs_allowed() (alternative_tokens_allowed)


a_token_kind get_token(void)
/*
Scan the next token of input, and return its kind.  The kind of token is
also saved in curr_token.

If fetch_pp_tokens is TRUE when this routine is called, a preprocessing
token (pp-token) is fetched instead of a regular token.  Constants are
not converted (and numeric ones are returned as pp-numbers); adjacent
string literals are not concatenated; keywords are not recognized; and
errors are not issued for malformed tokens.  Also, start_of_curr_token
and end_of_curr_token will be set to point to the beginning and end of
the current token, and len_of_curr_token will be set to its length.

If expand_macros is TRUE, preprocessing macros are expanded as they are
scanned.  The caller sees only the tokens after expansion.  If a
macro name is preceded by an LE_INERT_MACRO escape sequence, the
macro is not expanded and the identifier is returned to the caller with
curr_token_is_inert_macro set to TRUE.

When fetch_pp_tokens is FALSE, do_string_literal_concatenation controls
whether adjacent string literals are concatenated.

If in_preprocessing_directive is TRUE, the definition of white space is
changed to that for within preprocessing directives, and newline is
returned as a token.  Normally, if in_preprocessing_directive is TRUE
keywords are not recognized, and "#", and "##" are recognized and
returned as tokens.  Recognition of keywords is re-enabled when
caching_pragma_tokens and recognize_keywords_in_pragma are both TRUE.
The "#" and "##" tokens are re-enabled when caching_pragma_tokens is TRUE.

If in_pp_if_expression is TRUE (indicating that we are inside a
preprocessing #if expression), integer constants will get an implicit
"L" suffix, and undefined identifiers will be returned as the integer
constant 0L.

If exp_header_name is TRUE, then "..." will be scanned as a header name
for a #include (tok_header_name).  Other tokens will be processed normally.
exp_system_header_name is similar, for header names of the form <...>.

If exp_digit_sequence is TRUE, then a string of decimal digits will be
scanned as a tok_digit_sequence (used in the #line directive).  Other tokens
will be processed normally.

If treat_newline_as_token is TRUE, return tok_newline for ends of lines.

If cached_token_rescan_list is non-NULL, it points to a list of cached
tokens which are to be rescanned; the first token on that list is removed
and returned.  Otherwise, if reusable_cache_stack is non-NULL, it points
to a list of reusable cache entries; the next token on that list is
returned without destroying the reusable cache.

In the case where an invalid token is scanned, tok_error is returned
and err_code_for_error_token is set to indicate a diagnostic that
describes the error.  Except when fetch_pp_tokens is TRUE, the error
will have been issued by get_token.

This routine is called an enormous number of times, and therefore has
been written to be as fast as possible.  Structure has been sacrificed
to speed in some cases.
*/
{
  register a_token_kind ctoken;
  register char         ch;
  register a_symbol_ptr	assoc_symbol;
  a_symbol_kind		id_kind;
  a_boolean		rescan, is_inert_macro = FALSE;
  a_boolean		continue_scan;
#if DEBUG
  a_boolean             gotten_from_cache = FALSE;
#endif /* DEBUG */

  if (any_initial_get_token_tests_needed) {
    /* Before fetching a new token, do any processing required for pragmas
       that preceded the current token.  Don't do this when fetching
       preprocessing tokens -- pragmas should only be processed when
       a "real" token of the source program is fetched. */
    if (curr_token_pragmas != NULL &&
        !suppress_pragma_processing) {
      process_curr_token_pragmas();
      recalc_any_initial_get_token_tests_needed();
    }  /* if */
    /* If there are cached tokens to be rescanned, first check the
       cached_token_rescan_list and take the first token on the list if
       it is non-NULL, otherwise check the reusable cache stack. */
    /* If there are cached tokens to be rescanned, take the first on the
       list. */
    if (cached_token_rescan_list != NULL) {
      ctoken = get_token_from_cached_token_rescan_list();
#if DEBUG
      gotten_from_cache = TRUE;
#endif /* DEBUG */
      goto return_from_token_scan;
    } else if (reusable_cache_stack != NULL) {
      /* If there are tokens to be rescanned from the reusable cache stack
         take the next one on the list. */
      ctoken = get_token_from_reusable_cache_stack();
#if DEBUG
      gotten_from_cache = TRUE;
#endif /* DEBUG */
      goto return_from_token_scan;
    }  /* if */
  }  /* if */
  /* A new token is being scanned from the input stream.  Assign a token
     sequence number to this token. */
  curr_token_sequence_number = ++last_token_sequence_number_used;
rescan_token:
  /* Skip over any initial white space blanks and horizontal tabs.
     These are very common, so they're handled inline here.  The
     other potential white space characters (newline, vertical tab,
     form feed, and "/" and "*" indicating the start of a comment) are
     processed out of the main "switch" statement. */
  if ((ch = *curr_char_loc) == ' ' || ch == '\t') {
    do {} while ((ch = *(++curr_char_loc)) == ' ' || ch == '\t');
  }  /* if */
start_of_token_scan:  /* Restart here after scanning white space. */
  /* Remember the start character position of the token. */
  start_of_curr_token = curr_char_loc;
  /* Branch to different processing code according to the first
     character of the token.  *curr_char_loc must be used instead of
     ch because ch is not set when arriving at start_of_token_scan
     via goto from elsewhere. */
  switch (*curr_char_loc) {
    case LE_ESCAPE:
      /* Lexical escape.  Second character indicates which. */
      ch = curr_char_loc[1];
      if (ch == LE_END_OF_LINE || ch == LE_END_OF_INSERTION) {
        /* End of line or end of macro insertion.  Let the white-space
           routine figure it out. */
        skip_white_space();
        /* If we are not at end of file after the white-space skip, go scan
           the next token. */
        if (*curr_char_loc != LE_ESCAPE ||
            (curr_char_loc[1] != LE_END_OF_LINE &&
             curr_char_loc[1] != LE_END_OF_INSERTION)) {
          goto start_of_token_scan;
        }  /* if */
        /* This is the ultimate end of file, or the end of a macro argument
           string being scanned in isolation from the rest of the source.
           Return end of file. */
        ctoken = tok_end_of_source;
        start_of_curr_token = curr_char_loc;
        /* Remember the character position of the end of the token. */
        end_of_curr_token = curr_char_loc + LE_ESCAPE_LEN - 1;
        /* Determine the source position of the end of source token. */
        remember_token_start();
        /* If this is the line-end escape at the end of the primary source
           line, back up the column so it points at the end of the
           line. */
        if (curr_char_loc[1] == LE_END_OF_LINE) {
          if (curr_char_loc >= curr_source_line+LE_ESCAPE_LEN &&
              curr_char_loc[-LE_ESCAPE_LEN] == LE_ESCAPE) {
            /* The previous character is an escape sequence, presumably
               for a newline. */
            pos_curr_token.column -= LE_ESCAPE_LEN;
          } else {
            pos_curr_token.column -= 1;
          }  /* if */
          error_position.column = pos_curr_token.column;
        }  /* if */
        /* Go exit with the end-of-source token. */
        goto end_of_token_scan_b;
      } else if (ch == LE_NEWLINE) {
        /* Newline.  Is white space ordinarily, but a token within
           preprocessing directives and inside an asm function body. */
        if (in_preprocessing_directive || treat_newline_as_token) {
          ctoken = tok_newline;
          curr_char_loc += LE_ESCAPE_LEN;
          goto save_end_position;
        } else {
          skip_white_space();
          goto start_of_token_scan;
        }  /* if */
      } else if (ch == LE_END_OF_TOKEN) {
        /* Marker put into text by preprocessing of macros, to force the same
           interpretation of token boundaries as during the macro definition.
           At this level, should be ignored. */
        curr_char_loc += LE_ESCAPE_LEN;
        goto rescan_token;
      } else if (ch == LE_INERT_MACRO) {
        /* Marker put into text preceding a macro name to indicate that the
           macro name should not be expanded. */
        curr_char_loc += LE_ESCAPE_LEN;
        start_of_curr_token = curr_char_loc;
        is_inert_macro = TRUE;
        goto id_scan;
      } else {
        unexpected_condition_str("get_token: bad lexical escape");
      }  /* if */
      break;
    case '\f':
    case VERTICAL_TAB_CHARACTER:
      /* Form feed, vertical tab.  Usually white space, but implementation-
         defined when inside a preprocessing directive.  Let the white space
         routine decide. */
      skip_white_space();
      goto start_of_token_scan;
    case ATTENTION_MARKER:
      /* Marker placed into source text to provide a cue to the fact that
         a source modification (probably a text replacement due to a
         macro expansion) begins here.  Let the white-space routine
         handle it. */
      skip_white_space();
      goto start_of_token_scan;
    case '[':
      ctoken = tok_lbracket;
      break;
    case ']':
      ctoken = tok_rbracket;
      break;
    case '(':
      ctoken = tok_lparen;
      break;
    case ')':
      ctoken = tok_rparen;
      break;
    case '{':
      ctoken = tok_lbrace;
      break;
    case '}':
      ctoken = tok_rbrace;
      break;
    case ',':
      ctoken = tok_comma;
      break;
    case '~':
      ctoken = tok_compl;
      break;
    case ':':
      /* In C++, may be "::" or ":>". */
      if ((ch = *(curr_char_loc+1)) == ':' && !C_mode()) {
        ctoken = tok_colon_colon;
        goto two_char_token;
      } else if (ch == '>' && digraphs_allowed()) {
        ctoken = tok_rbracket;
	goto two_char_token;
      }  /* if */
      ctoken = tok_colon;
      break;
    case ';':
      ctoken = tok_semicolon;
      break;
    case '?':
      ctoken = tok_quest_mark;
      break;
    case '-':
      /* One of "--", "->", "-=", or "-". */
      /* In C++, "->*" is also a possibility. */
      if ((ch = *(curr_char_loc+1)) == '-') {
        ctoken = tok_minus_minus;
        goto two_char_token;
      } else if (ch == '>') {
        if (C_dialect == C_dialect_cplusplus &&
            *(curr_char_loc+2) == '*') {
          ctoken = tok_arrow_star;
          goto three_char_token;
        }  /* if */
        ctoken = tok_arrow;
        goto two_char_token;
      } else if (ch == '=') {
        ctoken = tok_minus_assign;
        goto two_char_token;
      }  /* if */
      /* Just plain "-". */
      ctoken = tok_minus;
      break;
    case '+':
      /* One of "++", "+=", or "+". */
      if ((ch = *(curr_char_loc+1)) == '+') {
        ctoken = tok_plus_plus;
        goto two_char_token;
      } else if (ch == '=') {
        ctoken = tok_plus_assign;
        goto two_char_token;
      }  /* if */
      /* Just plain "+". */
      ctoken = tok_plus;
      break;
    case '*':
      /* One of "*=" or "*". */
      if (*(curr_char_loc+1) == '=') {
        ctoken = tok_times_assign;
        goto two_char_token;
      }  /* if */
      /* Just plain "*". */
      ctoken = tok_star;
      break;
    case '/':
      /* One of "/ *" (start of comment), "/=", or "/". */
      /*          ^--- space here to avoid nested-comment problem. */
      if (start_of_comment()) {
        /* Go skip the comment. */
        skip_white_space();
        goto start_of_token_scan;
      } else if (*(curr_char_loc+1) == '=') {
        ctoken = tok_divide_assign;
        goto two_char_token;
      }  /* if */
      /* Just plain "/". */
      ctoken = tok_divide;
      break;
    case '&':
      /* One of "&&", "&=", or "&". */
      if ((ch = *(curr_char_loc+1)) == '&') {
        ctoken = tok_and_and;
        goto two_char_token;
      } else if (ch == '=') {
        ctoken = tok_and_assign;
        goto two_char_token;
      }  /* if */
      /* Just plain "&". */
      ctoken = tok_ampersand;
      break;
    case '%':
      /* One of "%=" or "%".  Or, in C++, "%>", "%:", "%:%:". */
      if (*(curr_char_loc+1) == '=') {
        ctoken = tok_remainder_assign;
        goto two_char_token;
      } else if ((ch = *(curr_char_loc+1)) == ':' && digraphs_allowed()) {
        if (*(curr_char_loc+2) == ':' &&
            *(curr_char_loc+3) != ':' && !C_mode()) {
          /* We have a construct like "%::I", which is invalid if we
             interpret "%:" as a digraph.  Issue a warning. */
          warning(ec_probable_inadvertent_sharp_digraph);
        }  /* if */
        goto check_start_of_pp_directive;
      } else if (ch == '>' && digraphs_allowed()) {
        ctoken = tok_rbrace;
        goto two_char_token;
      }  /* if */
      /* Just plain "%". */
      ctoken = tok_remainder;
      break;
    case '<':
      /* One of "<<", "<<=", "<=", or "<". In C++, "<%" or "<:".
         If exp_system_header_name is TRUE, a header name of the form
         <filename>. */
      if (exp_system_header_name) {
        ctoken = scan_header_name();
        goto end_of_token_scan;
      } else if ((ch = *(curr_char_loc+1)) == '<') {
        if (*(curr_char_loc+2) == '=') {
          ctoken = tok_shift_left_assign;
          goto three_char_token;
        } else {
          ctoken = tok_shift_left;
          goto two_char_token;
        }  /* if */
      } else if (ch == '=') {
        ctoken = tok_le;
        goto two_char_token;
      } else if (ch == '%' && digraphs_allowed()) {
        ctoken = tok_lbrace;
	goto two_char_token;
      } else if (ch == ':' && digraphs_allowed()) {
        ctoken = tok_lbracket;
        if (*(curr_char_loc+2) == ':' && *(curr_char_loc+3) != ':') {
          /* We have a construct like "<::I", which is invalid if we
             interpret "<:" as a digraph.  Issue a warning. */
          warning(ec_probable_inadvertent_lbracket_digraph);
        }  /* if */
	goto two_char_token;
      }  /* if */
      /* Just plain "<". */
      ctoken = tok_lt;
      break;
    case '>':
      /* One of ">>", ">>=", ">=", or ">". */
      if ((ch = *(curr_char_loc+1)) == '>') {
        if (*(curr_char_loc+2) == '=') {
          ctoken = tok_shift_right_assign;
          goto three_char_token;
        } else {
          ctoken = tok_shift_right;
          goto two_char_token;
        }  /* if */
      } else if (ch == '=') {
        ctoken = tok_ge;
        goto two_char_token;
      }  /* if */
      /* Just plain ">". */
      ctoken = tok_gt;
      break;
    case '=':
      /* One of "==" or "=". */
      if (*(curr_char_loc+1) == '=') {
        ctoken = tok_eq;
        goto two_char_token;
      }  /* if */
      /* Just plain "=". */
      ctoken = tok_assign;
      break;
    case '!':
      /* One of "!=" or "!". */
      if (*(curr_char_loc+1) == '=') {
        ctoken = tok_ne;
        goto two_char_token;
      }  /* if */
      /* Just plain "!". */
      ctoken = tok_not;
      break;
    case '^':
      /* One of "^=" or "^". */
      if (*(curr_char_loc+1) == '=') {
        ctoken = tok_excl_or_assign;
        goto two_char_token;
      }  /* if */
      /* Just plain "^". */
      ctoken = tok_excl_or;
      break;
    case '|':
      /* One of "||", "|=", or "|". */
      if ((ch = *(curr_char_loc+1)) == '|') {
        ctoken = tok_or_or;
        goto two_char_token;
      } else if (ch == '=') {
        ctoken = tok_or_assign;
        goto two_char_token;
      }  /* if */
      /* Just plain "|". */
      ctoken = tok_or;
      break;
    case '.':
      /* One of ".", a float constant, or "...". */
      /* In C++, ".*" is also a possibility. */
      ch = *(curr_char_loc+1);
      if (isdigit((unsigned char)ch)) {
        ctoken = scan_number();
        goto end_of_token_scan;
      } else if (ch == '.' && *(curr_char_loc+2) == '.') {
        ctoken = tok_ellipsis;
        goto three_char_token;
      } else if (ch == '*' && C_dialect == C_dialect_cplusplus) {
        ctoken = tok_period_star;
        goto two_char_token;
      }  /* if */
      /* Just plain ".". */
      ctoken = tok_period;
      break;
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      /* Integer or float constant. If exp_digit_sequence is TRUE, scan
          digit_sequence instead (used in #line directive). */
      if (!exp_digit_sequence) {
        ctoken = scan_number();
        /* Adjust the length of an integer constant in a preprocessing #if
           expression. */
        if (in_pp_if_expression &&
            ctoken == tok_int_constant) {
          adjust_pp_int_constant();
        }  /* if */
      } else {
        do {} while (isdigit((unsigned char)*(++curr_char_loc)));
        end_of_curr_token = curr_char_loc - 1;
        ctoken = tok_digit_sequence;
      }  /* if */
      goto end_of_token_scan;
    case '$':
      /* The dollar sign can optionally be accepted as an ID character.
         If it is to be accepted then we go to the code responsible for
         scanning identifiers; otherwise it is an unrecognized token. */
      if (allow_dollar_in_id_chars) {
        goto id_scan;
      } else {
        goto bad_token;
      }  /* if */
      /* This can't fall through into the next case. */
    case '\\':
      /* Either the start of a universal character name or an invalid
         token.  If the next character is "U" or "u", this is a universal
         character name. */
      ch = *(curr_char_loc+1);
      if ((ch == 'U' || ch == 'u') && universal_character_names_allowed) {
        goto id_scan;
      } else {
        goto bad_token;
      }  /* if */
      /* This can't fall through into the next case. */
    case 'L':
      /* Probably an identifier, but check for a wide character
         constant (L'x') or wide string literal (L"xyz") first. */
      if ((ch = *(curr_char_loc+1)) == '\'') {
        ctoken = scan_char_constant();
        goto end_of_token_scan;
      } else if (ch == '"') {
        remember_token_start();
        ctoken = scan_string_literal();
        goto concatenate_adjacent_string_literals;
      }  /* if */
      /* Neither of those cases, fall through into identifier processing. */
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
    case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
    case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
    case 'v': case 'w': case 'x': case 'y': case 'z':
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
    case 'H': case 'I': case 'J': case 'K': /*above*/ case 'M': case 'N':
    case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
    case 'V': case 'W': case 'X': case 'Y': case 'Z':
    case '_':
id_scan:
      /* Identifier (including keywords, macros, etc.). */
      /* See check_for_following_parenthesis in macro.c for code that
         also checks for the first character of an identifier. */
      /* Find end of identifier.  Identifiers can contain alphabetic
         characters, underscores, and digits after the first character. */
      remember_token_start();
      ctoken = tok_identifier;
      do {
        continue_scan = FALSE;
        if (allow_dollar_in_id_chars && strict_ansi_mode &&
            !dollar_in_id_diagnostic_issued) {
          /* Use a special scanning loop when we must check for dollar signs
             (which are nonstandard) while accumulating the characters of the
             identifier.  The diagnostic is issued only once. */
          register a_boolean dollar_used = (ch == '$');
          for (; is_id_char[(ch = *(curr_char_loc))-CHAR_MIN];
               curr_char_loc++) {
            if (ch == '$') dollar_used = TRUE;
          }  /* for */
          if (dollar_used) {
            diagnostic(strict_ansi_error_severity,
                       ec_dollar_used_in_identifier);
            dollar_in_id_diagnostic_issued = TRUE;
          }  /* if */
        } else {
          /* Dollar signs are not allowed, or the diagnostic has been issued
             already, so use the normal (faster) loop. */
          /* Accumulate characters of the identifier after the first. */
          while (is_id_char[(ch = *(curr_char_loc))-CHAR_MIN]) {
            curr_char_loc++;
          }  /* while */
        }  /* if */
        /* We have just scanned a sequence of "normal" identifier characters.
           Check whether we are now at a universal character name.  If so,
           scan the universal character and check for additional "normal"
           identifier characters. */
        if (*curr_char_loc == '\\') {
          ch = *(curr_char_loc + 1);
          if ((ch == 'u' || ch == 'U') &&
              universal_character_names_allowed) {
            continue_scan = TRUE;
            (void)scan_universal_character(&curr_char_loc,
			                   /*is_identifier=*/TRUE,
					   /*is_identifier_start=*/
                                            curr_char_loc==start_of_curr_token,
                                           /*issue_diagnostics=*/TRUE);
          }  /* if */
        }  /* if */
      } while (continue_scan);
      end_of_curr_token = curr_char_loc - 1;
      /* Clear the symbol locator for the current identifier.  This is done 
         even if the identifier is not looked up in the symbol table. */
      clear_locator(&locator_for_curr_id, &pos_curr_token);
      if ((fetch_pp_tokens || in_preprocessing_directive) &&
          !expand_macros && !caching_pragma_tokens) {
        /* Raw preprocessing tokens wanted, so do not look up the
           identifier. */
      } else {
        /* If variadic macros are allowed, '__VA_ARGS__' should appear only in
           the replacement list of such macros. */
        a_symbol_header_ptr	sym_hdr;
        check_use_of_VA_ARGS(
                    (sizeof_t)(end_of_curr_token - start_of_curr_token + 1),
                    start_of_curr_token);
        /* Look up the identifier in the symbol table. */
        sym_hdr = find_symbol_header(start_of_curr_token,
                                     (sizeof_t)((end_of_curr_token -
                                                     start_of_curr_token + 1)),
                                     &locator_for_curr_id);
        assoc_symbol = symbol_list_for_file_scope_symbols(sym_hdr);
        /* See if the identifier is a macro or keyword.  "Macro" should
           take precedence over "keyword", but it will naturally, since
           keywords are entered first and therefore appear at the end of the
           list. */
        while (assoc_symbol != NULL) {
          if ((id_kind = assoc_symbol->kind) == (a_symbol_kind)sk_macro &&
              !is_inert_macro) {
            /* Macro to be expanded. */
            if (expand_macros) {
              ctoken = macro_invocation(assoc_symbol, &rescan);
              /* In the usual case, we rescan the expanded form of the
                 macro. */
              if (rescan) goto rescan_token;
              /* Otherwise, macro_invocation has returned a token we can use
                 immediately.  If it is an identifier, fall into the
                 remaining processing for non-macro identifiers.
                 If it is a string literal, go look for
                 adjacent string literals with which it might be
                 concatenated.  If it is an integer constant in a
                 preprocessing #if expression, adjust its length.
                 Otherwise, just return what we have been given. */
              if (ctoken == tok_identifier) {
                /* Fall into remaining processing for non-macro identifiers. */
              } else if (ctoken == tok_string_literal) {
                goto concatenate_adjacent_string_literals;
              } else if (in_pp_if_expression &&
                         ctoken == tok_int_constant) {
                adjust_pp_int_constant();
                goto end_id_scan;
              } else {
                /* Anything else. */
                goto end_id_scan;
              }  /* if */
            }  /* if */
          } else if (id_kind == (a_symbol_kind)sk_keyword) {
            /* Keyword, return the proper token for it.  When fetching raw
               preprocessing tokens, or inside a preprocessing directive,
               the keywords mean nothing (except the keywords that correspond
               to operators).  An exception is when we are
	       processing a pragma that is explicitly designated as requiring
	       keyword recognition. */
            if (!fetch_pp_tokens &&
                (!in_preprocessing_directive ||
                 assoc_symbol->variant.keyword.is_preprocessing_op_or_punc ||
                 (caching_pragma_tokens && recognize_keywords_in_pragma))) {
              ctoken = (a_token_kind)assoc_symbol->variant.keyword.token;
              /* Check for a keyword that is not yet implemented.  If one is
                 found, issue a diagnostic and treat the keyword as an
		 identifier. */
              if (ctoken == tok_unimplemented) {
                unimplemented_keyword_diagnostic(assoc_symbol);
                ctoken = tok_identifier;
	      } else if (ctoken == tok_false || ctoken == tok_true) {
                /* A C++ boolean constant. */
		scan_boolean_constant(ctoken);
              } else {
#if MICROSOFT_EXTENSIONS_ALLOWED
                if (ctoken == tok_microsoft_asm && microsoft_mode &&
                    !scanning_microsoft_asm) {
                  /* Build a string representation of a Microsoft asm
                     and attach it to the current token. */
                  build_microsoft_asm_string();
                }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                goto end_id_scan;
              }  /* if */
            }  /* if */
          }  /* if */
          assoc_symbol = assoc_symbol->next;
        }  /* while */
        /* The identifier is not a macro and not a keyword.  If we are
           in a preprocessing #if expression, replace the identifier with
           the value 0L. */
        if (in_pp_if_expression) {
          remark(ec_undefined_preproc_id);
          ctoken = make_pp_int_constant(0L);
        }  /* if */
      }  /* if */
end_id_scan:
      /* Use "_b" exit because we may no longer be on the line on which
         the identifier appears if it is a macro name and the opening
         parenthesis of the parameter list is on a different line. */
      goto end_of_token_scan_b;
    case '\'':
      /* Character constant. */
      ctoken = scan_char_constant();
      goto end_of_token_scan;
    case '"':
      /* String literal. */
      if (exp_header_name) {
        /* If in a preprocessing directive, and exp_header_name is
           TRUE, the string should be scanned as a header name (file
           name on a #include). */
        ctoken = scan_header_name();
        goto end_of_token_scan;
      } else {
        /* Scan as a string literal, not a header name.  We could still
	   be in a preprocessing directive, though. */
        remember_token_start();
        ctoken = scan_string_literal();
        goto concatenate_adjacent_string_literals;
      }  /* if */
      /* No break needed, both branches end with a goto. */
    case '#':
check_start_of_pp_directive:
      {
        a_boolean   first_char_is_digraph;
	/* As the first token on a line, "#" opens a preprocessing directive.
	   "#" and "##" are also allowed within the body of a #define
	   (for stringizing and pasting).  */
        first_char_is_digraph = *curr_char_loc == '%';
	/* Advance past the initial "#" or "%:". */
        curr_char_loc++;
        if (first_char_is_digraph) curr_char_loc++;
#if ATT_PREPROCESSING_EXTENSIONS_ALLOWED
	/* An AT&T System V release 4 extension uses #name(tokens) in a
	   preprocessing #if to test an #assert predicate name. */
	if (in_pp_if_expression) {
	  /* Scan the #name(tokens) and create a 1 (TRUE) or 0 (FALSE) constant
	     value accordingly. */
          scan_assert_predicate_reference(&rescan);
          if (rescan) goto rescan_token;
          ctoken = tok_error;
          goto end_of_token_scan;
	} /* if */
#endif /* ATT_PREPROCESSING_EXTENSIONS_ALLOWED */
	if (in_preprocessing_directive && !caching_pragma_tokens) {
	  /* We recognize and return these preprocessing tokens even if
	     we do not know that we are in the body of a #define; this
	     helps produce reasonable error messages.  Note that this is
             done even in PCC preprocessing mode even though PCC
             preprocessors don't generally support these operators. */
	  if (*curr_char_loc == '#' && !first_char_is_digraph) {
	    ctoken = tok_paste;
	    curr_char_loc++;
          } else if (*curr_char_loc == '%' &&
		     *(curr_char_loc+1) == ':' && first_char_is_digraph) {
	    ctoken = tok_paste;
	    curr_char_loc += 2;
#if MICROSOFT_EXTENSIONS_ALLOWED
          } else if (microsoft_mode &&
                     *curr_char_loc == '@' && !first_char_is_digraph) {
            /* In Microsoft mode, a macro definition "#define M(x) #@x" causes
               "M(a)" to be expanded to 'a'. */
	    ctoken = tok_charize;
	    curr_char_loc++;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
	  } else {
	    ctoken = tok_sharp;
	  } /* if */
	} else if (!any_tokens_gotten_from_curr_source_line) {
	  /* A sharp that is the first thing on a line -- This is a
	     preprocessing directive. */
	  if (!currently_in_pp_if_skip) {
	    remember_token_start(); /* For the "#" pseudo-token. */
	    {
#if CHECKING
	      a_cached_token_ptr curr_cached_token = cached_token_rescan_list;
#endif /* CHECKING */
	      pp_directive();
	      check_assertion_str2(curr_cached_token ==
				                    cached_token_rescan_list,
				   "get_token: token cache",
				   "affected by preprocessing directive");
	    }
	    /* After the directive has been processed, go skip white space and
	       scan another token. */
	    skip_white_space();
	    goto start_of_token_scan;
	  } /* if */
	  /* Skipping because of an #if or the like, just return this
	     as a token for further checking. */
	  ctoken = tok_sharp;
	} else {
	  /* "#" outside of a preprocessing directive, and not at the
             start of a line; don't know what it means. */
	  err_code_for_error_token = ec_bad_use_of_sharp;
	  if (!fetch_pp_tokens) {
	    error_at_line_pos(err_code_for_error_token, start_of_curr_token);
	  } /* if */
	  ctoken = tok_error;
	} /* if */
      }
      /* When we reach this point, curr_char_loc should have already been
         advanced past the characters that make up this token (unlike
	 most cases in which curr_char_loc still points to the final
	 character of the token). */
      goto save_end_position;
      /* No break needed. */
    default:
    bad_token:
      /* Something else, an error. */
      err_code_for_error_token = ec_bad_token;
      if (!fetch_pp_tokens) {
        error_at_line_pos(err_code_for_error_token, start_of_curr_token);
      }  /* if */
      ctoken = tok_error;
  }  /* switch */

  /* Normal assumption on break from switch is that the current character
     is part of the token, and therefore the current position needs to be
     incremented. */
  curr_char_loc++;

save_end_position:
  /* Remember character position of end of token.  It's one before the
     current position.  In cases where that is not right (as when it is
     necessary to scan white space following the token to check something),
     one should set end_of_curr_token explicitly and goto end_of_token_scan. */
  end_of_curr_token = curr_char_loc - 1;

end_of_token_scan:
  remember_token_start();
end_of_token_scan_b:;
  /* The "_b" entry here is for cases where something beyond the end
     of the token has been scanned, and therefore we may be on a new
     line now.  Cases where this is true (perhaps because skip_white_space
     has been called) should call remember_token_start before scanning
     the initial token, and then should branch here after all other
     processing is done. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (start_of_curr_token != NULL) {
    /* Determine the source position of the end of the token. */
    if (ctoken == tok_end_of_source) {
      /* The end-of-source position must be done specially, because the
         character position lies outside the usual range, and the
         pos_curr_token position was also determined specially. */
      end_pos_curr_token = pos_curr_token;
    } else {
      macro_line_loc_to_source_pos(end_of_curr_token, end_pos_curr_token);
    }  /* if */
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
return_from_token_scan:
  if (start_of_curr_token != NULL) {
    len_of_curr_token = end_of_curr_token - start_of_curr_token + 1;
  }  /* if */
  curr_token_is_inert_macro = is_inert_macro;
#if DEBUG
  if (debug_level >= 3) {
    /* Write out the current token. */
    fprintf(f_debug, "get_token%s: pos = %lu/%2d, %-10s",
                     gotten_from_cache ? " (from cache)" : "",
                     pos_curr_token.seq, pos_curr_token.column,
                     token_names[(int)ctoken]);
    if (start_of_curr_token != NULL) {
      /* Print token string if valid. */
      fprintf(f_debug, ", \"%.*s\"", (int)len_of_curr_token,
                                     start_of_curr_token);
      if (debug_level >= 5) {
        /* Print the length of the token. */
        fprintf(f_debug, " (%d bytes)", (int)len_of_curr_token);
      }  /* if */
    }  /* if */
    if (curr_token_is_inert_macro) {
      fprintf(f_debug, " (inert)");
    }  /* if */
    /* Dump constants only if they have been converted. */
    if (!fetch_pp_tokens && is_literal_constant_token(ctoken)) {
      /* Dump value for constant. */
      fprintf(f_debug, ", ");
      db_constant(&const_for_curr_token);
    }  /* if */
    (void)fputc('\n', f_debug);
  }  /* if */
#endif /* DEBUG */
  if (!in_preprocessing_directive) {
    any_tokens_fetched_from_curr_input_file = TRUE;
  }  /* if */
  return (curr_token = ctoken);

two_char_token:
  /* For two-character tokens, increment the source position appropriately
     and then rejoin the main code. */
  curr_char_loc += 2;
  goto save_end_position;

three_char_token:
  /* For three-character tokens, increment the source position appropriately
     and then rejoin the main code. */
  curr_char_loc += 3;
  goto save_end_position;

concatenate_adjacent_string_literals:
  /* Come here after scanning a string literal or wide string literal.
     If appropriate, string literals following the current one will be
     concatenated with it.  See ANSI C 2.1.1.2, translation phase 6. */
  if (fetch_pp_tokens ||
      (in_preprocessing_directive && !caching_pragma_tokens) ||
      !do_string_literal_concatenation) {
    /* String literal concatenation should not be done in the current mode. */
    goto end_of_token_scan_b;
  }  /* if */
  /* Do string literal concatenation. */
  check_assertion_str(ctoken == tok_string_literal,
                      "get_token: concatenating string literal, bad token");
  concat_adjacent_string_literals();
  goto return_from_token_scan;
}  /* get_token */


static a_stop_token_stack_entry_ptr alloc_stop_token_stack_entry(void)
/*
Allocate a new stop token stack entry, initialize it, and return a pointer
to it.
*/
{
  a_stop_token_stack_entry_ptr	stsep;

  if (avail_stop_token_stack_entries != NULL) {
    /* Reuse an existing entry. */
    stsep = avail_stop_token_stack_entries;
    avail_stop_token_stack_entries = avail_stop_token_stack_entries->next;
  } else {
    /* Allocate a new entry. */
    stsep = (a_stop_token_stack_entry_ptr)
                                   alloc_fe(sizeof(a_stop_token_stack_entry));
#if DEBUG
   num_stop_token_stack_entries_allocated++;
#endif /* DEBUG */
  }  /* if */
  stsep->next = NULL;
  memzero((char*)stsep->stop_tokens, sizeof(a_token_set_array));
  return stsep;
}  /* alloc_stop_token_stack_entry */


void push_stop_token_stack(void)
/*
Push a new entry on the stop token entry stack.  This is used when a new
lexical context is being processed.  A pointer to the previous entry
is saved for use when the stack is popped.
*/
{
  a_stop_token_stack_entry_ptr	stsep;

  stsep = alloc_stop_token_stack_entry();
  stsep->next = curr_stop_token_stack_entry;
  curr_stop_token_stack_entry = stsep;
}  /* push_stop_token_stack */

#if DEBUG

void db_stop_tokens(void)
/*
Display the current stop token array.
*/
{
  int				token;
  a_token_set_array_element	*stop_tokens;

  stop_tokens = curr_stop_token_stack_entry->stop_tokens;
  for (token = 0; token != (int)tok_last; token++) {
    if (stop_tokens[token] != 0) {
      fprintf(f_debug, "stop_tokens[\"%s\"] = %d\n", 
              token_names[token], stop_tokens[token]);
    }  /* if */
  }  /* for */
}  /* db_stop_tokens */

#endif /* DEBUG */


#if CHECKING
void check_all_stop_token_entries_are_reset(a_token_set_array stop_tokens)
/*
Check that the stop_token_array elements all made it back to zero.
(Every add_stop_token is supposed to have a corresponding remove_stop_token.)
*/
{
  int       token;
  a_boolean any_error = FALSE;

  for (token = 0; token != (int)tok_last; token++) {
    if (stop_tokens[token] != 0) {
      any_error = TRUE;
#if DEBUG
      if (debug_level != 0) {
        fprintf(f_debug, "stop_tokens[\"%s\"] != 0\n", 
                token_names[token]);
      }  /* if */
#endif /* DEBUG */
    }  /* if */
  }  /* for */
  check_assertion_str2(!any_error, "check_all_stop_token_entries_are_reset:",
                       "stop_token_array not all zero");
}  /* check_all_stop_token_entries_are_reset */
#endif /* CHECKING */


void pop_stop_token_stack(void)
/*
Pop the current entry off of the stop token stack.
*/
{
  a_stop_token_stack_entry_ptr	stsep;

  stsep = curr_stop_token_stack_entry;
#if CHECKING
  /* If we are doing expensive checking, or if the debug flag for
     "check_stop_tokens" is set, then call the routine to make sure all
     of the stop tokens have been reset. */
  { a_boolean	check_stop_tokens = FALSE;
#if DEBUG
    if (db_flag_is_set("check_stop_tokens")) check_stop_tokens = TRUE;
#endif /* DEBUG */
#if EXPENSIVE_CHECKING
    check_stop_tokens = TRUE;
#endif /* EXPENSIVE_CHECKING */
    if (check_stop_tokens) { /*lint !e774*/
      /* Make sure that all of the array elements of the entry being popped
         have been reset to their initial value of zero. */
      check_all_stop_token_entries_are_reset(stsep->stop_tokens);
    }  /* if */
  }
#endif /* CHECKING */
  /* Unlink this entry from the stack. */
  curr_stop_token_stack_entry = stsep->next;
  /* Add the old entry to the list of available stack entries. */
  stsep->next = avail_stop_token_stack_entries;
  avail_stop_token_stack_entries = stsep;
  check_assertion_str(curr_stop_token_stack_entry != NULL,
                      "pop_stop_token_stack: too many pops");
}  /* pop_stop_token_stack */


void flush_until_matching_token(void)
/*
The current token is the opening token of a pair of matched tokens (e.g.,
an opening parenthesis).  Flush to the corresponding closing token.
*/
{
  a_token_kind      closing_token;
  a_token_kind	    prev_token = tok_error;
  a_source_position start_pos;
  unsigned long     paren_count   = 0,
		    bracket_count = 0,
		    brace_count   = 0;
  unsigned long     max_lines;
  a_boolean         done = FALSE;

  db_enter(3, "flush_until_matching_token");
  /* Save the current position, to see later how much we have flushed. */
  copy_source_position(pos_curr_token, start_pos);

  /* Determine the associated closing token and the maximum number of lines
     to throw away. */
  max_lines = 2;
  switch (curr_token) {
    case tok_lparen:    closing_token = tok_rparen;   break;
    case tok_lbracket:  closing_token = tok_rbracket; break;
    case tok_lbrace:    closing_token = tok_rbrace; max_lines = 20; break;
    case tok_lt:        closing_token = tok_gt;       break;
#if CHECKING
    default:
      internal_error("flush_until_matching_token: bad opening token");
#endif /* CHECKING */
  }  /* switch */
  (void)get_token();

  while (!done && (curr_token != closing_token ||
         paren_count != 0 || bracket_count != 0 || brace_count != 0)) {
    /* Count paired tokens within the skip. */
    switch (curr_token) {
      case tok_lparen:                           paren_count++;   break;
      case tok_rparen:    if (paren_count > 0)   paren_count--;   break;
      case tok_lbracket:                         bracket_count++; break;
      case tok_rbracket:  if (bracket_count > 0) bracket_count--; break;
      case tok_lbrace:                           brace_count++;   break;
      case tok_rbrace:    if (brace_count > 0)   brace_count--;   break;
      case tok_gt:
        /* A ">" is only meaningful if when it is the token we are looking
           for. */
        if (closing_token == tok_gt) {
          /* A ">" only counts as the end of the parameter list if we are not
             inside some other construct.  For example when scanning
             "A<(1>2)>" the first ">" doesn't count. */
          if (paren_count == 0 && bracket_count == 0 && brace_count == 0) {
            done = TRUE;
          }  /* if */
        }  /* if */
        break;
      default:;
    }  /* switch */
    /* If we've skipped too many lines, give up the flush. */
    if ((pos_curr_token.seq - start_pos.seq) > max_lines) break;
    /* Check for the start of a template parameter list. */
    if (curr_token == tok_lt && prev_token == tok_identifier) {
      if (is_template_reference()) {
        flush_until_matching_token();
      }  /* if */
    }  /* if */
    /* Always stop the flush on:
       1)  End of source;
       2)  A newline, if in a preprocessing directive. */
    if (curr_token == tok_end_of_source ||
        (in_preprocessing_directive && curr_token == tok_newline)) break;
    /* None of the conditions was satisfied, so keep flushing tokens. */
    prev_token = curr_token;
    (void)get_token();
  }  /* while */

  db_exit();
}  /* flush_until_matching_token */


void flush_tokens_with_stop_tokens_and_warning_flag(
				a_token_set_array	stop_tokens,
				a_boolean		suppress_warning)
/*
Get and throw away tokens until a token is read that is in the set
of stop tokens specified by stop_tokens.  This routine is usually called
to recover from syntax errors.  A warning is sometimes issues depending
on the number of tokens skipped.  Suppress any diagnostics when
suppress_warning is TRUE.  This is used when this routine is called
to skip tokens for some purpose other than error recovery.
*/
{
  a_source_position start_pos;
  a_token_kind      prev_token = tok_error;

  db_enter(3, "flush_tokens_with_stop_tokens");
  /* Save the current position, to see later how much we have flushed. */
  copy_source_position(pos_curr_token, start_pos);

  /* Flush tokens until something in the stop tokens set turns up.
     Stop flushing if the end of file is reached.
     While flushing, note parentheses, etc., and flush to matching tokens. */
  /* Stop the flush on finding a token in the stop token set. */
  while (stop_tokens[(int)curr_token] == 0) {
    /* On paired tokens, skip to the corresponding closing token. */
    if (curr_token == tok_lparen || curr_token == tok_lbracket ||
        curr_token == tok_lbrace ||
        (curr_token == tok_lt &&
         ((prev_token == tok_identifier && is_template_reference()) ||
          prev_token == tok_template))) {
      flush_until_matching_token();
    }  /* if */
    /* Always stop the flush on:
       1)  End of source;
       2)  A newline, e.g., in a preprocessing directive. */
    if (curr_token == tok_end_of_source || curr_token == tok_newline) break;
    /* None of the conditions was satisfied, so keep flushing tokens. */
    prev_token = curr_token;
    (void)get_token();
  }  /* while */
  set_err_pos_to_curr_token();
  /* If the flushing threw away more than just a little bit, put out
     a diagnostic to tell the user where the parsing recovered. */
  if (!suppress_warning &&
      pos_curr_token.seq - start_pos.seq > 2) {
    warning(ec_end_of_flush);
  }  /* if */
  db_exit();
}  /* flush_tokens_with_stop_tokens_and_warning_flag */


static void flush_tokens_with_stop_tokens(a_token_set_array	stop_tokens)
/*
Interface to flush_tokens_with_stop_tokens_and_warning_flag that
indicates that warnings should be issued when appropriate.
*/
{
  flush_tokens_with_stop_tokens_and_warning_flag(stop_tokens,
                                                 /*suppress_warning=*/FALSE);
}  /* flush_tokens_with_stop_tokens */


void flush_tokens(void)
/*
Get and throw away tokens until a token is read that is in the set
of stop tokens.  This routine is called to recover from syntax errors.
This routine calls flush_tokens_with_stop_tokens, passing in the global
stop_token_array.
*/
{
  flush_tokens_with_stop_tokens(curr_stop_token_stack_entry->stop_tokens);
}  /* flush_tokens */


a_boolean required_token(a_token_kind  token,
			 an_error_code error_code)
/*
The current token is required to be "token".  If it is, advance normally
by calling get_token.  If not, issue the error message and call flush_tokens.
In either case, return TRUE if the required token showed up.
*/
{
  a_boolean token_present;

  db_enter(5, "required_token");

  if (curr_token == token) {
    (void)get_token();
    token_present = TRUE;
  } else {
    /* Token not present.  Report an error, flush to it or something
       else in the stop tokens set. */
    add_stop_token(token);
    set_err_pos_to_curr_token();
    syntax_error(error_code);
    remove_stop_token(token);
    /* If the token did show up, take it now. */
    token_present = (curr_token == token);
    if (token_present) (void)get_token();
  }  /* if */

  db_exit();

  return token_present;
}  /* required_token */


a_boolean required_token_no_advance(a_token_kind  token,
                                    an_error_code error_code)
/*
The current token is required to be "token".  If it is not, issue the error
message and call flush_tokens.  Do not advance past the token if it is
found, but do return TRUE.
*/
{
  a_boolean token_present;

  db_enter(5, "required_token_no_advance");

  if (curr_token == token) {
    token_present = TRUE;
  } else {
    /* Token not present.  Report an error, flush to it or something
       else in the stop tokens set. */
    add_stop_token(token);
    set_err_pos_to_curr_token();
    syntax_error(error_code);
    remove_stop_token(token);
    /* If the token did show up, take it now. */
    token_present = (curr_token == token);
  }  /* if */

  db_exit();

  return token_present;
}  /* required_token */


a_boolean loop_token(a_token_kind token)
/*
If the current token is the indicated token, then take that token (by
calling get_token), and return TRUE; otherwise, do not call get_token,
and return FALSE.  This function is used for a bottom-of-loop test for
repeated constructs, as in

   do {
     ... Scan the construct to be repeated. ...
   } while (loop_token(tok_comma));

*/
{
  a_boolean return_value;

  if (curr_token == token) {
    (void)get_token();
    return_value = TRUE;
  } else {
    return_value = FALSE;
    set_err_pos_to_curr_token();
  }  /* if */
  return(return_value);
}  /* loop_token */


a_token_kind next_token_with_seq_number(a_token_sequence_number *seq)
/*
Return the next token after the current one while leaving the current
token unchanged.  This is used in recursive-descent parsing routines
to peek ahead at the next token and decide on a path through the syntax.
If seq is not NULL, return the token sequence number of the next token
in the location pointed to by seq.
*/
{
  a_token_cache 	cache;
  a_token_kind 		ntoken;
  a_cached_token_ptr	ctp = NULL;

  db_enter(5, "next_token_with_seq_number");
  if (in_preprocessing_directive && curr_token == tok_newline) {
    /* If we have reached the end of a preprocessing directive, don't attempt
       to scan tokens past the end.  Return a tok_newline without actually
       looking at the next token. */
    ntoken = tok_newline;
    /* If seq is not NULL, return the sequence number the current token
       (since we can't get the number of the next token. */
    if (seq != NULL) *seq = curr_token_sequence_number;
    goto done;
  }  /* if */
  /* If we are currently rescanning tokens from a cache then we should
     just be able to fetch the token kind from the next token on the
     list to be rescanned.  This code does not handle some of the more complex
     cases such as when we have to scan over the end of a reusable cache.
     In those cases we use the more general (and slower) method to fetch
     the next token. */
  if (cached_token_rescan_list != NULL) {
    /* There are tokens on the non-reusable rescan list. */
    ctp = cached_token_rescan_list;
  } else if (reusable_cache_stack != NULL) {
    /* There are tokens on the reusable rescan list. */
    ctp = reusable_cache_stack->next_cached_token;
  }  /* if */
  /* Get the next token that is not a pragma state entry. */
  while (ctp != NULL &&
         ctp->extra_info_kind ==
                             (a_token_extra_info_kind)teik_pragma) {
    ctp = ctp->next;
  }  /* for */
  /* If there is no cached token or if the token is the end-of-source token
     which is used to terminate the token cache, then disregard this token
     and fetch the next token using the slower method. */
  if (ctp != NULL && ctp->token != (a_byte_token_kind)tok_end_of_source) {
    /* There is a cached token from which we can get then token kind. */
    ntoken = (a_token_kind)ctp->token;
    /* If seq is not NULL, return the sequence number of the next token. */
    if (seq != NULL) *seq = ctp->token_sequence_number;
  } else {
    /* Put the current token into a token cache so it can be rescanned. */
    clear_token_cache(&cache, /*reusable=*/FALSE);
    cache_curr_token(&cache);
    /* Fetch the next token and remember its kind. */
    ntoken = get_token();
    /* If seq is not NULL, return the sequence number of the next token. */
    if (seq != NULL) *seq = curr_token_sequence_number;
    /* Put the two tokens in the cache (original, next) on the rescan list,
       and refetch the original token.  Note that the "next" token remains on
       the rescan list. */
    rescan_cached_tokens(&cache);
  }  /* if */
done:
  db_exit();
  return ntoken;
}  /* next_token_with_seq_number */


a_token_kind next_two_tokens(a_token_kind	first_token_must_be,
                             a_token_kind	*token_2)

/*
Return the next two tokens after the current one while leaving the current
token unchanged.  The second token is only fetched if the first token matches
the value passed by the caller, otherwise the second token is tok_error.
The next token is the return value of the function, the second token is
returned in the argument "token_2".  This is like next_token except it
fetches the next two tokens instead of only one.  This routine
cannot be used when fetching raw preprocessing tokens.
*/
{
  a_token_cache 	cache;
  a_token_kind		ntoken;
  a_cached_token_ptr	ctp = NULL;
  a_boolean		tokens_found = FALSE;

  db_enter(3, "next_two_tokens");
  if (in_preprocessing_directive && curr_token == tok_newline) {
    /* If we have reached the end of a preprocessing directive, don't attempt
       to scan tokens past the end.  Return a tok_newline without actually
       looking at the next token. */
    ntoken = tok_newline;
    *token_2 = tok_error;
    goto done;
  }  /* if */
  /* If we are currently rescanning tokens from a cache then we should
     just be able to fetch the token kind from the next token on the
     list to be rescanned.  This code does not handle some of the more complex
     cases such as when we have to scan over the end of a reusable cache.
     In these cases the more general (and slower) method is used
     to fetch the next token. */
  if (cached_token_rescan_list != NULL) {
    /* There are tokens on the non-reusable rescan list. */
    ctp = cached_token_rescan_list;
  } else if (reusable_cache_stack != NULL) {
    /* There are tokens on the reusable rescan list. */
    ctp = reusable_cache_stack->next_cached_token;
  }  /* if */
  /* Get the next token that is not a pragma entry. */
  while (ctp != NULL &&
         ctp->extra_info_kind ==
                             (a_token_extra_info_kind)teik_pragma) {
    ctp = ctp->next;
  }  /* while */
  /* If there is no cached token or if the token is the end-of-source token
     which is used to terminate the token cache, then disregard this token
     and fetch the next token using the slower method. */
  if (ctp != NULL && ctp->token != (a_byte_token_kind)tok_end_of_source) {
    /* There is a cached token from which we can get then token kind. */
    ntoken = (a_token_kind)ctp->token;
    if (ntoken != first_token_must_be) {
      /* The first token indicates that the second token is not needed. */
      *token_2 = tok_error;
      tokens_found = TRUE;
    } else {
      ctp = ctp->next;
      /* Get the next token that is not a pragma entry. */
      while (ctp != NULL &&
             ctp->extra_info_kind ==
                                 (a_token_extra_info_kind)teik_pragma) {
        ctp = ctp->next;
      }  /* while */
      if (ctp != NULL && ctp->token != (a_byte_token_kind)tok_end_of_source) {
        /* We have found the next token in the cache that can be used to
           return the value of token_2.  Return the value and set tokens_found
           to indicate that no further processing is needed. */
        *token_2 = (a_token_kind)ctp->token;
        tokens_found = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  /* The next token information could not be determined just by looking at
     the token cache.  Scan forward by getting and caching the necessary
     tokens. */
  if (!tokens_found) {
    /* Put the current token into a token cache so it can be rescanned. */
    clear_token_cache(&cache, /*reusable=*/FALSE);
    cache_curr_token(&cache);
    /* Fetch the token or possibly the two next tokens and remember their
       kinds.  If the first token doesn't match the value specified by the
       caller the caller should not use the value in *token_2. */
    ntoken = get_token();
    if (ntoken == first_token_must_be) {
      cache_curr_token(&cache);
      *token_2 = get_token();
    } else {
      *token_2 = tok_error;
    }  /* if */
    /* Put the two or three tokens in the cache (original, next 1, and possibly
       next 2) on the rescan list, and refetch the original token.  Note
       that the "next" token remains on the rescan list. */
    rescan_cached_tokens(&cache);
  }  /* if */
done:
  db_exit();
  return ntoken;
}  /* next_two_tokens */


void unget_token(void)
/*
"Unget" the current token, i.e., put it back on the input list so that
it will be fetched again on the next get_token.  On return, the current
token is still the same.  This is intended for use with unusual errors,
so efficiency is not a prime concern.
*/
{
  a_token_cache cache;

  db_enter(3, "unget_token");
  /* Create a token cache containing a copy of the current token. */
  clear_token_cache(&cache, /*reusable=*/FALSE);
  cache_curr_token(&cache);
  /* Push the cache.  This pushes two copies of the original token,
     and fetches one of them as the current token. */
  rescan_cached_tokens(&cache);
  db_exit();
}  /* unget_token */


static a_symbol_ptr name_matches_base_class(a_type_ptr tp)
/*
Determine if the name in the locator matches the name of a base class
of tp.  If so, return a pointer to the symbol associated with the matching
base class.
*/
{
  a_base_class_ptr	bcp;
  a_symbol_ptr		result_sym = NULL;

  bcp = base_classes_of(tp);
  for (; bcp != NULL; bcp = bcp->next) {
    /* Get the symbol pointer associated with the base class. */
    a_symbol_ptr	sym;
    a_type_ptr		base_type = bcp->type;
    sym = (a_symbol_ptr)base_type->source_corresp.assoc_info;
    if (sym->header == locator_for_curr_id.symbol_header) {
      result_sym = sym;
      break;
    }  /* if */
  }  /* for */
  return result_sym;
}  /* name_matches_base_class */


static
a_symbol_ptr look_up_destructor_name(
		a_symbol_locator		*locator,
		a_boolean			is_file_scope_qualified_name,
		a_symbol_ptr			qualifier_sym,
                a_boolean			no_normal_lookup,
                an_identifier_options_set	options)
/*
Look up the name of a destructor.  The locator provides the name of
the destructor (the thing following the "~".  qualifier_sym points to
a symbol that represent the qualifier that preceded the destructor
name, if any.  is_file_scope_qualified_name is true if a leading :: was
present.
*/
{
  a_symbol_ptr	type_sym = NULL;
  clear_specific_symbol(*locator);
  if (is_file_scope_qualified_name) {
    type_sym = file_scope_id_lookup(il_header.primary_scope, locator, options);
  } else if (qualifier_sym != NULL && qualifier_sym->is_class_member) {
    type_sym = class_qualified_id_lookup(locator,
                                         qualifier_sym->parent.class_type,
                                         options);
  } else if (qualifier_sym != NULL && !qualifier_sym->is_class_member &&
             qualifier_sym->parent.namespace_ptr != NULL) {
    type_sym = namespace_qualified_id_lookup
                                         (locator,
                                          qualifier_sym->parent.namespace_ptr,
                                          options);
  } else if (!no_normal_lookup) {
    type_sym = normal_id_lookup(locator, options);
  }  /* if */
  return type_sym;
}  /* look_up_destructor_name */


static a_boolean acceptable_dtor_type(a_type_ptr	field_sel_type,
				      a_type_ptr	dtor_type)
/*
Determine whether dtor_type is an acceptable type to be used in an
explicit destructor call for an object of field_sel_type.

Normally the types must be identical, but if field_sel_type is a proxy
class, we should accept any type.
*/
{
  a_class_symbol_supplement_ptr	cssp;
  a_boolean			result = FALSE;

  check_assertion(is_immediate_class_type(field_sel_type));
  cssp = symbol_supplement_for_class(field_sel_type);
  if (cssp->template_param_for_proxy_class != NULL) {
    result = TRUE;
  } else {
    result = identical_types(field_sel_type, dtor_type);
  }  /* if */
  return result;
}  /* acceptable_dtor_type */


static void get_destructor_name(a_type_ptr	field_sel_type,
				a_boolean	is_file_scope_qualified_name,
				a_symbol_ptr	qualifier_sym)
/*
The current token is the "~" at the start of a destructor name.  Scan the
name and build a locator for the destructor name in locator_for_curr_id.

A destructor declaration can only use the true name of the class.  A
destructor reference (in a field selection operation), on the other
hand, can use either the true name of the class or can use a typedef
that refers to the class.  Because of this, the lookup of the name
that follows the "~" in a destructor reference is quite complicated.

If the destructor reference is not a qualified name the lookup is done
as a normal lookup and a lookup in the class of the right operand of
the field selection.

If the reference is a qualified name the lookup is done as a normal
lookup, and a lookup in the scope that contains the class specified in
the qualifier.  For example:

	::A::~B		B is looked up in the file scope
	X::A::~B	B is looked up in X

In addition to these lookups, the name of the destructor is compared with
the name of the class of the right operand of the field selection.

If any of these lookups result in a class that matches the class of the
field selection operation, that result is used as the destructor.
Otherwise, each of the lookup results is checked to see if it matches
a base class of the field selection.  If exactly one of the lookups
matches a base class, that result is used.  If more than one matches, the
lookup is ambiguous.  If no match is found, the original name is converted
to a destructor name (e.g., "X" is changed to "~X") and a lookup error
will be diagnosed by the caller.

The caller is responsible for ensuring that the current token is "~" before
calling this routine.  This routine is called only in C++ mode.

field_sel_type is NULL except when scanning the right operand of a field
selection operator, in which case it points to the type of the left operand.
qualifier_sym points to a symbol that describes the qualifier when
the destructor is part of a qualified name (e.g., "A::B::~B").
*/
{
  /* Skip past the "~", check for an identifier. */
  (void)get_token();
  if (!is_generalized_identifier_start(GID_DISALLOW_QUALIFIED_NAME |
				       GID_DISALLOW_OPERATOR_NAME)) {
    /* syntax_error is deliberately not called. */
    error(ec_exp_identifier);
    /* Put back the current token and make a fake error identifier. */
    unget_token();
    curr_token = tok_identifier;
    make_specific_symbol_error_locator(&locator_for_curr_id);
  } else {
    /* "~identifier" is present. */
    if (field_sel_type == NULL ||
        !is_class_struct_union_type(field_sel_type)) {
      /* Either no field type was provided, or the type provided is not a
         class type.  Don't do the special lookup processing in this case. */
    } else {
      a_symbol_ptr	field_sym;
      a_symbol_ptr	type_sym = NULL;
      a_symbol_locator	normal_locator;
      a_symbol_locator	other_locator;
      a_symbol_ptr	normal_sym = NULL;
      a_symbol_ptr	other_sym = NULL;
      a_symbol_ptr	base_sym = NULL;
      a_symbol_ptr	ambiguous_sym = NULL;
      a_boolean		destructor_okay = FALSE;
      a_type_ptr	normal_tp = NULL;
      a_type_ptr	other_tp = NULL;
      a_type_ptr	tp;
      a_boolean		ambiguous = FALSE;
      a_boolean		error_already_issued = FALSE;

      field_sel_type = skip_typerefs(field_sel_type);
      field_sym = (a_symbol_ptr)field_sel_type->source_corresp.assoc_info;
      check_assertion_str2(field_sym != NULL, "get_destructor_name:",
                           "NULL assoc_info");
      if (qualifier_sym != NULL && is_type_symbol(qualifier_sym)) {
        /* If the destructor name was specified with a qualified name,
           make sure the class specified by the qualifier names the
           field selection class or a base class thereof. */
        a_type_ptr	qualifier_type;
        qualifier_type = type_symbol_type(qualifier_sym);
        if (!acceptable_dtor_type(field_sel_type, qualifier_type) &&
            (is_template_param_type(qualifier_type) ||
             find_base_class_of(field_sel_type, qualifier_type) == NULL)) {
          pos_ty2_error(ec_destructor_qualifier_type_mismatch,
                        &locator_for_curr_id.source_position,
                        qualifier_type,
                        field_sel_type);
          set_to_error_locator(locator_for_curr_id);
          error_already_issued = TRUE;
        }  /* if */
      }  /* if */
      if (error_already_issued) {
        /* Skip this section if an error was already issued. */
      } else if (field_sym->header == locator_for_curr_id.symbol_header) {
        /* The destructor name matches the class name -- this is a normal
           destructor reference. */
        destructor_okay = TRUE;
      } else {
        clear_specific_symbol(locator_for_curr_id);
        /* Do a normal lookup.  If this produces a class symbol, see if the
           class matches the field selection class or one of its base classes.
           If it matches the class, the lookup is done.  If it matches a
           base, save this result for later.  Make a copy of the locator and
           use that for the lookup.  A copy is made to preserve other
           flags set by the lookup (such as the semivisible nested class
           flag). */
        normal_locator = locator_for_curr_id;
        normal_sym = normal_id_lookup(&normal_locator, IDL_MUST_BE_CLASS);
        if (normal_sym != NULL &&
            (is_class_symbol(normal_sym) ||
             is_template_param_type_symbol(normal_sym))) {
          normal_tp = type_symbol_type(normal_sym);
          normal_tp = skip_typerefs(normal_tp);
          if (acceptable_dtor_type(field_sel_type, normal_tp)) {
            type_sym = normal_sym;
            destructor_okay = TRUE;
            locator_for_curr_id = normal_locator;
          } else {
            if (is_template_param_type(normal_tp) ||
                find_base_class_of(field_sel_type, normal_tp) == NULL) {
              normal_sym = NULL;
            }  /* if */
          }  /* if */
        }  /* if */
        if (!destructor_okay) {
          /* Look up the destructor name based on the qualifier that was
             present (if any).  If no qualifier was present, look up the
             destructor in the field selection class. */
          other_locator = locator_for_curr_id;
          if (qualifier_sym != NULL || is_file_scope_qualified_name) {
            other_sym = look_up_destructor_name(&other_locator,
                                                is_file_scope_qualified_name,
                                                qualifier_sym,
                                                /*no_normal_lookup=*/TRUE,
                                                IDL_MUST_BE_CLASS);
          } else {
            other_sym = class_qualified_id_lookup(&other_locator,
                                                  field_sel_type,
                                                  IDL_MUST_BE_CLASS);
          }  /* if */
          if (other_sym != NULL && is_class_symbol(other_sym)) {
            other_tp = skip_typerefs(other_sym->variant.type.ptr);
            if (acceptable_dtor_type(field_sel_type, other_tp)) {
              type_sym = other_sym;
              destructor_okay = TRUE;
              locator_for_curr_id = other_locator;
            } else {
              if (find_base_class_of(field_sel_type, other_tp) == NULL) {
                other_sym = NULL;
              }  /* if */
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
      if (!destructor_okay && !error_already_issued) {
        /* None of the lookups match the field selection class.  Determine
           whether any of them match a base class.  If either of the
           previous lookups do match a base class, the symbol will still be
           set to the lookup result.  Otherwise, the symbol (normal_sym and/or
           other_sym) will have been set to NULL.  The "tp" pointer is used
           to point to the type of the destructor found.  This is used to
           determine whether two of the symbols actually point to the same
           type.  The ambiguous flag is set if two or more of the symbols found
           point to different types. */
        tp = NULL;
        base_sym = name_matches_base_class(field_sel_type);
        if (base_sym != NULL) tp = type_symbol_type(base_sym);
        if (base_sym != NULL) type_sym = base_sym;
        /* Use the normal symbol if the base name lookup failed. */
        if (normal_sym != NULL && normal_tp != tp) {
          if (type_sym != NULL) {
            ambiguous = TRUE;
            if (ambiguous_sym == NULL) ambiguous_sym = normal_sym;
          }  /* if */
          if (!ambiguous) {
            type_sym = normal_sym;
            tp = normal_tp;
            locator_for_curr_id = normal_locator;
          }  /* if */
        }  /* if */
        /* Use the "other" lookup symbol if both the base name lookup and
           normal lookups produced no result. */
        if (other_sym != NULL && other_tp != tp) {
          if (type_sym != NULL) {
            ambiguous = TRUE;
            if (ambiguous_sym == NULL) ambiguous_sym = other_sym;
          }  /* if */
          if (!ambiguous) {
            type_sym = other_sym;
            locator_for_curr_id = other_locator;
          }  /* if */
        }  /* if */
        if (type_sym != NULL) destructor_okay = TRUE;
      }  /* if */
      if (error_already_issued) {
        /* Skip this section if an error was already issued. */
      } else if (!destructor_okay) {
        /* No match was found -- issue an error. */
        pos_ty_error(ec_invalid_destructor_name,
                     &locator_for_curr_id.source_position,
                     field_sel_type);
        set_to_error_locator(locator_for_curr_id);
      } else if (ambiguous) {
        /* The destructor reference is ambiguous.  Although it is possible for
           three lookup results to be produced, only two are included in the
           diagnostic. */
        pos_sy2_error(ec_ambiguous_destructor,
                      &locator_for_curr_id.source_position,
                      type_sym, ambiguous_sym);
        set_to_error_locator(locator_for_curr_id);
      } else if (type_sym == NULL) {
        /* We are using the original destructor name for which we don't
           need to construct a new locator. */
      } else {
        /* The lookup produced a unique symbol. */
        a_source_position	saved_position;
        if (locator_for_curr_id.is_semivisible_nested_type) {
          /* The symbol in the locator is a nested class that is not visible
             according to the ARM lookup rules but is returned in support of
             the nested class anachronism (ARM 18.3.5). Issue an anachronism
             diagnostic. */
          sym_diagnostic(anachronism_error_severity,
                         ec_nested_class_anachronism,
                         locator_for_curr_id.specific_symbol);
        }  /* if */
        /* Create a locator that points to the type described by the symbol
           that was found. */
        saved_position = locator_for_curr_id.source_position;
        tp = type_symbol_type(type_sym);
        tp = skip_typerefs(tp);
        type_sym = (a_symbol_ptr)tp->source_corresp.assoc_info;
        make_locator_for_symbol(type_sym, &locator_for_curr_id);
        locator_for_curr_id.source_position = saved_position;
      }  /* if */
    }  /* if */
    /* Convert the locator to a locator for the destructor. */
    if (!is_error_locator(locator_for_curr_id)) {
      tildize_locator(&locator_for_curr_id);
    }  /* if */
  }  /* if */
}  /* get_destructor_name */


static void get_opname(a_boolean                   	is_class_member,
                       a_parent_class_or_namespace_ptr	parent)
/*
The current token is the token "operator" at the start of an operator name,
like "operator+".  Scan the name and build a locator for the operator name
in locator_for_curr_id.  parent is a pointer to the class type or the
namespace of the qualifier associated with the generalized identifier
being scanned.  is_class_member is TRUE if the parent points
to a class, it is FALSE if parent points to a namespace or if there
is no parent.  If the parent pointer is not NULL then push a class or
namespace reactivation scope before scanning the type name in a type
conversion operator.

This routine is called only in C++ mode.
*/
{
  a_source_position start_position;
  a_token_kind      token, second_token;
  an_opname_kind    opname;

  start_position = pos_curr_token;
  /* Skip past the "operator", check for an operator. */
  token = get_token();
  if (scan_conversion_operator(&start_position, is_class_member, parent)) {
    /* This is a conversion operator function -- "operator" followed by
       a type name. */
  } else {
    /* It must be an overloaded operator name (or an error). */
    opname = opname_kind_for_token[(int)token];
    if (opname == (an_opname_kind)onk_function_call ||
        opname == (an_opname_kind)onk_subscript) {
      /* Two-token operators: () and [].  Peek ahead to the next token; if
         it's the right one, swallow it and leave the opname kind as is.
         Otherwise change the opname kind to onk_none so that an error will
         be issued. */
      if (next_token() == ((opname == (an_opname_kind)onk_function_call) ?
                                                tok_rparen : tok_rbracket)) {
        /* Advance to the second token. */
        (void)get_token();
      } else {
        /* Error case. */
        opname = (an_opname_kind)onk_none;
      }  /* if */
    } else if (opname == (an_opname_kind)onk_new ||
               opname == (an_opname_kind)onk_delete) {
      /* See if this is really new[] or delete[].  If so, adjust the opname. */
      if (next_two_tokens(tok_lbracket, &second_token) &&
          second_token == tok_rbracket) {
        if (!array_new_and_delete_enabled) {
          /* Issue an error if support for array new/delete is not enabled,
             but continue parsing as though it were. */
          error(ec_no_array_new_and_delete_support);
        }  /* if */
        /* Advance past the two tokens. */
        (void)get_token();
        (void)get_token();
        opname = (opname == (an_opname_kind)onk_new) ?
                    (an_opname_kind)onk_array_new :
                    (an_opname_kind)onk_array_delete;
      }  /* if */
    }  /* if */
    if (opname == (an_opname_kind)onk_none ||
        opname == (an_opname_kind)onk_question) {
      /* Note that onk_question is not treated as an operator -- '?' is
         included in the opname kind table as a convenience in expression
         processing only. */
      /* syntax_error is deliberately not called. */
      error(ec_exp_operator);
      if (curr_token != tok_lparen && next_token() == tok_lparen) {
        /* Ignore the current token (whatever it might be -- e.g., '?') and
           make a fake identifier to represent the operator. */
      } else {
        /* Either the operator token was omitted and we are at the '(' or else
           we're lost.  Put back the current token and make a fake error
           identifier. */
        unget_token();
      }  /* if */
      make_specific_symbol_error_locator(&locator_for_curr_id);
    } else {
      /* Convert the locator to a locator for the operator. */
      make_opname_locator(opname, &locator_for_curr_id, &start_position);
    }  /* if */
    curr_token = tok_identifier;
    pos_curr_token = error_position = start_position;
  }  /* if */
}  /* get_opname */


static a_boolean is_global_new_or_delete(void)
/*
Return TRUE if the current and next token form ":: new" or ":: delete".
These look like qualified names but aren't.
*/
{
  a_boolean    is_new_or_delete = FALSE;
  a_token_kind ntoken;

  if (curr_token == tok_colon_colon) {
    ntoken = next_token();
    if (ntoken == tok_new || ntoken == tok_delete) {
      is_new_or_delete = TRUE;
    }  /* if */
  }  /* if */
  return is_new_or_delete;
}  /* is_global_new_or_delete */


void flush_to_end_of_arg_list(void)
/*
Flush tokens in an argument list.
*/
{
  a_token_set_array_element save_comma_stop_token_count;
  a_token_set_array_element *comma_entry_ptr;
  /* Remove comma from the stop tokens set so that we can flush to the
     end of the argument list. */
  comma_entry_ptr = &(curr_stop_token_stack_entry->
                                                 stop_tokens[(int)tok_comma]);
  save_comma_stop_token_count = *comma_entry_ptr;
  *comma_entry_ptr = 0;
  flush_tokens();
  /* Restore comma as a stop token (if it was one). */
  *comma_entry_ptr = save_comma_stop_token_count;
}  /* flush_to_end_of_arg_list */


a_template_ptr scan_template_template_argument(
				a_template_ptr		param_template,
				a_source_position	*err_pos)
/*
Scan the actual argument for a template template parameter.  param_template
is the template pointer of the corresponding template template parameter.
err_pos is the position to be used to report any errors.
*/
{
  a_symbol_ptr				sym = NULL;
  a_boolean				err = FALSE;
  a_template_ptr			result = NULL;
  a_boolean				any_errors = FALSE;
  a_template_symbol_supplement_ptr	tssp;

  if (is_generalized_identifier_start(GID_TEMPLATE_ARGS_OPTIONAL)) {
    sym = coalesce_and_lookup_generalized_identifier(
                                 GID_TEMPLATE_ARGS_OPTIONAL, ilm_normal, &err);
    /* Make sure the symbol found is accessible and unambiguous. */
    check_ambiguity_and_verify_access(&locator_for_curr_id);
    if (err || (sym != NULL && sym->ambiguous)) {
      /* An error was already diagnosed by the identifier coalescing
         routines. */
      any_errors = TRUE;
    } else if (sym == NULL) {
      any_errors = TRUE;
      pos_st_error(ec_undefined_identifier, err_pos,
                   locator_for_curr_id.symbol_header->identifier);
    } else if (!is_class_template_symbol(sym)) {
      any_errors = TRUE;
      pos_sy_error(ec_sym_not_a_class_template, err_pos, sym);
    }  /* if */
    /* Bypass the identifier token. */
    (void)get_token();
  } else {
    /* Not an identifier. */
    any_errors = TRUE;
    syntax_error(ec_exp_identifier);
  }  /* if */
  if (sym != NULL) {
    /* If this is a template template parameter, replace the template symbol
       with the one referred to by the parameter. */
    sym =  template_argument_if_template_template_param(sym);
  }  /* if */
  if (!any_errors && param_template != NULL) {
    /* Make sure this argument is compatible with the template template
       parameter. */
    a_template_symbol_supplement_ptr	tssp1;
    a_template_symbol_supplement_ptr	tssp2;
    tssp1 = template_supplement_for_template(param_template);
    tssp2 = sym->variant.template_info;
    if (!tssp1->is_nonreal_member && !tssp2->is_nonreal_member) {
      /* Nonreal members have no template parameter lists.  The comparison
         will be done again later when a real member is available. */
      if (!equiv_template_param_lists(tssp1->cache.decl_info->parameters,
                                      tssp2->cache.decl_info->parameters,
		 		      /*issue_errors=*/FALSE,
				      (a_source_position*)NULL)) {
        a_symbol_ptr	param_sym;
        param_sym = (a_symbol_ptr)param_template->source_corresp.assoc_info;
        pos_sy2_error(ec_not_compatible_with_templ_templ_param, err_pos, sym, 
                      param_sym);
        any_errors = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (!any_errors) {
    /* The symbol is valid. Record the reference on the symbol. */
    mark_referenced(sym, &locator_for_curr_id.source_position);
  } else {
    /* An error occurred while scanning the argument.  Use a shared error
       class template as the result. */
    sym = error_class_template();
  }  /* if */
  tssp = sym->variant.template_info;
  result = tssp->il_template_entry;
  return result;
}  /* scan_template_template_argument */


static a_template_arg_ptr scan_unknown_template_arg_list(a_boolean is_nonreal)
/*
Scan a template argument list associated with an unknown template
parameter list.  This is done when scanning the template arguments
for an explicitly specified function template argument list, when
the specific template whose arguments are being scanned may not be
known yet.  is_nonreal is FALSE to indicate that an explicit function
template argument list is being scanned.

When is_nonreal is TRUE, the argument list being scanned is associated with
a template that is a member of a proxy or nonreal class.  This occurs as a 
result of constructs like T::A<int>.  In such cases there is no template
parameter list to use as a basis for the template arguments that are scanned.

For each argument, determine whether it is a type or nontype.  This is
done using the disambiguation routines.
*/
{
  a_template_arg_ptr              arg_ptr;
  a_template_arg_ptr              arg_list = NULL;
  a_template_arg_ptr              last_arg = NULL;
  a_boolean                       is_type_param;
  a_templ_arg_kind		  arg_kind;
  a_type_ptr                      argument_type;
  a_constant_ptr                  constant;
  a_symbol_ptr			  sym;

  do {
    /* If the current token is a ">" then exit the loop.  This should only be
       possible on the first iteration if we have an empty argument list.
       If it occurs elsewhere, we must have a comma followed by the closing
       ">" of the template argument list. */
    if (curr_token == tok_gt) {
      if (arg_list != NULL) {
        error(ec_expected_template_arg);
      }  /* if */
      break;
    }  /* if */
    add_stop_token(tok_comma);
    sym = NULL;
    /* Determine the kind of template argument. */
    if (is_generalized_identifier_start(GID_TEMPLATE_ARGS_OPTIONAL)) {
      a_boolean	err;
      sym = coalesce_and_lookup_generalized_identifier(
                                 GID_TEMPLATE_ARGS_OPTIONAL, ilm_normal, &err);
    }  /* if */
    if (sym != NULL && is_class_template_symbol(sym)) {
      arg_kind = (a_templ_arg_kind)tak_template;
    } else {
      is_type_param = is_decl_not_expr(DFS_ABSTRACT_DECLARATOR_ALLOWED |
                                       DFS_SINGLE_TYPE_REQUIRED |
                                       DFS_IS_TEMPLATE_ARGUMENT);
      arg_kind = is_type_param ? (a_templ_arg_kind)tak_type
                               : (a_templ_arg_kind)tak_nontype;
    }  /* if */
    arg_ptr = alloc_template_arg(arg_kind);
    /* When is_nonreal is FALSE, we are scanning an explicit function
       template argument list. */
    arg_ptr->explicitly_specified = !is_nonreal;
    if (is_type_templ_arg(arg_ptr)) {
      type_name(&argument_type);
      arg_ptr->variant.type = argument_type;
    } else if (is_nontype_templ_arg(arg_ptr)) {
      if (is_nonreal) {
        /* Scan a constant.  We can't know the type, so pass in a NULL
           type to indicate this. */
        constant = fs_constant((a_constant_repr_kind)ck_error);
        scan_template_argument_constant_expression((a_type_ptr)NULL, constant);
        arg_ptr->variant.constant = constant;
      } else {
        /* Scan the expression, but retain it in the form of an operand so
           that the necessary conversions can be done later when the parameter
           type is known. */
        arg_ptr->variant.constant = NULL;
        arg_ptr->arg_operand = scan_nontype_template_argument();
      }  /* if */
    } else {
      /* A template template argument. */
      a_template_ptr	templ_ptr;
      check_assertion(is_template_templ_arg(arg_ptr));
      templ_ptr = scan_template_template_argument((a_template_ptr)NULL,
                                                   &error_position);
      arg_ptr->variant.templ = templ_ptr;
    }  /* if */
    /* Link this entry on to the argument list. */
    if (arg_list == NULL) arg_list = arg_ptr;
    if (last_arg != NULL) last_arg->next = arg_ptr;
    last_arg = arg_ptr;
    remove_stop_token(tok_comma);
  } while (loop_token(tok_comma));
  return arg_list;
}  /* scan_unknown_template_arg_list */


static
a_template_arg_ptr scan_template_argument_list(a_symbol_ptr	template_sym,
					       a_boolean        *any_errors)
/*
Scan a comma separated list of arguments.  The arguments can be
type names, constant expressions, or addresses of objects or functions
with external linkage, or of static class members.  It is not necessary
to distinguish between the type and constant case because we can use the
type of the formal parameter to make this selection.

template_sym points to the template with which this argument list is
associated.  any_errors is set to TRUE if any errors are detected by
this routine.  Its value is unchanged if no errors are detected.
*/
{
  a_template_param_ptr            param_ptr = NULL;
  a_symbol_ptr                    sym;
  a_type_ptr                      argument_type;
  a_constant_ptr                  constant;
  a_template_arg_ptr              arg_ptr;
  a_template_arg_ptr              arg_list = NULL;
  a_template_arg_ptr              last_arg = NULL;
  a_templ_arg_kind		  arg_kind;
  a_template_decl_info_ptr	  decl_info;

  decl_info = template_sym->variant.template_info->cache.decl_info;
  param_ptr = decl_info->parameters;
  /* Indicate that this is an error case if the template has any empty
     parameter list. */
  if (param_ptr == NULL) *any_errors = TRUE;
  do {
    a_source_position  arg_pos;
    /* If the current token is a ">", and this is the first argument,
       then exit the loop (an empty argument list). */
    if (curr_token == tok_gt && arg_list == NULL) break;
    arg_pos = pos_curr_token;
    /* If the template parameter list is empty, exit the loop.  This only
       occurs in error cases. */
    if (param_ptr == NULL) break;
    add_stop_token(tok_comma);
    sym = param_ptr->param_symbol;
    /* Determine the template argument kind for this parameter. */
    arg_kind = templ_arg_kind_for_symbol_kind(sym->kind);
    arg_ptr = alloc_template_arg(arg_kind);
    if (is_type_templ_arg(arg_ptr)) {
      type_name(&argument_type);
      if (is_or_contains_local_type(argument_type)) {
        /* Be sure the type does not involve any local classes -- only
           externally visible types are allowed, since template classes are
           themselves externally linked. */
        pos_error(ec_local_type_in_template_arg, &arg_pos);
        argument_type = error_type();
      }  /* if */
      arg_ptr->variant.type = argument_type;
    } else if (is_nontype_templ_arg(arg_ptr)) {
      a_type_ptr  constant_type = sym->variant.constant->type;
      /* If the type of a constant involves a template parameter type,
         rescan the declaration of the parameter type to get the type
         to be used in this argument list. */
      if (param_ptr->variant.constant.type_involves_template_param) {
	constant_type = rescan_template_constant_parameter
                             (template_sym, sym, param_ptr, arg_list,
                              /*do_default_arg=*/FALSE, (a_constant_ptr*)NULL);
      }  /* if */
      constant = fs_constant((a_constant_repr_kind)ck_error);
      scan_template_argument_constant_expression(constant_type, constant);
      /* Make sure the constant does not use a local or nonexternal
         variable, etc. */
      if (constant_references_non_external_entity(constant)) {
        pos_error(ec_nonexternal_entity_in_template_arg, &arg_pos);
        set_error_constant(constant);
      }  /* if */
      arg_ptr->variant.constant = constant;
    } else {
      /* A template template argument. */
      a_template_ptr	templ;
      check_assertion_str(sym->kind == (a_symbol_kind)sk_class_template,
                          "scan_template_argument_list: template expected");
      templ = scan_template_template_argument(
                        param_ptr->variant.templ->il_template_entry, &arg_pos);
      arg_ptr->variant.templ = templ;
    }  /* if */
    /* Link this entry on to the argument list. */
    if (arg_list == NULL) arg_list = arg_ptr;
    if (last_arg != NULL) last_arg->next = arg_ptr;
    last_arg = arg_ptr;
    remove_stop_token(tok_comma);
    param_ptr = param_ptr->next;
  } while (param_ptr != NULL && loop_token(tok_comma));

  /* All arguments should have been processed and the current token should
     be the closing angle bracket. */
  if (param_ptr != NULL) {
    /* There are still entries on the formal parameters list -- see if
       the remaining parameters have default values. */
    if (param_ptr->has_default_arg) {
      /* The template has parameters with default values.  Fill in the
         remainder of the parameter list with the defaults. */
      while (param_ptr != NULL) {
        sym = param_ptr->param_symbol;
        /* Determine the template argument kind for this parameter. */
        arg_kind = templ_arg_kind_for_symbol_kind(sym->kind);
        arg_ptr = alloc_template_arg(arg_kind);
	if (is_type_templ_arg(arg_ptr)) {
          if (param_ptr->has_default_arg) {
            /* A type parameter with a default value.  The default can be
	       either a type or a token cache that needs to be scanned. */
            arg_ptr->variant.type =
                     rescan_template_type_default_arg(template_sym,
                                                      param_ptr, arg_list);
          } else {
            /* A type parameter with no default argument.  This occurs only
               in error cases.  Use an error type. */
            arg_ptr->variant.type = error_type();
          }  /* if */
        } else if (is_template_templ_arg(arg_ptr)) {
          /* A template template argument. */
          if (param_ptr->has_default_arg) {
            /* A type parameter with a default value.  The default can be
	       either a type or a token cache that needs to be scanned. */
            arg_ptr->variant.templ =
                     rescan_template_template_default_arg(template_sym,
                                                          param_ptr, arg_list);
          } else {
            /* A template template parameter with no default argument.
               This occurs only in error cases.  Use an error template. */
            a_symbol_ptr	error_sym;
            error_sym = error_class_template();
            arg_ptr->variant.templ =
                           error_sym->variant.template_info->il_template_entry;
          }  /* if */
        } else {
          /* A nontype argument. */
          check_assertion(is_nontype_templ_arg(arg_ptr));
	  if (param_ptr->has_default_arg) {
            /* A constant parameter.  The default value can be either a
  	       constant value or a token cache that needs to be scanned.
               Call a routine that will rescan the type declaration and/or
               default argument expression. */
            (void)rescan_template_constant_parameter
                                    (template_sym, sym, param_ptr, arg_list,
                                     /*do_default_arg=*/TRUE, &constant);
            arg_ptr->variant.constant = constant;
          } else {
            /* A nontype constant without a default argument.  This also only
               occurs in error cases.  Use an error constant. */
            constant = alloc_error_constant();
            arg_ptr->variant.constant = constant;
          }  /* if */
        }  /* if */
        /* Link this entry on to the argument list. */
        if (arg_list == NULL) arg_list = arg_ptr;
        if (last_arg != NULL) last_arg->next = arg_ptr;
        last_arg = arg_ptr;
	param_ptr = param_ptr->next;
      }  /* while */
    } else {
      /* The next parameter doesn't have a default value (note that
	 nontype parameters cannot have defaults).  Issue an error. */
      sym_error(ec_too_few_template_args, template_sym);
      *any_errors = TRUE;
    }  /* if */
  } else if (curr_token == tok_comma) {
    /* All of the formal parameters have been accounted for and there are
       more actuals -- too many arguments were supplied. */
    pos_sy_error(ec_too_many_template_args, &pos_curr_token, template_sym);
    flush_to_end_of_arg_list();
    *any_errors = TRUE;
  }  /* if */
  return arg_list;
}  /* scan_template_argument_list */


static void invalid_end_of_template_arg_list(void)
/*
This routine is called when the end of a template argument list is reached
but the current token is not ">".  The main purpose of this routine is
to more gracefully handle the case when a ">>" appears where a ">" was
expected, and there is currently more than one template argument list in
the process of being scanned.
*/
{
  if (curr_token == tok_shift_right &&
      scope_stack[depth_scope_stack].pending_templ_arg_lists > 1) {
    /* A ">>" that appears to have been intended to close two template
       argument lists.  Issue a special diagnostic for this case and insert a
       ">" into the token stream that will close the outer template
       argument list. */
    a_token_cache 	cache;
    error(ec_exp_gt_not_shift_right);
    clear_token_cache(&cache, /*reusable=*/FALSE);
    curr_token = tok_gt;
    cache_curr_token(&cache);
    rescan_cached_tokens(&cache);
  } else {
    /* There are not two template argument lists pending.  Simply issue an
       "expected '>'" error. */
    syntax_error(ec_exp_gt);
  }  /* if */
}  /* invalid_end_of_template_arg_list */


a_symbol_ptr coalesce_template_class_reference(
			a_symbol_ptr			template_sym,
			an_identifier_options_set	options,
			a_boolean			*err)
/*
The current identifier is a class template name.  Look for an optional
template argument list.  If an argument list is present, scan the argument
list and call a routine to lookup or create the symbol and type information
for an instance of the class template.  The template argument list is
required unless either the GID_TEMPLATE_ARGS_OPTIONAL flag is set in the
"options" argument, or the class template pointed to by "template_sym"
is the same as the class template associated with the innermost instantiation
scope.   If no errors occur while scanning the argument list, we call
a routine to lookup the appropriate instance (or generate one if needed).
*/
{
  a_source_position               start_position;
  a_template_arg_ptr              arg_list = NULL;
  a_symbol_ptr                    new_sym = NULL;
  a_symbol_ptr			  current_instantiation_sym;
  a_boolean                       any_errors = FALSE;
  a_memory_region_number          region_to_switch_back_to;
  a_token_kind			  next_tok;
  a_boolean			  class_is_being_instantiated = FALSE;
  a_boolean			  arg_list_coalesced = FALSE;
  a_boolean			  arg_list_processed = FALSE;
  a_symbol_locator		  orig_locator;
  a_boolean			  error_locator_created = FALSE;
  a_boolean			  is_constructor_reference = FALSE;
  a_type_ptr			  orig_ctor_type;
  a_symbol_ptr			  orig_ctor_symbol;
  a_boolean                       is_expr_context =
                                         (options & GID_IS_EXPR_CONTEXT) != 0;

  db_enter(3, "coalesce_template_class_reference");

  *err = FALSE;
  next_tok = next_token();
  /* Save source position for error reporting. */
  start_position = pos_curr_token;
  /* Save the current locator. */
  orig_locator = locator_for_curr_id;
  if (microsoft_mode && template_sym != NULL && next_tok == tok_lt &&
      is_constructor_symbol(template_sym)) {
    /* The symbol passed in is a constructor symbol followed by a template
       argument list, as in A<T>::A<T>.  Replace the constructor symbol with
       the symbol associated with the original template so that the template
       argument list can be processed.  Later, the constructor symbol will be
       substituted for the class instance that is created.  This is only
       permitted in Microsoft mode. */
    a_type_ptr	parent_class;
    a_class_symbol_supplement_ptr	parent_cssp;
    parent_class = template_sym->parent.class_type;
    parent_cssp = symbol_supplement_for_class(parent_class);
    if (parent_cssp->class_template != NULL) {
      orig_ctor_symbol = template_sym;
      template_sym = parent_cssp->class_template;
      /* Get the primary template in case this instance is associated with
         a partial specialization. */
      template_sym = primary_template_of(template_sym);
      is_constructor_reference = TRUE;
      orig_ctor_type = parent_class;
    }  /* if */
  }  /* if */
  if (template_sym == NULL ||
      !is_class_template_or_injected_template_symbol(template_sym)) {
    /* The symbol is not a class template symbol.  If the symbol
       is a type symbol followed by what looks like the beginning
       of a template argument list (i.e., a "<") issue an error
       indicating that the current symbol cannot have a template argument
       list.  If the symbol is not a type the action taken depends on whether
       or not we are in a context in which a "<" is valid as part of
       an expression.  If the "<" might be part of an expression, we
       return without further processing.  Otherwise, a diagnostic is
       issued and the template argument list is processed. */
    a_boolean	lt_permitted_context;
    a_boolean	is_error_symbol;
    lt_permitted_context = (options & GID_IS_NEW_TYPE_NAME) != 0 ||
                           (options & GID_IS_FIELD_SELECTION_OPERAND) != 0;
    is_error_symbol = template_sym == NULL || template_sym->is_error ||
                      template_sym->ambiguous ||
                      template_sym->kind == (a_symbol_kind)sk_undefined;
    if (template_sym != NULL &&
        is_type_symbol(template_sym) && next_tok == tok_lt &&
        !lt_permitted_context) {
      /* A type name followed by a template argument list. */
      pos_sy_error(ec_unexpected_template_arg_list, &start_position,
                   template_sym);
      template_sym = NULL;
    } else if (sun_mode && template_sym != NULL &&
               scope_stack[depth_scope_stack].in_prototype_instantiation &&
               !is_type_symbol(template_sym) &&
               !is_error_symbol && !is_expr_context && !lt_permitted_context) {
      /* A nontype symbol (probably from a nonreal base) during a prototype
         instantiation in Sun mode.  Ignore this error. */
      template_sym = NULL;
    } else if (!is_error_symbol &&
               !lt_permitted_context && !is_expr_context) {
      /* A nontype symbol followed by a template argument list in a
         nonexpression context. */
      pos_sy_error(ec_sym_not_a_template, &start_position,
                   template_sym);
      template_sym = NULL;
    } else if (is_error_symbol &&
               !lt_permitted_context && !is_expr_context) {
      /* A NULL symbol, error symbol, or ambiguous symbol followed by
         a template argument list. */
      if (template_sym == NULL) {
        /* If the symbol is an error symbol or ambiguous, assume an error
           has already been issued. */
        if ((options & GID_IS_CLASS_TEMPLATE_DECL) != 0) {
          /* Use a special message in a class template declaration that
             indicates that a template argument list is not permitted on
             a primary template. */
          pos_error(ec_partial_spec_is_primary_template, &start_position);
        } else if ((options & GID_IS_TEMPLATE_PRESCAN) != 0) {
          /* Suppress the diagnostic in this case. */
        } else if (is_error_locator(locator_for_curr_id)) {
          /* An error locator.  Don't issue a diagnostic for this case. */
        } else {
          if (sun_mode &&
              scope_stack[depth_scope_stack].in_prototype_instantiation &&
              !is_expr_context && !lt_permitted_context) {
            /* The Sun compiler does not do prototype instantiations, so
               suppress this error in Sun mode. */
          } else {
            pos_st_error(ec_not_a_template, &start_position,
                         locator_for_curr_id.symbol_header->identifier);
          }  /* if */
        }  /* if */
      }  /* if */
      template_sym = NULL;
    } else {
      /* A valid symbol in an expression context.  Just return the symbol
         that was passed in. */
      new_sym = template_sym;
      goto skip_processing;
    }  /* if */
  }  /* if */
  /* Determine whether the class template (or a member of the class template)
     is currently being instantiated.  This affects how references to the
     class template name are handled. */
  current_instantiation_sym = template_sym;
  if (template_sym != NULL) {
    class_is_being_instantiated =
            current_class_symbol_if_class_template(&current_instantiation_sym);
  }  /* if */
  if (next_tok != tok_lt) {
    if (options & GID_CLASS_TEMPLATE_REQUIRED) {
      /* The caller wants the class template symbol.  Just return the
         class template symbol that we started with. */
      new_sym = template_sym;
      goto skip_processing;
    } else if (template_sym != NULL &&
               is_injected_template_symbol(template_sym)) {
      /* The template symbol is actually an sk_type symbol that points
         to the template instance type.  When this symbol is not followed
         by a template argument list it may be used to refer to the
         current instance (which it already does). */
      new_sym = template_sym;
      goto skip_processing;
    } else {
      /* There is no template argument list.  If we are in an instantiation of
         this class template, use the symbol associated with the innermost
         instantiation of this class, otherwise just return the class
         template symbol.  This mechanism is used when class name injection
         is not enabled.  When class name injection is enabled, the injected
         name will be found in place of the template in contexts in which
         the current instance should be used.  When class name injection is
         not enabled, the current instance will be used except when the
         template was named with a qualified name. */
      if (class_is_being_instantiated &&
          (!class_name_injection_enabled || microsoft_bugs) &&
          !locator_for_curr_id.is_qualified_name) {
        /* We have the symbol for the current instantiation of the
           class template.  This is done in Microsoft bugs mode even when class
           name injection is enabled because template names are not injected
           in Microsoft mode. */
        new_sym = current_instantiation_sym;
        goto normal_exit;
      } else {
        if (options & GID_TEMPLATE_ARGS_OPTIONAL) {
           /* Template arguments are not required -- simply return the
              symbol of the class template. */
           new_sym = template_sym;
           goto skip_processing;
        } else {
          /* Issue an error and return an error locator. */
          if (template_sym != NULL) {
            /* template_sym will be NULL if the symbol passed in was
               invalid. */
            pos_sy_error(ec_missing_template_arg_list, &start_position,
                         template_sym);
          }  /* if */
          make_specific_symbol_error_locator(&locator_for_curr_id);
          new_sym = locator_for_curr_id.specific_symbol;
          any_errors = TRUE;
          error_locator_created = TRUE;
          goto normal_exit;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (template_sym != NULL && is_injected_template_symbol(template_sym)) {
    /* The symbol is the injected name of a class template.  In a template
       class this points to the current instance of the class.  When
       followed by a template argument list, we need to substitute the
       class template symbol for the injected symbol. */
    template_sym = class_template_for_injected_template_symbol(template_sym);
  }  /* if */
  /* Always allocate template arguments at the file scope. */
  switch_to_file_scope_region(&region_to_switch_back_to);
  add_stop_token(tok_gt);
  add_stop_token(tok_lbrace);
  add_stop_token(tok_semicolon);
  /* Get the angle bracket token. */
  (void)get_token();
  /* Get token following opening angle bracket. */
  (void)get_token();
  /* Increment the number of template argument lists that are being scanned. */
  scope_stack[depth_scope_stack].pending_templ_arg_lists++;
  if (template_sym != NULL &&
      !template_sym->variant.template_info->is_nonreal_member &&
      !template_sym->variant.template_info->is_error) {
    /* Scan the template argument list. */
    arg_list = scan_template_argument_list(template_sym, &any_errors);
  } else {
    /* The template is a member of a proxy or nonreal class.  This occurs
       as a result of constructs like T::A<int>.  In such cases there is
       no template parameter list to use as a basis for the template
       arguments that are scanned.  Scan the template arguments that
       have been supplied.  This kind of scan is also done when there
       is no template symbol, which happens if an undefined symbol is
       followed by a template argument list. */
    arg_list = scan_unknown_template_arg_list(/*is_nonreal=*/TRUE);
  }  /* if */
  arg_list_processed = TRUE;
  /* We should now be at the closing angle bracket.  Note that we don't
     scan the token after the closing angle because we update the current
     token below to represent the original identifier with the newly
     found template class symbol. */
  set_err_pos_to_curr_token();
  if (curr_token != tok_gt) {
    if (!any_errors) {
      /* Report the error and gracefully recover if a ">>" was used in place
         of "> >". */
      invalid_end_of_template_arg_list();
    }  /* if */
    any_errors = TRUE;
  }  /* if */
  /* Decrement the number of template argument lists that are being scanned. */
  scope_stack[depth_scope_stack].pending_templ_arg_lists--;
  if (!any_errors && template_sym != NULL) {
    /* Everything is OK -- find the instance that matches these arguments.
       Create a new instance if needed.  There can be two instances
       of a class A<T> One is the prototype instantiation which is
       used when A<T> is referenced within the definition of class
       template A or in the declarator of an out-of-line definition of a member
       function or static data member.  The other is the nonreal class
       which is used when, for example, A<T> is a member of class template
       B.  We find the prototype instantiation unless we are currently
       inside the instantiation of a different class. */
    a_boolean			prototype_allowed;
    a_boolean			is_templ_member_class_sym = FALSE;
    a_scope_stack_entry_ptr	ssep;
    ssep = &scope_stack[depth_scope_stack];
    /* Determine whether the template being used is either the class associated
       with a member that is being defined, or a template enclosing that
       class. */
    {
      a_symbol_ptr			tmc_sym = ssep->templ_member_class_sym;
      while (tmc_sym != NULL) {
        a_template_symbol_supplement_ptr	tmc_tssp;
        a_type_ptr				parent_type = NULL;
        tmc_tssp = tmc_sym->variant.template_info;
        /* If the template of which a member is being defined is a partial
           specialization, use the primary template instead for the purpose
           of determining whether the template being used matches the template
           of which a member is being defined. */
        if (tmc_tssp->variant.class_template.primary_template_sym != NULL) {
          tmc_sym = tmc_tssp->variant.class_template.primary_template_sym;
        }  /* if */
        if (tmc_sym == template_sym) {
          is_templ_member_class_sym = TRUE;
          break;
        }  /* while */
        if (tmc_sym->is_class_member) {
          /* Get the class template symbol associated with the nearest
             enclosing class template. */
          a_class_symbol_supplement_ptr	cssp;
          parent_type = tmc_sym->parent.class_type;
          while (parent_type->source_corresp.is_class_member &&
                 parent_type->variant.class_struct_union.extra_info->
                                                  template_arg_list == NULL) {
            parent_type = parent_type->source_corresp.parent.class_type;
          }  /* while */
          cssp = symbol_supplement_for_class(parent_type);
          tmc_sym = cssp->class_template;
        } else {
          tmc_sym = NULL;
        }  /* if */
      }  /* while */
    }
    prototype_allowed = ((options & GID_USE_PROTOTYPE_NOT_NONREAL) != 0) ||
                        is_templ_member_class_sym;
    new_sym = find_template_class(template_sym, &arg_list, prototype_allowed,
                                  current_instantiation_sym);
    arg_list_coalesced = TRUE;
    if (is_constructor_reference) {
      /* If a constructor symbol was passed originally, replace the class
         symbol that resulted from processing the template argument list with
         its constructor symbol.  Make sure the class type is complete before
         doing so. */
      a_class_symbol_supplement_ptr	cssp;
      a_type_ptr			new_type;
      new_type = type_symbol_type(new_sym);
      cssp = new_sym->variant.class_struct_union.extra_info;
      complete_class_type_is_needed(new_type);
      /* If the constructor name followed a class qualifier, make sure that
         the constructor name matches the original class name. */
      if (identical_types(new_type, orig_ctor_type)) {
        new_sym = cssp->constructor;
      } else {
        pos_ty2_error(ec_bad_constructor_type, &error_position, new_type,
                      orig_ctor_type);
        new_sym = orig_ctor_symbol;
      }  /* if */  
    }  /* if */
  } else if (sun_mode && !any_errors && !is_expr_context &&
             scope_stack[depth_scope_stack].in_prototype_instantiation) {
    /* In Sun mode it is possible to refer to undeclared templates.  We get
       here for example with 
             template<class T> struct S { friend void f<>(); };  */
  } else {
    /* Free any allocated template arguments. */
    if (arg_list != NULL) free_template_arg_list(arg_list);
    /* An error occurred while scanning the argument list so make an error
       locator and return a pointer to its specific symbol. */
    make_specific_symbol_error_locator(&locator_for_curr_id);
    error_locator_created = TRUE;
    new_sym = locator_for_curr_id.specific_symbol;
  }  /* if */
  switch_back_to_original_region(region_to_switch_back_to);
  remove_stop_token(tok_gt);
  remove_stop_token(tok_lbrace);
  remove_stop_token(tok_semicolon);

normal_exit:
  if (arg_list_processed && curr_token != tok_gt) {
    /* Below we will set curr_token to tok_identifier.  Do an unget
       of the token that stopped the flush so that it can be processed
       later. */
    unget_token();
  }  /* if */
  /* When we return to the caller the current identifier should be an 
     identifier and the locator should point to the template class that we
     have just looked up. */
  curr_token = tok_identifier;
  /* Update the locator to reflect the new symbol that is being returned.
     We start by restoring the locator as it was when this routine was
     called and then update it to reflect the new symbol that was produced
     by this process.  The symbol header should already be correct but
     is updated just for safety.   If a template argument list has been
     coalesced, set the do_not_clear_speecific symbol field of the locator.
     This is needed to ensure because the argument list information is now
     represented by the fact that the specific symbol points to a particular
     template class instance, and this information cannot be recreated
     once the template reference has been coalesced. */
  if (error_locator_created) {
    /* Don't reset the locator if we created an error locator earlier.
       Just update the source position to reflect the position of the
       original locator. */
    locator_for_curr_id.source_position = orig_locator.source_position;
  } else {
    locator_for_curr_id = orig_locator;
  }  /* if */
  if (new_sym != NULL) {
    locator_for_curr_id.specific_symbol = new_sym;
    locator_for_curr_id.do_not_clear_specific_symbol = arg_list_coalesced;
    locator_for_curr_id.symbol_header = new_sym->header;
  }  /* if */
  /* Don't set the is_template_id field after processing a constructor
     reference followed by a template argument list in Microsoft mode. */
  locator_for_curr_id.is_template_id = !is_constructor_reference;
  /* Set source position for error reporting. */
  error_position = start_position;

#if DEBUG
  if (debug_level >= 5) {
    db_symbol(template_sym, "Template symbol: ", 2);
  }  /* if */
  if (debug_level >= 4) {
    db_symbol(new_sym, "Returning: ", 2);
  }  /* if */
#endif /* DEBUG */

skip_processing:
  *err = any_errors;
  db_exit();
  return new_sym;
}  /* coalesce_template_class_reference */


static a_symbol_ptr coalesce_template_function_reference(
			a_symbol_ptr			template_sym,
			a_token_kind			next_tok,
			a_boolean			*err)
/*
The current identifier is a function template symbol or an overload set
containing a function template symbol, and if next_tok is tok_lt ("<"),
is followed by a template argument list.  Scan the template argument list
and update the locator to point to it.  The template arguments are not
scanned with respect to a particular template parameter list because, in
the general case, you don't know which of several potential parameter lists
is the one actually associated with this reference.
*/
{
  a_source_position             start_position;
  a_template_arg_ptr            arg_list = NULL;
  a_memory_region_number        region_to_switch_back_to;
  a_symbol_locator		orig_locator;
  a_boolean			any_errors = FALSE;

  db_enter(3, "coalesce_template_function_reference");
  /* Save source position for error reporting. */
  start_position = pos_curr_token;
  if (next_tok == tok_lt) {
    /* Save the current locator. */
    orig_locator = locator_for_curr_id;
    /* Always allocate template arguments at the file scope. */
    switch_to_file_scope_region(&region_to_switch_back_to);
    add_stop_token(tok_gt);
    add_stop_token(tok_lbrace);
    add_stop_token(tok_semicolon);
    /* Get the angle bracket token. */
    (void)get_token();
    check_assertion(curr_token == tok_lt);
    /* Get token following opening angle bracket. */
    (void)get_token();
    /* Increment the number of template argument lists that are being
       scanned. */
    scope_stack[depth_scope_stack].pending_templ_arg_lists++;
    /* Scan the template argument list. */
    arg_list = scan_unknown_template_arg_list(/*is_nonreal=*/FALSE);
    /* We should now be at the closing angle bracket.  Note that we don't
       scan the token after the closing angle because we update the current
       token below to represent the original identifier with the newly
       found template class symbol. */
    set_err_pos_to_curr_token();
    if (curr_token != tok_gt) {
      /* Report the error and gracefully recover if a ">>" was used in place
         of "> >". */
      invalid_end_of_template_arg_list();
      any_errors = TRUE;
    }  /* if */
    /* Decrement the number of template argument lists that are being
       scanned. */
    scope_stack[depth_scope_stack].pending_templ_arg_lists--;
    switch_back_to_original_region(region_to_switch_back_to);
    remove_stop_token(tok_gt);
    remove_stop_token(tok_lbrace);
    remove_stop_token(tok_semicolon);
    if (curr_token != tok_gt) {
      /* Below we will set curr_token to tok_identifier.  Do an unget
         of the token that stopped the flush so that it can be processed
         later. */
      unget_token();
    }  /* if */
    /* Upon return, the locator should refer to the symbol that was passed
       in, but should also include the template argument list. */
    curr_token = tok_identifier;
    locator_for_curr_id = orig_locator;
  }  /* if */
  locator_for_curr_id.is_template_id = TRUE;
  locator_for_curr_id.template_arg_list = arg_list;
  /* Set source position for error reporting. */
  error_position = start_position;
  *err = any_errors;
  db_exit();
  return template_sym;
}  /* coalesce_template_function_reference */


static a_symbol_ptr ensure_correct_nonreal_instance_kind(
			a_symbol_ptr			sym,
			an_identifier_options_set	options,
			a_symbol_ptr			orig_template_sym)
/*
When scanning a name that comes after the "template" keyword it is
difficult to know whether the name is intended to be a class template
reference or a function template reference.  The name is a class
template reference if it is part of a qualified name that was preceded
with the "typename" keyword, or if the template argument list is followed
by "::".  For example:

  p->template g<1>();     // template is a function
  p->template g<1>::f();  // template is a class

We can't know what follows the argument list until we get there.  So,
we initially scan the reference as an unknown class template reference.

This routine converts the reference to a function if that is what is
required.  Actually, when this is done, "orig_template_sym" is returned,
otherwise the original "sym" is returned.
*/
{
  a_class_symbol_supplement_ptr	cssp;
  a_symbol_ptr			template_sym;

  /* First make sure this is really the case we need to worry about.
     The caller only makes a cursory check.  The check must be done
     when "sym" refers to a nonreal class that is an instance of
     a nonreal template.  The caller already checked that it is a
     nonreal class. */
  check_assertion(is_class_struct_union_symbol(sym));
  cssp = sym->variant.class_struct_union.extra_info;
  template_sym = cssp->class_template;
  check_assertion(template_sym != NULL);
  if (template_sym->variant.template_info->is_nonreal_member) {
    /* The class is a member of a nonreal template.  Determine
       whether we want a class or not. */
    a_boolean	type_wanted = FALSE;
    if ((options & GID_IS_TYPENAME) != 0) {
      type_wanted = TRUE;
    } else if (next_token() == tok_colon_colon) {
      type_wanted = TRUE;
    } else if (implicit_typename_enabled &&
               (options & GID_IS_EXPR_CONTEXT) == 0) {
      type_wanted = TRUE;
    } else if ((options & GID_IMPLICIT_TYPE_CONTEXT) != 0) {
      type_wanted = TRUE;
    }  /* if */
    if (!type_wanted) {
      /* Restore the template argument list from the template class.  Make a
         copy that can be freed later when it is used. */
      locator_for_curr_id.template_arg_list = copy_template_arg_list(
                     sym->variant.class_struct_union.type->
                     variant.class_struct_union.extra_info->template_arg_list);
      sym = orig_template_sym;
      locator_for_curr_id.specific_symbol = sym;
      locator_for_curr_id.is_unknown_template_reference = TRUE;
    }  /* if */
  }  /* if */
  return sym;
}  /* ensure_correct_nonreal_instance_kind */


static a_symbol_ptr coalesce_template_id(
			a_symbol_ptr			template_sym,
			a_token_kind			next_tok,
			an_identifier_options_set	options,
			a_boolean			*err)
/*
This routine is called when an identifier is followed by "<" sign that
may be the start of a template argument list, or if the identifier
was preceded by the "template" keyword.  next_tok is tok_lt ("<") if
a template argument list is present.  If the symbol is a function
template symbol, or an overload set containing one or more templates,
coalesce_template_function_reference is called to scan the argument list.
Otherwise, coalesce_template_class_reference is called to either scan
the class template argument list or diagnose an invalid template reference.
*/
{
  a_symbol_ptr	result_sym;

  if (template_sym != NULL &&
      !is_class_template_or_injected_template_symbol(template_sym) && 
      symbol_is_or_contains_template(template_sym)) {
    /* A function template symbol or overload set containing a function
       template symbol. */
    result_sym = coalesce_template_function_reference(template_sym,
                                                      next_tok, err);
  } else {
    /* A class template symbol or a potential error case. */
    result_sym = coalesce_template_class_reference(template_sym, options, err);
    if (result_sym != NULL && is_nonreal_instance_class_symbol(result_sym) &&
        result_sym != template_sym) {
      /* We scanned this as a class template reference, but it is possible
         that it should really be considered a function (but we could not
         tell until we found out what token was after the template argument
         list).  Check that we have the right kind of entity, and convert
         to the right kind if necessary. */
      result_sym = ensure_correct_nonreal_instance_kind(result_sym, options,
                                                        template_sym);
    }  /* if */
  }  /* if */
  return result_sym;
}  /* coalesce_template_id */


static a_boolean f_check_for_template_declarator_errors(
				an_identifier_options_set	options,
				a_source_position		*error_pos)
/*
Check for certain errors that can occur while scanning the identifier
in a declarator of a template declaration.
*/
{
  a_boolean		any_errors = FALSE;
  a_symbol_ptr		sym = locator_for_curr_id.specific_symbol;

  if (is_error_locator(locator_for_curr_id) || sym == NULL) {
    /* An error has already been issued. */
  } else if (!locator_for_curr_id.is_qualified_name) {
    /* No error tests are done on unqualified names. */
  } else if (symbol_is_or_contains_template(sym)) {
    /* Okay -- the symbol found refers to a template. */
  } else if (scope_stack[depth_scope_stack].in_prototype_instantiation) {
    /* Ignore errors in prototype instantiations. */
  } else if (sym->is_class_member &&
             is_prototype_instantiation_symbol((a_symbol_ptr)sym->
                              parent.class_type->source_corresp.assoc_info)) {
    /* The symbol is a member of a prototype instantiation -- this is the
       definition of a member of a class template. */
  } else if (options & GID_IS_TEMPLATE_SPECIALIZATION) {
    /* We are processing a template specialization (but not a full
       specialization), and the symbol found does not represent a
       template.  This is an error. */
    pos_sy_error(ec_partial_specialization_not_allowed, error_pos, sym);
    any_errors = TRUE;
  } else {
    /* The remaining valid cases are members of class templates or classes
       nested within class templates. */
    if (!sym->is_class_member) {
      /* The named entity is not a template and is not a class member.
         No diagnostic is issued here.  That will be left to the caller.
         Special treatment is given to class members to better diagnose
         invalid declarations of members of class templates. */
    } else {
      /* The qualifier class type must point to a prototype instantiation. */
      a_type_ptr	tp;
      a_symbol_ptr	type_sym;
      a_boolean		is_prototype_instantiation;

      tp = sym->parent.class_type;
      type_sym = (a_symbol_ptr)tp->source_corresp.assoc_info;
      is_prototype_instantiation = is_prototype_instantiation_symbol(type_sym);
      if (is_prototype_instantiation) {
         /* Okay. */
      } else if (is_nonreal_instance_class_symbol(type_sym)) {
        /* If the class is not a real class type, then it is expected
           to be the prototype instantiation.  Decide which of two
           errors should be issued for this case.  The usual cause of
           this error is using an incorrect template argument list
           (one that does not match the template parameter list, but
           this may also be caused if the class template definition is
           currently incomplete (so there is no prototype
           instantiation yet). */
        a_symbol_ptr				template_sym;
        a_template_symbol_supplement_ptr	tssp;
        template_sym =
              type_sym->variant.class_struct_union.extra_info->class_template;
        tssp = template_supplement_for_symbol(template_sym);
        /* See if this template is a subordinate template (a template in
           a template class that is based on a member template from
           the class template).  If so, use the prototype template to check
           for a prototype instantiation. */
        if (tssp->prototype_template != NULL &&
            !tssp->is_specific_definition) {
          template_sym = tssp->prototype_template;
        }  /* if */
        if (template_sym->variant.template_info->
                    variant.class_template.prototype_instantiation == NULL) {
          /* There is no prototype yet.  This is probably caused by the
             class template being incomplete at this point. */
          pos_error(ec_incomplete_type_not_allowed, error_pos);
          any_errors = TRUE;
        } else {
          /* The class is a template class but not the prototype
             instantiation. */
          pos_error(ec_must_be_prototype_instantiation, error_pos);
          any_errors = TRUE;
        }  /* if */
      } else {
        /* Some other invalid type. */
        pos_ty_error(ec_not_a_class_template, error_pos, tp);
        any_errors = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return any_errors;
}  /* check_for_template_declarator_errors */


static a_boolean f_check_for_generalized_identifier_errors
		(an_identifier_options_set options,
                 a_source_position         *pos)
/*
Compares the flags in locator_for_curr_id with the error flags in the
options set passed by the caller and issues errors for cases not allowed
by the options.  Returns TRUE if any errors were diagnosed.
*/
{
  a_boolean   any_errors = FALSE;
  a_boolean   qualified_name_error;
  a_boolean   global_qualifier_error;
  a_boolean   operator_name_error;
  /* If no position was specified use the current error position. */
  if (pos == NULL) pos = &error_position;
  qualified_name_error = (locator_for_curr_id.is_qualified_name &&
                          (options & GID_DISALLOW_QUALIFIED_NAME));
  global_qualifier_error = (locator_for_curr_id.is_global_qualified_name &&
                            (options & GID_DISALLOW_GLOBAL_QUALIFIER));
  operator_name_error = (locator_for_curr_id.is_operator_name ||
                         locator_for_curr_id.is_conversion_name) &&
                          (options & GID_DISALLOW_OPERATOR_NAME);
  if (qualified_name_error && global_qualifier_error) {
    /* If both errors are being checked for, and both errors exist, only
       issue one of the errors using the following rules:

		::A		Global qualifier error
		::A::B		Qualified name error
    */
    if (locator_for_curr_id.is_file_scope_qualified_name) {
      qualified_name_error = FALSE;
    } else {
      global_qualifier_error = FALSE;
    }  /* if */
  }  /* if */

  if (qualified_name_error) {
    pos_error(ec_qualified_name_not_allowed, pos);
    any_errors = TRUE;
  } else if (global_qualifier_error) {
    pos_error(ec_global_qualifier_not_allowed, pos);
    any_errors = TRUE;
  } else if (operator_name_error) {
    pos_error(ec_operator_name_not_allowed, pos);
  }  /* if */
  return any_errors;
}  /* f_check_for_generalized_identifier_errors */


static a_boolean qualifier_delimiter_does_not_follow_token(void)
/*
Return TRUE if we can tell that the token following the current
token (usually an identifier) is not a "::", a "<",  or, in cfront
compatibility mode, a ".".  These delimiters are used to indicate
qualified names, e.g., A::B::x. If it is not easy to tell whether a
delimiter appears, the safe answer of FALSE is returned.  This routine
is part of a speed optimization: if the token following an identifier
is clearly not a qualified name delimiter, some expensive processing
can be avoided.
*/
{
  a_boolean     delim_does_not_follow = FALSE;
  register char ch;

  if (rescanning_cached_tokens()) {
    /* Tokens are coming from a token cache.  The optimization cannot be
       done in this case, but it is also not really needed because
       the next token can be inspected quickly when the tokens are coming
       from a cache. */
    /* delim_does_not_follow = FALSE;  -- already set. */
  } else {
    register a_boolean	ch_is_punct;
    /* Skip over any initial white space blanks and horizontal tabs.
       These are very common, so they're handled inline here. */
    if ((ch = *curr_char_loc) == ' ' || ch == '\t') {
      do {} while ((ch = *(++curr_char_loc)) == ' ' || ch == '\t');
    }  /* if */
    /* If the current character is a punctuation character but not a slash,
       or if the current character is alphabetic then we know we don't have
       to check for the more complex forms of white space; otherwise call
       skip_white_space to handle the other cases. */
    ch_is_punct = ispunct((unsigned char)ch);
    if (ch_is_punct && ch != '/') {
      /* Not the start of a comment but possibly a character that could
         be a qualifier delimiter. */
    } else if (isalpha((unsigned char)ch)) {
      /* An alphabetic character.  This might be a macro call so we
         can't tell whether or not this might be a qualifier delimiter.
         No need to check further below. */
      goto done;
    } else {
      /* Not one of the special cases.  Call the general skip white space
         routine before further checking. */
      skip_white_space();
      ch = *curr_char_loc;
      ch_is_punct = ispunct((unsigned char)ch);
    }  /* if */
    if (ch_is_punct) {
      /* The next token begins with a punctuation character.  Check for the
         special cases. */
      if (ch == ':' && curr_char_loc[1] == ':') {
        /* Definitely a "::". */
        /* delim_does_not_follow = FALSE;  -- already set. */
      } else if (ch == '<') {
        /* A "<" that could be a template argument list delimiter. */
        /* delim_does_not_follow = FALSE;  -- already set. */
      } else if (ch == '.' && cfront_2_1_mode) {
        /* Definitely a "." in cfront mode. */
        /* delim_does_not_follow = FALSE;  -- already set. */
      } else if (ch == '#') {
        /* The beginning of a preprocessing directive.  We don't know what
           token follows the directive, so assume it can be a qualifier. */
        /* delim_does_not_follow = FALSE;  -- already set. */
      } else {
        /* Some other operator, e.g., ";" or "(", so delimiter does not
           follow the token. */
        delim_does_not_follow = TRUE;
      }  /* if */
    } else if (isalpha((unsigned char)ch)) {
      /* This might be a macro call, so we can't tell. */
    } else {
      /* Anything else (e.g., a constant), so the delimiter does not appear. */
      delim_does_not_follow = TRUE;
    }  /* if */
  }  /* if */
done:
  return delim_does_not_follow;
}  /* qualifier_delimiter_does_not_follow_token */


static a_symbol_ptr select_dual_lookup_symbol(
					a_symbol_ptr	normal_fund_sym,
					a_symbol_ptr	normal_sym,
					a_symbol_ptr	class_fund_sym,
					a_symbol_ptr	class_sym,
					a_boolean	might_be_template)
/*
normal_fund_sym and class_fund_sym are the results of a normal and
class-qualified ID lookup, respectively.  normal_sym and class_sym are
the symbols placed in the specific_symbol field by those lookups, and
which may point to projection symbols.  might_be_template is TRUE if
the name being looked up is followed by a "<".  Reconcile the two
symbols according to the rules for the dual lookup, issue any
diagnostics that might be needed, and return the symbol to be used.
Set the specific symbol to the associated nonfundamental symbol.
*/
{
  a_symbol_ptr	result_sym;
  a_symbol_ptr	specific_symbol;

  /* This implements the special handling in 3.4.5 (basic.lookup.classref)
     of possible template names in class member access expressions
     (e.g., "p->f<...").  If the name is found in the class, the class
     symbol should be used unless it is a class template, in which case
     the normal symbol can also be considered if it is also a class
     template. */
  if (normal_sym != NULL && class_sym != NULL && might_be_template) {
    if (is_template_symbol(class_fund_sym)) {
      /* When the identifier is followed by a "<", and the name is found
         as a template in the class, ignore the other symbol unless it is a
         class template. */
      if (normal_sym != NULL &&
          !is_class_template_or_injected_template_symbol(normal_fund_sym)) {
        normal_sym = NULL;
      }  /* if */
    } else if (is_constructor_symbol(class_sym)) {
      /* The class symbol is the constructor.  Ignore this for purposes of
         this lookup, because the constructor cannot be referenced by name
         in this context. */
      class_sym = NULL;
    } else if (class_fund_sym->is_nonreal_member &&
               !is_template_symbol(class_fund_sym) &&
               is_template_or_injected_template_symbol(normal_fund_sym)) {
      /* The class symbols is a nonreal nontemplate and the normal symbol
         is a template.  Use the normal symbol. */
      class_sym = NULL;
    } else {
      /* The name is a member of the class that is not a template.  Use that
         name and ignore the normal lookup name. */
      normal_sym = NULL;
    }  /* if */
  }  /* if */
  if (normal_sym != NULL && class_sym != NULL) {
    /* There are two symbols -- see if they are equivalent.  If the
       normal symbol refers to a class and the class symbol refers to the
       constructor, they are considered equivalent. */
    a_boolean	equiv_symbols = FALSE;
    a_boolean	use_normal_if_equiv = TRUE;
    if (normal_fund_sym == class_fund_sym) {
      equiv_symbols = TRUE;
    } else if (is_class_symbol(normal_fund_sym) &&
               is_injected_class_symbol(class_fund_sym)) {
      /* The normal symbol is a class and the class symbol is an injected
         class name.  They are equivalent if they refer to the same type. */
      equiv_symbols = identical_types(type_symbol_type(normal_fund_sym),
                                      class_fund_sym->variant.type.ptr);
    } else if (is_class_template_symbol(normal_fund_sym) &&
               is_injected_template_symbol(class_fund_sym)) {
      /* The normal symbol is a class template and the class symbol is an
         injected template name.  They are equivalent if the template
         associated with the injected name is the same as the class template.
         If they are equivalent, use the class symbol because it is the one
         for which a following template argument list is optional. */
      equiv_symbols = normal_sym ==
                   class_template_for_injected_template_symbol(class_fund_sym);
      use_normal_if_equiv = FALSE;
    } else if (is_constructor_symbol(class_fund_sym) &&
               is_class_symbol(normal_fund_sym)) {
      a_type_ptr	normal_type;
      normal_type = type_symbol_type(normal_fund_sym);
      if (identical_types(normal_type, class_fund_sym->parent.class_type)) {
        equiv_symbols = TRUE;
      }  /* if */
    } else if (class_fund_sym->is_nonreal_member) {
      /* The class symbol is nonreal.  Use the normal symbol.  During the
         real instantiation, any class symbol that is found is required to
         refer to the same type as the one found by the normal lookup,
         so we can safely use the normal symbol here and issue an error
         during the real instantiation if necessary. */
      equiv_symbols = TRUE;
    }  /* if */
    if (equiv_symbols) {
      /* The symbols are equivalent.  Use the normal symbol unless otherwise
         specified. */
      if (use_normal_if_equiv) {
        result_sym = normal_fund_sym;
        specific_symbol = normal_sym;
      } else {
        result_sym = class_fund_sym;
        specific_symbol = class_sym;
      }  /* if */
    } else {
      /* The symbols are not equivalent.  Issue a diagnostic.
         When the name is followed by a "::" (when might_be_template
         is FALSE) the normal lookup symbol is used to duplicate the behavior
         that existed before the dual lookup was implemented.  When the name
         is followed by a "<" (might_be_template is TRUE), the class symbol
         is preferred.  This is also done for compatibility to prevent
         something like "p->f < 1" from breaking when a global template "f"
         is declared. */
      an_error_severity	severity;
      a_symbol_ptr	sym_to_use;
      a_symbol_ptr	diag_sym_to_use;
      a_symbol_ptr	sym_to_ignore;
      a_symbol_ptr	diag_class_sym;
      /* If the class symbol is a constructor, use the class symbol in place
         of the constructor symbol for error reporting. */
      if (is_constructor_symbol(class_fund_sym)) {
        diag_class_sym = (a_symbol_ptr)class_fund_sym->
                                  parent.class_type->source_corresp.assoc_info;
      } else {
        diag_class_sym = class_fund_sym;
      }  /* if */
      if (might_be_template) {
        sym_to_use = class_fund_sym;
        diag_sym_to_use = diag_class_sym;
        specific_symbol = class_sym;
        sym_to_ignore = normal_fund_sym;
        severity = es_warning;
      } else {
        sym_to_use = normal_fund_sym;
        diag_sym_to_use = sym_to_use;
        specific_symbol = normal_sym;
        sym_to_ignore = diag_class_sym;
        severity = strict_ansi_error_severity;
      }  /* if */
      pos_sy2_diagnostic(severity, ec_dual_lookup_ambiguous_name,
                         &error_position, diag_sym_to_use, sym_to_ignore);
      result_sym = sym_to_use;
    }  /* if */
  } else {
    /* One or both are NULL.  If one is non-NULL, return that one, otherwise
       return a NULL. */
    if (normal_sym != NULL) {
      result_sym = normal_fund_sym;
      specific_symbol = normal_sym;
    } else {
      result_sym = class_fund_sym;
      specific_symbol = class_sym;
    }  /* if */
  }  /* if */
  locator_for_curr_id.specific_symbol = specific_symbol;
  return result_sym;
}  /* select_dual_lookup_symbol */


static a_boolean is_microsoft_qualifier_start(a_symbol_ptr	sym)
/*
In a construct like "X::Y", the Microsoft compiler does not consider this
to be a qualification of X when X is a typedef to a non-class type.
In other words, this is treated as "X" and "::Y" by the Microsoft
compiler.
*/
{
  a_boolean	result = TRUE;

  if (sym != NULL && sym->kind == (a_symbol_kind)sk_type) {
    a_type_ptr	tp;
    tp = sym->variant.type.ptr;
    tp = skip_typerefs(tp);
    /* A class, struct, union, or template parameter type is
       permitted as the start of a qualified name. */
    switch (tp->kind) {
      case tk_class:
      case tk_struct:
      case tk_union:
      case tk_template_param:
        break;
      case tk_enum:
        result = tp->source_corresp.is_class_member;
        break;
      default:
        result = FALSE;
        break;
    }  /* switch */
  }  /* if */
  return result;
}  /* is_microsoft_qualifier_start */


static a_symbol_ptr look_up_qualifier_start(
			an_id_lookup_options_set	lookup_kind,
			a_type_ptr			class_type,
			a_boolean			might_be_vacuous_dtor,
			a_boolean			*is_vacuous_dtor,
			a_boolean			might_be_template)
/*
This routine does the "dual lookup" that is done in contexts such as
the "A" in "p->A::B" and "f" in "p->f<...>...".  This involves looking
the name up using a "normal" lookup and also looking it up in the class
type of the left operand of the "." or "->".  might_be_template is TRUE
if the token after the identifier is a "<".  might_be_vacuous_dtor is
TRUE if the name being looked up is followed by "::~", and a vacuous
destructor is valid in the current context.  *is_vacuous_dtor is set to
TRUE if a symbol that can only be a vacuous destructor is returned.
*/
{
  a_symbol_ptr	normal_sym;
  a_symbol_ptr	class_sym;
  a_symbol_ptr	normal_fund_sym;
  a_symbol_ptr	class_fund_sym;
  a_symbol_ptr	sym;
  a_boolean	do_class_lookup;

  /* Do the lookup if a type was provided that is a class type that is
     either complete or in the process of being defined.  Also do the
     lookup if the class is nonreal. */
  do_class_lookup = class_type != NULL &&
                    is_class_struct_union_type(class_type) &&
                    (class_type->variant.class_struct_union.
                                         extra_info->assoc_scope != NULL ||
                     class_type->variant.class_struct_union.is_nonreal_class);
  /* Only get normal_sym from the locator if a fundamental symbol was
     returned by the lookup.  The specific symbol in the locator could
     be non-NULL in error cases. */ 
  normal_fund_sym = normal_id_lookup(&locator_for_curr_id, lookup_kind);
  normal_sym = normal_fund_sym == NULL ? NULL
                                       : locator_for_curr_id.specific_symbol;
  if (do_class_lookup) {
    clear_specific_symbol(locator_for_curr_id);
    /* These lookups are speculative -- don't create projection symbols for
       them. */
    lookup_kind |= IDL_DO_NOT_CREATE_PROJ_SYM;
    /* Only get class_sym from the locator if a fundamental symbol was
       returned by the lookup.  The specific symbol in the locator could
       be non-NULL in error cases. */
    class_fund_sym = class_qualified_id_lookup(&locator_for_curr_id,
                                               class_type, lookup_kind);
    class_sym = class_fund_sym == NULL ? NULL
                                       : locator_for_curr_id.specific_symbol;
    sym = select_dual_lookup_symbol(normal_fund_sym, normal_sym, 
                                    class_fund_sym, class_sym,
                                    might_be_template);
  } else {
    sym = normal_fund_sym;
  }  /* if */
  if (microsoft_bugs && sym != NULL &&
      is_enum_symbol(sym) && might_be_vacuous_dtor) {
    /* The "must be class or namespace" lookup can return an enumeration
       in Microsoft bugs mode.  If it does, and if we are processing what
       might be a vacuous destructor, indicate that it is a vacuous
       destructor. */
    *is_vacuous_dtor = TRUE;
  }  /* if */
  if (sym == NULL && (might_be_vacuous_dtor || microsoft_bugs)) {
    /* The lookup has failed so far.  If this might be a vacuous destructor,
       do a more general lookup to find a nonclass type that might be
       used as a qualifier for a vacuous destructor.  The more general lookup
       is also done in Microsoft bugs mode.  The Microsoft compiler does not
       consider an identifier to start a qualified name if this lookup finds
       a type that is not a class, enum, or template parameter. */
    normal_fund_sym = normal_id_lookup(&locator_for_curr_id, IDL_NO_OPTIONS);
    normal_sym = locator_for_curr_id.specific_symbol;
    if (do_class_lookup) {
      clear_specific_symbol(locator_for_curr_id);
      class_fund_sym = class_qualified_id_lookup(&locator_for_curr_id,
                                                 class_type,
                                                 IDL_NO_OPTIONS);
      class_sym = locator_for_curr_id.specific_symbol;
      sym = select_dual_lookup_symbol(normal_fund_sym, normal_sym,
                                      class_fund_sym, class_sym,
                                      might_be_template);
    } else {
      sym = normal_fund_sym;
    }  /* if */
    if (sym != NULL && !might_be_vacuous_dtor) {
      /* In Microsoft bugs mode, ignore this symbol unless it is one
         that actually does not indicate the start of a qualified name. */
      if (is_microsoft_qualifier_start(sym)) sym = NULL;
    }  /* if */
    *is_vacuous_dtor = might_be_vacuous_dtor;
  }  /* if */
  return sym;
}  /* look_up_qualifier_start */


static a_boolean sym_can_follow_template_keyword(a_symbol_ptr sym)
/*
In a construct like "p->template X<...>" or "p->T::template X<...>",
this routine determines whether the symbol (representing X) is
permitted to follow the template keyword.
*/
{
  a_boolean	result = FALSE;

  /* The symbol must represent a template or an overload set containing
     a template. */
  if (symbol_is_or_contains_template(sym)) {
    if (is_injected_class_symbol(sym)) {
      /* For the purpose of this test, use the "real" symbol for a class in
         place of the injected class symbol. */
      a_type_ptr	tp;
      tp = sym->variant.type.ptr;
      sym = (a_symbol_ptr)tp->source_corresp.assoc_info;
    }  /* if */
    /* The symbol must name a member template. */
    if (sym->is_class_member) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* sym_can_follow_template_keyword */


/*
Macro that calls qualifier_delimiter_does_not_follow_token to determine
whether the next token may be one of the tokens that
f_is_generalized_identifier_start needs to inspect and, if so, calls
next_two_tokens;  Otherwise, second_token is set to tok_error and tok_error
is returned.
*/
#define next_two_tokens_if_qualifier_delimiter(separator, second_token)	\
  (qualifier_delimiter_does_not_follow_token() ?			\
    (*(second_token) = tok_error), tok_error :				\
    next_two_tokens(separator, second_token))


a_boolean f_is_generalized_identifier_start(
			an_identifier_options_set	options,
			a_type_ptr			field_sel_type)
/*
Determine whether the current token is the start of a "generalized
identifier" -- a qualified name, identifier, operator name, conversion
name, or destructor name.  If the token does begin an identifier, the
tokens that make up the identifier are coalesced into a single token.
This consists of scanning the tokens that make up the identifier,
retaining the information conveyed by the tokens in the symbol
locator, setting curr_token to tok_identifier, and setting
pos_curr_token to the position of the first token in the sequence.
We return TRUE if an identifier was found, otherwise we return FALSE.

Pointer to members are coalesced into a tok_ptr_to_member. We return
FALSE for pointer to members.

Returns TRUE and sets curr_token to tok_identifier for the
following cases:
	
	::i
	X::i
	::X::i
	::A::B::i
	A<int>::i
	A::operator =
	A::operator int
	A::~A()	
	A:: ... anything except * ...
	NS::i
	NS::A::i
	i
	operator =
	operator int
	~A		When options & GID_DTOR_RECOGNIZED = TRUE
	A<int>		Template reference will be coalesced
	NS::A<int>	Template reference will be coalesced
        int::~int	When options & GID_VACUOUS_DTOR_RECOGNIZED = TRUE

Returns FALSE and sets curr_token to tok_ptr_to_member for:

	X::*
	A<int>::*	Template reference will be coalesced

Returns FALSE and leaves curr_token_unchanged for:

	::new
	::delete
	~name		When options & GID_DTOR_RECOGNIZED = FALSE
	anything else

When the GID_TEMPLATE_ARGS_OPTIONAL flag is set in "options" the 
template argument list (e.g., "<int>") can be omitted.  This flag
is passed to coalesce_template_class_reference.

The "options" parameter allows the caller to specify the types of
identifiers to be recognized and/or allowed.

When the GID_VACUOUS_DTOR_RECOGNIZED flag is set in "options" a qualified
destructor name will be recognized for non-class types and for class
types that have no destructors.  This is used to handle
constructs such as "p->int::~int".  Note that the parent.class_type field
in the locator normally contains the type of the qualifier portion of
a qualified name.  For nonclass vacuous destructors, however, it contains the
type of the thing after the "::~".  This is necessary because some vacuous
destructors may not have a qualifier and the type information is still
needed by the caller in this case.  So qualifier_type starts out with the
qualifier type and is updated by the vacuous destructor code to contain
the type of the destructor name following the "::~".  This really only
matters for error handling because in nonerror cases the two types
will be the same.

If the token following a qualifier is not part of a valid identifier
we still return TRUE so that an appropriate diagnostic can be generated when
an attempt is made to use the thing after the qualifier.

This routine performs ambiguity and access checking on the components of the
qualified name.

field_sel_type is NULL except when scanning the right operand of a field
selection operator, in which case it points to the type of the left operand.
*/
{
  a_type_ptr			qualifier_type = NULL;
  a_boolean     		is_file_scope_qualified_name = FALSE;
  a_boolean			is_global_qualified_name = FALSE;
  a_boolean     		is_qualified_name = FALSE;
  a_boolean             	is_ptr_to_member = FALSE;
  a_boolean			is_identifier = FALSE;
  a_symbol_ptr			qualifier_sym = NULL;
  a_symbol_ptr			specific_sym = NULL;
  a_source_position		start_position;
  a_source_position		orig_error_position;
  a_token_kind			next_tok;
  a_token_kind			next_tok_2;
  a_boolean			result = FALSE;
  a_boolean			err = FALSE;
  a_boolean			can_be_vacuous_dtor =
				  (options & GID_VACUOUS_DTOR_RECOGNIZED);
  a_boolean			dtor_must_be_nonclass =
				  (options & GID_DTOR_MUST_BE_NONCLASS);
  a_boolean			is_vacuous_dtor = FALSE;
  a_boolean			is_nonclass_dtor = FALSE;
  a_type_ptr			dtor_class_type = NULL;
  a_type_ptr			dtor_type = NULL;
  a_source_position		tilde_position;
  a_boolean            		might_be_qualifier;
  a_token_kind         		qualifier_separator = tok_colon_colon;
  a_boolean                     qualifier_is_type = TRUE;
  a_boolean                     qualifier_is_enum = FALSE;
  a_boolean			qualifier_type_is_class = FALSE;
  a_namespace_ptr		qualifier_namespace;
  a_token_sequence_number	start_seq_number;
  a_boolean			follows_template;

  db_enter(4, "f_is_generalized_identifier_start");
  /* If the current token is an identifier, then check the flag in the
     locator to see if it has already been coalesced.  If so, simply
     return TRUE with no further processing.  If the current token is a
     tok_ptr_to_member, return FALSE with no further processing. */
  if (curr_token == tok_identifier &&
      locator_for_curr_id.has_been_coalesced) {
    result = TRUE;
    goto exit;
  } else if (curr_token == tok_ptr_to_member) {
    result = FALSE;
    goto exit;
  }  /* if */
  start_position = pos_curr_token;
  start_seq_number = curr_token_sequence_number;
  orig_error_position = error_position;
  if (C_dialect != C_dialect_cplusplus) {
    /* Skip qualifier, destructor, and operator processing if not in C++
       mode.  If we have an identifier go to the code that updates the
       locator.  If we don't have an identifier, simply return. */
    result = curr_token == tok_identifier;
    if (result) goto wrapup;
    goto exit;
  }  /* if */
  follows_template = (options & GID_FOLLOWS_TEMPLATE) != 0;
  /* Look for a leading unary "::".  Don't be fooled by "::new" and
     "::delete".  Don't treat ::* as a pointer to member declarator.
     ::* would be rejected below as a pointer to member declarator, but
     doing so here provides better error recovery. */
  if (curr_token == tok_colon_colon && !is_global_new_or_delete() &&
      next_token() != tok_star) {
    is_global_qualified_name = TRUE;
    is_file_scope_qualified_name = TRUE;
    is_qualified_name = TRUE;
    if (follows_template) {
      /* "p->template ::..." is not valid. */
      error(ec_exp_identifier);
      follows_template = FALSE;
    }  /* if */
    (void)get_token();
  }  /* if */
  if (curr_token == tok_operator) {
    get_opname(/*is_class_member=*/FALSE,
               (a_parent_class_or_namespace*)NULL);
  }  /* if */
  /* For the next token to be part of the qualifier it must be a class name
     followed by "::".  Templates make it more difficult to detect this
     situation so we accept an identifier followed by either a "::" or a
     left angle bracket.  A vacuous destructor reference can also begin
     with an identifier that is a type name or a token that begins
     a simple type name.  The function "type_keyword" will detect
     a token that begins a simple type.  We will check later to determine
     whether the identifier is a class name or a type name, if needed.  */
  might_be_qualifier = FALSE;
  if (curr_token == tok_identifier) {
    next_tok = next_two_tokens_if_qualifier_delimiter(tok_colon_colon,
                                                      &next_tok_2);
    if (next_tok == tok_colon_colon || next_tok == tok_lt ||
        follows_template) {
      might_be_qualifier = TRUE;
    } else if (cfront_2_1_mode && next_tok == tok_period &&
               !(options & GID_IS_FIELD_SELECTION_OPERAND)) {
      /* Check for the anachronism of allowing a "." as a qualifier separator
         where a "::" should be used.  This is only done in cfront mode
         because this is something that cfront labels as an anachronism but
         is not in the ARM list of anachronisms.  Note that when using the
         "." notation, you must use "." at all levels of qualification
         except global.  That is, you must say A.B.C not A.B::C or A::B.C.
         Also "." qualifiers are not supported for vacuous destructor
         references or template references.  We can't tell yet whether this
         is a qualified name or simply a normal field reference.  We'll
         assume this is a qualifier for now and make a final decision after
         we try to look up the identifier.  A warning will be issued,
         if appropriate, after the lookup is done.  The "." may not
         be used as a qualifier separator in a field selection operator. */
      might_be_qualifier = TRUE;
      qualifier_separator = tok_period;
    }  /* if */
  } else if (dtor_must_be_nonclass) {
    dtor_class_type = type_keyword();
    if (dtor_class_type != NULL) {
      next_tok = next_two_tokens_if_qualifier_delimiter(tok_colon_colon,
                                                        &next_tok_2);
      if (next_tok == tok_colon_colon && next_tok_2 == tok_compl) {
        might_be_qualifier = TRUE;
        if (strict_ansi_mode) {
          /* A vacuous destructor reference is no longer permitted to
             use a type keyword, only a typedef name. */
          error(ec_exp_identifier);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (might_be_qualifier) {
    /* Look up the identifier to see if it could be a class name.  Note that
       we don't consider the normal eclipsing rules.  A class or namespace
       can be found even when hidden by something else:
         class A {int i;};
         int f() {
           int A;
           A::i = 1;   // The class A is found.
         }
       
       If the name is not found, and vacuous destructor references are
       recognized, and the token following the "::" is a tilde, we repeat
       the lookup without the restriction that the name must be a class
       or namespace name.
    */
    if (dtor_class_type != NULL) {
      /* This looks like a vacuous destructor reference.  We may change
         this later if we don't find the right name following the "::". */
      is_vacuous_dtor = TRUE;
      qualifier_sym = NULL;
    } else {
      an_id_lookup_options_set	lookup_kind;
      a_boolean			might_be_vacuous_dtor;
      /* A normal qualified name or a vacuous destructor reference that
         begins with a normal qualified name (e.g., A::B::T::~T).
         If a vacuous destructor reference is allowed and the token
         following the "::" is a tilde, then the identifier we are looking
         up doesn't have to be a class name.  If the class lookup fails,
         do another lookup without the requirement that a class be found. */
      might_be_vacuous_dtor = next_tok_2 == tok_compl && can_be_vacuous_dtor;
      /* The lookup of a class name in a qualified name is done as a
         "must be class (or namespace)" lookup.  If, however, the name
         being scanned is followed by a "<" we don't yet know whether this
         is a template reference or simply a less than sign.  We must
         assume it could be a less than sign and do a normal
         (nonclass) lookup.  Furthermore, this is done as a "tentative type"
         lookup so that, should a template not be found, a projection symbol
         is not created.  This should not make any difference for
         file scope lookups because class template names cannot
         coexist with other names at file scope.  A normal lookup must
         also be done when scanning what might be a use of a "." in
         place of "::" as a qualifier.  We don't know whether the "."
         is being used as a qualifier or as a field selection operator
         so we need to do a normal lookup and then decide based on the
         type of the thing we find. */
      if (follows_template) {
        lookup_kind = IDL_TREAT_AS_TEMPLATE_ID |
                      IDL_TENTATIVE_TEMPLATE_LOOKUP;
      } else if (next_tok == tok_lt) {
        lookup_kind = IDL_TENTATIVE_TEMPLATE_LOOKUP;
      } else if (qualifier_separator == tok_period) {
        lookup_kind = IDL_NO_OPTIONS;
      } else {
        lookup_kind = IDL_MUST_BE_CLASS_OR_NAMESPACE;
      }  /* if */
      if (is_global_qualified_name) {
        /* There was a leading unary "::", so look up the name in the file
           scope. */
        qualifier_sym = file_scope_id_lookup(il_header.primary_scope,
					     &locator_for_curr_id,
                                             lookup_kind);
        if (qualifier_sym == NULL && might_be_vacuous_dtor) {
          qualifier_sym = file_scope_id_lookup(il_header.primary_scope,
					       &locator_for_curr_id,
                                               IDL_NO_OPTIONS);
          is_vacuous_dtor = TRUE;
        }  /* if */
      } else {
        /* Usual case (no leading "::").  Look up the name in both the
           current context (i.e., a normal lookup) and in the class type
           of the field selection operator, if any. */
        qualifier_sym = look_up_qualifier_start(
                                   lookup_kind, field_sel_type,
                                   might_be_vacuous_dtor, &is_vacuous_dtor,
				   /*might_be_template=*/next_tok == tok_lt ||
                                                         follows_template);
        if (locator_for_curr_id.is_semivisible_nested_type) {
          /* The symbol in the locator is a nested class that is not visible
             according to the ARM lookup rules but is returned in support of
             the nested class anachronism (ARM 18.3.5). Issue an anachronism
             diagnostic. */
          sym_diagnostic(anachronism_error_severity,
                         ec_nested_class_anachronism,
                         locator_for_curr_id.specific_symbol);
        }  /* if */
        /* See if this name is ambiguous.  This is done here because
	   the specific symbol may be cleared and set to a different
	   value below, and the ambiguity and access check must be
	   done based on the projection symbol, not on the symbol
	   pointed to by the projection symbol.  An ambiguous injected
	   template symbol is accepted if it unambiguously refers to a
	   specific class template, and if the next token is a "<"
	   indicating that we are really referring to the template and
	   not the (ambiguous) class type. */
        check_ambiguity_and_access_with_template_flag(
                        &locator_for_curr_id, (a_boolean)(next_tok == tok_lt));
        /* The call above will create an error locator if an ambiguity is
           is detected. */
        if (is_error_locator(locator_for_curr_id)) {
          make_specific_symbol_error_locator(&locator_for_curr_id);
          qualifier_sym = locator_for_curr_id.specific_symbol;
          err = TRUE;
        }  /* if */
      }  /* if */
      if (follows_template && qualifier_sym != NULL &&
          next_tok == tok_lt &&
          !sym_can_follow_template_keyword(qualifier_sym)) {
        /* A construct like "p->template X< ...".  When the template keyword
           is so used, "X" must be a member template.   The test of next_tok
               is used to suppress this error if we already complained
               about a missing template argument list.*/
        diagnostic(strict_ansi_discretionary_severity,
                   ec_invalid_name_after_template);
      }  /* if */
      if (qualifier_separator == tok_period) {
        if (qualifier_sym != NULL && is_class_symbol(qualifier_sym)) {
          /* In cfront mode we have a construct like "A." where A is a
             class name.  This is a use of a cfront anachronism where "."
             is used in a qualified name where "::" should be used.
             Issue a warning. */
          warning(ec_period_used_as_qualifier);
        } else {
          /* In cfront mode we have found a construct like "A." and A is not
             a class name.  What we have is probably just a normal field
             reference.  Reset the qualifier_separator so that we don't
             try to process this as a qualified name. */
          qualifier_separator = tok_colon_colon;
        }  /* if */
      }  /* if */
      /* If we think we have a vacuous destructor reference, make sure the
         symbol found is a type.  An error will be issued below. */
      if (is_vacuous_dtor && qualifier_sym != NULL &&
          !is_type_symbol(qualifier_sym)) {
        qualifier_sym = NULL;
      }  /* if */
    }  /* if */
    /* Clear the specific symbol field which may have been set by the lookups
       performed above.  It must be cleared in case this isn't actually a
       qualified name.  We clear it now because it may be set again if a
       template reference is coalesced and we don't want to lose that value. */
    specific_sym = locator_for_curr_id.specific_symbol;
    clear_specific_symbol(locator_for_curr_id);
    /* If the class symbol is for a class template, process the argument
       list. */
    if ((qualifier_sym != NULL &&
         is_class_template_or_injected_template_symbol(qualifier_sym)) ||
        next_tok == tok_lt || follows_template) {
      /* Process a template reference.  This is considered a potential
         template reference if the symbol points to a class template
         or if the next token is a "<" (which could be a function template
         reference or an error case). */
      qualifier_sym = coalesce_template_id(qualifier_sym, next_tok, options,
                                           &err);
      specific_sym = locator_for_curr_id.specific_symbol;
    }  /* if */
    /* See if the identifier is followed by "::".  Note that nex_tok is not
       used because the next token may have changed while scanning a
       template argument list. */
    if (dtor_class_type != NULL) {
      /* We have a vacuous destructor reference of the form "int::~...".
         Skip of the code in the "else" clause that processing the
         rest of the class qualifier. */
      qualifier_type = dtor_class_type;
      qualifier_is_type = TRUE;
      qualifier_type_is_class = FALSE;
      (void)get_token();  /* Gets the type name. */
      (void)get_token();  /* The "::" that follows the type name. */
      is_qualified_name = TRUE;
    } else if (next_token() == qualifier_separator &&
               (!microsoft_bugs || is_vacuous_dtor ||
                is_microsoft_qualifier_start(qualifier_sym))) {
      /* This is an identifier followed by the qualifier separator
         (usually something like "X::").  Scan the qualified name. */
      a_source_position type_position;
      type_position = start_position;
      /* This is a qualifier. */
      is_qualified_name = TRUE;
      if (!is_vacuous_dtor) is_file_scope_qualified_name = FALSE;
      /* Restore the specific_symbol with the class symbol determined earlier.
         This needs to be restored so that access and ambiguity checking can
	 be done. */
      locator_for_curr_id.specific_symbol = specific_sym;
      for (;;) {
        /* Keep looping while there are more levels of class qualification.
           Exit from loop is in the middle. */
        a_boolean	is_template = FALSE;
        a_symbol_ptr	prev_qualifier_sym = qualifier_sym;
        a_boolean	invalid_qualifier_sym = FALSE;
        if (qualifier_sym == NULL || err) {
          invalid_qualifier_sym = TRUE;
        } else if (qualifier_sym->is_error) {
          invalid_qualifier_sym = TRUE;
          err = TRUE;
        } else if (qualifier_sym->kind == (a_symbol_kind)sk_class_template) {
          invalid_qualifier_sym = TRUE;
        } else if (is_class_symbol(qualifier_sym)) {
          /* Get the type associated with the class symbol. */
          a_type_ptr	qualifier_sym_type;
          qualifier_sym_type = type_symbol_type(qualifier_sym);
          qualifier_type = skip_typerefs(qualifier_sym_type);
          qualifier_is_type = TRUE;
          qualifier_type_is_class = TRUE;
        } else if (is_namespace_symbol(qualifier_sym)) {
          /* Get the namespace from the symbol entry. */
          qualifier_namespace = namespace_symbol_namespace(qualifier_sym);
          qualifier_is_type = FALSE;
        } else if (microsoft_bugs && is_enum_symbol(qualifier_sym) &&
                   skip_typerefs(type_symbol_type(qualifier_sym))->
                                              source_corresp.is_class_member) {
          /* In Microsoft bugs mode the qualifier can be an enumeration
             name.  Only member enumerations are considered. */
          qualifier_type = type_symbol_type(qualifier_sym);
          qualifier_type = skip_typerefs(qualifier_type);
          qualifier_is_type = TRUE;
          qualifier_type_is_class = FALSE;
          qualifier_is_enum = TRUE;
        } else if (qualifier_sym->kind == (a_symbol_kind)sk_type ||
                   (qualifier_sym->kind == (a_symbol_kind)sk_enum_tag &&
                    is_vacuous_dtor)) {
            /* The class symbol points to a type.  This is the case when
               a class qualifier contains template parameter types or for
               the last qualifier of a vacuous destructor.  Set
               class type to the type pointed to. */
            qualifier_type = type_symbol_type(qualifier_sym);
            if (!is_vacuous_dtor) {
              /* Qualifiers need to be preserved for vacuous destructors
                 because the type must match the type specified for the
                 destructor name. */
              qualifier_type = skip_typerefs(qualifier_type);
            }  /* if */
            qualifier_is_type = TRUE;
            qualifier_type_is_class = FALSE;
            check_assertion(is_template_param_type(qualifier_type) ||
                            is_vacuous_dtor);
        } else {
          /* Some other kind of symbol -- must be an error. */
          invalid_qualifier_sym = TRUE;
        }  /* if */
        if (invalid_qualifier_sym) {
          /* The identifier is followed by a "::" but is not a class symbol. */
          if (!err) {
            if (is_vacuous_dtor) {
              error(ec_id_must_be_class_or_type_name);
            } else {
              error(ec_id_must_be_class_or_namespace_name);
            }  /* if */
            err = TRUE;
          }  /* if */
          qualifier_is_type = TRUE;
          qualifier_type = NULL;
          qualifier_type_is_class = FALSE;
        } else {
          /* The qualifier symbol is valid. Record the reference on the
             symbol. */
          mark_referenced(qualifier_sym, &locator_for_curr_id.source_position);
        }  /* if */
        /* Skip over the class-name, and the "::".  After the two get_token
           calls, the current token will be whatever follows the
           qualifier. */
        (void)get_token();
        (void)get_token();
        if (curr_token == tok_template) {
          is_template = TRUE;
          (void)get_token();
        }  /* if */
        /* If the current token begins an operator name, then coalesce
           the operator. */
        if (curr_token == tok_operator) {
          /* get_opname requires a parent class or namespace pointer.
             Construct one for the qualifier that has been scanned. */
          a_parent_class_or_namespace parent;
          if (qualifier_is_type) {
            parent.class_type = qualifier_type;
          } else {
            parent.namespace_ptr = qualifier_namespace;
          }  /* if */
          get_opname(qualifier_is_type, &parent);
        }  /* if */
        next_tok = next_two_tokens_if_qualifier_delimiter
                                            (qualifier_separator, &next_tok_2);
        if (curr_token != tok_identifier ||
            (next_tok != qualifier_separator && next_tok != tok_lt &&
             !is_template)) {
          /* Not an identifier followed by "::" or "<", so end the loop. */
          break;
        }  /* if */
        /* There is another level of qualification.  Search for the identifier
           in the given scope.  Once again, if vacuous destructor references
           are allowed we may need to repeat the lookup without the
           requirement that a class be found. */
        if (!err) {

          a_boolean	might_be_vacuous_dtor = next_tok_2 == tok_compl &&
                                                can_be_vacuous_dtor;
          if (qualifier_is_type && qualifier_type_is_class) {
            /* Make sure that this class has been instantiated. */
            complete_class_type_is_needed(qualifier_type);
          }  /* if */
          /* Make sure that the class type is a complete type. */
          if (qualifier_is_type && qualifier_type_is_class &&
              is_incomplete_type(qualifier_type) &&
              qualifier_type->variant.class_struct_union.
                                         extra_info->assoc_scope == NULL) {
            /* If the type is incomplete we also check whether the type is
	       currently being defined -- it is considered complete if it
	       is being defined.  We determine this by checking the
	       assoc_scope field of the class type supplement. */
            pos_error(ec_incomplete_type_not_allowed, &type_position);
	    err = TRUE;
	    qualifier_sym = NULL;
          } else {
            an_id_lookup_options_set	lookup_options;
            /* The lookup of a class name in a qualified name is done as a
               "must be class (or namespace)" lookup.  If, however, the name
               being scanned is followed by a "<" we don't yet know whether
               this is a template reference or simply a less than sign.
               We must assume it could be a less than sign and do a normal
               (nonclass) lookup. */
            lookup_options = next_tok == tok_lt || is_template
                                   ? IDL_TENTATIVE_TEMPLATE_LOOKUP
                                   : IDL_MUST_BE_CLASS_OR_NAMESPACE;
            /* If the identifier was preceded by "template", the name can be
               assumed to be a template. */
            if (is_template) {
              lookup_options |= IDL_TREAT_AS_TEMPLATE_ID |
                                IDL_TENTATIVE_TEMPLATE_LOOKUP;
            }  /* if */
            if ((options & GID_IS_TYPENAME) != 0) {
              /* If this name followed the typename keyword, indicate that the
                 name found must be a type.  This affects creation of members
                 of proxy classes. A "<" seen in a qualified name that follows
                 "typename" is considered to be the start of a template
                 argument list. */
              lookup_options |= IDL_TYPENAME_LOOKUP;
              if ((next_tok == tok_lt || is_template) &&
                  implicit_typename_enabled) {
                lookup_options |= IDL_TREAT_AS_TEMPLATE_ID;
              }  /* if */
            } else if (implicit_typename_enabled &&
                       (options & GID_IS_EXPR_CONTEXT) == 0) {
              /* If this is a name being used in a declarative context (i.e.,
                 not in an expression, and we are in implicit typename mode,
                 and the name is followed by a "<", set the "treat as template
                 ID" flag to indicate that if a nonreal class member needs
                 to be created, it should be created as a template name. */
              if (next_tok == tok_lt || is_template) {
                lookup_options |= IDL_TREAT_AS_TEMPLATE_ID;
              }  /* if */
            } else if ((options & GID_IS_EXPR_CONTEXT) != 0) {
              /* Pass in a special flag for expression contexts.  This controls
                 the type of nonreal class member created. */
              lookup_options |= IDL_IS_EXPR_CONTEXT;
            }  /* if */
            if (qualifier_is_type) {
              if (qualifier_is_enum) {
              /* In Microsoft mode, an enumeration can be used as the
                   qualifier in a qualified name.  Look up the name in the
                   enumeration. */
                qualifier_sym = enum_qualified_id_lookup(&locator_for_curr_id,
							 qualifier_type);
              } else {
                  /* Look up the name in the class specified by the qualifier
                   that has been scanned so far. */
                qualifier_sym = class_qualified_id_lookup
                                         (&locator_for_curr_id, qualifier_type,
                                          lookup_options);
                /* If the class lookup fails, and a vacuous destructor is
                   allowed, do another lookup without the requirement that
                   a class be found. */
                if (qualifier_sym == NULL && might_be_vacuous_dtor) {
                  qualifier_sym = class_qualified_id_lookup(
                                          &locator_for_curr_id, qualifier_type,
                                          IDL_NO_OPTIONS);
                  is_vacuous_dtor = TRUE;
                  if (qualifier_sym != NULL &&
                      !is_type_symbol(qualifier_sym)) {
                    qualifier_sym = NULL;
                  }  /* if */
                }  /* if */
              }  /* if */
            } else {
              /* Look up the name in the namespace that has been scanned so
                 far. */
              check_assertion(qualifier_namespace != NULL);
              qualifier_sym = namespace_qualified_id_lookup
                                         (&locator_for_curr_id,
                                          qualifier_namespace,
                                          lookup_options);
              /* If the namespace lookup fails, and a vacuous destructor is
                 allowed, do another lookup without the requirement that
                 a class be found.  This could occur for a vacuous
                 destructor reference of the form i->N::T::~T, where N
                 is a namespace, and T is a typedef in that namespace. */
              if (qualifier_sym == NULL && might_be_vacuous_dtor) {
                qualifier_sym = namespace_qualified_id_lookup
                                                       (&locator_for_curr_id,
                                                        qualifier_namespace,
                                                        IDL_NO_OPTIONS);
                is_vacuous_dtor = TRUE;
                if (qualifier_sym != NULL && !is_type_symbol(qualifier_sym)) {
                  qualifier_sym = NULL;
                }  /* if */
              }  /* if */
            }  /* if */
          }  /* if */
          if (qualifier_sym != NULL) {
            if (qualifier_sym->is_class_member ||
                locator_for_curr_id.specific_symbol->ambiguous) {
              /* Do ambiguity and access control checking on the qualifier
                 symbol.  Access checking is only done for class members
                 to ensure  that the check will be suppressed for template
                 parameters (i.e., the T in T::X).  Access for template
                 parameters should be checked at the point at which the type
                 is used as a template argument.  The routine is also called
                 if the symbol is known to be ambiguous, so that it can
                 report the ambiguity error.  No access checking will be done
                 when an ambiguity error exists. */
              check_ambiguity_and_access_with_template_flag(
                        &locator_for_curr_id, (a_boolean)(next_tok == tok_lt));
              /* The call above will create an error locator if an ambiguity is
                 is detected. */
              if (is_error_locator(locator_for_curr_id)) {
                make_specific_symbol_error_locator(&locator_for_curr_id);
                qualifier_sym = locator_for_curr_id.specific_symbol;
                err = TRUE;
              }  /* if */
            }  /* if */
          }  /* if */
          if (is_template && qualifier_sym != NULL &&
              !err && next_tok == tok_lt &&
              !sym_can_follow_template_keyword(qualifier_sym)) {
            /* A construct like "p->A::template X< ...".  When the template
               keyword is so used, "X" must be a member template. The test of
               next_tok is used to suppress this error if we already complained
               about a missing template argument list. */
            diagnostic(strict_ansi_discretionary_severity,
                       ec_invalid_name_after_template);
          }  /* if */
          if (qualifier_sym != NULL &&
              (is_class_template_or_injected_template_symbol(qualifier_sym) ||
               next_tok == tok_lt || is_template)) {
            /* Process a template reference.  This is considered a potential
               template reference if the symbol points to a class template
               or if the next token is a "<" (which could be a function
               template reference or an error case). */
            qualifier_sym = coalesce_template_id(qualifier_sym, next_tok,
                                                 options, &err);
            /* We can only now determine whether this template reference is
               followed by a "::".  If it is not, break out of the qualifier
               loop. */
            if (next_token() != tok_colon_colon) break;
          } else if (next_tok == tok_lt) {
            /* The qualified name is followed by a "<" but this is not a
               template reference.  Reset qualifier_sym to the value it had
               before the lookup was done.  Clear the specific_symbol
               field in the symbol locator.  Break out of the qualifier
               scanning loop as this must be the final identifier. */
            qualifier_sym = prev_qualifier_sym;
            clear_specific_symbol(locator_for_curr_id);
            break;
          }  /* if */
        }  /* if */
        type_position = pos_curr_token;
      }  /* for */
    }  /* if */
  }  /* if */
  /* Assume we have found an identifier until we discover otherwise. */
  is_identifier = TRUE;
  if (is_qualified_name && qualifier_is_type &&
      !is_file_scope_qualified_name) {
    /* This is a qualifier (but not a file scope qualified name such as
       ::x) -- see if it is a pointer to member. */
    if (curr_token == tok_star) {
      /* We have a pointer to member token (i.e, A::*).  Update the current
         token. */
      is_identifier = FALSE;
      is_ptr_to_member = TRUE;
    }  /* if */
  }  /* if */
  if (is_identifier) {
    /* We still think we have an identifier.  curr_token contains
       either the token following the qualifier or the first token of what
       might be an identifier.  Determine whether it is an identifier. */
    if (is_vacuous_dtor && curr_token != tok_compl) {
      /* We thought we were processing a vacuous destructor because we
         found something unusual in the qualifier, but there is no
         tilde after the qualifier.  Reset the flag. */
      is_vacuous_dtor = FALSE;
    }  /* if */
    if (curr_token == tok_identifier) {
      /* It is an identifier. */
    } else if (curr_token == tok_operator) {
      /* Could be something like either "operator +" or "operator int".
         We don't determine at this point whether this is a legal operator,
         just that it couldn't legally be anything else.  We recognize
         operator names even if they are disallowed by the "options" flags.
         An error will be issued later if needed. */
    } else if (curr_token == tok_compl &&
               ((options & GID_DTOR_RECOGNIZED) ||
               (is_qualified_name && qualifier_is_type))) {
      /* A destructor name (e.g., ~A or A::~A).  Destructor names are
         always recognized after qualifiers.  If not preceded by a qualifier,
         then they are only recognized when GID_DTOR_RECOGNIZED is TRUE. */
     /* If we have already discovered that we have a vacuous destructor
        reference, then it must be a non-class destructor reference
	(e.g., int::~int). */
     is_nonclass_dtor = is_vacuous_dtor;
     if (!is_vacuous_dtor && can_be_vacuous_dtor &&
         (is_qualified_name && qualifier_is_type) &&
         qualifier_type_is_class && !err) {
       /* So far this looks like a normal destructor reference (i.e.,
          the qualified name represents a class, not some other type).
          See if the class has a destructor.  If it does not, this is a
          vacuous reference. */
       check_assertion_str(qualifier_type != NULL,
                           "figis: qualifier_type == NULL");
       if (symbol_supplement_for_class(qualifier_type)->destructor == NULL) {
         is_vacuous_dtor = TRUE;
       }  /* if */
     } else if (dtor_must_be_nonclass && !is_qualified_name) {
       /* A destructor that is not part of a qualified name must be a
          nonclass vacuous destructor when we are in "dtor_must_be_nonclass"
          mode. */
       is_vacuous_dtor = is_nonclass_dtor = TRUE;
     }  /* if */
     tilde_position = pos_curr_token;
    } else if (is_qualified_name) {
      /* A qualifier followed by something invalid.  Proceed as if
         it is an identifier and let an error be diagnosed later when we
         have more information about what is being processed. */
    } else {
      /* Anything else is not the start of an identifier. */
      is_identifier = FALSE;
    }  /* if */
  }  /* if */
  /* This routine has two functions: determining whether the thing
     being scanned is a generalized identifier, pointer to member,
     or something else; and, if it is an identifier, coalescing the
     identifier and storing the information in the locator.  The code
     above performed the first part of the job.  The code below does
     the coalescing now that we know what we are scanning. */
  if (is_ptr_to_member) {
    curr_token = tok_ptr_to_member;
    /* parent.class_type and is_class_member are the only fields of the
       locator that are valid when curr_token is tok_ptr_to_member. */
    locator_for_curr_id.parent.class_type = qualifier_type;
    locator_for_curr_id.is_class_member = TRUE;
    /* Clear the is_template_id flag in the locator in case it was set before
       this was recognized to be ptr-to-member. */
    locator_for_curr_id.is_template_id = FALSE;
    /* Since we're returning a pseudo-token, set pos_curr_token. */
    pos_curr_token = start_position;
    curr_token_sequence_number = start_seq_number;
    /* Restore the original error position. */
    error_position = orig_error_position;
  } else if (is_identifier) {
    /* A possibly qualified identifier. */
    result = TRUE;
    if (is_qualified_name) {
      /* Make sure that the class has been instantiated. */
      if (!err && qualifier_is_type &&
          qualifier_type != NULL && qualifier_type_is_class) {
        complete_class_type_is_needed(qualifier_type);
      }  /* if */
    }  /* if */
    set_err_pos_to_curr_token();
    /* Process the identifier after the optional qualifier.  Coalesce
       multi-token identifiers (such as "operator +").  This is done
       my scanning the tokens of the identifier, updating the locator
       to reflect what was scanned, and setting curr_token to
       tok_identifier.  Most of this processing is actually done by
       get_destructor_name and get_opname.  */
    /* Check for a destructor name.  Destructor names are always recognized
       following a class qualifier, but otherwise are only recognized if the
       GID_DTOR_RECOGNIZED flag is set. */
    if (is_nonclass_dtor) {
      /* Don't do normal destructor processing on a non-class vacuous
         destructor.  Set the destructor flag in the locator and
         look up the identifier or type that follows the tilde. */
      (void)get_token();  /* Get the token after the "~". */
      if (curr_token == tok_identifier) {
	/* A typedef name -- lookup the symbol and find the type pointed to.
           This will be something like "i::~i" or "A::i::~i".  If "i"
           is a member of a class then we need to do the lookup in the class
           of which "i" is a member.  Likewise, if "i" is a member of a
           namespace then we need to do the lookup in the namespace of which
           "i" is a member.  If "i" is not a member, then do either a
           normal or file-scope lookup depending on whether "i" had used
           a global scope qualifier. */
        a_symbol_ptr	type_sym = NULL;

        /* Set dtor_class_type to class_type.  This is only needed when
	   we have a typedef name.  For a type name like "int" it will
	   already have been set. */
        dtor_class_type = qualifier_type;
        /* Look up the destructor name based on the qualifier that was
           present (if any). */
        type_sym = look_up_destructor_name(&locator_for_curr_id,
                                           is_file_scope_qualified_name,
                                           qualifier_sym,
                                           /*no_normal_lookup=*/FALSE,
                                           IDL_NO_OPTIONS);
        if (type_sym != NULL) {
          /* Make sure the lookup of the destructor type was not ambiguous. */
          check_for_ambiguity(&locator_for_curr_id);
        }  /* if */
        /* Clear the specific symbol found by these lookups. */
        clear_specific_symbol(locator_for_curr_id);
        if (type_sym != NULL && is_type_symbol(type_sym)) {
	  /* If the symbol found is a type, get the type pointed to. */
	  dtor_type = type_symbol_type(type_sym);
          /* This will eventually result in the locator qualifier class type
	     being set to the type of the vacuous destructor. */
          qualifier_type = dtor_type;
          qualifier_is_type = TRUE;
        } else {
          pos_st_error(ec_not_a_type_name, &tilde_position,
                       locator_for_curr_id.symbol_header->identifier);
          err = TRUE;
	}  /* if */
      } else if (!strict_ansi_mode && (dtor_type = type_keyword()) != NULL) {
	/* A type keyword (e.g. int, long, etc.). Get the type
           associated with the keyword. */
        /* If the thing being scanned looks like "T::~int", where T is a
	   typedef, save the type pointed to as dtor_class_type.  This
	   will be used later for error checking. */
        check_assertion(qualifier_is_type == TRUE);
        if (dtor_class_type == NULL) dtor_class_type = qualifier_type;
        /* This will eventually result in the locator qualifier class type
	   being set to the type of the vacuous destructor. */
        qualifier_type = dtor_type;
        /* Make the current token a tok_identifier. */
        curr_token = tok_identifier;
      } else {
        /* The token after the "~" is not an identifier or a type name. */
        error(ec_exp_identifier);
        err = TRUE;
      }  /* if */
      locator_for_curr_id.is_destructor_name = TRUE;
      if (!err && field_sel_type != NULL) {
        /* When field_sel_type is non-NULL we are processing the right hand
           side of a field selection (e.g., "p->~X()").  Make sure that the
           destructor type that has been found matches the type of the
           left operand. */
        check_assertion(dtor_type != NULL);
        if (!identical_types(field_sel_type,
                             f_skip_typerefs(dtor_type))) { /*lint !e666*/
          pos_ty_error(ec_invalid_destructor_name, &tilde_position,
                       field_sel_type);
          err = TRUE;
        }  /* if */
      }  /* if */
      if (err) {
        /* An error has occurred.  Set the result types to NULL to prevent
           subsequent errors. */
        qualifier_type = NULL;
        dtor_type = NULL;
        dtor_class_type = NULL;
      }  /* if */
    } else if (((options & GID_DTOR_RECOGNIZED) ||
                (is_qualified_name && qualifier_is_type)) &&
               !is_file_scope_qualified_name) {
      /* The name can be a destructor name like "~A". */
      if (curr_token == tok_compl) {
        get_destructor_name(field_sel_type, is_file_scope_qualified_name,
                            qualifier_sym);
      }  /* if */
    }  /* if */
    if (locator_for_curr_id.is_destructor_name) {
      /* The position of the current identifier should be the tilde that
         begins the destructor name. */
      locator_for_curr_id.source_position = tilde_position;
    }  /* if */
    if (is_vacuous_dtor && is_qualified_name) {
      /* Make sure a vacuous destructor reference is correctly formed. 
         These tests only apply if the vacuous destructor is part of
         a qualified name. */
      check_assertion(qualifier_is_type == TRUE);
      if (!is_nonclass_dtor) {
	/* If qualifier_type is NULL an error must have already occurred. */
        if (qualifier_type != NULL) {
          /* If this is a vacuous destructor reference, just make sure
             the name of the destructor matches the name of the class. */
          a_symbol_ptr	class_sym;
          class_sym = (a_symbol_ptr)qualifier_type->source_corresp.assoc_info;
          if (is_error_locator(locator_for_curr_id)) {
             /* An error occurred earlier while checking the destructor. */
             err = TRUE;
            /* Set the class type to NULL as an indicator to the
	       coalesce routine that an error has occurred. */
             qualifier_type = NULL;
          } else if (!destructor_name_matches_class_name(class_sym)) {
            pos_ty_error(ec_destructor_name_mismatch, &tilde_position,
			 qualifier_type);
  	    err = TRUE;
            /* Set the class type to NULL as an indicator to the
	       coalesce routine that an error has occurred. */
	    qualifier_type = NULL;
          }  /* if */
        }  /* if */
      } else {
        /* This is a vacuous destructor reference for a non-class
           type (e.g., int::~int).  Make sure the type of the thing
           before the "::" matches the type of the thing after it. */
        /* If the dtor_class_type is NULL then an error occurred while
	   scanning the part before the "::~", so don't issue another
           error here.  If the type of the thing after the "::~" is NULL,
           or doesn't match dtor_class_type, issue an error. */
        if (dtor_class_type == NULL) {
	  qualifier_type = NULL;
        } else if (dtor_type == NULL ||
                        !identical_types(dtor_class_type, dtor_type)) {
          pos_ty_error(ec_destructor_type_mismatch, &tilde_position,
		       dtor_class_type);
          err = TRUE;
          /* Set the class type to NULL as an indicator to the
	     coalesce routine that an error has occurred. */
	  qualifier_type = NULL;
        }  /* if */
      }  /* if*/
    }  /* if */
    /* The name can be an operator name like "operator+". */
    if (curr_token == tok_operator) {
      /* get_opname requires a parent class or namespace pointer.
         Construct one for the qualifier that has been scanned. */
      a_parent_class_or_namespace parent;
      if (qualifier_is_type) {
        parent.class_type = qualifier_type;
      } else {
        parent.namespace_ptr = qualifier_namespace;
      }  /* if */
      get_opname(qualifier_is_type, &parent);
    }  /* if */
wrapup:
    /* The current token must now be the final identifier of the
       qualified name, e.g., "x" in "A::B::x".  In the destructor and
       operator name cases, curr_token has been changed to
       tok_identifier. */
    if (curr_token != tok_identifier && !is_nonclass_dtor) {
      /* The final identifier is missing.  Unget the current token and
         build an error locator. */
      unget_token();
      curr_token = tok_identifier;
      /* syntax_error is deliberately not called. */
      error(ec_exp_identifier);
      /* For the error cases, set the current locator to an error locator
         with specific_symbol pointing to a newly-created error
         symbol of kind sk_undefined. */
      make_specific_symbol_error_locator(&locator_for_curr_id);
    }  /* if */
    if (qualifier_is_type) {
      locator_for_curr_id.parent.class_type = qualifier_type;
      locator_for_curr_id.is_class_member = qualifier_type != NULL;
    } else {
      if (ignore_std_namespace &&
          qualifier_namespace ==
                        symbol_for_namespace_std->variant.namespace_info.ptr) {
        /* When using the g++ compatibility mode where "std" is an alias for
           the global namespace, ignore a qualifier that refers to "std"
           namespace. */
        locator_for_curr_id.parent.namespace_ptr = NULL;
        is_file_scope_qualified_name = TRUE;
      } else {
        locator_for_curr_id.parent.namespace_ptr = qualifier_namespace;
      }  /* if */
    }  /* if */
    /* Update the locator with information about the qualifier (if any)
       that was discovered by is_generalized_identifier_start. */
    locator_for_curr_id.is_qualified_name = is_qualified_name;
    locator_for_curr_id.is_global_qualified_name = is_global_qualified_name;
    locator_for_curr_id.is_file_scope_qualified_name =
						is_file_scope_qualified_name;
    locator_for_curr_id.has_been_coalesced = TRUE;
    locator_for_curr_id.is_vacuous_destructor_reference = is_vacuous_dtor;
    locator_for_curr_id.is_nonclass_destructor = is_nonclass_dtor;

    /* Since we're returning a pseudo-token, set pos_curr_token. */
    pos_curr_token = start_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    /* The ending position should already be set correctly. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    curr_token_sequence_number = start_seq_number;
    /* Restore the original error position. */
    error_position = orig_error_position;
    /* Perform error checks as specified in "options". */
    err |= check_for_generalized_identifier_errors(options, &pos_curr_token);
  }  /* if */

exit:
  db_exit();
  return result;
}  /* f_is_generalized_identifier_start */


a_boolean coalesce_and_lookup_qualified_name
              (an_identifier_options_set        options,
	       an_identifier_lookup_mode	ilm,
               a_boolean			*err)
/*
Coalesces a generalized identifier (see coalesce_generalized_identifier).
If the identifier was qualified (e.g., A::x or ::x) the identifier
is looked up.  Returns TRUE if identifier is a qualified name.
The caller must guarantee that is_generalized_identifier_start is TRUE
(i.e., that the thing being scanned is, in fact, an identifier).
*/
{
  a_boolean             return_value = FALSE;
  a_boolean		okay = TRUE;
  a_type_ptr		qualifier_type = NULL;
  a_namespace_ptr	qualifier_namespace = NULL;
  a_boolean		qualifier_is_type = TRUE;
  a_boolean		is_vacuous_dtor = FALSE;
  db_enter(4, "coalesce_and_lookup_qualified_name");

  *err = FALSE;
  if (C_dialect != C_dialect_cplusplus) goto exit;
  if (curr_token == tok_identifier &&
      locator_for_curr_id.specific_symbol != NULL) {
    /* The current token is already a qualified name or specific symbol.
       Recheck for qualifier errors since the options specified may be
       different than a previous call. */
    return_value = locator_for_curr_id.is_qualified_name;
    /* Perform error checks as specified in "options". */
    okay = TRUE;
    /* Don't check any further if we already have an error locator.  Leave
       okay TRUE so that we don't try to build a new error locator later. */
    *err = is_error_locator(locator_for_curr_id);
    if (!*err) {
      if (check_for_generalized_identifier_errors(options, &error_position) ||
          check_for_template_declarator_errors(options, &error_position)) {
        *err = TRUE;
        okay = FALSE;
      }  /* if */
    }  /* if */
  } else {
    /* Mask the error flags out of the options flags to prevent the errors
       from being diagnosed more than once. */
    if (is_generalized_identifier_start(options & ~GID_ERROR_FLAGS) &&
        curr_token == tok_identifier &&
        locator_for_curr_id.is_qualified_name) {
      /* The current identifier is a qualified name. */
      an_error_code	error_code;
      a_source_position	identifier_pos;
      identifier_pos = locator_for_curr_id.source_position;
      /* If the qualifier is a type, qualifier_type will point to the
         type and qualifier_namespace will be NULL.  If the qualifier
         is a namespace, qualifier_type will be NULL and qualifier_namespace
         will point to the namespace. */
      qualifier_is_type = locator_for_curr_id.is_class_member;
      qualifier_type = qualifier_class_type(locator_for_curr_id);
      qualifier_namespace = qualifier_namespace_ptr(locator_for_curr_id);
      is_vacuous_dtor = locator_for_curr_id.is_vacuous_destructor_reference;
      return_value = TRUE;
      *err |= is_error_locator(locator_for_curr_id);
      /* Perform error checks as specified in "options". */
      if (*err) {
        /* Don't try to lookup the identifier if an error occurred earlier. */
        okay = FALSE;
      } else if (check_for_generalized_identifier_errors(options,
		   				         &pos_curr_token)) {
        *err = TRUE;
        okay = FALSE;
      } else {
	a_boolean			is_nonclass_dtor =
			 locator_for_curr_id.is_nonclass_destructor;
        an_id_lookup_options_set	idl_options;
        /* Translate the general identifier options into ID lookup options. */
	idl_options = idl_options_for_lookup_mode[(int)ilm];
        /* No errors were diagnosed. */
        if (locator_for_curr_id.is_file_scope_qualified_name) {
          /* Look up the id in the file scope. */
          if (is_vacuous_dtor) {
            /* If qualifier_type is NULL an error occurred while processing
               the vacuous destructor.  Treat this the same way we would
	       a failed lookup. */
	    okay = qualifier_type != NULL;
          } else if (file_scope_id_lookup(il_header.primary_scope,
					  &locator_for_curr_id,
                                          idl_options) != NULL) {
          } else {
            /* The identifier could not be found in the file scope. */
	    if (ilm == ilm_tentative_type) {
	      /* It is OK for a tentative type lookup to fail. */
	      okay = TRUE;
	    } else {
              /* Issue one of several difference messages depending on the
                 kind of symbol we are looking for. */
              if (ilm == ilm_tag) {
                error_code = ec_name_not_tag_in_file_scope;
              } else if (ilm == ilm_class ||
                         ilm == ilm_qualified_ctor_initializer_name) {
                error_code = ec_name_not_class_in_file_scope;
              } else {
                error_code = ec_name_not_found_in_file_scope;
              }  /* if */
              pos_st_error(error_code, &identifier_pos,
                           locator_for_curr_id.symbol_header->identifier);
              okay = FALSE;
            }  /* if */
          }  /* if */
        } else {
          if ((qualifier_is_type && qualifier_type == NULL) ||
              (!qualifier_is_type && qualifier_namespace == NULL)) {
	    okay = FALSE;
          } else if (qualifier_is_type && !is_nonclass_dtor && 
                     is_incomplete_type(qualifier_type) &&
                     is_class_struct_union_type(qualifier_type) &&
                     qualifier_type->variant.class_struct_union.
                                         extra_info->assoc_scope == NULL) {
            /* An error must have occurred while scanning the class
               qualifier.  Don't try to do the lookup of the identifier
               following the qualifier.   If the type is incomplete we also
               check whether the type is currently being defined -- it is
               considered complete if it is being defined.  We determine
               this by checking the assoc_scope field of the class type
               supplement.  If the type is incomplete an error will have
               been issued earlier.  The check for the type kind being
               a class/struct/union type is needed because the type may
               also be a template parameter type. */
            okay = FALSE;
            pos_error(ec_incomplete_type_not_allowed, &pos_curr_token);
          } else {
            /* Don't try to look up a vacuous destructor name. */
            if (is_vacuous_dtor) {
              /* If qualifier_type is NULL an error occurred while processing
		 the vacuous destructor.  Treat this the same way we would
		 a failed lookup. */
	      okay = qualifier_type != NULL;
            } else {
              /* Look up the id in the class scope. */
              a_boolean	qualifier_is_enum_type;
              qualifier_is_enum_type = microsoft_bugs && qualifier_is_type &&
                                       is_enum_type(qualifier_type);
              if (microsoft_bugs && qualifier_is_enum_type &&
                  is_enum_type(qualifier_type) &&
                  enum_qualified_id_lookup(&locator_for_curr_id,
                                            qualifier_type) != NULL) {
                /* In Microsoft bugs mode, enumerations can be used as
                   qualifiers.  The name was found as an enumerator. */
              } else if (qualifier_is_type && !qualifier_is_enum_type &&
                  class_qualified_id_lookup(&locator_for_curr_id,
                                            qualifier_type,
					    idl_options) != NULL) {
                /* Ambiguity and access control checking is not done because
                   we don't know yet what kind of reference this is. */
              } else if (!qualifier_is_type && qualifier_namespace != NULL &&
                         namespace_qualified_id_lookup
                                 (&locator_for_curr_id,
                                  skip_namespace_aliases(qualifier_namespace),
                                  idl_options) != NULL) {
                /* The identifier was found in the namespace. */
	      } else {
                /* The identifier could not be found in the class or namespace
                   scope. */
	        if (ilm == ilm_tentative_type) {
		  /* It is OK for a tentative type lookup to fail. */
		  okay = TRUE;
		} else if (is_error_locator(locator_for_curr_id)) {
		  /* An error was previously issued. */
		} else {
                  /* Issue one of several difference messages depending on the
                     kind of symbol we are looking for. */
                  a_symbol_ptr  err_sym;
                  if (ilm == ilm_tag) {
                    error_code = ec_not_a_tag_member;
                  } else if (ilm == ilm_class ||
                             ilm == ilm_qualified_ctor_initializer_name) {
                    error_code = ec_not_a_member_class;
                  } else {
                    error_code = C_mode() ? ec_not_a_field : ec_not_a_member;
                  }  /* if */
                  if (qualifier_is_type) {
                    err_sym = (a_symbol_ptr)qualifier_type->
                                                    source_corresp.assoc_info;
                  } else {
                    err_sym = (a_symbol_ptr)qualifier_namespace->
                                                    source_corresp.assoc_info;
                  }  /* if */
                  pos_stsy_error(error_code, &identifier_pos,
                                 locator_for_curr_id.symbol_header->identifier,
                                 err_sym);
                  okay = FALSE;
                }  /* if */
              }  /* if */
            }  /* if */
          }  /* if*/
        }  /* if */ 
      }  /* if */
      /* If the symbol found is a class template then this must be a
         reference to a instance of the class template.  This can occur when
         a template name appears in a qualified name without a template
         argument list (if a template argument list were specified it would
         have been coalesced by f_is_generized_identifier_start). */
      {
        a_symbol_ptr	symbol;
	a_boolean	templ_err = FALSE;
        symbol = locator_for_curr_id.specific_symbol;
        if (symbol != NULL &&
            symbol->kind == (a_symbol_kind)sk_class_template) {
          symbol = coalesce_template_class_reference(symbol,
                                                     options, &templ_err);
           if (templ_err) {
             okay = FALSE;
             *err = TRUE;
           }  /* if */
        }  /* if */
      }
      /* A qualified declarator name in a template declaration must name
         a template or a member of a class template. */
      if (check_for_template_declarator_errors(options, &error_position)) {
        *err = TRUE;
        okay = FALSE;
      }  /* if */
    } else {
      /* Not a qualified name -- possibly not an identifier.  If an
         error locator was built by is_generalized_identifier_start,
         set err to TRUE. */
      if (is_error_locator(locator_for_curr_id)) {
        *err = TRUE;
      }  /* if */
    }  /* if */
  }  /*if */
  if (!okay) {
    /* For the error cases, set the current locator to an error locator. 
       Restore the type of the qualifier so that it can be used for
       additional error diagnosis.  For example, in an access adjustment
       such as "A::b" we would like to be able to give an error if "A" is not
       a base class of the current class even if "b" is not a member of "A".
       Also restore the vacuous destructor flag to assist in error
       diagnosis by the caller.  */
    make_specific_symbol_error_locator(&locator_for_curr_id);
    if (qualifier_is_type) {
      locator_for_curr_id.parent.class_type = qualifier_type;
      locator_for_curr_id.is_class_member = qualifier_type != NULL;
    } else {
      locator_for_curr_id.parent.namespace_ptr = qualifier_namespace;
      locator_for_curr_id.is_class_member = FALSE;
    }  /* if */
    locator_for_curr_id.is_vacuous_destructor_reference = is_vacuous_dtor;
    *err = TRUE;
  }  /* if */
  /* Set error position to the beginning of the coalesced pseudo-token. */
  error_position = pos_curr_token;

#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "coalesce_and_lookup_qualified_name: ");
    if (return_value) {
      if (is_error_locator(locator_for_curr_id)) {
        fprintf(f_debug, "<error>\n");
      } else if (locator_for_curr_id.specific_symbol == NULL) {
        fprintf(f_debug, "name = %s (no specific symbol)\n",
                locator_for_curr_id.symbol_header->identifier);
      } else {
        fprintf(f_debug, "name = %s\n",
                locator_for_curr_id.specific_symbol->header->identifier);
      }  /* if */
    } else {
      fprintf(f_debug, "not qualified name\n");
    }  /* if */
  }  /* if */
#endif /* DEBUG */
exit:
  db_exit();
  return return_value;
}  /* coalesce_and_lookup_qualified_name */


a_symbol_ptr coalesce_and_lookup_generalized_identifier
                 (an_identifier_options_set        options,
                  an_identifier_lookup_mode        ilm,
                  a_boolean                        *err)
/*
The current token is the start of a name, qualified or not.
Call coalesce_and_lookup_qualified_name and then normal_id_lookup, and return a
pointer to the symbol found, if any.  options is a bit set of options
controlling the lookup of the final id of a qualified name or the
normal identifier.  The caller must guarantee that
is_generalized_identifier_start is TRUE (i.e., that the thing being
scanned is, in fact, an identifier).
*/
{
  a_symbol_ptr			symbol;
  an_id_lookup_options_set	idl_options;
  a_boolean			templ_err = FALSE;

  /* Mask the error flags out of the options flags to prevent the errors
     from being diagnosed more than once. */
  if (coalesce_and_lookup_qualified_name(options & ~GID_ERROR_FLAGS,
                                         ilm, err)) {
    /* The identifier is a qualified name. */
    symbol = locator_for_curr_id.specific_symbol;
#if CHECKING
    if (symbol == NULL && ilm != ilm_tentative_type) {
      internal_error
       ("coalesce_and_lookup_generalized_identifier: specific_symbol is NULL");
    }  /* if */
#endif /* CHECKING */
    if (symbol != NULL) reduce_projection_symbol_to_fundamental_symbol(symbol);
  } else {
#if CHECKING
    if (curr_token != tok_identifier) {
      internal_error
               ("coalesce_and_lookup_generalized_identifier: not identifier");
    }  /* if */
#endif /* CHECKING */
    /* Normal identifier -- look it up. */
    /* Translate the general identifier options into ID lookup options. */
    idl_options = idl_options_for_lookup_mode[(int)ilm];
    symbol = normal_id_lookup(&locator_for_curr_id, idl_options);
  }  /* if */
  /* If this is the symbol of a class template then this must be a reference
     to a instance of the class template.  Scan the argument list and
     get a pointer to the symbol for the specific instance of the template
     class.  is_template_id will be TRUE if the template reference has already
     been coalesced. */
  if (symbol != NULL &&
      is_class_template_or_injected_template_symbol(symbol)) {
    if (locator_for_curr_id.is_unknown_template_reference) {
      /* This is a template class reference that was changed back to a
         template reference by ensure_correct_nonreal_instance_kind.
         It is now being used in a way that requires a type of some kind.
         Redo the operation that finds the specific class being named. */
      if (ilm == ilm_tag ||
          ilm == ilm_typename ||
          ilm == ilm_class ||
          ilm == ilm_using_typename ||
          (ilm == ilm_tentative_type && implicit_typename_enabled)) {
        a_template_arg_ptr	arg_list;
        arg_list = locator_for_curr_id.template_arg_list;
        symbol = find_template_class(symbol, &arg_list,
                                     /*prototype_allowed=*/FALSE,
                                     (a_symbol_ptr)NULL);
        locator_for_curr_id.is_unknown_template_reference = FALSE;
      }  /* if */
    } else {
      symbol = coalesce_template_class_reference(symbol, options, &templ_err);
    }  /* if */
  }  /* if */
  *err |= templ_err;
  /* If an error occurred while scanning the template argument list,
     return a NULL symbol.  The locator will already be set to an error
     locator. */
  if (templ_err) symbol = NULL;
  /* Perform error checks as specified in "options". */
  *err |= check_for_generalized_identifier_errors(options, &error_position);
  return symbol;
}  /* coalesce_and_lookup_generalized_identifier */


static a_file_suffix_ptr alloc_file_suffix(void)
/*
Allocate a file suffix entry, initialize it, and return a pointer to it.
*/
{
  a_file_suffix_ptr fsp;

  fsp = (a_file_suffix_ptr)alloc_general(sizeof(a_file_suffix));
#if DEBUG
  num_file_suffixes_allocated++;
#endif /* DEBUG */
  fsp->next = NULL;
  fsp->suffix = NULL;
  return fsp;
}  /* alloc_file_suffix */


static void add_to_file_suffix_list(a_file_suffix_ptr	*list_ptr,
				    char*		suffix,
                                    int			length)
/*
Add a new entry to the end of the file suffix list specified by list_ptr.
If the entry is already on the list the new entry is ignored.
*/
{
  a_file_suffix_ptr	fsp;
  a_file_suffix_ptr	prev_fsp = NULL;
  a_boolean		found = FALSE;

  fsp = *list_ptr;
  while (fsp != NULL) {
    if (strcmp(fsp->suffix, suffix) == 0) {
      /* The suffix is already on the list. */
      found = TRUE;
      break;
    }  /* if */
    prev_fsp = fsp;
    fsp = fsp->next;
  }  /* while */
  if (!found) {
    /* If the suffix is not found add it to the end of the list which is
       now pointed to by prev_fsp. */
    fsp = alloc_file_suffix();
    /* Allocate space for the suffix including a null delimiter. */
    fsp->suffix = (char *)alloc_general((sizeof_t)length + 1);
    strncpy(fsp->suffix, suffix, length);
    /* Terminate the copy of the string with a null character. */
    fsp->suffix[length] = '\0';
    if (prev_fsp == NULL) {
      /* This is the first entry on the list. */
      *list_ptr = fsp;
    } else {
      prev_fsp->next = fsp;
    }  /* if */
#if DEBUG
    if (db_flag_is_set("add_to_file_suffix_list")) {
      fprintf(f_debug, "Added \"%s\" to the suffix list.\n", fsp->suffix);
    }  /* if */
#endif /* DEBUG */
  }  /* if */
}  /* add_to_file_suffix_list */


static a_file_suffix_ptr conv_string_to_file_suffix_list(char *list)
/*
Convert the members of a colon separated list of file suffixes to a
list of file suffix entries.  Return a pointer to the newly created
list.
*/
{
  char			*ptr = list;
  char			*start;
  char			*end;
  a_file_suffix_ptr	list_fsp = NULL;

  /* Skip over an initial ":" in the string. */
  if (*ptr == ':') ptr++;
  while (*ptr) {
    /* Skip any spaces. */
    while (*ptr == ' ') ptr++;
    /* See if we've reached the end of the string. */
    if (!*ptr) break;
    start = ptr;
    /* Find the ending delimiter. */
    ptr = strchr(start, ':');
    if (ptr == NULL) {
      /* If there is no ending delimiter use the end of the string. */
      ptr = start + strlen(start);
    }  /* if */
    /* Get a pointer to the last character of the string. */
    end = ptr - 1;
    /* Move back past any trailing spaces. */
    while (*end == ' ') end--;
    add_to_file_suffix_list(&list_fsp, start, (int)(end - start + 1));
    /* If we haven't reached the end of the string, move the pointer past
       the delimiter. */
    if (*ptr) ptr++;
  }  /* while */
  return list_fsp;
}   /* conv_string_to_file_suffix_list */


static void cache_to_compound_stmt(a_token_cache	*p_token_cache,
				   a_token_set_array	stop_tokens)
/*
Cache tokens into p_token_cache until a left brace or semicolon is
encountered.  stop_tokens is the stop token set to be used.
*/
{
  incr_token_set_array_element(stop_tokens, tok_lbrace);
  incr_token_set_array_element(stop_tokens, tok_semicolon);
  cache_token_stream(p_token_cache, stop_tokens);
  decr_token_set_array_element(stop_tokens, tok_lbrace);
  decr_token_set_array_element(stop_tokens, tok_semicolon);
}  /* cache_to_compound_stmt */


static void cache_compound_stmt(a_token_cache		*p_token_cache,
				a_token_set_array	stop_tokens)
/*
Cache a compound statement (i.e., "{...}") into p_token_cache.
stop_tokens is the stop token set to be used.  After this routine is
called the current token is the right brace, except for error cases.
*/
{
  check_assertion(curr_token == tok_lbrace);
  /* Cache the "{" and advance past it. */
  cache_curr_token(p_token_cache);
  (void)get_token();
  /* Cache all tokens up to the "}" (or end-of-source). */
  incr_token_set_array_element(stop_tokens, tok_rbrace);
  cache_token_stream(p_token_cache, stop_tokens);
  /* Cache the "}" and append an end-of-source token. */
  if (curr_token == tok_rbrace) {
    cache_curr_token(p_token_cache);
    /* A get_token is intentionally not done -- the caller will
       advance past the end of the template declaration. */
  }  /* if */
}  /* cache_compound_stmt */


static void cache_catch_clauses(a_token_cache		*p_token_cache,
				a_token_set_array	stop_tokens)
/*
Cache into p_token_cache the catch clauses that follow a function
definition that are associated with a function try block.  stop_tokens
is the stop token set to be used.  After this routine is
called the current token is the right brace, except for error cases.
*/
{
  while (next_token() == tok_catch) {
    /* Advance past the right brace that has already been cached. */
    (void)get_token();
    /* Cache the "catch" and advance past it. */
    cache_curr_token(p_token_cache);
    (void)get_token();
    /* Cache the parameter declaration. */
    cache_to_compound_stmt(p_token_cache, stop_tokens);
    /* Cache the catch compound statement (if it looks like one is next). */
    if (curr_token == tok_lbrace) {
      cache_compound_stmt(p_token_cache, stop_tokens);
    }  /* if */
    /* Exit the loop if we are not at the expected close of the
       compound statement. */
    if (curr_token != tok_rbrace) break;
  }  /* while */
}  /* cache_catch_clauses */


a_boolean cache_function_body(
			a_token_cache		*p_token_cache,
			a_boolean		is_constructor,
			a_boolean		*missing_end,
			a_token_sequence_number	*first_tsn,
			a_token_sequence_number	*last_tsn,
			a_source_position	*start_pos,
			a_source_position	*end_pos)
/*
Scan the tokens of a function definition.  This can include scanning
the tokens of ctor-initializers for a constructor definition and can
also include scanning the tokens of a function try block.  Return TRUE
if a complete function definition was scanned.  is_constructor is TRUE
if the function is a constructor.  missing_end is set to TRUE if the
ending brace of the function body is not found; it can be NULL if this
information need not be returned.  first_tsn and last_tsn are the
token sequence numbers of the first and last tokens of the function
body; they can be NULL if the token sequence numbers need not be
returned.  start_pos and end_pos are the starting and ending source
positions of the function body; they can be NULL if the positions need
not be returned.
*/
{
  a_token_set_array  stop_tokens;
  a_boolean	     result = FALSE;
  a_boolean	     try_found = FALSE;

  db_enter(3, "cache_function_body");
  if (first_tsn != NULL) *first_tsn = NO_TOKEN_SEQUENCE_NUMBER;
  if (last_tsn != NULL) *last_tsn = NO_TOKEN_SEQUENCE_NUMBER;
  if (missing_end != NULL) *missing_end = FALSE;
  if (start_pos != NULL) *start_pos = null_source_position;
  if (end_pos != NULL) *end_pos = null_source_position;
  if (curr_token == tok_lbrace || curr_token == tok_try ||
      (curr_token == tok_colon && is_constructor)) {
    /* Initialize a local stop token set. */
    clear_token_set_array(stop_tokens);
    /* Make a note whether this is a function try block.  This controls whether
       we look for catch clauses later. */
    try_found = curr_token == tok_try;
    if (try_found) {
      /* Cache the "try" and advance past it. */
      cache_curr_token(p_token_cache);
      (void)get_token();
    }  /* if */
    /* Save the token sequence number of the first token of the definition. */
    if (first_tsn != NULL) *first_tsn = curr_token_sequence_number;
    if (curr_token == tok_colon) {
      /* This is a ctor-initializer list on a constructor.  Cache it. */
      cache_to_compound_stmt(p_token_cache, stop_tokens);
    }  /* if */
    if (curr_token == tok_lbrace) {
      /* This is a compound statement that is the body of the function. */
      /* Save the starting position of the main block of the function. */
      if (start_pos != NULL) *start_pos = pos_curr_token;
      cache_compound_stmt(p_token_cache, stop_tokens);
#if EXTRA_SOURCE_POSITIONS_IN_IL
      /* Save the ending position of the main block of the function. */
      if (end_pos != NULL) *end_pos = end_pos_curr_token;
#endif /*  EXTRA_SOURCE_POSITIONS_IN_IL */
      if (try_found) {
        /* If this function is a function try block, cache the associated
           catch clauses. */
        cache_catch_clauses(p_token_cache, stop_tokens);
      }  /* if */
      if (curr_token == tok_rbrace) {
        /* A get_token is intentionally not done -- the caller will
           advance past the end of the template declaration. */
        result = TRUE;
      } else {
        /* The end of the function body was not found.  This is usually
           the result of a mismatched delimiter. */
        if (missing_end != NULL) *missing_end = TRUE;
      }  /* if */
      /* Save the token sequence number of the last token of the definition. */
      if (last_tsn != NULL) *last_tsn = curr_token_sequence_number;
    }  /* if */
    /* Add an end-of-source token to the end of the token cache to
       assure that we don't scan past the end of the cache in the actual
       scan. */
    terminate_token_cache(p_token_cache);
  }  /* if */
  db_exit();
  return result;
}  /* cache_function_body */


void begin_rescan_of_pragma_tokens(a_pending_pragma_ptr ppp)
/*
Active the token cache containing the pragma to be scanned and push a
pragma scope to be used while scanning the pragma tokens.
*/
{
  /* Start a new stop token state. */
  push_stop_token_stack();
  rescan_reusable_cache(&ppp->token_cache);
  /* Push a pragma scope.  This prevents names introduced by the pragma
     processing from polluting the current scope. */
  (void)push_scope((a_scope_kind)sck_pragma, NO_SCOPE_NUMBER, (a_type_ptr)NULL,
	           (a_routine_ptr)NULL);
}  /* begin_rescan_of_pragma_tokens */


void wrapup_rescan_of_pragma_tokens(a_boolean	       error_in_pragma)
/*
This routine is called by pragma processing routines when they have reached
the end of the pragma directive being scanned.  This routine fetches
the token that terminates the token cache and returns the token stream
to its original state.  If the current token is not the end-of-source token
that terminates the pragma directive, an error is issued (unless the
error_in_pragma flag is set indicating the pragma processing routine already
diagnosed an error).  The pragma scope pushed when the token cache
is actived is popped here.
*/
{
  if (curr_token != tok_end_of_source) {
    if (!error_in_pragma) {
      pos_error(ec_extra_text_in_pp_directive, &pos_curr_token);
    }  /* if */
    /* Flush any tokens until an end-of-source token is found. */
    while (curr_token != tok_end_of_source) {
      (void)get_token();
    }  /* while */
  }  /* if */
  /* Bypass the cache terminator. */
  (void)get_token();
  /* Restore the stop token set as at entry. */
  pop_stop_token_stack();
  /* Pop the pragma scope. */
  pop_scope();
}  /* wrapup_rescan_of_pragma_tokens */


#if TOKENS_TO_STRING_NEEDED
/*
Static variables and routines that are used to build up a string representation
of a token cache.  Characters are added to the temp_text_buffer, which is
indefinitely expandable, and then a string is created -- a copy of the
assigned portion of the buffer, with a null terminator appended.
*/
static a_seq_number
		curr_seq;
			/* The sequence number of the source position of the
			   most recently added token, etc., in the buffer. */

static an_il_to_str_output_control_block
		octl;
			/* Output control block used to interface to the
			   il_to_str routines. */


static void add_whitespace_to_string(a_seq_number     seq_incr,
                                              a_column_number  column_incr)

/*
Add seq_incr newline characters and column_incr blanks to temp_text_buffer,
incrementing pos_in_temp_text_buffer accordingly.
*/
{
  for (; seq_incr > 0; --seq_incr) {
    put_ch_to_temp_text_buffer('\n');
  }  /* for */
  for (; column_incr > 0; --column_incr) {
    put_ch_to_temp_text_buffer(' ');
  }  /* for */
}  /* add_whitespace_to_string */


static void add_token_to_string(a_cached_token_ptr ctp)
/*
Copy characters representing the token specified by ctp into
temp_text_buffer, and increase pos_in_temp_text_buffer by the number
of characters added.
*/
{
  a_seq_number     seq_incr;
  a_column_number  column_incr;
  a_token_kind	   token = (a_token_kind)ctp->token;

  db_enter(5, "add_token_to_string");
  if (ctp->source_position.seq <= curr_seq) {
    /* We're on the same line as the previous token processed, so just add
       a space (in most cases) to separate the tokens.  (Note: the line for
       the current token may be less than curr_seq when a macro expansion
       occurs.  Treat the token as being on the current line.) */
    if (token == tok_comma || token == tok_semicolon ||
        pos_in_temp_text_buffer == 0) {
      /* No space is needed before a comma or semicolon -- or if this is
         the very first token of the declaration. */
      column_incr = 0;
    } else {
      check_assertion(pos_in_temp_text_buffer > 0 ||
                      curr_seq < ctp->source_position.seq);
      /* Add a single space. */
      column_incr = 1;
    }  /* if */
    /* Don't add a line feed. */
    seq_incr = 0;
  } else {
    /* We've moved to a new line.  Compute the indentation. */
    column_incr = ctp->source_position.column - 1;
    /* Compute the number of line feed characters to add. */
    seq_incr =  ctp->source_position.seq - curr_seq;
    /* Reset the current line. */
    curr_seq = ctp->source_position.seq;
  }  /* if */
  /* Add any spaces and line feeds that might be required. */
  if (seq_incr > 0 || column_incr > 0) {
    add_whitespace_to_string(seq_incr, column_incr);
  }  /* if */
  /* Now put out the characters representing the token. */
  if (token == tok_newline) {
    /* Ignore tok_newline.  It only comes up in Microsoft asm blocks.
       The required number of newline characters will be added when the
       sequence number changes. */
  } else if (ctp->extra_info_kind == (a_token_extra_info_kind)teik_pp_token) {
    /* A token that was saved as a pp-token.  This token should be added to
       the string using the start of token pointer.  Note that the
       copy of the token made for a cached pp-token is known to be null
       terminated. */
    put_str_to_temp_text_buffer(ctp->variant.pp_token_descr.token_start);
  } else if (token == tok_int_constant ||
             token == tok_float_constant ||
             token == tok_string_literal ||
             token == tok_char_constant) {
    a_constant_ptr	constant = ctp->variant.constant;
    /* Write out a string that represents the constant. */
    if (constant->kind == (a_constant_repr_kind)ck_error) {
      /* If there was an error in scanning the token, reset the flag to avoid
         an assertion failure in the subroutine.  This means something like
         "<error-const>" will be put out in the template string. */
      octl.gen_compilable_code = FALSE;
    }  /* if */
    form_constant(constant, /*need_parens=*/TRUE, &octl);
    /* Reset the flag, in case it had been changed. */
    octl.gen_compilable_code = TRUE;
#if CHECKING
    /* Check for tokens that should not show up in the cached tokens of
       a template declaration.  These tokens will be accepted if cached
       as pp-tokens. */
  } else if (token == tok_end_of_source ||
             token == tok_header_name ||
             token == tok_pp_number ||
             token == tok_digit_sequence ||
             token == tok_cpp_quote ||
             token == tok_ptr_to_member) {
    internal_error("add_token_to_string: unexpected token");
#endif /* CHECKING */
  } else if (token == tok_identifier) {
    /* An identifier. */
    check_assertion(!ctp->variant.locator.has_been_coalesced);
    put_str_to_temp_text_buffer(ctp->variant.locator.symbol_header->
                                                               identifier);
  } else if (microsoft_mode && token == tok_asm) {
    /* In Microsoft mode always put out "__asm", since "asm" is not
       necessarily accepted by the Microsoft compiler. */
    put_str_to_temp_text_buffer("__asm");
  } else {
    /* A keyword or other token whose literal name can be put out. */
    put_str_to_temp_text_buffer(token_names[(int)token]);
  }  /* if */    
  db_exit();
}  /* add_token_to_string */


static void add_pragmas_to_string(a_pending_pragma_ptr pragmas)
/*
If the current token has any pragmas associated with it, add strings to
represent them to temp_text_buffer.  Note that all pragmas that are
encountered, whatever their other characteristics, are included.
*/
{
  a_pending_pragma_ptr  ppp;
  a_boolean             is_pseudo_pragma;
  a_seq_number          seq_incr;
  a_column_number       column_incr;

  db_enter(5, "add_pragmas_to_string");
  for (ppp = pragmas; ppp != NULL; ppp = ppp->next) {
    if (ppp->pragma_position.seq <= curr_seq) {
      /* We're on the same line as the previous token processed, so just add
         a space (in most cases) to separate the tokens.  (Note: the line for
         the current token may be less than curr_seq when a macro expansion
         occurs.  Treat the token as being on the current line.) */
      column_incr = 1;
      /* Don't add a line feed. */
      seq_incr = 0;
    } else {
      /* We've moved to a new line.  Compute the indentation. */
      column_incr = ppp->pragma_position.column - 1;
      /* Compute the number of line feed characters to add. */
      seq_incr =  ppp->pragma_position.seq - curr_seq;
      /* Reset the current line. */
      curr_seq = ppp->pragma_position.seq;
    }  /* if */
    /* Add any spaces and line feeds that might be required. */
    if (seq_incr > 0 || column_incr > 0) {
      add_whitespace_to_string(seq_incr, column_incr);
    }  /* if */
    is_pseudo_pragma = ppp->descr_ptr->is_pseudo_pragma;
    if (is_pseudo_pragma) {
      /* Add comment delimiter to the template string. */
      put_str_to_temp_text_buffer("/*");
    } else {
      /* Add "#pragma " to the template string. */
      put_str_to_temp_text_buffer("#pragma ");
    }  /* if */
    if (ppp->descr_ptr->make_text_not_tokens) {
      /* Note: the pragma id is already part of pragma_text. */
      check_assertion(ppp->pragma_text != NULL);
      put_str_to_temp_text_buffer(ppp->pragma_text);
    } else {
      /* The pragma id is not part of pragma_text, so it has to be added
         explicitly. */
      put_str_to_temp_text_buffer(pragma_ids[(int)ppp->descr_ptr->kind]);
      if (ppp->token_cache.first_token != NULL) {
	/* Add the tokens from the pragma token cache to the string. */
        add_token_cache_to_string(&ppp->token_cache);
      }  /* if */
    }  /* if */
    if (is_pseudo_pragma) {
      /* Add terminating comment delimiter to the template string. */
      put_str_to_temp_text_buffer("*/");
    }  /* if */
  }  /* for */
  db_exit();
}  /* add_pragmas_to_string */


void add_token_cache_to_string(a_token_cache_ptr	cache)
/*
Go through a token cache and add the tokens to the string that is
being constructed that represents the tokens in the cache.
*/
{
  a_cached_token_ptr	ctp = cache->first_token;

#if DEBUG
  if (db_flag_is_set("atcts")) {
    db_token_cache(cache, "add_token_cache_to_string");
  }  /* if */
#endif /* DEBUG */
  for (; ctp != NULL; ctp = ctp->next) {
    a_token_extra_info_kind	teik_kind;
    /* Stop when we run out of tokens or hit an end-of-source token. */
    if ((a_token_kind)ctp->token == tok_end_of_source) break;
    if (ctp->token == (a_byte_token_kind)tok_removed_default_arg) {
      /* A special token that indicates the location of a removed
         default argument.  The actual default argument tokens should
         still be used for purposes of generating the template string. */
      ctp = ctp->variant.extracted_template.next_in_token_string;
    }  /* if */
    teik_kind = ctp->extra_info_kind;
    if (teik_kind == (a_token_extra_info_kind)teik_pragma) {
      /* This token entry represents one or more pragmas.  Call a routine
         to add the pragmas to the string. */
      add_pragmas_to_string(ctp->variant.pragmas);
    } else if (teik_kind == (a_token_extra_info_kind)teik_extracted_body) {
      if (ctp->variant.extracted_template.next_in_token_string == NULL) {
        a_boolean	add_orig_token = TRUE;
        a_boolean	add_body_string = TRUE;
        /* A template body was extracted at this location.  Insert the body of
           the template at this point in the string. */
        a_symbol_ptr			sym;
        a_template_symbol_supplement_ptr	tssp;
        sym =   ctp->variant.extracted_template.symbol;
        tssp = template_supplement_for_symbol(sym);
#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
        if (sym->kind == (a_symbol_kind)sk_member_function) {
          /* When source sequence entries for nonclass template instantiations
             are generated, member function bodies of class templates are
             suppressed because certain compilers don't permit a member
             function to be both defined in the class and specialized later. */
          add_body_string = FALSE;
        }  /* if */
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
        if (add_body_string) { /*lint !e774*/
          add_token_cache_to_string(&tssp->cache.tokens);
        }  /* if */
        /* This semicolon was inserted, and so should be suppressed if the body
           is output above. */
        add_orig_token = !add_body_string ||
                         !ctp->variant.extracted_template.semicolon_inserted;
        if (add_orig_token) {
          add_token_to_string(ctp);
        }  /* if */
      } else {
        /* A friend function defined in a class template.  Skip over the tokens
           in the cache that represent the body, and add a semicolon to the
           string. */
        ctp = ctp->variant.extracted_template.next_in_token_string;
        put_ch_to_temp_text_buffer(';');
      }  /* if */
    } else {
      /* A normal token (including, possibly, a pp-token). */
      add_token_to_string(ctp);
    }  /* if */
    if (teik_kind == (a_token_extra_info_kind)teik_asm_string) {
      /* A Microsoft asm string.  Add the asm string to the buffer. */
      put_str_to_temp_text_buffer(ctp->variant.asm_string);
    }  /* if */
  }  /* for */
}  /* add_token_cache_to_string */


void init_token_string(a_source_position *pos)
/*
Prepare to generate a string from one or more token caches.  Initialize
the string length to zero and set the current sequence number to the
position specified by pos.
*/
{
  curr_seq = pos->seq;
  pos_in_temp_text_buffer = 0;
}  /* init_token_string */
#endif /* TOKENS_TO_STRING_NEEDED */


#if DEBUG
void db_token_cache(a_token_cache *cache,
                    char	  *cache_name)
/*
Display the contents of a token cache.
*/
{
  a_cached_token_ptr	ctp;
  unsigned long		count = 0;

  fprintf(f_debug, "%s token cache at %p\n", cache_name, cache);
  if (cache != NULL) {
    fprintf(f_debug, "first_token: %p\n", cache->first_token);
    fprintf(f_debug, "last_token: %p\n", cache->last_token);
    fprintf(f_debug, "token_count: %lu\n", cache->token_count);
    fprintf(f_debug, "pragma_count: %lu\n", cache->pragma_count);
    for (ctp = cache->first_token; ctp != NULL; ctp = ctp->next) {
      if (count != 0) fprintf(f_debug, "\n");
      fprintf(f_debug, "Token %lu:\n", count++);
      fprintf(f_debug, "  kind: %s", token_names[(int)ctp->token]);
      if ((a_token_kind)ctp->token == (a_token_kind)tok_identifier &&
          ctp->extra_info_kind == (a_token_extra_info_kind)teik_identifier) {
        fprintf(f_debug, " %s",
                ctp->variant.locator.symbol_header->identifier);
      }  /* if */
      fprintf(f_debug, "\n");
      fprintf(f_debug, "  sequence_number: %lu\n", ctp->token_sequence_number);
      if (ctp->extra_info_kind != (a_token_extra_info_kind)teik_none &&
          ctp->extra_info_kind != (a_token_extra_info_kind)teik_identifier) {
        char	*s;
        switch (ctp->extra_info_kind) {
          case teik_identifier:     s = "identifier"; break; /* not used */
          case teik_constant:       s = "constant"; break;
          case teik_pragma:         s = "pragma"; break;
          case teik_pp_token:       s = "pp_token"; break;
          case teik_extracted_body: s = "extracted_body"; break;
          case teik_asm_string:     s = "asm_string"; break;
          default:                  unexpected_condition();
        }  /* switch */
        fprintf(f_debug, "  extra_info_kind: %s\n", s);
      }  /* if */
      if (ctp->extra_info_kind == (a_token_extra_info_kind)teik_pragma) {
        a_pending_pragma_ptr	ppp;
        for (ppp = ctp->variant.pragmas; ppp != NULL; ppp = ppp->next) {
          fprintf(f_debug, "  Pragma: %s\n",
                                     pragma_ids[(int)ppp->descr_ptr->kind]);
        }  /* for */
      }  /* if */
    }  /* for */
  }  /* if */
}  /* db_token_cache */


unsigned long show_lexical_space_used(void)
/*
Display and return the amount of space used for various lexical tables.
*/
{
  unsigned long num, size, total, grand_total = 0;

  /* Subtract the number of tokens and pragmas used in reusable caches from
     the total number allocated.  Reusable cached tokens and pragmas will be
     reported separately. */
  num_cached_tokens_allocated -= num_cached_tokens_in_reusable_caches;
  num_pending_pragmas_allocated -= num_pragmas_in_reusable_caches;

  db_space_used_header("Lexical table use:");

  db_space_used_lost("orig line modif", avail_orig_line_modifs,
                     num_orig_line_modifs_allocated, an_orig_line_modif);
  db_space_used_lost("source line modif", avail_source_line_modifs,
                     num_source_line_modifs_allocated, a_source_line_modif);
  db_space_used_lost("cached token", avail_cached_tokens,
                     num_cached_tokens_allocated, a_cached_token);
  db_space_used("reusable cached token",
                 num_cached_tokens_in_reusable_caches, a_cached_token);
  db_space_used_lost("cached constant", avail_cached_constants,
                     num_cached_constants_allocated, a_constant);
  db_space_used_lost("cache stack entry", avail_reusable_cache_entries,
                     num_reusable_cache_entries_allocated,
                     a_reusable_cache_entry);
  db_space_used_lost("pending pragma entry", avail_pending_pragmas,
                     num_pending_pragmas_allocated,
                     a_pending_pragma);
  db_space_used_lost("stop token stack entry", avail_stop_token_stack_entries,
                     num_stop_token_stack_entries_allocated,
                     a_stop_token_stack_entry);
  db_space_used("reusable cache pragmas",
                 num_pragmas_in_reusable_caches, a_pending_pragma);
  db_space_used("pragma kind descriptions", num_pragma_descriptions_allocated,
                a_pragma_kind_description);
  db_space_used("file suffixes", num_file_suffixes_allocated,
                a_file_suffix);
  db_space_used("include file histories", num_include_file_histories_allocated,
                an_include_file_history);
  db_space_used_other("cached pp token strings", cached_pp_token_string_space,
                      "");
  grand_total += cached_pp_token_string_space;

  total = after_end_of_curr_source_line - curr_source_line;
  db_space_used_general_buffer("curr_source_line", total);
  if (size_pp_dir_string_buffer != 0) {
    db_space_used_general_buffer
          ("pragma string", ((unsigned long)size_pp_dir_string_buffer));
  }  /* if */

  if (after_end_of_raw_listing_buffer != NULL) {
    total = after_end_of_raw_listing_buffer - raw_listing_buffer;
    db_space_used_general_buffer("raw_listing_buffer", total);
  }  /* if */

  db_space_used_total();

  return (grand_total);
}  /* show_lexical_space_used */
#endif /* DEBUG */


static void init_include_file_suffixes(void)
/*
Create the include file suffix list used for header files with no suffix.
*/
{
  if (include_file_suffixes != NULL &&
      *include_file_suffixes != '\0') {
    include_file_suffix_list =
                        conv_string_to_file_suffix_list(include_file_suffixes);
  } else {
    /* The list is empty.  The empty suffix should be included in such
       cases. */
    add_to_file_suffix_list(&include_file_suffix_list, "", 0);
  }  /* if */
}  /* init_include_file_suffixes */


void init_name_linkage_constants(void)
/*
Create an array of string constants from the string literals that describe
which name linkages are recognized.  This process ensures that any needed
host-target conversions are performed.
*/
{
  a_name_linkage_kind  kind;
  a_boolean     unterminated;
  unsigned long num_chars;
  an_error_code err_code;
  char          *err_pos;

  name_linkage_constants =
                   (a_constant_ptr)alloc_fe((int)nlk_last*sizeof(a_constant));
  for (kind = (a_name_linkage_kind)nlk_cplusplus_external;
       (int)kind < (int)nlk_last;
       kind = (a_name_linkage_kind)(kind + 1)) {
    char      *name_linkage = name_linkage_kind_names[kind];
    sizeof_t  orig_len = strlen(name_linkage);
    /* First initialize the current source line to scan the string. */
    ensure_min_curr_source_line_length(orig_len+2+2*LE_ESCAPE_LEN);
    curr_source_line[0] = '"';
    strcpy(curr_source_line+1, name_linkage);
    curr_source_line[orig_len+1] = '"';
    curr_source_line[orig_len+2] = LE_ESCAPE;
    curr_source_line[orig_len+3] = LE_NEWLINE;
    curr_source_line[orig_len+4] = LE_ESCAPE;
    curr_source_line[orig_len+5] = LE_END_OF_LINE;
    start_of_curr_token = curr_char_loc = curr_source_line;
    /* Tokenize the string. */
    curr_char_loc++;
    num_chars = 0;
    unterminated = accum_quoted_string(&num_chars,
                                       /*is_header_name=*/FALSE,
                                       /*is_wide=*/FALSE,
                                       '"');
    check_assertion(unterminated == FALSE);
    /* Convert it to internal form. */
    conv_string_literal(num_chars, &err_code, &err_pos);
    check_assertion(err_code == ec_no_error);
    /* Copy the result for later use. */
    copy_constant(&const_for_curr_token, name_linkage_constants+(int)kind);
  }  /* for */
}  /* init_name_linkage_constants */


void lexical_one_time_init(void)
/*
Do one-time initialization of variables related to lexical processing.
(Variables that need to be reinitialized with each new translation unit
are handled in lexical_init.)
*/
{
  register int c;  /* Has to be "int" so "for" loop will work. */

  /* Do the initial allocation for curr_source_line.  (Since the space is
     allocated in general storage, it does not need to be reallocated for
     each source file; for the same reason, after_end_of_curr_source_line
     should not be reset.)  The space will be reallocated (larger) if
     necessary, but the size here should be big enough for the expected
     cases. */
  /* Allocate one more byte than required, so that a pointer past the end
     will not have the same address as a pointer to the next object in
     memory. */
  curr_source_line = alloc_general(
                            (sizeof_t)(CURR_SOURCE_LINE_INITIAL_ALLOCATION+1));
  after_end_of_curr_source_line = curr_source_line +
                                    CURR_SOURCE_LINE_INITIAL_ALLOCATION;
  if (f_raw_listing != NULL) {
    /* Similar allocation for raw_listing_buffer.  Similar reasoning. */
    raw_listing_buffer = alloc_general(
                              (sizeof_t)RAW_LISTING_BUFFER_INITIAL_ALLOCATION);
    after_end_of_raw_listing_buffer = raw_listing_buffer +
                                        RAW_LISTING_BUFFER_INITIAL_ALLOCATION;
    clear_raw_listing_buffer();
  }  /* if */
#if CHECKING
  /* Check that the table of token names is correctly initialized.  This
     guards against someone changing the enumeration and forgetting to
     update token_names. */
  if (token_names[(int)tok_last] == NULL ||
      strcmp(token_names[(int)tok_last], "last") != 0) {
    internal_error(
                 "lexical_init: initialization of token_names is not correct");
  }  /* if */
  /* Check that the table of opname kinds is correctly initialized. */
  if (opname_kind_for_token[(int)tok_last] != (an_opname_kind)onk_last) {
    internal_error("lexical_init: bad init of opname_kind_for_token");
  }  /* if */
#endif /* CHECKING */
  /* Initialize is_id_char to the characters that can appear in an identifier
     after the first character (i.e., a-z, A-Z, 0-9, and "_").
     See standard, 3.1.2.  Also used in scanning pp-numbers; the same
     set applies.  See standard, 3.1.8. */
  for (c = CHAR_MIN; c <= CHAR_MAX; c++) {
    is_id_char[c-CHAR_MIN] = (isalpha((unsigned char)c) ||
                              isdigit((unsigned char)c));
  }  /* for */
  is_id_char['_' - CHAR_MIN] = TRUE;
  if (allow_dollar_in_id_chars) {
    is_id_char['$' - CHAR_MIN] = TRUE;
  }  /* if */
  /* Some character sets use some C special characters as letters, e.g.,
     the position that is ASCII "]" is "U umlaut" in German.  Take those
     characters back.  Note that a setlocale call has been done already. */
  /* "@" and "`" are letters in some European character sets, but they're
     not used in C so there's no need to take them back. */
  is_id_char['[' - CHAR_MIN] = FALSE;
  is_id_char['\\'- CHAR_MIN] = FALSE;
  is_id_char[']' - CHAR_MIN] = FALSE;
  is_id_char['^' - CHAR_MIN] = FALSE;
  is_id_char['{' - CHAR_MIN] = FALSE;
  is_id_char['|' - CHAR_MIN] = FALSE;
  is_id_char['}' - CHAR_MIN] = FALSE;
  is_id_char['~' - CHAR_MIN] = FALSE;
  /* Also initialize pp_lexical_category, used to determine whether or
     not extra token-separating blanks are required between tokens resulting
     from macro expansion.  See gen_pp_output_for_curr_line. */
  for (c = CHAR_MIN; c <= CHAR_MAX; c++) {
    if (is_id_char[c-CHAR_MIN] || c == '.') {
      pp_lexical_category[c-CHAR_MIN] = PLC_ID_OR_NUMBER;
    } else {
      switch (c) {
        /* White-space characters are always singletons: */
        case '\f':
        case VERTICAL_TAB_CHARACTER:
        case '\t':
        case ' ':
        case '\n':
        /* Operators and punctuators that don't appear as part of other
           tokens are also singletons (see 3.1.5, 3.1.6). */
        case '[':  case ']':  case '{':  case '}':  case '(':  case ')':
        case ',':  case '~':  case ';':  case '?':
          pp_lexical_category[c-CHAR_MIN] = PLC_SINGLETON;
          break;
        case ':':
          /* In C++, ":" can be part of "::". */
          pp_lexical_category[c-CHAR_MIN] =
                            (C_dialect == C_dialect_cplusplus) ? PLC_OTHER :
                                                                 PLC_SINGLETON;
          break;
        default:
          pp_lexical_category[c-CHAR_MIN] = PLC_OTHER;
      }  /* switch */
    }  /* if */
  }  /* for */
  /* Compute opname_names from opname_kind_for_token and token_names. */
  (void)memzero((char *)opname_names, sizeof(opname_names));
  { int  tok_kind, opname_kind;
    char *str;

    for (tok_kind = 0; tok_kind < (int)tok_last; tok_kind++) {
      opname_kind = opname_kind_for_token[tok_kind];
      if (opname_kind != (int)onk_none) {
        str = token_names[tok_kind];
        /* A few opname kinds are made up of two tokens and require some
           special handling. */
        if (opname_kind == (int)onk_function_call) {
          str = "()";
        } else if (opname_kind == (int)onk_subscript) {
          str = "[]";
        }  /* if */
        opname_names[opname_kind] = str;
      }  /* if */
    }  /* for */
    /* new[] and delete[] do not map to a single token. */
    opname_names[(int)onk_array_new] = "new[]";
    opname_names[(int)onk_array_delete] = "delete[]";
#if CHECKING
    /* Make sure all the slots were initialized. */
    for (opname_kind = (int)onk_none+1;
         opname_kind < (int)onk_last;
         opname_kind++) {
      if (opname_names[opname_kind] == NULL) {
        internal_error("lexical_init: bad init of opname_names");
      }  /* if */
    }  /* for */
#endif /* CHECKING */
  }
#if DEBUG
  num_file_suffixes_allocated = 0;
#endif /* DEBUG */
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
  /* Create the instantiation file suffix list. */
  implicit_instantiation_file_suffix_list =
       conv_string_to_file_suffix_list(DEFAULT_INSTANTIATION_FILE_SUFFIX_LIST);
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
  init_include_file_suffixes();
  /* Save variables from lexical.h and lexical.c that are needed for
     precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(curr_seq_number),
      pch_saved_var_array_elem(seq_number_last_read),
      pch_saved_var_array_elem(avail_orig_line_modifs),
      pch_saved_var_array_elem(avail_source_line_modifs),
      pch_saved_var_array_elem(sequence_id_for_source_line_modifs),
      pch_saved_var_array_elem(last_token_sequence_number_used),
      pch_saved_var_array_elem(avail_cached_tokens),
      pch_saved_var_array_elem(avail_cached_constants),
      pch_saved_var_array_elem(avail_reusable_cache_entries),
      pch_saved_var_array_elem(avail_pending_pragmas),
      pch_saved_var_array_elem(avail_stop_token_stack_entries),
      pch_saved_var_array_elem(include_file_history_list),
      pch_saved_var_array_elem(name_linkage_constants),
      pch_saved_var_array_elem(curr_stop_token_stack_entry),
#if DEBUG
      pch_saved_var_array_elem(num_orig_line_modifs_allocated),
      pch_saved_var_array_elem(num_source_line_modifs_allocated),
      pch_saved_var_array_elem(num_cached_tokens_allocated),
      pch_saved_var_array_elem(num_cached_tokens_in_reusable_caches),
      pch_saved_var_array_elem(num_pragmas_in_reusable_caches),
      pch_saved_var_array_elem(num_cached_constants_allocated),
      pch_saved_var_array_elem(num_reusable_cache_entries_allocated),
      pch_saved_var_array_elem(num_pending_pragmas_allocated),
      pch_saved_var_array_elem(num_pragma_descriptions_allocated),
      pch_saved_var_array_elem(num_stop_token_stack_entries_allocated),
      pch_saved_var_array_elem(num_include_file_histories_allocated),
      pch_saved_var_array_elem(cached_pp_token_string_space),
#endif /* DEBUG */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  register_trans_unit_variable(next_token_is_top_level_decl_start);
  register_trans_unit_variable(curr_stop_token_stack_entry);
  register_trans_unit_variable(curr_token);
  register_trans_unit_variable(curr_token_pragmas);
  register_trans_unit_variable(const_for_curr_token);
  register_trans_unit_variable(pos_curr_token);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  register_trans_unit_variable(end_pos_curr_token);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  register_trans_unit_variable(start_of_curr_token);
  register_trans_unit_variable(end_of_curr_token);
  register_trans_unit_variable(len_of_curr_token);
  register_trans_unit_variable(cached_token_rescan_list);
  register_trans_unit_variable(reusable_cache_stack);
  register_trans_unit_variable(any_initial_get_token_tests_needed);
  register_trans_unit_variable(treat_newline_as_token);
  register_trans_unit_variable(curr_token_asm_string);
  register_trans_unit_variable(curr_token_sequence_number);
}  /* lexical_one_time_init */


void lexical_reset(void)
/*
Initialize variables that are used to record the state of the lexical
routines.  These variables are reset after the initial scan that is
done to determine whether a precompiled header may be used.
*/
{
  /* Variables in lexical.h: */
  depth_input_stack = -1;
  curr_token = tok_error;
  curr_ise = NULL;
  if (is_primary_translation_unit) {
    /* These must be reset here after the PCH prefix has been read. */
    curr_seq_number = 0;
    seq_number_last_read = 0;
    last_token_sequence_number_used = NO_TOKEN_SEQUENCE_NUMBER;
  }  /* if */
  orig_line_modif_list = NULL;
  end_orig_line_modif_list = NULL;
  source_line_modif_list = NULL;
  line_start_source_line_modif = NULL;
  sequence_id_for_source_line_modifs = 0;
  delete_source_from_loc = NULL;
  curr_token_pragmas = NULL;
  /* Static variables in lexical.c: */
  curr_input_stream = NULL;
  eof_read_on_curr_input_stream = FALSE;
  at_end_of_source_file = FALSE;
  after_end_of_all_source = FALSE;
  init_do_not_put_curr_line_in_pp_output = TRUE;
  curr_raw_listing_line_code = '\0';
  cached_token_rescan_list = NULL;
  reusable_cache_stack = NULL;
  any_initial_get_token_tests_needed = FALSE;
  treat_newline_as_token = FALSE;
  curr_token_sequence_number = NO_TOKEN_SEQUENCE_NUMBER;
  any_tokens_fetched_from_curr_input_file = FALSE;
  curr_token_asm_string = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  scanning_microsoft_asm = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if ASM_SUPPORT_NEEDED
  asm_func_body_buffer = NULL;
  size_asm_func_body_buffer = 0;
#endif /* ASM_SUPPORT_NEEDED */
}  /* lexical_reset */


void lexical_trans_unit_init(void)
/*
Initialize variables that are specific to a given translation unit.
*/
{
  lexical_reset();
  /* The following variable is declared in decls.h, but initialized here
     since it is related to tokenization. */
  next_token_is_top_level_decl_start = FALSE;
  include_file_history_list = NULL;
  /* Clear the set of tokens on which to stop a flush following a
     syntax error. */
  curr_stop_token_stack_entry = NULL;
  push_stop_token_stack();
  clear_stop_tokens();
}  /* lexical_trans_unit_init */


void lexical_init(void)
/*
Initialize static variables related to the lexical routines.  This is done
as a subroutine (rather than relying on static initialization) so that it
can be redone to compile more than one source file in a single invocation
of the front end.
*/
{
  /* Variables in lexical.h: */
  avail_orig_line_modifs = NULL;
  avail_source_line_modifs = NULL;
  sequence_id_for_source_line_modifs = 0;
  delete_source_from_loc = NULL;
  /* Static variables in lexical.c: */
  avail_cached_tokens = NULL;
  avail_cached_constants = NULL;
  avail_reusable_cache_entries = NULL;
  avail_stop_token_stack_entries = NULL;
  avail_pending_pragmas = NULL;
  dollar_in_id_diagnostic_issued = FALSE;
#if TOKENS_TO_STRING_NEEDED
  /* Initialize the output control block for the il-to-str routines. */
  clear_il_to_str_output_control_block(&octl);
  octl.output_str = put_str_to_temp_text_buffer;
  octl.gen_compilable_code = TRUE;
#endif /* TOKENS_TO_STRING_NEEDED */
#if DEBUG
  num_orig_line_modifs_allocated = 0;
  num_source_line_modifs_allocated = 0;
  num_cached_tokens_allocated = 0;
  num_cached_tokens_in_reusable_caches = 0;
  num_pragmas_in_reusable_caches = 0;
  num_cached_constants_allocated = 0;
  num_reusable_cache_entries_allocated = 0;
  num_pending_pragmas_allocated = 0;
  num_stop_token_stack_entries_allocated = 0;
  num_pragma_descriptions_allocated = 0;
  num_include_file_histories_allocated = 0;
  cached_pp_token_string_space = 0;
#endif /* DEBUG */
#if CHECKING
  /* Make sure the UCN table is properly formed.  Each element of the
     array must be a start-end range where the ending value is greater
     than the starting value, and the starting value is greater than the
     end of the previous range. */
  {
    unsigned long	last_end = 0;
    unsigned int	i;
    for (i = 0; i < sizeof(UCN_table) / sizeof(a_UCN_range); i++) {
      a_UCN_range_ptr	p = &UCN_table[i];
      check_assertion_str2(p->start <= p->end && p->start > last_end,
                           "lexical_init:",
                           "UCN_table is not sorted properly");
      last_end = p->end;
    }  /* for */
  }
#endif /* CHECKING */
}  /* lexical_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
