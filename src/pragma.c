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
#if 0
  ppp->il_pragma_entry = pp;
#endif /* if 0 */
}  /* add_pragma_to_il */


a_pending_pragma_ptr get_specific_pragmas(a_pragma_kind    kind,
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
  a_pending_pragma_ptr           ppp, *scope_list_addr;
  a_pragma_kind_description_ptr  pdp;
  a_pending_pragma_ptr           new_list = NULL, end_of_new_list = NULL;
  a_pending_pragma_ptr           prev_in_scope_list, next_in_scope_list;
  a_boolean                      is_bound_to_curr_construct;
  a_scope_stack_entry_ptr        ssep;

  /* Get the pragma description entry for the specified kind. */
  pdp = pragma_description_for_pragma_kind[(int)kind];
  if (pdp->binding_kind == (a_pragma_binding_kind)pbk_next_construct) {
    /* Set up to search for a pragma bound to the current declaration or
       statement. */
    check_assertion(!pdp->automatically_include_in_il ||
                    (sym == NULL) == (sp != NULL));
    is_bound_to_curr_construct = TRUE;
    ssep = &scope_stack[depth_scope_stack];
    scope_list_addr = &ssep->pragmas_bound_to_curr_decl_or_stmt;
  } else {
    /* Set up to search for a pbk_other pragma. */
    check_assertion((sym == NULL) && (sp == NULL));
    is_bound_to_curr_construct = FALSE;
    ssep = &scope_stack[pdp->global ? DEPTH_OF_FILE_SCOPE : depth_scope_stack];
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
      if (ppp->descr_ptr == pdp) {
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
        if (pdp->automatically_include_in_il) {
          char              *entity;
          an_il_entry_kind  entity_kind;
          a_boolean         at_file_scope;

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
            at_file_scope = pdp->global;
          }  /* if */
          add_pragma_to_il(ppp, entity_kind, entity, at_file_scope);
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
}  /* get_specific_pragmas */


#if 0
void end_of_construct_pragma_check(a_symbol_ptr     sym,
                                   a_statement_ptr  sp)
/*
*/
{
  a_pending_pragma_ptr      ppp, *scope_list_addr;
  a_pragma_description_ptr  pdp;
  char                      *entity;
  an_il_entry_kind          entity_kind;

  check_assertion((sym == NULL) == (sp != NULL));
  /* Go though the pragmas that are meant to apply to the current
     declaration or statement. */
  scope_list_addr = &scope_stack[depth_scope_stack].
                                           pragmas_bound_to_curr_decl_or_stmt;
  for(ppp = *scope_list_addr; ppp != NULL; ppp = ppp->next) {
    pdp = ppp->descr_ptr;
    if (pdp->may_bind_to_decl && sym != NULL ||
        pdp->may_bind_to_stmt && sp != NULL) {
      if (pdp->automatically_include_in_il) {
        if (sym != NULL) {
          entity = il_entry_for_symbol(sym, &entity_kind);
        } else {
          entity = (char *)sp;
          entity_kind = (an_il_entry_kind)iek_statement;
        }  /* if */
        if (entity != NULL) {
          add_pragma_to_il(ppp, entity_kind, entity, /*at_file_scope=*/FALSE);
        }  /* if */
      }  /* if */
      if (pdp->processing_function != NULL) {
#if 0
        (pdp->processing_function)(ppp, sym, (a_statement_ptr)NULL);
#endif /* if 0 */
      }  /* if */
    } else {
      /* Must be that a pragma was intended for a statement but current
         construct is a declaration, or vice versa.  Issue a diagnostic? */
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if 0
/* Shouldn't this be added to free_pending_pragma? */
#endif /* if 0 */
    if (ppp->source_sequence_entry != NULL &&
        ppp->source_sequence_entry->entity.kind ==
                                        (an_il_entry_kind)iek_none) {
      a_src_seq_sublist_ptr  sublist = NULL;
      remove_from_source_sequence_list(ppp->source_sequence_entry, &sublist);
      ppp->source_sequence_entry = NULL;
    }  /* if */
#endif /* if GENERATE_SOURCE_SEQUENCE_LISTS */
  }  /* for */
  free_pending_pragma_list(*scope_list_addr);
  *scope_list_addr = NULL;
}  /* end_of_construct_pragma_check */
#endif /* if 0 */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1994 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
