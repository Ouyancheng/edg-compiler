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

#include "basics.h"
#include "def_arg.h"
#include "class_decl.h"
#include "debug.h"
#include "mem_manage.h"
#include "il.h"
#include "symbol_tbl.h"


/* Previously allocated fixup entries available for reuse. */
static a_def_arg_expr_fixup_ptr avail_def_arg_expr_fixup;


#if DEBUG
/*
Counter to track use of memory.
*/
static unsigned long
		num_def_arg_expr_fixups_allocated;

unsigned long db_show_def_arg_expr_fixups_used(unsigned long grand_total)
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
  clear_token_cache(&daefp->token_cache, /*reusable=*/FALSE);

  return daefp;
}  /* alloc_def_arg_expr_fixup */


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


void prescan_default_arg_expr(a_token_cache	*token_cache,
			      a_boolean		is_template_param)
/*
Place the tokens for a default argument expression into a token cache, to
await actual processing at a later point.
*/
{
  a_token_set_array  stop_tokens;

  db_enter(3, "prescan_default_arg_expr");
  clear_token_cache(token_cache, /*reusable=*/TRUE);
  /* Initialize a local stop token set. */
  clear_token_set_array(stop_tokens);
  /* In the normal case we will scan an expression and encounter a comma
     or right parenthesis.  If both of these are omitted, terminate the token
     stream when some likely delimiter is reached. */
  incr_token_set_array_element(stop_tokens, tok_comma);
  incr_token_set_array_element(stop_tokens, tok_rparen);
  incr_token_set_array_element(stop_tokens, tok_semicolon);
  incr_token_set_array_element(stop_tokens, tok_lbrace);
  incr_token_set_array_element(stop_tokens, tok_rbrace);
  /* When scanning a template default argument add ">" to the stop tokens. */
  if (is_template_param) {
    incr_token_set_array_element(stop_tokens, tok_gt);
  }  /* if */
  cache_token_stream(token_cache, stop_tokens);
  /* Note that the terminating token (comma, rparen, etc.) is not added to
     the cache. */
  /* Add an end-of-source token to the end of the token cache.  This assures
     that we won't scan past the end of the cache in the actual scan. */
  terminate_token_cache(token_cache);
  db_exit();
}  /* prescan_default_arg_expr */


void prescan_default_function_arg_expr(a_param_type_ptr 	ptp,
			               a_def_arg_expr_fixup_ptr	*list)
/*
Place the tokens for a default argument expression into a token cache, to
await actual processing at a later point.  Link the default argument
entry onto the list provided by the caller.
*/
{
  a_def_arg_expr_fixup_ptr  new_daefp, daefp;
  a_token_cache             token_cache;

  db_enter(3, "prescan_default_function_arg_expr");
  /* Scan the default argument expression. */
  prescan_default_arg_expr(&token_cache, /*is_template_param=*/FALSE);
  /* Allocate a default arg expr fixup entry. */
  new_daefp = alloc_def_arg_expr_fixup();
  new_daefp->param_type = ptp;
  new_daefp->token_cache = token_cache;
  if (list == NULL) {
    /* No list pointer was passed by the caller.  This indicates that the
       argument information should simply be discarded. */
    discard_token_cache(&token_cache);
  } else {
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


void delayed_scan_of_default_arg_expr(a_param_type_ptr param_type_entry)
/*
Do the delayed scan of the default argument expression for a parameter.  The
cache has just been reactivated, so curr_token should represent the first
token in the cache.  Before doing the scan check that default expressions have
been declared for all successor arguments.
*/
{
  a_param_type_ptr  ptp;
  a_boolean         err = FALSE;

  db_enter(3, "delayed_scan_of_default_arg_expr");
  if (param_type_entry->default_arg_expr != NULL &&
      !is_error_node(param_type_entry->default_arg_expr)) {
    pos_error(ec_default_arg_already_defined, &pos_curr_token);
  }  /* if */
  /* Make a pass over all the param type entries that follow the current one.
     It is an error if there are any without a default argument. */
  for (ptp = param_type_entry->next; ptp != NULL; ptp = ptp->next) {
    if (!ptp->has_default_arg) {
      /* Issue an error on the first successor in the parameter list that does
         not have a default argument. */
      if (!err) {
        pos_error(ec_default_arg_not_at_end, &pos_curr_token);
        err = TRUE;
      }  /* if */
      ptp->has_default_arg = TRUE;
      ptp->default_arg_expr = error_node();
    }  /* if */
  }  /* for */
  /* We scan the expression whether an error was detected or not. */
  scan_default_arg_expr(param_type_entry);
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
  db_exit();
}  /* delayed_scan_of_default_arg_expr */


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
  db_exit();
}  /* delayed_scan_of_template_default_arg_expr */


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
