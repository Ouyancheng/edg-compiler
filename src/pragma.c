/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1994 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
/*

pragma.c -- Routines to support #pragma directives

*/

#include "basics.h"
#include "host_envir.h"
#include "lang_feat.h"
#include "target.h"
#include "pragma.h"
#include "lexical.h"
#include "il.h"
#include "symbol_tbl.h"


static add_pragma_to_il(a_pending_pragma_ptr  ppp,
                        an_il_entry_kind      entity_kind,
                        char                  *entity_ptr,
                        a_boolean             at_file_scope)
/*
ppp points to the front-end representation of a pragma.  When the pragma
binding kind is pbk_next, entity_ptr is a pointer to the IL entry of the
specified entity_kind with which the pragma is associated; otherwise,
entity_ptr is NULL.  at_file_scope is TRUE if the pragma IL entry should be
allocated in the file-scope memory region and added to the file-scope
pragmas list; it is FALSE when the current IL scope should be used.

This routine (1) allocates the IL pragma entry and initializes it, (2)
binds it to the entity it's associated with, if any, and sets the
has_associated_pragma flag in the latter, (3) adds it to the appropriate
scope pragma list, (4) updates the source sequence entry if there is one,
and (5) returns a pointer to the IL pragma entry to the caller, in case
there is additional processing to be done.
*/
{
  a_pragma_ptr            pp;
  a_memory_region_number  region_to_switch_back_to;

#if CHECKING
  if (entity_ptr != NULL && in_file_scope(entity_ptr) && !at_file_scope) {
    check_assertion(curr_il_region_number == FILE_SCOPE_REGION_NUMBER);
  }  /* if */
#endif /* CHECKING */
  if (at_file_scope) switch_to_file_scope_region(&region_to_switch_back_to);
  pp = alloc_pragma(ppp->descr_ptr->kind);
  pp->decl_position = ppp->id_position;
  pp->pragma_text = ppp->pragma_text;
  if (entity_ptr != NULL) {
    check_assertion(ppp->descr_ptr->binding_kind ==
                                (a_pragma_binding_kind)pbk_next_construct);
    pp->entity.kind = entity_kind;
    pp->entity.ptr = entity_ptr;
    if (entity_kind == (an_il_entry_kind)iek_statement) {
      ((a_statement_ptr)entity_ptr)->has_associated_pragma = TRUE;
    } else {
      ((a_variable_ptr)entity_ptr)->
                      source_corresp.has_associated_pragma = TRUE;
    }  /* if */
  }  /* if */
  add_to_pragma_list(pp, at_file_scope ?
                            DEPTH_OF_FILE_SCOPE : depth_scope_stack);
#if GENERATE_SOURCE_SEQUENCE_LISTS
  f_update_source_sequence_list((char *)pp, (an_il_entry_kind)iek_pragma,
                                &pp->decl_position,
                                ppp->source_sequence_entry);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (at_file_scope) switch_back_to_original_region(region_to_switch_back_to);
  ppp->il_pragma_entry = pp;
}  /* add_pragma_to_il */


static void create_il_entry_for_pragma(a_pending_pragma_ptr ppp,
                                       a_symbol_ptr         sym,
                                       a_statement_ptr      sp)
/*
Create an IL pragma entry for a pending pragma entry.  If either
sym or sp is non-NULL, bind the pragma IL entry to the IL entry
indicated by sym or sp.  For pbk_next_construct pragmas, a non-NULL sym
or sp pointer must be supplied.  The IL entry is then added to the IL.
*/
{
  char              		 *entity;
  an_il_entry_kind	 	 entity_kind;
  a_boolean	         	 at_file_scope;
  a_boolean			 is_bound_to_curr_construct;
  a_pragma_kind_description_ptr	 pkdp;

  pkdp = ppp->descr_ptr;
  is_bound_to_curr_construct = pkdp->binding_kind == pbk_next_construct;
  /* A symbol pointer or statement pointer may only be supplied for
     pbk_next_construct pragmas. */
  check_assertion_str
        (is_bound_to_curr_construct && ((sym == NULL) != (sp == NULL)),
        "create_il_entry_for_pragma: invalid next_construct call");
  check_assertion_str
         (!is_bound_to_curr_construct && (sym == NULL && sp == NULL),
          "create_il_entry_for_pragma: binding kind/argument mismatch");
  if (is_bound_to_curr_construct) {
    if (sym != NULL) {
      entity = il_entry_for_symbol(sym, &entity_kind);
    } else {
      entity = (char *)sp;
      entity_kind = (an_il_entry_kind)iek_statement;
    }  /* if */
    at_file_scope = FALSE;
  } else {
    entity = NULL;
    entity_kind = (an_il_entry_kind)iek_none;
    at_file_scope = pkdp->global;
  }  /* if */
  add_pragma_to_il(ppp, entity_kind, entity, at_file_scope);
}  /* create_il_entry_for_pragma */


a_pending_pragma_ptr extract_specific_pragmas(a_pragma_kind    kind,
                                              a_symbol_ptr     sym,
                                              a_statement_ptr  sp)
/*
Return one or more pending-pragma entries of the specified pragma kind.  If
the pragma binds to the currrent declaration or statement and the pragma's
automatically_include_in_il flag is TRUE, then the currrent construct must be
specified: either sym, if this is a declaration, or sp, if it's a statement,
but not both, must be non-NULL.  If automatically_include_in_il is TRUE, the
IL entry is created before the associated pending-pragma entry is returned.
If more than one pending pragma entry of the required kind is found, they
are returned in a linked list.  This is possible, since the entries returned
are first removed from the lists they currently reside on.
*/
{
  a_pending_pragma_ptr           ppp;
  a_pending_pragma_ptr		 *scope_list_addr;
  a_pragma_kind_description_ptr  pkdp;
  a_pending_pragma_ptr           new_list = NULL;
  a_pending_pragma_ptr           end_of_new_list = NULL;
  a_pending_pragma_ptr           prev_in_scope_list;
  a_pending_pragma_ptr           next_in_scope_list;
  a_boolean                      is_bound_to_curr_construct;
  a_scope_stack_entry_ptr        ssep;

  /* Get the pragma description entry for the specified kind. */
  pkdp = pragma_description_for_pragma_kind[(int)kind];
  if (pkdp->binding_kind == (a_pragma_binding_kind)pbk_next_construct) {
    /* Set up to search for a pragma bound to the current declaration or
       statement. */
    is_bound_to_curr_construct = TRUE;
    ssep = &scope_stack[depth_scope_stack];
    scope_list_addr = &ssep->pragmas_bound_to_curr_decl_or_stmt;
  } else {
    /* Set up to search for a pbk_other pragma. */
    is_bound_to_curr_construct = FALSE;
    ssep = &scope_stack[pkdp->global ?
                                     DEPTH_OF_FILE_SCOPE : depth_scope_stack];
    scope_list_addr = &ssep->pending_pragmas;
  }  /* if */
  /* The outer loop examines one or more scope stack entries.  If it's a
     bind-to-next pragma, only the current scope stack list is checked.
     Otherwise, if it's a global pragma, only the file scope list is checked;
     otherwise, all the scope stack lists, from the current scope stack
     out to the file scope, are checked in turn. */
  for (;;) {
    prev_in_scope_list = NULL;
    /* Check the appropriate list of pending-pragma entries. */
    for (ppp = *scope_list_addr; ppp != NULL; ppp = next_in_scope_list) {
      next_in_scope_list = ppp->next;
      if (ppp->descr_ptr == pkdp) {
        /* It's the right kind -- remove it from the scope stack list. */
        if (prev_in_scope_list == NULL) {
          (*scope_list_addr) = ppp->next;
        } else {
          prev_in_scope_list->next = ppp->next;
        }  /* if */
        /* Now add it to the end of the list to return to the caller. */
        if (new_list == NULL) {
          new_list = ppp;
        } else {
          end_of_new_list->next = ppp;
        }  /* if */
        ppp->next = NULL;
        end_of_new_list = ppp;
        /* If an IL pragma should be generated for it, do that now. */
        if (pkdp->automatically_include_in_il) {
          create_il_entry_for_pragma(ppp, sym, sp);
        }  /* if */
      } else {
        /* Not a match.  Save the prev pointer and advance to the next entry
           on the scope's list. */
        prev_in_scope_list = ppp;
      }  /* if */
    }  /* for */
    /* The appropriate list for the scope has been examined.  Move on the the
       containing scope if appropriate; otherwise, terminate the loop. */
    if (is_bound_to_curr_construct) {
      /* Only one iteration of the loop for bind-to-next pragmas. */
      break;
    } else if (ssep == &scope_stack[DEPTH_OF_FILE_SCOPE]) {
      /* Nothing else on the scope stack. */
      break;
    } else {
      /* Advance on through the scope stack. */
      --ssep;
      scope_list_addr = &ssep->pending_pragmas;
    }  /* if */
  }  /* for */
  return new_list;  
}  /* extract_specific_pragmas */


void process_pragmas_bound_to_curr_decl_or_stmt(a_symbol_ptr     sym,
                                                a_statement_ptr  sp)
/*
Go through the list of pragmas that are to be bound to the current
declaration or statement and perform any actions required to process
the pragmas.
*/
{
  a_pending_pragma_ptr     	ppp;
  a_pending_pragma_ptr		list_start;
  a_pragma_kind_description_ptr	pkdp;
  a_scope_stack_entry_ptr   	ssep;

  check_assertion_str((sym == NULL) == (sp != NULL),
                      "process_pragmas_bound...: invalid arguments");
  /* Go though the pragmas that are meant to apply to the current
     declaration or statement. */
  ssep = &scope_stack[depth_scope_stack];
  ppp = ssep->pragmas_bound_to_curr_decl_or_stmt;
  list_start = ppp;
  for(; ppp != NULL; ppp = ppp->next) {
    a_next_construct_pragma_function_ptr ncpfp;
    pkdp = ppp->descr_ptr;
    /* Make sure that the binding information in the pragma description
       is consistent with the argument list.  If these do not match then
       the is_decl flag used when select_pragmas_bound_to_curr_decl_or_stmt
       must have been invalid. */
    check_assertion_str((pkdp->may_bind_to_decl && sym != NULL) || 
                        (pkdp->may_bind_to_stmt && sp != NULL),
                   "process_pragmas_bound...: binding/argument list mismatch")
    ncpfp = pkdp->variant.next_construct_processing_function;
    if (pkdp->automatically_include_in_il) {
      /* Create an IL entry for pragmas that should automatically be
         included in the IL. */
      create_il_entry_for_pragma(ppp, sym, sp);
    }  /* if */
    if (ncpfp != NULL) {
      /* Call the pragma processing function associated with this pragma. */
      (ncpfp)(ppp, sym, (a_statement_ptr)NULL);
    }  /* if */
  }  /* for */
  free_pending_pragma_list(list_start);
  ssep->pragmas_bound_to_curr_decl_or_stmt = NULL;
}  /* process_pragmas_bound_to_curr_decl_or_stmt */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1994 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
