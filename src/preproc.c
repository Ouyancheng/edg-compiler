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
       Error except when emulating pcc, which doesn't give an error. */
    if (C_dialect != C_dialect_pcc
#if MICROSOFT_EXTENSIONS_ALLOWED
        /* The Microsoft compiler doesn't give an error on this either. */
        && !microsoft_mode
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                  ) {
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
    /* The identifier __VA_ARGS__ is not allowed if variadic macros are
       accepted. */
    check_use_of_VA_ARGS(len_of_curr_token, start_of_curr_token);
    /* Look to see if there is a macro with this name. */
    sym_hdr = find_symbol_header(start_of_curr_token, len_of_curr_token,
                                 &locator_for_curr_id);
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
    /* The identifier __VA_ARGS__ is not allowed if variadic macros are
       accepted. */
    check_use_of_VA_ARGS(len_of_curr_token, start_of_curr_token);
    /* Look to see if there is a macro with this name. */
    /* find_defined_macro cannot be used because if we have "#undef defined"
       we want to give an error, not ignore it. */
    assoc_symbol = find_macro_symbol_by_name(start_of_curr_token,
                                             len_of_curr_token,
	                                     &locator_for_curr_id);
    if (assoc_symbol == NULL) {
      /* No such macro, so #undef is ignored. */
    } else if (assoc_symbol->variant.macro_def->cannot_be_redefined &&
               !microsoft_mode) {
      /* The macro is predefined. */
      diagnostic(es_discretionary_error, ec_cannot_undef_predef_macro);
    } else {
      if (assoc_symbol->variant.macro_def->cannot_be_redefined) {
        /* The Microsoft compiler gives a warning for a case like this.
           The warning says the #undef is ignored, but it isn't. */
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
  }  /* if */
  return (curr_token == tok_header_name);
}  /* get_header_name */


static void trim_leading_and_trailing_blanks_from_header_name(char      **name,
                                                              sizeof_t  *len)
/*
The header name consists of *len characters starting at *name.  This routine
modifies those quantities to trim leading and trailing whitespace.
E.g., "    stdio   " becomes "stdio".
*/
{
  /* Skip leading whitespace. */
  while (**name == ' ' || **name == '\t') {
    ++(*name);
    --(*len);
  }  /* while */
  /* Trim trailing whitespace. */
  while ((*name)[*len - 1] == ' ' || (*name)[*len - 1] == '\t') {
    --(*len);
  }  /* while */
}  /* trim_leading_and_trailing_blanks_from_header_name */


static char *copy_header_name(a_boolean process_escapes)
/*
Allocate and copy the file name from the current token (a header name).
Escapes in the string are processed only if process_escapes is TRUE.
*/
{
  char		*name_start_pos, *in_pos, *out_pos;
  sizeof_t	name_len, i;
  int           remaining_mbc_char_count = 0;
  unsigned long ch;
  unsigned long centity_mask;

  /* Build a mask used to mask individual characters. */
  centity_mask = (unsigned long)1 << (targ_host_string_char_bit-1);
  centity_mask = centity_mask | (centity_mask-1);
  name_start_pos = alloc_primary_file_scope_il((sizeof_t)(
           (name_len = len_of_curr_token - 2 /* Drop quoting characters. */)
           + 1 /* Space for null. */));
  in_pos = start_of_curr_token+1;
  if (microsoft_mode && *start_of_curr_token == '<') {
    /* Microsoft compilers ignore leading and trailing whitespace inside
       #include <...> directives. */
    trim_leading_and_trailing_blanks_from_header_name(&in_pos, &name_len);
  }  /* if */
  out_pos = name_start_pos;
  /* Copy the string, processing escapes if appropriate.  Note that space
     including unprocessed escapes was allocated in the output string, so
     there may be a bit of wasted space. */
  for (i = 1; i <= name_len; i++) {
    check_assertion_str(*in_pos != LE_ESCAPE,
                        "copy_header_name: lexical escape in header name");
    if (process_escapes) {
      /* Process the character, considering escape characters. */
      char *prev_pos = in_pos;
      conv_single_char(&in_pos, &remaining_mbc_char_count, &ch, centity_mask);
      i += (in_pos - prev_pos) - 1;
      *out_pos++ = (char)ch;
    } else {
      /* Escapes should not be considered; just copy one character. */
      *out_pos++ = *in_pos++;
    }  /* if */
  }  /* for */
  *out_pos = '\0';
  return name_start_pos;
}  /* copy_header_name */


static void proc_stdarg_include(void)
/*
Process an #include of <stdarg.h> by creating definitions for the things
the header defines instead of reading the header file.  This is used when
we want to pass references to the <stdarg.h> macros through to the output,
e.g., in generated C code.
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
    /* Declare va_list as a type of "void *". */
    declare_builtin_va_list_type();
    if (generate_pp_output) {
      pass_directive_to_output();
    }  /* if */
  }  /* if */
}  /* proc_stdarg_include */


static void proc_include(a_boolean is_include_next)
/*
Scan and process a #include directive.  If is_include_next is TRUE, the
directive is a #include_next (a gcc extension that begins the search for
the file in the directory on the search path that follows the directory
in which the current file was found).
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
    is_system_include = *start_of_curr_token == '<';
    /* Allocate space for and copy the name. */
    /* Escapes are not processed.  That's an implementation choice; you
       can change this if you'd rather have it the other way.  (But note
       that Microsoft compatibility requires ignoring the escapes, because
       "\" can be used in file names.) */
    name_start_pos = copy_header_name(/*process_escapes=*/FALSE);
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
         (!C_mode() && strcmp(name_start_pos, "cstdarg") == 0))) {
      /* Instead or reading the <stdarg.h> or <cstdarg> header file, create
         builtin definitions for the things it's known to define. */
      proc_stdarg_include();
      /* Check whether a PCH file should be generated at the end of the
         execution of this include directive. */
      check_for_generation_of_pch_on_return_to_primary_file();
    } else {
      /* Push the name and associated search directory onto the input stack,
         thus starting input from that file. */
      open_file_and_push_input_stack(name_start_pos,
                                     /*use_search_path=*/TRUE,
                                     /*is_include_file=*/TRUE,
                                     is_system_include,
                                     /*is_preinclude=*/FALSE,
			             /*preinclude_macros=*/FALSE,
                                     /*is_implicit_include=*/FALSE,
                                     is_include_next);
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
                                   /*is_include_next=*/FALSE);
  }  /* if */
}  /* proc_import */

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
                                (a_boolean)actual_sfp->
                                                      from_system_include_dir);
  }
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
{ if (size_pp_dir_string_buffer < (size_needed)) {                     \
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
  while (curr_token != tok_newline && curr_token != tok_end_of_source) {
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
  a_boolean	save_do_string_literal_concatenation;
  a_boolean	save_fetch_pp_tokens;
  char		*il_string;

  db_enter(4, "convert_pragma_to_string");
  /* Save the current value of the lexical scanning mode flags. */
  save_expand_macros = expand_macros;
  save_do_string_literal_concatenation = do_string_literal_concatenation;
  save_fetch_pp_tokens = fetch_pp_tokens;
  /* Set the new values. */
  expand_macros = pkdp->expand_macros;
  do_string_literal_concatenation = FALSE;
  fetch_pp_tokens = TRUE;
  /* We expect caching_pragma_tokens to be FALSE when building a string
     representation of the pragma. */
  check_assertion_str2(!caching_pragma_tokens,
		       "convert_pp_directive_to_string:",
		       "invalid token scanning mode");
  convert_pp_directive_to_string();
  /* Allocate a block of file scope IL memory into which the string may
     be copied. */
  il_string = (char *)alloc_primary_file_scope_il(
                                               pp_directive_string_length + 1);
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
  /* Restore the previous values. */
  expand_macros = save_expand_macros;
  do_string_literal_concatenation = save_do_string_literal_concatenation;
  fetch_pp_tokens = save_fetch_pp_tokens;
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
  a_boolean	save_caching_pragma_tokens;
  a_boolean	save_do_string_literal_concatenation;
  a_boolean	save_fetch_pp_tokens;
  a_boolean     save_recognize_keywords_in_pragma;

  /* Save the current value of the lexical scanning mode flags. */
  save_expand_macros = expand_macros;
  save_caching_pragma_tokens = caching_pragma_tokens;
  save_do_string_literal_concatenation = do_string_literal_concatenation;
  save_fetch_pp_tokens = fetch_pp_tokens;
  save_recognize_keywords_in_pragma = recognize_keywords_in_pragma;
  /* Set the new values. */
  expand_macros = pkdp->expand_macros;
  caching_pragma_tokens = TRUE;
  recognize_keywords_in_pragma = pkdp->processing_C_code;
  do_string_literal_concatenation = pkdp->processing_C_code;
  fetch_pp_tokens = FALSE;
  /* Bypass the identifier that indicates the pragma kind. */
  (void)get_token();
  /* Cache the tokens until an end-of-line is found. */
  for (;;) {
    if (curr_token == tok_newline || curr_token == tok_end_of_source) break;
    cache_curr_token(&ppp->token_cache);
    (void)get_token();
  }  /* for */
  /* Terminate the token cache. */
  terminate_token_cache(&ppp->token_cache);
  /* Restore the previous values. */
  expand_macros = save_expand_macros;
  caching_pragma_tokens = save_caching_pragma_tokens;
  do_string_literal_concatenation = save_do_string_literal_concatenation;
  fetch_pp_tokens = save_fetch_pp_tokens;
  recognize_keywords_in_pragma = save_recognize_keywords_in_pragma;
}  /* cache_pragma_tokens */


static void enter_pending_pragma(a_pragma_kind_description_ptr  pkdp,
                                 a_source_position              *directive_pos,
                                 a_source_position              *id_pos)
/*
Scan the current pragma directive, which has already been determined to be
of a kind associated with the entry pointed to pkdp.  It may be recorded as
either a token cache or as a character string.  *directive_pos is the source
position of the start of the directive; *id_pos is the source position of
the pragma identifier.
*/
{
  a_pending_pragma_ptr	ppp;

  ppp = alloc_pending_pragma(pkdp);
  ppp->id_position = *id_pos;
  ppp->pragma_position = *directive_pos;
  if (pkdp->make_text_not_tokens) {
    /*  The character string representation is usually used for pragmas that
        are to be passed to the C or C++ generating back end, but may be
        used for other pragmas in which a character string is simpler to
        manipulate. */
    convert_pragma_to_string(ppp, pkdp);
  } else {
    /* Cache the tokens that make up the pragma directive. */
    cache_pragma_tokens(ppp, pkdp);
  }  /* if */
  /* Add this pragma to the list of pragmas associated with the
     current token. */
  add_to_curr_token_pragma_list(ppp);
}  /* enter_pending_pragma */


/*ARGSUSED*/ /* <-- kind is not used. */
void once_pragma(a_pragma_kind kind)
/*
Process a "#pragma once" directive.  This directive indicate that
this file should be included only once, and if it is #included
again in the same compilation unit, the include should be skipped.
Record this information in the input stack entry.
*/
{
  set_ifg_state(IFG_STATE_ONCE);
  curr_ise->include_history->pragma_once = TRUE;
  /* Bypass the "once" token. */
  (void)get_token();
}  /* once_pragma */


/*ARGSUSED*/ /* <-- kind is not used. */
void hdrstop_or_no_pch_pragma(a_pragma_kind kind)
/*
A PCH control pragma.  The actual processing of these
pragmas is handled in the special prefix processing code for
preprocessing directives.  When they are encountered during a
real compilation, they should just be ignored.
*/
{
  while (curr_token != tok_newline) (void)get_token();
}  /* hdrstop_or_no_pch_pragma */


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
    fetch_pp_tokens = FALSE;
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


void record_pragma(a_pragma_kind_description_ptr	pkdp,
		   a_source_position			*start_of_dir_position,
		   a_source_position			*id_position)
/*
Record the pragma whose kind is specified by pkdp (which may be NULL).
start_of_dir_position is the position of the first character of the
pragma directive.  id_position is the position of the pragma identifier.
*/
{
  a_boolean processed = FALSE;
  if (pkdp != NULL) {
    if (pkdp->binding_kind == pbk_preproc_immediate) {
      /* Preprocessing immediate pragmas are processed when
         encountered.  Call the processing routine associated with
         this pragma. */
      a_preproc_immediate_pragma_function_ptr pipfp;
      pipfp = pkdp->variant.preproc_immediate_processing_function;
      if (pipfp != NULL) (*pipfp)(pkdp->kind);
    } else {
      /* Scan the pragma directive, recording it as either a token cache
         or as a character string. */
      enter_pending_pragma(pkdp, start_of_dir_position, id_position);
    }  /* if */
    processed = TRUE;
  }  /* if */
  if (!processed) {
    /* Unrecognized pragma, just ignore (this is required by the
       standard). */
    pos_warning(ec_unrecognized_pragma, id_position);
    flush_to_newline();
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

  /* Look up the identifier that specifies the kind of pragma. */
  pkdp = look_up_pragma_id(&id_position);
  if (generate_pp_output && do_preprocessing_only) {
    /* Generating preprocessing output for some other compiler.  Pass the
       #pragma to the output.  The information in the pragma description is
       used to determine how the tokens of the pragma should be processed
       (e.g., should macros be expanded). */
    /* Look for the special case of "#pragma once".  This is different
       from other pragmas in that it must be handled in preprocessing
       even when only generating a preprocessed output file. */
    if (pkdp != NULL && pkdp->kind == (a_pragma_kind)pk_once) {
      /* This file should be included only once, and if it is #included
         again in the same compilation unit, the include should be skipped.
         Record this information in the input stack entry. */
      once_pragma((a_pragma_kind)pk_once);
    }  /* if */
    pass_pragma_to_output(pkdp);
  } else {
    /* Compiling.  Record the pragma for later processing, or for
       processing now in the case of immediate pragmas. */
    record_pragma(pkdp, start_of_dir_position, &id_position);
    if (generate_pp_output) {
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
                         directive_pos, &pos_curr_token);
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
  if (curr_token != tok_string_literal ||
      is_error_constant(&const_for_curr_token) ||
      char_int_kind_from_string_type(const_for_curr_token.type) !=
                                                 plain_char_int_kind) {
    error(ec_bad_ident_string);
    err = TRUE;
  } else {
    switch_to_file_scope_region(&region_to_switch_back_to);
    cp = alloc_unshared_constant(&const_for_curr_token);
    switch_back_to_original_region(region_to_switch_back_to);
    (void)get_token();
  }  /* if */
  wrapup_rescan_of_pragma_tokens(err);
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

This routine is called to process the pragmas when they are known to appear
in a valid location.  It is called from compound_statement for block scope
pragmas, and by stdc_pragma for pragmas that appear in the file scope.
*/
{
  a_stdc_pragma_kind	kind = (a_stdc_pragma_kind)stdc_pk_none;
  a_stdc_pragma_value	value = (a_stdc_pragma_value)stdc_pv_none;
  a_boolean		err = FALSE;
  char			*str;
  a_stdc_pragma_value	*state_var_ptr;

  begin_rescan_of_pragma_tokens(ppp);
  if (curr_token == tok_identifier) {
    str = locator_for_curr_id.symbol_header->identifier;
    if (strcmp(str, "FP_CONTRACT") == 0) {
      kind = (a_stdc_pragma_kind)stdc_pk_fp_contract;
      state_var_ptr = &curr_fp_contract_state;
    } else if (strcmp(str, "FENV_ACCESS") == 0) {
      kind = (a_stdc_pragma_kind)stdc_pk_fenv_access;
      state_var_ptr = &curr_fenv_access_state;
    } else if (strcmp(str, "CX_LIMITED_RANGE") == 0) {
      kind = (a_stdc_pragma_kind)stdc_pk_cx_limited_range;
      state_var_ptr = &curr_cx_limited_range_state;
    }  /* if */
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
      if (strcmp(str, "ON") == 0) {
        value = (a_stdc_pragma_value)stdc_pv_on;
      } else if (strcmp(str, "OFF") == 0) {
        value = (a_stdc_pragma_value)stdc_pv_off;
      } else if (strcmp(str, "DEFAULT") == 0) {
        value = (a_stdc_pragma_value)stdc_pv_default;
      }  /* if */
    }  /* if */
    if (value == (a_stdc_pragma_value)(a_stdc_pragma_value)stdc_pv_none) {
      diagnostic(strict_ansi_error_severity, ec_bad_stdc_pragma_arg);
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
If there are any current token pragmas that are C99 predefined
pragmas, process them now.
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
      convert_pp_directive_to_string();
      add_pch_event(pchek_pp_directive, kind, pp_dir_string_buffer, pos,
                    actual_line);
    }  /* if */
  }  /* if */
}  /* pch_prefix_processing_for_pp_directive */


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
  a_source_position  	start_of_dir_position;
  a_pp_directive_kind	dir_kind;
  a_boolean		local_is_header_stop_dir;

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
  /* Start a new stop token context that will stop flushing on error, and
     put the newline token into it. */
  push_stop_token_stack();
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
        proc_include(/*is_include_next=*/FALSE);
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
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      case ppd_include_next:
        nonstandard_pp_directive();
        proc_include(/*is_include_next=*/TRUE);
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
  /* Restore the stop token set as at entry. */
  pop_stop_token_stack();
  in_preprocessing_directive = FALSE;
  fetch_pp_tokens = save_fetch_pp_tokens;
  expand_macros = save_expand_macros;
  do_string_literal_concatenation = save_do_string_literal_concatenation;
  /* Restore the error position as at entry. */
  copy_source_position(save_error_position, error_position);
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


void process_macro_preinclude(void)
/*
When the preinclude_macros option is used, scan and discard any tokens
until the end of the preinclude file is reached.
*/
{
  for (;;) {
    if (get_token() == tok_end_of_source) break;
  }  /* for */
  pop_input_stack();
}  /* process_macro_preinclude */


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
  if (is_macro_preinclude) process_macro_preinclude();
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


void preproc_one_time_init(void)
/*
One-time initialization for preproc.c and preproc.h variables.
*/
{
  /* Global variables declared in preproc.h. */
  size_pp_dir_string_buffer = 0;
  /* Static variables declared in this file. */
  pp_if_stack = NULL;
  size_pp_if_stack = 0;
  pp_dir_string_buffer = NULL;
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
  caching_pragma_tokens = FALSE;
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
}  /* preproc_trans_unit_init */


void preproc_init(void)
/*
Initialize things related to preprocessing that must be initialized
for each compilation.  (Predefined macros are established by 
init_predefined_macros.)
*/
{
}  /* preproc_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
