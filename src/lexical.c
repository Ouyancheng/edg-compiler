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

lexical.c -- Source input and lexical scanning routines.

These routines and data structures handle reading of source lines
and parsing of them into tokens.

*/

#include "basics.h"
#include "target.h"
#include "lexical.h"
#include "preproc.h"
#include "error.h"
#include "host_envir.h"
#include "cmd_line.h"
#include "debug.h"
#include "mem_manage.h"
#include "symbol_tbl.h"
#include "macro.h"
#include "il.h"
#include "literals.h"
#include "statements.h"
#include "decls.h"
#include "templates.h"
#include "pragma.h"

#if ASM_FUNCTION_ALLOWED
#include "asm_func.h"
#endif /* ASM_FUNCTION_ALLOWED */


/*
Return TRUE if tok is a token kind that is a literal constant.
*/
#define is_literal_constant_token(tok)                                \
  (tok == tok_float_constant || tok == tok_int_constant ||            \
   tok == tok_char_constant  || tok == tok_string_literal)


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
                                        reusable_cache_stack != NULL)	   \


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
static a_seq_number
		seq_number_last_read;
			/* The sequence number of the physical line last read
			   from curr_input_stream.  This is often not the
			   same as the line number in the file.  Once the
			   end of file on the primary source file has been
			   passed, this indicates a sequence number one past
			   the highest sequence number actually read, as an
			   indication of a sort of end-of-file line. */
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
                           displayed.*/


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

/*
Flag that indicates whether a dollar sign was found in any identifiers.
Used in strict ANSI mode to make sure that this diagnostic is only given
once per compilation unit.
*/
static a_boolean
		dollar_in_id_diagnostic_issued;

/*
Array of identifier lookup options indexed by identifier lookup mode.  Used
to translate the lookup mode into a set of identifier lookup options.
*/
static an_id_lookup_options_set idl_options_for_lookup_mode[ilm_last + 1] = {
  /* ilm_normal */		IDL_NO_OPTIONS,
  /* ilm_class */		IDL_MUST_BE_CLASS,
  /* ilm_tag */			IDL_MUST_BE_TAG,
  /* ilm_tentative_type */	IDL_DO_NOT_MAKE_PROJECTION_IF_NOT_TYPE_NAME,
  /* ilm_ctor_initializer_name */ IDL_SKIP_CURR_FUNCTION_SCOPE,
  /* ilm_last */		IDL_NO_OPTIONS
};


#if INSTANTIATION_BY_IMPLICIT_INCLUSION
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

static a_file_suffix_ptr
		 implicit_instantiation_file_suffix_list = NULL;
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */


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
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
                num_file_suffixes_allocated,
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
		num_reusable_cache_entries_allocated;
#endif /* DEBUG */


static void unimplemented_keyword_diagnostic(a_symbol_ptr  sym)
/*
Issue a diagnostic on unimplemented keywords.
*/
{
  if (!sym->variant.keyword.unimplemented_diagnostic_issued) {
    an_error_severity severity;
    severity = strict_ansi_mode ? strict_ansi_error_severity : es_remark;
    sym_diagnostic(severity, ec_unimplemented_keyword, sym);
    sym->variant.keyword.unimplemented_diagnostic_issued = TRUE;
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
by the caller (including token and extra_info_kind).
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
  ctp->source_position = pos_curr_token;                                \
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
  ctp->extra_info_kind = (a_token_extra_info_kind)teik_pragma;
  ctp->variant.pragmas = curr_token_pragmas;
  ctp->token = (a_byte_token_kind)tok_error;
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


void terminate_token_cache(a_token_cache *cache)
/*
Save an end-of-source token on the end of the list of tokens saved in *cache.
*/
{
  a_cached_token_ptr ctp;

  /* Build an entry for the end-of-source token. */
  alloc_cached_token(ctp);
  ctp->token = (a_byte_token_kind)tok_end_of_source;
  ctp->token_sequence_number = NO_TOKEN_SEQUENCE_NUMBER;
  ctp->extra_info_kind = (a_token_extra_info_kind)teik_none;
  /* Add the end-of-source token to the end of the cache. */
  add_cached_token_to_cache(ctp, cache);
}  /* terminate_token_cache */


void cache_curr_token(a_token_cache *cache)
/*
Save the current token on the end of the list of tokens saved in *cache.
This is used to save tokens for later rescanning.  This may not be used
for pp-tokens.
*/
{
  a_cached_token_ptr ctp;

#if CHECKING
  if (fetch_pp_tokens) {
    internal_error("cache_curr_token: called with fetch_pp_tokens TRUE");
  }  /* if */
#endif /* CHECKING */
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
  ctp->token_sequence_number = curr_token_sequence_number;
  if (curr_token == tok_identifier || curr_token == tok_ptr_to_member) {
    /* Identifier -- save information about it. */
    ctp->extra_info_kind = (a_token_extra_info_kind)teik_identifier;
    ctp->variant.locator = locator_for_curr_id;
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

  /* Back up to the identifier. */
  unget_token();
  sym = normal_id_lookup(&locator_for_curr_id, IDL_NO_OPTIONS);
  if (sym != NULL && sym->kind == (a_symbol_kind)sk_class_template) {
    result = TRUE;
  }  /* if */
  /* Return to the token that was the current token when we were called. */
  (void)get_token();
  return result;
}  /* is_template_reference */


static void cache_token_stream_until_matching_token(a_token_cache *cache)
/*
Given curr_token of '(', '[', or '{', copy tokens into the token cache
specified by cache up to but not including the corresponding closing token,
')', ']', or '}', respectively.  Return immediately if end of source is
reached.  (This routine is similar to flush_until_matching_token, but instead
of throwing tokens away it adds them to the specified token cache.)
*/
{
  a_token_kind  closing_token;
  a_token_kind	prev_token = tok_error;
  int           paren_count = 0, bracket_count = 0, brace_count = 0;
  a_boolean	done = FALSE;

  db_enter(4, "cache_token_stream_until_matching_token");
  /* Determine the closing token that corresponds to curr_token. */
  switch (curr_token) {
    case tok_lparen:    closing_token = tok_rparen;   break;
    case tok_lbracket:  closing_token = tok_rbracket; break;
    case tok_lbrace:    closing_token = tok_rbrace;   break;
    case tok_lt:        closing_token = tok_gt;       break;
#if CHECKING
    default:
      internal_error("cache_token_stream_until_matching_token: bad token");
#endif /* CHECKING */
  }  /* switch */
  /* Cache the current token, and advance to its successor. */
  cache_curr_token(cache);
  (void)get_token();
  /* Keep looping through successive tokens until the corresponding closing
     token is found at level zero (i.e., not within a nesting of parens,
     brackets, or braces). */
  while (!done && (curr_token != closing_token ||
                   paren_count != 0 || bracket_count != 0 ||
		   brace_count != 0)) {
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
    /* Always stop the flush on end of source. */
    if (curr_token == tok_end_of_source) break;
    /* Check for the start of a template parameter list. */
    if (curr_token == tok_lt && prev_token == tok_identifier) {
      if (is_template_reference()) {
        cache_token_stream_until_matching_token(cache);
      }  /* if */
    }  /* if */
    /* None of the conditions was satisfied, so keep going. */
    cache_curr_token(cache);
    prev_token = curr_token;
    (void)get_token();
  }  /* while */
  db_exit();
}  /* cache_token_stream_until_matching_token */


void cache_token_stream(a_token_cache      *cache,
                        a_token_set_array  stop_tokens)
/*
Copy the current token and succeeding tokens into the token cache specified
by cache up to but not including the first token that matches a member of
the stop tokens array.  Return immediately if end of source is reached.
(This routine is similar to flush_tokens, but instead of throwing tokens
away it adds them to the specified token cache.)
*/
{
  a_token_kind	prev_token = tok_error;
  db_enter(4, "cache_token_stream");
  /* Loop through the tokens, beginning with the current token and stopping
     when a token in the stop token array is found.  Whenever a '(', '[', or
     '{' is encountered, ignore the stop token array until the corresponding
     ')', ']', or '}' is reached. */
  while (stop_tokens[(int)curr_token] == 0) {
    if (curr_token == tok_lparen || curr_token == tok_lbracket ||
        curr_token == tok_lbrace ||
        (curr_token == tok_lt && prev_token == tok_identifier &&
         is_template_reference())) {
      cache_token_stream_until_matching_token(cache);
    }  /* if */
    /* Stop immediately when end of source is reached. */
    if (curr_token == tok_end_of_source) break;
    /* Add the current token to the cache and advance to its successor. */
    cache_curr_token(cache);
    prev_token = curr_token;
    (void)get_token();
  }  /* while */
  /* Leave error_position associated with what is now curr_token. */
  set_err_pos_to_curr_token();
  db_exit();
}  /* cache_token_stream */


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
the original source with no need to worry about detection of the the
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


/*
Macro to free a cached token entry, i.e., to put it on the avail list to be
reused.  If the entry points to a cached constant entry, free it, too.
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
  /* Entry is for a token (normal case). */
  ctoken = (a_token_kind)ctp->token;
  pos_curr_token = ctp->source_position;
  error_position = pos_curr_token;
  curr_token_sequence_number = ctp->token_sequence_number;
  start_of_curr_token = end_of_curr_token = NULL;
  len_of_curr_token = 0;
  if (ctp->extra_info_kind == (a_token_extra_info_kind)teik_identifier) {
    /* For an identifier, restore the locator. */
    locator_for_curr_id = ctp->variant.locator;
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
  /* Entry is for a token (normal case). */
  ctoken = (a_token_kind)ctp->token;
  pos_curr_token = ctp->source_position;
  error_position = pos_curr_token;
  curr_token_sequence_number = ctp->token_sequence_number;
  start_of_curr_token = end_of_curr_token = NULL;
  len_of_curr_token = 0;
  if (ctp->extra_info_kind == (a_token_extra_info_kind)teik_identifier) {
    /* For an identifier, restore the locator. */
    locator_for_curr_id = ctp->variant.locator;
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
  *dslmp->inserted_chars = '\0'; \
  dslmp->inserted_text = dslmp->end_inserted_text = dslmp->inserted_chars; \
  dslmp->is_for_comment = for_comment; \
}  /* add_deletion_source_line_modif */


/*
Add a source line modification to indicate replacement of num_chars
characters staring at line_loc by a space.  This is used for deletion
of comments.  A different space string must be used for each comment
in the current line so that one can get back from each one to the
right place based only on line position; for this reason, the
space-null string is placed in the a_source_line_modif entry.
*/
#define replace_source_string_by_space(line_loc, num_chars) \
{ a_source_line_modif_ptr rslmp; \
  rslmp = add_source_line_modif(line_loc, num_chars, \
                                (char *)NULL, (char *)NULL); \
  rslmp->inserted_chars[0] = ' '; \
  rslmp->inserted_chars[1] = '\0'; \
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
void gen_pp_line_info(char kind,
		      int  increment)
/*
Write a line-identification directive to f_pp_output as part of preprocessor
output.  It should identify the current line plus "increment" lines.
The kind character is the third operand: '1' for entry into a file,
'2' for exit from a file, and ' ' for anything else.  This routine should
only be called when generate_pp_output is TRUE.
*/
{
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
      fprintf(f_pp_output, "#line %lu \"%s\"",
                           (a_line_number)(curr_ise->line_number+increment),
                           curr_ise->file_name);
    } else {
      /* pcc version. */
      fprintf(f_pp_output, "# %lu \"%s\"",
                           (a_line_number)(curr_ise->line_number+increment),
                           curr_ise->file_name);
#if GEN_EXTRA_LINE_ID_INFO
      if (kind != ' ') {
        putc(' ', f_pp_output);
        putc(kind, f_pp_output);
      }  /* if */
#endif /* GEN_EXTRA_LINE_ID_INFO */
    }  /* if */
    putc('\n', f_pp_output);
    next_seq_in_pp_output = curr_seq_number + increment;
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
In pcc mode, no blank is ever put out.

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
    if (!pcc_preprocessing_mode) {                                    \
      if ((cat_prev_ch = pp_lexical_category[prev_ch-CHAR_MIN]) ==    \
                                                         PLC_SINGLETON || \
          (cat_ch = pp_lexical_category[ch-CHAR_MIN]) == PLC_SINGLETON) { \
        /* At least one is a singleton, so no space is needed. */     \
      } else if (cat_prev_ch != cat_ch &&                             \
        /* The two characters have different categories (i.e., one is \
           a character that can appear in identifiers or pp-numbers,  \
           and the other is not).  We probably do not need the space. \
           However, check for one bizarre special case having to do   \
           with pp-numbers (3.1.8): "e" or "E" followed by "+" or "-" \
           can appear in a pp-number. */                              \
                 ((prev_ch != 'e' && prev_ch != 'E') ||               \
                  (ch != '+' && ch != '-'))) {                        \
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
      if (curr_seq_number <= next_seq_in_pp_output+5) {
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
        gen_pp_line_info(' ', 0);
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
      if (fputs(curr_source_line, f_pp_output) == EOF) {
        /* Error in writing the pp output file.  This check is done on 
           most lines and supplements the check done when the file is closed.
           Checking here is so that a disk full error is caught fairly
           quickly. */
        str_catastrophe(ec_file_write_error, "preprocessing output");
      }  /* if */
      next_seq_in_pp_output++;
      prev_pp_output_line_was_complete = TRUE;
    } else {
      /* Construct the preprocessing output from the source line and the
         list of modifications.  Start at the beginning, and scan through
         characters.  Remove end-of-token markers.  On hitting attention
         markers, find and process the associated source line modification
         entries.  On hitting a null, leave a source line modification entry
         or (if not in a modification entry) stop. */
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
          token_start = TRUE;
        } else if (ch == '\0') {
          /* Null indicates either the end of the whole line or the end
             of a macro expansion.  Exit the loop if at the end of the whole
             line. */
          if (slmp == NULL) break;
          /* If we've just finished the inserted text at the start of the
             source line, and the main part of the line is not supposed to
             be displayed (see comment above), stop here. */
          if (slmp == line_start_source_line_modif &&
              do_not_put_curr_line_in_pp_output) break;
          /* End of a macro.  Pick up after the invocation text. */
          walk_out_of_insertion(slmp, loc_in_line);
          token_start = TRUE;
        } else {
          if (ch == END_OF_TOKEN_MARKER) {
            /* Do not output end-of-token markers. */
            token_start = TRUE;
          } else {
            /* Normal character. */
            /* Output a blank to separate the character from the previous
               character if necessary to prevent tokenizing confusion. */
            token_separator_blank_if_needed(ch, prev_ch, token_start,
                                            putc(' ', f_pp_output));
            /* Output the character. */
            putc(ch, f_pp_output);
            /* Count newlines.  This is done inside the loop because there are
               cases where no newline is output (two lines are joined in the
               output), and cases where one line has two newlines, and we
               don't want to miscount. */
            prev_pp_output_line_was_complete = (ch == '\n');
            if (prev_pp_output_line_was_complete) next_seq_in_pp_output++;
          }  /* if */
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
         of the line. */
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
       ??=pragma hello / *
       * / there 
     (where the / * and * / are really comment delimiters, of course).
     For that case, we'll get here with "#pragma hello " with no newline,
     and then later with " there".  The whole line is written when the
     piece with the newline arrives.  That's because the raw listing
     output has to look like
       N??=pragma hello / *
       N* / there
       X#pragma hello   there
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
      } else if (ch == '\0') {
        /* Null indicates either the end of the whole line or the end
           of a macro expansion.  Exit the loop if at the end of the whole
           line or the end of the insert at the beginning of the line. */
        if (slmp == NULL) break;
        if (slmp == line_start_source_line_modif) break;
        /* End of a macro.  Pick up after the invocation text. */
        walk_out_of_insertion(slmp, loc_in_line);
        token_start = TRUE;
      } else {
        if (ch == END_OF_TOKEN_MARKER) {
          /* Do not output end-of-token markers. */
          token_start = TRUE;
        } else {
          /* Normal character. */
          /* Output a blank to separate the character from the previous
             character if necessary to prevent tokenizing confusion. */
          token_separator_blank_if_needed(ch, prev_ch, token_start,
                                          add_char_to_raw_listing_buffer(' '));
          /* Output the character. */
          add_char_to_raw_listing_buffer(ch);
          /* If the character is a newline, we have a complete line and we
             should output it or throw it away now. */
          if (ch == '\n') {
            if (must_display_raw_listing_buffer) {
              *loc_in_raw_listing_buffer = '\0';
              putc('X', f_raw_listing);
              fputs(raw_listing_buffer, f_raw_listing);
            }  /* if */
            clear_raw_listing_buffer();
          }  /* if */
        }  /* if */
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
          /* If the trigraph is "??/", which turns into "\", and it's at the
             end of a line, the "\" will indicate a line splice.  In that
             case, the "\" for the line splice should not be put out. */
          olmp_next = olmp->next;
          if (olmp_next != NULL && olmp_next->kind == olm_line_splice &&
              olmp_next->line_loc == olmp->line_loc) {
            /* Partially process the line-splice entry. */
            olmp = olmp_next;
            goto partially_process_line_splice;
          }  /* if */
          /* Normal trigraph. */
          loc_in_line = olmp->line_loc + 1;
          break;
        case olm_line_splice:
          putc('\\', f_raw_listing);
partially_process_line_splice:
          putc('\n', f_raw_listing);
          putc(curr_raw_listing_line_code, f_raw_listing);
          loc_in_line = olmp->line_loc;
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


void open_file_and_push_input_stack
                                (char                       *file_name,
                                 a_directory_name_entry_ptr search_path,
				 a_boolean		    is_include_file,
				 a_boolean	            is_system_include)
/*
Push the indicated file onto the input stack, so that the next time a line
is read, it will come from that file.  If the file cannot be opened,
generate a catastrophic error and do not return.  search_path gives the
list of directories to be tried, in order, or is NULL if there is no
search path.  file_name must be allocated in IL storage.
is_include_file is TRUE if the file is being read as the result of a
#include directive.  It is FALSE for implicitly included files.
is_system_include is TRUE for files included with the #include <file.h>
notation and FALSE for all other files.
*/
{
  char  *full_file_name, *display_name;
  FILE  *input_file;

  db_enter(2, "open_file_and_push_input_stack");
  input_file = open_file_for_input(file_name, search_path,
                                   /*replace_suffix=*/FALSE, &full_file_name,
                                   &display_name);
  check_assertion(input_file != NULL);
  push_input_stack(input_file, file_name, display_name, full_file_name,
                   is_include_file, is_system_include);
  db_exit();
}  /* open_file_and_push_input_stack */


#if !INSTANTIATION_BY_IMPLICIT_INCLUSION
/*ARGSUSED*/ /* <-- replace_suffix is used only if instantiation may use
                    implicit inclusion. */
#endif /* !INSTANTIATION_BY_IMPLICIT_INCLUSION */
FILE *open_file_for_input(char                       *file_name,
                          a_directory_name_entry_ptr search_path,
                          a_boolean                  replace_suffix,
                          char                       **full_file_name,
                          char                       **display_name)
/*
Try to open file_name, and return a pointer to the file if the open is
successful.  file_name must be allocated in IL storage.  If suffixes is
non-NULL, the suffix currently on file_name will be replaced by each
suffix in turn, in the specified order.  If search_path is non-NULL,
the search begins in the first directory on the path (for each suffix, if
appropriate), and proceeds until a file is found;  if search_path is NULL,
only the current directory is checked.  If the open is successful, the full
name of the file that is opened is returned in *full_file_name and the name
intended for use in diagnostics and other output is returned in *display_name.
If replace_suffix is FALSE, the open must be successful and a catastrophic
error will be issued if it is not; otherwise, a NULL file pointer will be
returned.
*/
{
  a_directory_name_entry_ptr  curr_directory_name_entry;
  char                        *temp_file_name, *prev_dir_name;
  FILE                        *new_input_file;
  a_boolean                   not_found = FALSE,
                              bad_format = FALSE,
                              bad_name = FALSE;
  /* Buffer in which directory names and file names are combined.  Longer
     names will bypass the buffer and be allocated directly via alloc_il. */
#define BUFFER_SIZE 130
  char                        buffer[BUFFER_SIZE];

  db_enter(2, "open_file_for_input");
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
    char               *suffix_loc;
    a_file_suffix_ptr  fsp;
    a_boolean          done = FALSE;

    /* We need to try a set of suffixes till we find a file we can open. */
    curr_directory_name_entry = search_path;
    for (;;) {
      if (is_absolute_file_name(file_name)) {
        /* Force the name to be copied to the buffer or to new storage. */
        temp_file_name = file_name;
        /* Set done to keep from doing the outer loop more than once. */
        done = TRUE;
      } else if (search_path == NULL) {
        /* No search path was provided, so we'll just return NULL. */
        break;
      } else {
        /* We need to traverse the search path.  Merge the current entry in
           the path with the file name and use that name as the base for
           replacing the suffixes. */
        temp_file_name = combine_dir_and_file_name(
                                        curr_directory_name_entry->dir_name,
                                        file_name, buffer, BUFFER_SIZE);
      }  /* if */
      if (temp_file_name == file_name) {
        if (strlen(file_name) < (sizeof_t)(BUFFER_SIZE - 1)) {
          /* Copy file_name into the buffer.  Its suffix will be replaced
             in the inner loop. */
          (void)strcpy(buffer, file_name);
          temp_file_name = buffer;
        } else {
          /* Since we're going to try to modify the file name in place, by
             replacing its current suffix with another, allocate storage
             for it. */
          temp_file_name = alloc_il((sizeof_t)(strlen(file_name)+1));
          (void)strcpy(temp_file_name, file_name);
        }  /* if */
      }  /* if */
      /* Loop through the linked list of suffixes. */
      suffix_loc = NULL;
      for (fsp = implicit_instantiation_file_suffix_list;
           fsp != NULL;
           fsp = fsp->next) {
        /* Replace the existing suffix with a new one. */
        temp_file_name = replace_file_name_suffix(fsp->suffix,
                                                  temp_file_name, buffer,
                                                  BUFFER_SIZE, &suffix_loc);
        /* Now try to open the modified file. */
        new_input_file = open_source_file(temp_file_name, &not_found,
                                          &bad_format, &bad_name);
        /* If not_found is FALSE then either temp_file_name is non-NULL
           (i.e., the input file was opened) or else there was an error
           on the open.  In either case, stop searching. */
        if (!not_found) {
          done = TRUE;
          break;
        }  /* if */
      }  /* for */
      /* Test for outer loop. */
      if (done || (curr_directory_name_entry =
                         curr_directory_name_entry->next) == NULL) {
        /* If done is TRUE it is because the file has been found or the
           directory path is not being searched.  If we are searching the
           directory path we want to stop after processing the last entry. */
        break;
      }  /* if */
    }  /* for */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
  } else {
    if (curr_ise == NULL || is_absolute_file_name(file_name)) {
      /* File name is absolute, so search path is not used. */
      /* Also used for primary source input file; search current directory. */
      temp_file_name = file_name;
      new_input_file = open_source_file(temp_file_name,
                                        &not_found, &bad_format, &bad_name);
    } else if (search_path == NULL) {
      /* No search path, so file can't be found.  Issue a catastrophic error.
         Use special message to make it clearer, since problem may be that
         there are no -I options on the command line. */
      str_catastrophe(ec_empty_include_search_path, file_name);
    } else {
      /* File name is relative, use search path. */
      prev_dir_name = NULL;
      for (curr_directory_name_entry = search_path;
           curr_directory_name_entry != NULL;
           curr_directory_name_entry = curr_directory_name_entry->next) {
        if (curr_directory_name_entry->dir_name == prev_dir_name) {
          /* Two directories with the same name are adjacent in the stack.
             No need to try to open the same file a second time. */
          continue;
        }  /* if */
        /* Try opening the file name with this directory name. */
        temp_file_name = combine_dir_and_file_name(
                                        curr_directory_name_entry->dir_name,
                                        file_name, buffer, BUFFER_SIZE);
        /* Now try opening the file.  Exit the loop on success. */
        new_input_file = open_source_file(temp_file_name, &not_found,
                                          &bad_format, &bad_name);
        if (new_input_file != NULL) {
          /* The file was opened successfully.  Exit the loop. */
          break;
        } else {
          /* Issue a catastrophic error if the file could not be opened
             because of an error. */
          if (bad_format) {
            str_catastrophe(ec_source_file_has_bad_format, file_name);
          } else if (bad_name) {
            str_catastrophe(ec_illegal_source_file_name, file_name);
          }  /* if */
        }  /* if */
        /* The file could not be found.  Keep looking. */
      }  /* while */
    }  /* if */
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
    if (temp_file_name == buffer) {
      temp_file_name = alloc_il((sizeof_t)(strlen(buffer)+1));
      (void)strcpy(temp_file_name, buffer);
    }  /* if */
    *full_file_name = temp_file_name;
    /* Note that *display_name gets the same name as full name.  This is
       a matter of taste. */
    *display_name = temp_file_name;
  }  /* if */
  db_exit();
  return new_input_file;
}  /* open_file_for_input */
  

void push_input_stack (FILE      *new_input_file,
                       char      *name_as_written,
                       char      *display_name,
                       char      *full_file_name,
		       a_boolean is_include_file,
		       a_boolean is_system_include)
/*
Push the indicated file onto the input stack.
*/
{
  int                isnum, times_name_appears;
  a_source_file_ptr  parent_file;

  db_enter(2, "push_input_stack");
#if DEBUG
  if (debug_level >= 2) {
    fprintf(f_debug, "file_name = %s\n", full_file_name);
  }  /* if */
#endif /* DEBUG */
  /* Check for recursion of #includes.  This is done by looking through the
     stack for the file name we just opened. */
  times_name_appears = 0;
  for (isnum = depth_input_stack; isnum >= 0; isnum--) {
    if (strcmp(input_stack[isnum].full_name, full_file_name) == 0) {
      /* The entry in the input stack has the same file name as the
         file we just opened.  This is okay once (it has to be), but
         if it happens several times, it probably means recursion. */
      times_name_appears++;
      if (times_name_appears >= 10 /* Arbitrary, must be > 1 */) {
        str_catastrophe(ec_include_recursion, full_file_name);
      }  /* if */
    }  /* if */
  }  /* for */
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
  curr_ise->is_include_file = is_include_file;
  curr_ise->nested_inclusion = (times_name_appears != 0);
  /* Create an intermediate file record describing this file.  It is
     useful later in converting sequence numbers into file name/line
     information. */
  if (depth_input_stack == 0) {
#if !INSTANTIATION_BY_IMPLICIT_INCLUSION
    parent_file = NULL;
#else /* if INSTANTIATION_BY_IMPLICIT_INCLUSION */
    /* Parent file will be set to NULL if this is the primary source file
       (which won't yet have been recorded in il_header), but if this is
       a file included as a result of the implicit inclusion feature of
       automatic instantiation, its parent should be the (already closed)
       primary source file. */
    parent_file = il_header.primary_source_file;
    /* after_end_of_all_source was set TRUE when we reached the end of the
       primary source file.  Reset it so that we can continue accepting
       input from the implicitly included template definition files. */
    after_end_of_all_source = FALSE;
#endif /* !INSTANTIATION_BY_IMPLICIT_INCLUSION */
  } else {
    parent_file = input_stack[depth_input_stack-1].assoc_il_file;
  }  /* if */
  record_start_of_source_file(parent_file,
                              (a_seq_number)seq_number_last_read+1,
                              (a_line_number)1, display_name,
                              full_file_name, name_as_written,
                              &(curr_ise->assoc_il_file), is_system_include);
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
      gen_pp_line_info(' ', 1);
    } else {
      gen_pp_line_info('1', 1);
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
    fprintf(f_pp_output, "%s\n", curr_ise->file_name);
  }  /* if */
  if (curr_ise->assoc_actual_il_file != il_header.primary_source_file) {
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


static void pop_input_stack(void)
/*
Pop the input stack, and correctly prepare for input from the file
at the next level down.
*/
{
  a_boolean	is_end_of_primary_source_file = FALSE;
  db_enter(2, "pop_input_stack");
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
    if (curr_ise->assoc_actual_il_file != il_header.primary_source_file) {
      record_end_of_source_file(il_header.primary_source_file,
                                seq_number_last_read);
    } else {
      is_end_of_primary_source_file = TRUE;
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
#if STACK_REFERENCED_INCLUDE_DIRECTORIES
      /* When the include list contains a stack of directory names of
         active include files, we need to remove the entries added for
         implicitly included files.  Don't do this for the primary source
         file.  We want that entry to stay on the list for use by
         subsequent implicit includes. */
      check_assertion(incl_search_path != NULL &&
                      incl_search_path->next != NULL);
      pop_primary_include_search_dir(incl_search_path->next->dir_name);
#endif /* STACK_REFERENCED_INCLUDE_DIRECTORIES */
    }  /* if */
  } else {
    an_input_stack_entry_ptr  prev_ise = curr_ise;
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
        catastrophe(ec_source_file_could_not_be_opened);
      }  /* if */
      if (fseek(curr_ise->file, curr_ise->position, SEEK_SET) != 0) {
        /* The seek could not be done.  Again, this implies some change
           in the file since last it was opened. */
        catastrophe(ec_source_file_could_not_be_opened);
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
      gen_pp_line_info('2', 1);
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
    if (list_makefile_dependencies && prev_ise->is_include_file &&
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
      a_source_file_ptr	sfp = prev_ise->assoc_actual_il_file;
      f_source = open_file_for_input(sfp->name_as_written,
                                     sfp->included_by_system_include ?
                                                         sys_incl_search_path :
                                                         incl_search_path,
 				     /*replace_suffix=*/TRUE,
				     &full_file_name, &display_name);
      if (f_source != NULL) {
        /* A related source file was found.  Make sure that the name of the
           file found is not the same as the file we started with.  This
           could occur if the user included a .c file that contains a
           template declaration. */
        if (strcmp(full_file_name, sfp->full_name) != 0) {
#if DEBUG
          if (debug_level >= 3) {
            fprintf(f_debug, "  Including text from '%s'\n", full_file_name);
          }  /* if */
#endif /* DEBUG */
          /* Push the new file onto the input stack and scan it.  There is
             no "name as written" so a NULL pointer is passed in. */
          push_input_stack(f_source, (char *)NULL, display_name,
                           full_file_name, /*is_include_file=*/FALSE,
                           (a_boolean)sfp->included_by_system_include);
        }  /* if */
      }  /* if */
    }  /* if */
#endif  /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
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
      } else if (olmp->kind == olm_line_splice) {
        /* If the case that a line splice is followed by the end of the
           logical source line, use the position of the "\" on the current
           line as the error position.  This is useful when the last line
           of a file ends with a backslash. */
        if (*adj_loc_in_line == '\n') break;
        /* Keep track of the current physical line. */
        start_of_curr_phys_line = olmp->line_loc;
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


static void error_at_line_pos(an_error_code error_code,
                              char          *loc_in_line)
/*
Record the occurrence of the indicated error at the indicated character
position of the current logical source line.
*/
{
  /* Convert the character position into an error position. */
  conv_line_loc_to_source_pos(loc_in_line, &error_position);
  error(error_code);
}  /* error_at_line_pos */


static void warning_at_line_pos(an_error_code error_code,
                                char          *loc_in_line)
/*
Record the occurrence of the indicated warning at the indicated character
position of the current logical source line.
*/
{
  /* Convert the character position into an error position. */
  conv_line_loc_to_source_pos(loc_in_line, &error_position);
  warning(error_code);
}  /* warning_at_line_pos */


/*
Macro to temporarily add newline/null to the current contents of the
source buffer.  Needed when an error is detected while building the
source line.  Without the newline/null, the partial line could not
be properly displayed with the error message.  Fortunately, the errors
that there are occur at the end of lines, so the "partial" line is really
the full line.
*/
#define finish_off_source_line_so_it_can_be_displayed_in_error()      \
{ *loc_in_line = '\n'; *(loc_in_line+1) = '\0'; }


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


a_boolean read_logical_source_line(a_boolean do_pop_on_end_of_file)
/*
Read the next logical source line into curr_source_line and
the related variables.  A "logical source line" is what results after
trigraph characters (see standard, 2.2.1.1) have been replaced, and
lines ending in newline-backslash have been spliced with the lines
immediately following (see standard, 2.1.1.2).  The logical source
line thus formed is returned in curr_source_line, terminated
with both a newline and a null.  The input is read from curr_input_stream.
Information that allows the mapping of characters in curr_source_line
back to the corresponding source sequence number and column is
maintained in orig_line_modif_list.

If end of file is not encountered, curr_char_loc is pointed at the first
character of the line just read.

If the end of the current input file is reached, then:

1)  If do_pop_on_end_of_file is TRUE, pop_input_stack is called (perhaps
more than once) to get to the next line of input, and that is returned
in the usual way.  If the end of the primary source file is reached
and that file is popped, then after_end_of_all_source is set to TRUE,
an empty line (just a null) is placed in curr_source_line, and
curr_char_loc is pointed at the null.

2)  If do_pop_on_end_of_file is FALSE, then the current line and the
current position within it are left unchanged, and at_end_of_source_file is
set to TRUE.

The return value from this function is at_end_of_source_file ||
after_end_of_all_source -- i.e., TRUE if no current source line was read.
*/
{
  register int    ch;
  register char   *loc_in_line;
  a_boolean       return_value;
  int		  curr_column;
  int             next_ch;
  a_boolean       char_is_trapped = FALSE;
  an_orig_line_modif_ptr
		  olmp;
  a_source_line_modif_ptr
		  slmp;
  sizeof_t        offset_in_line;
  char		  *after_curr_source_line_minus_2 =
                                             after_end_of_curr_source_line - 2;
			/* For checking of buffer overflow -- "-2" to leave
			   room for a newline and null. */

  /* This routine handles translation phases 1 (trigraphs, newlines) and
     2 (line splices) from the description of translation phases in
     2.1.1.2 of the standard. */
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
     end of source case, a line with just a null is placed in curr_source_line
     and the sequence number is incremented to an "after all source"
     position. */
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
    /* End of all source.  Go end the line with a null and return. */
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
      do {
        /* Check for question marks.  Presence of 2 in a row suggests there
           may be a trigraph in the line. */
        if (ch == '?') {
          /* One "?", check previous character to see if it is also a "?". */
          if (loc_in_line != curr_source_line &&
              *(loc_in_line-1) == '?') {
            /* On reasonable suspicion of a trigraph, exit to more expensive
               processing code.  Note that this is done before the second
               "? is stored, so that we do not ever store more characters
               than ultimately required, and therefore avoid spurious
               buffer overflows on trigraphs at the ends of very long lines. */
            goto possible_trigraph;
          }  /* if */
        }  /* if */
        /* Check that there is still room in the line buffer.  We have to
           leave room for both the final newline and null. */
        if (loc_in_line == after_curr_source_line_minus_2) {
          /* The line is too long; the buffer must be expanded.  Note that
             after the buffer is expanded we do not return to this loop for
             the current line; the rest of the line is processed in the
             more expensive loop. */
          goto expand_buffer;
        }  /* if */
        /* Put the character into curr_source_line. */
        *loc_in_line++ = ch;
        /* Get next character, check for end of file without newline. */
        if (ch = getc(curr_input_stream), is_eof_char(ch))
                                                       goto partial_final_line;
        /* Check for newline, which ends loop. */
      } while (ch != '\n');
#if READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS
      /* Ignore carriage return right before newline. */
      if (*(loc_in_line-1) == '\r') {
        loc_in_line--;
        /* Avoid the line splice test if the line is empty except for the
           carriage return. */
        if (loc_in_line == curr_source_line) {
          goto add_newline_and_null_and_return;
        }  /* if */
      }  /* if */
#endif /* READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS */
      /* End of a line containing at least one character.  Check to see
         if the last character is a backslash.  If so, the current line
         should be spliced with the line following. */
      if (*(loc_in_line-1) == '\\') goto line_splice;
    }  /* if */
  }  /* if */

add_newline_and_null_and_return:
  /* Store the final newline and null. */
  *loc_in_line++ = '\n';

return_with_line:
  /* Store the final null. */
  *loc_in_line = '\0';
  /* Set the input character position to the start of the line. */
  curr_char_loc = curr_source_line;
  any_tokens_gotten_from_curr_source_line = FALSE;

simple_return:
  /* The return value is TRUE on any end-of-file case. */
  return_value = at_end_of_source_file || after_end_of_all_source;
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
    init_do_not_put_curr_line_in_pp_output = do_not_put_curr_line_in_pp_output;
    if (return_value) do_not_put_curr_line_in_pp_output = TRUE;
  }  /* if */
  if (f_raw_listing != NULL) {
    /* If a raw listing file is being generated, save the line type:
       "N" indicating that this is a source line, "S" if this line
       is part of an #if-skip, or '\0' if there is no source line. */
    curr_raw_listing_line_code = return_value ? '\0' :
                                         (currently_in_pp_if_skip ? 'S' : 'N');
  }  /* if */
#if DEBUG
  if (debug_level >= 1) {
    /* Display the line just read. */
    fprintf(f_debug, "Returning from read_logical_source_line, ");
    fprintf(f_debug, "return value = %d, ", return_value);
    if (after_end_of_all_source) {
      fprintf(f_debug, "\nafter_end_of_all_source = TRUE.\n");
    } else {
      fprintf(f_debug, "seq = %lu\n%s", curr_seq_number, curr_source_line);
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
  /* The final line of a file does not end with a newline.  Issue a warning,
     add a newline and null to the line, and return. */
  eof_read_on_curr_input_stream = TRUE;
  finish_off_source_line_so_it_can_be_displayed_in_error();
  warning_at_line_pos(ec_last_line_incomplete, loc_in_line);
  goto add_newline_and_null_and_return;

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
          if (C_dialect != C_dialect_pcc) {
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
      }  /* if */
      /* Check that there is still room in the line buffer.  We have to
         leave room for both the final newline and null. */
      if (loc_in_line == after_curr_source_line_minus_2) {
entry_for_expand_buffer:
        /* The line is too long; the buffer must be expanded. */
        offset_in_line = loc_in_line - curr_source_line;
        expand_curr_source_line();
        loc_in_line = curr_source_line + offset_in_line;
        after_curr_source_line_minus_2 = after_end_of_curr_source_line - 2;
      }  /* if */
      /* Put the character into curr_source_line. */
      *loc_in_line++ = ch;
      /* Get next character, check for end of file without newline. */
      if (char_is_trapped) {
        ch = next_ch;
        char_is_trapped = FALSE;
      } else {
        ch = getc(curr_input_stream);
      }  /* if */
      if (is_eof_char(ch)) goto partial_final_line;
    } while (ch != '\n');
#if READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS
    /* Ignore carriage return right before newline. */
    if (*(loc_in_line-1) == '\r') {
      loc_in_line--;
      /* Avoid the line splice test if the line is empty except for the
         carriage return. */
      if (loc_in_line == curr_source_line) {
        goto add_newline_and_null_and_return;
      }  /* if */
    }  /* if */
#endif /* READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS */
    /* Check for backslash indicating line-splice.  Go add trailing newline
       and null, and then exit, if no backslash is present. */
    if (*(loc_in_line-1) == '\\') {
entry_for_line_splice:
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
      error_at_line_pos(ec_last_line_backslash, loc_in_line);
      /* Ignore the backslash, end the logical line at this point. */
    }  /* if */
  }  /* if */
  goto add_newline_and_null_and_return;
  
}  /* read_logical_source_line */


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
      error_position = comment_start_pos;
      error(ec_comment_unclosed_at_eof);
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
not be considered to be the start of a comment.  In pcc mode, the
token pasting rules are different, and the start of a comment is
assumed.
Also tests for "//" in C++ mode.
*/
#define start_of_comment()                                            \
  ((*(curr_char_loc+1) == '*' ||                                      \
    (C_dialect == C_dialect_cplusplus && *(curr_char_loc+1) == '/')) && \
   (within_curr_source_line(curr_char_loc) ||                         \
    (pcc_preprocessing_mode && !in_pcc_mode_half_comment)))


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
#if ASM_FUNCTION_ALLOWED
/* If asm functions are allowed, also delete comments if inside an asm
   function body. */
#define NEED_TO_DELETE_COMMENT                                        \
  ((((generate_pp_output && !do_not_put_curr_line_in_pp_output) ||    \
     f_raw_listing != NULL) &&                                        \
    !keep_comments_in_pp_output) ||                                   \
   in_asm_function_body)
#else /* !ASM_FUNCTION_ALLOWED */
#define NEED_TO_DELETE_COMMENT                                        \
  (((generate_pp_output && !do_not_put_curr_line_in_pp_output) ||     \
    f_raw_listing != NULL) &&                                         \
   !keep_comments_in_pp_output)
#endif /* ASM_FUNCTION_ALLOWED */
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
    case '\n':  /* Newline. */
      /* Newline is white space ordinarily, but a token to be returned if
         in a preprocessing directive. */
      if (in_preprocessing_directive) goto end_skip;
#if ASM_FUNCTION_ALLOWED
      /* Newline is also a token in asm functions. */
      if (in_asm_function_body) goto end_skip;
#endif /* ASM_FUNCTION_ALLOWED */
      /* The newline character is white space, and is being thrown away. */
      kind_skipped |= WHITE_SPACE_OTHER;
      curr_char_loc++;
      /* Fall through to process the null that must follow this newline. */
    case '\0':
      /* Null.  Usually, this indicates the end of a line.  However, it
         can also mean the end of source, or the end of the text of a macro
         expansion, so check for that. */
      /* Note that kind_skipped is not set for the null itself, since
         the null is not white space. */
      /* Check to see if the hanging deletion flag is set. */
      if ((delete_from = delete_source_from_loc) != NULL) {
        /* Source from the indicated position to the end of the line or
           macro is to be deleted.  This is typically because a macro
           invocation argument list is being scanned.  Keep the final null,
           but delete the newline. */
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
      if (within_curr_source_line(curr_char_loc)) {
        /* curr_char_loc falls within curr_source_line, so this is not
           the end of a macro expansion. */
        /* We have to read a new logical source line now. 
           read_logical_source_line will pop the input stack if end of
           file is encountered.  On the final end of file, TRUE is returned,
           and we want to exit this routine.  If we are already at the
           final end of file, don't try reading again, just exit. */
        if (after_end_of_all_source ||
            read_logical_source_line(/*do_pop_on_end_of_file=*/TRUE)) {
          /* End of file, end the white-space skip. */
          goto end_skip;
        } /* if */
        /* Not end of file, keep checking for white space in the new line. */
      } else {
        /* End of the expansion text for a macro.  Find the character
           location of the character following the macro invocation, and
           continue there. */
        slmp = assoc_source_line_modif(curr_char_loc);
        /* See if the current position is part of the text of a macro argument
           being macro-expanded; such text is expanded in isolation from
           the rest of the source file (see 3.8.3.1).  In that case, the
           null is significant, and is returned to the caller. */
        if (slmp->is_isolated_text) goto end_skip;
        /* Normal case; continue with the text following the macro
           invocation. */
        leave_insertion(slmp, curr_char_loc);
      }  /* if */
      /* If the hanging deletion flag was set, reset it to the new
         current position. */
      if (delete_from != NULL) {
        delete_source_from_loc = curr_char_loc;
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
    case END_OF_TOKEN_MARKER:
      /* Marker put into text by preprocessing of macros, to force the same
         interpretation of token boundaries as during the macro definition.
         At this level, should be ignored.  Note that kind_skipped is not
         set, since this is not white space. */
      curr_char_loc++;
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
          !within_curr_source_line(curr_char_loc)) {
        /* We're in pcc mode, and the token opening characters came from
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
        /* Advance to the end of line. */
        do {} while (*(++curr_char_loc) != '\n');
        if (NEED_TO_DELETE_COMMENT) {
          /* Delete the comment entirely. */
          add_deletion_source_line_modif(comment_start_loc,
                                   (sizeof_t)(curr_char_loc-comment_start_loc),
                                         /*for_comment=*/TRUE);
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
          if (ch == '\n' || ch == '\0') {
            /* End of the first line of the comment. */
            /* Determine the source position for the start of the comment
               now, before we lose the current source line. */
            determine_comment_pos_if_not_yet_done();
            /* We are supposed to delete the characters of the source line
               from delete_source_from_loc on, if it is non-NULL.  This would
               be, for example, because we are scanning a macro invocation. */
            delete_from = delete_source_from_loc;
            if (delete_from != NULL) {
              /* Reset the "from" position for subsequent lines. */
              delete_source_from_loc = curr_source_line;
              delete_only_for_comment = FALSE;
            } else if (NEED_TO_DELETE_COMMENT) {
              /* Delete the characters of the comment if writing preprocessor
                 output with the comments deleted.  The newline will be kept,
                 and therefore the comment is deleted entirely rather than
                 replaced by one blank -- since there is already white space
                 here in the form of the newline. */
              delete_from = comment_start_loc;
              delete_only_for_comment = TRUE;
            }  /* if */
            if (delete_from != NULL) {
              /* Delete the required characters.  Leave the newline and null.
                 If this comment is part of a preprocessing directive,
                 delete the newline as well so that multi-line directives
                 will become one-line directives. */
              if (in_preprocessing_directive && ch == '\n') {
                delete_to = curr_char_loc;
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
               (it is an error for a comment to be unclosed at the end of
               the file in which it was opened). */
            if (read_logical_source_line(/*do_pop_on_end_of_file=*/FALSE)) {
              /* End of file encountered, unclosed comment. */
              error_position = comment_start_pos;
              error(ec_comment_unclosed_at_eof);
              /* Consider the comment closed. */
              goto end_of_comment;
            }  /* if */
            /* Reset the start of comment location for subsequent lines. */
            comment_start_loc = curr_source_line;
          } else {
            /* Not a newline. */
            /* Check for possible nested comment, issue a warning.  This
               helps catch unclosed comments. */
            if (ch == '/' && *(curr_char_loc+1) == '*') {
              warning_at_line_pos(ec_nested_comment, curr_char_loc);
            }  /* if */
            /* Advance to the next character position. */
            curr_char_loc++;
          }  /* if */
        }  /* while */
        /* End of comment.  Take the "*" and "/", go back to throw away more
           white space. */
        curr_char_loc += 2;
        if (NEED_TO_DELETE_COMMENT && delete_source_from_loc == NULL) {
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
  register enum {k_decimal, k_octal, k_hex, k_float}
		kind;
  register a_token_kind 
		ctoken;
  a_boolean     err = FALSE;
  char		*err_pos;
  an_error_code	err_code;

  /* Collect the characters of the constant, and figure out where it
     ends.  In the process, figure out what kind of token it is.
     This is done according to the syntax for integer constants (3.1.3.2)
     and floating constants (3.1.3.1), rather than according to the
     pp-number syntax (3.1.8).  */
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
      do {} while (isxdigit((unsigned char)*(++curr_char_loc)));
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
    if (strict_ansi_mode && !fetch_pp_tokens && l_seen == 2) {
      if (strict_ansi_error_severity == es_error) {
        error_at_line_pos(ec_nonstd_long_long, start_of_curr_token);
      } else {
        warning_at_line_pos(ec_nonstd_long_long, start_of_curr_token);
      }  /* if */
    }  /* if */
#endif /* LONG_LONG_ALLOWED */
  }
  goto constant_accumulated;

float_accum_1:
  /* At the decimal point in a floating constant.  Take whatever decimal
     digits follow it. */
  do {} while (isdigit((unsigned char)*(++curr_char_loc)));
  if ((ch = *curr_char_loc) != 'e' && ch != 'E') goto end_float_accum;
float_accum_2:
  /* At the "e" or "E" indicating the start of the exponent of a floating
     constant.  Take an optional sign, then decimal digits of the exponent. */
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
            pp-number .

     digit is any decimal digit (0-9).
     nondigit is an alphabetic or "_".
  */
  /* Do this only in strict ANSI mode, or in default mode when scanning
     preprocessing tokens, but never in pcc mode. */
  if ((strict_ansi_mode || fetch_pp_tokens) && C_dialect != C_dialect_pcc) {
    while (is_id_char[(ch = *curr_char_loc)-CHAR_MIN] || ch == '.' ||
           ((ch == '+' || ch == '-') &&
            ((ch = *(curr_char_loc-1)) == 'e' || ch == 'E'))) {
      /* 0-9, a-z, A-Z, "_", ".", or sign preceded by "e" or "E".
         Keep accumulating. */
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
    while (isalnum((unsigned char)(ch = *curr_char_loc))) {
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
        conv_float_literal(&err_code, &err_pos);
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
  return (ctoken);
}  /* scan_number */


static a_token_kind accum_quoted_string(a_token_kind  ctoken,
                                        unsigned long *num_chars,
                                        a_boolean     *err)
/*
Scan a quoted construct, of kind indicated by ctoken.  The initial quote
is at curr_char_loc.  Scan to the matching closing quote, and do not be
confused by escaped characters.  Return in *num_chars the actual number
of characters (after escape processing) contained within the quotes.
Return *err TRUE if there was an error.  The value of the function is
ctoken usually, but tok_error if there was an error and fetch_pp_tokens
is TRUE.  This routine is used for character constants and string literals,
in both the "wide" and normal forms, and for header names in #include
directives.
*/
{
  register char quoting_char, ch;
  a_boolean     may_have_zero_characters = (ctoken == tok_string_literal);
  a_boolean     is_header_name = (ctoken == tok_header_name);

  *err = FALSE;
  *num_chars = 0;
  quoting_char = *curr_char_loc;
  /* For <...> header names, the closing quoting character is different
     than the opening one. */
  if (quoting_char == '<') quoting_char = '>';
  while ((ch = *(++curr_char_loc)) != quoting_char) {
    (*num_chars)++;
    if (ch == '\\') {
      /* Backslash, escapes the next character.  If followed by "0" or
         "x", an octal or hexadecimal value must be scanned.  We recognize
         those digits so we can accurately count characters, but we do
         not convert them at this point. */
      ch = *(++curr_char_loc);
      if (isdigit((unsigned char)ch) && ch != '8' && ch != '9') {
        /* Octal escape, one to three digits.  Note that neither ANSI nor
           pcc allows 8 and 9 as octal digits in this case.  Note that
           there is code in conv_single_char that must match this code.*/
        ch = *(curr_char_loc+1);
        if (isdigit((unsigned char)ch) && ch != '8' && ch != '9') {
          curr_char_loc++;
          ch = *(curr_char_loc+1);
          if (isdigit((unsigned char)ch) && ch != '8' && ch != '9') {
            curr_char_loc++;
          }  /* if */
        }  /* if */
      } else if (ch == 'x') {
        /* Hex escape, any number of digits. */
        while (isxdigit((unsigned char)*(curr_char_loc+1))) curr_char_loc++;
      }  /* if */
    } else if (ch == '\n' ||
               (ch == END_OF_TOKEN_MARKER && !is_header_name) ||
               ch == '\0') {
      /* Newline -- error, quoted string unclosed. */
      /* Similar error for other strange cases of incomplete strings, which
         can come up with preprocessing.  Note that end-of-token markers do
         not terminate header names, because header names can result from
         several adjacent preprocessing tokens when macro expansion is
         involved (see 3.8.2).  The end-of-token markers are removed when the
         file name is constructed later (see proc_include). */
      /* Message is generic -- "Missing closing quote". */
      err_code_for_error_token = ec_unclosed_string;
      if (fetch_pp_tokens) {
        ctoken = tok_error;
      } else {
        error_at_line_pos(err_code_for_error_token, start_of_curr_token);
      }  /* if */
      *err = TRUE;
      goto return_point;
    }  /* if */
  }  /* while */
  /* Skip the closing quote. */
  curr_char_loc++;
  if (*num_chars == 0 && !may_have_zero_characters) {
    /* Error -- The string may not have zero characters. */
    err_code_for_error_token = ec_zero_length_string;
    if (fetch_pp_tokens) {
      ctoken = tok_error;
    } else {
      error_at_line_pos(err_code_for_error_token, start_of_curr_token);
    }  /* if */
    *err = TRUE;
  }  /* if */
return_point:
  end_of_curr_token = curr_char_loc - 1;
  return ctoken;
}  /* accum_quoted_string */


static a_token_kind scan_char_constant(void)
/*
Scan a character constant token, return the token kind or tok_error.
*/
{
  a_token_kind  ctoken;
  a_boolean     err;
  unsigned long num_chars;
  an_error_code err_code;
  char          *err_pos;

  ctoken = accum_quoted_string(tok_char_constant, &num_chars, &err);
  if (!fetch_pp_tokens) {
    /* Convert the constant to internal form. */
    if (err) {
      set_error_constant(&const_for_curr_token);
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


static a_token_kind scan_string_literal(void)
/*
Scan a string literal token, return the token kind or tok_error.
*/
{
  a_token_kind  ctoken;
  a_boolean     err;
  unsigned long num_chars;
  an_error_code err_code;
  char          *err_pos;

  ctoken = accum_quoted_string(tok_string_literal, &num_chars, &err);
  if (!fetch_pp_tokens) {
    /* Convert the constant to internal form. */
    if (err) {
      set_error_constant(&const_for_curr_token);
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


static a_token_kind scan_wide_char_constant(void)
/*
Scan a wide character constant token, return the token kind or tok_error.
*/
{
  /* Skip over the "L". */
  curr_char_loc++;
  /* Otherwise, handle the same as the non-wide case. */
  return(scan_char_constant());
}  /* scan_wide_char_constant */


static a_token_kind scan_wide_string_literal(void)
/*
Scan a wide string literal token, return the token kind or tok_error.
*/
{
  /* Skip over the "L". */
  curr_char_loc++;
  /* Otherwise, handle the same as the non-wide case. */
  return(scan_string_literal());
}  /* scan_wide_string_literal */


static void adjust_pp_int_constant(void)
/*
The current token is an integer constant scanned within a preprocessing
#if expression.  It should be made long if it is not already so.
See standard, 3.8.1.
*/
{
  an_integer_kind ik;

  ik = const_for_curr_token.type->variant.integer.int_kind;
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
}  /* adjust_pp_int_constant */


a_token_kind scan_literal_constant(a_token_kind kind)
/*
Scan a literal constant token, according to the value of kind:
Integer or floating point for tok_pp_number, character for 
tok_char_constant, and string literal for tok_string_literal.
Return the type of token scanned, or tok_error if there was an error.
This is used in scanning the expansion of a macro that expands simply
to a constant, in order to be able to save its value.
Note that adjacent string literals are not supposed to be concatenated,
and integer constants are not adjusted in length even if we are
within a preprocessing #if expression, since we want the generic
reusable value of the constant.
*/
{
  a_token_kind ctoken;

  switch (kind) {
    case tok_pp_number:
      ctoken = scan_number();
      break;
    case tok_char_constant:
      if (*curr_char_loc == 'L') {
        ctoken = scan_wide_char_constant();
      } else {
        ctoken = scan_char_constant();
      }  /* if */
      break;
    case tok_string_literal:
      if (*curr_char_loc == 'L') {
        ctoken = scan_wide_string_literal();
      } else {
        ctoken = scan_string_literal();
      }  /* if */
      break;
#if CHECKING
    default:
      internal_error("scan_literal_constant: bad kind");
#endif /* CHECKING */
  }  /* switch */
  return(ctoken);
}  /* scan_literal_constant */


static void concat_adjacent_string_literals(void)
/*
The current token (not in curr_token yet) is a string literal
(tok_string_literal), and in the current lexical mode normal (not pp)
tokens should be fetched, and concatenation of adjacent string literals
should be done.  Look to see if the next token of input is a string literal,
and if so, concatenate it with the current token.  Loop to pick up all
the adjacent string literals.  The C standard says that a wide string literal
next to a normal string literal is undefined; we choose not to concatenate
them unless wchar_t is char.
*/
{
  an_integer_kind    centity_int_kind;
  a_token_cache      cache;
  a_cached_token_ptr ctp, ctp_next, first_string_token = NULL;
  a_boolean          more_than_one_string = FALSE;

  db_enter(5, "concat_adjacent_string_literals");
  check_assertion_str(!fetch_pp_tokens && do_string_literal_concatenation,
                      "concat_adjacent_string_literals: bad mode");
  /* Get the string element integer kind from the string. */
  centity_int_kind = plain_char_int_kind;
  /* Watch out for the case where the constant is an error constant. */
  if (!is_error_constant(&const_for_curr_token)) {
    centity_int_kind =
                     char_int_kind_from_string_type(const_for_curr_token.type);
  }  /* if */
  /* Start a token cache in which we will accumulate all the adjacent
     string literals.  Usually, this will be just a single string literal. */
  clear_token_cache(&cache, /*reusable=*/FALSE);
  /* Set up the first string literal as the current token, so it can be
     cached.  This routine is called from get_token at a point where
     there is, in effect, no current token, so we're just anticipating the
     action that would be done on return from get_token here.
     const_for_curr_token is already set; start_of_curr_token is already
     set (if it needs to be); len_of_curr_token does not need to be set. */
  curr_token = tok_string_literal;
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
       vice-versa (actually, if the underlying character representations
       are different). */
    if (!is_error_constant(&const_for_curr_token) &&
        centity_int_kind != 
              char_int_kind_from_string_type(const_for_curr_token.type)) break;
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
    concat_string_literals(&cache, centity_int_kind);
    /* The constants have been concatenated into the first constant in the
       token cache (which might not be the first entry in the cache, if there
       are pragma entries first).  Discard the token cache entries for the
       string literals tokens after that first one. */
    last_token = first_string_token;
    for (ctp = first_string_token->next; ctp != NULL; ctp = ctp_next) {
      ctp_next = ctp->next;
      if (ctp->extra_info_kind == (a_token_extra_info_kind)teik_pragma) {
        /* Keep a pragma entry (this is a pragma entry that appeared between
           string literals). */
        last_token->next = ctp;
        last_token = ctp;
      } else {
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
scanned.  The caller sees only the tokens after expansion.

When fetch_pp_tokens is FALSE, do_string_literal_concatenation controls
whether adjacent string literals are concatenated.

If in_preprocessing_directive is TRUE, the definition of white space is
changed to that for within preprocessing directives, and newline is
returned as a token.  If in_preprocessing_directive is TRUE and
processing_C_code_in_pragma is FALSE, keywords are not recognized, and
"#", and "##" are recognized and returned as tokens.

If in_pp_if_expression is TRUE (indicating that we are inside a
preprocessing #if expression), integer constants will get an implicit
"L" suffix, and undefined identifiers will be returned as the integer
constant 0L.

If exp_header_name is TRUE, then <...> and "..." will be scanned
as a header name for a #include (tok_header_name).  Other tokens will
be processed normally.

If exp_digit_sequence is TRUE, then a string of decimal digits will be
scanned as a tok_digit_sequence (used in the #line directive).  Other tokens
will be processed normally.

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
#if ASM_FUNCTION_ALLOWED
/*
If in_asm_function_body is TRUE, return tok_newline for ends of lines.
*/
#endif /* ASM_FUNCTION_ALLOWED */
{
  register a_token_kind ctoken;
  register char         ch;
  register a_symbol_ptr	assoc_symbol;
  a_boolean             err;
  unsigned long         num_chars;
  a_symbol_kind		id_kind;
  a_boolean		rescan;
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
    case '\0':
      /* Null.  Usually, this indicates the end of a line (probably
         after some error at end of a file).  However, it can also
         mean the end of source, or the end of the text of a macro
         expansion.  Let the white-space routine figure it out. */
      skip_white_space();
      /* If we are not at end of file, go scan the next token. */
      if (*curr_char_loc != '\0') goto start_of_token_scan;
      /* This is the ultimate end of file, or the end of a macro argument
         string being scanned in isolation from the rest of the source.
         Return end of file. */
      ctoken = tok_end_of_source;
      start_of_curr_token = curr_char_loc;
      /* Remember the character position of the end of the token. */
#if 0
      /* At the end of file, this is the position preceding the beginning
         of the input buffer, which is nonstandard (though probably harmless)
         unless that position is really allocated space. */
#endif /* 0 */
      end_of_curr_token = curr_char_loc - 1;
      /* Determine the source position of the end of source token. */
      remember_token_start();
      /* If this is the null character at the end of the primary source
         line, back up the column by 1 so it points at the end of the line. */
      if (within_curr_source_line(start_of_curr_token)) {
        pos_curr_token.column--;
        error_position.column = pos_curr_token.column;
      }  /* if */
      /* Go exit with the end-of-source token. */
      goto end_of_token_scan_b;
    case '\n':
      /* Newline.  Is white space ordinarily, but a token within
         preprocessing directives. */
      if (in_preprocessing_directive
#if ASM_FUNCTION_ALLOWED
          /* ... or if inside an asm function body. */
          || in_asm_function_body
#endif /* ASM_FUNCTION_ALLOWED */
                                    ) {
        ctoken = tok_newline;
      } else {
        skip_white_space();
        goto start_of_token_scan;
      }  /* if */
      break;
    case '\f':
    case VERTICAL_TAB_CHARACTER:
      /* Form feed, vertical tab.  Usually white space, but implementation-
         defined when inside a preprocessing directive.  Let the white space
         routine decide. */
      skip_white_space();
      goto start_of_token_scan;
    case END_OF_TOKEN_MARKER:
      /* Marker put into text by preprocessing of macros, to force the same
         interpretation of token boundaries as during the macro definition.
         At this level, should be ignored. */
      curr_char_loc++;
      goto rescan_token;
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
      /* In C++, "::" is a possibility. */
      if (C_dialect == C_dialect_cplusplus &&
          *(curr_char_loc+1) == ':') {
        ctoken = tok_colon_colon;
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
      /* One of "%=" or "%". */
      if (*(curr_char_loc+1) == '=') {
        ctoken = tok_remainder_assign;
        goto two_char_token;
      }  /* if */
      /* Just plain "%". */
      ctoken = tok_remainder;
      break;
    case '<':
      /* One of "<<", "<<=", "<=", or "<".  If exp_header_name is
         TRUE, a header name of the form <filename>. */
      if (exp_header_name) {
        ctoken = accum_quoted_string(tok_header_name, &num_chars, &err);
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
    case 'L':
      /* Probably an identifier, but check for a wide character
         constant (L'x') or wide string literal (L"xyz") first. */
      if ((ch = *(curr_char_loc+1)) == '\'') {
        ctoken = scan_wide_char_constant();
        goto end_of_token_scan;
      } else if (ch == '"') {
        remember_token_start();
        ctoken = scan_wide_string_literal();
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
      if (allow_dollar_in_id_chars && strict_ansi_mode &&
          !dollar_in_id_diagnostic_issued) {
        /* Use a special scanning loop when we must check for dollar signs
           (which are nonstandard) while accumulating the characters of the
           identifier.  The diagnostic is issued only once. */
        register a_boolean dollar_used = (ch == '$');
        while (is_id_char[(ch = *(++curr_char_loc))-CHAR_MIN]) {
          if (ch == '$') dollar_used = TRUE;
        }  /* while */
        if (dollar_used) {
          diagnostic(strict_ansi_error_severity, ec_dollar_used_in_identifier);
          dollar_in_id_diagnostic_issued = TRUE;
        }  /* if */
      } else {
        /* Dollar signs are not allowed, so use the normal (faster) loop. */
        /* Accumulate characters of the identifier after the first. */
        while (is_id_char[(ch = *(++curr_char_loc))-CHAR_MIN]) {}
      }  /* if */
      end_of_curr_token = curr_char_loc - 1;
      /* Clear the symbol locator for the current identifier.  This is done 
         even if the identifier is not looked up in the symbol table. */
      clear_locator(&locator_for_curr_id, &pos_curr_token);
      if ((fetch_pp_tokens || in_preprocessing_directive) && !expand_macros) {
        /* Raw preprocessing tokens wanted, so do not look up the
           identifier. */
      } else {
        /* Look up the identifier in the symbol table. */
        assoc_symbol = find_symbol(start_of_curr_token,
                                   (sizeof_t)((end_of_curr_token -
                                                     start_of_curr_token + 1)),
                              &locator_for_curr_id);
        /* See if the identifier is a macro or keyword.  "Macro" should
           take precedence over "keyword", but it will naturally, since
           keywords are entered first and therefore appear at the end of the
           list. */
        while (assoc_symbol != NULL) {
          if ((id_kind = assoc_symbol->kind) == (a_symbol_kind)sk_macro) {
            /* Macro to be expanded. */
            if (expand_macros) {
              ctoken = macro_invocation(assoc_symbol, &rescan);
              /* In the usual case, we rescan the expanded form of the
                 macro. */
              if (rescan) goto rescan_token;
              /* Otherwise, expand_token has returned a token we can use
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
               the keywords mean nothing.  An exception is when we are
	       processing a pragma that contains C code. */
            if (!fetch_pp_tokens &&
	        (!in_preprocessing_directive || processing_C_code_in_pragma)) {
              ctoken = assoc_symbol->variant.keyword.token;
              /* Check for a keyword that is not yet implemented.  If one is
                 found, issue a diagnostic and treat the keyword as an
		 identifier. */
              if (ctoken == tok_unimplemented) {
                unimplemented_keyword_diagnostic(assoc_symbol);
                ctoken = tok_identifier;
              } else {
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
        ctoken = accum_quoted_string(tok_header_name, &num_chars, &err);
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
      /* As the first token on a line, "#" opens a preprocessing directive.
         "#" and "##" are also allowed within the body of a #define
         (for stringizing and pasting).  */
#if ATT_PREPROCESSING_EXTENSIONS_ALLOWED
      /* An AT&T System V release 4 extension uses #name(tokens) in a
         preprocessing #if to test an #assert predicate name. */
      if (in_pp_if_expression) {
        /* Scan the #name(tokens) and create a 1 (TRUE) or 0 (FALSE) constant
           value accordingly. */
        curr_char_loc++;
        ctoken = make_pp_int_constant(scan_assert_predicate_reference() ?
                                                                      1L : 0L);
        goto end_of_token_scan;
      }  /* if */
#endif /* ATT_PREPROCESSING_EXTENSIONS_ALLOWED */
      if (!pcc_preprocessing_mode && in_preprocessing_directive &&
	  !processing_C_code_in_pragma) {
        /* We recognize and return these preprocessing tokens even if
           we do not know that we are in the body of a #define; this
           helps produce reasonable error messages. */
        if (*(curr_char_loc+1) == '#') {
          ctoken = tok_paste;
          goto two_char_token;
        } else {
          ctoken = tok_sharp;
        }  /* if */
      } else if (!any_tokens_gotten_from_curr_source_line) {
        /* A sharp that is the first thing on a line -- This is a
           preprocessing directive. */
        if (!currently_in_pp_if_skip) {
          remember_token_start();  /* For the "#" pseudo-token. */
          curr_char_loc++;  /* Skip over the "#". */
#if CHECKING
	  {
	  a_cached_token_ptr	curr_cached_token = cached_token_rescan_list;
#endif /* CHECKING */
          pp_directive();
#if CHECKING
	  if (curr_cached_token != cached_token_rescan_list) {
	    internal_error("get_token: token cache affected by preprocessing directive");
	  }  /* if */
          }
#endif /* CHECKING */
          /* After the directive has been processed, go skip white space and
             scan another token. */
          skip_white_space();
          goto start_of_token_scan;
        }  /* if */
        /* Skipping because of an #if or the like, just return this
           as a token for further checking. */
        ctoken = tok_sharp;
      } else {
        /* "#" outside of a preprocessing directive, and not at the start of a
           line; don't know what it means. */
        err_code_for_error_token = ec_bad_use_of_sharp;
        if (!fetch_pp_tokens) {
          error_at_line_pos(err_code_for_error_token, start_of_curr_token);
        }  /* if */
        ctoken = tok_error;
      }  /* if */
      break;
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
end_of_token_scan_b:
  /* The "_b" entry here is for cases where something beyond the end
     of the token has been scanned, and therefore we may be on a new
     line now.  Cases where this is true (perhaps because skip_white_space
     has been called) should call remember_token_start before scanning
     the initial token, and then should branch here after all other
     processing is done. */
  if (start_of_curr_token != NULL) {
    len_of_curr_token = end_of_curr_token - start_of_curr_token + 1;
  }  /* if */
return_from_token_scan:
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
      (in_preprocessing_directive && !processing_C_code_in_pragma) ||
      !do_string_literal_concatenation) {
    /* String literal concatenation should not be done in the current mode. */
    goto end_of_token_scan_b;
  }  /* if */
  /* Do string literal concatenation. */
  check_assertion_str(ctoken == tok_string_literal,
                      "get_token: concatenating string literal, bad token");
  concat_adjacent_string_literals();
  start_of_curr_token = NULL;
  goto return_from_token_scan;
}  /* get_token */


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
    /* Always stop the flush on:
       1)  End of source;
       2)  A newline, if in a preprocessing directive. */
    if (curr_token == tok_end_of_source ||
        (in_preprocessing_directive && curr_token == tok_newline)) break;
    /* If we've skipped too many lines, give up the flush. */
    if ((pos_curr_token.seq - start_pos.seq) > max_lines) break;
    /* Check for the start of a template parameter list. */
    if (curr_token == tok_lt && prev_token == tok_identifier) {
      if (is_template_reference()) {
        flush_until_matching_token();
      }  /* if */
    }  /* if */
    /* None of the conditions was satisfied, so keep flushing tokens. */
    prev_token = curr_token;
    (void)get_token();
  }  /* while */

  db_exit();
}  /* flush_until_matching_token */


void flush_tokens(void)
/*
Get and throw away tokens until a token is read that is in the set
of stop tokens.  This routine is called to recover from syntax
errors.
*/
{
  a_source_position start_pos;
  a_token_kind      prev_token = tok_error;

  db_enter(3, "flush_tokens");
  /* Save the current position, to see later how much we have flushed. */
  copy_source_position(pos_curr_token, start_pos);

  /* Flush tokens until something in the stop tokens set turns up.
     Stop flushing if the end of file is reached.
     While flushing, note parentheses, etc., and flush to matching tokens. */
  /* Stop the flush on finding a token in the stop token set. */
  while (stop_token_array[(int)curr_token] == 0) {
    /* On paired tokens, skip to the corresponding closing token. */
    if (curr_token == tok_lparen || curr_token == tok_lbracket ||
        curr_token == tok_lbrace ||
        (curr_token == tok_lt && prev_token == tok_identifier && 
         is_template_reference())) {
      flush_until_matching_token();
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
  set_err_pos_to_curr_token();
  /* If the flushing threw away more than just a little bit, put out
     a diagnostic to tell the user where the parsing recovered. */
  if (pos_curr_token.seq - start_pos.seq > 2) {
    warning(ec_end_of_flush);
  }  /* if */
  db_exit();
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


a_token_kind next_token(void)
/*
Return the next token after the current one while leaving the current
token unchanged.  This is used in recursive-descent parsing routines
to peek ahead at the next token and decide on a path through the syntax.
This routine cannot be used when fetching raw preprocessing tokens.
*/
{
  a_token_cache 	cache;
  a_token_kind 		ntoken;
  a_cached_token_ptr	ctp = NULL;

  db_enter(5, "next_token");
  if (in_preprocessing_directive && curr_token == tok_newline) {
    /* If we have reached the end of a preprocessing directive, don't attempt
       to scan tokens past the end.  Return a tok_newline without actually
       looking at the next token. */
    ntoken = tok_newline;
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
  } else {
    /* Put the current token into a token cache so it can be rescanned. */
    clear_token_cache(&cache, /*reusable=*/FALSE);
    cache_curr_token(&cache);
    /* Fetch the next token and remember its kind. */
    ntoken = get_token();
    /* Put the two tokens in the cache (original, next) on the rescan list,
       and refetch the original token.  Note that the "next" token remains on
       the rescan list. */
    rescan_cached_tokens(&cache);
  }  /* if */
done:
  db_exit();
  return ntoken;
}  /* next_token */


static a_token_kind next_two_tokens(a_token_kind	first_token_must_be,
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


a_boolean f_get_destructor_name(void)
/*
The current token is the "~" at the start of a destructor name.  Scan the
name and build a locator for the destructor name in locator_for_curr_id.
Note that this routine does not check that the name is a class name or that
the destructor exists.  Return TRUE always (this routine is called from the
macro get_destructor_name; it handles the FALSE case).  This routine is called
only in C++ mode.
*/
{
  /* Skip past the "~", check for an identifier. */
  if (get_token() != tok_identifier) {
    /* syntax_error is deliberately not called. */
    error(ec_exp_identifier);
    /* Put back the current token and make a fake error identifier. */
    unget_token();
    curr_token = tok_identifier;
    make_specific_symbol_error_locator(&locator_for_curr_id);
  } else {
    /* "~identifier" is present. */
    if (depth_innermost_instantiation_scope != NO_SCOPE_DEPTH) {
      a_symbol_ptr sym;
      /* Check for situations like p->~T, where T is a template parameter.
         This is unlike other template parameter references because the T
         must be replaced by the actual argument name before the lookup
         is done.  Lookup the identifier and see if it is a template parameter.
         If the type kind is tk_template_param then we are in a prototype
         instantiation and no substitution is attempted.  Note that this
         routine is not called for nonclass vacuous destructors. */
      sym = normal_id_lookup(&locator_for_curr_id, IDL_NO_OPTIONS);
      if (sym != NULL && sym->is_template_param &&
          sym->kind == (a_symbol_kind)sk_type &&
          is_class_symbol(sym)) {
        a_symbol_ptr		type_sym;
        a_source_position	saved_position;
        saved_position = locator_for_curr_id.source_position;
        /* Get the symbol pointer associated with the type pointed to. */
        type_sym = (a_symbol_ptr)sym->variant.type->source_corresp.assoc_info;
        make_locator_for_symbol(type_sym, &locator_for_curr_id);
        locator_for_curr_id.source_position = saved_position;
      } else {
        /* We don't need to do anything here.  The locator will be reset
           by tildize_locator. */
      }  /* if */
    }  /* if */
    /* Convert the locator to a locator for the destructor. */
    tildize_locator(&locator_for_curr_id);
  }  /* if */
  return TRUE;
}  /* f_get_destructor_name */


a_boolean f_get_opname(a_type_ptr class_type)
/*
The current token is the token "operator" at the start of an operator name,
like "operator+".  Scan the name and build a locator for the operator name
in locator_for_curr_id.  Return TRUE always (this routine is called from the
macro get_opname; it handles the FALSE case).  class_type is a pointer
to the class type of a the qualified name associated with the generalized
identifier being scanned.  If class_type is not NULL then push a class
reactivation scope before scanning type name in a type conversion operator.

This routine is called only in C++ mode.
*/
{
  a_source_position start_position;
  a_token_kind      token;
  an_opname_kind    opname;

  start_position = pos_curr_token;
  /* Skip past the "operator", check for an operator. */
  token = get_token();
  if (scan_conversion_operator(&start_position, class_type)) {
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
  /* Always return TRUE, as a convenience to macro get_opname. */
  return TRUE;
}  /* f_get_opname */


a_boolean is_global_new_or_delete(void)
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


static void flush_to_end_of_arg_list(void)
/*
*/
{
  unsigned char save_comma_stop_token_count;
  /* Remove comma from the stop tokens set so that we can flush to the
     end of the argument list. */
  save_comma_stop_token_count = stop_token_array[(int)tok_comma];
  stop_token_array[(int)tok_comma] = 0;
  flush_tokens();
  /* Restore comma as a stop token (if it was one). */
  stop_token_array[(int)tok_comma] = save_comma_stop_token_count;
}  /* flush_to_end_of_arg_list */


static a_type_ptr rescan_template_constant_parameter
                                          (a_symbol_ptr		template_sym,
					   a_template_param_ptr param_ptr,
					   a_template_arg_ptr   arg_list)
/*
Rescan the tokens of a template parameter declaration using the current
values of any previous parameters so that the declaration is processed
with the types with which the class is to be instantiated.  This is
used to get the correct types for template parameters whose types depend
on other template parameters.
*/
{
  a_template_symbol_supplement_ptr	tssp;
  a_type_ptr				param_type_ptr;
  a_symbol_locator   			param_locator;
  a_source_position  			saved_pos_curr_token;
  a_source_position  			saved_error_position;

  tssp = template_sym->variant.template_info;
  /* Push the template instantiation scope.  Note that the instance symbol
     passed to push_scope is NULL because we don't yet know which instance
     is being instantiated.  Also note that a class type is not being
     passed for the same reason. */
  (void)push_scope((a_scope_kind)sck_template_instantiation,
                   tssp->declaration_scope, (a_type_ptr)NULL,
                   (a_routine_ptr)NULL, (a_symbol_ptr)NULL, template_sym,
                   arg_list);
  /* Rescan the tokens of the function declaration. */
  saved_pos_curr_token = pos_curr_token;
  saved_error_position = error_position;
  rescan_reusable_cache(&param_ptr->token_cache);
  /* Scan the declaration specifiers. */
  scan_a_template_parameter_declaration(&param_locator, &param_type_ptr);
  error_position = saved_error_position;
  pos_curr_token = saved_pos_curr_token;
  /* Skip past any tokens remaining in the cache.  Extra tokens will
     be present under certain error conditions and when a default argument
     has been supplied. */
  flush_past_token_cache_terminator();
  /* Pop the template instantiation scope. */
  pop_scope();
  return param_type_ptr;
}  /* rescan_template_constant_parameter */


a_symbol_ptr coalesce_template_class_reference
			(a_symbol_ptr		   template_sym,
			 an_identifier_options_set options,
			 a_boolean		   *err)
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
  a_source_position               locator_pos;
  a_template_param_ptr            param_ptr;
  a_template_arg_ptr              arg_list = NULL;
  a_template_arg_ptr              last_arg = NULL;
  a_symbol_ptr                    new_sym = NULL;
  a_symbol_ptr			  current_instantiation_sym;
  a_boolean                       any_errors = FALSE;
  a_memory_region_number          region_to_switch_back_to;
  a_symbol_ptr                    sym;
  a_boolean                       is_type_param;
  a_type_ptr                      argument_type;
  a_constant_ptr                  constant;
  a_template_arg_ptr              arg_ptr;
  a_token_kind			  next_tok;
  a_boolean			  class_is_being_instantiated;
  a_boolean			  arg_list_coalesced = FALSE;

  db_enter(3, "coalesce_template_class_reference");

  *err = FALSE;
  next_tok = next_token();
  /* Save source position for error reporting. */
  start_position = pos_curr_token;
  /* Save the current locator. */
  locator_pos = locator_for_curr_id.source_position;
  if (template_sym->kind != (a_symbol_kind)sk_class_template) {
    /* The symbol is not a class template symbol.  If the symbol
       is a type symbol followed by what looks like the beginning
       of a template argument list (i.e., a "<") issue an error
       indicating that the current symbol is not a class template.
       If the symbol is not a type symbol simply return without
       doing anything because the "<" may be a less than sign.  This
       test is also suppressed when processing the type name in a new
       expression and the operand of a field selection operation
       because they may legitimately be followed by a less than sign. */
    if (is_type_symbol(template_sym) && next_tok == tok_lt &&
        !(options & GID_IS_NEW_TYPE_NAME) &&
        !(options & GID_IS_FIELD_SELECTION_OPERAND)) {
      pos_sy_error(ec_unexpected_template_arg_list, &start_position,
                   template_sym);
      add_stop_token(tok_gt);
      flush_to_end_of_arg_list();
      remove_stop_token(tok_gt);
      make_specific_symbol_error_locator(&locator_for_curr_id);
      new_sym = template_sym;
      any_errors = TRUE;
      goto normal_exit;
    } else {
      /* Just return the symbol that was passed in. */
      new_sym = template_sym;
      goto skip_processing;
    }  /* if */
  }  /* if */
  /* Determine whether the class template (or a member of the class template)
     is currently being instantiated.  This affects how references to the
     class template name are handled. */
  current_instantiation_sym = template_sym;
  class_is_being_instantiated =
            current_class_symbol_if_class_template(&current_instantiation_sym);
  if (next_tok != tok_lt) {
     /* There is no template argument list.  If we are in an instantiation of
        this class template, use the symbol associated with the innermost
        instantiation of this class, otherwise just return the class
        template symbol. */
    new_sym = current_instantiation_sym;
    if (class_is_being_instantiated) {
      /* We have the symbol for the current instantiation of the
         class template. */
      goto normal_exit;
    } else {
      if (options & GID_TEMPLATE_ARGS_OPTIONAL) {
         /* Template arguments are not required -- simply return the
            symbol of the class template. */
         goto skip_processing;
      } else {
        /* Issue an error and return an error locator. */
        pos_sy_error(ec_missing_template_arg_list, &start_position,
                     template_sym);
        make_specific_symbol_error_locator(&locator_for_curr_id);
        new_sym = locator_for_curr_id.specific_symbol;
        any_errors = TRUE;
        goto normal_exit;
      }  /* if */
    }  /* if */
  }  /* if */
  /* Always allocate template arguments at the file scope. */
  switch_to_file_scope_region(&region_to_switch_back_to);
  add_stop_token(tok_gt);
  /* Get the angle bracket token. */
  (void)get_token();
  /* Get token following opening angle bracket. */
  (void)get_token();
  /* Scan a comma separated list of arguments.  The arguments can be
     type names, constant expressions, or addresses of objects or functions
     with external linkage, or of static class members (WP 14.2).  It
     is not necessary to distinguish between the type and constant case
     because we can use the type of the formal parameter to make this
     selection. */
  param_ptr = template_sym->variant.template_info->parameters;
  do {
    /* If the current token is a ">" then exit the loop.  This should only be
       possible on the first iteration if we have an empty argument list. */
    if (curr_token == tok_gt) break;
    add_stop_token(tok_comma);
    sym = param_ptr->param_symbol;
    /* Determine whether this argument should be a type or a constant. */
    is_type_param = (sym->kind == (a_symbol_kind)sk_type);
    arg_ptr = alloc_template_arg(is_type_param);
    if (is_type_param) {
      a_source_position  arg_pos;

      arg_pos = pos_curr_token;
      type_name(&argument_type);
      /* Be sure the type does not involve any local classes -- only externally
         visible types are allowed, since template classes are themselves
         externally linked. */
      if (is_or_contains_local_type(argument_type)) {
        pos_error(ec_local_type_in_template_arg, &arg_pos);
        argument_type = error_type();
      }  /* if */
      arg_ptr->variant.type = argument_type;
    } else {  /* else executed when !is_type_param */
      a_type_ptr  constant_type = sym->variant.constant->type;
#if CHECKING
      if (sym->kind != (a_symbol_kind)sk_constant) {
        internal_error("coalesce_template_class_reference: constant expected");
      }  /* if */
#endif /* CHECKING */
      /* If the type of a constant involves a template parameter type,
         rescan the declaration of the parameter type to get the type
         to be used in this argument list. */
      if (param_ptr->variant.param_constant.type_involves_template_param) {
	constant_type = rescan_template_constant_parameter(template_sym,
							   param_ptr,
							   arg_list);
      }  /* if */
      constant = fs_constant((a_constant_repr_kind)ck_error);
      scan_template_argument_constant_expression(constant_type, constant);
      arg_ptr->variant.constant = constant;
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
    if (param_ptr->param_symbol->kind == (a_symbol_kind)sk_constant &&
	param_ptr->variant.param_constant.has_default_arg) {
      /* The template has parameters with default values.  Fill in the
         remainder of the parameter list with the defaults. */
      while (param_ptr != NULL) {
        sym = param_ptr->param_symbol;
        /* Determine whether this argument should be a type or a constant. */
        is_type_param = (sym->kind == (a_symbol_kind)sk_type);
        arg_ptr = alloc_template_arg(is_type_param);
	if (is_type_param) {
	  /* A type parameter.  This can only occur in error cases because
             type parameters cannot have default values.  Use an error type
	     as the template parameter. */
	  arg_ptr->variant.type = error_type();
	} else if (!param_ptr->variant.param_constant.has_default_arg) {
	  /* A nontype constant without a default argument.  This also only
	     occurs in error cases.  Use an error constant. */
          constant = fs_constant((a_constant_repr_kind)ck_error);
          arg_ptr->variant.constant = constant;
        } else {
          a_type_ptr  constant_type = sym->variant.constant->type;
          /* A constant parameter.  The default value can be either a
	     constant value or a token cache that needs to be scanned. */
	  if (param_ptr->variant.param_constant.type_involves_template_param) {
            /* If the type of a constant involves a template parameter type,
               rescan the declaration of the parameter type to get the type
               to be used in this argument list. */
            if (param_ptr->variant.param_constant.
						type_involves_template_param) {
              constant_type = rescan_template_constant_parameter(template_sym,
							         param_ptr,
							         arg_list);
            }  /* if */
	    rescan_reusable_cache(&param_ptr->variant.param_constant.
						default_arg.token_cache);
            constant = fs_constant((a_constant_repr_kind)ck_error);
	    delayed_scan_of_template_default_arg_expr(constant_type, constant);
	    arg_ptr->variant.constant = constant;
          } else {
	    arg_ptr->variant.constant =
		      param_ptr->variant.param_constant.default_arg.constant;
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
      any_errors = TRUE;
    }  /* if */
  } else if (curr_token == tok_comma) {
    /* All of the formal parameters have been accounted for and there are
       more actuals -- too many arguments were supplied. */
    pos_sy_error(ec_too_many_template_args, &pos_curr_token, template_sym);
    flush_to_end_of_arg_list();
    any_errors = TRUE;
  }  /* if */
  /* We should now be at the closing angle bracket.  Note that we don't
     scan the token after the closing angle because we update the current
     token below to represent the original identifier with the newly
     found template class symbol. */
  set_err_pos_to_curr_token();
  if (curr_token != tok_gt) {
    syntax_error(ec_exp_gt);
    any_errors = TRUE;
  }  /* if */
  if (!any_errors) {
    /* Everything is OK -- find the instance that matches these arguments.
       Create a new instance if needed.  There can be two instances
       of a class A<T> One is the prototype instantiation which is
       used when A<T> is referenced within the definition of class
       template A or in the declarator of an out-of-line definition of a member
       function or static data member.  The other is the nonreal class
       which is used when, for example, A<T> is a member of class template
       B.  We find the prototype instantiation unless we are currently
       inside the instantiation of a different class. */
    a_boolean	prototype_allowed;
    prototype_allowed = class_is_being_instantiated ||
                        depth_innermost_instantiation_scope == NO_SCOPE_DEPTH;
    new_sym = find_template_class(template_sym, &arg_list, &start_position,
                                  prototype_allowed);
    arg_list_coalesced = TRUE;
  } else {
    /* Free any allocated template arguments. */
    if (arg_list != NULL) free_template_arg_list(arg_list);
    /* An error occurred while scanning the argument list so make an error
       locator and return a pointer to its specific symbol. */
    make_specific_symbol_error_locator(&locator_for_curr_id);
    new_sym = locator_for_curr_id.specific_symbol;
  }  /* if */
  switch_back_to_original_region(region_to_switch_back_to);
  remove_stop_token(tok_gt);

normal_exit:
  /* When we return to the caller the current identifier should be an 
     identifier and the locator should point to the template class that we
     have just looked up. */
  curr_token = tok_identifier;
  /* Update the locator to reflect the new symbol that is being returned
     and restore the source position of the beginning of the template
     class reference.  The symbol header is updated to point to the
     symbol associated with the class template name.  The header will
     have been modified by scanning the argument list.  If a template
     argument list has been coalesced, set the do_not_clear_speecific
     symbol field of the locator.  This is needed to ensure because the
     argument list information is now represented by the fact that the
     specific symbol points to a particular template class instance, and
     this information cannot be recreated once the template reference has
     been coalesced. */
  locator_for_curr_id.specific_symbol = new_sym;
  locator_for_curr_id.do_not_clear_specific_symbol = arg_list_coalesced;
  locator_for_curr_id.symbol_header = new_sym->header;
  locator_for_curr_id.source_position = locator_pos;
  locator_for_curr_id.is_template_id = TRUE;
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


a_boolean f_check_for_generalized_identifier_errors
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
  /* If no position was specified use the current error position. */
  if (pos == NULL) pos = &error_position;
  qualified_name_error = (locator_for_curr_id.is_qualified_name &&
                          (options & GID_DISALLOW_QUALIFIED_NAME));
  global_qualifier_error = (locator_for_curr_id.is_global_qualified_name &&
                            (options & GID_DISALLOW_GLOBAL_QUALIFIER));
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
  }  /* if */
  if (!any_errors) {
    /* If GID_CLASS_MUST_BE_PROTOTYPE_INSTANTIATION is specified we are
       scanning a template definition of a member function or static
       data member.  The class portion must specify the prototype
       instantiation (i.e., the argument list must match the
       template parameter list). */
    if (locator_for_curr_id.is_qualified_name &&
        locator_for_curr_id.qualifier_class_type != NULL &&
        options & GID_CLASS_MUST_BE_PROTOTYPE_INSTANTIATION) {
      a_symbol_ptr  type_sym;
      a_type_ptr	type = locator_for_curr_id.qualifier_class_type;
      while (type->source_corresp.class_of_which_a_member != NULL) {
        type = type->source_corresp.class_of_which_a_member;
      }  /* while */
      type_sym = (a_symbol_ptr)type->source_corresp.assoc_info;
      if (!is_template_class_symbol(type_sym)) {
        /* The class is not a template class. */
        pos_ty_error(ec_not_a_class_template, &pos_curr_token, type);
        any_errors = TRUE;
      } else if (!is_prototype_instantiation_symbol(type_sym)) {
        /* The class is a template class but not the prototype
           instantiation.  Decide which of two errors should be issued
           for this case.  The usual cause of this error is using an
           incorrect template argument list (one that does not match the
           template parameter list, but this may also be caused if the
           class template definition is currently incomplete (so there is
           no prototype instantiation yet). */
        a_symbol_ptr	template_sym;
        template_sym =
              type_sym->variant.class_struct_union.extra_info->class_template;
        if (template_sym->variant.template_info->
                    variant.class_template.prototype_instantiation == NULL) {
          /* There is no prototype yet.  This is probably caused by the
             class template being incomplete at this point. */
          pos_error(ec_incomplete_type_not_allowed, &pos_curr_token);
          any_errors = TRUE;
        } else {
          /* The class is a template class but not the prototype
             instantiation. */
          pos_error(ec_must_be_prototype_instantiation, &pos_curr_token);
          any_errors = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
#if CHECKING
  if ((locator_for_curr_id.is_operator_name ||
       locator_for_curr_id.is_conversion_name) &&
      (options & GID_DISALLOW_OPERATOR_NAME)) {
    internal_error("f_check_for_generalized...: operator name error used");
  }  /* if */
#endif /* CHECKING */
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
      (void)skip_white_space();
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


a_boolean f_is_generalized_identifier_start(an_identifier_options_set options)
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
	i
	operator =
	operator int
	~A		When options & GID_DTOR_RECOGNIZED = TRUE
	A<int>		Template reference will be coalesced
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
constructs such as "p->int::~int".  Note that the qualifier_class_type field
in the locator normally contains the type of the qualifier portion of
a qualified name.  For nonclass vacuous destructors, however, it contains the
type of the thing after the "::~".  This is necessary because vacuous
destructors may not have a qualifier and the type information is still
needed by the caller in this case.  So class_type starts out with the
qualifier type and is updated by the vacuous destructor code to contain
the type of the destructor name following the "::~".  This really only
matters for error handling because in nonerror cases the two types
will be the same.

If the token following a class qualifier is not part of a valid identifier
we still return TRUE so that an appropriate diagnostic can be generated when
an attempt is made to use the thing after the qualifier.

This routine performs ambiguity and access checking on the components of the
qualified name.  Only the ambiguity errors are actually issued, however.
Information about any access errors is accumulated in a list of
an_access_error_descr entries pointed to by locator_for_curr_id.
It is the responsibility of the caller to ensure that either
issue_qualifier_access_errors or do_not_issue_qualifier_access_errors is
called to do the appropriate processing and free the entries on the list.
This routine issues any access errors encountered if the construct
scanned is a pointer to member.

This routine may only be called in C++ mode. 
*/
{
  a_type_ptr		class_type = NULL;
  a_boolean     	is_file_scope_qualified_name = FALSE;
  a_boolean		is_global_qualified_name = FALSE;
  a_boolean     	is_qualified_name = FALSE;
  a_boolean             is_ptr_to_member = FALSE;
  a_boolean		is_identifier = FALSE;
  a_symbol_ptr		class_symbol = NULL;
  a_source_position	start_position;
  a_source_position	orig_error_position;
  a_token_kind		next_tok;
  a_token_kind		next_tok_2;
  a_boolean		result = FALSE;
  a_boolean		err = FALSE;
  an_access_error_descr_ptr
			aedp = NULL;
  an_access_error_descr_ptr
			first_aedp = NULL;
  an_access_error_descr_ptr
			last_aedp = NULL;
  a_boolean		can_be_vacuous_dtor =
				 (options & GID_VACUOUS_DTOR_RECOGNIZED);
  a_boolean		dtor_must_be_nonclass =
				 (options & GID_DTOR_MUST_BE_NONCLASS);
  a_boolean		is_vacuous_dtor = FALSE;
  a_boolean		is_nonclass_dtor = FALSE;
  a_type_ptr		dtor_class_type = NULL;
  a_type_ptr		dtor_type = NULL;
  a_source_position	tilde_position;
  a_boolean             might_be_qualifier;
  a_token_kind          qualifier_separator = tok_colon_colon;
  a_boolean		class_type_is_really_a_class;

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
  orig_error_position = error_position;
  if (C_dialect != C_dialect_cplusplus) {
    /* Skip qualifier, destructor, and operator processing if not in C++
       mode.  If we have an identifier go to the code that updates the
       locator.  If we don't have an identifier, simply return. */
    result = curr_token == tok_identifier;
    if (result) goto wrapup;
    goto exit;
  }  /* if */
  /* Look for a leading unary "::".  Don't be fooled by "::new" and
     "::delete".  Don't treat ::* as a pointer to member declarator.
     ::* would be rejected below as a pointer to member declarator, but
     doing so here provides better error recovery. */
  if (curr_token == tok_colon_colon && !is_global_new_or_delete() &&
      next_token() != tok_star) {
    is_global_qualified_name = TRUE;
    is_file_scope_qualified_name = TRUE;
    is_qualified_name = TRUE;
    (void)get_token();
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
    if (next_tok == tok_colon_colon || next_tok == tok_lt) {
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
      }  /* if */
    }  /* if */
  }  /* if */
  if (might_be_qualifier) {
    /* Look up the identifier to see if it could be a class name.  Note that
       we don't consider the normal eclipsing rules.  A class can be found
       even when hidden by something else:
         class A {int i;};
         int f() {
           int A;
           A::i = 1;   // The class A is found.
         }
       
       If the name is not found, and vacuous destructor references are
       recognized, and the token following the "::" is a tilde, we repeat
       the lookup without the restriction that the name must be a class name.
    */
    if (dtor_class_type != NULL) {
      /* This looks like a vacuous destructor reference.  We may change
         this later if we don't find the right name following the "::". */
      is_vacuous_dtor = TRUE;
      class_symbol = NULL;
    } else {
      an_id_lookup_options_set	lookup_kind;
      a_boolean			might_be_vacuous_dtor;
      /* A normal qualified name or a vacuous destructor reference that
         begins with a normal qualified name (e.g., A::B::T::~T).
         If a vacuous destructor reference is allowed and the token
         following the "::" is a tilde, then the identifier we are looking
         up doesn't have to be a class name.  If the class lookup fails,
         do another lookup without the requirement that a class be found. */
      might_be_vacuous_dtor = next_tok_2 == tok_compl;
      /* The lookup of a class name in a qualified name is done as a
         "must be class" lookup.  If, however, the name being scanned is
         followed by a "<" we don't yet know whether this is a template
         reference or simply a less than sign.  We must assume it could
         be a less than sign and do a normal (nonclass) lookup. This
         should not make any difference for file scope lookups because
         class template names cannot coexist with other names at
         file scope.  A normal lookup must also be done when scanning
         what might be a use of a "." in place of "::" as a qualifier.  We
         don't know whether the "." is being used as a qualifier or as
         a field selection operator so we need to do a normal lookup
         and then decide based on the type of the thing we find. */
      if (next_tok == tok_lt || qualifier_separator == tok_period) {
        lookup_kind = IDL_NO_OPTIONS;
      } else {
        lookup_kind = IDL_MUST_BE_CLASS;
      }  /* if */
      if (is_global_qualified_name) {
        /* There was a leading unary "::", so look up the name in the file
           scope. */
        class_symbol = file_scope_id_lookup(&locator_for_curr_id,
                                            lookup_kind);
        if (class_symbol == NULL && might_be_vacuous_dtor) {
          class_symbol = file_scope_id_lookup(&locator_for_curr_id,
                                              IDL_NO_OPTIONS);
          is_vacuous_dtor = TRUE;
        }  /* if */
      } else {
        /* Usual case (no leading "::"). */
        class_symbol = normal_id_lookup(&locator_for_curr_id,
                                        lookup_kind);
        if (class_symbol == NULL && might_be_vacuous_dtor) {
          class_symbol = normal_id_lookup(&locator_for_curr_id,
                                          IDL_NO_OPTIONS);
          is_vacuous_dtor = TRUE;
        }  /* if */
        if (locator_for_curr_id.is_semivisible_nested_type) {
          /* The symbol in the locator is a nested class that is not visible
             according to the ARM lookup rules but is returned in support of
             the nested class anachronism (ARM 18.3.5). Issue an anachronism
             diagnostic. */
          sym_diagnostic(anachronism_error_severity,
                         ec_nested_class_anachronism,
                         locator_for_curr_id.specific_symbol);
        }  /* if */
      }  /* if */
      if (qualifier_separator == tok_period) {
        if (class_symbol != NULL && is_class_symbol(class_symbol)) {
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
      if (is_vacuous_dtor && class_symbol != NULL &&
          !is_type_symbol(class_symbol)) {
        class_symbol = NULL;
      }  /* if */
    }  /* if */
    /* Clear the specific symbol field which may have been set by the lookups
       performed above.  It must be cleared in case this isn't actually a
       qualified name.  We clear it now because it may be set again if a
       template reference is coalesced and we don't want to lose that value. */
    clear_specific_symbol(locator_for_curr_id);
    /* If the class symbol is for a class template, process the argument
       list. */
    if (class_symbol != NULL &&
        (class_symbol->kind == (a_symbol_kind)sk_class_template ||
         next_tok == tok_lt)) {
      /* Process a template reference.  This is considered a potential
         template reference if the symbol points to a class template
         or if the next token is a "<" (the latter case is handled here
         for error recovery purposes. */
      class_symbol = coalesce_template_class_reference(class_symbol,
                                                       options, &err);
    }  /* if */
    /* See if the identifier is followed by "::".  Note that nex_tok is not
       used because the next token may have changed while scanning a
       template argument list. */
    if (dtor_class_type != NULL) {
      /* We have a vacuous destructor reference of the form "int::~...".
         Skip of the code in the "else" clause that processing the
         rest of the class qualifier. */
      class_type = dtor_class_type;
      class_type_is_really_a_class = FALSE;
      (void)get_token();  /* Gets the type name. */
      (void)get_token();  /* The "::" that follows the type name. */
      is_qualified_name = TRUE;
      is_file_scope_qualified_name = FALSE;
    } else if (next_token() == qualifier_separator) {
      a_boolean         first_class = TRUE;
      a_source_position type_position;
      type_position = start_position;
      /* This is a qualifier. */
      is_qualified_name = TRUE;
      is_file_scope_qualified_name = FALSE;
      /* Restore the specific_symbol with the class symbol determined earlier.
         This needs to be restored so that access and ambiguity checking can
	 be done. */
      locator_for_curr_id.specific_symbol = class_symbol;
      for (;;) {
        /* Keep looping while there are more levels of class qualification.
           Exit from loop is in the middle. */
        if (class_symbol == NULL || err ||
            class_symbol->kind == (a_symbol_kind)sk_class_template) {
          /* The identifier is followed by a "::" but is not a class symbol. */
          if (!err) {
            if (is_vacuous_dtor) {
              error(ec_id_must_be_class_or_type_name);
            } else {
              error(ec_id_must_be_class_name);
            }  /* if */
            err = TRUE;
          }  /* if */
          class_type = NULL;
        } else {
          /* Record the reference on the symbol. */
          mark_referenced(class_symbol, &pos_curr_token);
          if (class_symbol->class_of_which_a_member != NULL) {
            /* Do ambiguity and access control checking on the class symbol.
               Only do the check if the symbol points to a class member.
               The requirement that the class symbol be a member also ensures
               that the check will be suppressed for template parameters
               (i.e., the T in T::X).  Access for template parameters should
               be checked at the point at which the type is used as a 
               template argument. Ambiguity errors will be issued but
               access errors will only be detected.  A pointer to the
               description of the access error,  if any, is returned in
               aedp.  If an error occurred, link the description onto the
               end of a list of errors. */
            aedp = NULL;
            member_check_ambiguity_verify_access_and_return_error_descr
		                                (&locator_for_curr_id, &aedp);
            if (aedp != NULL) {
              if (last_aedp != NULL) last_aedp->next = aedp;
              last_aedp = aedp;
              if (first_aedp == NULL) first_aedp = aedp;
            }  /* if */
          }  /* if */
          if (is_class_symbol(class_symbol)) {
            /* Get the type associated with the class symbol. */
            class_type = skip_typerefs(class_symbol->
                                          variant.class_struct_union.type);
            class_type_is_really_a_class = TRUE;
          } else {
            /* The class symbol points to a type.  This is the case when
               a class qualifier contains template parameter types or for
               the last qualifier of a vacuous destructor.  Set
               class type to the type pointed to. */
            check_assertion(class_symbol->kind == (a_symbol_kind)sk_type);
            class_type = class_symbol->variant.type;
            check_assertion(is_template_param_type(class_type) ||
                            is_vacuous_dtor);
            class_type_is_really_a_class = FALSE;
          }  /* if */
        }  /* if */
        /* Skip over the class-name, and the "::". */
        (void)get_token();
        if (get_token() != tok_identifier ||
            next_two_tokens_if_qualifier_delimiter(qualifier_separator,
                                                   &next_tok_2) !=
                                                       qualifier_separator) {
          /* Not an identifier followed by "::", so end the loop. */
          break;
        }  /* if */
        /* There is another level of qualification.  Search for the identifier
           in the given scope.  Once again, if vacuous destructor references
           are allowed we may need to repeat the lookup without the
           requirement that a class be found. */
        if (!err) {

          a_boolean	might_be_vacuous_dtor = next_tok_2 == tok_compl;
          if (first_class && class_type_is_really_a_class) {
            /* Make sure that this class has been instantiated.  This is
               only needed for the first class name because template classes
               must be at file scope. */
            check_for_uninstantiated_template_class(class_type);
          }  /* if */
          /* Make sure that the class type is a complete type. */
          if (class_type_is_really_a_class && is_incomplete_type(class_type) &&
              class_type->variant.class_struct_union.
                                         extra_info->assoc_scope == NULL) {
            /* If the type is incomplete we also check whether the type is
	       currently being defined -- it is considered complete if it
	       is being defined.  We determine this by checking the
	       assoc_scope field of the class type supplement. */
            pos_error(ec_incomplete_type_not_allowed, &type_position);
	    err = TRUE;
	    class_symbol = NULL;
          } else {
            class_symbol = class_qualified_id_lookup(&locator_for_curr_id,
                                                     class_type,
                                                     IDL_MUST_BE_CLASS);
            /* If the class lookup fails, and a vacuous destructor is
	       allowed, do another lookup without the requirement that
               a class be found. */
            if (class_symbol == NULL && might_be_vacuous_dtor) {
              class_symbol = class_qualified_id_lookup(&locator_for_curr_id,
                                                       class_type,
                                                       IDL_NO_OPTIONS);
              is_vacuous_dtor = TRUE;
              if (class_symbol != NULL && !is_type_symbol(class_symbol)) {
                class_symbol = NULL;
              }  /* if */
            }  /* if */
          }  /* if */
        }  /* if */
        first_class = FALSE;
        type_position = pos_curr_token;
      }  /* for */
    }  /* if */
  }  /* if */
  /* Assume we have found an identifier until we discover otherwise. */
  is_identifier = TRUE;
  if (is_qualified_name && !is_file_scope_qualified_name) {
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
               ((options & GID_DTOR_RECOGNIZED) || is_qualified_name)) {
      /* A destructor name (e.g., ~A or A::~A).  Destructor names are
         always recognized after qualifiers.  If not preceded by a qualifier,
         then they are only recognized when GID_DTOR_RECOGNIZED is TRUE. */
     /* If we have already discovered that we have a vacuous destructor
        reference, then it must be a non-class destructor reference
	(e.g., int::~int). */
     is_nonclass_dtor = is_vacuous_dtor;
     if (!is_vacuous_dtor && can_be_vacuous_dtor && is_qualified_name) {
       /* So far this looks like a normal destructor reference (i.e.,
          the qualified name represents a class, not some other type).
          See if the class has a destructor.  If it does not, this is a
          vacuous reference. */
       check_assertion_str(class_type != NULL, "figis: class_type == NULL");
       if (symbol_supplement_for_class(class_type)->destructor == NULL) {
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
      /* A class qualifier followed by something invalid.  Proceed as if
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
    /* The qualifier class type is the only field of the locator
       that is valid when curr_token is tok_ptr_to_member. */
    locator_for_curr_id.qualifier_class_type = class_type;
    /* Clear the is_template_id flag in the locator in case it was set before
       this was recognized to be ptr-to-member. */
    locator_for_curr_id.is_template_id = FALSE;
    /* For pointer to member, issue any access errors that were detected. */
    if (first_aedp != NULL) issue_qualifier_access_errors(&first_aedp);
    /* Since we're returning a pseudo-token, set pos_curr_token. */
    pos_curr_token = start_position;
    /* Restore the original error position. */
    error_position = orig_error_position;
  } else if (is_identifier) {
    /* A possibly qualified identifier. */
    result = TRUE;
    if (is_qualified_name) {
      /* Issue any access errors detected while scanning the class
         qualifier.  If access errors are to be suppressed, free the list
          of access errors. */
      if (first_aedp != NULL) {
        if (options & GID_SUPPRESS_ACCESS_ERRORS) {
          /* Discard the errors. */
          do_not_issue_qualifier_access_errors(&first_aedp);
        } else if (options & GID_DEFER_ACCESS_ERRORS) {
          /* Access errors are to be saved and possibly issued later. */
        } else {
          /* Issue the errors. */
          issue_qualifier_access_errors(&first_aedp);
        }  /* if */
      }  /* if */
      /* Make sure that the class has been instantiated. */
      if (!err && class_type != NULL && class_type_is_really_a_class) {
        check_for_uninstantiated_template_class(class_type);
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
           of which "i" is a member.  If "i" is not a member, then just
           a normal lookup.  Global qualifiers are not allowed in field
	   selection operators (which is the only place where vacuous
	   destructor references are allowed).  If a global qualifier is
	   present, skip the lookup and set class_type to NULL so that the
	   only error to be issued will be "global qualifier not allowed"
	   error issued by the coalesce routine. */
        a_symbol_ptr	type_sym = NULL;
	a_type_ptr	cowam;
        /* Set dtor_class_type to class_type.  This is only needed when
	   we have a typedef name.  For a type name like "int" it will
	   already have been set. */
        dtor_class_type = class_type;
        if (is_global_qualified_name) {
	  class_type = NULL;
	} else if (class_symbol != NULL &&
                   (cowam = class_symbol->class_of_which_a_member) != NULL) {
          type_sym = class_qualified_id_lookup(&locator_for_curr_id, cowam,
                                               IDL_NO_OPTIONS);
	} else {
	  type_sym = normal_id_lookup(&locator_for_curr_id, IDL_NO_OPTIONS);
        }  /* if */
        /* Clear the specific symbol found by these lookups. */
        clear_specific_symbol(locator_for_curr_id);
        if (type_sym != NULL && is_type_symbol(type_sym)) {
	  /* If the symbol found is a type, get the type pointed to. */
	  dtor_type = type_symbol_type(type_sym);
          dtor_type = skip_typerefs(dtor_type);
          /* This will eventually result in the locator qualifier class type
	     being set to the type of the vacuous destructor. */
          class_type = dtor_type;
	}  /* if */
      } else {
	/* A type keyword (e.g. int, long, etc.). Get the type
           associated with the keyword. */
        dtor_type = type_keyword();
        /* If the thing being scanned looks like "T::~int", where T is a
	   typedef, save the type pointed to as dtor_class_type.  This
	   will be used later for error checking. */
        if (dtor_class_type == NULL) dtor_class_type = class_type;
        /* This will eventually result in the locator qualifier class type
	   being set to the type of the vacuous destructor. */
        class_type = dtor_type;
        /* Make the current token a tok_identifier. */
        curr_token = tok_identifier;
      }  /* if */
      locator_for_curr_id.is_destructor_name = TRUE;
    } else if (((options & GID_DTOR_RECOGNIZED) || (is_qualified_name)) &&
        !is_file_scope_qualified_name) {
      /* The name can be a destructor name like "~A". */
      (void)get_destructor_name();
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
      if (!is_nonclass_dtor) {
	/* If class_type is NULL an error must have already occurred. */
        if (class_type != NULL) {
          /* If this is a vacuous destructor reference, just make sure
             the name of the destructor matches the name of the class. */
          a_symbol_ptr	class_sym;
          class_sym = (a_symbol_ptr)class_type->source_corresp.assoc_info;
          if (!destructor_name_matches_class_name(class_sym)) {
            pos_ty_error(ec_destructor_name_mismatch, &tilde_position,
			 class_type);
  	    err = TRUE;
            /* Set the class type to NULL as an indicator to the
	       coalesce routine that an error has occurred. */
	    class_type = NULL;
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
	  class_type = NULL;
        } else if (dtor_type == NULL ||
			(skip_typerefs(dtor_class_type) != dtor_type)) {
          pos_ty_error(ec_destructor_type_mismatch, &tilde_position,
		       dtor_class_type);
          err = TRUE;
          /* Set the class type to NULL as an indicator to the
	     coalesce routine that an error has occurred. */
	  class_type = NULL;
        }  /* if */
      }  /* if*/
    }  /* if */
    /* The name can be an operator name like "operator+". */
    (void)get_opname(class_type);
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
    /* Update the locator with information about the qualifier (if any)
       that was discovered by is_generalized_identifier_start. */
    locator_for_curr_id.is_qualified_name = is_qualified_name;
    locator_for_curr_id.is_global_qualified_name = is_global_qualified_name;
    locator_for_curr_id.is_file_scope_qualified_name =
						is_file_scope_qualified_name;
    locator_for_curr_id.qualifier_class_type = class_type;
    locator_for_curr_id.access_errors = first_aedp;
    locator_for_curr_id.has_been_coalesced = TRUE;
    locator_for_curr_id.is_vacuous_destructor_reference = is_vacuous_dtor;
    locator_for_curr_id.is_nonclass_destructor = is_nonclass_dtor;

    /* Since we're returning a pseudo-token, set pos_curr_token. */
    pos_curr_token = start_position;
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
  a_type_ptr		class_type = NULL;
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
      if (check_for_generalized_identifier_errors(options, &error_position)) {
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
      class_type = locator_for_curr_id.qualifier_class_type;
      is_vacuous_dtor = locator_for_curr_id.is_vacuous_destructor_reference;
      return_value = TRUE;
      /* Perform error checks as specified in "options". */
      if (check_for_generalized_identifier_errors(options,
						  &pos_curr_token)) {
        *err = TRUE;
        okay = FALSE;
      } else if (*err) {
        /* Don't try to lookup the identifier if an error occurred earlier. */
        okay = FALSE;
      } else {
        an_id_lookup_options_set	idl_options;
        /* Translate the general identifier options into ID lookup options. */
	idl_options = idl_options_for_lookup_mode[(int)ilm];
        /* No errors were diagnosed. */
        if (locator_for_curr_id.is_file_scope_qualified_name) {
          /* Look up the id in the file scope. */
          if (file_scope_id_lookup(&locator_for_curr_id,
                                   idl_options) != NULL) {
          } else {
            /* The identifier could not be found in the file scope. */
	    if (ilm == ilm_tentative_type) {
	      /* It is OK for a tentative type lookup to fail. */
	      okay = TRUE;
	    } else {
              /* Issue an alternate version of the error if we are looking
	         for a tag symbol. */
	      error_code = ilm == ilm_tag ? ec_name_not_tag_in_file_scope :
					    ec_name_not_found_in_file_scope;
              pos_st_error(error_code, &identifier_pos,
                           locator_for_curr_id.symbol_header->identifier);
              okay = FALSE;
            }  /* if */
          }  /* if */
        } else {
	  a_boolean	is_vacuous_dtor =
			 locator_for_curr_id.is_vacuous_destructor_reference;
	  a_boolean	is_nonclass_dtor =
			 locator_for_curr_id.is_nonclass_destructor;
          if (class_type == NULL) {
	    okay = FALSE;
          } else if (!is_nonclass_dtor && 
                     is_incomplete_type(class_type) &&
                     is_class_struct_union_type(class_type) &&
                     class_type->variant.class_struct_union.
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
              /* If class_type is NULL an error occurred while processing
		 the vacuous destructor.  Treat this the same way we would
		 a failed lookup. */
	      okay = class_type != NULL;
            } else {
              /* Look up the id in the class scope. */
              if (class_qualified_id_lookup(&locator_for_curr_id,  class_type,
					    idl_options) != NULL) {
                /* Ambiguity and access control checking is not done because
                   we don't know yet what kind of reference this is. */
	      } else {
                /* The identifier could not be found in the class scope. */
	        if (ilm == ilm_tentative_type) {
		  /* It is OK for a tentative type lookup to fail. */
		  okay = TRUE;
		} else {
                  /* Issue an alternate version of the error if we are looking
		     for a tag symbol. */
	          error_code = ilm == ilm_tag ?
                               ec_not_a_tag_member :
                               (C_mode() ? ec_not_a_field : ec_not_a_member);
                  pos_stsy_error(error_code, &identifier_pos,
                                 locator_for_curr_id.symbol_header->identifier,
                                 (a_symbol_ptr)class_type->
                                                    source_corresp.assoc_info);
                  okay = FALSE;
                }  /* if */
              }  /* if */
            }  /* if */
          }  /* if*/
        }  /* if */ 
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
    locator_for_curr_id.qualifier_class_type = class_type;
    locator_for_curr_id.is_vacuous_destructor_reference = is_vacuous_dtor;
    *err = TRUE;
  }  /* if */
  /* Issue access errors if needed. */
  if (locator_for_curr_id.access_errors != NULL) {
    if (options & GID_SUPPRESS_ACCESS_ERRORS) {
      do_not_issue_qualifier_access_errors(&locator_for_curr_id.access_errors);
    } else if (options & GID_DEFER_ACCESS_ERRORS) {
      /* Do nothing for the time being. */
    } else {
      issue_qualifier_access_errors(&locator_for_curr_id.access_errors);
    }  /* if */
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
    /* If this is the symbol of a class template then this must be a reference
       to a instance of the class template.  Scan the argument list and
       get a pointer to the symbol for the specific instance of the template
       class. */
    if (symbol != NULL && symbol->kind == (a_symbol_kind)sk_class_template) {
      symbol = coalesce_template_class_reference(symbol, options, &templ_err);
    }  /* if */
    *err |= templ_err;
    /* If an error occurred while scanning the template argument list,
       return a NULL symbol.  The locator will already be set to an error
       locator. */
    if (templ_err) symbol = NULL;
  }  /* if */
  /* Perform error checks as specified in "options". */
  *err |= check_for_generalized_identifier_errors(options, &error_position);
  return symbol;
}  /* coalesce_and_lookup_generalized_identifier */


#if INSTANTIATION_BY_IMPLICIT_INCLUSION
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


void add_to_instantiation_file_suffix_list(char* suffix,
                                           int   length)
/*
Add a new entry to the end of the implicit instantiation file suffix list.
If the entry is already on the list the new entry is ignored.
*/
{
  a_file_suffix_ptr	fsp;
  a_file_suffix_ptr	prev_fsp = NULL;
  a_boolean		found = FALSE;

  fsp = implicit_instantiation_file_suffix_list;
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
      implicit_instantiation_file_suffix_list = fsp;
    } else {
      prev_fsp->next = fsp;
    }  /* if */
#if DEBUG
    if (debug_level >= 5) {
      fprintf(f_debug, "Added \"%s\" to the suffix list.\n", fsp->suffix);
    }  /* if */
#endif /* DEBUG */
  }  /* if */
}  /* add_to_instantiation_file_suffix_list */


void add_list_of_suffixes_to_instantiation_file_suffix_list(char *list)
/*
Add the members of a colon separated list of file suffixes to the
instantiation file suffix list.
*/
{
  char	*ptr = list;
  char	*start;
  char	*end;

  while (*ptr) {
    /* Skip of any spaces. */
    while (*ptr == ' ') ptr++;
    /* See if we've reached the end of the string. */
    if (!*ptr) break;
    /* Check for a null string entry. */
    if (*ptr == ':') {
      ptr++;
      continue;
    }  /* if */
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
    add_to_instantiation_file_suffix_list(start, (int)(end - start + 1));
    /* If we haven't reached the end of the string, move the pointer past
       the delimiter. */
    if (*ptr) ptr++;
  }  /* while */
}   /* add_list_of_suffixes_to_instantiation_file_suffix_list */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */


void begin_rescan_of_pragma_tokens(a_pending_pragma_ptr ppp,
				   a_stop_token_array   save_stop_token_array)
/*
Active the token cache containing the pragma to be scanned and push a
pragma scope to be used while scanning the pragma tokens.
*/
{
  /* Save and clear the list of tokens that will stop flushing on error, and
     put the newline token into it. */
  copy_stop_tokens(stop_token_array, save_stop_token_array);
  clear_stop_tokens();
  add_stop_token(tok_newline);
  rescan_reusable_cache(&ppp->token_cache);
  /* Push a pragma scope.  This prevents names introduced by the pragma
     processing from polluting the current scope. */
  (void)push_scope((a_scope_kind)sck_pragma, NO_SCOPE_NUMBER, (a_type_ptr)NULL,
	           (a_routine_ptr)NULL, (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
	           (a_template_arg_ptr)NULL);
}  /* begin_rescan_of_pragma_tokens */


void wrapup_rescan_of_pragma_tokens(a_boolean	       error_in_pragma,
				    a_stop_token_array save_stop_token_array)
/*
This routine is called by pragma processing routines when they have reached
the end of the pragma directive being scanned.  This routine fetches
the token that terminates the token cache and returns the token stream
to its original state.  If the current token is not the newline that
terminates the pragma directive, an error is issued (unless the
error_in_pragma flag is set indicating the pragma processing routine already
diagnosed an error).  The pragma scope pushed when the token cache
is actived is popped here.
*/
{
  if (curr_token != tok_newline) {
    if (!error_in_pragma) {
      pos_error(ec_extra_text_in_pp_directive, &pos_curr_token);
    }  /* if */
    /* Flush any tokens until a newline is found.  Also stop at end of
       source just in case the user pragma processing routine left us in
       an unexpected state. */
    while (curr_token != tok_newline && curr_token != tok_end_of_source) {
      (void)get_token();
    }  /* while */
  }  /* if */
  check_assertion_str(curr_token == tok_newline,
                      "wrapup_rescan_of_pragma_tokens: tok_newline expected");
  /* Bypass the newline token. */
  (void)get_token();
  check_assertion_str(curr_token == tok_end_of_source,
                 "wrapup_rescan_of_pragma_tokens: tok_end_of_source expected");
  /* Bypass the cache terminator. */
  (void)get_token();
  /* Restore the stop token set as at entry. */
  copy_stop_tokens(save_stop_token_array, stop_token_array);
  /* Pop the pragma scope. */
  pop_scope();
}  /* wrapup_rescan_of_pragma_tokens */


#if DEBUG
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
  db_space_used("reusable cache pragmas",
                 num_pragmas_in_reusable_caches, a_pending_pragma);
  db_space_used("pragma kind descriptions", num_pragma_descriptions_allocated,
                a_pragma_kind_description);
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
  db_space_used("file suffixes", num_file_suffixes_allocated,
                a_file_suffix);
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */

  total = after_end_of_curr_source_line - curr_source_line;
  db_space_used_general_buffer("curr_source_line", total);
  if (size_pragma_string_buffer != 0) {
    db_space_used_general_buffer("pragma string",
                                 ((unsigned long)size_pragma_string_buffer));
  }  /* if */

  if (after_end_of_raw_listing_buffer != NULL) {
    total = after_end_of_raw_listing_buffer - raw_listing_buffer;
    db_space_used_general_buffer("raw_listing_buffer", total);
  }  /* if */

  db_space_used_total();

  return (grand_total);
}  /* show_lexical_space_used */
#endif /* DEBUG */


void lexical_init(void)
/*
Initialize static variables related to the lexical routines.  This is done
as a subroutine (rather than relying on static initialization) so that it
can be redone to compile more than one source file in a single invocation
of the front end.
*/
{
  register int c;  /* Has to be "int" so "for" loop will work. */

  /* Variables in lexical.h: */
  depth_input_stack = -1;
  curr_ise = NULL;
  seq_number_last_read = 0;
  curr_seq_number = 0;
  orig_line_modif_list = NULL;
  end_orig_line_modif_list = NULL;
  avail_orig_line_modifs = NULL;
  source_line_modif_list = NULL;
  line_start_source_line_modif = NULL;
  avail_source_line_modifs = NULL;
  sequence_id_for_source_line_modifs = 0;
  delete_source_from_loc = NULL;
  /* Clear the set of tokens on which to stop a flush following a
     syntax error. */
  clear_stop_tokens();
  curr_token_pragmas = NULL;
  /* Static variables in lexical.c: */
  curr_input_stream = NULL;
  eof_read_on_curr_input_stream = FALSE;
  at_end_of_source_file = FALSE;
  after_end_of_all_source = FALSE;
  init_do_not_put_curr_line_in_pp_output = TRUE;
  curr_raw_listing_line_code = '\0';
  cached_token_rescan_list = NULL;
  avail_cached_tokens = NULL;
  avail_cached_constants = NULL;
  avail_reusable_cache_entries = NULL;
  avail_pending_pragmas = NULL;
  reusable_cache_stack = NULL;
  dollar_in_id_diagnostic_issued = FALSE;
  any_initial_get_token_tests_needed = FALSE;
  last_token_sequence_number_used = NO_TOKEN_SEQUENCE_NUMBER;
  curr_token_sequence_number = NO_TOKEN_SEQUENCE_NUMBER;
#if DEBUG
  num_orig_line_modifs_allocated = 0;
  num_source_line_modifs_allocated = 0;
  num_cached_tokens_allocated = 0;
  num_cached_tokens_in_reusable_caches = 0;
  num_pragmas_in_reusable_caches = 0;
  num_cached_constants_allocated = 0;
  num_reusable_cache_entries_allocated = 0;
  num_pending_pragmas_allocated = 0;
  num_pragma_descriptions_allocated = 0;
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
  num_file_suffixes_allocated = 0;
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
#endif /* DEBUG */

  /* Do the initial allocation for curr_source_line the first time this
     routine is called.  Since the space is allocated in general storage,
     it does not need to be reallocated for each source file.  For the same
     reason, after_end_of_curr_source_line should not be reset. */
  if (after_end_of_curr_source_line == NULL) {
    /* First time through.  Do initial allocation for the source line.
       The space will be reallocated (larger) if necessary, but the size
       here should be big enough for the expected cases. */
    /* Allocate one more byte than required, so that a pointer past the end
       will not have the same address as a pointer to the next object in
       memory. */
    curr_source_line = alloc_general(
                            (sizeof_t)(CURR_SOURCE_LINE_INITIAL_ALLOCATION+1));
    after_end_of_curr_source_line = curr_source_line +
                                    CURR_SOURCE_LINE_INITIAL_ALLOCATION;
  }  /* if */
  /* Similar allocation for raw_listing_buffer.  Similar reasoning. */
  if (f_raw_listing != NULL) {
    if (after_end_of_raw_listing_buffer == NULL) {
      raw_listing_buffer = alloc_general(
                              (sizeof_t)RAW_LISTING_BUFFER_INITIAL_ALLOCATION);
      after_end_of_raw_listing_buffer = raw_listing_buffer +
                                        RAW_LISTING_BUFFER_INITIAL_ALLOCATION;
    }  /* if */
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
                              isdigit((unsigned char)c) ||
                              c == '_' ||
                              (c == '$' && allow_dollar_in_id_chars));
  }  /* for */
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
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
  /* Create the instantiation file suffix list if it has not already been
     created. */
  if (implicit_instantiation_file_suffix_list == NULL) {
    add_list_of_suffixes_to_instantiation_file_suffix_list
                                    (DEFAULT_INSTANTIATION_FILE_SUFFIX_LIST);
  }  /* if */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
}  /* lexical_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
