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

preproc.c -- Preprocessing directives.

*/


/* Header files common to all files. */
#include "fe_common.h"
/* Header files used by files involved in declaration processing. */
#include "decl_hdrs.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Additional header files. */
#include "expr.h"
#include "macro.h"
#include "literals.h"
#if MICROSOFT_EXTENSIONS_ALLOWED
#include "ms_metadata.h"
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

typedef struct a_pp_if_stack_entry *a_pp_if_stack_entry_ptr;
typedef struct a_pp_if_stack_entry {
  /* Entry on the stack for preprocessing #ifs (and #ifdefs, etc.). */
  a_source_position
		if_pos;	/* Position at which the #if appeared. */
  a_boolean	else_encountered;
			/* TRUE once the #else (if any) has been seen. */
} a_pp_if_stack_entry;

static a_pp_if_stack_entry_ptr
		pp_if_stack;
			/* Stack of preprocessing ifs.  Dynamically allocated;
			   can be expanded if necessary.  size_pp_if_stack
			   gives the number of elements currently allocated.
			   Allocation is not per-file. */
static sizeof_t	size_pp_if_stack;
			/* Allocated size in elements of pp_if_stack.
			   Not per-file. */
#define PP_IF_STACK_INCREMENTAL_ALLOCATION 30
			/* The number of elements added to pp_if_stack each
			   time it is reallocated; also the initial
			   allocation. */
/* See preproc.h for pp_if_stack_depth and base_pp_if_stack_depth. */

static a_source_position
		pos_of_curr_directive;
			/* The source position of the current preprocessing
			   directive. */

static a_boolean
		is_header_stop_dir;
			/* TRUE when the directive being scanned is the
			   header stop directive after which a precompiled
			   header file should be generated. */

static a_text_buffer_ptr
		header_name_buffer;
			/* Text buffer used by copy_header_name. */

#if MICROSOFT_EXTENSIONS_ALLOWED

static a_hash_table_ptr
		include_alias_hash_table;
			/* A hash table used to search for include alias
			   entries.  NULL if no such aliases exist. */

#if DEBUG
static unsigned long
		num_include_aliases_allocated;
			/* Count of include aliases allocated, for space used
			   purposes. */
#endif /* DEBUG */

/*
Entry used by the include_alias_hash_table to record information about
include_alias pragmas that have been encountered.  This is used for the
implementation of the Microsoft include_alias pragma.
*/
typedef struct an_include_alias *an_include_alias_ptr;
typedef struct an_include_alias {
  char		*long_file_name;
			/* The file name that is to be aliased to another
			   name.  This contains the raw characters of the
			   header name token. */
  sizeof_t	long_file_name_length;
			/* The length of long_file_name, not including the
			   null terminator. */
  char		*short_file_name;
			/* The file name to be used in place of
			   long_file_name.  This contains the file name
			   after conversions such as possible conversion to
			   UTF-8. */
} an_include_alias;


static void clear_include_alias(an_include_alias_ptr iap)
/*
Initialize the fields of an include alias entry.
*/
{
  iap->long_file_name = NULL;
  iap->long_file_name_length = 0;
  iap->short_file_name = NULL;
}  /* clear_include_alias */


static an_include_alias_ptr alloc_include_alias(void)
/*
Allocate a new include alias entry, initialize its fields, and return a
pointer to it.
*/
{
  an_include_alias_ptr	iap;

  iap = alloc_fe_of_type(an_include_alias);
#if DEBUG
  num_include_aliases_allocated++;
#endif /* DEBUG */
  clear_include_alias(iap);
  return iap;
}  /* alloc_include_alias */


a_hash_value hash_include_alias(a_void_ptr	key)
/*
Produce a hash value for an include alias entry.  The key is
an_include_alias_ptr.
*/
{
  a_hash_value		value;
  an_include_alias_ptr	iap;

  iap = (an_include_alias_ptr)key;
  value = hash_source_string((a_void_ptr)iap->long_file_name);
  return value;
}  /* hash_include_alias */


a_boolean compare_include_alias(a_void_ptr	entry,
                                a_void_ptr	key)
/*
Compare an entry in the include alias hash table with an entry to be
found.  "entry" and "key" are of type an_include_alias_ptr.  Return TRUE
if the key matches the entry.  Return TRUE if the entries match.
*/
{
  an_include_alias_ptr	entry_iap;
  an_include_alias_ptr	key_iap;
  a_boolean		result;

  entry_iap = (an_include_alias_ptr)entry;
  key_iap = (an_include_alias_ptr)key;
  result = entry_iap->long_file_name_length ==
                                              key_iap->long_file_name_length &&
           strcmp(entry_iap->long_file_name, key_iap->long_file_name) == 0;
  return result;
}  /* compare_include_alias */


static an_include_alias_ptr find_or_create_include_alias(
						char		*long_name,
						char		*short_name,
						a_boolean	create)
/*
Look for long_name in the include alias hash table.  If it is not found
and create is TRUE, add an entry.  A pointer to the entry is returned, or
NULL if no entry was found or created.  If create is TRUE and an existing
entry is found, the short file name that entry refers to is updated to
refer to the new short_name.
*/
{
  an_include_alias	ia;
  an_include_alias_ptr	*iap_in_table;
  an_include_alias_ptr	iap = NULL;

  /* Create an include alias entry that describes the entry to be found.
     This is the key used for the hash table lookup. */
  clear_include_alias(&ia);
  ia.long_file_name = long_name;
  ia.long_file_name_length = strlen(long_name);
  iap_in_table = (an_include_alias_ptr*)hash_find(
						include_alias_hash_table,
						(a_void_ptr)&ia, create);
  if (iap_in_table != NULL) iap = *iap_in_table;
  if (create) {
    /* Create the entry to be referenced by the hash table.  If a previous
       entry was found, the short file name that it refers to will be replaced
       with the new one. */
    if (iap == NULL) {
      iap = alloc_include_alias();
      *iap_in_table = iap;
      /* Copy the key entry created above into the new entry. */
      *iap = ia;
    }  /* if */
    iap->short_file_name = short_name;
  }  /* if */
  return iap;
}  /* find_or_create_include_alias */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static char *check_for_include_alias(void)
/*
Check whether the current header name token refers to a file name for
which an include_alias pragma has been seen.
*/
{
#if MICROSOFT_EXTENSIONS_ALLOWED
  an_include_alias_ptr	iap;
  char			*result = NULL;
  a_text_buffer_ptr	buf = header_name_buffer;

  /* The hash table will only exist if an include alias has been seen. */
  if (include_alias_hash_table != NULL) {
    /* Extract the raw characters of the header name from the token. */
    reset_text_buffer(buf);
    (void)add_to_text_buffer(buf, start_of_curr_token, len_of_curr_token);
    add_char_to_text_buffer(buf, '\0');
    iap = find_or_create_include_alias(buf->buffer, (char*)NULL,
                                       /*create=*/FALSE);
    if (iap != NULL) result = iap->short_file_name;
#if DEBUG
    if (db_flag_is_set("include_alias")) {
      fprintf(f_debug, "Looking for alias for %s, found %s\n", buf->buffer,
              result == NULL ? "NULL" : result);
    }  /* if */
#endif /* DEBUG */
  }  /* if */
  return result;
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
  return NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* check_for_include_alias */


/* Advance declaration needed because of mutual recursion: */
static void skip_to_endif(a_boolean stop_skip_on_else_or_elif);
static void pass_directive_to_output(void);


/*
Macro that compares the current token (an identifier) against a constant
string, and returns true if the two match.
*/
#define curr_id_is(str)                                               \
  (len_of_curr_token == sizeof(str)-1 &&                              \
   strncmp(str, start_of_curr_token, size_t_arg(sizeof(str)-1)) == 0)

/*
Macro that compares the current token (an identifier) with the pragma
identifier associated with the specified pragma kind.
*/
#define curr_id_matches_pragma_id(pragma_kind)				\
   (len_of_curr_token == strlen(pragma_ids[(int)(pragma_kind)]) &&	\
    strncmp(pragma_ids[(int)(pragma_kind)], start_of_curr_token,	\
            size_t_arg(len_of_curr_token)) == 0)


static a_pp_directive_kind identify_dir_keyword(void)
/*
Identify the keyword of the current preprocessing directive, and return the
proper value for it, or ppd_not_valid if the keyword is not valid.
The current source position is just after the "#" of the directive, with
the "#" the current token (at least logically).
*/
{
  register a_pp_directive_kind kind;

  some_error_in_curr_directive = FALSE;
  /* If preprocessing output is being produced, delete this directive.
     This usually leaves a blank line in place of the directive. */
  do_not_put_curr_line_in_pp_output = TRUE;
  /* Skip horizontal white space following the "#", and get the keyword.
     exp_digit_sequence is set so that the line number of the
     line-identifying directive will be scanned correctly. */
  exp_digit_sequence = TRUE;
  (void)get_token();
  exp_digit_sequence = FALSE;
  /* Remember the position of the directive, for error purposes. */
  copy_source_position(pos_curr_token, pos_of_curr_directive);
  if (curr_token == tok_newline) {
    /* Null directive. */
    kind = ppd_null;
  } else if (curr_token == tok_digit_sequence) {
    /* A line-identifying directive (output from cpp); this is similar
       to a #line directive, but not exactly the same. */
    kind = ppd_linedef;
  } else if (curr_token != tok_identifier) {
    /* The token following the "#" is not an identifier: error. */
    kind = ppd_not_valid;
  } else {
    /* Identify the directive keyword. */
    /* The tests here should be in order of expected frequency. */
    if (       curr_id_is("if")) {
      /* #if directive. */
      kind = ppd_if;
    } else if (curr_id_is("ifdef")) {
      /* #ifdef directive. */
      kind = ppd_ifdef;
    } else if (curr_id_is("ifndef")) {
      /* #ifndef directive. */
      kind = ppd_ifndef;
    } else if (curr_id_is("else")) {
      /* #else directive. */
      kind = ppd_else;
    } else if (curr_id_is("endif")) {
      /* #endif directive. */
      kind = ppd_endif;
    } else if (curr_id_is("elif")) {
      /* #elif directive. */
      kind = ppd_elif;
    } else if (curr_id_is("define")) {
      /* #define directive. */
      kind = ppd_define;
    } else if (curr_id_is("include")) {
      /* #include directive. */
      kind = ppd_include;
    } else if (curr_id_is("undef")) {
      /* #undef directive. */
      kind = ppd_undef;
    } else if (curr_id_is("line")) {
      /* #line directive. */
      kind = ppd_line;
    } else if (curr_id_is("pragma")) {
      /* #pragma directive. */
      kind = ppd_pragma;
    } else if (curr_id_is("error")) {
      /* #error directive. */
      kind = ppd_error;
    } else if (!strict_ansi_mode && curr_id_is("warning")) {
      /* #warning directive. */
      kind = ppd_warning;
#if IDENT_DIRECTIVE_AND_PRAGMA
    } else if (curr_id_is("ident")) {
      /* #ident directive. */
      kind = ppd_ident;
#endif /* IDENT_DIRECTIVE_AND_PRAGMA */
#if ALIAS_DIRECTIVE
    } else if (curr_id_is("alias")) {
      /* #alias directive. */
      kind = ppd_alias;
#endif /* ALIAS_DIRECTIVE */
#if ATT_PREPROCESSING_EXTENSIONS_ALLOWED
    } else if (curr_id_is("assert")) {
      /* #assert directive (an AT&T extension in System V release 4). */
      kind = ppd_assert;
    } else if (curr_id_is("unassert")) {
      /* #unassert directive (an AT&T extension in System V release 4). */
      kind = ppd_unassert;
#endif /* ATT_PREPROCESSING_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (microsoft_mode && curr_id_is("import")) {
      /* #import directive (a Microsoft extension). */
      kind = ppd_import;
    } else if (curr_id_is("using")) {
      /* #using directive (a C++/CLI directive).  A diagnostic will be issued
         if C++/CLI is not enabled. */
      kind = ppd_using;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else if (curr_id_is("include_next")) {
      /* #include_next directive. */
      kind = ppd_include_next;
    } else {
      kind = ppd_not_valid;
    }  /* if */
  }  /* if */
  return(kind);
} /* identify_dir_keyword */


static void nonstandard_pp_directive(void)
/*
Issue a diagnostic for a use of a nonstandard preprocessing directive.
*/
{
  if (strict_ansi_mode) {
    diagnostic(strict_ansi_discretionary_severity, ec_nonstd_pp_directive);
  }  /* if */
}  /* nonstandard_pp_directive */


static void ignore_harmless_trailing_comment(void)
/*
Ignore a harmless comment at the end of a preprocessing directive.
If the dialect of C being compiled is pcc, this is done without
a diagnostic (pcc allows comments at the ends of several kinds of lines).
*/
{
  if (curr_token != tok_newline) {
    if (!pcc_preprocessing_mode) {
      pos_diagnostic(strict_ansi_mode ? strict_ansi_discretionary_severity
                                      : es_warning,
                     ec_extra_text_in_pp_directive, &pos_curr_token);
    }  /* if */
    flush_to_newline();
  }  /* if */
}  /* ignore_harmless_trailing_comment */


static void end_of_directive_processing(void)
/*
Do whatever processing is required at the end of all preprocessing directives.
This includes checking that there is no extra text beyond the expected
end of the directive.
*/
{
  /* Check that everything up to the closing newline has been taken.
     If not, flush to the newline, with an error only if there was no
     previous error (or a warning in Microsoft mode). */
  if (curr_token != tok_newline) {
    if (!some_error_in_curr_directive) {
      pos_diagnostic(microsoft_mode ? es_warning : es_discretionary_error,
                     ec_extra_text_in_pp_directive, &pos_curr_token);
    }  /* if */
    flush_to_newline();
  }  /* if */
}  /* end_of_directive_processing */


static void scan_if_expr(a_boolean *condition)
/*
Scan the expression for a #if or #elif, and return in *condition its
truth value.
*/
{
  a_boolean  save_fetch_pp_tokens = fetch_pp_tokens;
  a_boolean  save_expand_macros = expand_macros;
  a_constant temp_const;

  fetch_pp_tokens = FALSE;
  expand_macros = TRUE;
  in_pp_if_expression = TRUE;
  (void)get_token();
  /* Scan the conditional expression. */
  scan_pp_expression(&temp_const);
  in_pp_if_expression = FALSE;
  if (is_error_constant(&temp_const)) {
    *condition = FALSE;
    some_error_in_curr_directive = TRUE;
  } else {
    /* The constant is guaranteed to be integer. */
    /* Determine whether or not it is zero. */
    *condition = !eqlit_integer_constant(&temp_const, (a_host_large_integer)0);
  }  /* if */
  fetch_pp_tokens = save_fetch_pp_tokens;
  expand_macros = save_expand_macros;
}  /* scan_if_expr */


static void proc_else(a_boolean perform_else)
/*
Scan and process an #else directive.  If perform_else == TRUE, skip to
the #endif.
*/
{
  if (pp_if_stack_depth <= base_pp_if_stack_depth) {
    /* There was no #if corresponding to this #else. */
    error(ec_missing_pp_if);
    flush_to_newline();
  } else if (pp_if_stack[pp_if_stack_depth].else_encountered) {
    /* An #else has been seen already, so this is a second #else.
       Error except when emulating pcc or early versions of the Microsoft
       compiler, which didn't give an error. */
    if (C_dialect == C_dialect_pcc ||
        (microsoft_mode && microsoft_version < 1200)) {
      warning(ec_pp_else_already_appeared);
    } else {
      diagnostic(es_discretionary_error, ec_pp_else_already_appeared);
    }  /* if */
    flush_to_newline();
  } else {
    /* The #else is valid, process it. */
    a_byte	ifg_state = get_ifg_state();
    if (pp_if_stack_depth == (base_pp_if_stack_depth+1) &&
	ifg_state != IFG_STATE_FAIL &&
	ifg_state != IFG_STATE_ONCE) {
      /* We've encountered a #else at the outermost level.  This
         means that this file is not a candidate for suppression
         of subsequent includes. */
      set_ifg_state(IFG_STATE_FAIL);
    }  /* if */
    pp_if_stack[pp_if_stack_depth].else_encountered = TRUE;
    (void)get_token();
    ignore_harmless_trailing_comment();
    if (perform_else) {
      skip_to_endif(/*stop_skip_on_else_or_elif=*/FALSE);
    }  /* if */
  }  /* if */
}  /* proc_else */


static void proc_elif(a_boolean perform_elif)
/*
Scan and process an #elif directive.  If perform_elif == TRUE, scan and
evaluate the expression, and do the skip if appropriate.
*/
{
  if (pp_if_stack_depth <= base_pp_if_stack_depth) {
    /* There was no #if corresponding to this #elif. */
    error(ec_missing_pp_if);
    flush_to_newline();
  } else if (pp_if_stack[pp_if_stack_depth].else_encountered) {
    /* #else has already appeared; #elif is not valid here. */
    error(ec_pp_else_already_appeared);
    flush_to_newline();
  } else {
    /* The #elif is valid, process it. */
    a_byte	ifg_state = get_ifg_state();
    if (pp_if_stack_depth == (base_pp_if_stack_depth+1) &&
	ifg_state != IFG_STATE_FAIL &&
	ifg_state != IFG_STATE_ONCE) {
      /* We've encountered a #elif at the outermost level.  This
         means that this file is not a candidate for suppression
         of subsequent includes. */
      set_ifg_state(IFG_STATE_FAIL);
    }  /* if */
    if (perform_elif) {
      /* When an #elif is hit when not skipping, it always acts like an
         #else.  That is, the value of the expression is unimportant:
         the skip to #endif is always done. */
      flush_to_newline();
      skip_to_endif(/*stop_skip_on_else_or_elif=*/FALSE);
    }  /* if */
  }  /* if */
}  /* proc_elif */


static void proc_endif(void)
/*
Scan and process an #endif directive.
*/
{
  if (pp_if_stack_depth <= base_pp_if_stack_depth) {
    /* There was no #if corresponding to this #endif. */
    error(ec_missing_pp_if);
    flush_to_newline();
  } else {
    /* The #endif is valid, process it. */
    if (pp_if_stack_depth == (base_pp_if_stack_depth+1)) {
      /* Update the include file guard state for the current file.  If
         we've seen the opening #ifndef then go to the state that says
         that we're OK as long as we don't see any more tokens from this
         file.  If we are in any other state (not including ONCE, then
         goto the FAIL state. */
      a_byte	ifg_state = get_ifg_state();
      if (ifg_state == IFG_STATE_INTERMED) {
        set_ifg_state(IFG_STATE_ACCEPT);
      } else if (ifg_state != IFG_STATE_ONCE) {
        set_ifg_state(IFG_STATE_FAIL);
      }  /* if */
    }  /* if */
#if DEBUG
    if (debug_level >= 3) {
      fprintf(f_debug, "endif, pp_if_stack_depth = %ld\n", pp_if_stack_depth);
    }  /* if */
#endif /* DEBUG */
    pp_if_stack_depth--;
    /* Set things up for the check for extra text on the end of the line. */
    (void)get_token();
    ignore_harmless_trailing_comment();
  }  /* if */
}  /* proc_endif */


static void push_pp_if_stack(void)
/*
Push a new entry on the preprocessing-if stack (pp_if_stack).
*/
{
  if ((sizeof_t)pp_if_stack_depth+1 == size_pp_if_stack) {
    /* Stack is full; expand it. */
    sizeof_t new_size = size_pp_if_stack + PP_IF_STACK_INCREMENTAL_ALLOCATION;
    pp_if_stack = (a_pp_if_stack_entry_ptr)realloc_buffer(
                      (char *)pp_if_stack,
                      (sizeof_t)(size_pp_if_stack*sizeof(a_pp_if_stack_entry)),
                      (sizeof_t)(new_size*sizeof(a_pp_if_stack_entry)));
    size_pp_if_stack = new_size;
  }  /* if */ 
  pp_if_stack_depth++;
  copy_source_position(pos_of_curr_directive,
                       pp_if_stack[pp_if_stack_depth].if_pos);
  pp_if_stack[pp_if_stack_depth].else_encountered = FALSE;
#if DEBUG
  if (debug_level >= 3) {
    fprintf(f_debug, "push, pp_if_stack_depth = %ld\n", pp_if_stack_depth);
  }  /* if */
#endif /* DEBUG */
}  /* push_pp_if_stack */


static void skip_to_endif(a_boolean stop_skip_on_else_or_elif)
/*
Skip to the #endif corresponding to the current directive.  If
stop_skip_on_else_or_elif is true, also stop the skip on a
corresponding #else or #elif.  On entry, the position should be at
the newline of the preprocessing directive that is causing this skip.
*/
{
  a_boolean           condition;
  a_boolean           save_currently_in_pp_if_skip = currently_in_pp_if_skip;
  a_source_position   start_of_dir_position;

  db_enter(3, "skip_to_endif");
  /* Before leaving the current directive, check that all of it was taken.
     If this directive should allow a trailing comment, it should have
     been skipped before this routine was called. */
  end_of_directive_processing();
  fetch_pp_tokens = TRUE;
  expand_macros = FALSE;
  currently_in_pp_if_skip = TRUE;
  in_preprocessing_directive = FALSE;
  /* Loop until a #endif or the like is found. */
  for (;;) {
    /* Fetch and throw away tokens, looking for a "#" that is the first
       token in its logical source line. */
    for (;;) {
      /* End of file indicates a missing #endif.  End skip, let the driver
         routine put out the error. */
      if (curr_token == tok_end_of_source) {
        some_error_in_curr_directive = TRUE;
        goto end_skip;
      }  /* if */
      /* On a "#" at the start of a line, end the loop.  get_token will
         only return tok_sharp for a "#" that is the first token on a line. */
      if (get_token() == tok_sharp) break;
      /* Any other token. Ignore it. */
    }  /* for */
    /* "#" found, and it is on a different line than the previous
       token, and thus the first token on its line.  This is a preprocessing
       directive. */
    start_of_dir_position = pos_curr_token;
    in_preprocessing_directive = TRUE;
    /* Identify the directive and process it. */
    switch ((int)identify_dir_keyword()) {
      case ppd_endif:
        /* #endif, valid end of if-skip. */
        proc_endif();
        /* See if this is an #endif that also marks a PCH header stop
           position. */
        if (is_header_stop_position(start_of_dir_position)) {
          is_header_stop_dir = TRUE;
        }  /* if */
        goto end_skip;
      case ppd_else:
        proc_else(/*perform_else=*/FALSE);
        /* Stop skipping if we're supposed to stop on an else. */
        if (stop_skip_on_else_or_elif) goto end_skip;
        break;
      case ppd_elif:
        proc_elif(/*perform_elif=*/FALSE);
        /* Stop skipping if we're supposed to stop on an else. */
        if (stop_skip_on_else_or_elif) {
          /* Scan and evaluate the expression. */
          scan_if_expr(&condition);
          /* If the condition is TRUE, stop skipping. */
          if (condition) goto end_skip;
          /* Check for extra text beyond the end of the expression.  For
             the case where the skip is ended, the check is done at a
             higher level. */
          end_of_directive_processing();
        }  /* if */
        break;
      case ppd_if:
      case ppd_ifdef:
      case ppd_ifndef:
        /* Various #if's; we keep skipping, but we do so in a recursive
           call to this routine, since we have to find and throw away the
           matching #endif. */
        /* Throw away the expression. */
        flush_to_newline();
        push_pp_if_stack();
        skip_to_endif(/*stop_skip_on_else_or_elif=*/FALSE);
        break;
      case ppd_not_valid:
        /* Unrecognized directive.  Issue a warning in strict mode, and then
           ignore it. */
        if (strict_ansi_mode) {
          warning(ec_bad_pp_directive_keyword);
          some_error_in_curr_directive = TRUE;
        }  /* if */
        break;
      default:;
        /* Other directives -- ignored. */
    }  /* switch */
    in_preprocessing_directive = FALSE;
    /* Keep looping, looking for directives. */
  }  /* for */
end_skip:;
  currently_in_pp_if_skip = save_currently_in_pp_if_skip;
  db_exit();
}  /* skip_to_endif */


static void perform_if(a_boolean condition)
/*
Perform the processing for the beginning of an if-test:  If condition
is non-zero, set flags to allow a later #else or #endif, and return.
If condition is zero, skip now to the corresponding #else or #endif,
then return.
*/
{
  db_enter(3, "perform_if");
#if DEBUG
  if (debug_level >= 3) {
    fprintf(f_debug, "perform_if, condition = %d\n", condition);
  }  /* if */
#endif /* DEBUG */
  push_pp_if_stack();
  if (condition) {
    /* The condition is TRUE.  No action required, just return. */
  } else {
    /* The condition is FALSE.  Skip to the corresponding #else, #elif,
       or #endif.  The #if itself should be deleted. */
    skip_to_endif(/*stop_skip_on_else_or_elif=*/TRUE);
  }  /* if */
  db_exit();
}  /* perform_if */


static void check_for_if_defined_include_guard(void)
/*
This routine is called to determine whether an #if might be an include file
guard test.  It is only called if the include guard state indicates
that we are at the start of an include file.   It checks to see if the
current #if directive is of one of the following forms:

	#if defined(X)
	#if !defined(X)

If it is of one of those forms, the include guard state is updated to
indicate that a guard test of the macro X has been seen; otherwise, the
file is marked as not being a candidate for suppression of future includes.

The check is done by examining the current source line to look for one of the
patterns above.  The check is very strict (e.g., comments are not allowed
to appear in a valid pattern).  The include guard processing is an
optimization, so the failure to detect the presence of a guard does not
affect the proper compilation of the program.
*/
{
  char		*ptr;
  a_boolean	not_operator_present = FALSE;
  a_boolean	is_possible_include_guard = FALSE;

/* Local macro to skip white space characters. */
#define local_skip_white_space() while (*ptr == ' ' || *ptr == '\t') ptr++

  /* The current token is the identifier token for the "if".  Start scanning
      after the end of that token. */
  ptr = end_of_curr_token + 1;
  local_skip_white_space();
  /* Check for the presence of a "!". */
  if (*ptr == '!') {
    not_operator_present = TRUE;
    ptr++;
  }  /* if */
  local_skip_white_space();
  /* Check for the string "defined". */
  if (strncmp(ptr, "defined", 7) == 0) {
    /* Skip past the "defined". */
    ptr += 7;
    local_skip_white_space();
    /* Check for a "(". */
    if (*ptr++ == '(') {
      char	*id_start;
      sizeof_t	id_len;
      local_skip_white_space();
      /* Find the end of the macro identifier.  This only needs to be the
         actual end of the identifier for valid cases.  Other cases will be
         rejected by the call of is_valid_identifier below. */
      id_start = ptr;
      while (*ptr != ' ' && *ptr != '\t' && *ptr != ')' && *ptr != LE_ESCAPE) {
        ptr++;
      }  /* while */
      id_len = ptr - id_start;
      local_skip_white_space();
      /* Check for a "(". */
      if (*ptr++ == ')') {
        local_skip_white_space();
        /* We should now be at the end of the line. */
        if (*ptr == LE_ESCAPE && ptr[1] == LE_NEWLINE) {
          a_symbol_ptr		sym;
          a_symbol_locator	locator;
          /* The line matches our pattern.  If the identifier is valid,
             update the include guard information. */
          if (is_valid_identifier(id_start, id_len, &sym, &locator)) {
            is_possible_include_guard = TRUE;
            set_ifg_state(IFG_STATE_INTERMED);
            if (not_operator_present) {
              curr_ise->include_history->ifndef_guard = TRUE;
            } else {
              curr_ise->include_history->ifdef_guard = TRUE;
            }  /* if */
            curr_ise->include_history->controlling_macro_name =
                                             locator.symbol_header->identifier;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (!is_possible_include_guard) {
    /* Mark the file as not being a candidate for future suppression. */
    set_ifg_state(IFG_STATE_FAIL);
  }  /* if */
#undef local_skip_white_space
}  /* check_for_if_defined_include_guard */


static void proc_if(void)
/*
Scan and process an #if directive.
*/
{
  a_boolean	condition;
  a_byte	ifg_state = get_ifg_state();

  if (ifg_state == IFG_STATE_START) {
    /* We are at the start of an include file.  Look for an
       "#if !defined(X)" form of include guard. */
    check_for_if_defined_include_guard();
  } else if (ifg_state == IFG_STATE_ACCEPT) {
    set_ifg_state(IFG_STATE_FAIL);
  } else {
    /* Do nothing if state is FAIL, INTERMED or ONCE. */
  }  /* if */
  scan_if_expr(&condition);
  perform_if(condition);
}  /* proc_if */


static void proc_ifdef(a_boolean is_ifdef)
/*
Scan and process an #ifdef or #ifndef directive (is_ifdef == TRUE and
FALSE, respectively).
*/
{
  a_symbol_ptr		assoc_symbol;
  a_boolean		condition = FALSE;
  a_symbol_header_ptr	sym_hdr;

  if (get_token() != tok_identifier) {
    /* Expected an identifier.  If the token is an integer, give a warning
       rather than an error because UNIX code includes things like #ifdef 3b5,
       which is treated as undefined. */
    if ((!strict_ansi_mode || strict_ansi_error_severity != es_error) &&
        isdigit((unsigned char)*start_of_curr_token)) {
      warning(ec_exp_identifier);
      condition = FALSE;
      flush_to_newline();
    } else {
      syntax_error(ec_exp_identifier);
      some_error_in_curr_directive = TRUE;
    }  /* if */
  } else {
    a_byte   ifg_state = get_ifg_state();
    char     *id_ptr = start_of_curr_token;
    sizeof_t id_len = len_of_curr_token;
    /* Get the canonical spelling of the identifier. */
    if (id_contains_ucn_or_multibyte_char) {
      id_ptr = make_canonical_identifier(start_of_curr_token, &id_len);
    }  /* if */
    if (ifg_state == IFG_STATE_START) {
      /* If we are at the start of an include file then record 
         information about this @ifdef so that it can be used later to
         see if subsequent includes can be suppressed. */
      char *nm = alloc_fe(id_len + 2);
      strncpy(nm, id_ptr, size_t_arg(id_len));
      nm[id_len] = 0;
      set_ifg_state(IFG_STATE_INTERMED);
      if (is_ifdef) {
        curr_ise->include_history->ifdef_guard = TRUE;
      } else {
        curr_ise->include_history->ifndef_guard = TRUE;
      }  /* if */
      curr_ise->include_history->controlling_macro_name = nm;
    } else if (ifg_state == IFG_STATE_ACCEPT) {
      set_ifg_state(IFG_STATE_FAIL);
    } else {
      /* Do nothing if state is FAIL, INTERMED or ONCE. */
    }  /* if */
    /* The identifier __VA_ARGS__ is not allowed if variadic macros are
       accepted. */
    check_use_of_VA_ARGS(id_len, id_ptr);
    /* Look to see if there is a macro with this name. */
    sym_hdr = find_symbol_header(id_ptr, id_len, &locator_for_curr_id);
    assoc_symbol = find_defined_macro(sym_hdr);
    if (assoc_symbol != NULL) {
      condition = TRUE;
      mark_referenced(assoc_symbol, &pos_curr_token);
    }  /* if */
    if (!is_ifdef) condition = !condition;
    /* Move past the identifier. */
    (void)get_token();
    ignore_harmless_trailing_comment();
  }  /* if */
  /* Do the if processing. */
  perform_if(condition);
}  /* proc_ifdef */

#if RECORD_MACROS_IN_IL

static void make_il_undef_entry(a_symbol_ptr          undef_sym,
                                a_source_position_ptr undef_pos)
/*
Create an IL entry for an #undef of undef_sym.  The #undef has
source position *undef_pos.
*/
{
  sizeof_t    len;
  char        *ptr;
  a_macro_ptr mp;

  /* Allocate an IL area of the right size and put "#undef name" into it. */
#define UNDEF_STR "#undef "
  len = sizeof(UNDEF_STR) + undef_sym->header->identifier_length;
  ptr = alloc_il(len);
  (void)strcpy(ptr, UNDEF_STR);
  (void)strcpy(ptr + sizeof(UNDEF_STR) - 1, undef_sym->header->identifier);
  /* Allocate and fill in the IL macro entry. */
  mp = alloc_macro();
  mp->is_undef = TRUE;
  mp->text = ptr;
  mp->source_corresp.decl_position = *undef_pos;
  set_source_corresp(&mp->source_corresp, undef_sym);
  /* Add the macro entry to the IL list. */
  add_to_macros_list(mp);
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Add a source sequence entry for the #undef. */
  add_to_source_sequence_list((char *)mp, (an_il_entry_kind)iek_macro);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
}  /* make_il_undef_entry */

#endif /* RECORD_MACROS_IN_IL */

static void proc_undef(void)
/*
Scan and process an #undef directive.
*/
{
  a_symbol_ptr assoc_symbol;

  if (get_token() != tok_identifier) {
    /* Expected an identifier. */
    syntax_error(ec_exp_identifier);
    some_error_in_curr_directive = TRUE;
  } else {
    /* Get the canonical spelling of the identifier. */
    char     *id_ptr = start_of_curr_token;
    sizeof_t id_len = len_of_curr_token;
    if (id_contains_ucn_or_multibyte_char) {
      id_ptr = make_canonical_identifier(start_of_curr_token, &id_len);
    }  /* if */
    /* The identifier __VA_ARGS__ is not allowed if variadic macros are
       accepted. */
    check_use_of_VA_ARGS(id_len, id_ptr);
    /* Look to see if there is a macro with this name. */
    /* find_defined_macro cannot be used because if we have "#undef defined"
       we want to give an error, not ignore it. */
    assoc_symbol = find_macro_symbol_by_name(id_ptr, id_len,
	                                     &locator_for_curr_id);
    if (assoc_symbol == NULL) {
      /* No such macro, so #undef is ignored. */
    } else if (assoc_symbol->variant.macro_def->cannot_be_redefined &&
               !microsoft_mode && !gnu_mode) {
      /* The macro is predefined. */
      diagnostic(es_discretionary_error, ec_cannot_undef_predef_macro);
    } else {
      if (assoc_symbol->variant.macro_def->cannot_be_redefined && !gnu_mode) {
        /* The Microsoft compiler gives a warning for a case like this.
           The warning says the #undef is ignored, but it isn't.  In
           GNU mode, silently allow the undefinition. */
        warning(ec_cannot_undef_predef_macro);
      }  /* if */
#if RECORD_MACROS_IN_IL
      /* Make an IL entry for the #undef. */
      make_il_undef_entry(assoc_symbol, &pos_curr_token);
#endif /* RECORD_MACROS_IN_IL */
      /* Remove the macro's definition.  The a_macro_def entry pointed to
         by the symbol is not freed, and is therefore just lost.  */
      mark_referenced(assoc_symbol, &pos_curr_token);
      remove_symbol(assoc_symbol);
    }  /* if */
    /* Move past the identifier. */
    (void)get_token();
    ignore_harmless_trailing_comment();
  }  /* if */
}  /* proc_undef */


static a_boolean get_header_name(void)
/*
Scan a header name for a directive like a #include.  Return with
the current token variables set to indicate the complete header name
as a pseudo-token.  If the next token is not a header name, return
FALSE.
*/
{
  char              *p;
  a_source_position saved_pos_curr_token;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position saved_end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

  skip_white_space();
  if (*curr_char_loc == '<') {
    /* The next token appears to be a system header name.  Scan it as a
       single header name token.  Note that this is done only when the
       <...> appears at the top level, not when it appears within a
       macro invocation (in those cases, individual pp-tokens are scanned
       and then assembled into a header name token; see below). */
    exp_system_header_name = TRUE;
  }  /* if */
  /* Try to expand macros to get one of the normal forms. */
  expand_macros = TRUE;
  /* Fetch a token, expecting a header name. */
  exp_header_name = TRUE;
  (void)get_token();
  exp_header_name = FALSE;
  exp_system_header_name = FALSE;
  if (curr_token == tok_lt) {
    /* For <xxx.h> form header names that come from macro expansions,
       fetch the rest of the pp-tokens in the header name and make up a
       pseudo-token for the overall name. */
    saved_pos_curr_token = pos_curr_token;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    saved_end_pos_curr_token = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    pos_in_temp_text_buffer = 0;
    put_str_to_temp_text_buffer("<");
    /* Fetch tokens until ">" and put the text for them into the
       temp_text_buffer. */
    for (;;) {
      a_boolean white_space_skipped;
      skip_white_space();
      white_space_skipped = (kind_of_white_space_skipped != 0);
      /* End the loop on the closing ">". */
      if (get_token() == tok_gt) break;
      if (curr_token == tok_newline) {
        /* Missing closing ">". */
        curr_token = tok_error;
        pos_in_temp_text_buffer = 0;
        goto end_of_header_name;
      }  /* if */
      if (white_space_skipped) put_ch_to_temp_text_buffer(' ');
      for (p = start_of_curr_token; p <= end_of_curr_token; p++) {
        put_ch_to_temp_text_buffer(*p);
      }  /* for */
    }  /* for */
    put_str_to_temp_text_buffer(">");
    if (pos_in_temp_text_buffer == 2) {
      /* Error: empty <> is not valid. */
      curr_token = tok_error;
      pos_in_temp_text_buffer = 0;
      goto end_of_header_name;
    }  /* if */
    curr_token = tok_header_name;
end_of_header_name:
    start_of_curr_token = temp_text_buffer;
    len_of_curr_token = pos_in_temp_text_buffer;
    end_of_curr_token = start_of_curr_token+len_of_curr_token-1;
    pos_curr_token = saved_pos_curr_token;
    error_position = pos_curr_token;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    end_pos_curr_token = saved_end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  } else if (curr_token == tok_header_name && len_of_curr_token == 2) {
    /* Header names cannot be empty. */
    curr_token = tok_error;
  }  /* if */
  return (curr_token == tok_header_name);
}  /* get_header_name */


static void trim_leading_and_trailing_blanks_from_header_name(
                                                 char      **name,
                                                 sizeof_t  *len,
                                                 a_boolean trim_leading_blanks)
/*
The header name consists of *len characters starting at *name.  This routine
modifies those quantities to trim leading and trailing whitespace (only
trailing whitespace is trimmed if trim_leading_blanks is FALSE).
E.g., "    stdio   " becomes "stdio" with trim_leading_blanks TRUE.
*/
{
  char		*ptr;
  char		*last_nonblank;
  char		*end = *name + *len - 1;
  char		*begin = *name;
  sizeof_t	len_without_leading_blanks = *len;

  /* Scan past leading whitespace. */
  while (len_without_leading_blanks > 0 && (*begin == ' ' || *begin == '\t')) {
    ++begin;
    --len_without_leading_blanks;
  }  /* while */
  if (trim_leading_blanks || len_without_leading_blanks == 0) {
    /* Copy local results to caller. */
    *name = begin;
    *len = len_without_leading_blanks;
  }  /* if */
  if (len_without_leading_blanks > 0) {
    /* Find the last nonblank character of the name. */
    for (ptr = begin, last_nonblank = ptr; ptr <= end;
         increment_mbc_ptr(ptr)) {
       if (*ptr != ' ' && *ptr != '\t') last_nonblank = ptr;
    }  /* for */
    /* Trim trailing whitespace. */
    *len = last_nonblank - *name + 1;
  }  /* if */
}  /* trim_leading_and_trailing_blanks_from_header_name */


static char *copy_header_name(a_boolean process_escapes)
/*
Allocate and copy the file name from the current token (a header name).
Escapes in the string are processed only if process_escapes is TRUE.

When UNICODE_SOURCE_SUPPORTED is TRUE, this can also involve the
translation of certain characters to UTF-8.
*/
{
  char                    *name_start_pos, *in_pos;
  sizeof_t                name_len, i;
  unsigned long           ch;
  unsigned long           centity_mask;
  a_text_buffer_ptr       buf = header_name_buffer;
  char                    *result;
  sizeof_t                result_length;
  a_char_conversion_state conv_state;

  /* Build a mask used to mask individual characters. */
  centity_mask = (unsigned long)1 << (targ_host_string_char_bit-1);
  centity_mask = centity_mask | (centity_mask-1);
  name_len = len_of_curr_token - 2;  /* Drop quoting characters. */
  in_pos = start_of_curr_token+1;
  if (microsoft_mode) {
    /* Microsoft compilers ignore leading and trailing whitespace inside
       #include <...> directives and trailing whitespace inside
       #include "..." directives. */
    trim_leading_and_trailing_blanks_from_header_name(
                                                  &in_pos, &name_len,
                                                  *start_of_curr_token == '<');
  }  /* if */
  reset_text_buffer(buf);
  /* UTF-8 characters should not be translated to native multibyte
     characters. */
  clear_char_conversion_state(&conv_state, &in_pos,
                              /*translate_utf8=*/FALSE);
  /* Copy the string, processing escapes if appropriate.  The copy is done
     in two steps.  The first step processes one character (after processing
     of escapes, etc.) at a time.  The second step executed later in
     some configurations processes the resulting string and handles
     the conversion of any multibyte character sequences into Unicode. */
  /*lint --e{850} i modified in loop */
  for (i = 1; i <= name_len; i++) {
    char *prev_pos = in_pos;
    conv_single_char(&conv_state, process_escapes, &ch, centity_mask,
                     /*narrow_literal=*/TRUE);
    i += (in_pos - prev_pos) - 1;
#if UNICODE_SOURCE_SUPPORTED && !NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
    if (curr_file_unicode_source_kind == usk_none &&
        ch > 0x7f) {
      /* A character in the range 128-255 in a non-Unicode file must be
         converted to 2 bytes of UTF-8.  (This happens, for example, for
         European accented characters.) */
      char arr[4];
      (void)unicode_to_utf8(ch, arr);
      add_char_to_text_buffer(buf, arr[0]);
      ch = arr[1];
    }  /* if */
#endif /* UNICODE_SOURCE_SUPPORTED &&
          !NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
    add_char_to_text_buffer(buf, (char)ch);
  }  /* for */
  add_char_to_text_buffer(buf, '\0');
  result = buf->buffer;
  /* The length should not include the null terminator. */
  result_length = buf->size - 1;
#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
  if (curr_file_unicode_source_kind == usk_none) {
    /* Convert the string that resulted from the copy above from a
       non-Unicode multibyte encoding into UTF-8. */
    a_boolean	err;
    result = multibyte_chars_to_utf8(result, &result_length, &err);
    if (err) {
      pos_warning(ec_non_unicode_char_in_header, &pos_curr_token);
    }  /* if */
  }  /* if */
#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
  /* Increment the length to include the null terminator. */
  result_length++;
  /* Copy the name from the text buffer. */
  name_start_pos = alloc_primary_file_scope_il(result_length);
  (void)memcpy(name_start_pos, result, result_length);
  return name_start_pos;
}  /* copy_header_name */


static void proc_stdarg_include(a_boolean	is_cstdarg)
/*
Process an #include of <stdarg.h> by creating definitions for the things
the header defines instead of reading the header file.  This is used when
we want to pass references to the <stdarg.h> macros through to the output,
e.g., in generated C code.

is_cstdarg is TRUE in C++ if the header name was "cstdarg".
*/
{
  /* Ignore an #include after the first. */
  if (builtin_va_list_type == NULL) {
    /* Enter the va_start, va_arg, and va_end macros as keywords so they
       can be processed as expression operators. */
    enter_keyword((a_token_kind)tok_va_start, "va_start");
    enter_keyword((a_token_kind)tok_va_arg,   "va_arg");
    enter_keyword((a_token_kind)tok_va_end,   "va_end");
    /* Enter do-nothing macros for the three keywords, so that test suites
       that test that the header defined the macros will be happy. */
    (void)enter_predef_macro("va_start", "va_start",
                             /*cannot_be_redefined=*/FALSE,
                             /*ref_suppresses_pch_file=*/FALSE);
    (void)enter_predef_macro("va_arg", "va_arg",
                             /*cannot_be_redefined=*/FALSE,
                             /*ref_suppresses_pch_file=*/FALSE);
    (void)enter_predef_macro("va_end", "va_end",
                             /*cannot_be_redefined=*/FALSE,
                             /*ref_suppresses_pch_file=*/FALSE);
    if (va_copy_macro_allowed) {
      /* In addition, va_copy should be recognized in C99 mode. */
      enter_keyword((a_token_kind)tok_va_copy, "va_copy");
      (void)enter_predef_macro("va_copy", "va_copy",
                               /*cannot_be_redefined=*/FALSE,
                               /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
  }  /* if */
  /* Declare va_list as a type of "void *".  This call may be done multiple
     times, which is important if one include is of <stdarg.h> and the other
     is of <cstdarg>. */
  declare_builtin_va_list_type(is_cstdarg);
  if (generate_pp_output) {
    pass_directive_to_output();
  }  /* if */
}  /* proc_stdarg_include */


static void proc_include(a_boolean is_include_next,
                         a_boolean *was_simulated_stdarg_include)
/*
Scan and process a #include directive.  If is_include_next is TRUE, the
directive is a #include_next (a gcc extension that begins the search for
the file in the directory on the search path that follows the directory
in which the current file was found).  was_simulated_stdarg_include specifies
whether or not the include was of stdarg.h or cstdarg when using
pass_stdarg_references_to_generated_code.
*/
{
  char      *name_start_pos;
  a_boolean is_system_include;
  a_byte    ifg_state;

  /* The syntax is one of the following (see standard, 3.8.2):

     # include <h-char-sequence> new-line
     # include "q-char-sequence" new-line
     # include pp-tokens new-line

     (where the pp-tokens are macro-expanded to yield one of the
     first two forms.)
  */
  *was_simulated_stdarg_include = FALSE;
  ifg_state = get_ifg_state();
  if (ifg_state < IFG_STATE_FAIL) {
    /* If another include is seen outside of the #ifndef/#endif guard
       code of the current file then it is not a candidate for suppression
       of a subsequent include. */
    set_ifg_state(IFG_STATE_FAIL);
  }  /* if */
  if (is_include_next) {
    /* An include_next is meaningless in a primary source file.  Issue a
       warning and treat this as a normal include. */
    if (processing_primary_source_file()) {
      is_include_next = FALSE;
      warning(ec_include_next_in_primary_source_file);
    }  /* if */
  }  /* if */
  /* Scan a header name token. */
  if (!get_header_name()) {
    /* Missing include file name. */
    catastrophe(ec_exp_file_name);
  } else {
    /* A header name was scanned. */
    a_boolean	is_cstdarg = FALSE;
    is_system_include = *start_of_curr_token == '<';
    /* Check for an include alias where an alternate version of the file name
       should be used. */
    name_start_pos = check_for_include_alias();
    if (name_start_pos == NULL) {
      /* There was no alias.  Get a copy of the name in IL memory.
         Escapes are not processed.  That's an implementation choice; you
         can change this if you'd rather have it the other way.  (But note
         that Microsoft compatibility requires ignoring the escapes, because
         "\" can be used in file names.) */
      name_start_pos = copy_header_name(/*process_escapes=*/FALSE);
    }  /* if */
    /* Move past the header name. */
    (void)get_token();
    /* Ignore trailing junk on the line.  Do this before pushing the new file,
       so the error can be produced on the old line. */
    ignore_harmless_trailing_comment();
    /* Make sure no extra token separators are emitted while generating
       preprocessor (or raw listing) output.  This is necessary to avoid
       turning
         #define M <stdarg.h>
         #include M
       into
         #include<stdarg . h>  */
    no_token_separators_in_this_line_of_pp_output = TRUE;
    if (pass_stdarg_references_to_generated_code &&
        (strcmp(name_start_pos, "stdarg.h") == 0 ||
         (!C_mode() &&
          ((is_cstdarg = (strcmp(name_start_pos, "cstdarg") == 0),
	   is_cstdarg))))) {
      /* Instead or reading the <stdarg.h> or <cstdarg> header file, create
         builtin definitions for the things it's known to define. */
      proc_stdarg_include(is_cstdarg);
      actual_include_was_suppressed = TRUE;
    } else {
      /* Push the name and associated search directory onto the input stack,
         thus starting input from that file.  If the include file cannot be
         opened, we continue processing if we're doing preprocessing only,
         except in GNU mode where we only continue if actually creating
         preprocessed output (e.g., not when creating Makefile
         dependencies). */
      open_file_and_push_input_stack(name_start_pos,
                                     /*use_search_path=*/TRUE,
                                     /*is_include_file=*/TRUE,
                                     is_system_include,
                                     /*is_preinclude=*/FALSE,
			             /*preinclude_macros=*/FALSE,
                                     /*is_implicit_include=*/FALSE,
                                     is_include_next,
				     /*continue_on_open_failure=*/
                                            do_preprocessing_only &&
                                            (!gnu_mode || generate_pp_output),
                                     (a_boolean*)NULL);
    }  /* if */
  }  /* if */
}  /* proc_include */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void proc_import(void)
/*
Scan and process an #import directive.  This is a Microsoft extension.
We do not do the full processing done by the Microsoft compiler.  Rather,
we count on the fact that the Microsoft compiler's processing will create
a header file (with a .tlh suffix) in the object directory, and we can
simply include that.
*/
{
  char   *name;
  a_byte ifg_state;

  /* The syntax is

     # import "name.xxx" ... rest ignored ...
     # import <name.xxx> ... rest ignored ...

  */
  ifg_state = get_ifg_state();
  if (ifg_state < IFG_STATE_FAIL) {
    /* If another include is seen outside of the #ifndef/#endif guard
       code of the current file then it is not a candidate for suppression
       of a subsequent include. */
    set_ifg_state(IFG_STATE_FAIL);
  }  /* if */
  /* Scan a header name token. */
  if (!get_header_name()) {
    /* Missing include file name. */
    catastrophe(ec_exp_file_name);
  } else {
    /* A header name was scanned. */
    /* Allocate space for and copy the name. */
    /* Escapes are not processed.  That is appropriate since "\" is used
       in file names on Microsoft systems. */
    a_text_buffer_ptr	buffer;
    name = copy_header_name(/*process_escapes=*/FALSE);
    /* Move past the header name. */
    (void)get_token();
    /* Ignore the rest of the directives on the line. */
    flush_to_newline();
    /* Make the name of the file to be included.  It is the base name of
       the imported file name, with a .tlh suffix and the directory
       specified by import_dir_name. */
    name = derived_name(name, ".tlh");
    buffer = combine_dir_and_file_name(import_dir_name, name,
                                       (a_text_buffer_ptr)NULL);
    /* Copy the name to a string in the IL memory region.  Note that
       the buffer includes the null terminator. */
    name = (char*)alloc_primary_file_scope_il(buffer->size);
    (void)strcpy(name, buffer->buffer);
    /* Push the name and associated search directory onto the input stack,
       thus starting input from that file. */
    open_file_and_push_input_stack(name,
                                   /*use_search_path=*/FALSE,
                                   /*is_include_file=*/TRUE,
                                   /*is_system_include=*/FALSE,
                                   /*is_preinclude=*/FALSE,
			           /*preinclude_macros=*/FALSE,
                                   /*is_implicit_include=*/FALSE,
                                   /*is_include_next=*/FALSE,
				   /*continue_on_open_failure=*/FALSE,
                                   (a_boolean*)NULL);
  }  /* if */
}  /* proc_import */


static a_cli_metadata_file_ptr make_cli_metadata_file(
                                 char                  *name,
                                 char                  *full_name,
                                 a_boolean             as_friend,
                                 a_boolean             is_system_include,
                                 a_boolean             referenced_by_preusing,
                                 a_source_position_ptr pos)
/*
Create a CLI metadata file entry and initialize the fields using the arguments.
The entry is saved on a list in the il_header and if source sequence lists
are being generated, the directive is added to that list as well.
*/
{
  a_cli_metadata_file_ptr cmfp;

  /* Create and initialize the directive. */
  cmfp = alloc_cli_metadata_file();
  cmfp->name_as_written = name;
  cmfp->full_name = full_name;
  cmfp->position = *pos;
  cmfp->as_friend = as_friend;
  cmfp->referenced_by_preusing = referenced_by_preusing;
  cmfp->referenced_by_system_using = is_system_include;
  /* Append the entry to the list of directives in il_header. */
  if (il_header.cli_metadata_files == NULL) {
    il_header.cli_metadata_files = cmfp;
  } else {
    a_cli_metadata_file_ptr cli_metadata_files_tail =
                                         il_header.cli_metadata_files;
    while (cli_metadata_files_tail->next != NULL) {
      cli_metadata_files_tail = cli_metadata_files_tail->next;
    }  /* while */
    cli_metadata_files_tail->next = cmfp;
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  /* Create a record of this source file, and map a source position to be
     used for all tokens read from this file. */
  record_inclusion_of_assembly_source_file(full_name,
                                           full_name,
                                           name,
                                           &cmfp->assembly_file,
                                           is_system_include,
                                           referenced_by_preusing,
                                           &cmfp->inserted_position);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  add_to_source_sequence_list((char*)cmfp, iek_cli_metadata_file);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  return cmfp;
}  /* make_cli_metadata_file */


static char *search_for_metadata_file(char *file_name)
/*
Attempt to find the given file_name, searching, if necessary, in this order:
  * Current directory
  * .NET system directory (if we haven't seen --no_using_framework_directory)
  * Directories specified from the --using_directory option
  * Directories from the environment variable LIBPATH
If a file is found, the return value is the full path to the file; otherwise,
it is NULL.
*/
{
  a_directory_name_entry_ptr    curr_directory_name_entry;
  a_text_buffer_ptr             buffer = NULL;
  char                          *new_input_file = NULL;

  if (is_absolute_file_name(file_name)) {
    /* Searching isn't necessary since the name is fully specified.  We still
       need to verify the file exists and is a regular file (as opposed to a
       directory, a device, or something else). */
    if (is_regular_file(file_name)) {
      /* Copy the filename from temporary memory to an IL region. */
      new_input_file = alloc_primary_file_scope_il(strlen(file_name) + 1);
      strcpy(new_input_file, file_name);
    }  /* if */
  } else {
    /* The search path is initialized properly before we get here. */
    for (curr_directory_name_entry = assembly_search_path;
         curr_directory_name_entry != NULL;
         curr_directory_name_entry = curr_directory_name_entry->next) {
      /* Join the directory and file name. */
      buffer = combine_dir_and_file_name(curr_directory_name_entry->dir_name,
                                         file_name, NULL);
      /* Check if a file exists in the directory and is not a directory. */
      if (is_regular_file(buffer->buffer)) {
        /* A file has been found, so stop searching.  Copy the filename from
           temporary memory to an IL region because we synthesized the name
           here. */
        new_input_file = alloc_primary_file_scope_il(buffer->size);
        strncpy(new_input_file, buffer->buffer, buffer->size);
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return new_input_file;
}  /* search_for_metadata_file */


static void import_metadata(char                  *name,
                            a_boolean             as_friend,
                            a_boolean             is_system_include,
                            a_boolean             referenced_by_preusing,
                            a_source_position_ptr pos)
/*
Search for the metadata file "name" using the usual search, create a CLI
metadata file entry, and begin the process of importing the types and symbols
in the metadata file.
*/
{
  char                      *full_name;

  full_name = search_for_metadata_file(name);
  if (full_name == NULL) {
    pos_str2_catastrophe(ec_cannot_open_file, error_text(ec_metadata),
                         name, pos);
  } else {
    a_boolean                 is_duplicate = FALSE;
    a_cli_metadata_file_ptr   cmfp;
    a_cpp_cli_import_flag_set import_flags = default_cpp_cli_import_flags;
    if (as_friend) {
      import_flags |= (a_cpp_cli_import_flag_set)cpp_cli_as_friend_assembly;
    }  /* if */
    if (wchar_t_is_keyword) {
      import_flags |= (a_cpp_cli_import_flag_set)cpp_cli_wchar_t_is_keyword;
    }  /* if */
    cmfp = make_cli_metadata_file(name, full_name, as_friend, 
                                   is_system_include, referenced_by_preusing,
                                   pos);
    cmfp->assembly_index = import_metadata_file(cmfp->full_name,
                                                import_flags,
                                                &is_duplicate);
    if (cmfp->assembly_index == 0) {
      /* Failed to import metadata. */
      pos_st_error(ec_cannot_import_metadata, &cmfp->position,
                   cmfp->name_as_written);
    } else if (!is_duplicate) {
      a_boolean         save_fetch_pp_tokens = fetch_pp_tokens;
      a_boolean         save_in_preprocessing_directive
                                             = in_preprocessing_directive;
      an_assembly_index save_assembly_index = curr_assembly_index;
      char              *buffer;

#if DEBUG
      if (db_flag_is_set("dump_metadata") ||
          db_flag_is_set("dump_full_metadata")) {
        fprintf(f_debug, "Importing metadata from '%s' returns %x.\n",
                cmfp->full_name, cmfp->assembly_index);
      }  /* if */
#endif /* DEBUG */
      /* Import the top level declarations. */
      fetch_pp_tokens = FALSE;
      in_preprocessing_directive = FALSE;
      curr_assembly_index = cmfp->assembly_index; 
      buffer = generate_top_level_metadata_code(cmfp->assembly_index);
      scan_top_level_metadata_declarations(buffer, cmfp->assembly_index);
      /* If this is not a preusing, the next token should be tok_newline of 
         the #using directive. */
      check_assertion(curr_token == tok_newline || 
                      cmfp->referenced_by_preusing);
      /* Restore the flags. */
      fetch_pp_tokens = save_fetch_pp_tokens;
      in_preprocessing_directive = save_in_preprocessing_directive;
      curr_assembly_index = save_assembly_index;
    }  /* if */
  }  /* if */
}  /* import_metadata */


void process_preusings(void)
/*
Import mscorlib.dll or the file to be used in place of mscorlib to define
system types.  Then import any other metadata files specified via --preusing.
*/
{
  char *name;
  char *mscorlib;
  char *il_mscorlib;

  /* mscorlib_file_name will be non-NULL if a user-specified file should
     be used in place of mscorlib. */
  if (mscorlib_file_name != NULL) {
    mscorlib = mscorlib_file_name;
  } else {
    mscorlib = "mscorlib.dll";
  }  /* if */    
  il_mscorlib = alloc_il(strlen(mscorlib) + 1);
  strcpy(il_mscorlib, mscorlib);
  import_metadata(il_mscorlib, /*as_friend=*/FALSE, /*is_system_include=*/TRUE,
                  /*referenced_by_preusing=*/TRUE,
                  &preinclude_source_position);
  init_cli_symbols();
  while (preusing_file_list != NULL) {
    name = alloc_il(strlen(preusing_file_list->file_name) + 1);
    strcpy(name, preusing_file_list->file_name);
    import_metadata(name, /*as_friend=*/FALSE, /*is_system_include=*/FALSE,
                    /*referenced_by_preusing=*/TRUE,
                    &preinclude_source_position);
    preusing_file_list = preusing_file_list->next;
  }  /* while */
}  /* process_preusings */


static void proc_using(a_source_position_ptr directive_start_pos)
/*
Scan and process a #using directive.  This directive makes the entities
from the indicated metadata file available to the compilation if they are
referenced.
*/
{
  char                        *name;
  a_boolean                   as_friend         = FALSE;
  a_boolean                   is_system_include = FALSE;

  /* We don't update the include file guard state because the file imported
     is always imported just once. */
  if (generate_pp_output) {
    /* Generating preprocessing output.  Pass the directive to the output. */
    pass_directive_to_output();
  } else if (!get_header_name()) {
    /* Missing include file name. */
    catastrophe(ec_exp_file_name);
  } else {
    /* A header name was scanned. */
    is_system_include = *start_of_curr_token == '<';
    /* Allocate space for and copy the name. */
    /* Escapes are not processed.  That is appropriate since "\" is used
       in file names on Microsoft systems. */
    name = copy_header_name(/*process_escapes=*/FALSE);
    /* Move past the header name. */
    (void)get_token();
    /* Look for the optional "as_friend" qualifier. */
    if (curr_token == tok_identifier && curr_id_is("as_friend")) {
      as_friend = TRUE;
      /* Move past "as_friend". */
      (void)get_token();
    }  /* if */
    /* Ignore trailing comments on the line. */
    ignore_harmless_trailing_comment();
    if (!cppcli_enabled) {
      /* C++/CLI is not enabled.  Skip processing the #using. */
      str_error(ec_cppcli_not_enabled, "#using");
    } else if (depth_scope_stack != DEPTH_OF_FILE_SCOPE) {
      /* #using must occur at file scope. */
      error(ec_using_not_at_file_scope);
    } else {
      import_metadata(name, as_friend, is_system_include, FALSE,
                      directive_start_pos);
    }  /* if */
  }  /* if */
}  /* proc_using */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void proc_line(a_boolean cpp_output_form)
/*
Scan and process a #line directive.  If cpp_output_form is TRUE, the
directive is the variant that is output from cpp (has no keyword "line",
may have extra operand at end).
*/
{
  char          *temp_ptr;
  a_line_number temp_line;
  int           digit;
  char          *temp_file;
  a_boolean     bad_line_number;
  a_boolean	from_system_include = FALSE;

  /* The ULTRIX C compiler has trouble with the type of folded compile-time
     unsigned expressions, so we use a variable for this value. */
  a_line_number max_line_div_10 = MAX_LINE_NUMBER;
  max_line_div_10 /= (a_line_number)10;

  /* Line directives are not allowed in files used to generate
     precompiled headers. */
  suppress_creation_of_pch();

  /* Any number should be scanned as a digit sequence. */
  exp_digit_sequence = TRUE;
  /* The syntax for the standard #line is (see 3.8.4):

     #line digit-sequence
     #line digit-sequence string-literal
     #line pp-tokens              <-- Must turn into one of the other forms.

     The syntax for the cpp-output form is:

     # digit-sequence string-literal kind [kind ...]
     # digit-sequence

     where kind is empty or is a sequence of digits separated by spaces.
     Multiple "kind" values are only generated by the GNU preprocessor.
  */
  if (!cpp_output_form) {
    /* Standard #line; see if the next token appears to be a number. */
    /* Try to expand macros to get one of the normal forms. */
    expand_macros = TRUE;
    /* Scan the line number. */
    if (get_token() != tok_digit_sequence) {
      /* The line number is missing. */
      syntax_error(ec_exp_line_number);
      some_error_in_curr_directive = TRUE;
      goto return_point;
    }  /* if */
  }  /* if */
  /* Now, for either form, the current token is tok_digit_sequence.
     Convert the digit sequence to a line number. */
  temp_line = 0;
  bad_line_number = FALSE;
  for (temp_ptr = start_of_curr_token; temp_ptr <= end_of_curr_token;
       temp_ptr++) {
    if (temp_line > max_line_div_10) {
      bad_line_number = TRUE;
      break;
    }  /* if */
    temp_line *= 10;
    digit = *temp_ptr - '0';
    if (temp_line > MAX_LINE_NUMBER-(unsigned long)digit) {
      bad_line_number = TRUE;
      break;
    }  /* if */
    temp_line += digit;
  }  /* for */
  /* Zero is not allowed as a line number (it's not allowed by ANSI,
     and it looks like end of file to the error routines).  gcc 4.1
     introduced a bug that caused zero to be emitted in line directives
     for certain built-in declarations.  Accept zero in a cpp-form line
     directive, but use the value one in its place. */
  if (temp_line == 0) {
    if (cpp_output_form) {
      temp_line = 1;
    } else {
      bad_line_number = TRUE;
    }  /* if */
  }  /* if */
  if (bad_line_number) {
    /* The line number was incorrect or too large.  Note that scanning of the
       directive continues, because this isn't a syntax error; we haven't
       lost our place.  Below, the updating of the position information is
       suppressed. */
    error(ec_bad_line_number);
  }  /* if */
  /* After the line number, there may be a file name as a string literal. */
  /* Note that since in_pp_if_expression is FALSE, the constant will
     be returned in character form rather than converted. */
  if (get_token() == tok_newline) {
    /* The optional file name is missing, so keep the same name. */
    temp_file = curr_ise->file_name;
    if (curr_ise->assoc_il_file->from_system_include_dir) {
      /* If this is a subsequent line directive for a file that had the system
         header flag on a prior entry, still treat it as a system header. */
      from_system_include = TRUE;
    }  /* if */
  } else if (curr_token == tok_string_literal && *start_of_curr_token != 'L') {
    /* Check for "L" is to disallow wide string literals. */
    /* The file name is present.  Since the constant is unconverted, allocate
       space for the string, and copy it.  Note that the string can be
       empty, which is not really a problem.  Also note that unlike
       in #include, escape characters in the string must be honored
       (see the ISO C standard -- it uses "s-char-sequence" for #line
       and "h-char-sequence" for #include). */
    temp_file = copy_header_name(/*process_escapes=*/!pcc_preprocessing_mode);
    /* Move past the string literal. */
    (void)get_token();
  } else {
    /* Error, expected file name. */
    syntax_error(ec_exp_file_name);
    some_error_in_curr_directive = TRUE;
    goto return_point;
  }  /* if */
  /* For the cpp-output form, ignore the trailing flag operands if they are
     present.  Multiple flags separated by spaces may be present.  The
     flag values are "1" for entry into an include file, "2" for the first
     directive after exit from an include file, "3" for text from a system
     include, and "4" for text that should be treated as inside an extern
     "C" block (but, as mentioned above, the values are ignored).  Multiple
     values, and values other than "1" and "2" are only generated by the
     GNU preprocessor. */
  if (cpp_output_form) {
    while (curr_token == tok_digit_sequence) {
      if (*start_of_curr_token == '3' && len_of_curr_token == 1) {
        from_system_include = TRUE;
      }  /* if */
      (void)get_token();
    }  /* while */
  }  /* if */
  /* If there was an error in the line number, do not update the position
     information. */
  if (bad_line_number) goto return_point;
  /* Install the new file name and line number, as a source-file entry;
     this entry is not for a file, but instead represents the remapping
     caused by the #line. */
  /* Note that full_name stays pointing to the actual input file name. */
  curr_ise->file_name = temp_file;
  curr_ise->line_number = temp_line - 1;  /* Number will be incremented. */
  /* If there is already an active #line, record the end of its range. */
  if (curr_ise->assoc_il_file != curr_ise->assoc_actual_il_file) {
    record_end_of_source_file(curr_ise->assoc_il_file, seq_number_last_read);
  }  /* if */
  /* The new entry is entered under the current file entry, whether that
     entry is for the primary source file, an include file, or a #line
     directive.  curr_ise->assoc_il_file points to the new entry.
     Note that curr_ise->assoc_actual_il_file is NOT changed; it remains
     pointing to the entry for the actual file being read. */
  {
    a_source_file_ptr	actual_sfp = curr_ise->assoc_actual_il_file;
    record_start_of_source_file(curr_ise->assoc_actual_il_file,
                                (a_seq_number)seq_number_last_read+1,
                                temp_line,
                                temp_file,
                                (char *)NULL,  /* Indicates #line entry. */
                                (char *)NULL,  /* Indicates #line entry. */
                                &(curr_ise->assoc_il_file),
                                (a_boolean)actual_sfp->is_include_file,
                                (a_boolean)actual_sfp->
                                                   included_by_system_include,
                                (a_boolean)actual_sfp->included_by_preinclude,
				(a_boolean)actual_sfp->preinclude_macros_only,
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
				(a_boolean)actual_sfp->is_implicit_include,
#else /* !INSTANTIATION_BY_IMPLICIT_INCLUSION */
                                (a_boolean)FALSE,
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
                                (actual_sfp->from_system_include_dir ||
                                 from_system_include),
#if MICROSOFT_EXTENSIONS_ALLOWED
				(a_boolean)actual_sfp->is_assembly_file
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
                                (a_boolean)FALSE
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                                );
  }
  if (generate_pp_output) {
    /* Generate the line-identifying directive if necessary for preprocessing
       output.  Force out because cpp always puts one out. */
    gen_pp_line_info(' ', /*next_line=*/TRUE);
  }  /* if */
  /* If raw listing information is being generated (for input to a program
     that will generate an interspersed listing), generate line information
     for it. */
  if (f_raw_listing != NULL) {
    gen_rlisting_line_info(' ');
  }  /* if */
return_point:
  exp_digit_sequence = FALSE;
  return;
}  /* proc_line */


static void proc_error(void)
/*
Scan and process a #error directive.  Terminates the compilation,
does not return.
*/
{
  /* Produce an error message including the rest of the #error directive. */
  skip_white_space();
  str_catastrophe(ec_error_directive, curr_char_loc);
}  /* proc_error */


static void proc_warning(void)
/*
Scan and process a #warning directive.  Similar to proc_error, but only a
warning is issued.
*/
{
  /* Produce a warning including the rest of the #warning directive. */
  skip_white_space();
  str_warning(ec_warning_directive, curr_char_loc);
  /* Skip over the warning text. */
  flush_to_newline();
}  /* proc_warning */


static void pass_directive_to_output(void)
/*
Scan a directive, passing it textually to the pp output file for
processing by some compiler.  This routine should only be called if
generate_pp_output is TRUE.
*/
{
#if CHECKING
  if (!generate_pp_output) {
    internal_error("pass_directive_to_output: generate_pp_output FALSE");
  }  /* if */
#endif /* CHECKING */
  /* Set flags to cause the directive's text to be passed to the preprocessing
     output file. */
  do_not_put_curr_line_in_pp_output = FALSE;
  pass_pp_directive_to_output = TRUE;
  /* Skip over the contents of the directive. */
  flush_to_newline();
  pass_pp_directive_to_output = FALSE;
}  /* pass_directive_to_output */


/*
Dynamically allocated buffer used to contain preprocessing directives
that are being recorded as character strings.
*/
static char	*pp_dir_string_buffer;
			/* Not allocated on a per-file basis. */

#define PP_DIR_STRING_BUFFER_INCREMENTAL_ALLOCATION 300
			/* Incremental (and also initial) allocation size for
                           pp_dir_string_buffer.  The initial allocation
			   should be such that almost all cases can be
			   accepted (so that the realloc is hardly ever
			   needed). */


static void expand_pp_dir_string_buffer(sizeof_t size_needed)
/*
Expand the pp_dir_string_buffer by reallocating it, so that its total
size is at least size_needed.  Called by ensure_pp_dir_string_buffer_space.
*/
{
  sizeof_t new_size;

  new_size = size_pp_dir_string_buffer +
             PP_DIR_STRING_BUFFER_INCREMENTAL_ALLOCATION;
  if (new_size < size_needed) new_size  = size_needed;
  pp_dir_string_buffer = realloc_buffer(pp_dir_string_buffer,
                                        size_pp_dir_string_buffer, new_size);
  size_pp_dir_string_buffer = new_size;
}  /* expand_pp_dir_string_buffer */


/*
Ensure that pp_dir_string_buffer has at least size_needed bytes in it.
If not, expand pp_dir_string_buffer by reallocating it.
*/
#define ensure_pp_dir_string_buffer_space(size_needed)                 \
{ if (size_pp_dir_string_buffer < (size_needed)) {                     \
    expand_pp_dir_string_buffer((sizeof_t)(size_needed));              \
  }  /* if */                                                          \
}  /* ensure_pp_dir_string_buffer_space */


static void convert_pp_directive_to_string(
					a_boolean is_microsoft_pragma_operator)
/*
Scans the tokens that make up a preprocessing directive and converts them into
a single null terminated character string.  The string is constructed in
a dedicated buffer which is enlarged as needed to be able to contain
the entire string.  The tokens are scanned as preprocessing tokens.  If
is_microsoft_pragma_operator is TRUE, scan to the closing parenthesis.
*/
{
  a_boolean	any_white_space_skipped = FALSE;
  sizeof_t	pos_in_buffer = 0;
  int		paren_count = 0;

  db_enter(4, "convert_pp_directive_to_string");
  while (curr_token != tok_newline && curr_token != tok_end_of_source &&
         (!is_microsoft_pragma_operator ||
          (curr_token != tok_rparen || paren_count != 0))) {
    if (curr_token == tok_lparen) {
      paren_count++;
    } else if (curr_token == tok_rparen) {
      if (paren_count > 0) paren_count--;
    }  /* if */
    /* The +1 in the following call is to make sure there is space for
       a null terminator to be added. */
    ensure_pp_dir_string_buffer_space(pos_in_buffer + len_of_curr_token +
                                      any_white_space_skipped + 1);
    if (any_white_space_skipped) pp_dir_string_buffer[pos_in_buffer++] = ' ';
    (void)memcpy(&pp_dir_string_buffer[pos_in_buffer], start_of_curr_token,
                 size_t_arg(len_of_curr_token));
    pos_in_buffer += len_of_curr_token;
    /* Skip any white space and record whether any was skipped.  Any white
       space will be replaced by a single blank. */
    skip_white_space();
    any_white_space_skipped = (kind_of_white_space_skipped != 0);
    (void)get_token();
  }  /* while */
  /* Add a null terminator. */
  ensure_pp_dir_string_buffer_space(pos_in_buffer + 1);
  pp_dir_string_buffer[pos_in_buffer] = '\0';
  db_exit();
}  /* convert_pp_directive_to_string */


static void convert_pragma_to_string(a_pending_pragma_ptr          ppp)
/*
A token cache has been created to store the tokens for the pragma.
Convert the token cache into a string representation of the pragma.
The string is constructed into the temp_text_buffer.  Once the entire
string has been created, a buffer of the appropriate size is allocated
in the file scope IL memory region and the pragma string is copied
there.
*/
{
  db_enter(4, "convert_pragma_to_string");
  /* Initialize the token string.  Use the pragma ID position as the start
     position associated with the token string. */
  init_token_string(&ppp->id_position, /*keep_spacing=*/FALSE,
                    /*suppress_identifier_wrapping=*/FALSE);
  add_token_cache_to_string(&ppp->token_cache);
  /* Copy the string to IL memory. */
  ppp->pragma_text = make_copy_of_token_string();
#if DEBUG
  if (debug_level >= 5 || db_flag_is_set("pragma_string")) {
    fprintf(f_debug, "Saved pragma string: '%s'\n", ppp->pragma_text);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* convert_pragma_to_string */


static void cache_pragma_tokens(
		a_pending_pragma_ptr		ppp,
		a_pragma_kind_description_ptr	pkdp,
		a_boolean			is_microsoft_pragma_operator)
/*
Cache the tokens that make up a pragma directive.  The global variables
that determine the current lexical scanning mode are saved and reset
based on the information specified in the pragma description entry.
is_microsoft_pragma_operator is TRUE when the pragma being scanned is a
Microsoft __pragma operator.*/
{
  a_boolean	save_expand_macros;
  a_boolean	save_caching_pragma_tokens;
  a_boolean	save_do_string_literal_concatenation;
  a_boolean	save_fetch_pp_tokens;
  a_boolean     save_recognize_keywords_in_pragma;
  a_boolean	save_in_preprocessing_directive;

  /* Cache the pragma identifier. */
  cache_curr_token(&ppp->token_cache);
  /* Save the current value of the lexical scanning mode flags. */
  save_expand_macros = expand_macros;
  save_caching_pragma_tokens = caching_pragma_tokens;
  save_do_string_literal_concatenation = do_string_literal_concatenation;
  save_fetch_pp_tokens = fetch_pp_tokens;
  save_recognize_keywords_in_pragma = recognize_keywords_in_pragma;
  save_in_preprocessing_directive = in_preprocessing_directive;
  /* We need to set in_preprocessing_directive in case this is called to
     process a _Pragma operator. */
  in_preprocessing_directive = TRUE;
  /* Set the new values. */
  expand_macros = pkdp->expand_macros;
  caching_pragma_tokens = TRUE;
  recognize_keywords_in_pragma = pkdp->processing_C_code;
  do_string_literal_concatenation = pkdp->processing_C_code;
  fetch_pp_tokens = pkdp->fetch_pp_tokens;
  /* Bypass the identifier that indicates the pragma kind. */
  (void)get_token();
  if (is_microsoft_pragma_operator) {
    /* A Microsoft __pragma operator.  Cache the tokens until a right
       parenthesis is found. */
    a_token_set_array  stop_tokens;
    /* Initialize a local stop token set. */
    clear_token_set_array(stop_tokens);
    incr_token_set_array_element(stop_tokens, tok_newline);
    incr_token_set_array_element(stop_tokens, tok_end_of_source);
    incr_token_set_array_element(stop_tokens, tok_rparen);
    cache_token_stream(&ppp->token_cache, stop_tokens);
  } else {
    /* A normal #pragma or C99-style _Pragma.  Cache the tokens until an
       end-of-line is found. */
    for (;;) {
      if (curr_token == tok_newline || curr_token == tok_end_of_source) break;
      cache_curr_token(&ppp->token_cache);
      (void)get_token();
    }  /* for */
  }  /* if */
  /* Terminate the token cache. */
  terminate_token_cache(&ppp->token_cache);
  /* Restore the previous values. */
  expand_macros = save_expand_macros;
  caching_pragma_tokens = save_caching_pragma_tokens;
  do_string_literal_concatenation = save_do_string_literal_concatenation;
  fetch_pp_tokens = save_fetch_pp_tokens;
  recognize_keywords_in_pragma = save_recognize_keywords_in_pragma;
  in_preprocessing_directive = save_in_preprocessing_directive;
}  /* cache_pragma_tokens */


static void convert_pp_token_pragma_to_string(
		a_pragma_kind_description_ptr	pkdp,
		a_boolean			is_microsoft_pragma_operator)
/*
Convert the current pragma directive to a string.  This routine is
used for pragmas that are scanned as pp-tokens.  The pragma has been
determined to be of a kind associated with the entry pointed to pkdp.
is_microsoft_pragma_operator is TRUE when the pragma being scanned is
a Microsoft __pragma operator.

The pragma is converted by convert_pp_directive_to_string and the resulting
string will be in the pp_dir_string_buffer.
*/
{
  a_boolean	save_expand_macros;

  save_expand_macros = expand_macros;
  expand_macros = pkdp->expand_macros;
  convert_pp_directive_to_string(is_microsoft_pragma_operator);
  expand_macros = save_expand_macros;
}  /*  convert_pp_token_pragma_to_string */


static void enter_pending_pragma(
		a_pragma_kind_description_ptr	pkdp,
		a_source_position		*directive_pos,
		a_source_position		*id_pos,
		a_boolean			is_microsoft_pragma_operator)
/*
Scan the current pragma directive, which has already been determined to be
of a kind associated with the entry pointed to pkdp.  It may be recorded as
either a token cache or as a character string.  *directive_pos is the source
position of the start of the directive; *id_pos is the source position of
the pragma identifier.  is_microsoft_pragma_operator is TRUE when the pragma
being scanned is a Microsoft __pragma operator.
*/
{
  a_pending_pragma_ptr	ppp;

  ppp = alloc_pending_pragma(pkdp);
  ppp->id_position = *id_pos;
  ppp->pragma_position = *directive_pos;
  ppp->is_microsoft_pragma_operator = is_microsoft_pragma_operator;
  if (pkdp->fetch_pp_tokens) {
    /* When fetching pp-tokens, convert the tokens to a string in
       such a way that no additional white space is added. */
    if (pkdp->binding_kind == pbk_preproc_immediate &&
        (!pkdp->record_pragma_text || !pkdp->automatically_include_in_il)) {
      /* A pragma string is not created for preprocessing immediate pragmas
         that are not automatically included in the IL.  This is done for
         backward compatibility purposes so that the pragma processing
         routines for such pragmas can scan the tokens themselves rather
         than having only the string representation. */
    } else {
      convert_pp_token_pragma_to_string(pkdp, is_microsoft_pragma_operator);
      ppp->pragma_text = copy_string_to_region(file_scope_region_number,
                                               pp_dir_string_buffer);
#if DEBUG
      if (db_flag_is_set("pragma_string")) {
        fprintf(f_debug, "pp-token pragma string: '%s'\n", ppp->pragma_text);
      }  /* if */
#endif /* DEBUG */
    }  /* if */
  } else {
    /* Cache the tokens that make up the pragma directive. */
    if (pkdp->read_string_as_header_name) {
      /* The pragma may have a string in which escape sequences are to be
         ignored (e.g., a Windows-style path name with backslash as the
         directory separator).  Scan such strings as header names. */
      exp_header_name = TRUE;
      cache_pragma_tokens(ppp, pkdp, is_microsoft_pragma_operator);
      exp_header_name = FALSE;
    } else {
      cache_pragma_tokens(ppp, pkdp, is_microsoft_pragma_operator);
    }  /* if */
    if (pkdp->record_pragma_text) {
      /*  The character string representation is usually used for pragmas that
          are to be passed to the C or C++ generating back end, but may be
          used for other pragmas in which a character string is simpler to
          manipulate. */
      convert_pragma_to_string(ppp);
    }  /* if */
    /* Remove the initial token from the token cache.  For historical
       reasons, the cache does not include the pragma identifier, but
       it must be cached initially so that it can be included in the
       pragma string when making text, not tokens. */
    remove_token_from_cache(ppp->token_cache.first_token,
                            &ppp->token_cache.first_token,
                            &ppp->token_cache);
  }  /* if */
  if (pkdp->binding_kind == pbk_preproc_immediate) {
    /* Process a "preprocessing immediate" pragma.  Such pragmas are
       processed when they are encountered instead of being associated
       with a token or construct.  Since source sequence lists don't represent
       entities as fine-grained as tokens or preprocessing tokens, the
       representation of these pragmas may not be entirely precise. */
    a_preproc_immediate_pragma_function_ptr pipfp;
    pipfp = (a_preproc_immediate_pragma_function_ptr)index_to_function_pointer(
                                              pkdp->processing_function_index);
    if (pipfp != NULL) (*pipfp)(ppp);
    if (pkdp->automatically_include_in_il) {
#if GENERATE_SOURCE_SEQUENCE_LISTS
      ppp->source_sequence_entry = add_empty_source_sequence_entry();
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      add_pragma_to_il(ppp, (an_il_entry_kind)iek_none, (char*)NULL,
                       /*is_global=*/TRUE);
    }  /* if */
    free_pending_pragma(ppp);
  } else {
    /* Add this pragma to the list of pragmas associated with the
       current token. */
    add_to_curr_token_pragma_list(ppp);
  }  /* if */
}  /* enter_pending_pragma */


/*ARGSUSED*/ /* <-- ppp is not used. */
void once_pragma(a_pending_pragma_ptr	ppp)
/*
Process a "#pragma once" directive.  This directive indicate that
this file should be included only once, and if it is #included
again in the same compilation unit, the include should be skipped.
Record this information in the input stack entry.
*/
{
  if (curr_ise->is_include_file) {
    /* No processing is needed for a #once pragma in a primary file. */
    set_ifg_state(IFG_STATE_ONCE);
    curr_ise->include_history->pragma_once = TRUE;
  }  /* if */
  /* Bypass the "once" token. */
  (void)get_token();
}  /* once_pragma */


/*ARGSUSED*/ /* <-- ppp is not used. */
void hdrstop_or_no_pch_pragma(a_pending_pragma_ptr ppp)
/*
A PCH control pragma.  The actual processing of these
pragmas is handled in the special prefix processing code for
preprocessing directives.  When they are encountered during a
real compilation, they should just be ignored.
*/
{
  while (curr_token != tok_newline && curr_token != tok_end_of_source) {
    (void)get_token();
  }  /* while */
}  /* hdrstop_or_no_pch_pragma */

#if MICROSOFT_EXTENSIONS_ALLOWED

static char *get_raw_header_name(a_boolean	issue_error)
/*
Use get_header_name to scan a header name, and return a copy the raw
characters of the header name.  If the next token is not a header name,
optionally issue a diagnostic (if issue_error is TRUE) and return NULL.
The returned value includes the delimiters of the header name.
*/
{
  a_text_buffer_ptr	buf = header_name_buffer;
  char			*result = NULL;

  /* Scan the header name. */
  if (get_header_name()) {
    /* Get the raw version of the header name that was just scanned. */
    reset_text_buffer(buf);
    (void)add_to_text_buffer(buf, start_of_curr_token, len_of_curr_token);
    add_char_to_text_buffer(buf, '\0');
    /* Allocate memory for the name.  Note that the buffer size includes the
       null terminator. */
    result = alloc_primary_file_scope_il(buf->size);
    (void)strcpy(result, buf->buffer);
  } else {
    if (issue_error) {
      pos_warning(ec_exp_file_name, &pos_curr_token);
    }  /* if */
  }  /* if */
  return result;
}  /* get_raw_header_name */


static void create_include_alias_entry(char	*long_name,
				       char	*short_name)
/*
Create an entry in the include alias table to map uses of long_name to
short_name.
*/
{
  if (include_alias_hash_table == NULL) {
    /* Allocate a hash table for include aliases when the first one is
       encountered. */
    include_alias_hash_table = alloc_hash_table(NO_MEMORY_REGION_NUMBER,
                                       (a_hash_table_size)128,
                                       fn_for_function(hash_include_alias),
                                       fn_for_function(compare_include_alias));
  }  /* if */
#if DEBUG
  if (db_flag_is_set("include_alias")) {
    fprintf(f_debug, "Creating include alias for %s to %s\n", long_name,
            short_name);
  }  /* if */
#endif /* DEBUG */
  /* Create an entry for long_name and short_name.  If an entry already
     exists for long_name, it will be updated to refer to the (possibly new)
     short_name. */
  (void)find_or_create_include_alias(long_name, short_name, /*create=*/TRUE);
}  /* create_include_alias_entry */


/*ARGSUSED*/ /* <-- ppp is not used. */
void microsoft_include_alias_pragma(a_pending_pragma_ptr ppp)
/*
Process a Microsoft include alias pragma.  The form of the pragma is:

  #pragma include_alias("long_filename", "short_filename")
  #pragma include_alias(<long_filename>, <short_filename>)

After the pragma has been encountered, an include of the long file name
will instead include the short file name.  The name specified in the include
directive must match the long file name exactly (including use of '"' vs.
'<', and the exact characters between the delimiters).
*/
{
  a_boolean	any_errors = FALSE;
  char		*long_name = NULL;
  char		*short_name = NULL;

  /* Bypass the "include_alias" token. */
  (void)get_token();
  /* Scan the "(". */
  if (curr_token == tok_lparen) {
    /* We don't use get_token here because get_header_name requires that
       the characters of the header name not be processed by get_token. */
  } else {
    pos_warning(ec_exp_lparen, &pos_curr_token);
    any_errors = TRUE;
  }  /* if */
  /* Get the long file name. */
  long_name = get_raw_header_name(!any_errors);
  if (long_name == NULL) any_errors = TRUE;
  /* Advance past the header name and look for a comma. */
  if (!any_errors && get_token() == tok_comma) {
    /* We don't use get_token here because get_header_name requires that
       the characters of the header name not be processed by get_token. */
  } else {
    if (!any_errors) {
      pos_warning(ec_exp_comma, &pos_curr_token);
      any_errors = TRUE;
    }  /* if */
  }  /* if */
  /* Get the short file name. */
  if (!any_errors && get_header_name()) {
    /* Make sure the include delimiter is the same for both file names. */
    if (*start_of_curr_token != *long_name) {
      if (!any_errors) {
        pos_warning(ec_include_kind_mismatch, &pos_curr_token);
        any_errors = TRUE;
      }  /* if */
    } else {
      /* Escapes are not processed.  That is appropriate since "\" is used
         in file names on Microsoft systems. */
      short_name = copy_header_name(/*process_escapes=*/FALSE);
    }  /* if */
  } else {
    if (!any_errors) {
      pos_warning(ec_exp_file_name, &pos_curr_token);
      any_errors = TRUE;
    }  /* if */
  }  /* if */
  /* Advance past the header name and look for a closing parenthesis. */
  if (!any_errors && get_token() == tok_rparen) {
    (void)get_token();
  } else {
    if (!any_errors) {
      pos_warning(ec_exp_rparen, &pos_curr_token);
      any_errors = TRUE;
    }  /* if */
  }  /* if */
  if (!any_errors) {
    /* Create an entry in the include alias table. */
    create_include_alias_entry(long_name, short_name);
  } else {
    some_error_in_curr_directive = TRUE;
  }  /* if */
}  /* microsoft_include_alias_pragma */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void pass_pragma_to_output(a_pragma_kind_description_ptr pkdp)
/*
Output a pragma directive to the preprocessed output file.  Turn on
expansion of macros if necessary for this kind of pragma.
*/
{
  a_boolean	save_expand_macros;
  a_boolean	save_do_string_literal_concatenation;
  a_boolean	save_fetch_pp_tokens;
  a_boolean     save_recognize_keywords_in_pragma;

  if (pkdp != NULL) {
    /* Save the current value of the lexical scanning mode flags. */
    save_expand_macros = expand_macros;
    save_do_string_literal_concatenation = do_string_literal_concatenation;
    save_fetch_pp_tokens = fetch_pp_tokens;
    save_recognize_keywords_in_pragma = recognize_keywords_in_pragma;
    /* Set the new values. */
    expand_macros = pkdp->expand_macros;
    recognize_keywords_in_pragma = pkdp->processing_C_code;
    do_string_literal_concatenation = pkdp->processing_C_code;
    fetch_pp_tokens = pkdp->fetch_pp_tokens;
  }  /* if */
  /* Fetch the remaining tokens of this directive and output it to the
     preprocessed output file. */
  pass_directive_to_output();
  if (pkdp != NULL) {
    /* Restore the previous values. */
    expand_macros = save_expand_macros;
    do_string_literal_concatenation = save_do_string_literal_concatenation;
    fetch_pp_tokens = save_fetch_pp_tokens;
    recognize_keywords_in_pragma = save_recognize_keywords_in_pragma;
  }  /* if */
}  /* pass_pragma_to_output */


void record_pragma(a_pragma_kind_description_ptr pkdp,
		   a_source_position		 *start_of_dir_position,
		   a_source_position		 *id_position,
		   a_boolean			 is_microsoft_pragma_operator)
/*
Record the pragma whose kind is specified by pkdp (which may be NULL).
start_of_dir_position is the position of the first character of the
pragma directive.  id_position is the position of the pragma identifier.
is_microsoft_pragma_operator is TRUE when the pragma being scanned is
a Microsoft __pragma operator.  When the front end is doing preprocessing
only, only preprocessing immediate pragmas are actually processed.
*/
{
  a_boolean processed = FALSE;
  a_boolean suppress_diagnostic = FALSE;

  if (do_preprocessing_only &&
      (pkdp == NULL || pkdp->binding_kind != pbk_preproc_immediate)) {
    /* Pragmas other than preprocessing immediate pragmas are ignored
       when doing preprocessing only. */
    suppress_diagnostic = TRUE;
  } else if (pkdp != NULL) {
    /* Scan the pragma directive, recording it as either a token cache
       or as a character string. */
    enter_pending_pragma(pkdp, start_of_dir_position, id_position,
                         is_microsoft_pragma_operator);
    processed = TRUE;
  }  /* if */
  if (!processed) {
    /* Unrecognized pragma, just ignore (this is required by the
       standard). */
    if (!suppress_diagnostic) pos_warning(ec_unrecognized_pragma, id_position);
    if (is_microsoft_pragma_operator) {
      flush_to_closing_paren();
    } else {
      flush_to_newline();
    }  /* if */
  }  /* if */
}  /* record_pragma */


a_pragma_kind_description_ptr look_up_pragma_id(
					a_source_position	*id_position)
/*
The current token is expected to be the identifier of a pragma.  Look
up the identifier and return the associated pragma kind description
pointer.  If the token is not an identifier, return NULL.  If the
identifier is not found, return NULL unless unrecognized pragmas are
included in the IL, in which case the unknown pragma kind is returned.
The position of the pragma ID is returned in id_position;
*/
{
  a_pragma_kind_description_ptr	pkdp = NULL;

  (void)get_token();
  /* Save the position of the start of the token(s) that identify
     the kind of pragma being processed.  Save this position even if the
     identifier is missing -- it is still used if we include unrecognized
     pragmas in the IL. */
  *id_position = pos_curr_token;
  /* Identify the pragma that is being processed. */
  if (curr_token == tok_identifier) {
    /* The identifier __VA_ARGS__ is not allowed if variadic macros are
       accepted. */
    check_use_of_VA_ARGS(len_of_curr_token, start_of_curr_token);
    /* Look for a matching pragma identifier in the pragma descriptions
       list.  If any pragma need to be added in where the pragma is
       not specified by an identifier following the #pragma keyword,
       this code will need to be modified. */
    pkdp = pragma_kind_descriptions;
    while (pkdp != NULL) {
      if (curr_id_matches_pragma_id(pkdp->kind)) break;
      pkdp = pkdp->next;
    }  /* while */
#if INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL
    /* If no matching pragma name was found, set the pragma kind to
       pk_unrecognized and scan the pragma according to the associated
       description. */
    if (pkdp == NULL) {
      pkdp = pragma_description_for_pragma_kind[(int)pk_unrecognized];
    }  /* if */
#endif /* INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL */
  }  /* if */
  return pkdp;
}  /* look_up_pragma_id */


static void proc_pragma(a_source_position *start_of_dir_position)
/*
Scan and process a #pragma directive.
*/
{
  a_pragma_kind_description_ptr	pkdp = NULL;
  a_source_position		id_position;
  a_boolean			pass_to_output = generate_pp_output;

  /* Look up the identifier that specifies the kind of pragma. */
  pkdp = look_up_pragma_id(&id_position);
  if (generate_pp_output && do_preprocessing_only) {
    /* Generating preprocessing output for some other compiler.  In most cases
       the #pragma is passed to the output.  The information in the pragma
       description is used to determine how the tokens of the pragma should
       be processed (e.g., should macros be expanded). */
    /* Look for pragmas that must be handled during preprocessing. */
    if (pkdp != NULL) {
      if (pkdp->kind == (a_pragma_kind)pk_once) {
        /* This file should be included only once, and if it is #included
           again in the same compilation unit, the include should be skipped.
           Record this information in the input stack entry. */
        /* Turn on the flags to copy the pragma to the output so that the
           first line will appear, in case the line ends inside a
           comment. */
        do_not_put_curr_line_in_pp_output = FALSE;
        pass_pp_directive_to_output = TRUE;
        once_pragma((a_pending_pragma_ptr)NULL);
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (pkdp->kind == (a_pragma_kind)pk_push_macro) {
        push_macro_pragma((a_pending_pragma_ptr)NULL);
        pass_to_output = FALSE;
      } else if (pkdp->kind == (a_pragma_kind)pk_pop_macro) {
        pop_macro_pragma((a_pending_pragma_ptr)NULL);
        pass_to_output = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      }  /* if */
    }  /* if */
    if (pass_to_output) pass_pragma_to_output(pkdp);
  } else {
    /* Compiling.  Record the pragma for later processing, or for
       processing now in the case of immediate pragmas. */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (pkdp != NULL &&
        (pkdp->kind == (a_pragma_kind)pk_push_macro ||
         pkdp->kind == (a_pragma_kind)pk_pop_macro)) {
      /* These pragmas are useless in the preprocessor output because
         there are no macro definitions or invocations. */
      pass_to_output = FALSE;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (pass_to_output) {
      /* Make sure that the first line of the pragma is output in case
         the line ends inside a comment. */
      do_not_put_curr_line_in_pp_output = FALSE;
      pass_pp_directive_to_output = TRUE;
    }  /* if */
    record_pragma(pkdp, start_of_dir_position, &id_position,
                  /*is_microsoft_pragma_operator=*/FALSE);
    if (pass_to_output) {
      /* If we are generating preprocessed output, but we are also
         doing real compilation (i.e., do_preprocessing_only is FALSE),
         then we need to output the directive as well as actually evaluating
         the pragma. */
      pass_directive_to_output();
    }  /* if */
  }  /* if */
}  /* proc_pragma */


#if IDENT_DIRECTIVE_AND_PRAGMA
static void proc_ident(a_source_position  *directive_pos)
/*
Scan and process a #ident directive.
*/
{
  if (generate_pp_output) {
    /* Generating preprocessing output for some other compiler.  Pass the
       directive unchanged to output. */
    pass_directive_to_output();
  } else {
    /* #ident "xxx" is treated as another spelling of #pragma ident "xxx",
       so put out a pending-pragma entry for it. */
    enter_pending_pragma(pragma_description_for_pragma_kind[(int)pk_ident],
                         directive_pos, &pos_curr_token,
                         /*is_microsoft_pragma_operator=*/FALSE);
  }  /* if */
}  /* proc_ident */


void ident_pragma(a_pending_pragma_ptr ppp)
/*
Process a cached #pragma ident directive.  The syntax is:

  #pragma ident <string>

where <string> is a quoted character string (not wide chars).
*/
{
  a_boolean               err = FALSE;
  a_constant_ptr          cp;
  a_memory_region_number  region_to_switch_back_to;

  begin_rescan_of_pragma_tokens(ppp);
  if (curr_token == tok_string_literal &&
      is_error_constant(&const_for_curr_token)) {
    /* A diagnostic was already issued. */
    err = TRUE;
  } else if (curr_token != tok_string_literal ||
             !is_normal_character_kind(const_for_curr_token.character_kind)) {
    error(ec_bad_ident_string);
    err = TRUE;
  } else {
    switch_to_file_scope_region(&region_to_switch_back_to);
    cp = alloc_unshared_constant(&const_for_curr_token);
    switch_back_to_original_region(region_to_switch_back_to);
    (void)get_token();
  }  /* if */
  if (!err && curr_token != tok_end_of_source) {
    /* The GNU compiler accepts extra text following the ident string with
       just a warning.  Display a warning and then flush to the end of the
       token cache, but don't set err, so that the pragma will be entered
       into the IL. */
    warning(ec_extra_text_in_pp_directive);
    wrapup_rescan_of_pragma_tokens(/*error_in_pragma=*/TRUE);
  } else {
    wrapup_rescan_of_pragma_tokens(err);
  }  /* if */
  if (!err) {
    create_il_entry_for_pragma(ppp, (a_symbol_ptr)NULL, (a_statement_ptr)NULL);
    if (ppp->il_pragma_entry != NULL) {
      ppp->il_pragma_entry->variant.ident_string = cp;
    }  /* if */
  }  /* if */
}  /* ident_pragma */
#endif /* IDENT_DIRECTIVE_AND_PRAGMA */


static void process_stdc_pragma(a_pending_pragma_ptr	ppp)
/*
Process a predefined C99 STDC pragma.  These pragmas have the following form:

  #pragma STDC FP_CONTRACT [ ON | OFF | DEFAULT ]
  #pragma STDC FENV_ACCESS [ ON | OFF | DEFAULT ]
  #pragma STDC CX_LIMITED_RANGE [ ON | OFF | DEFAULT ]

In addition, this routine also handles pragmas controlling the behavior of
certain fixed-point operations as defined by ISO TR 18037.  Since support
for fixed-point types can be enabled in some non-C99 modes, this routine
may be called in non-C99 mode.  The fixed-point pragmas have the form:

  #pragma STDC FX_FULL_PRECISION [ ON | OFF | DEFAULT ]
  #pragma STDC FX_FRACT_OVERFLOW [ SAT | DEFAULT ]
  #pragma STDC FX_ACCUM_OVERFLOW [ SAT | DEFAULT ]

This routine is called to process the pragmas when they are known to appear
in a valid location.  It is called from compound_statement for block scope
pragmas, and by translation_unit for pragmas that appear in the file scope.
*/
{
  a_stdc_pragma_kind	kind = (a_stdc_pragma_kind)stdc_pk_none;
  a_stdc_pragma_value	value = (a_stdc_pragma_value)stdc_pv_none;
  a_boolean		err = FALSE, accept_on_off = FALSE;
  a_boolean		accept_sat = FALSE;
  char			*str;
  a_stdc_pragma_value	*state_var_ptr;

  begin_rescan_of_pragma_tokens(ppp);
  if (curr_token == tok_identifier) {
    str = locator_for_curr_id.symbol_header->identifier;
    if (c99_mode) {
      if (strcmp(str, "FP_CONTRACT") == 0) {
        kind = (a_stdc_pragma_kind)stdc_pk_fp_contract;
        state_var_ptr = &curr_fp_contract_state;
        accept_on_off = TRUE;
      } else if (strcmp(str, "FENV_ACCESS") == 0) {
        kind = (a_stdc_pragma_kind)stdc_pk_fenv_access;
        state_var_ptr = &curr_fenv_access_state;
        accept_on_off = TRUE;
      } else if (strcmp(str, "CX_LIMITED_RANGE") == 0) {
        kind = (a_stdc_pragma_kind)stdc_pk_cx_limited_range;
        state_var_ptr = &curr_cx_limited_range_state;
        accept_on_off = TRUE;
      }  /* if */
    }  /* if */
#if FIXED_POINT_ALLOWED
    if (fixed_point_enabled) {
      if (strcmp(str, "FX_FULL_PRECISION") == 0) {
        kind = (a_stdc_pragma_kind)stdc_pk_fx_full_precision;
        state_var_ptr = &curr_fx_full_precision_state;
        accept_on_off = TRUE;
      } else if (strcmp(str, "FX_FRACT_OVERFLOW") == 0) {
        kind = (a_stdc_pragma_kind)stdc_pk_fx_fract_overflow;
        state_var_ptr = &curr_fx_fract_overflow_state;
        accept_sat = TRUE;
      } else if (strcmp(str, "FX_ACCUM_OVERFLOW") == 0) {
        kind = (a_stdc_pragma_kind)stdc_pk_fx_accum_overflow;
        state_var_ptr = &curr_fx_accum_overflow_state;
        accept_sat = TRUE;
      }  /* if */
    }  /* if */
#endif /* FIXED_POINT_ALLOWED */
  }  /* if */
  if (kind == (a_stdc_pragma_kind)(a_stdc_pragma_kind)stdc_pk_none) {
    diagnostic(strict_ansi_error_severity, ec_unrecognized_stdc_pragma);
    err = TRUE;
  }  /* if */
  if (!err) {
    /* Get the setting that appears after the kind. */
    (void)get_token();
    if (curr_token == tok_identifier) {
      str = locator_for_curr_id.symbol_header->identifier;
      if (accept_on_off && strcmp(str, "ON") == 0) {
        value = (a_stdc_pragma_value)stdc_pv_on;
      } else if (accept_on_off && strcmp(str, "OFF") == 0) {
        value = (a_stdc_pragma_value)stdc_pv_off;
#if FIXED_POINT_ALLOWED
      } else if (accept_sat && strcmp(str, "SAT") == 0) {
        value = (a_stdc_pragma_value)stdc_pv_sat;
#endif /* FIXED_POINT_ALLOWED */
      } else if (strcmp(str, "DEFAULT") == 0) {
        value = (a_stdc_pragma_value)stdc_pv_default;
      }  /* if */
    }  /* if */
    if (value == (a_stdc_pragma_value)(a_stdc_pragma_value)stdc_pv_none) {
      /* coverity[dead_error_line] */ /* coverity[dead_error_condition] */
      diagnostic(strict_ansi_error_severity,
                 accept_sat ? ec_bad_stdc_fx_overflow_pragma_arg
                            : ec_bad_stdc_pragma_arg);
      err = TRUE;
    }  /* if */
    /* Bypass the value. */
    if (!err) (void)get_token();
  }  /* if */
  wrapup_rescan_of_pragma_tokens(err);
  if (!err) {
    /* Create the IL entry for this pragma, and fill in the information.
       This is only done if an IL entry is actually created.  An IL entry
       is not created if the pragma appears in an invalid location. */
    create_il_entry_for_pragma(ppp, (a_symbol_ptr)NULL, (a_statement_ptr)NULL);
    if (ppp->il_pragma_entry != NULL) {
      ppp->il_pragma_entry->variant.stdc.kind = kind;
      ppp->il_pragma_entry->variant.stdc.value = value;
    }  /* if */
    /* Update the state variable that indicates the current setting of this
       pragma. */
    *state_var_ptr = value;
  }  /* if */
}  /* process_stdc_pragma */


void stdc_pragma(a_pending_pragma_ptr	ppp)
/*
Process a predefined C99 STDC pragma.  This is the routine that is
registered with the pragma processing routines.  When a STDC pragma
appears in a valid location, process_stdc_pragma is called (via
check_for_stdc_pragmas).  Pragmas that appear elsewhere result in diagnostics.
*/
{
  pos_diagnostic(strict_ansi_error_severity,
                 ec_stdc_pragma_not_allowed_here, &ppp->pragma_position);
}  /* stdc_pragma */


void check_for_stdc_pragmas(void)
/*
If there are any current token pragmas that are C99 predefined pragmas
(or predefined fixed-point pragmas), process them now.
*/
{
  a_pending_pragma_ptr	ppp;
  a_pending_pragma_ptr	prev_ppp = NULL;
  a_pending_pragma_ptr	next_ppp;

  for (ppp = curr_token_pragmas; ppp != NULL; ppp = next_ppp) {
    next_ppp = ppp->next;
    if (ppp->descr_ptr->kind == (a_pragma_kind)pk_stdc) {
      process_stdc_pragma(ppp);
      /* Unlink this entry from the list of current token pragmas. */
      if (prev_ppp == NULL) {
        curr_token_pragmas = ppp->next;
      } else {
        prev_ppp->next = ppp->next;
      }  /* if */
      free_pending_pragma(ppp);
    } else {
      prev_ppp = ppp;
    }  /* if */
  }  /* for */
}  /* check_for_stdc_pragmas */

#if GNU_EXTENSIONS_ALLOWED
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED

/*ARGSUSED*/  /* <-- ppp is not used. */
static void process_gnu_visibility_pragma(a_pending_pragma_ptr  ppp)
/*
Handle
	#pragma GCC visibility push(<ELF-visibility>)
and
	#pragma GCC visibility pop
The "#pragma GCC visibility" part has been seen already, and the current
token is the identifier "visibility".  Warnings and/or errors are issued if
the construct is not correctly formed.
*/
{
  a_boolean  recognized = FALSE, warning_issued = FALSE;
  /* Skip the "visibility" identifier. */
  (void)get_token();
  if (curr_token == tok_identifier) {
    char *str = locator_for_curr_id.symbol_header->identifier;
    if (strcmp(str, "push") == 0) {
      recognized = TRUE;
      (void)get_token();
      if (curr_token == tok_lparen) {
        (void)get_token();
        if (curr_token == tok_identifier) {
          an_ELF_visibility_kind evk = ELF_visibility_from_string(
                               locator_for_curr_id.symbol_header->identifier);
          if (evk == (an_ELF_visibility_kind)evk_unspecified) {
            /* An invalid visibility kind was specified. */
            warning(ec_unrecognized_visibility);
            warning_issued = TRUE;
          } else {
            ppp->variant.gcc.kind = (a_gcc_pragma_kind)gcc_pk_visibility_push;
            ppp->variant.gcc.variant.visibility = evk;
          }  /* if */
          push_ELF_visibility(evk, /*namespace_attribute=*/FALSE);
          (void)get_token();
          if (curr_token != tok_rparen) {
            warning(ec_exp_rparen);
            warning_issued = TRUE;
          } else {
            (void)get_token();
          }  /* if */
        }  /* if */
      } else {
        warning(ec_exp_lparen);
        warning_issued = TRUE;
      }  /* if */
    } else if (strcmp(str, "pop") == 0) {
      recognized = TRUE;
      pop_ELF_visibility(/*namespace_attribute=*/FALSE);
      (void)get_token();
      ppp->variant.gcc.kind = (a_gcc_pragma_kind)gcc_pk_visibility_pop;
    }  /* if */
  }  /* if */
  if (warning_issued) {
    /* Do not issue another warning. */
  } else if (!recognized) {
    warning(ec_unrecognized_gcc_visibility_pragma);
  } else if (curr_token != tok_end_of_source) {
    warning(ec_extra_text_in_pp_directive);
  }  /* if */
}  /* process_gnu_visibility_pragma */

#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */

static void process_gnu_system_header_pragma(a_pending_pragma_ptr  ppp)
/*
Handle
   #pragma GCC system_header
*/
{
  ppp->variant.gcc.kind = (a_gcc_pragma_kind)gcc_pk_system_header;
  check_assertion(curr_ise != NULL);
  if (!curr_ise->assoc_il_file->from_system_include_dir) {
    if (curr_ise->assoc_il_file->is_include_file) {
      /* The remainder of the current input file should be treated as if it
         came from a system header.  We achieve this by generating #line
         directive with "from_system_include_dir" set to TRUE. */
      a_source_file_ptr  actual_sfp = curr_ise->assoc_actual_il_file;
      /* If there is already an active #line, record the end of its range. */
      if (curr_ise->assoc_il_file != actual_sfp) {
        record_end_of_source_file(curr_ise->assoc_il_file,
                                  seq_number_last_read-1);
      }  /* if */
      /* Record this as a #line directive (achieved by passing NULL values for
         full_name and name_as_written). */
      record_start_of_source_file(
                               actual_sfp, (a_seq_number)seq_number_last_read,
                               curr_ise->line_number, curr_ise->file_name,
                               /*full_name=*/(char *)NULL,
                               /*name_as_written=*/(char *)NULL,
                               &curr_ise->assoc_il_file,
                               actual_sfp->is_include_file,
                               actual_sfp->included_by_system_include,
                               actual_sfp->included_by_preinclude,
                               actual_sfp->preinclude_macros_only,
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
                               actual_sfp->is_implicit_include,
#else /* !INSTANTIATION_BY_IMPLICIT_INCLUSION */
                               FALSE,
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
                               /*from_system_include_dir=*/TRUE,
#if MICROSOFT_EXTENSIONS_ALLOWED
                               actual_sfp->is_assembly_file
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
                               FALSE
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                    );
      check_assertion(curr_ise->assoc_il_file->from_system_include_dir);
      curr_ise->from_system_include_dir = TRUE;
    } else {
      /* This directive has no effect in the primary source file. */
      pos_warning(ec_pragma_gcc_system_header_in_primary_file,
                  &pos_curr_token);
    }  /* if */
  }  /* if */
  /* Skip the "system_header" identifier. */
  (void)get_token();
  if (curr_token != tok_end_of_source) {
    warning(ec_extra_text_in_pp_directive);
  }  /* if */
}  /* process_gnu_system_header_pragma */


void gcc_pragma(a_pending_pragma_ptr  ppp)
/*
Process a "#pragma GCC ..." construct.
*/
{
  a_boolean     recognized = FALSE;
  a_boolean     ignore_in_back_end = FALSE;
  a_pragma_ptr  il_pragma_entry;

  begin_rescan_of_pragma_tokens(ppp);
  if (curr_token == tok_identifier) {
    char *str = locator_for_curr_id.symbol_header->identifier;
    if (strcmp(str, "system_header") == 0) {
      recognized = TRUE;
      ignore_in_back_end = TRUE;
      process_gnu_system_header_pragma(ppp);
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
    } else if (strcmp(str, "visibility") == 0) {
      recognized = TRUE;
      process_gnu_visibility_pragma(ppp);
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
    }  /* if */
  }  /* if */
  if (!recognized) {
    warning(ec_unrecognized_gcc_pragma);
  }  /* if */
  /* Pass error_in_pragma as TRUE to avoid diagnostics; any needed diagnostic
     will already have been issued. */
  wrapup_rescan_of_pragma_tokens(/*error_in_pragma=*/TRUE);
  /* Record an IL entry for the pragma. */
  create_il_entry_for_pragma(ppp, (a_symbol_ptr)NULL, (a_statement_ptr)NULL);
  il_pragma_entry = ppp->il_pragma_entry;
  if (recognized && il_pragma_entry != NULL) {
    /* Copy the GCC pragma description to the IL entry. */
    il_pragma_entry->ignore_in_back_end = ignore_in_back_end;
    il_pragma_entry->variant.gcc = ppp->variant.gcc;
  }  /* if */
}  /* gcc_pragma */

#endif /* GNU_EXTENSIONS_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED
/*
Processing of UPC pragmas.
*/

typedef struct a_upc_pragma_stack_entry *a_upc_pragma_stack_entry_ptr;
 
/* An entry on the UPC pragma stack. */
typedef struct a_upc_pragma_stack_entry {
  a_upc_pragma_stack_entry_ptr
              next;
                      /* Next entry on the stack.  NULL for the entry at
                         the bottom of the stack. */
  union {
    a_upc_access_method
              access_method;
                      /* Indicates whether strict or relaxed UPC semantics
                         are the default. */
  } variant;
} a_upc_pragma_stack_entry;

#if DEBUG
/*
Counts of entries allocated, to track total use of memory.
*/
static unsigned long
		num_upc_pragma_stack_entries_allocated;
#endif /* DEBUG */

static a_upc_pragma_stack_entry_ptr
		upc_coherence_stack;
			/* Pointer to the top of the UPC pragma stack. */

static a_upc_pragma_stack_entry_ptr
		avail_upc_pragma_stack_entries;
			/* List of stack entries freed and available
			   for reuse. */


static void push_upc_pragma(void)
/*
Push an entry onto the top of the stack.
*/
{
  a_upc_pragma_stack_entry_ptr  upsep;

  if (avail_upc_pragma_stack_entries != NULL) {
    upsep = avail_upc_pragma_stack_entries;
    avail_upc_pragma_stack_entries = upsep->next;
  } else {
#if DEBUG
    ++num_upc_pragma_stack_entries_allocated;
#endif /* DEBUG */
    upsep = (a_upc_pragma_stack_entry_ptr)alloc_fe(
                                            sizeof(a_upc_pragma_stack_entry));
  }  /* if */
  upsep->next = upc_coherence_stack;
  upc_coherence_stack = upsep;
} /* push_upc_pragma */


static void pop_upc_pragma(void)
/*
Pop the top entry from the UPC pragma stack and return it to the available
list.
*/
{
  a_upc_pragma_stack_entry_ptr  upsep = upc_coherence_stack;

  upc_coherence_stack = upsep->next;
  upsep->next = avail_upc_pragma_stack_entries;
  avail_upc_pragma_stack_entries = upsep;
}  /* pop_upc_pragma */


static void process_upc_pragma(a_pending_pragma_ptr  ppp,
                               a_statement_ptr       assoc_statement)
/*
Process a predefined UPC pragma.  These pragmas have the following form:

  #pragma upc relaxed
  #pragma upc strict
  #pragma upc coherence save
  #pragma upc coherence restore

This routine is called to process the pragmas when they are known to appear
in a valid location.  It is called in compound_statement for block scope
pragmas (in which case assoc_statement is the compound statement), and by
upc_pragma for pragmas that appear in the file scope (in which case
assoc_statement should be NULL).
*/
{
  a_upc_access_method  access = (a_upc_access_method)upc_access_unspecified;
  a_upc_coherence_stack_operation
                       stack_op = (a_upc_coherence_stack_operation)
                                                     upc_coherence_stack_noop;
  a_boolean            err = FALSE, unrecognized = FALSE, bad_context = FALSE;

  begin_rescan_of_pragma_tokens(ppp);
  if (curr_token != tok_identifier) {
    unrecognized = TRUE;
  } else {
    char  *str = locator_for_curr_id.symbol_header->identifier;
    if (strcmp(str, "strict") == 0) {
      access = (a_upc_access_method)upc_access_strict;
    } else if (strcmp(str, "relaxed") == 0) {
      access = (a_upc_access_method)upc_access_relaxed;
    } else if (strcmp(str, "coherence") == 0) {
      (void)get_token();
      if (curr_token != tok_identifier) {
        unrecognized = TRUE;
      } else {
        char  *str2 = locator_for_curr_id.symbol_header->identifier;
        if (strcmp(str2, "save") == 0) {
          stack_op = (a_upc_coherence_stack_operation)upc_coherence_stack_save;
        } else if (strcmp(str2, "restore") == 0) {
          stack_op =
                  (a_upc_coherence_stack_operation)upc_coherence_stack_restore;
          if (upc_coherence_stack == NULL) {
            /* There is nothing to restore. */
            bad_context = TRUE;
          }  /* if */
        } else {
          unrecognized = TRUE;
        }  /* if */
        if (!unrecognized && assoc_statement != NULL) {
          /* This pragma cannot appear here. */
          bad_context = TRUE;
        }  /* if */
      }  /* if */
    } else {
      unrecognized = TRUE;
    }  /* if */
  }  /* if */
  if (unrecognized) {
      pos_diagnostic(strict_ansi_error_severity, ec_unrecognized_upc_pragma,
                     &ppp->id_position);
      err = TRUE;
  } else if (bad_context) {
      pos_diagnostic(strict_ansi_error_severity,
                     ec_pragma_may_not_be_used_here,
                     &ppp->id_position);
      err = TRUE;
  }  /* if */
  /* Bypass the value. */
  if (!err) (void)get_token();
  wrapup_rescan_of_pragma_tokens(err);
  if (err) {
    /* Nothing more to be done. */
  } else if (access != (a_upc_access_method)upc_access_unspecified) {
    if (assoc_statement == (a_statement_ptr)NULL) {
      /* No associated statement, so update the global setting. */
      curr_upc_access_method = access;
    } else {
      check_assertion_str(
                        assoc_statement->kind == (a_statement_kind)stmk_block,
                        "process_upc_pragma: expected block");
      /* Save the local setting in the block. */
      assoc_statement->variant.block.extra_info->upc_access_method = access;
    }  /* if */
    /* Record the pragma in the IL. */
    create_il_entry_for_pragma(ppp, (a_symbol_ptr)NULL, (a_statement_ptr)NULL);
    if (ppp->il_pragma_entry != NULL) {
      ppp->il_pragma_entry->variant.upc.kind =
                                              (a_upc_pragma_kind)upc_pk_access;
      ppp->il_pragma_entry->variant.upc.value.access_method = access;
    }  /* if */
  } else if (stack_op !=
                  (a_upc_coherence_stack_operation)upc_coherence_stack_noop) {
    check_assertion(assoc_statement == (a_statement_ptr)NULL);
    if (stack_op ==
                  (a_upc_coherence_stack_operation)upc_coherence_stack_save) {
      push_upc_pragma();
      upc_coherence_stack->variant.access_method = curr_upc_access_method;
    } else {
      curr_upc_access_method = upc_coherence_stack->variant.access_method;
      pop_upc_pragma();
    }  /* if */
    /* Record the pragma in the IL. */
    create_il_entry_for_pragma(ppp, (a_symbol_ptr)NULL, (a_statement_ptr)NULL);
    if (ppp->il_pragma_entry != NULL) {
      ppp->il_pragma_entry->variant.upc.kind =
                                           (a_upc_pragma_kind)upc_pk_coherence;
      ppp->il_pragma_entry->variant.upc.value.operation = stack_op;
    }  /* if */
  }  /* if */
}  /* process_upc_pragma */


void check_for_upc_pragmas(a_statement_ptr sp)
/*
If there are any current token pragmas that are UPC predefined pragmas,
process them now.  sp should point to the block statement in which the
pragma appears.
*/
{
  a_pending_pragma_ptr  ppp;
  a_pending_pragma_ptr  prev_ppp = NULL;
  a_pending_pragma_ptr  next_ppp;

  for (ppp = curr_token_pragmas; ppp != NULL; ppp = next_ppp) {
    next_ppp = ppp->next;
    if (ppp->descr_ptr->kind == (a_pragma_kind)pk_upc) {
      process_upc_pragma(ppp, sp);
      /* Unlink this entry from the list of current token pragmas. */
      if (prev_ppp == NULL) {
        curr_token_pragmas = ppp->next;
      } else {
        prev_ppp->next = ppp->next;
      }  /* if */
      free_pending_pragma(ppp);
    } else {
      prev_ppp = ppp;
    }  /* if */
  }  /* for */
}  /* check_for_upc_pragmas */


void upc_pragma(a_pending_pragma_ptr  ppp)
/*
Process a predefined UPC pragma.  This is the routine that is
registered with the pragma processing routines.  It calls process_upc_pragma
for file scope pragmas.  Pragmas that appear elsewhere result in diagnostics.
For block scope pragmas that appear in a valid location, process_upc_pragma
is called directly by compound_statement.
*/
{
  if (scope_stack[depth_scope_stack].kind == (a_scope_kind)sck_file) {
    process_upc_pragma(ppp, (a_statement_ptr)NULL);
  } else {
    pos_warning(ec_pragma_may_not_be_used_here, &ppp->pragma_position);
  }  /* if */
}  /* upc_pragma */

#endif /* UPC_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED

void microsoft_start_map_region_pragma(a_pending_pragma_ptr  ppp)
/*
Scan a pragma for the form
	#pragma start_map_region ( <string-literal> )
This is a Microsoft extension that determines whether constructs of the form
__declspec(implementation_key(<integer-constant>)) are accepted.  The effect
of this pragma extends to the next occurrence of #pragma stop_map_region.
*/
{
  a_boolean          err = FALSE;
  a_source_position  pragma_pos;

  begin_rescan_of_pragma_tokens(ppp);
  pragma_pos = pos_curr_token;
  /* Scan the "(". */
  if (curr_token == tok_lparen) {
    (void)get_token();
  } else {
    warning(ec_exp_lparen);
    err = TRUE;
  }  /* if */
  add_stop_token(tok_rparen);
  /* Scan the string literal that specifies the identifier.  (Note that the
     "string literal" is actually scanned as a tok_header_name; the reason
     for this is to ensure that a backslash directory separator is not
     interpreted as the start of an escape sequence, which could lead to
     spurious errors and warnings.) */
  if (curr_token != tok_header_name) {
    if (!err) {
      warning(ec_exp_string_literal);
      err = TRUE;
    }  /* if */
  } else {
    /* Skip the string literal. */
    (void)get_token();
  }  /* if */
  /* Scan the ")". */
  if (curr_token == tok_rparen) {
    (void)get_token();
  } else if (!err) {
    warning(ec_exp_rparen);
    err = TRUE;
  }  /* if */
  remove_stop_token(tok_rparen);
  /* Microsoft ignores extra tokens in the pragma: Passing TRUE for
     error_in_pragma achieves the same effect. */
  wrapup_rescan_of_pragma_tokens(/*error_in_pragma=*/TRUE);
  if (!err) {
    if (in_microsoft_implementation_key_mapping_region) {
      pos_warning(ec_start_map_region_ignored, &pragma_pos);
    } else {
      in_microsoft_implementation_key_mapping_region = TRUE;
    }  /* if */
  }  /* if */
}  /* microsoft_start_map_region_pragma */


void microsoft_stop_map_region_pragma(a_pending_pragma_ptr  ppp)
/*
Scan a pragma for the form
	#pragma stop_map_region
This is a Microsoft extension that terminates the effect of the preceding
#pragma start_map_region construct.
*/
{
  begin_rescan_of_pragma_tokens(ppp);
  if (!in_microsoft_implementation_key_mapping_region) {
    warning(ec_stop_map_region_ignored);
  } else {
    in_microsoft_implementation_key_mapping_region = FALSE;
  }  /* if */
  /* Microsoft ignores extra tokens in the pragma: Passing TRUE for
     error_in_pragma achieves the same effect. */
  wrapup_rescan_of_pragma_tokens(/*error_in_pragma=*/TRUE);
}  /* microsoft_stop_map_region_pragma */


void microsoft_comment_pragma(a_pending_pragma_ptr  ppp)
/*
Scan a pragma of the form
        #pragma comment(xxx [, "str"])
This is a Microsoft extension that adds a comment record to an object or
executable file.
*/
{
  a_boolean                       err = FALSE;
  a_microsoft_pragma_comment_type kind;
  a_constant_ptr                  cp = NULL;

  begin_rescan_of_pragma_tokens(ppp);
  if (curr_token != tok_lparen) {
    error(ec_exp_lparen);
    err = TRUE;
  } else {
    /* Skip over the "(". */
    (void)get_token();
    if (curr_token != tok_identifier) {
      error(ec_exp_identifier);
      err = TRUE;
    } else {
      char *str = locator_for_curr_id.symbol_header->identifier;
      int  i;
      for (i = 0; i < (int)mpct_last; ++i) {
        if (strcmp(str, microsoft_pragma_comment_ids[i]) == 0) {
          /* Found the comment type. */
          kind = (a_microsoft_pragma_comment_type)i;
          break;
        }  /* if */
      }  /* for */
      if (i == (int)mpct_last) {
        /* Unrecognized comment type. */
        str_error(ec_unrecognized_microsoft_comment_pragma_type, str);
        err = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (!err) {
    /* Skip over the comment type identifier. */
    (void)get_token();
    if (curr_token == tok_comma) {
      /* Pick up the optional string argument.  Skip over the comma. */
      (void)get_token();
      if (curr_token == tok_string_literal) {
        if (is_error_constant(&const_for_curr_token)) {
          /* A diagnostic was already issued. */
          err = TRUE;
        } else if (!is_normal_character_kind(
                                        const_for_curr_token.character_kind)) {
          error(ec_bad_pragma_comment_string);
          err = TRUE;
        } else {
          /* Create a constant for the IL entry. */
          a_memory_region_number region_to_switch_back_to;
          switch_to_file_scope_region(&region_to_switch_back_to);
          cp = alloc_unshared_constant(&const_for_curr_token);
          switch_back_to_original_region(region_to_switch_back_to);
          /* Skip over the string literal. */
          (void)get_token();
        }  /* if */
      } else {
        error(ec_exp_string_literal);
        err = TRUE;
      }  /* if */
    } else if (curr_token != tok_rparen) {
      error(ec_exp_comma);
      err = TRUE;
    }  /* if */
  }  /* if */
  if (!err) {
    if (curr_token == tok_rparen) {
      /* Skip over the ")". */
      (void)get_token();
    } else {
      error(ec_exp_rparen);
      err = TRUE;
    }  /* if */
  }  /* if */
  wrapup_rescan_of_pragma_tokens(err);
  if (!err) {
    create_il_entry_for_pragma(ppp, (a_symbol_ptr)NULL, (a_statement_ptr)NULL);
    if (ppp->il_pragma_entry != NULL) {
      ppp->il_pragma_entry->variant.comment.kind = kind;
      ppp->il_pragma_entry->variant.comment.str = cp;
    }  /* if */
  }  /* if */
}  /* microsoft_comment_pragma */



/* An entry on the "#pragma conform(forScope) stack. */
typedef struct a_forScope_stack_entry
			*a_forScope_stack_entry_ptr;
typedef struct a_forScope_stack_entry {
  a_forScope_stack_entry_ptr
		next;
			/* Next entry on the stack.  NULL for the entry at
			   the bottom of the stack. */
  char		*name;
			/* The identifying name of this stack entry -- used
			   for "targeted" popping.  May be NULL. */
  a_boolean
		use_nonstandard_for_init_scope;
			/* Recorded value of the global variable of the same
			   name. */
  a_boolean
		microsoft_type_dependent_for_init_scope;
			/* Recorded value of the global variable of the same
			   name. */
} a_forScope_stack_entry;

#if DEBUG
static unsigned long
		num_forScope_stack_entries_allocated;
#endif /* DEBUG */

static a_forScope_stack_entry_ptr
		forScope_stack;
			/* Pointer to the top of the stack of forScope
			   conformance states. */

static a_forScope_stack_entry_ptr
		avail_forScope_stack_entries;
			/* List of forScope state stack entries that were
			   freed and are now available for reuse. */


static void push_forScope_stack_entry(char  *name)
/*
Push an entry onto the top of the stack of forScope conformance states.
Record the given name and the current conformance state in that entry.
*/
{
  a_forScope_stack_entry_ptr  fssep;

  if (avail_forScope_stack_entries != NULL) {
    fssep = avail_forScope_stack_entries;
    avail_forScope_stack_entries = fssep->next;
  } else {
    fssep = (a_forScope_stack_entry_ptr)alloc_fe(
                               sizeof(a_forScope_stack_entry));
#if DEBUG
    ++num_forScope_stack_entries_allocated;
#endif /* DEBUG */
  }  /* if */
  fssep->next = forScope_stack;
  fssep->name = name;
  fssep->use_nonstandard_for_init_scope = use_nonstandard_for_init_scope;
  fssep->microsoft_type_dependent_for_init_scope =
                                      microsoft_type_dependent_for_init_scope;
  forScope_stack = fssep;
}  /* push_forScope_stack_entry */


static void pop_forScope_stack_entry(void)
/*
Pop the top entry from the forScope stack.
*/
{
  a_forScope_stack_entry_ptr  fssep;

  fssep = forScope_stack;
  forScope_stack = fssep->next;
  fssep->next = avail_forScope_stack_entries;
  avail_forScope_stack_entries = fssep;
}  /* pop_forScope_stack_entry */


static a_forScope_stack_entry_ptr find_forScope_stack_entry(char  *id)
/*
Look through the forScope stack for an entry that has the given identifier
associated with it.  If id is NULL, return the last pushed entry.  If the
stack is empty or if no matching entry is found, return NULL.
*/
{
  a_forScope_stack_entry_ptr  result = forScope_stack;

  if (id != NULL) {
    for (; result != NULL; result = result->next) {
      if (result->name != NULL && strcmp(result->name, id) == 0) {
        /* Found a match. */
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return result;
}  /* find_forScope_stack_entry */


void microsoft_conform_pragma(a_pending_pragma_ptr  ppp)
/*
Scan a pragma with one of the following forms:
    #pragma conform(forScope, on|off)
    #pragma conform(forScope, show)
    #pragma conform(forScope, push|pop [ , <identifier> [ , on|off ] ] )
The first form determines whether the standard for-init scope rules are in
effect ("on") or not ("off").  The second form triggers a warning reporting
the current state of forScope conformance.  
The third form with "push" pushes the state of the current behavior on a stack
and, optionally, associates a given identifier with that state.  The "pop"
variant without an identifier restores the last recorded state and pops it
from the stack.  The variant with an identifier restores the last state that
was pushed with the same identifier and pops it and all later recorded states
from the stack.  The "push" and "pop" variants that included an identifier
naming a state can also be followed by "on" or "off", which behaves as if the
first form had followed the "push" or "pop".
Malformed constructs result in warnings, not errors.
*/
{
  a_boolean  err = FALSE;
  a_boolean  on = FALSE, off = FALSE, show = FALSE, push = FALSE, pop = FALSE;
  char       *id = NULL;

#define check_and_skip_token(tok, ec)                                        \
  if (curr_token != tok) {                                                   \
    warning(ec);                                                             \
    err = TRUE;                                                              \
    goto end_of_parse;                                                       \
  } else {                                                                   \
    (void)get_token();                                                       \
  }  /* if */

#define scan_on_or_off()                                                     \
  if (curr_token_is_identifier_string("on")) {                               \
    on = TRUE;                                                               \
    (void)get_token();                                                       \
  } else if (curr_token_is_identifier_string("off")) {                       \
    off = TRUE;                                                              \
    (void)get_token();                                                       \
  } else {                                                                   \
    warning(ec_exp_on_or_off);                                               \
    err = TRUE;                                                              \
    goto end_of_parse;                                                       \
  }  /* if */

  /* First parse the pragma, to determine the value of various flags. */
  begin_rescan_of_pragma_tokens(ppp);
  check_and_skip_token(tok_lparen, ec_exp_lparen);
  if (!curr_token_is_identifier_string("forScope")) {
    warning(ec_invalid_pragma_conform_kind);
    err = TRUE;
    goto end_of_parse;
  }  /* if */
  /* Skip over "forScope" */
  (void)get_token();
  check_and_skip_token(tok_comma, ec_exp_comma);
  if (curr_token_is_identifier_string("show")) {
    show = TRUE;
    (void)get_token();
  } else if ((push = curr_token_is_identifier_string("push")) == TRUE ||
             (pop = curr_token_is_identifier_string("pop")) == TRUE) {
    (void)get_token();
    if (curr_token == tok_rparen) goto check_terminating_rparen;
    check_and_skip_token(tok_comma, ec_exp_comma);
    if (curr_token == tok_identifier) {
      id = locator_for_curr_id.symbol_header->identifier;
      (void)get_token();
      if (curr_token == tok_comma) {
        (void)get_token();
        scan_on_or_off();
      }  /* if */
    } else {
      warning(ec_exp_identifier);
      err = TRUE;
      goto end_of_parse;
    }  /* if */
  } else {
    scan_on_or_off();
  }  /* if */
check_terminating_rparen:
  check_and_skip_token(tok_rparen, ec_exp_rparen);
end_of_parse:
  /* wrapup_rescan_of_pragma_tokens will warn about addition trailing tokens
     unless err is TRUE. */
  wrapup_rescan_of_pragma_tokens(err);
#undef scan_on_or_off
#undef check_and_skip_token
  /* err == TRUE only indicates that we issued a syntax-related warning (the
     Microsoft compiler is frequently silent in those cases), but if we got
     far enough with parsing the pragma can still have an effect. */
  if (show || push || pop || on || off) {
    check_assertion(!on || !off);
    /* Create the IL entry. */
    create_il_entry_for_pragma(ppp, (a_symbol_ptr)NULL, (a_statement_ptr)NULL);
    if (ppp->il_pragma_entry != NULL) {
      ppp->il_pragma_entry->variant.conform.kind =
                                (a_microsoft_pragma_conform_kind)mpck_forScope;
      ppp->il_pragma_entry->variant.conform.on = on;
      ppp->il_pragma_entry->variant.conform.off = off;
      ppp->il_pragma_entry->variant.conform.show = show;
      ppp->il_pragma_entry->variant.conform.push = push;
      ppp->il_pragma_entry->variant.conform.pop = pop;
      ppp->il_pragma_entry->variant.conform.identifier = id;
    }  /* if */
    /* Apply the pragma. */
    if (C_mode()) {
      /* The Microsoft C compiler accepts "#pragma conform", but it has no
         effect. */
    } else if (show) {
      check_assertion(!on && !off && !push && !pop);
      if (use_nonstandard_for_init_scope ||
          microsoft_type_dependent_for_init_scope) {
        pos_warning(ec_show_pragma_conform_forScope_is_nonstandard,
                     &ppp->id_position);
      } else {
        pos_warning(ec_show_pragma_conform_forScope_is_standard,
                     &ppp->id_position);
      }  /* if */
    } else {
      if (push) {
        push_forScope_stack_entry(id);
      } else if (pop) {
        if (forScope_stack == NULL) {
          pos_warning(ec_forScope_stack_empty, &ppp->id_position);
        } else {
          /* Determine the scope stack to pop. */
          a_forScope_stack_entry_ptr  fssep = find_forScope_stack_entry(id);
          if (fssep == NULL) {
            pos_st_warning(ec_no_matching_forScope_stack_entry,
                           &ppp->id_position, id);
          } else {
            /* Restore the previously saved state. */
            use_nonstandard_for_init_scope =
                                        fssep->use_nonstandard_for_init_scope;
            microsoft_type_dependent_for_init_scope =
                               fssep->microsoft_type_dependent_for_init_scope;
            while (fssep != forScope_stack) pop_forScope_stack_entry();
            pop_forScope_stack_entry();
          }  /* if */
        }  /* if */
      }  /* if */
      if (on) {
        /* Turn on standard for-init-scope behavior. */
        use_nonstandard_for_init_scope = FALSE;
        microsoft_type_dependent_for_init_scope = FALSE;
      } else if (off) {
        /* Turn off standard for-init-scope behavior. */
        if (microsoft_version < 1310) {
          use_nonstandard_for_init_scope = TRUE;
        } else {
          microsoft_type_dependent_for_init_scope = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* microsoft_conform_pragma */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if ALIAS_DIRECTIVE
static void proc_alias(void)
/*
Scan and process a #alias directive.
*/
{
  if (generate_pp_output) {
    /* Generating preprocessing output for some other compiler.  Pass the
       directive unchanged to output. */
    pass_directive_to_output();
  } else {
    /* Ignore the directive. */
    flush_to_newline();
  }  /* if */
}  /* proc_alias */
#endif /* ALIAS_DIRECTIVE */


static void pch_prefix_processing_for_pp_directive(
					a_pp_directive_kind	kind,
                                        a_source_position	*pos)
/*
Create a precompiled header prefix event for a preprocessing directive.
This is done by converting the tokens that follow the preprocessing
directive into a character string and calling the routine to add a
pch event.

The special "prescan" mode is also used when bypassing the initial portion
of a file whose input is obtained from a precompiled header.  In this mode,
we simply flush directives until we find one that matches the position
of the last thing obtained from the PCH.  We then reset the
building_pch_prefix flag so that normal compilation processing will
begin.
*/
{
  a_boolean	is_pragma_hdrstop;
  a_line_number	actual_line;

   /* Save the line number of the beginning of the directive. */
  actual_line = curr_ise->actual_line;
  /* Bypass the directive keyword. */
  (void)get_token();
  /* See if this is the special header stop pragma. */
  is_pragma_hdrstop = kind == ppd_pragma && curr_id_is("hdrstop");
  if (using_a_pch_file) {
    /* Skip to the end of this directive. */
    while (curr_token != tok_newline) (void)get_token();
    if (building_pch_prefix) {
      if (is_pragma_hdrstop ||
          (actual_line == (a_line_number)pos_of_last_event_from_pch.seq &&
           pos->column == pos_of_last_event_from_pch.column)) {
        /* Actually, both conditions should be TRUE when a pragma hdrstop
           is found. */
        next_event_resumes_compilation = TRUE;
      }  /* if */
    }  /* if */
  } else if (pragma_hdrstop_found) {
    /* We previously encountered a pragma hdrstop, disregard any
       additional events. */
  } else {
    /* Terminate the event list processing when a pragma hdrstop is found. */
    if (is_pragma_hdrstop) {
      process_prefix_pragma_hdrstop();
    } else if (kind == ppd_pragma && curr_id_is("no_pch")) {
      /* A #pragma no_pch has been found.  Suppress the creation and
         of a precompiled header for this compilation. */
      suppress_creation_of_pch();
    } else {
      convert_pp_directive_to_string(/*is_microsoft_pragma_operator=*/FALSE);
      add_pch_event(pchek_pp_directive, kind, pp_dir_string_buffer, pos,
                    actual_line);
    }  /* if */
  }  /* if */
}  /* pch_prefix_processing_for_pp_directive */


void create_preinclude_pch_event(void)
/*
Create a PCH event for a preinclude file.
*/
{
  add_pch_event(pchek_pp_directive, ppd_include, (char*)NULL,
                &preinclude_source_position, (a_line_number)0);
}  /* create_preinclude_pch_event */


void pch_prefix_processing_for_preinclude(void)
/*
This routine is called at the start of a translation unit to determine
whether "real" compilation of the current file should being immediately
following the processing of a preincluded file.
*/
{
  if (using_a_pch_file) {
    if (cmp_source_positions(pos_of_last_event_from_pch,
                             preinclude_source_position) == 0) {
      next_event_resumes_compilation = TRUE;
    }  /* if */
  }  /* if */
}  /* pch_prefix_processing_for_preinclude */


void pp_directive(void)
/*
The "#" of a preprocessor directive is the current character.  Scan and
execute the preprocessor directive.
*/
{
  a_boolean	     	save_fetch_pp_tokens = fetch_pp_tokens;
  a_boolean	     	save_expand_macros = expand_macros;
  a_boolean          	save_do_string_literal_concatenation =
                                               do_string_literal_concatenation;
  a_source_position  	save_error_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position     save_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_source_position  	start_of_dir_position;
  a_pp_directive_kind	dir_kind;
  a_boolean		local_is_header_stop_dir;
  a_boolean		was_simulated_stdarg_include = FALSE;

  db_enter(3, "pp_directive");

  /* Save the error position for later restoration because we may change
     it.  Similarly save the current construct's end position: We may
     temporarily set it while scanning a preprocessor expression, but
     outside of preprocessing, the "current construct" should not include
     preprocessing constructs. */
  copy_source_position(error_position, save_error_position);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  copy_source_position(curr_construct_end_position,
                       save_construct_end_position);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Save the position of the beginning of the directive. */
  start_of_dir_position = pos_curr_token;
  in_preprocessing_directive = TRUE;
  fetch_pp_tokens = TRUE;
  expand_macros = FALSE;
  do_string_literal_concatenation = FALSE;
  actual_include_was_suppressed = FALSE;
  /* Start a lexical context.  This includes a new stop token set that will
     stop flushing on error.  Put the newline token in the stop token set. */
  push_lexical_state_stack();
  add_stop_token(tok_newline);
  /* Identify the keyword and go to the right processing routine. */
  dir_kind = identify_dir_keyword();
  if (next_event_resumes_compilation) {
    /* We are done skipping the file prefix when making use of a PCH. */
    pch_fixup_part_2();
  }  /* if */
  /* See if this directive marks the header stop position.  If so,
     after processing the directive, we need to call
     generate_precompiled_header. */
  local_is_header_stop_dir = is_header_stop_position(start_of_dir_position);
  if (is_header_stop_dir || local_is_header_stop_dir) {
    if (dir_kind == ppd_include || dir_kind == ppd_include_next
#if MICROSOFT_EXTENSIONS_ALLOWED
        || dir_kind == ppd_import
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                 ) {
      /* When we have completed scanning of this include file, generate
         the precompiled header.  This test is done here, because
         the flag must be set before the input stack is pushed.  If the
         include is optimized away, pop_input_stack won't be able to
         generate the PCH file. */
      is_header_stop_dir = FALSE;
      local_is_header_stop_dir = FALSE;
      generate_pch_on_return_to_primary_source_file = TRUE;
    }  /* if */
  }  /* if */
  if (!building_pch_prefix) {
    switch ((int)dir_kind) {
      case ppd_not_valid:
        /* Unrecognized preprocessing directive. */
        diagnostic(es_discretionary_error, ec_bad_pp_directive_keyword);
        some_error_in_curr_directive = TRUE;
        break;
      case ppd_if:
        proc_if();
        break;
      case ppd_ifdef:
        proc_ifdef(/*is_ifdef=*/TRUE);
        break;
      case ppd_ifndef:
        proc_ifdef(/*is_ifdef=*/FALSE);
        break;
      case ppd_elif:
        proc_elif(/*perform_elif=*/TRUE);
        break;
      case ppd_else:
        proc_else(/*perform_else=*/TRUE);
        break;
      case ppd_endif:
        proc_endif();
        break;
      case ppd_include:
        proc_include(/*is_include_next=*/FALSE, &was_simulated_stdarg_include);
        break;
      case ppd_define:
        (void)proc_define();
        break;
      case ppd_undef:
        proc_undef();
        break;
      case ppd_line:
        proc_line(/*cpp_output_form=*/FALSE);
        break;
      case ppd_error:
        proc_error();
        break;
      case ppd_warning:
        proc_warning();
        break;
      case ppd_pragma:
        proc_pragma(&start_of_dir_position);
        break;
#if IDENT_DIRECTIVE_AND_PRAGMA
      case ppd_ident:
        nonstandard_pp_directive();
        proc_ident(&start_of_dir_position);
        break;
#endif /* IDENT_DIRECTIVE_AND_PRAGMA */
#if ALIAS_DIRECTIVE
      case ppd_alias:
        nonstandard_pp_directive();
        proc_alias();
        break;
#endif /* ALIAS_DIRECTIVE */
#if ATT_PREPROCESSING_EXTENSIONS_ALLOWED
      case ppd_assert:
        nonstandard_pp_directive();
        proc_assert();
        break;
      case ppd_unassert:
        nonstandard_pp_directive();
        proc_unassert();
        break;
#endif /* ATT_PREPROCESSING_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
      case ppd_import:
        proc_import();
        break;
      case ppd_using:
        proc_using(&start_of_dir_position);
        break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      case ppd_include_next:
        nonstandard_pp_directive();
        proc_include(/*is_include_next=*/TRUE, &was_simulated_stdarg_include);
        break;
      case ppd_null:
        /* Null directive -- ignore. */
        break;
      case ppd_linedef:
        /* A line-identifying directive (output from cpp); this is similar
           to a #line directive, but not exactly the same. */
        nonstandard_pp_directive();
        proc_line(/*cpp_output_form=*/TRUE);
        break;
      default:
        unexpected_condition_str("pp_directive: bad pp directive code");
    }  /* switch */
    /* If some other preprocessing directive is seen outside of the
       #ifndef/#endif guard code of the current file then it is not a
       candidate for suppression of a subsequent include. */
    switch (dir_kind) {
      case ppd_if:
      case ppd_ifdef:
      case ppd_ifndef:
      case ppd_else:
      case ppd_endif:
      case ppd_include:
      case ppd_include_next:
#if MICROSOFT_EXTENSIONS_ALLOWED
      case ppd_import:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Processing for these directives is done in the specific routines
           called above. */
        break;
      default:
        {
          a_byte	ifg_state = get_ifg_state();
          if (ifg_state < IFG_STATE_FAIL) {
            set_ifg_state(IFG_STATE_FAIL);
          }  /* if */
        }
        break;
    }  /* switch */
    /* Check that all of the text of the directive was taken. */
    end_of_directive_processing();
  } else {
    /* We are building the precompiled header prefix information.  Simply
       record information about the preprocessing directive that was
       encountered. */
    pch_prefix_processing_for_pp_directive(dir_kind, &start_of_dir_position);
  }  /* if */
  remove_stop_token(tok_newline);
  /* Restore the lexical context as at entry. */
  pop_lexical_state_stack();
  in_preprocessing_directive = FALSE;
  fetch_pp_tokens = save_fetch_pp_tokens;
  expand_macros = save_expand_macros;
  do_string_literal_concatenation = save_do_string_literal_concatenation;
  if (actual_include_was_suppressed) {
    /* Normally the check for generation of a PCH file is done when the
       input stack is popped.  For include operations where the actual
       include is suppressed, it is done here (so that certain state
       information, such as the stop token stack state, is correct). */
    check_for_generation_of_pch_on_return_to_primary_file();
  }  /* if */
  /* Restore the positions saved at entry. */
  copy_source_position(save_error_position, error_position);
  process_immediate_pragmas();
#if EXTRA_SOURCE_POSITIONS_IN_IL
  copy_source_position(save_construct_end_position,
                       curr_construct_end_position);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (is_header_stop_dir || local_is_header_stop_dir) {
    if (dir_kind != ppd_include && dir_kind != ppd_include_next
#if MICROSOFT_EXTENSIONS_ALLOWED
        && dir_kind != ppd_import
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                 ) {
      /* This is the last directive in a precompiled header file that is
         to be generated. */
      is_header_stop_dir = FALSE;
      generate_precompiled_header();
      header_stop_no_longer_pending();
    }  /* if */
  }  /* if */
  db_exit();
}  /* pp_directive */


void verify_that_all_pp_ifs_were_closed(void)
/*
Verify that all preprocessing #ifs were closed (we are currently at end of
file).
*/
{
  /* Check that all the #ifs opened in the current file have been closed;
     any opened in outer files need not be closed yet. */
  while (pp_if_stack_depth > base_pp_if_stack_depth) {
    pos_error(ec_missing_endif, &pp_if_stack[pp_if_stack_depth].if_pos);
    pp_if_stack_depth--;
  }  /* while */
}  /* verify_that_all_pp_ifs_were_closed */


void process_macro_preincludes(void)
/*
When the preinclude_macros option is used, scan and discard any tokens
until the end of the preinclude file is reached.  The files have already
been pushed onto the input stack.  Read to the end of each file.
*/
{
  /* The preinclude will have been processed as part of the PCH when
     using a precompiled header. */
  if (!using_a_pch_file && macro_preinclude_file_list != NULL) {
    a_preinclude_file_ptr	pfp;
    a_boolean			save_generate_pp_output;
    /* Suppress preprocessed output while scanning macro preincludes. */
    save_generate_pp_output = generate_pp_output;
    generate_pp_output = FALSE;
    for (pfp = macro_preinclude_file_list; pfp != NULL; pfp = pfp->next) {
      /* Flush the tokens from this preinclude.  Any macros will be evaluated
         during this process.  Stop at the end of the file.  Note that the
         push_input_stack for a macro preinclude file sets
         do_not_advance_past_end_of_file in the input stack entry, which
         prevents running off the end of the file. */
      for (;;) {
        if (get_token() == tok_end_of_source) break;
      }  /* for */
      /* Restore the generate_pp_output value before popping the last
         macro preinclude file.  This is necessary to output a #line
         directive for the primary source file.  Set the flag to indicate
         that the current source line should not be emitted though. */
      if (pfp->next == NULL) {
        generate_pp_output = save_generate_pp_output;
        do_not_put_curr_line_in_pp_output = TRUE;
      }  /* if */
      /* Pop the current file, and push another preinclude if there
         is another one. */
      pop_input_stack();
    }  /* for */
  }  /* if */
}  /* process_macro_preincludes */


void cpp_driver(void)
/*
Read through the source, and preprocess it.  Depending on command-line
options, possibly write out the preprocessed text.
This is called instead of the main compiler driver when the compiler
is asked to act like cpp.
*/
{
  /* Read source until end of file. */
  fetch_pp_tokens = TRUE;
  /* Expand macros only if generating preprocessing output (and not, for
     example, when generating makefile dependencies) . */
  expand_macros = generate_pp_output;
  /* If the preinclude_macros option was used, scan the files that provide
     macro definitions. */
  if (macro_preinclude_file_list != NULL) process_macro_preincludes();
  do {} while (get_token() != tok_end_of_source);
  /* In some cases involving macro ids right before the end of file,
     the end of file line will have been modified (characters will have
     been inserted into it).  Check for that, and dump out the final
     preprocessing and/or expanded raw listing output line in that case. */
  if (source_line_modif_list != NULL) {
    if (generate_pp_output) {
      gen_pp_output_for_curr_line();
    }  /* if */
    if (f_raw_listing != NULL) {
      gen_expanded_raw_listing_output_for_curr_line(/*do_inserted_text=*/TRUE);
    }  /* if */
  }  /* if */
}  /* cpp_driver */

#if DEBUG

unsigned long show_preproc_space_used(void)
/*
Display and return the amount of space used for preprocessing structures.
*/
{
  unsigned long grand_total = 0;
#if MICROSOFT_EXTENSIONS_ALLOWED || UPC_EXTENSIONS_ALLOWED
  unsigned long num, size, total;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || UPC_EXTENSIONS_ALLOWED */

  db_space_used_header("Preprocessing table use:");

#if UPC_EXTENSIONS_ALLOWED
  db_space_used_lost("UPC pragma stack entries",
                     avail_upc_pragma_stack_entries,
                     num_upc_pragma_stack_entries_allocated,
                     a_upc_pragma_stack_entry);
#endif /* UPC_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  db_space_used_lost("forScope pragma stk ents",
                     avail_forScope_stack_entries,
                     num_forScope_stack_entries_allocated,
                     a_forScope_stack_entry);
  db_space_used("include alias entries", num_include_aliases_allocated,
                an_include_alias);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

  db_space_used_total();

  return (grand_total);
}  /* show_preproc_space_used */

#endif /* DEBUG */

void preproc_one_time_init(void)
/*
One-time initialization for preproc.c and preproc.h variables.
*/
{
  /* Save variables that are needed for precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
#if UPC_EXTENSIONS_ALLOWED
      pch_saved_var_array_elem(avail_upc_pragma_stack_entries),
#if DEBUG
      pch_saved_var_array_elem(num_upc_pragma_stack_entries_allocated),
#endif /* if DEBUG */
#endif /* UPC_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
      pch_saved_var_array_elem(in_microsoft_implementation_key_mapping_region),
      pch_saved_var_array_elem(forScope_stack),
      pch_saved_var_array_elem(avail_forScope_stack_entries),
#if DEBUG
      pch_saved_var_array_elem(num_forScope_stack_entries_allocated),
#endif /* if DEBUG */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  /* Global variables declared in preproc.h. */
  size_pp_dir_string_buffer = 0;
  /* Static variables declared in this file. */
  pp_if_stack = NULL;
  size_pp_if_stack = 0;
  pp_dir_string_buffer = NULL;
#if UPC_EXTENSIONS_ALLOWED
  register_trans_unit_variable(upc_coherence_stack);
#endif /* UPC_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  register_trans_unit_variable(forScope_stack);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* preproc_one_time_init */


void preproc_trans_unit_init(void)
/*
Initialization of things related to preprocessing that must be repeated for
every translation unit.
*/
{
  /* Most of these variables control lexical functions, but they are defined
     in preproc.h. */
  fetch_pp_tokens = FALSE;
  expand_macros = TRUE;
  in_preprocessing_directive = FALSE;
  suppress_keyword_recognition = FALSE;
  caching_pragma_tokens = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  in_microsoft_attribute = FALSE;
  forScope_stack = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  recognize_keywords_in_pragma = FALSE;
  do_string_literal_concatenation = TRUE;
  in_pp_if_expression = FALSE;
  exp_header_name = FALSE;
  exp_system_header_name = FALSE;
  exp_digit_sequence = FALSE;
  do_not_put_curr_line_in_pp_output = TRUE;
  pass_pp_directive_to_output = FALSE;
  next_seq_in_pp_output = 1;
  prev_pp_output_line_was_complete = TRUE;
  currently_in_pp_if_skip = FALSE;
  pp_if_stack_depth = -1;
  base_pp_if_stack_depth = -1;
  is_header_stop_dir = FALSE;
  actual_include_was_suppressed = FALSE;
#if UPC_EXTENSIONS_ALLOWED
  upc_coherence_stack = NULL;
#endif /* UPC_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  in_microsoft_implementation_key_mapping_region = FALSE;
  curr_assembly_index = 0;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* preproc_trans_unit_init */


void preproc_init(void)
/*
Initialize things related to preprocessing that must be initialized
for each compilation.  (Predefined macros are established by 
init_predefined_macros.)
*/
{
#if UPC_EXTENSIONS_ALLOWED
  avail_upc_pragma_stack_entries = NULL;
#if DEBUG
  num_upc_pragma_stack_entries_allocated = 0;
#endif /* DEBUG */
#endif /* UPC_EXTENSIONS_ALLOWED */
  header_name_buffer = alloc_text_buffer(256);
#if MICROSOFT_EXTENSIONS_ALLOWED
  avail_forScope_stack_entries = NULL;
  include_alias_hash_table = NULL;
#if DEBUG
  num_forScope_stack_entries_allocated = 0;
  num_include_aliases_allocated = 0;
#endif /* DEBUG */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* preproc_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2012 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
