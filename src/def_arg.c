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

def_arg.c -- Processing of default arguments

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


/* Previously allocated fixup entries available for reuse. */
static a_def_arg_expr_fixup_ptr avail_def_arg_expr_fixup;


#if DEBUG
/*
Counter to track use of memory.
*/
static unsigned long
		num_def_arg_expr_fixups_allocated;

unsigned long db_show_def_arg_expr_fixups_used(unsigned long grand_total)
/*
Display the amount of space used for allocation of default argument fixup
entries, for debugging purposes.  Also return the total amount.
*/
{
  unsigned long  num, size, total;

  db_space_used_lost("def arg expr fixups", avail_def_arg_expr_fixup,
                     num_def_arg_expr_fixups_allocated, a_def_arg_expr_fixup);
  return grand_total;
}  /* db_show_routine_fixups_used */

#endif /* DEBUG */

static a_def_arg_expr_fixup_ptr alloc_def_arg_expr_fixup(void)
/*
Allocate (or take from the available-list) a default arg expression fixup
entry and initialize it.
*/
{
  a_def_arg_expr_fixup_ptr  daefp;
  
  if (avail_def_arg_expr_fixup != NULL) {
    /* Reuse a previously allocated entity. */
    daefp = avail_def_arg_expr_fixup;
    avail_def_arg_expr_fixup = daefp->next;
  } else {
    /* Allocate memory for a new entity. */
    daefp = (a_def_arg_expr_fixup_ptr)alloc_fe(sizeof(a_def_arg_expr_fixup));
#if DEBUG
    num_def_arg_expr_fixups_allocated++;
#endif /* DEBUG */
  }  /* if */
  /* Clear the entity. */
  daefp->next = NULL;
  daefp->param_type = NULL;
  clear_template_cache(&daefp->cache, /*reusable=*/FALSE);
  daefp->param_number = 0;

  return daefp;
}  /* alloc_def_arg_expr_fixup */


a_def_arg_expr_fixup_ptr copy_def_arg_expr_fixup_list(
				a_def_arg_expr_fixup_ptr	orig_list)
/*
Make a copy of the default argument fixup list specified by orig_list.
Return a pointer to the new list.
*/
{
  a_def_arg_expr_fixup_ptr	new_list = NULL;
  a_def_arg_expr_fixup_ptr	new_tail = NULL;
  a_def_arg_expr_fixup_ptr	daefp;
  a_def_arg_expr_fixup_ptr	new_daefp;

  for (daefp = orig_list; daefp != NULL; daefp = daefp->next) {
    new_daefp = alloc_def_arg_expr_fixup();
    *new_daefp = *daefp;
    new_daefp->next = NULL;
    if (new_list == NULL) new_list = new_daefp;
    if (new_tail != NULL) new_tail->next = new_daefp;
    new_tail = new_daefp;
  }  /* for */
  return new_list;
}  /* copy_def_arg_expr_fixup_list */


void free_def_arg_expr_fixup(a_def_arg_expr_fixup_ptr  daefp)
/*
Return a default arg expr fixup entry, and any others chained to it, to the
available-list.
*/
{
  if (daefp != NULL) {
    free_def_arg_expr_fixup(daefp->next);
    daefp->next = avail_def_arg_expr_fixup;
    avail_def_arg_expr_fixup = daefp;
  }  /* if */
}  /* free_def_arg_expr_fixup */


void prescan_default_arg_expr(a_token_cache_ptr		token_cache,
			      a_boolean			is_template_param,
			      a_boolean			is_function_template,
			      a_boolean			is_friend_decl,
                              a_token_cache_ptr		src_cache)
/*
Place the tokens for a default argument expression into a token cache, to
await actual processing at a later point.  src_cache points to a token
cache containing the tokens that will be copied to token_cache.
is_template_param is TRUE if the expression is the default for a nontype
template parameter.  is_function_template is TRUE if the expression is the
default for a function template function parameter.  is_friend_decl is
TRUE if the expression is the default for a friend declaration; it must
be FALSE when either is_function_template or is_template_param are FALSE.
*/
{
  a_token_set_array		stop_tokens;
  a_token_sequence_number	first_tsn;
  a_token_sequence_number	last_tsn;
  a_scope_stack_entry_ptr	ssep;

  db_enter(3, "prescan_default_arg_expr");
  /* Initialize a local stop token set. */
  clear_token_set_array(stop_tokens);
  /* Save the token sequence number of the first token to be cached. */
  first_tsn = curr_token_sequence_number;
  /* In the normal case we will scan an expression and encounter a comma
     or right parenthesis.  If both of these are omitted, terminate the token
     stream when some likely delimiter is reached. */
  incr_token_set_array_element(stop_tokens, tok_comma);
  incr_token_set_array_element(stop_tokens, tok_ellipsis);
  incr_token_set_array_element(stop_tokens, tok_rparen);
  incr_token_set_array_element(stop_tokens, tok_semicolon);
  incr_token_set_array_element(stop_tokens, tok_lbrace);
  incr_token_set_array_element(stop_tokens, tok_rbrace);
  /* When scanning a template default argument add ">" to the stop tokens. */
  if (is_template_param) {
    incr_token_set_array_element(stop_tokens, tok_gt);
  }  /* if */
  clear_token_cache(token_cache, /*reusable=*/TRUE);
  cache_token_stream_coalesce_identifiers(token_cache, stop_tokens,
                                          src_cache);
  /* Save the token sequence number of the last token of the default
     argument.  The cache actually contains the token after the last one
     of the default argument, but that token should not be used as the
     last token of the cache segment. */
  last_tsn = curr_token_sequence_number - 1;
  ssep = &scope_stack[depth_scope_stack];
  if (!is_template_param &&
      ((ssep->in_prototype_instantiation && !is_friend_decl) ||
       (is_function_template &&
        depth_innermost_instantiation_scope == NO_SCOPE_DEPTH))) {
    /* This is a function default argument within a prototype instantiation
       or in a template declaration.  Save the token numbers associated with
       this default argument so that it can be removed from the cache later. */
    a_template_cache_segment_ptr	tcsp;
    tcsp = alloc_template_cache_segment(
                   (a_symbol_ptr)NULL, (a_template_symbol_supplement_ptr)NULL);
    tcsp->first_token_number = first_tsn;
    /* When there is no default, the computed last token number could be
       less that the first.  In that case, use the first token number as
       the last. */
    tcsp->last_token_number = last_tsn < first_tsn ? first_tsn : last_tsn;
    tcsp->is_default_arg = TRUE;
    /* Check for the case where the cache is empty. */
    tcsp->default_arg_missing = token_cache->first_token == NULL;
  }  /* if */
  /* Note that the terminating token (comma, rparen, etc.) is not added to
     the cache. */
  terminate_token_cache(token_cache);
  db_exit();
}  /* prescan_default_arg_expr */


void prescan_default_function_arg_expr(
			a_param_type_ptr		ptp,
		        a_def_arg_expr_fixup_ptr	*list,
                        a_token_cache_ptr		src_cache,
			a_boolean			is_function_template,
			a_boolean			is_friend_decl,
			unsigned long			param_number)
/*
Place the tokens for a default argument expression into a token cache, to
await actual processing at a later point.  Link the default argument
entry onto the list provided by the caller.  src_cache points to
a token cache containing the entire template declaration, of which
this default argument is a part.  param_number specifies the position of
the parameter in the parameter list.  If list or ptp is NULL, scan the
default argument expression but discard the token cache.
*/
{
  a_def_arg_expr_fixup_ptr  new_daefp, daefp;
  a_token_cache             token_cache;

  db_enter(3, "prescan_default_function_arg_expr");
  /* Scan the default argument expression. */
  prescan_default_arg_expr(&token_cache, /*is_template_param=*/FALSE,
                           is_function_template, is_friend_decl, src_cache);
  if (list == NULL || ptp == NULL) {
    /* Either no list pointer, or no param type pointer was passed by
       the caller.  This indicates that the argument information should
       simply be discarded. */
    discard_token_cache(&token_cache);
  } else {
    /* Allocate a default arg expr fixup entry. */
    new_daefp = alloc_def_arg_expr_fixup();
    new_daefp->param_type = ptp;
    new_daefp->cache.tokens = token_cache;
    new_daefp->param_number = param_number;
    /* Add the entry to the end of the list of default arg expr fixup entries
       for the current routine fixup. */
    if (*list == NULL) {
      *list = new_daefp;
    } else {
      daefp = *list;
      while (daefp->next != NULL) daefp = daefp->next;
      daefp->next = new_daefp;
    }  /* if */
  }  /* if */
  db_exit();
}  /* prescan_default_function_arg_expr */


void delayed_scan_of_default_arg_expr(a_param_type_ptr param_type_entry,
                                      a_boolean	       check_for_errors)
/*
Do the delayed scan of the default argument expression for a parameter.  The
cache has just been reactivated, so curr_token should represent the first
token in the cache.  If check_for_errors is TRUE, then before doing the scan
check that default expressions have been declared for all successor arguments.

The error checks are suppressed when this routine is called for a
function template because the checks are done elsewhere (and cannot
be done here because the default arguments for a given function template
may be spread between several declarations).
*/
{
  a_param_type_ptr  ptp;
  a_boolean         err = FALSE;

  db_enter(3, "delayed_scan_of_default_arg_expr");
  if (param_type_entry->default_arg_expr != NULL &&
      !is_error_node(param_type_entry->default_arg_expr)) {
    pos_error(ec_default_arg_already_defined, &pos_curr_token);
  }  /* if */
  if (check_for_errors) {
    /* Make a pass over all the param type entries that follow the current one.
       It is an error if there are any without a default argument. */
   for (ptp = param_type_entry->next; ptp != NULL; ptp = ptp->next) {
     if (!ptp->has_default_arg) {
        /* Issue an error on the first successor in the parameter list that
           does not have a default argument. */
        if (!err) {
          pos_error(ec_default_arg_not_at_end, &pos_curr_token);
          err = TRUE;
        }  /* if */
        ptp->has_default_arg = TRUE;
        ptp->default_arg_expr = error_node();
      }  /* if */
    }  /* for */
  }  /* if */
  /* We scan the expression whether an error was detected or not. */
  scan_default_arg_expr(param_type_entry);
  /* In the normal case the current token should be end_of_source,
     which was inserted to mark the end of the cached token
     stream. */
  if (curr_token != tok_end_of_source) {
    pos_error(ec_unexpected_end_of_default_arg, &pos_curr_token);
    /* If necessary, keep flushing until end-of-source is found. */
    while (curr_token != tok_end_of_source) (void)get_token();
  }  /* if */
  /* Advance past the end-of-source token, which was added in
     the prescan routine. */
  (void)get_token();
  db_exit();
}  /* delayed_scan_of_default_arg_expr */


static void check_for_valid_end_of_template_def_arg(void)
/*
We have just scanned the default for a template argument.  We
should now be at the tok_end_of_source that terminates the cache.
*/
{
  /* In the normal case the current token should be end_of_source,
     which was inserted to mark the end of the cached token
     stream. */
  if (curr_token != tok_end_of_source) {
    pos_error(ec_exp_comma, &pos_curr_token);
    /* If necessary, keep flushing until end-of-source is found. */
    while (curr_token != tok_end_of_source) (void)get_token();
  }  /* if */
  /* Advance past the end-of-source token, which was added in
     the prescan routine. */
  (void)get_token();
}  /* check_for_valid_end_of_template_def_arg */


void delayed_scan_of_template_default_arg_expr(a_type_ptr	type,
					       a_constant_ptr   constant)
/*
Do the delayed scan of the default argument expression for a template
parameter.  The cache has just been reactivated, so curr_token should
represent the first token in the cache.
*/
{
  db_enter(3, "delayed_scan_of_template_default_arg_expr");
  scan_template_argument_constant_expression(type, constant);
  check_for_valid_end_of_template_def_arg();
  db_exit();
}  /* delayed_scan_of_template_default_arg_expr */


a_type_ptr delayed_scan_of_template_default_type_arg(void)
/*
Do the delayed scan of the default argument expression for a template
type parameter.  The cache has just been reactivated, so curr_token should
represent the first token in the cache.  Return a pointer to the type
that was scanned.
*/
{
  a_type_ptr	tp = NULL;

  db_enter(3, "delayed_scan_of_template_default_type_arg");
  type_name(&tp);
  check_for_valid_end_of_template_def_arg();
  db_exit();
  return tp;
}  /* delayed_scan_of_template_default_type_arg */


a_template_ptr delayed_scan_of_template_default_template_arg(
				a_template_ptr		param_template,
				a_source_position	*err_pos)
/*
Do the delayed scan of the default argument expression for a template
template parameter.  The cache has just been reactivated, so curr_token should
represent the first token in the cache.  Return a pointer to the template
that was scanned.
*/
{
  a_template_ptr	templ;

  db_enter(3, "delayed_scan_of_template_default_template_arg");
  templ = scan_template_template_argument(param_template, err_pos);
  check_for_valid_end_of_template_def_arg();
  db_exit();
  return templ;
}  /* delayed_scan_of_template_default_template_arg */


void def_arg_one_time_init(void)
/*
One-time initialization for def_arg.c static variables.
*/
{
  /* Save variables that are needed for precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(avail_def_arg_expr_fixup),
#if DEBUG
      pch_saved_var_array_elem(num_def_arg_expr_fixups_allocated),
#endif /* if DEBUG */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
}  /* def_arg_one_time_init */


void def_arg_init(void)
/*
Initializations for class declaration processing.
*/
{
  /* Initialize the list of freed delayed-scan-fixup entries. */
  avail_def_arg_expr_fixup = NULL;
#if DEBUG
  num_def_arg_expr_fixups_allocated = 0;
#endif /* DEBUG */
  return;
}  /* def_arg_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
