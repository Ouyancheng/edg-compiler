/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2020-2020 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

modules.c -- Module handling classes and routines.

*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

static a_symbol_ptr scan_module_qualified_name()
/*
Scan a qualified module name portion (either the primary name or the partition
name) and return the head of the symbol list, or NULL if the list is empty.
curr_token refers to the first qualifier in the module name portion being
scanned (a tok_identifier if the name is non-empty).
*/
{
  a_symbol_ptr result = NULL, *next_sym = &result;

  while (curr_token == tok_identifier) {
    (*next_sym) = alloc_symbol(sk_undefined, locator_for_curr_id.symbol_header,
                               &pos_curr_token);
    next_sym = &(*next_sym)->next;
    (void)get_token(); /* Advance past the identifier. */
    if (curr_token == tok_period) {
      /* This was a qualifier - we expect a tok_identifier to come next. */
      (void)get_token();
      (void)required_token_no_advance(tok_identifier, ec_exp_identifier);
    }  /* if */
  }  /* while */
  return result;
}  /* scan_module_qualified_name */


void scan_module_name(a_symbol_ptr *primary_name,
                      a_symbol_ptr *partition_name)
/*
Scan a module name, including its partition (if present) into the provided
symbol pointers.  A module or partition name can have any number of qualifiers,
e.g., "A.B.C:D.E.F".
*/
{
  *partition_name = NULL;
  add_stop_token(tok_semicolon);
  add_stop_token(tok_colon);
  if (curr_token != tok_colon) {
    required_token_no_advance(tok_identifier, ec_exp_identifier);
  }  /* if */
  *primary_name = scan_module_qualified_name();
  remove_stop_token(tok_colon);
  if (curr_token == tok_colon) {
    (void)get_token();
    (void)required_token_no_advance(tok_identifier, ec_exp_identifier);
    *partition_name = scan_module_qualified_name();
  }  /* if */
  remove_stop_token(tok_semicolon);
}  /* scan_module_name */


static a_boolean same_module_name(a_symbol_ptr module_name,
                                  a_symbol_ptr other_module_name)
{
  a_boolean result = FALSE;

  if (module_name == other_module_name) {
    result = TRUE;
  } else if (module_name == NULL || other_module_name == NULL) {
    /* result = FALSE */
  } else if (module_name->header->identifier_length !=
             other_module_name->header->identifier_length) {
    /* result = FALSE */
  } else if (strncmp(module_name->header->identifier,
                     other_module_name->header->identifier,
                     module_name->header->identifier_length) != 0) {
    /* result = FALSE */
  } else {
    /* This portion of the name was the same - check the rest. */
    result = same_module_name(module_name->next, other_module_name->next);
  }
  return result;
}  /* same_module_name */


a_boolean check_module_has_interface_dependency(
                                           a_symbol_ptr          module_sym,
                                           a_symbol_ptr          interface_sym,
                                           a_source_position_ptr module_pos)
/*
Determine whether the given module has an interface dependency on the module
referred to by interface_sym.  Return TRUE if there exists an interface
dependency, FALSE otherwise.
*/
{
  a_boolean result = FALSE;

  /* The easy case of a module importing itself. */
  if (same_module_name(module_sym->variant.module_info.primary_name,
                       interface_sym->variant.module_info.primary_name) &&
      same_module_name(module_sym->variant.module_info.partition_name,
                       interface_sym->variant.module_info.partition_name)) {
    pos_error(ec_module_cannot_depend_on_itself, module_pos);
    result = TRUE;
  }
  /* FIXME: Recurse over interface_sym's interface dependencies. */
  return result;
}  /* check_module_has_interface_dependency */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2020-2020 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
