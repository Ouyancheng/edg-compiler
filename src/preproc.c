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

preproc.c -- Preprocessing directives.

*/


#include "basics.h"
#include "preproc.h"
#include "decls.h"
#include "error.h"
#include "lexical.h"
#include "mem_manage.h"
#include "host_envir.h"
#include "il.h"
#include "cmd_line.h"
#include "expr.h"
#include "symbol_tbl.h"
#include "symbol_ref.h"
#include "macro.h"
#include "const_ints.h"
#include "pch.h"

typedef struct a_pp_if_stack_entry *a_pp_if_stack_entry_ptr;
typedef struct a_pp_if_stack_entry {
  /* Entry on the stack for preprocessing #ifs (and #ifdefs, etc.). */
  a_source_position
		if_pos;	/* Position at which the #if appeared. */
  a_boolean	else_encountered;
			/* TRUE once the #else (if any) has been seen. */
} a_pp_if_stack_entry;

static a_pp_if_stack_entry_ptr
		pp_if_stack = NULL;
			/* Stack of preprocessing ifs.  Dynamically allocated;
			   can be expanded if necessary.  size_pp_if_stack
			   gives the number of elements currently allocated.
			   Allocation is not per-file. */
static sizeof_t	size_pp_if_stack = 0;
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


/* Advance declaration needed because of mutual recursion: */
static void skip_to_endif(a_boolean stop_skip_on_else_or_elif);

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
    } else if (curr_id_is("ident")) {
      /* #ident directive. */
      kind = ppd_ident;
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
    diagnostic(strict_ansi_error_severity, ec_nonstd_pp_directive);
  }  /* if */
}  /* nonstandard_pp_directive */


/*
Flush to the newline at the end of the current preprocessing directive.
End-of-source is also checked for because it can come up in some error
cases.
*/
#define flush_to_newline()                                            \
{ while (curr_token != tok_newline &&                                 \
         curr_token != tok_end_of_source) (void)get_token();}


static void ignore_harmless_trailing_comment(void)
/*
Ignore a harmless comment at the end of a preprocessing directive.
If the dialect of C being compiled is pcc, this is done without
a diagnostic (pcc allows comments at the ends of several kinds of lines).
*/
{
  if (curr_token != tok_newline) {
    if (strict_ansi_mode) {
      pos_diagnostic(strict_ansi_discretionary_severity,
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
     previous error. */
  if (curr_token != tok_newline) {
    if (!some_error_in_curr_directive) {
      pos_error(ec_extra_text_in_pp_directive, &pos_curr_token);
      some_error_in_curr_directive = TRUE;
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
    *condition = !eqlit_integer_constant(&temp_const, 0L);
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
       Error except when emulating pcc, which doesn't give an error. */
    if (C_dialect != C_dialect_pcc) {
      error(ec_pp_else_already_appeared);
    } else {
      warning(ec_pp_else_already_appeared);
    }  /* if */
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
    pp_if_stack = (a_pp_if_stack_entry_ptr)realloc_general(
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
    in_preprocessing_directive = TRUE;
    /* Identify the directive and process it. */
    switch ((int)identify_dir_keyword()) {
      case ppd_endif:
        /* #endif, valid end of if-skip. */
        proc_endif();
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
        /* Unrecognized directive.  Issue a warning, then ignore. */
        warning(ec_bad_pp_directive_keyword);
        some_error_in_curr_directive = TRUE;
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


static void proc_if(void)
/*
Scan and process an #if directive.
*/
{
  a_boolean condition;

  scan_if_expr(&condition);
  perform_if(condition);
}  /* proc_if */


static void proc_ifdef(a_boolean is_ifdef)
/*
Scan and process an #ifdef or #ifndef directive (is_ifdef == TRUE and
FALSE, respectively).
*/
{
  a_symbol_ptr assoc_symbol;
  a_boolean    condition = FALSE;

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
    a_byte	ifg_state = get_ifg_state();
    if (ifg_state == IFG_STATE_START) {
      /* If we are at the start of an include file then record 
         information about this @ifdef so that it can be used later to
         see if subsequent includes can be suppressed. */
      char *nm = alloc_fe(len_of_curr_token+2);
      strncpy(nm, start_of_curr_token, size_t_arg(len_of_curr_token));
      nm[len_of_curr_token] = 0;
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
    /* Look to see if there is a macro with this name. */
    assoc_symbol = find_symbol(start_of_curr_token, len_of_curr_token,
                               &locator_for_curr_id);
    assoc_symbol = find_defined_macro(assoc_symbol);
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
    /* Look to see if there is a macro with this name. */
    assoc_symbol = find_symbol(start_of_curr_token, len_of_curr_token,
                               &locator_for_curr_id);
    /* find_defined_macro cannot be used because if we have "#undef defined"
       we want to give an error, not ignore it. */
    get_symbol_of_kind((a_symbol_kind)sk_macro, assoc_symbol);
    if (assoc_symbol == NULL) {
      /* No such macro, so #undef is ignored. */
    } else if (assoc_symbol->variant.macro_def->cannot_be_redefined) {
      /* The macro is predefined. */
      error(ec_cannot_undef_predef_macro);
    } else {
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


static char *copy_header_name(void)
/*
Allocate and copy the file name from the current token (a header name).
There may be end-of-token markers in the string, which must be removed
(this is because header names that result from macro expansions can be
made up of several preprocessing tokens; see 3.8.2 and also
accum_quoted_string).
*/
{
  char		*name_start_pos, *in_pos, *out_pos;
  sizeof_t	name_len, i;

  name_start_pos = alloc_il((sizeof_t)(
           (name_len = len_of_curr_token - 2 /* Drop quoting characters. */)
           + 1 /* Space for null. */));
  in_pos = start_of_curr_token+1;
  out_pos = name_start_pos;
  /* Copy the string, removing end-of-token markers.  Note that space
     including the end-of-token markers was allocated in the output
     string, so there may be a bit of wasted space. */
  for (i = 1; i <= name_len; i++) {
    if (*in_pos != END_OF_TOKEN_MARKER) *out_pos++ = *in_pos;
    in_pos++;
  }  /* for */
  *out_pos = '\0';
  return(name_start_pos);
}  /* copy_header_name */


static void proc_include(void)
/*
Scan and process a #include directive.
*/
{
  char                       *name_start_pos;
  a_directory_name_entry_ptr search_path;
  a_boolean		     is_system_include;
  a_byte		     ifg_state;

  /* The syntax is one of the following (see standard, 3.8.2):

     # include <h-char-sequence> new-line
     # include "q-char-sequence" new-line
     # include pp-tokens new-line

     (where the pp-tokens are macro-expanded to yield one of the
     first two forms.)
  */
  ifg_state = get_ifg_state();
  if (ifg_state < IFG_STATE_FAIL) {
    /* If another include is seen outside of the #ifndef/#endif guard
       code of the current file then it is not a candidate for suppression
       of a subsequent include. */
    set_ifg_state(IFG_STATE_FAIL);
  }  /* if */
  /* Try to expand macros to get one of the normal forms. */
  expand_macros = TRUE;
  exp_header_name = TRUE;
  (void)get_token();
  exp_header_name = FALSE;
  if (curr_token != tok_header_name) {
    /* Missing include file name. */
    catastrophe(ec_exp_file_name);
  } else {
    /* A header name was scanned. */
    /* Pick the appropriate search path of directories for "name" vs.
       <name>. */
    is_system_include = *start_of_curr_token == '<';
    if (is_system_include) {
      search_path = sys_incl_search_path;
    } else {
      search_path = incl_search_path;
    }  /* if */
    /* Allocate space for and copy the name. */
    name_start_pos = copy_header_name();
    /* Move past the header name. */
    (void)get_token();
    /* Ignore trailing junk on the line.  Do this before pushing the new file,
       so the error can be produced on the old line. */
    ignore_harmless_trailing_comment();
    /* Push the name and associated search directory onto the input stack,
       thus starting input from that file. */
    open_file_and_push_input_stack(name_start_pos, search_path,
                                   /*is_include_file=*/TRUE,
                                   is_system_include);
  }  /* if */
}  /* proc_include */


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
  /* The ULTRIX C compiler has trouble with the type of folded compile-time
     unsigned expressions, so we use a variable for this value. */
  a_line_number max_line_div_10 = MAX_LINE_NUMBER;
  max_line_div_10 /= (a_line_number)10;

  /* Any number should be scanned as a digit sequence. */
  exp_digit_sequence = TRUE;
  /* The syntax for the standard #line is (see 3.8.4):

     #line digit-sequence
     #line digit-sequence string-literal
     #line pp-tokens              <-- Must turn into one of the other forms.

     The syntax for the cpp-output form is:

     # digit-sequence string-literal kind
     # digit-sequence

     where kind is empty or is "1" or "2".
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
     and it looks like end of file to the error routines). */
  if (temp_line == 0) bad_line_number = TRUE;
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
  } else if (curr_token == tok_string_literal && *start_of_curr_token != 'L') {
    /* Check for "L" is to disallow wide string literals. */
    /* The file name is present.  Since the constant is unconverted, allocate
       space for the string, and copy it.  Note that the string can be
       empty, which is not really a problem. */
    temp_file = copy_header_name();
    /* Move past the string literal. */
    (void)get_token();
  } else {
    /* Error, expected file name. */
    syntax_error(ec_exp_file_name);
    some_error_in_curr_directive = TRUE;
    goto return_point;
  }  /* if */
  /* For the cpp-output form, ignore the third operand if it is present
     (it is "1" for entry into an include file, "2" for the first directive
     after exit from an include file, and missing otherwise). */
  if (cpp_output_form && curr_token == tok_digit_sequence) (void)get_token();
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
    record_end_of_source_file(curr_ise->assoc_il_file, curr_seq_number);
  }  /* if */
  /* The new entry is entered under the current file entry, whether that
     entry is for the primary source file, an include file, or a #line
     directive.  curr_ise->assoc_il_file points to the new entry.
     Note that curr_ise->assoc_actual_il_file is NOT changed; it remains
     pointing to the entry for the actual file being read. */
  record_start_of_source_file(curr_ise->assoc_actual_il_file,
                              (a_seq_number)curr_seq_number+1,
                              temp_line,
                              temp_file,
                              (char *)NULL,  /* Indicates #line entry. */
                              (char *)NULL,  /* Indicates #line entry. */
                              &(curr_ise->assoc_il_file),
                              /*is_system_include=*/FALSE);
  if (generate_pp_output) {
    /* Generate the line-identifying directive if necessary for preprocessing
       output.  Force out because cpp always puts one out. */
    gen_pp_line_info(' ', 1);
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
static char	*pp_dir_string_buffer = NULL;
			/* Not allocated on a per-file basis. */

static sizeof_t	pp_directive_string_length;
			/* Size of the string in the pp_directive buffer,
                           not including any null terminator. */

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
  pp_dir_string_buffer = realloc_general(pp_dir_string_buffer,
                                        size_pp_dir_string_buffer, new_size);
  size_pp_dir_string_buffer = new_size;
}  /* expand_pp_dir_string_buffer */


/*
Ensure that pp_dir_string_buffer has at least size_needed bytes in it.
If not, expand pp_dir_string_buffer by reallocating it.
*/
#define ensure_pp_dir_string_buffer_space(size_needed)                 \
{ if (size_pp_dir_string_buffer < size_needed) {                       \
    expand_pp_dir_string_buffer((sizeof_t)(size_needed));              \
  }  /* if */                                                          \
}  /* ensure_pp_dir_string_buffer_space */


static void convert_pp_directive_to_string(void)
/*
Scans the tokens that make up a preprocessing directive and converts them into
a single null terminated character string.  The string is constructed in
a dedicated buffer which is enlarged as needed to be able to contain
the entire string.  The tokens are scanned as preprocessing tokens.
*/
{
  a_boolean	any_white_space_skipped = FALSE;
  sizeof_t	pos_in_buffer = 0;

  db_enter(4, "convert_pp_directive_to_string");
  /* The current token is the identifier that indicates the pragma kind.
     This should be included in the generated string. */
  do {
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
  } while (curr_token != tok_newline);
  /* Add a null terminator.  We made sure there was room for it earlier. */
  pp_dir_string_buffer[pos_in_buffer] = '\0';
  pp_directive_string_length = pos_in_buffer;
  db_exit();
}  /* convert_pp_directive_to_string */


static void convert_pragma_to_string(a_pending_pragma_ptr          ppp,
				     a_pragma_kind_description_ptr pkdp)
/*
Scans the tokens that make up a pragma directive and converts them
into a single null terminated character string.  Once the entire
pragma has been scanned, a buffer of the appropriate size is allocated
in the file scope IL memory region and the pragma string is copied
there.
*/
{
  a_boolean	save_expand_macros;
  a_boolean	save_processing_C_code_in_pragma;
  a_boolean	save_do_string_literal_concatenation;
  a_boolean	save_fetch_pp_tokens;
  char		*il_string;

  /* Save the current value of the lexical scanning mode flags. */
  save_expand_macros = expand_macros;
  save_processing_C_code_in_pragma = processing_C_code_in_pragma;
  save_do_string_literal_concatenation = do_string_literal_concatenation;
  save_fetch_pp_tokens = fetch_pp_tokens;
  /* Set the new values. */
  expand_macros = pkdp->expand_macros;
  processing_C_code_in_pragma = pkdp->processing_C_code_in_pragma;
  do_string_literal_concatenation = processing_C_code_in_pragma;
  /* Restore the previous values. */
  expand_macros = save_expand_macros;
  processing_C_code_in_pragma = save_processing_C_code_in_pragma;
  do_string_literal_concatenation = save_do_string_literal_concatenation;
  fetch_pp_tokens = save_fetch_pp_tokens;
  /* We expect expand_macros and processing_C_code_in_pragma to be FALSE when
     building a string representation of the pragma. */
  check_assertion_str2(!expand_macros && !processing_C_code_in_pragma,
		       "convert_pp_directive_to_string:",
		       "invalid token scanning mode");
  fetch_pp_tokens = TRUE;
  convert_pp_directive_to_string();
  /* Allocate a block of file scope IL memory into which the string may
     be copied. */
  il_string = (char *)alloc_il(pp_directive_string_length + 1);
  (void)memcpy(il_string, pp_dir_string_buffer,
               size_t_arg(pp_directive_string_length));
  /* Add a null terminator. */
  il_string[pp_directive_string_length] = '\0';
  ppp->pragma_text = il_string;
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "Saved pragma string: '%s'\n", il_string);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* convert_pragma_to_string */


static void cache_pragma_tokens(a_pending_pragma_ptr          ppp,
				a_pragma_kind_description_ptr pkdp)
/*
Cache the tokens that make up a pragma directive.  The global variables
that determine the current lexical scanning mode are saved and reset
based on the information specified in the pragma description entry.
*/
{
  a_boolean	save_expand_macros;
  a_boolean	save_processing_C_code_in_pragma;
  a_boolean	save_do_string_literal_concatenation;
  a_boolean	save_fetch_pp_tokens;

  /* Save the current value of the lexical scanning mode flags. */
  save_expand_macros = expand_macros;
  save_processing_C_code_in_pragma = processing_C_code_in_pragma;
  save_do_string_literal_concatenation = do_string_literal_concatenation;
  save_fetch_pp_tokens = fetch_pp_tokens;
  /* Set the new values. */
  expand_macros = pkdp->expand_macros;
  processing_C_code_in_pragma = pkdp->processing_C_code_in_pragma;
  do_string_literal_concatenation = processing_C_code_in_pragma;
  fetch_pp_tokens = FALSE;
  /* Bypass the identifier that indicates the pragma kind. */
  (void)get_token();
  /* Cache the tokens until an end-of-line is found. */
  for (;;) {
    cache_curr_token(&ppp->token_cache);
    if (curr_token == tok_newline) break;
    (void)get_token();
  }  /* for */
  /* Terminate the token cache. */
  terminate_token_cache(&ppp->token_cache);
  /* Restore the previous values. */
  expand_macros = save_expand_macros;
  processing_C_code_in_pragma = save_processing_C_code_in_pragma;
  do_string_literal_concatenation = save_do_string_literal_concatenation;
  fetch_pp_tokens = save_fetch_pp_tokens;
}  /* cache_pragma_tokens */


static void proc_pragma(a_source_position *start_of_dir_position)
/*
Scan and process a #pragma directive.
*/
{
  a_boolean processed = FALSE;

  if (generate_pp_output) {
    /* Generating preprocessing output for some other compiler.  Pass the
       #pragma unchanged to output. */
    /* Look for the special case of "#pragma once".  This is different
       from other pragmas in that it must be handled in preprocessing. */
    if (get_token() == tok_identifier) {
      if (curr_id_is("once")) {
        /* This file should be included only once, and if it is #included
           again in the same compilation unit, the include should be skipped.
           Record this information in the input stack entry. */
        set_ifg_state(IFG_STATE_ONCE);
        curr_ise->include_history->pragma_once = TRUE;
        processed = TRUE;
      }  /* if */
    }  /* if */
    pass_directive_to_output();
  } else {
    /* Compiling.  Identify the pragma. */
    if (get_token() == tok_identifier) {
      a_pragma_kind_description_ptr	pkdp;
      a_source_position			id_position;
      /* Save the position of the start of the token(s) that identify
         the kind of pragma being processed. */
      id_position = pos_curr_token;
      /* Look for the special case of "#pragma once".  This is different
         from other pragmas in that it must be handled in preprocessing. */
      if (curr_id_is("once")) {
        /* This file should be included only once, and if it is #included
           again in the same compilation unit, the include should be skipped.
           Record this information in the input stack entry. */
        set_ifg_state(IFG_STATE_ONCE);
        curr_ise->include_history->pragma_once = TRUE;
	processed = TRUE;
      } else {
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
        if (pkdp != NULL) {
          /* Scan the pragma directive.  The pragma statement may be recorded
             as either a token cache or as a character string.  The character
             string representation is usually used for pragmas that are to
             be passed to the C or C++ generating back end, but may be used for
             other pragmas in which a character string is simpler to
             manipulate. */
          a_pending_pragma_ptr	ppp;
          processed = TRUE;
          ppp = alloc_pending_pragma(pkdp);
          ppp->id_position = id_position;
          ppp->pragma_position = *start_of_dir_position;
          if (pkdp->make_text_not_tokens) {
            convert_pragma_to_string(ppp, pkdp);
          } else {
            /* Cache the tokens that make up the pragma directive. */
            cache_pragma_tokens(ppp, pkdp);
          }  /* if */
          /* Add this pragma to the list of pragmas associated with the
             current token. */
          add_to_curr_token_pragma_list(ppp);
        }  /* if */
      }  /* if */
    }  /* if */
    if (!processed) {
      /* Unrecognized pragma, just ignore (this is required by the
         standard). */
      warning(ec_unrecognized_pragma);
      flush_to_newline();
    }  /* if */
  }  /* if */
}  /* proc_pragma */


static void proc_ident(void)
/*
Scan and process a #ident directive.
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
}  /* proc_ident */


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


static void pch_prefix_processing_for_pp_directive(a_pp_directive_kind kind,
                                                   a_source_position   *pos)
/*
Create a precompiled header prefix event for a preprocessing directive.
This is done by converting the tokens that follow the preprocessing
directive into a character string and calling the routine to add a
pch event.
*/
{
  /* Bypass the directive keyword. */
  (void)get_token();
  convert_pp_directive_to_string();
  if (kind == ppd_include) {
    /* Add a sequence point marker before and after include directives. */
    add_pch_event(pchek_sequence_marker, ppd_not_valid, (char *)NULL,
                  &null_source_position);
  }  /* if */
  add_pch_event(pchek_pp_directive, kind, pp_dir_string_buffer, pos);
  if (kind == ppd_include) {
    /* Add a sequence point marker before and after include directives. */
    add_pch_event(pchek_sequence_marker, ppd_not_valid, (char *)NULL,
                  &null_source_position);
  }  /* if */
}  /* pch_prefix_processing_for_pp_directive */


void pp_directive(void)
/*
The "#" of a preprocessor directive is the current character.  Scan and
execute the preprocessor directive.
*/
{
  /* Place to save current value of stop token set for later restoration. */
  a_stop_token_array 	save_stop_token_array;
  a_boolean	     	save_fetch_pp_tokens = fetch_pp_tokens;
  a_boolean	     	save_expand_macros = expand_macros;
  a_boolean          	save_do_string_literal_concatenation =
                                               do_string_literal_concatenation;
  a_source_position  	save_error_position;
  a_source_position  	start_of_dir_position;
  a_pp_directive_kind	dir_kind;

  db_enter(3, "pp_directive");

  /* Save the error position for later restoration because we may change
     it. */
  copy_source_position(error_position, save_error_position);
  /* Save the position of the beginning of the directive. */
  start_of_dir_position = pos_curr_token;
  in_preprocessing_directive = TRUE;
  fetch_pp_tokens = TRUE;
  expand_macros = FALSE;
  do_string_literal_concatenation = FALSE;
  /* Save and clear the list of tokens that will stop flushing on error, and
     put the newline token into it. */
  copy_stop_tokens(stop_token_array, save_stop_token_array);
  clear_stop_tokens();
  add_stop_token(tok_newline);
  /* Identify the keyword and go to the right processing routine. */
  dir_kind = identify_dir_keyword();
  if (!building_pch_prefix) {
    switch ((int)dir_kind) {
      case ppd_not_valid:
        error(ec_bad_pp_directive_keyword);
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
        proc_include();
        break;
      case ppd_define:
        proc_define();
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
      case ppd_pragma:
        proc_pragma(&start_of_dir_position);
        break;
      case ppd_ident:
        nonstandard_pp_directive();
        proc_ident();
        break;
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
      case ppd_null:
        /* Null directive -- ignore. */
        break;
      case ppd_linedef:
        /* A line-identifying directive (output from cpp); this is similar
           to a #line directive, but not exactly the same. */
        nonstandard_pp_directive();
        proc_line(/*cpp_output_form=*/TRUE);
        break;
#if CHECKING
      default:
        internal_error("pp_directive: bad pp directive code");
        break;
#endif /* CHECKING */
    }  /* switch */
    /* If some other preprocessing directive is seen outside of the
       #ifndef/#endif guard code of the current file then it is not a
       candidate for suppression of a subsequent include. */
    switch (dir_kind) {
      case ppd_ifdef:
      case ppd_ifndef:
      case ppd_else:
      case ppd_endif:
      case ppd_include:
        /* Processing for these directives is done in the specific routines
           called above. */
        break;
      default:
        {
          a_byte	ifg_state = get_ifg_state();
          if (ifg_state < IFG_STATE_FAIL)
            ifg_state = IFG_STATE_FAIL;
          }  /* if */
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
  /* Restore the stop token set as at entry. */
  copy_stop_tokens(save_stop_token_array, stop_token_array);
  in_preprocessing_directive = FALSE;
  fetch_pp_tokens = save_fetch_pp_tokens;
  expand_macros = save_expand_macros;
  do_string_literal_concatenation = save_do_string_literal_concatenation;
  /* Restore the error position as at entry. */
  copy_source_position(save_error_position, error_position);

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
  if (f_pp_output != NULL) {
    /* Check for errors in writing the pp output file, then close it. */
    if (fflush(f_pp_output) || ferror(f_pp_output) ||
        (f_pp_output != stdout && fclose(f_pp_output))) {
      str_catastrophe(ec_file_write_error, "preprocessing output");
    }  /* if */
  }  /* if */
}  /* cpp_driver */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
