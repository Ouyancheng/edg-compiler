/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1998 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

src_seq.c -- Support for source sequence list management

*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

#if GENERATE_SOURCE_SEQUENCE_LISTS
#if !STANDALONE_UTILITY_PROGRAM

#if DEBUG

static void put_str_to_f_debug(char *str)
/*
Output the indicated string to f_debug.  This is used as an output routine
when using the il_to_str routines.
*/
{
  fputs(str, f_debug);
}  /* put_str_to_f_debug */


void db_source_sequence_entry(a_source_sequence_entry_ptr  ssep)
/*
Display the source-sequence entry pointed to by ssep, for debugging purposes.
*/
{
  an_il_entry_kind  kind = (an_il_entry_kind)ssep->entity.kind;
  a_statement_ptr   sp;
  a_seq_number      seq;
  a_boolean         print_type = FALSE;
  a_type_ptr        declared_type = NULL;

  fputs(il_entry_kind_names[(int)kind], f_debug);
  if (kind == (an_il_entry_kind)iek_src_seq_sublist) {
    fputs(" ==>\n", f_debug);
    ssep = assoc_sublist_of(ssep)->source_sequence_list;
    for (; ssep != NULL; ssep = ssep->next) {
      fputs("    ", f_debug);
      db_source_sequence_entry(ssep);
    }  /* for */
  } else {
    if (kind == (an_il_entry_kind)iek_statement) {
      sp = (a_statement_ptr)ssep->entity.ptr;
      seq = seq_number_from_stmt_source_position(sp->position);
      if (seq != 0) fprintf(f_debug, " (at %lu)", seq);
      fputs(": ", f_debug);
      if (sp->kind == (a_statement_kind)stmk_init) {
        fputs("**BAD STMT KIND**", f_debug);
      } else {
        db_statement_kind((a_statement_kind)sp->kind);
      }  /* if */
      if (sp->kind == (a_statement_kind)stmk_expr) db_expr_summary(sp->expr);
    } else if (kind == (an_il_entry_kind)iek_pragma) {
      a_pragma_ptr  pp = (a_pragma_ptr)ssep->entity.ptr;

      fprintf(f_debug, " (at %lu): %s", pp->position.seq,
                       pragma_ids[(int)pp->kind]);
    } else if (kind == (an_il_entry_kind)iek_switch_clause) {
      /* Nothing else to display. */
    } else if (kind == (an_il_entry_kind)iek_src_seq_end_of_construct) {
      a_src_seq_end_of_construct_ptr  sseocp;
      sseocp = (a_src_seq_end_of_construct_ptr)ssep->entity.ptr;
      seq = sseocp->position.seq;
      if (seq != 0) fprintf(f_debug, " (at %lu)", seq);
      fputs(": ", f_debug);
      switch (sseocp->entity.kind) {
        case iek_statement:
          sp = (a_statement_ptr)sseocp->entity.ptr;
          db_statement_kind(sp->kind);
          fputs(" statement", f_debug);
          seq = seq_number_from_stmt_source_position(sp->position);
          if (seq != 0) {
            fprintf(f_debug, " (at %lu)", seq);
          }  /* if */
          break;
        case iek_type:
          fputc('"', f_debug);
          db_type_name((a_type_ptr)sseocp->entity.ptr);
          fputc('"', f_debug);
          break;
        case iek_template:
          fputc('"', f_debug);
          db_template_name((a_template_ptr)sseocp->entity.ptr);
          fputc('"', f_debug);
          break;
        case iek_namespace:
          fputc('"', f_debug);
          db_name(&((a_namespace_ptr)sseocp->entity.ptr)->source_corresp);
          fputc('"', f_debug);
          break;
        default:
          fprintf(f_debug, "***BAD END-OF-CONSTRUCT KIND %s***",
                           il_entry_kind_names[(int)sseocp->entity.kind]);
      }  /* switch */
    } else if (kind == (an_il_entry_kind)iek_using_decl) {
      a_using_decl_ptr         udp = (a_using_decl_ptr)ssep->entity.ptr;
      fprintf(f_debug, " (at %lu", udp->position.seq);
      if (udp->is_using_directive) {
        /* A namespace directive. */
        fputs(", using-directive", f_debug);
      } else if (udp->is_class_member) {
        fputs(", ", f_debug);
        db_access_control(udp->access);
      }  /* if */
      fputs("): \"", f_debug);
      /* Loop through the names in the overload set, if required. */
      for (;;) {
        if (!udp->is_using_directive && !udp->is_class_member) {
          /* Nonmember using-declaration -- check for global qualifier. */
          a_source_correspondence  *scp;
          scp = source_corresp_for_il_entry(udp->entity.ptr,
                                           (an_il_entry_kind)udp->entity.kind);
          if (scp != NULL && scp->parent.namespace_ptr == NULL) {
            fputs("::", f_debug);
          }  /* if */
        }  /* if */
        if (udp->entity.kind == (a_byte_il_entry_kind)iek_type) {
          db_type_name((a_type_ptr)udp->entity.ptr);
        } else {
          db_name(&((a_field_ptr)udp->entity.ptr)->source_corresp);
        }  /* if */
        fputc('"', f_debug);
        if (udp->hidden) fputs(" (hidden)", f_debug);
        udp = udp->next_in_overload_set;
        if (udp == NULL) break;
        fputs(", \"", f_debug);
      }  /* for */
    } else if (kind == (an_il_entry_kind)iek_instantiation_directive) {
      an_instantiation_directive_ptr  idp;
      idp = (an_instantiation_directive_ptr)ssep->entity.ptr;
      fprintf(f_debug, " (at %lu", idp->position.seq);
      if (idp->do_not_instantiate) fputs(", do not instantiate", f_debug);
      fputs("): \"", f_debug);
      if (idp->entity.kind == (a_byte_il_entry_kind)iek_type) {
        db_type_name((a_type_ptr)idp->entity.ptr);
      } else {
        db_name(source_corresp_for_il_entry(idp->entity.ptr,
                                         (an_il_entry_kind)idp->entity.kind));
        declared_type = ((a_routine_ptr)idp->entity.ptr)->type;
        print_type = TRUE;
      }  /* if */
      fputc('"', f_debug);
    } else {
      a_source_position             *pos;
      a_source_correspondence       *scp;
      a_symbol_ptr                  sym;
      a_boolean                     lparen_printed = FALSE;
      a_boolean                     autonomous = FALSE;
      a_boolean                     is_friend = FALSE;
      a_boolean                     is_implicit = FALSE;
      a_boolean                     is_anon_union_parent = FALSE;
      a_boolean                     new_specialization = FALSE;
      a_boolean                     func_prototype_decl = FALSE;
      a_boolean                     first_decl = FALSE;
      a_boolean                     other_scope_def = FALSE;
      a_type_ptr                    type_entry_type = NULL;
      a_src_seq_secondary_decl_ptr  sssdp = NULL;

      if (ssep->entity.ptr == NULL) {
        fputs(" <null entity ptr>", f_debug);
      } else {
        if (kind == (an_il_entry_kind)iek_src_seq_secondary_decl) {
          sssdp = (a_src_seq_secondary_decl_ptr)ssep->entity.ptr;
          scp = source_corresp_for_il_entry(
                                  sssdp->entity.ptr,
                                  (an_il_entry_kind)sssdp->entity.kind);
          check_assertion(scp != NULL);
          pos = &sssdp->decl_position;
          if (sssdp->autonomous_tag_decl) autonomous = TRUE;
          if (sssdp->friend_decl) is_friend = TRUE;
          if (sssdp->implicit_decl) is_implicit = TRUE;
          if (sssdp->declared_in_func_prototype) func_prototype_decl = TRUE;
          if (sssdp->specialized_with_new_syntax) new_specialization = TRUE;
          if (sssdp->first_declaration) first_decl = TRUE;
          if (sssdp->entity.kind == (a_byte_il_entry_kind)iek_type) {
            type_entry_type = (a_type_ptr)sssdp->entity.ptr;
          }  /* if */
        } else {
          scp = source_corresp_for_il_entry(
                                         ssep->entity.ptr,
                                         (an_il_entry_kind)ssep->entity.kind);
          check_assertion(scp != NULL);
          pos = &scp->decl_position;
          if (kind == (an_il_entry_kind)iek_type) {
            type_entry_type = (a_type_ptr)ssep->entity.ptr;
            if (type_entry_type->autonomous_primary_tag_decl) {
               autonomous = TRUE;
            }  /* if */
            if (is_immediate_class_type(type_entry_type) &&
                type_entry_type->variant.class_struct_union.is_specialized &&
                !type_entry_type->variant.class_struct_union.
                                               specialized_with_old_syntax) {
              new_specialization = TRUE;
            }  /* if */
            if (type_entry_type->kind == (a_type_kind)tk_typeref) {
              declared_type = type_entry_type->variant.typeref.type;
              print_type = TRUE;
            }  /* if */
          } else if (kind == (an_il_entry_kind)iek_routine) {
            a_routine_ptr  rp = (a_routine_ptr)ssep->entity.ptr;
            if (rp->defined_in_friend_decl) is_friend = TRUE;
            if (rp->defined_outside_of_parent) other_scope_def = TRUE;
            if (rp->is_specialized && !rp->specialized_with_old_syntax) {
              new_specialization = TRUE;
            }  /* if */
            declared_type = rp->declared_type;
            print_type = TRUE;
          } else if (kind == (an_il_entry_kind)iek_variable) {
            a_variable_ptr  vp = (a_variable_ptr)ssep->entity.ptr;
            if (vp->is_anonymous_parent_object) is_anon_union_parent = TRUE;
            if (vp->is_specialized && !vp->specialized_with_old_syntax) {
              new_specialization = TRUE;
            }  /* if */
            declared_type = vp->declared_type;
            print_type = TRUE;
          }  /* if */
        }  /* if */
        sym = (a_symbol_ptr)scp->assoc_info;
        if (kind == (an_il_entry_kind)iek_variable &&
            ((a_variable_ptr)ssep->entity.ptr)->is_parameter) {
          fprintf(f_debug, " (function param");
          lparen_printed = TRUE;
        }  /* if */
        if (sym != NULL && sym->decl_seq > 0) {
          fprintf(f_debug, "%s#%lu", (lparen_printed ? ", " : " ("),
                  sym->decl_seq);
          lparen_printed = TRUE;
        }  /* if */
        if (pos->seq > 0) {
          fprintf(f_debug, "%sat %lu", (lparen_printed ? ", " : " ("),
                  pos->seq);
          lparen_printed = TRUE;
        }  /* if */
        if (is_friend) {
          fprintf(f_debug, "%sfriend",
                           (lparen_printed ? ", " : " ("));
          lparen_printed = TRUE;
        }  /* if */
        if (other_scope_def) {
          fprintf(f_debug, "%soutside of parent",
                           (lparen_printed ? ", " : " ("));
          lparen_printed = TRUE;
        }  /* if */
        if (is_implicit) {
          fprintf(f_debug, "%simplicit decl",
                           (lparen_printed ? ", " : " ("));
          lparen_printed = TRUE;
        }  /* if */
        if (func_prototype_decl) {
          fprintf(f_debug, "%sfunc-prototype decl",
                           (lparen_printed ? ", " : " ("));
          lparen_printed = TRUE;
        }  /* if */
        if (autonomous) {
          fprintf(f_debug, "%sautonomous decl",
                           (lparen_printed ? ", " : " ("));
          lparen_printed = TRUE;
        }  /* if */
        if (is_anon_union_parent) {
          fprintf(f_debug, "%sanon union parent",
                           (lparen_printed ? ", " : " ("));
          lparen_printed = TRUE;
        }  /* if */
        if (new_specialization) {
          fprintf(f_debug, "%stemplate<>",
                           (lparen_printed ? ", " : " ("));
          lparen_printed = TRUE;
        }  /* if */
        if (first_decl) {
          fprintf(f_debug, "%sfirst decl",
                           (lparen_printed ? ", " : " ("));
          lparen_printed = TRUE;
        }  /* if */
        fprintf(f_debug, "%s: \"", (lparen_printed ? ")" : ""));
        if (type_entry_type != NULL) {
          db_type_name(type_entry_type);
        } else if (kind == (an_il_entry_kind)iek_template && sym != NULL) {
          /* Use the symbol name since there's more information in it. */
          an_il_to_str_output_control_block octl;

          clear_il_to_str_output_control_block(&octl);
          octl.output_str = put_str_to_f_debug;
          octl.debug_output = TRUE;
          form_symbol_name(sym, &octl);
        } else {
          db_name(scp);
        }  /* if */
        fputc('"', f_debug);
        if (sssdp != NULL) {
          /* Secondary declaration. */
          if (sssdp->entity.kind != (a_byte_il_entry_kind)iek_namespace) {
            declared_type = sssdp->declared_type;
            if (type_entry_type == NULL ||
                (declared_type != NULL &&
                 !same_entities(declared_type, type_entry_type))) {
              print_type = TRUE;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    if (print_type) {
      fprintf(f_debug, " (");
      if (declared_type == NULL) {
        fputs("type = ***NULL***", f_debug);
      } else if (has_name(declared_type)) {
        fputc('"', f_debug);
        db_type_name(declared_type);
        fputc('"', f_debug);
      } else {
        db_abbreviated_type(declared_type);
      }  /* if */
      fputc(')', f_debug);
    }  /* if */
    fputc('\n', f_debug);
  }  /* if */
}  /* db_source_sequence_entry */

#if CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS

static a_boolean is_ss_entry_for_class_template_definition(
                                       a_source_sequence_entry_ptr  ssep,
                                       a_symbol_ptr                 *sym)
/*
If the indicated source sequence entry represents a class template definition,
return TRUE and set *sym to point to the associated sk_class_template symbol.
*/
{
  a_boolean       flag = FALSE;
  a_template_ptr  tp = ss_entry_ptr(ssep, a_template_ptr);
  a_symbol_ptr    local_sym = (a_symbol_ptr)tp->source_corresp.assoc_info;

  if (local_sym != NULL && is_class_template_symbol(local_sym)) {
    /* Be sure this is the defining declaration of this template, in
       case it was declared more than once. */
    if (local_sym->variant.template_info->il_template_entry == tp) {
      *sym = local_sym;
      flag = TRUE;
    }  /* if */
  }  /* if */
  return flag;
}  /* is_ss_entry_for_class_template_definition */


static void db_ss_list_for_prototype_instantiation(
                                     a_source_sequence_entry_ptr  ssep,
                                     int           indent)
/*
Display the indicated list of source sequence entries, indenting each
source-sequence entry by "indent" spaces.
*/
{
  int                          i;

  for (; ssep != NULL; ssep = ssep->next) {
    for (i = 0; i < indent; i++) fputc(' ', f_debug);
    db_source_sequence_entry(ssep);
    /* If ssep represents a class template definition, put out the
       associated source sequence entries at this point. */
    if (ss_entry_kind(ssep) == (an_il_entry_kind)iek_template) {
      a_source_sequence_entry_ptr  list;
      a_symbol_ptr                 sym;

      if (is_ss_entry_for_class_template_definition(ssep, &sym)) {
        check_assertion(sym != NULL && is_class_template_symbol(sym));
        list = sym->variant.template_info->
                        variant.class_template.source_sequence_list;
        db_ss_list_for_prototype_instantiation(list, indent+2);
      }  /* if */
    }  /* if */
  }  /* for */
}  /* db_ss_list_for_prototype_instantiation */

#endif /* CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */

void db_ss_list(a_source_sequence_entry_ptr  ssep)
/*
Display the list of source-sequence entries pointed to by ssep, for debugging
purposes.
*/
{
  for (; ssep != NULL; ssep = ssep->next) {
    fputs("  ", f_debug);
    db_source_sequence_entry(ssep);
#if CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
    /* If ssep represents a class template definition, put out the
       associated source sequence entries at this point. */
    if (ss_entry_kind(ssep) == (an_il_entry_kind)iek_template) {
      a_source_sequence_entry_ptr  list;
      a_symbol_ptr                 sym;

      if (is_ss_entry_for_class_template_definition(ssep, &sym)) {
        check_assertion(sym != NULL && is_class_template_symbol(sym));
        list = sym->variant.template_info->
                        variant.class_template.source_sequence_list;
        db_ss_list_for_prototype_instantiation(list, 4);
      }  /* if */
    }  /* if */
#endif /* CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
  }  /* for */
}  /* db_ss_list */


void db_ss_list_for_scope_depth(a_scope_depth  depth)
/*
Given a scope-stack depth, display the associated list of source sequence
entries, for debugging purposes.
*/
{
  if (depth <= depth_scope_stack && depth > NO_SCOPE_DEPTH) {
    fputs("source sequence list for ", f_debug);
    db_scope_stack_entry_at_depth(depth);
    if (scope_stack[depth].source_sequence_list == NULL) {
      fputs(": <empty>\n", f_debug);
    } else {
      fputs(":\n", f_debug);
      db_ss_list(scope_stack[depth].source_sequence_list);
    }  /* if */
  }  /* if */
}  /* db_ss_list_for_scope_depth */


void db_ss_list_for_scope(a_scope_ptr  sp)
/*
Given a scope pointer, display its list of source-sequence entries, for
debugging purposes.
*/
{
  fputs("source-sequence list for ", f_debug);
  if (sp == NULL) {
    fputs("***NULL IL SCOPE***\n", f_debug);
  } else {
    db_scope(sp);
    if (sp->source_sequence_list == NULL) {
      fputs(": <empty>\n", f_debug);
    } else {
      fputs(":\n", f_debug);
      db_ss_list(sp->source_sequence_list);
    }  /* if */
  }  /* if */
}  /* db_ss_list_for_scope */

#endif /* DEBUG */

void fixup_function_scope_source_sequence_list(a_scope_ptr  sp)
/*
The function scope pointed to by sp has a list of source sequence entries
associated with it that may include a mixture of entries from the memory
region of the current function and the file scope memory region.  This
would cause violations of the rule that pointers from the file scope
memory region cannot point "down" to storage locations in the function
scope memory region.  Fix up the source sequence list to eliminate such
violations -- place entries belonging to the file-scope memory region on
separate sublists.
*/
{
  a_source_sequence_entry_ptr  ssep, new_ssep;
  a_src_seq_sublist_ptr        sublist, end_of_sublist_list = NULL;
  a_memory_region_number       region_to_switch_back_to;

  db_enter(4, "fixup_function_scope_source_sequence_list");
  check_assertion(sp->kind == (a_scope_kind)sck_function);
  for (ssep = sp->source_sequence_list; ssep != NULL; ssep = ssep->next) {
    if (in_file_scope(ssep)) {
      /* The source sequence entry belongs to the file scope memory region,
         but it's on a list of source sequence entries that are in the
         function's memory region.  But this can violate the requirement
         that no pointers in the file-scope memory region contain addresses
         in other memory regions.  The solution is to isolate the source
         file-scope sequence entries on a side list.  The side list header
         (a sublist entry) is in the file scope memory region; this is
         because it may be pointed to by a scope_orphaned_list_header. */
      switch_to_file_scope_region(&region_to_switch_back_to);
      sublist = alloc_src_seq_sublist();
      switch_back_to_original_region(region_to_switch_back_to);
      /* Add the sublist entry to a linked list of sublist entries that
         hangs off the IL scope entry for the current function. */
      if (sp->src_seq_sublist_list == NULL) {
        sp->src_seq_sublist_list = sublist;
      } else {
        end_of_sublist_list->next = sublist;
      }  /* if */
      end_of_sublist_list = sublist;
      /* Allocate a new source sequence entry to point to the sublist
         header.  It will replace ssep in the main list for the function,
         pointing through the sublist header to one or more entries that
         belong to the file-scope memory region. */
      switch_to_scope_region(depth_innermost_function_scope,
                             &region_to_switch_back_to);
      new_ssep = alloc_source_sequence_entry();
      switch_back_to_original_region(region_to_switch_back_to);
      new_ssep->entity.kind = (a_byte_il_entry_kind)iek_src_seq_sublist;
      new_ssep->entity.ptr  = (char *)sublist;
      /* Add new_ssep to the main list and detach the rest of the list
         (ssep and its successors) from the main list and move it onto the
         sublist. */
      if (ssep->prev == NULL) {
        sp->source_sequence_list = new_ssep;
      } else {
        ssep->prev->next = new_ssep;
        new_ssep->prev = ssep->prev;
        ssep->prev = NULL;
      }  /* if */
      sublist->source_sequence_list = ssep;
      sublist->last_source_sequence_entry = ssep;
      /* Proceed through the list now headed by ssep.  As long as successor
         entries belong to the file scope memory region, leave them on the
         sublist (by advancing the sublist's tail pointer). */
      for (ssep = ssep->next; ssep != NULL; ssep = ssep->next) {
        if (in_file_scope(ssep)) {
          /* This entry remains on the sublist. */
          sublist->last_source_sequence_entry = ssep;
        } else {
          /* This entry belongs to the function scope memory region, so it
             and all its successors are moved back to the main list. */
          ssep->prev->next = NULL;
          ssep->prev = new_ssep;
          new_ssep->next = ssep;
          /* Continue the outer loop. */
          break;
        }  /* if */
      }  /* for */
      if (ssep == NULL) break;
    }  /* if */
  }  /* for */
#if DEBUG
  if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
    if (sp->src_seq_sublist_list == NULL) {
      fputs("fixup: no change to source sequence list for ", f_debug);
      db_scope(sp);
      fputc('\n', f_debug);
    } else {
      fputs("fixup: modified ", f_debug);
      db_ss_list_for_scope(sp);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* fixup_function_scope_source_sequence_list */


static a_source_sequence_entry_ptr f_unlink_src_seq_entries(
                                a_source_sequence_entry_ptr  head,
                                a_source_sequence_entry_ptr  tail,
                                a_source_sequence_entry_ptr  *list_ptr,
                                a_source_sequence_entry_ptr  *end_of_list_ptr)
/*
A source sequence list defined by head...tail is contained in another list
defined by *list_ptr...*end_of_list_ptr.  (end_of_list_ptr may be NULL,
indicating a list described by just one pointer, as with the source
sequence list pointed to by IL scope entries.)  Unlink the head...tail list
from the list of which it's a sublist, returning a pointer to head and
updating list_ptr and end_of_list_ptr if appropriate.
*/
{
  db_enter(4, "f_unlink_src_seq_entries");
  if (head->prev == NULL) {
    /* head is also the start of the containing list. */
    check_assertion(list_ptr != NULL && *list_ptr == head);
    *list_ptr = tail->next;
  } else {
    head->prev->next = tail->next;
  }  /* if */
  if (tail->next == NULL) {
    if (end_of_list_ptr != NULL) {
      check_assertion(*end_of_list_ptr == tail);
      *end_of_list_ptr = head->prev;
    }  /* if */
  } else {
    tail->next->prev = head->prev;
    tail->next = NULL;
  }  /* if */
  head->prev = NULL;
  db_exit();
  return head;
}  /* f_unlink_src_seq_entries */

#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS

/* Macro to call unlink_src_seq_entries.  head and tail are source sequence
   entry pointers that specify a sublist of a list of source sequence
   entries, and scope_stk_ptr points to the scope stack entry to which the
   list belongs.  (If the list does not belong to the scope stack, call
   f_unlink_src_seq_entries directly.) */
#define unlink_src_seq_entries(head, tail, scope_stack_ptr)		\
  f_unlink_src_seq_entries((head), (tail),			      	\
                           &(scope_stack_ptr)->source_sequence_list,	\
                           &(scope_stack_ptr)->end_of_source_sequence_list)

#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */

/* Macro to call unlink_src_seq_entries when there is only one entry to
   unlink (not a list).  scope_stack_ptr points to the scope stack entry
   to which the list belongs.  (If the list does not belong to the scope
   stack, call f_unlink_src_seq_entries directly.) */
#define unlink_src_seq_entry(ssep, scope_stack_ptr)			\
  f_unlink_src_seq_entries((ssep), (ssep),				\
                           &(scope_stack_ptr)->source_sequence_list,	\
                           &(scope_stack_ptr)->end_of_source_sequence_list)


/* Macro to call unlink_src_seq_entries when there is only one entry to
   unlink (not a list).  il_scope points to the scope entry to which the
   list belongs. */
#define unlink_il_scope_src_seq_entry(ssep, il_scope)                   \
  f_unlink_src_seq_entries((ssep), (ssep),                              \
                           &(il_scope)->source_sequence_list,           \
                           (a_source_sequence_entry_ptr *)NULL);

void add_source_sequence_entry_to_list(a_source_sequence_entry_ptr  new_ssep)
/*
Add new_ssep to the end of the source-sequence list of the appropriate scope,
which is either the file scope or a function scope; if the latter, new_ssep
will go on a sublist if it was allocated in the file-scope memory region.
*/
{
  a_scope_stack_entry_ptr  scope_stack_ptr;

  db_enter(4, "add_source_sequence_entry_to_list");
  scope_stack_ptr = &scope_stack[depth_scope_stack];
#if DEBUG
  if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
    a_scope_depth  depth_ss_list_scope =
                     (depth_innermost_function_scope == NO_SCOPE_DEPTH) ?
                        DEPTH_OF_FILE_SCOPE : depth_innermost_function_scope;
    fputs("adding to ss list for ", f_debug);
    if (scope_stack_ptr->il_scope != NULL) {
      db_scope(scope_stack_ptr->il_scope);
    } else {
      (void)db_scope_kind(scope_stack_ptr->kind);
      fprintf(f_debug, " scope %d", (int)scope_stack_ptr->number);
    }  /* if */
    if (depth_scope_stack != depth_ss_list_scope) {
      fputs(" within ", f_debug);
      db_scope(scope_stack[depth_ss_list_scope].il_scope);
    }  /* if */
    fputs(":\n  ", f_debug);
    db_source_sequence_entry(new_ssep);
  }  /* if */
#endif /* DEBUG */
  if (scope_stack[depth_scope_stack].source_sequence_list == NULL) {
    scope_stack[depth_scope_stack].source_sequence_list = new_ssep;
  } else {
    scope_stack[depth_scope_stack].end_of_source_sequence_list->next =
                                                                   new_ssep;
    new_ssep->prev =
                 scope_stack[depth_scope_stack].end_of_source_sequence_list;
  }  /* if */
  scope_stack[depth_scope_stack].end_of_source_sequence_list = new_ssep;

#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  if (scope_stack_ptr->ss_list_instantiation_insert_point == NULL) {
    if (scope_stack_ptr->kind == (a_scope_kind)sck_file ||
        scope_stack_ptr->kind == (a_scope_kind)sck_namespace ||
        scope_stack_ptr->kind == (a_scope_kind)sck_namespace_extension ||
        scope_stack_ptr->kind == (a_scope_kind)sck_class_struct_union) {
      /* This is the first source sequence entry to be entered on the source
         sequence list since the insert point for instantiations was set to
         NULL (at the point where a new declaration begins). Record the
         current entry as the insert point (i.e., the point before which the
         source sequence entries for an instantiation should be inserted). */
      scope_stack_ptr->ss_list_instantiation_insert_point = new_ssep;
    }  /* if */
  }  /* if */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
  db_exit();
}  /* add_source_sequence_entry_to_list */


void f_update_source_sequence_list(char                         *entity_ptr,
                                   an_il_entry_kind             kind,
                                   a_source_sequence_entry_ptr  old_ssep)
/*
Allocate a source sequence entry for the entity and add it to the list for
the current scope.  If old_ssep is non-NULL, it points to a source sequence
entry that has already been created and linked in for this entity.
*/
{
  a_source_sequence_entry_ptr   new_ssep;
  a_src_seq_secondary_decl_ptr  sssdp;
  a_source_correspondence       *scp;
  a_boolean                     force_alloc_in_filescope;
  a_memory_region_number        region_to_switch_back_to;

  db_enter(4, "f_update_source_sequence_list");
  check_assertion_str(!source_sequence_entries_disallowed,
                      "source sequence entries not allowed in current scope");
  if (curr_il_region_number != file_scope_region_number &&
      kind != iek_statement && kind != iek_switch_clause &&
      in_file_scope(entity_ptr)) {
    /* The entity is in the file scope, but the current memory region is
       a function-scope memory region.  We'll need to change memory regions
       before allocating a new source sequence entry. */
    force_alloc_in_filescope = TRUE;
    switch_to_file_scope_region(&region_to_switch_back_to);
  } else {
    /* Current memory region is fine. */
    force_alloc_in_filescope = FALSE;
  }  /* if */
  if (old_ssep == NULL) {
    /* There is no previously allocated source sequence entry to reuse, so
       allocate a new one.  It will be filled out later. */
    new_ssep = alloc_source_sequence_entry();
  } else {
    /* A "reusable" source sequence entry should be empty. */
    check_assertion((ss_entry_kind(old_ssep) == (an_il_entry_kind)iek_none &&
                     old_ssep->entity.ptr == NULL));
    if (in_file_scope(old_ssep) || !force_alloc_in_filescope) {
      /* Either old_ssep is already allocated in the file scope or it's
         okay as is.  We'll just reuse it. */
      new_ssep = old_ssep;
    } else {
      /* The existing entry, in a function scope source sequence list, has to
         be replaced by an file-scope entry on a sublist. */
      /* Allocate a new source sequence entry to replace old_ssep. */
      new_ssep = alloc_source_sequence_entry();
      if (old_ssep->prev != NULL) {
        new_ssep->prev = old_ssep->prev;
        old_ssep->prev->next = new_ssep;
      } else {
        scope_stack[depth_scope_stack].source_sequence_list = new_ssep;
      }  /* if */
      if (old_ssep->next != NULL) {
        new_ssep->next = old_ssep->next;
        old_ssep->next->prev = new_ssep;
      } else {
        scope_stack[depth_scope_stack].end_of_source_sequence_list = new_ssep;
      }  /* if */
    }  /* if */
  }  /* if */
  if (force_alloc_in_filescope) {
    switch_back_to_original_region(region_to_switch_back_to);
  }  /* if */
  /* Point the source sequence entry at the entity. */
  new_ssep->entity.kind = (a_byte_il_entry_kind)kind;
  new_ssep->entity.ptr = entity_ptr;
  /* Then point the entity back to the source sequence entry. */
  if (kind == (an_il_entry_kind)iek_src_seq_secondary_decl) {
    sssdp = (a_src_seq_secondary_decl_ptr)entity_ptr;
    kind = (an_il_entry_kind)sssdp->entity.kind;
    entity_ptr = sssdp->entity.ptr;
  }  /* if */
  if (kind == (an_il_entry_kind)iek_statement) {
    /* Statement. */
    ((a_statement_ptr)entity_ptr)->source_sequence_entry = new_ssep;
  } else if (kind == (an_il_entry_kind)iek_pragma) {
    /* Pragma. */
    ((a_pragma_ptr)entity_ptr)->source_sequence_entry = new_ssep;
  } else {
    /* See if there's a source sequence entry. */
    scp = source_corresp_for_il_entry(entity_ptr, kind);
    if (scp == NULL) {
      /* No source correspondence, so no pointer back from the IL entry to
         the source sequence entry. */
    } else {
      /* Declared entity (variable, routine, etc.). */
      if (scp->source_sequence_entry == NULL) {
        /* The entity does not yet point to a source sequence entry.  Note
           that this includes the case where the pointer has been cleared
           because a prior declaration was turned into a secondary declaration
           -- e.g., a forward reference to a function -- see mark_declared. */
        if (depth_innermost_function_scope != NO_SCOPE_DEPTH &&
            in_file_scope(new_ssep) && !scp->is_class_member &&
            (kind == (an_il_entry_kind)iek_routine ||
             kind == (an_il_entry_kind)iek_variable)) {
          /* This must be a block-extern declaration or (in C mode) an
             implicit routine declaration.  Don't set the source sequence
             pointer in the IL entry.  (It's not really needed, and it
             introduces implementation difficulties for removing unneeded
             function bodies from the IL.) */
        } else {
          /* Set the source sequence entry pointer in the IL entry. */
          scp->source_sequence_entry = new_ssep;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (old_ssep == NULL) {
    add_source_sequence_entry_to_list(new_ssep);
  } else {
#if DEBUG
    if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
      fputs("empty ss entry changed to ", f_debug);
      db_source_sequence_entry(new_ssep);
    }  /* if */
#endif /* DEBUG */
  } /* if */
  db_exit();
}  /* f_update_source_sequence_list */


a_source_sequence_entry_ptr add_empty_source_sequence_entry(void)
/*
Create an "empty" source sequence entry (one with a null entity pointer and
an entity kind of iek_none) -- it will be allocated in the current memory
region -- and then add it to the end of the source sequence list.
*/
{
  a_source_sequence_entry_ptr  ssep;

  db_enter(4, "add_empty_source_sequence_entry");
  if (source_sequence_entries_disallowed) {
    /* We are in a context in which source sequence entries are not being
       generated. */
    ssep = NULL;
  } else {
    check_assertion(curr_il_region_number == file_scope_region_number ||
                    scope_stack[depth_scope_stack].kind !=
                                       (a_scope_kind)sck_func_prototype);
    ssep = alloc_source_sequence_entry();
    ssep->entity.kind = (a_byte_il_entry_kind)iek_none;
    /* Note that the entity.ptr field is left NULL. */
    add_source_sequence_entry_to_list(ssep);
  }  /* if */
  db_exit();
  return ssep;
}  /* add_empty_source_sequence_entry */


void add_end_of_construct_source_sequence_entry(char                   *ptr,
                                                a_byte_il_entry_kind   kind)
/*
Allocate two entries, an end-of-construct entry and a source sequence entry
that points to it.  The former is made to have the specified kind and point
at the specified entry.  The latter is added to the appropriate source
sequence list.
*/
{
  a_src_seq_end_of_construct_ptr  sseocp;
  a_source_sequence_entry_ptr     ssep;
  a_boolean                       force_alloc_in_filescope;
  a_memory_region_number          region_to_switch_back_to;

  if (!source_sequence_entries_disallowed) {
    /* We are in a context in which source sequence entries are being
       generated. */
    if (kind == (a_byte_il_entry_kind)iek_type &&
        curr_il_region_number != file_scope_region_number) {
      /* Local type. */
      check_assertion(in_file_scope(ptr));
      force_alloc_in_filescope = TRUE;
      switch_to_file_scope_region(&region_to_switch_back_to);
    } else {
      force_alloc_in_filescope = FALSE;
    }  /* if */
    /* Allocate and fill in the src-seq end of construct entry. */
    sseocp = alloc_src_seq_end_of_construct();
    sseocp->position = pos_curr_token;
    if (kind == (a_byte_il_entry_kind)iek_statement) {
      a_statement_ptr  sp = (a_statement_ptr)ptr;
      if (sp->kind == (a_statement_kind)stmk_block &&
          seq_number_from_stmt_source_position(sp->position) == 0) {
        /* This construct is a compiler-generated block surrounding a
           dependent statement.  Since no source position is put out on the
           original entity, suppress it on the end-of-construct entry, too. */
        sseocp->position = null_source_position;
      }  /* if */
    }  /* if */
    sseocp->entity.kind = kind;
    sseocp->entity.ptr = ptr;
    /* Allocate and fill in the source sequence entry. */
    ssep = alloc_source_sequence_entry();
    ssep->entity.kind = (a_byte_il_entry_kind)iek_src_seq_end_of_construct;
    ssep->entity.ptr = (char *)sseocp;
    /* Add the source sequence entry to the list. */
    add_source_sequence_entry_to_list(ssep);
    if (force_alloc_in_filescope) {
      switch_back_to_original_region(region_to_switch_back_to);
    }  /* if */
#if CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
    /* If this represents the end of a class definition for the prototype
       instantiation of a class template, the source-sequence entries that
       belong the class can't be left in the IL (since they refer to
       template parameters and non-real types that are not in the IL).
       Move the list to the template symbol supplement. */
    if (!prototype_instantiations_in_il &&
        scope_stack[depth_scope_stack].in_prototype_instantiation &&
        kind == (a_byte_il_entry_kind)iek_type) {
      a_type_ptr                        tp = (a_type_ptr)ptr;
      a_source_sequence_entry_ptr       ss_list;
      a_template_symbol_supplement_ptr  tssp;
      a_scope_stack_entry_ptr           scope_stack_ptr;

      if (is_immediate_class_type(tp)) {
        tssp = symbol_supplement_for_class(tp)->template_info;
        if (tssp != NULL) {
          /* The first source sequence entry is for the template itself, and
             should be the last source sequence entry belonging to the
             immediately enclosing scope. */
          ss_list = tp->source_corresp.source_sequence_entry;
          scope_stack_ptr = &scope_stack[depth_scope_stack-1];
          check_assertion(ss_list != NULL && ss_list->next == NULL &&
                          ss_list == scope_stack_ptr->
                                          end_of_source_sequence_list);
          /* Remove it from the end of the list of the enclosing scope. */
          (void)unlink_src_seq_entry(ss_list, scope_stack_ptr);
          /* The rest of the list that represents the prototype body comprises
             the current scope's list.  ssep should be the last entry in that
             list. */
          scope_stack_ptr = &scope_stack[depth_scope_stack];
          check_assertion(ssep->next == NULL &&
                          ssep == scope_stack_ptr->
                                           end_of_source_sequence_list);
          /* Attach the list to the template symbol supplement for the
             class template. */
          ss_list->next = unlink_src_seq_entries(scope_stack_ptr->
                                                        source_sequence_list,
                                                 ssep, scope_stack_ptr);
          ss_list->next->prev = ss_list;
          tssp->variant.class_template.source_sequence_list = ss_list;
#if DEBUG
          if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
            fputs("ss-list for prototype instantiation of ", f_debug);
            db_type_name(tp);
            fputs(":\n", f_debug);
            db_ss_list_for_prototype_instantiation(ss_list, 2);
          }  /* if */
#endif /* DEBUG */
        }  /* if */
      }  /* if */
    }  /* if */
#endif /* CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
  }  /* if */
}  /* add_end_of_construct_source_sequence_entry */


void insert_src_seq_list(a_source_sequence_entry_ptr  head,
                         a_source_sequence_entry_ptr  tail,
                         a_scope_depth                scope_depth,
                         a_source_sequence_entry_ptr  insert_before)
/*
Insert the source sequence list defined by head and tail (respectively, the
start and end of the list, which may still be embedded in another list) into
the source sequence list of the specified scope stack depth.  When
scope_depth is NO_SCOPE_DEPTH, insert the entries into the list associated
with the file IL scope.  If insert_before is NULL, append the list to the
end; otherwise, insert it immediatedly before insert_before.
*/
{
  a_source_sequence_entry_ptr  insert_after;
  a_scope_stack_entry_ptr      scope_stack_ptr;
  a_scope_ptr                  il_scope;

  if (scope_depth != NO_SCOPE_DEPTH) {
    /* Use the list of a scope stack entry. */
    scope_stack_ptr = &scope_stack[scope_depth];
    il_scope = NULL;
  } else {
    /* Use the list of the IL scope for the file scope. */
    il_scope = il_header.primary_scope;
    scope_stack_ptr = NULL;
  }  /* if */
#if DEBUG
  if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
    a_source_sequence_entry_ptr  after_tail = tail->next;

    fprintf(f_debug, "inserting %s ss list for ",
            insert_before == NULL ? "at end of" : "into");
    if (scope_stack_ptr != NULL &&
        scope_stack_ptr->il_scope != NULL) {
      db_scope(scope_stack_ptr->il_scope);
    } else {
      (void)db_scope_kind(scope_stack_ptr->kind);
      fprintf(f_debug, " scope %d", (int)scope_stack_ptr->number);
    }  /* if */
    fputs("\n", f_debug);
    if (insert_before != NULL) {
      fputs("    in front of ", f_debug);
      db_source_sequence_entry(insert_before);
    }  /* if */
    tail->next = NULL;
    db_ss_list(head);
    tail->next = after_tail;
  }  /* if */
#endif /* DEBUG */
  if (insert_before == NULL) {
    check_assertion(scope_stack_ptr != NULL);
    insert_after = scope_stack_ptr->end_of_source_sequence_list;
  } else {
    insert_after = insert_before->prev;
  }  /* if */
  if (insert_after == NULL) {
    if (scope_stack_ptr != NULL) {
      scope_stack_ptr->source_sequence_list = head;
    } else {
      il_scope->source_sequence_list = head;
    }  /* if */
  } else {
    insert_after->next = head;
  }  /* if */
  head->prev = insert_after;
  if (insert_before == NULL) {
    scope_stack_ptr->end_of_source_sequence_list = tail;
  } else {
    insert_before->prev = tail;
  }  /* if */
  tail->next = insert_before;
}  /* insert_src_seq_list */


void f_remove_from_src_seq_list(a_source_sequence_entry_ptr  ssep,
                                a_scope_depth                depth)
/*
Remove the source sequence entry pointed to by ssep from the list belonging
to the scope stack entry at the indicated depth (or, if depth is
NO_SCOPE_DEPTH, to the list of the IL scope of the file scope) and place it
on the appropriate available list (depending on the memory region in which
it was allocated).

(Note: this routine does not handle "sublists" on the function scope source
sequence list.  That is not a problem as long as it is not called after
fixup_function_scope_source_sequence_list has been called.)
*/
{
  a_source_sequence_entry_ptr  *avail_list_ptr;

  db_enter(4, "f_remove_from_src_seq_list");
  /* Entries allocated in the file scope memory region may be on the list of
     the file scope itself or on a side list of a function scope. */
  if (depth == NO_SCOPE_DEPTH) {
    a_scope_ptr  il_scope = il_header.primary_scope;

    (void)unlink_il_scope_src_seq_entry(ssep, il_scope);
  } else {
    a_scope_stack_entry_ptr  scope_stack_ptr = &scope_stack[depth];

#if DEBUG
    if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
      fputs("removing from source sequence list for ", f_debug);
      db_scope(scope_stack_ptr->il_scope);
      fputs(":\n  ", f_debug);
      db_source_sequence_entry(ssep);
    }  /* if */
#endif /* DEBUG */
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
    if (scope_stack_ptr->ss_list_instantiation_insert_point == ssep) {
      scope_stack_ptr->ss_list_instantiation_insert_point = ssep->next;
    }  /* if */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
    /* Remove the entry from its list. */
    (void)unlink_src_seq_entry(ssep, scope_stack_ptr);
  }  /* if */
  /* Add it to the head of the available list. */
  if (in_file_scope(ssep)) {
    avail_list_ptr = &scope_stack[DEPTH_OF_FILE_SCOPE].
                                              source_sequence_avail_list;
  } else {
    check_assertion(depth_innermost_function_scope != NO_SCOPE_DEPTH); 
    avail_list_ptr = &scope_stack[depth_innermost_function_scope].
                                              source_sequence_avail_list;
  }  /* if */
  ssep->next = *avail_list_ptr;
  *avail_list_ptr = ssep;
  db_exit();
}  /* f_remove_from_src_seq_list */


a_src_seq_secondary_decl_ptr make_source_sequence_secondary_decl(
                                            char               *ptr,
                                            an_il_entry_kind   kind,
                                            a_type_ptr         declared_type)
/*
Allocate and initialize a secondary-declaration entry for the specified IL
entry.  Set its declared type to the indicated type.
*/
{
  a_src_seq_secondary_decl_ptr  sssdp;
  a_memory_region_number        region_to_switch_back_to;

  /* Allocate and initialize the source sequence entry. */
  switch_to_file_scope_region(&region_to_switch_back_to);
  sssdp = alloc_src_seq_secondary_decl();
  switch_back_to_original_region(region_to_switch_back_to);
  sssdp->entity.ptr = ptr;
  sssdp->entity.kind = (a_byte_il_entry_kind)kind;
  sssdp->declared_type = declared_type;
  return sssdp;
}  /* make_source_sequence_secondary_decl */

#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
#if CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS

void check_for_and_remove_redundant_secondary_decl_ss_entry(
                                                       a_type_ptr class_type)
/*
Look for a "compiler-generated" forward-declaration secondary-decl source
sequence entry.  If it immediately precedes the primary entry for the
class specified, remove it.
*/
{
  a_source_sequence_entry_ptr  ssep;
  a_src_seq_secondary_decl_ptr sssdp;

  ssep = class_type->source_corresp.source_sequence_entry;
  if (ssep != NULL) {
    ssep = ssep->prev;
    if (ssep != NULL &&
        ss_entry_kind(ssep) ==
                     (an_il_entry_kind)iek_src_seq_secondary_decl) {
      sssdp = ss_entry_ptr(ssep, a_src_seq_secondary_decl_ptr);
      /* If no instantiation was inserted between the source-sequence entry
         for the definition of the class and the secondary-decl entry added
         before it, the latter can be removed. */
      if (sssdp->entity.ptr == (char *)class_type &&
          sssdp->compiler_generated_forward_decl) {
        if (sssdp->first_declaration) {
          symbol_supplement_for_class(class_type)->
                                         definition_is_first_decl = TRUE;
        }  /* if */
        remove_from_src_seq_list(ssep);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* check_for_and_remove_redundant_secondary_decl_ss_entry */

#endif /* CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */

a_scope_depth scope_depth_for_class_ss_list(a_type_ptr  class_type)
/*
If class_type is a "real" non-local class, return the depth of the innermost
currently active namespace scope in which it is nested, or else the depth of
the file scope.  If it is a local class or non-real, return NO_SCOPE_DEPTH.
*/
{
  a_namespace_ptr                nsp;
  a_scope_depth                  scope_depth = NO_SCOPE_DEPTH;
  a_class_symbol_supplement_ptr  cssp;
#if EXPENSIVE_CHECKING
  a_type_ptr                     orig_class_type = class_type;
#endif /* EXPENSIVE_CHECKING */

  cssp = symbol_supplement_for_class(class_type);
  if (!class_type->source_corresp.is_local_to_function &&
      !class_type->variant.class_struct_union.is_nonreal_class) {
    if (class_type->variant.class_struct_union.is_template_class &&
        !class_type->variant.class_struct_union.is_specialized) {
      /* The class is a template instantiation.  That means its scope is
         the namespace in which it is referenced (and not, as one might
         expect, the scope in which its definition appears) -- see
         find_instantiation_insert_scope. */
      nsp = cssp->referencing_namespace;
    } else {
      /* If this is a nested class, find the top-most class. */
      while (class_type->source_corresp.is_class_member) {
        class_type = class_type->source_corresp.parent.class_type;
      }  /* while */
      /* Use the namespace of the top-most class. */
      nsp = class_type->source_corresp.parent.namespace_ptr;
    }  /* if */
    /* If a namespace was identified, be sure it's still on the stack.  If
       not (i.e., if its scope was popped), the associated source sequence
       entries will have migrated to a containing namespace scope or out to
       the file scope.  Find the innermost currently active namespace. */
    if (nsp == NULL) {
      scope_depth = DEPTH_OF_FILE_SCOPE;
    } else {
      nsp = skip_namespace_aliases(nsp);
      scope_depth = nsp->variant.assoc_scope->depth_in_scope_stack;
      if (scope_depth == NO_SCOPE_DEPTH) {
        /* nsp is no longer active on the stack.  Use the innermost enclosing
           namespace. */
        scope_depth = depth_innermost_namespace_scope;
      }  /* if */
      while (scope_depth != DEPTH_OF_FILE_SCOPE) {
        if (scope_stack[scope_depth].kind == (a_scope_kind)sck_namespace ||
            scope_stack[scope_depth].explicitly_declared_namespace_extension) {
          /* Found a currently active namespace scope among the namespace
             parents of class_type.  Note that namespace extension scopes
             qualify as "active" only when they correspond to an explicit
             extension-namespace-definition (7.3.1). */
          break;
        } else {
          /* Must not be an active namespace scope.  Advance to the enclosing
             namespace. */
          scope_depth = scope_stack[scope_depth-1].
                                           depth_innermost_namespace_scope;
        }  /* if */
      }  /* while */
    }  /* if */
#if EXPENSIVE_CHECKING
    /* Verify that the source sequence entry for class_type really is on the
       list at the inferred scope depth. */
    { a_source_sequence_entry_ptr  ssep;
      for (ssep = scope_stack[scope_depth].source_sequence_list;
           ssep != NULL;
           ssep = ssep->next) {
        if (ss_entry_ptr(ssep, a_type_ptr) == orig_class_type) break;
      }  /* for */
      check_assertion(ssep != NULL);
    }
#endif /* EXPENSIVE_CHECKING */
  }  /* if */
  return scope_depth;
}  /* scope_depth_for_class_ss_list */


static a_scope_depth find_innermost_namespace_scope_depth(
                                         a_scope_stack_entry_ptr  sse_ptr)
/*
Return the innermost namespace scope relative to the indicated scope stack
entry.  If the scope stack entry is a template instantiation scope or
belongs to a template instantiation, the innermost namespace scope to return
is that in which the instantiation is triggered, not the one in which the
template is defined.
*/
{
  a_scope_depth  depth;

  for (;;) {
    if (sse_ptr->kind == (a_scope_kind)sck_template_instantiation) {
      /* The instantiation context depth is the depth at the point the
         instantiation is triggered. */
      if (sse_ptr->instantiation_context_depth == NO_SCOPE_DEPTH) {
        /* The current scope must be a nested instantiation.  Get the
           context from the enclosing instantiation. */
        check_assertion(sse_ptr->depth_innermost_instantiation_scope !=
                                                           NO_SCOPE_DEPTH);
        sse_ptr = &scope_stack[sse_ptr->depth_innermost_instantiation_scope];
      } else {
        /* The scope in which the instantiation was triggered is recorded
           in the scope stack entry. */
        sse_ptr = &scope_stack[sse_ptr->instantiation_context_depth];
      }  /* if */
    } else if (sse_ptr->depth_innermost_instantiation_scope !=
                                                         NO_SCOPE_DEPTH) {
      /* The current scope is within an instantiation.  Find the innermost
         instantiation scope. */
      sse_ptr = &scope_stack[sse_ptr->depth_innermost_instantiation_scope];
    } else {
      /* The current scope will do. */
      break;
    }  /* if */
  }  /* for */
  depth = sse_ptr->depth_innermost_namespace_scope;
  for (;;) {
    sse_ptr = &scope_stack[depth];
    if (sse_ptr->kind != (a_scope_kind)sck_namespace_extension ||
        sse_ptr->explicitly_declared_namespace_extension) {
      break;
    }  /* if */
    sse_ptr--;
    depth = sse_ptr->depth_innermost_namespace_scope;
  }  /* for */
#if CHECKING
  switch (scope_stack[depth].kind) {
    case sck_file:
    case sck_namespace:
      /* Okay. */
      break;
    case sck_namespace_extension:
      check_assertion(scope_stack[depth].  
                              explicitly_declared_namespace_extension);
      break;
    default:
      unexpected_condition();
  }  /* switch */
#endif /* CHECKING */      
  return depth;
}  /* find_innermost_namespace_scope_depth */


static a_scope_depth find_instantiation_insert_scope(
                                     a_scope_stack_entry_ptr      curr_sse_ptr,
                                     a_source_sequence_entry_ptr  ssep)
/*
Find the scope stack entry in which a template instantiation (represented by
a list of source sequence entries) should be inserted.  curr_sse_ptr is a
pointer to the current scope stack entry, and ssep represents the template
instantiation that is to be added.  Return the scope depth in which the
insertion should occur.

Ordinarily, the insert point is in a namespace scope at a location preceding
the reference that triggered the instantiation.  For instance,

  template <class T> class A { };
  A<int> x;
  void f() { A<char> y; }
  class B { A<long> z; }

The insert point for the instantiation of A<int> is immediately before the
source sequence entry for the definition of variable x.  The insert point
for A<char> cannot be inside the function, so it is immediately before the
entry for the definition of f().  Similarly, the insert point for A<long> is
immediately before the entry for the definition of class B.

Sometimes, however, the instantiation cannot be moved outside a class
definition -- when it is the instantiation of a member template or when
it is a nonmember template with a template argument that involves a class
member.  For instance,

  template <class T> class X { };
  class A {
    class N { };
    X<N> x;
    template <class T> class Y { };
    Y<int> y;
  };

If the instantiation of X<N> is represented at file scope prior to the
definition of A, there will be an unresolved name reference (A::N).
Similarly, the instantiation of A::Y<int> cannot precede the definition of A.
Neither, of course, can the instantiations be inserted after the definition
of A.  (On the other hand, the problem with putting the instantiations where
they need to be is that template instantiations are represented in the
source sequence list as explicit specializations, and the C++ standard
requires explicit specializations to appear at file or namespace scope; of
the unavoidable difficulties with cases like the above, this is the least
bothersome -- it merely involves relaxing a semantic restriction, which some
C++ compilers do already by default.)

The algorithm used to determine the insert point in such cases embodies two
rules (where an "uncompleted class" is a class whose definition has started
but not yet finished): (1) when the instantiation of a member template
occurs during the definition the class hierarchy to which it belongs, it
cannot move out beyond the innermost uncompleted class of which it is a
member; (2) when the instantiation has template arguments that involve an
uncompleted class type, the instantiation cannot be moved beyond the
innermost such class.
*/
{
  a_scope_depth                 insert_scope_depth = NO_SCOPE_DEPTH;
  a_type_ptr                    entity_type, parent_class;
  an_il_entry_kind              entity_kind;
  char                          *entity_ptr;
  a_src_seq_secondary_decl_ptr  sssdp;
  a_scope_depth                 parent_scope_depth, ref_scope_depth;
  a_source_correspondence       *scp;
  a_template_arg_ptr            template_arg_list;
  a_boolean                     members_only;

  db_enter(4, "find_instantiation_insert_scope");
#if DEBUG
  if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
    fputs("finding insert point for ", f_debug);
    db_source_sequence_entry(ssep);
    fputs("  original ref scope: ", f_debug);
    db_scope_stack_entry_at_depth(curr_sse_ptr - scope_stack);
    fputs("\n", f_debug);
  }  /* if */
#endif /* DEBUG */
  /* Identify IL entity that is being instantiated. */
  entity_kind = ss_entry_kind(ssep);
  if (entity_kind == (an_il_entry_kind)iek_src_seq_secondary_decl) {
    sssdp = ss_entry_ptr(ssep, a_src_seq_secondary_decl_ptr);
    entity_kind = (an_il_entry_kind)sssdp->entity.kind;
    entity_ptr = sssdp->entity.ptr;
  } else {
    entity_ptr = ssep->entity.ptr;
  }  /* if */
  /* Identify the parent type of the entity.  If it has a non-null parent
     type, then it is an instance of a member template, and receives
     special handling -- see (1) above. */
  scp = source_corresp_for_il_entry(entity_ptr, entity_kind);
  check_assertion(scp != NULL);
  parent_scope_depth = NO_SCOPE_DEPTH;
  if (scp->is_class_member) {
    parent_class = scp->parent.class_type;
    /* Find the innermost uncompleted parent class. */
    for (;;) {
      parent_scope_depth = parent_class->variant.class_struct_union.
                               extra_info->assoc_scope->depth_in_scope_stack;
      if (parent_scope_depth != NO_SCOPE_DEPTH) {
        /* This is the innermost active class on the scope stack (active in
           the sense of still being defined). */
        break;
      } else if (!parent_class->source_corresp.is_class_member) {
        /* parent_class is not nested in another class -- remember the
           namespace it's a member of, if any. */
        parent_class = NULL;
        break;
      }  /* if */
      /* A nested class -- keep looping. */
      parent_class = parent_class->source_corresp.parent.class_type;
    }  /* for */
  } else {
    /* The entity is not a class member.  It may be a namespace member. */
    parent_class = NULL;
  }  /* if */
  if (parent_scope_depth == NO_SCOPE_DEPTH) {
    /* There is not a class parent scope that will serve for the insert
       scope.  Look for a namespace parent scope. */
    parent_scope_depth = find_innermost_namespace_scope_depth(curr_sse_ptr);
  }  /* if */
#if DEBUG
  if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
    fputs("  entity: ", f_debug);
    if (scp->assoc_info == NULL) {
      fputs("<null symbol ptr>", f_debug);
    } else {
      db_symbol_name((a_symbol_ptr)scp->assoc_info);
    }  /* if */
    if (parent_class == NULL) {
      fputs(", no uncompleted parent class\n", f_debug);
      if (parent_scope_depth != DEPTH_OF_FILE_SCOPE) {
        fputs("  scope of innermost active namespace: ", f_debug);
        db_scope_stack_entry_at_depth(parent_scope_depth);
        fputs("\n", f_debug);
      }  /* if */
    } else {
      fputs("\n  scope of innermost uncompleted parent class: ", f_debug);
      db_scope_stack_entry_at_depth(parent_scope_depth);
      fputs("\n", f_debug);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  for (ref_scope_depth = depth_scope_stack;
       ref_scope_depth > NO_SCOPE_DEPTH;
       ref_scope_depth--) {
    if (parent_scope_depth != NO_SCOPE_DEPTH &&
        ref_scope_depth <= parent_scope_depth) {
      insert_scope_depth = parent_scope_depth;
      break;
    }  /* if */
    if (scope_stack[ref_scope_depth].kind ==
                            (a_scope_kind)sck_class_struct_union) {
      break;
    }  /* if */
  }  /* for */
  if (insert_scope_depth == NO_SCOPE_DEPTH &&
      ref_scope_depth != NO_SCOPE_DEPTH) {
    template_arg_list = NULL;
    switch (entity_kind) {
      case iek_type:
        entity_type = (a_type_ptr)entity_ptr;
        break;
      case iek_routine:
        entity_type = ((a_routine_ptr)entity_ptr)->type;
        template_arg_list = ((a_routine_ptr)entity_ptr)->template_arg_list;
        break;
      case iek_variable:
        entity_type = ((a_variable_ptr)entity_ptr)->type;
        template_arg_list = NULL;
        break;
      default:
        unexpected_condition();
    }  /* switch */
#if DEBUG
    if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
      fputs("  entity type: ", f_debug);
      db_type_name(entity_type);
      fputs("\n", f_debug);
    }  /* if */
#endif /* DEBUG */
   for (; ref_scope_depth > NO_SCOPE_DEPTH; ref_scope_depth--) {
      if (parent_scope_depth != NO_SCOPE_DEPTH &&
          ref_scope_depth <= parent_scope_depth) {
        insert_scope_depth = parent_scope_depth;
        break;
      }  /* if */
      if (scope_stack[ref_scope_depth].kind ==
                              (a_scope_kind)sck_class_struct_union) {
        a_type_ptr  class_type = scope_stack[ref_scope_depth].assoc_type;

        if (!class_type->source_corresp.is_class_member) {
          members_only = TRUE;
        } else {
          a_class_type_supplement_ptr  ctsp;
          ctsp = class_type->source_corresp.parent.class_type->
                                 variant.class_struct_union.extra_info;
          members_only = (ctsp->assoc_scope->
                                     depth_in_scope_stack == NO_SCOPE_DEPTH);
        }  /* if */
#if DEBUG
        if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
          fprintf(f_debug, "  checking ref scope%s: ",
                           members_only ? " (members only)" : "");
          db_scope_stack_entry_at_depth(ref_scope_depth);
          fputs("\n", f_debug);
        }  /* if */
#endif /* DEBUG */
        if (type_involves_specific_class_type(entity_type, class_type,
                                              members_only) ||
            (template_arg_list != NULL &&
             template_args_involve_specific_class_type(template_arg_list,
                                                       class_type,
                                                       members_only))) {
          insert_scope_depth = ref_scope_depth;
          break;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  if (insert_scope_depth == NO_SCOPE_DEPTH) {
    a_scope_stack_entry_ptr  sse_ptr = curr_sse_ptr;

    for (; sse_ptr != NULL; sse_ptr = previous_scope_of(sse_ptr)) {
      if (sse_ptr->kind == (a_scope_kind)sck_file) {
        /* Insert it into the file scope. */
        insert_scope_depth = DEPTH_OF_FILE_SCOPE;
      } else if (sse_ptr->kind == (a_scope_kind)sck_namespace ||
                 (sse_ptr->kind == (a_scope_kind)sck_namespace_extension &&
                  sse_ptr->explicitly_declared_namespace_extension)) {
        /* This is the file scope or a namespace scope that corresponds to an
           actual source construct. */
        if (sse_ptr == &scope_stack[depth_innermost_namespace_scope]) {
          insert_scope_depth = depth_innermost_namespace_scope;
        } else {
          for (insert_scope_depth = DEPTH_OF_FILE_SCOPE + 1;;
               insert_scope_depth++) {
            if (sse_ptr == &scope_stack[insert_scope_depth]) break;
            check_assertion(insert_scope_depth != depth_scope_stack);
          }  /* for */
        }  /* if */
        break;
      }  /* if */
    }  /* for */
  }  /* if */
#if DEBUG
  if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
    a_source_sequence_entry_ptr  insert_point;

    insert_point = scope_stack[insert_scope_depth].
                                  ss_list_instantiation_insert_point;
    fprintf(f_debug, "insert point found: %s list for ",
                     insert_point == NULL ? "at end of" : "in");
    db_scope_stack_entry_at_depth(insert_scope_depth);
    if (insert_point != NULL) {
      fputs(" prior to:\n  ", f_debug);
      db_source_sequence_entry(insert_point);
    } else {
      fputs("\n", f_debug);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return insert_scope_depth;
}  /* find_instantiation_insert_scope */


void f_move_src_seq_list(a_source_sequence_entry_ptr  head,
                         a_source_sequence_entry_ptr  tail,
                         a_scope_depth                source_depth,
                         a_source_sequence_entry_ptr  insert_point,
                         a_scope_depth                target_depth)
/*
Unlink a linked list of source sequence entries, starting with head and
ending with tail, from the source sequence list of the scope stack entry
at source_depth, and then insert them into the source sequence list of the
scope stack entry at target_depth at a point immediately preceding the entry
pointed to by insert point; if insert_point is null, add them to to the end
of the list.
*/
{
  check_assertion(source_depth != NO_SCOPE_DEPTH &&
                  target_depth != NO_SCOPE_DEPTH);
  (void)unlink_src_seq_entries(head, tail, &scope_stack[source_depth]);
  insert_src_seq_list(head, tail, target_depth, insert_point);
}  /* f_move_src_seq_list */


void insert_instantiation_src_seq_list(a_scope_stack_entry_ptr scope_stack_ptr)
/*
Remove the source sequence list from the specified scope stack entry and
insert it at the appropriate place in another scope.
*/
{
  a_source_sequence_entry_ptr  head, tail, insert_before, insert_after, ssep;
  a_scope_depth                depth;
  a_source_correspondence      *scp;

  check_assertion(scope_stack_ptr->kind ==
                              (a_scope_kind)sck_template_instantiation &&
                  scope_stack_ptr->instance_sym != NULL);
  /* Remove the entire source sequence list associated with the scope stack
     entry. */
  head = scope_stack_ptr->source_sequence_list;
  tail = scope_stack_ptr->end_of_source_sequence_list;
  scope_stack_ptr->source_sequence_list = NULL;
  scope_stack_ptr->end_of_source_sequence_list = NULL;
  /* Find the list to insert in into. */
  scp = source_corresp_entry_for_symbol(scope_stack_ptr->instance_sym);
  ssep = scp->source_sequence_entry;
  depth = find_instantiation_insert_scope(scope_stack_ptr, ssep);
  insert_before = scope_stack[depth].ss_list_instantiation_insert_point;
  if (insert_before != NULL) {
    insert_after = insert_before->prev;
  } else {
    insert_after = scope_stack[depth].end_of_source_sequence_list;
  }  /* if */
  if (insert_after != NULL) {
    /* Often a secondary-source sequence entry for a partial instantiation
       is put out immediately prior to the entry for a full instantiation.
       This just clutters up the list, so remove the former. */
    if (ss_entry_kind(insert_after) ==
                           (an_il_entry_kind)iek_src_seq_secondary_decl) {
      a_src_seq_secondary_decl_ptr  sssdp;
      a_type_ptr                    tp;
      a_boolean                     unneeded;

      sssdp = ss_entry_ptr(insert_after, a_src_seq_secondary_decl_ptr);
      if (head->entity.ptr == sssdp->entity.ptr) {
        if (sssdp->is_partial_instantiation) {
          unneeded = TRUE;
        } else if (ss_entry_kind(head) == (an_il_entry_kind)iek_type) {
          tp = ss_entry_ptr(head, a_type_ptr);
          unneeded = (is_immediate_class_type(tp) &&
                      tp->variant.class_struct_union.is_template_class);
        } else {
          unneeded = FALSE;
        }  /* if */
        if (unneeded) f_remove_from_src_seq_list(insert_after, depth);
      }  /* if */
    }  /* if */
  }  /* if */
  insert_src_seq_list(head, tail, depth, insert_before);
}  /* insert_instantiation_src_seq_list */


void add_source_sequence_entry_for_partial_instantiation(
                                          char               *ptr,
                                          an_il_entry_kind   kind,
                                          a_type_ptr         declared_type)
/*
Add a source sequence secondary declaration entry to represent the
partial instantiation of the entity specified by the indicated entity.
declared_type points to a type that should be recorded in the entry.
*/
{
  a_src_seq_secondary_decl_ptr  sssdp;
  a_symbol_ptr                  sym;
  a_source_correspondence       *scp;
  a_source_sequence_entry_ptr   ssep;
  a_memory_region_number        region_to_switch_back_to;

  if (!scope_stack[DEPTH_OF_FILE_SCOPE].source_sequence_entries_disallowed) {
    /* Turn on the generation of source sequence entries. */
    source_sequence_entries_disallowed = FALSE;
    /* Create the entry. */
    sssdp = make_source_sequence_secondary_decl(ptr, kind, declared_type);
    sssdp->is_partial_instantiation = TRUE;
    sssdp->compiler_generated_forward_decl = TRUE;
    if (kind == (an_il_entry_kind)iek_type) {
      sssdp->autonomous_tag_decl = TRUE;
    }  /* if */
    /* This partial instantiation can be triggered anywhere.  Use the
       position associated with the symbol. */
    scp = source_corresp_for_il_entry(ptr, kind);
    sym = (a_symbol_ptr)scp->assoc_info;
    sssdp->decl_position = sym->decl_position;
#if BACK_END_IS_CP_GEN_BE
    sssdp->specialized_with_new_syntax =
                            !old_specializations_for_generated_instances;
#else /* !BACK_END_IS_CP_GEN_BE */
    sssdp->specialized_with_new_syntax = TRUE;
#endif /* BACK_END_IS_CP_GEN_BE */
#if DEBUG
    if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
      fputs("partial instantiation of ",
            f_debug);
      db_symbol_name((a_symbol_ptr)((a_type_ptr)ptr)->
                                       source_corresp.assoc_info);
      fputs("\n", f_debug);
    }  /* if */
#endif /* if DEBUG */
    switch_to_file_scope_region(&region_to_switch_back_to);
    ssep = alloc_source_sequence_entry();
    switch_back_to_original_region(region_to_switch_back_to);
    ssep->entity.kind = (a_byte_il_entry_kind)iek_src_seq_secondary_decl;
    ssep->entity.ptr  = (char *)sssdp;
#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
    if (kind == (an_il_entry_kind)iek_routine &&
        is_or_contains_member_of_uncompleted_class(
                                         ((a_routine_ptr)ptr)->type)) {
      /* This appears to be an instantiation triggered by a friend
         declaration.  The source sequence entry specifying the explicit
         specialization (by which the instantiation is represented) has to
         appear after the class definition is complete, so don't add the
         entry to the source sequence list at this time; it will be done
         later. */
#if DEBUG
      if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
        fputs("deferring addition to ss-list for", f_debug);
        fputs(" partial instantiation of \"", f_debug);
        db_name(&((a_routine_ptr)ptr)->source_corresp);
        fputs("\"\n", f_debug);
      }  /* if */
#endif /* if DEBUG */
      sym->variant.routine.instance_ptr->partial_instantiation = ssep;;
    } else
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
    /* Do not insert code here. */
    {
      a_scope_depth  depth;

      /* Add the entry to the appropriate source sequence list. */
      depth = find_instantiation_insert_scope(&scope_stack[depth_scope_stack],
                                              ssep);
      insert_src_seq_list(ssep, ssep, depth,
                          scope_stack[depth].
                                   ss_list_instantiation_insert_point);
      if (scp->source_sequence_entry == NULL) {
        scp->source_sequence_entry = ssep;
      }  /* if */
    }
    /* Restore the flag that controls whether source sequence entries are
       generated. */
    source_sequence_entries_disallowed =
          scope_stack[depth_scope_stack].source_sequence_entries_disallowed;
  }  /* if */
}  /* add_source_sequence_entry_for_partial_instantiation */

#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */

void reset_ss_list_instantiation_insert_point(void)
/*
Set the instantiation insert point associated with the current scope to
NULL.  This will cause source sequence entries representing instantiations
to be added to the end of the current scope's source sequence list.
*/
{
  scope_stack[depth_scope_stack].ss_list_instantiation_insert_point = NULL;
}  /* reset_ss_list_instantiation_insert_point */


a_source_sequence_entry_ptr last_matching_source_sequence_entry(char  *entity)
/*
Find the tail-most source sequence entry in the current source-sequence-list
that refers to the IL entity identified by entity.
*/
{
  a_source_sequence_entry_ptr  ssep, prev_ssep;
  char                         *temp;

  if (source_sequence_entries_disallowed) {
    ssep = NULL;
  } else {
    /* Determine whether the source-sequence entry we are looking for will
       be on the main list of the current source-sequence-list scope or on
       a sublist. */
    ssep = scope_stack[depth_scope_stack].end_of_source_sequence_list;
    for (; ssep != NULL; ssep = prev_ssep) {
      prev_ssep = ssep->prev;
      /* Stop if ssep refers to entity either directly or through a
         secondary-decl entry. */
      temp = ssep->entity.ptr;
      if (temp == entity ||
          ((ss_entry_kind(ssep) ==
                     (an_il_entry_kind)iek_src_seq_secondary_decl) &&
           ((a_src_seq_secondary_decl_ptr)temp)->entity.ptr == entity)) {
        break;
      }  /* if */
      /* Back up to the preceding entry on the main list. */
    }  /* for */
  }  /* if */
  return ssep;
}  /* last_matching_source_sequence_entry */


a_src_seq_secondary_decl_ptr set_src_seq_secondary_decl_fields(
                                             char              *il_entry_ptr,
                                             a_type_ptr        declared_type,
                                             an_sssd_flag_set  flags)
/*
Set the declared_type field and various flags in the secondary-decl source
sequence entry associated with il_entry_ptr; the source sequence entry is
used that matches il_entry_ptr and is closest to the end of the source
sequence list of the current scope stack entry.  declared_type may be
NULL.  flags is a bit vector whose non-zero bits correspond to bit fields
in the secondary source sequence entry that need to be set.
*/
{
  a_source_sequence_entry_ptr   ssep;
  a_src_seq_secondary_decl_ptr  sssdp = NULL;

  if (source_sequence_entries_disallowed) {
    /* We are in a context in which source sequence entries are not being
       created.  No further action is required. */
  } else {
    ssep = last_matching_source_sequence_entry(il_entry_ptr);
    if (ssep != NULL) {
      check_assertion(ss_entry_kind(ssep) == iek_src_seq_secondary_decl);
      sssdp = (a_src_seq_secondary_decl_ptr)ssep->entity.ptr;
      if (declared_type != NULL) sssdp->declared_type = declared_type;
      if (flags & SSSD_AUTONOMOUS_TAG_DECL) {
        sssdp->autonomous_tag_decl = TRUE;
      }  /* if */
      if (flags & SSSD_FRIEND_DECL) {
        sssdp->friend_decl = TRUE;
      }  /* if */
      if (flags & SSSD_IMPLICIT_DECL) {
        sssdp->implicit_decl = TRUE;
      }  /* if */
      if (flags & SSSD_DECLARED_IN_FUNC_PROTOTYPE) {
        sssdp->declared_in_func_prototype = TRUE;
      }  /* if */
      if (flags & SSSD_SPECIALIZED_WITH_NEW_SYNTAX) {
        sssdp->specialized_with_new_syntax = TRUE;
      }  /* if */
      if (flags & SSSD_FIRST_DECLARATION) {
        sssdp->first_declaration = TRUE;
      }  /* if */
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
      if (flags & SSSD_IS_PARTIAL_INSTANTIATION) {
        sssdp->is_partial_instantiation = TRUE;
      }  /* if */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#if GNU_EXTENSIONS_ALLOWED
      if (flags & SSSD_MARKED_AS_GNU_EXTENSION) {
        sssdp->marked_as_gnu_extension = TRUE;
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    }  /* if */
  }  /* if */
  return sssdp;
}  /* set_src_seq_secondary_decl_fields */

#endif /* !STANDALONE_UTILITY_PROGRAM */

a_type_ptr type_from_src_seq_declaration(a_source_sequence_entry_ptr ssep)
/*
ssep points to a source sequence entry.  If it points to a normal declaration
(specifically, one that can appear in a comma list), fetch the type of the
declared entity and return it.  Otherwise, return NULL.  If ssep is NULL,
return NULL.
*/
{
  a_type_ptr                   tp;
  a_src_seq_secondary_decl_ptr sssdp;

  if (ssep == NULL) {
    tp = NULL;
  } else {
    switch (ss_entry_kind(ssep)) {
      case iek_variable:
        tp = ss_entry_ptr(ssep, a_variable_ptr)->type;
        break;
      case iek_routine:
        tp = ss_entry_ptr(ssep, a_routine_ptr)->type;
        break;
      case iek_type:
        tp = ss_entry_ptr(ssep, a_type_ptr);
        if (tp->kind == (a_type_kind)tk_typeref) {
          /* For a typedef, drop the typedef itself to get to the declared
             type of the typedef. */
          tp = tp->variant.typeref.type;
        }  /* if */
        break;
      case iek_field:
        tp = ss_entry_ptr(ssep, a_field_ptr)->type;
        break;
      case iek_constant:
        tp = ss_entry_ptr(ssep, a_constant_ptr)->type;
        break;
      case iek_src_seq_secondary_decl:
        sssdp = ss_entry_ptr(ssep, a_src_seq_secondary_decl_ptr);
        if (sssdp->entity.kind == (a_byte_il_entry_kind)iek_variable ||
            sssdp->entity.kind == (a_byte_il_entry_kind)iek_routine) {
          tp = sssdp->declared_type;
          break;
        } else if (sssdp->entity.kind == (a_byte_il_entry_kind)iek_type) {
          tp = (a_type_ptr)sssdp->entity.ptr;
          if (tp->kind == (a_type_kind)tk_typeref) {
            /* For a typedef, drop the typedef itself to get to the declared
               type of the typedef. */
            tp = tp->variant.typeref.type;
          }  /* if */
          break;
        }  /* if */
        /* FALLTHROUGH */
      default:
        tp = NULL;
    }  /* switch */
  }  /* if */
  return tp;
}  /* type_from_src_seq_declaration */

#if !STANDALONE_UTILITY_PROGRAM

static a_source_sequence_entry_ptr find_src_seq_secondary_decl_entry(
                                     a_source_sequence_entry_ptr  ssep,
                                     char                         *entity_ptr)
/*
Walk the source-sequence list starting at ssep and return the first
secondary-decl source sequence entry that is associated with the IL entity
whose address is the same as entity_ptr.  If none is found, return NULL.
*/
{
  a_src_seq_secondary_decl_ptr  sssdp;

  for (ssep = ssep->next; ssep != NULL; ssep = ssep->next) {
    if (ss_entry_kind(ssep) == (an_il_entry_kind)iek_src_seq_secondary_decl) {
      sssdp = ss_entry_ptr(ssep, a_src_seq_secondary_decl_ptr);
      if (sssdp->entity.ptr == entity_ptr) {
        /* A match.  Break and return ssep. */
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return ssep;
}  /* find_src_seq_secondary_decl_entry */

#if MAINTAIN_NEEDED_FLAGS

static a_source_sequence_entry_ptr drop_tag_def_from_src_seq_list(
                                     a_source_sequence_entry_ptr  ssep,
                                     a_boolean                    retain_first)
/*
ssep is a source sequence entry representing the definition of a class or
enum type -- i.e., it will be followed by zero or more entries and then by an
end-of-construct entry that points back to the same type to which ssep points.
If retain_first is FALSE, remove all the entries from the source sequence
list; if retain_first is TRUE, leave the first in the list and remove the
others.  Note: this is not a general purpose routine but is rather part of
the processing that prunes the IL based on settings of the keep_in_il and
keep_definition_in_il flags.  Among other things, it assumes the list to
which the entries belong is the file-scope source sequence list.  It also
may do fixup on entities pointed to by source-sequence entries it removes.
*/
{
  a_type_ptr                   type_ptr = (a_type_ptr)ssep->entity.ptr;
  a_source_sequence_entry_ptr  prev_ssep, *prev_link_addr;
  a_src_seq_secondary_decl_ptr sssdp;

  db_enter(4, "drop_tag_def_from_src_seq_list");
  type_ptr = ss_entry_ptr(ssep, a_type_ptr);
  check_assertion_str(ss_entry_kind(ssep) == (an_il_entry_kind)iek_type &&
                      (is_immediate_class_type(type_ptr) ||
                       is_immediate_enum_type(type_ptr)),
                      "drop_tag_def_from_src_seq_list: bad entity kind");
  /* The source sequence entries will be removed by linking around them.
     Since source-sequence entries have a prev pointer, we need to remember
     what to point back to. */ 
  if (retain_first) {
    /* ssep itself is to be retained, so the prev pointer will point back to
       it when its successors are removed. */
    prev_ssep = ssep;
  } else {
    /* ssep is not to be retained, so remember its prev link. */
    prev_ssep = ssep->prev;
  }  /* if */
  /* Save the address from which the "linking around" will start. */
  if (prev_ssep != NULL) {
    prev_link_addr = &prev_ssep->next;
  } else {
    /* We will be linking around the head of the list.  Note the assumption
       that it is the file-scope list. */
    prev_link_addr = &scope_stack[DEPTH_OF_FILE_SCOPE].il_scope->
                                                        source_sequence_list;
    check_assertion(*prev_link_addr == ssep);
  }  /* if */
  /* Loop until the end-of-construct entry corresponding to ssep is found. */
  ssep = ssep->next;
  for (;;) {
    if (ss_entry_kind(ssep) ==
                   (an_il_entry_kind)iek_src_seq_end_of_construct &&
        ss_entry_ptr(ssep, a_src_seq_end_of_construct_ptr)->
                                           entity.ptr == (char *)type_ptr) {
      /* Found -- stop looping. */
      break;
    }  /* if */
    if (ss_entry_kind(ssep) == (an_il_entry_kind)iek_pragma
#if RECORD_MACROS_IN_IL
        || ss_entry_kind(ssep) == (an_il_entry_kind)iek_macro
#endif /* RECORD_MACROS_IN_IL */
        || (ss_entry_kind(ssep) == (an_il_entry_kind)iek_template &&
            !ss_entry_ptr(ssep, a_template_ptr)->
                                   source_corresp.is_class_member)
                                                                  ) {
      if (il_entry_prefix_of(ssep->entity.ptr).keep_in_il) {
        /* Link around a needed macro or pragma that appears inside this
           class/struct/union body. */
        *prev_link_addr = ssep;
        ssep->prev = prev_ssep;
        prev_ssep = ssep;
        prev_link_addr = &ssep->next;
      }  /* if */
    } else if (C_mode()) {
      /* Special processing in C mode, which does not have nested structs and
         enums in the sense that C++ does. */
      if (il_entry_prefix_of(ssep).keep_in_il) {
        /* A struct or enum definition that should be retained in the IL. */
        a_type_ptr  tp;
        a_boolean   is_primary_decl;

        if (ss_entry_kind(ssep) == (an_il_entry_kind)iek_type) {
          tp = ss_entry_ptr(ssep, a_type_ptr);
          is_primary_decl = TRUE;
        } else {
          /* A secondary-decl source sequence entry. */
          is_primary_decl = FALSE;
          check_assertion_str2(ss_entry_kind(ssep) ==
                                 (an_il_entry_kind)iek_src_seq_secondary_decl,
                               "drop_tag_def_from_src_seq_list:",
                               "bad entity kind");
          sssdp = ss_entry_ptr(ssep, a_src_seq_secondary_decl_ptr);
          check_assertion(sssdp->entity.kind ==
                                          (a_byte_il_entry_kind)iek_type);
          tp = (a_type_ptr)sssdp->entity.ptr;
#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
          if (tp->kind == (a_type_kind)tk_typeref) {
            check_assertion(is_class_struct_union_type(tp));
            /* No need to keep this entry in the IL.  This is a nonstandard
               case in which a struct is incorporated into another by
               means of a typeref reference -- e.g.,
                 typedef struct { int i,j } S;
                 struct X {
                   S;       // has the effect of making i and j members of X
                 };
               Reference to it may be removed from the source-sequence list. */
            ssep = ssep->next;
            continue;
          }  /* if */
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */
        }  /* if */
        check_assertion_str(is_immediate_class_type(tp) ||
                            is_immediate_enum_type(tp),
                            "drop_tag_def_from_src_seq_list: bad type kind");
        /* Link around the entries that have been seen thus far, skip the
           entries entailed by the struct or enum definition that should be
           retained, and then resume the processing in the outer loop. */
        *prev_link_addr = ssep;
        ssep->prev = prev_ssep;
        if (!is_primary_decl) {
          /* Not a definition. */
          prev_ssep = ssep;
          prev_link_addr = &ssep->next;
          /* Mark it as autonomous -- it was probably part of a declaration
             that is being eliminated. */
          sssdp->autonomous_tag_decl = TRUE;
        } else {
          ssep = ssep->next;
          for (;;) {
            if (!il_entry_prefix_of(ssep).keep_in_il) {
              /* An unneeded struct/enum definition embedded within the needed
                 one.  Remove it.  Note that ssep will, upon return from
                 the recursive call, point to the entry immediately following
                 the end-of-construct of the definition being removed. */
              ssep = drop_tag_def_from_src_seq_list(ssep,
                                                    /*retain_first=*/FALSE);
              /* Reset the prev-link state. */
              prev_ssep = ssep->prev;
              prev_link_addr = &ssep->prev->next;
            } else if (ss_entry_kind(ssep) ==
                           (an_il_entry_kind)iek_src_seq_end_of_construct &&
                       ss_entry_ptr(ssep, a_src_seq_end_of_construct_ptr)->
                                                  entity.ptr == (char *)tp) {
              /* We've located the end-of-construct entry for the struct/enum
                 definition.  Reset the prev-link state and break out of the
                 loop. */
              prev_ssep = ssep;
              prev_link_addr = &ssep->next;
              tp->autonomous_primary_tag_decl = TRUE;
              break;
            } else {
              /* Keep going. */
              ssep = ssep->next;
            }  /* if */
          }  /* for */
        }  /* if */
      }  /* if */
    } else {
      /* C++ mode.  If this represents a friend function declaration, reset
         the routine's source-sequence entry, if appropriate. */
      if (ss_entry_kind(ssep) ==
                    (an_il_entry_kind)iek_src_seq_secondary_decl) {
        sssdp = ss_entry_ptr(ssep, a_src_seq_secondary_decl_ptr);
        if (sssdp->friend_decl) {
          if (sssdp->entity.kind == (a_byte_il_entry_kind)iek_routine) {
            a_routine_ptr rp = (a_routine_ptr)sssdp->entity.ptr;
            if (rp->source_corresp.source_sequence_entry == ssep) {
              rp->source_corresp.source_sequence_entry =
                   find_src_seq_secondary_decl_entry(ssep, sssdp->entity.ptr);
            }  /* if */
          }  /* if */
        } else if (sssdp->first_declaration &&
                   il_entry_prefix_of(ssep).keep_in_il) {
          /* Link around a needed type declaration that appears inside this
             class/struct/union body. */
          check_assertion(sssdp->entity.kind ==
                                          (a_byte_il_entry_kind)iek_type &&
                          !((a_type_ptr)sssdp->entity.ptr)->
                                              source_corresp.is_class_member);
          *prev_link_addr = ssep;
          ssep->prev = prev_ssep;
          prev_ssep = ssep;
          prev_link_addr = &ssep->next;
          /* Mark it as autonomous -- it was probably part of a declaration
             that is being eliminated. */
          sssdp->autonomous_tag_decl = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
    ssep = ssep->next;
  }  /* for */
  /* Now set the pointers to effect linking around the entries that were to
     be removed. */
  *prev_link_addr = ssep->next;
  if (ssep->next != NULL) ssep->next->prev = prev_ssep;
  db_exit();

  /* Return the next entry. */
  return ssep->next;
}  /* drop_tag_def_from_src_seq_list */

#endif /* MAINTAIN_NEEDED_FLAGS */

static a_source_sequence_entry_ptr drop_from_fs_src_seq_list(
                                             a_source_sequence_entry_ptr  ssep)

/*
Remove ssep from the file-scope source sequence list.  If ssep corresponds to
the start of a class or enum definition, also remove all the source sequence
entries up to and including the corresponding end-of-construct entry.  Return
the source sequence entry that follows the entry or entries removed.
*/
{
  a_source_sequence_entry_ptr  next_ssep;
  a_scope_ptr                  file_scope;

  db_enter(5, "drop_from_fs_src_seq_list");
  if (ssep->entity.kind == (a_byte_il_entry_kind)iek_type &&
      (is_immediate_class_type((a_type_ptr)ssep->entity.ptr) ||
       is_immediate_enum_type((a_type_ptr)ssep->entity.ptr))) {
#if MAINTAIN_NEEDED_FLAGS
    /* It's a class or enum definition.  Remove everything from here through
       to the end-of-construct entry. */
    next_ssep = drop_tag_def_from_src_seq_list(ssep, /*retain_first=*/FALSE);
#else /* !MAINTAIN_NEEDED_FLAGS */
    unexpected_condition();
#endif /* MAINTAIN_NEEDED_FLAGS */
  } else {
    /* Link around ssep and return its successor in the list. */
    file_scope = scope_stack[DEPTH_OF_FILE_SCOPE].il_scope;
    next_ssep = ssep->next;
    f_remove_from_src_seq_list(ssep,
                               file_scope->source_sequence_list == NULL ?
                                 depth_innermost_namespace_scope :
                                 NO_SCOPE_DEPTH);
  }  /* if */
  db_exit();
  return next_ssep;
}  /* drop_from_fs_src_seq_list */


void eliminate_function_body_source_sequence_entries(a_scope_ptr  sp)
/*
Remove the source sequence entries that represent the body of the function
associated with the indicated sck_function scope.
*/
{
  a_routine_ptr                 rp;
  a_source_sequence_entry_ptr   ssep;
  a_src_seq_secondary_decl_ptr  sssdp;

  rp = sp->variant.routine.ptr;
  ssep = rp->source_corresp.source_sequence_entry;
  if (ssep != NULL) {
    a_source_correspondence  *scp = &rp->source_corresp;
    a_memory_region_number   region_to_switch_back_to;
    switch_to_file_scope_region(&region_to_switch_back_to);
    if (rp->defined_outside_of_parent &&
        !(scp->is_class_member && rp->is_specialized &&
          rp->template_arg_list == NULL)) {
      /* Definition of a class member outside the class definition or
         a namespace member outside the namespace definition.  Just
         drop the source sequence entry altogether.  This should not
         be done for the specialization of a member of a class template,
         because there is no secondary source sequence entry to fall
         back on. */
#if DEBUG
      if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
        fputs("dropping: ", f_debug);
        db_source_sequence_entry(ssep);
      }  /* if */
#endif /* DEBUG */
      (void)drop_from_fs_src_seq_list(ssep);
      /* The source-sequence entry pointer in the routine needs to be
         reset as though the definition had never happened.  This means
         finding its non-defining declaration within the class or
         namespace definition. */
      /* Loop through the source sequence list, starting at the point
         corresponding to the beginning of the class or namespace
         definition, until a secondary declaration entry pointing to
         the same routine is found. */
      if (scp->is_class_member) {
        ssep = scp->parent.class_type->
                             source_corresp.source_sequence_entry;
      } else {
        ssep = scp->parent.namespace_ptr->
                             source_corresp.source_sequence_entry;
      }  /* if */
      ssep = find_src_seq_secondary_decl_entry(ssep, (char *)rp);
      check_assertion_str2(ssep != NULL,
                           "eliminate_function_body_source_sequence_entries:",
                           "source sequence secondary decl not found");
      /* Reset the source sequence entry pointer in the routine entry. */
      scp->source_sequence_entry = ssep;
    } else {
      /* Turn the associated source sequence entry into a secondary-decl
         source sequence entry.  This is done even though the entry
         may be thrown away later, since it is easier to do it at this
         point than later, when we decide whether the routine entry
         itself will be kept. */
      check_assertion(ssep->entity.ptr == (char *)rp);
      sssdp = alloc_src_seq_secondary_decl();
      sssdp->entity = ssep->entity;
      ssep->entity.ptr = (char *)sssdp;
      ssep->entity.kind =
                    (a_byte_il_entry_kind)iek_src_seq_secondary_decl;
      sssdp->decl_position = rp->source_corresp.decl_position;
      /* Move the declared type pointer from the routine into the
         source-sequence entry, clearing the routine's pointer (since
         rp no longer represents a definition). */
      sssdp->declared_type = rp->declared_type;
      rp->declared_type = NULL;
      sssdp->friend_decl = rp->defined_in_friend_decl;
      rp->defined_in_friend_decl = FALSE;
      if (!C_mode() && sp->src_seq_sublist_list != NULL) {
        /* If any tags were introduced in the parameter declarations for
           this function, the associated source-sequence entries need to
           be promoted from the function-scope list (they'd be on a
           sublist) to the file-scope list.  For example (assuming f's
           definition is unneeded but that S must be kept in the IL):
             void f(struct S *ps) { ... }
           the secondary-decl entry for S must be inserted immediately
           after the secondary-decl entry for f (i.e., the one just
           created). */
        a_src_seq_sublist_ptr        sublist = sp->src_seq_sublist_list;
        a_source_sequence_entry_ptr  insert_ssep = ssep;
        a_source_sequence_entry_ptr  sublist_ssep, next_sublist_ssep;
        a_boolean                    remove_from_sublist;

        for (; sublist != NULL; sublist = sublist->next) {
          for (sublist_ssep = sublist->source_sequence_list;
               sublist_ssep != NULL;
               sublist_ssep = next_sublist_ssep) {
            next_sublist_ssep = sublist_ssep->next;
            if (ss_entry_kind(sublist_ssep) ==
                                   (an_il_entry_kind)iek_pragma
#if RECORD_MACROS_IN_IL
                || ss_entry_kind(sublist_ssep) == (an_il_entry_kind)iek_macro
#endif /* RECORD_MACROS_IN_IL */
                                                               ) {
              /* Ignore entries representing macros and pragmas. */
              continue;
            }  /* if */
            if (ss_entry_kind(sublist_ssep) !=
                       (an_il_entry_kind)iek_src_seq_secondary_decl) {
              goto done_with_func_prototype_decls;
            }  /* if */
            sssdp = ss_entry_ptr(sublist_ssep,
                                 a_src_seq_secondary_decl_ptr);
            if (!sssdp->declared_in_func_prototype) {
              goto done_with_func_prototype_decls;
            }  /* if */
            remove_from_sublist = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
            if (microsoft_mode && (rp->decl_modifiers & DM_DLLIMPORT)) {
              remove_from_sublist = TRUE;
            }  /* if */
#endif /* if MICROSOFT_EXTENSIONS_ALLOWED */
#if MAINTAIN_NEEDED_FLAGS
            if (il_entry_prefix_of(sssdp->entity.ptr).keep_in_il) {
              /* Be sure the keep-in-IL flags are set on the source
                 sequence information that's being promoted to the file
                 scope list. */
              il_entry_prefix_of(sublist_ssep).keep_in_il = TRUE;
              il_entry_prefix_of(sssdp).keep_in_il = TRUE;
              check_assertion(remove_from_sublist == FALSE);
              remove_from_sublist = TRUE;
            }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
            if (remove_from_sublist) {
              /* Remove the source sequence entry from the list in the
                 function scope. */
              if (sublist_ssep->prev == NULL) {
                sublist->source_sequence_list = next_sublist_ssep;
              } else {
                sublist_ssep->prev->next = next_sublist_ssep;
              }  /* if */
              if (next_sublist_ssep != NULL) {
                next_sublist_ssep->prev = sublist_ssep->prev;
              }  /* if */
              /* Add it to the source sequence list of the file scope,
                 inserting it immediately following insert_ssep. */
              sublist_ssep->next = insert_ssep->next;
              if (insert_ssep->next != NULL) {
                insert_ssep->next->prev = sublist_ssep;
              } else {
                a_scope_stack_entry_ptr  scope_stack_ptr;
                scope_stack_ptr = &scope_stack[DEPTH_OF_FILE_SCOPE];
                if (scope_stack_ptr->il_scope->source_sequence_list == NULL) {
                  scope_stack_ptr =
                           &scope_stack[depth_innermost_namespace_scope];
                  check_assertion(scope_stack_ptr->
                                   end_of_source_sequence_list == insert_ssep);
                  scope_stack_ptr->end_of_source_sequence_list = sublist_ssep;
                }  /* if */
              }  /* if */
              insert_ssep->next = sublist_ssep;
              sublist_ssep->prev = insert_ssep;
              /* Adjust insert_ssep to point to the entry just added, so
                 that the next one will be added right after it. */
              insert_ssep = sublist_ssep;
            }  /* if */
          }  /* for */
        }  /* for */
done_with_func_prototype_decls:;
      }  /* if */
    }  /* if */
    switch_back_to_original_region(region_to_switch_back_to);
  }  /* if */
  rp->defined_outside_of_parent = FALSE;
}  /* eliminate_function_body_source_sequence_entries */

#if MAINTAIN_NEEDED_FLAGS

void eliminate_class_body_source_sequence_entries(a_type_ptr  class_type)
/*
Remove the source sequence entries that represent the body of the indicated
class type.
*/
{
  a_source_sequence_entry_ptr   ssep;
  a_src_seq_secondary_decl_ptr  sssdp;

  /* The source sequence entry pointing to the class_type should be changed to
     a secondary source sequence entry, since only definitions have primary
     source sequence entries. */
  ssep = class_type->source_corresp.source_sequence_entry;
  if (ssep != NULL) {
    if (class_type->variant.class_struct_union.
                         nested_class_defined_outside_of_parent &&
        !class_type->variant.class_struct_union.is_template_class) {
      /* This is a nested class defined outside the definition of its parent
         class.  Remove from the file-scope source-sequence list the entries
         representing the definition. */
#if CHECKING
      /* This won't work for local classes. */
      check_assertion_str2(!class_type->source_corresp.is_local_to_function,
                           "turn_class_definition_into_declaration:",
                           "local classes not supported");
#endif /* CHECKING */
      (void)drop_tag_def_from_src_seq_list(ssep, /*retain_first=*/FALSE);
      /* Now reset the source-sequence entry in class_type to refer to the
         non-defining declaration inside the definition of its parent.  Start
         at the point in the source sequence list corresponding to the
         beginning of the class definition, and loop through the list till a
         secondary declaration pointing to class_type is found. */
      ssep = class_type->source_corresp.parent.class_type->
                                     source_corresp.source_sequence_entry;
      ssep = find_src_seq_secondary_decl_entry(ssep, (char *)class_type);
      check_assertion_str2(ssep != NULL,
                           "turn_class_definition_into_declaration:",
                           "source sequence secondary decl not found");
      /* Reset the source sequence entry pointer in the type entry. */
      class_type->source_corresp.source_sequence_entry = ssep;
    } else {
      check_assertion(ss_entry_ptr(ssep, a_type_ptr) == class_type);
      /* This is either a non-nested class or a nested class defined within
         the definition of its parent class.  This time, remove the entries
         representing the definition *except* the first, which will be
         transformed to represent a secondary declaration now that the
         definition has been eliminated. */
      (void)drop_tag_def_from_src_seq_list(ssep, /*retain_first=*/TRUE);
      /* Turn what was originally a definition into a secondary declaration
         (a nondefining class declaration) as far as the source-sequence
         representation is concerned. */
      sssdp = alloc_src_seq_secondary_decl();
      sssdp->entity = ssep->entity;
      ssep->entity.ptr = (char *)sssdp;
      ssep->entity.kind = (a_byte_il_entry_kind)iek_src_seq_secondary_decl;
      sssdp->decl_position = class_type->source_corresp.decl_position;
      sssdp->declared_type = class_type;
      sssdp->autonomous_tag_decl = TRUE;
      sssdp->first_declaration =
          symbol_supplement_for_class(class_type)->definition_is_first_decl;
    }  /* if */
  }  /* if */
}  /* eliminate_class_body_source_sequence_entries */


static void mark_func_prototype_decl_tags_autonomous(
                                             a_source_sequence_entry_ptr  ssep)
/*
ssep is a source sequence entry immediately following an entry representing
a secondary routine declaration.  Moreover, ssep's predecessor and the
associated routine entry have been eliminated, but tags introduced into the
program by that declaration may have to be retained in the IL.  If so, they
need to be marked as autonomous.  For instance:
  void f(struct A *);
  struct A * pa;
Here we assume f is never called and that the associated routine entry is
removed from the IL.  But A is still needed, so the source-sequence entry for
it needs to be marked as autonomous.
*/
{
  a_src_seq_secondary_decl_ptr  sssdp;

  /* Loop through ssep and its successors, checking for secondary tag
     declarations that are marked as having been declared in a function
     prototype. */
  for (; ssep != NULL; ssep = ssep->next) {
    if (ss_entry_kind(ssep) == (an_il_entry_kind)iek_pragma
#if RECORD_MACROS_IN_IL
        || ss_entry_kind(ssep) == (an_il_entry_kind)iek_macro
#endif /* RECORD_MACROS_IN_IL */
                                                                   ) {
      /* Ignore entries representing macros and pragmas. */
      continue;
    }  /* if */
    if (ss_entry_kind(ssep) != (an_il_entry_kind)iek_src_seq_secondary_decl) {
      /* First entry that is not a secondary-decl entry -- we must be past the
         function prototype declarations.  Stop looping. */
      break;
    }  /* if */
    sssdp = ss_entry_ptr(ssep, a_src_seq_secondary_decl_ptr);
    if (!sssdp->declared_in_func_prototype) {
      /* Not a function prototype declaration.  Stop looping. */
      break;
    }  /* if */
    /* A match.  Clear the one flag and set the other. */
    sssdp->declared_in_func_prototype = FALSE;
    sssdp->autonomous_tag_decl = TRUE;
  }  /* for */
}  /* mark_func_prototype_decl_tags_autonomous */


static a_source_sequence_entry_ptr src_seq_check_for_non_autonomous_tag(
                                             a_source_sequence_entry_ptr ssep)
/*
ssep points to a source sequence entry encountered while removing unneeded IL
entries; it is not itself to be removed from the IL.  If it is the end of a
definition of a tag and if the type with which it is associated is not marked
autonomous (or if it is a nonautonomous secondary tag declaration) the
definition (or declaration) is part of the declaration of another entity. But
if it turns out that the latter should be removed from the IL, the tag itself
(if this is a definition, otherwise the secondary declaration entry) should
be marked autonomous: that is the purpose of this routine.  Since it may skip
unneeded entities, it returns a pointer to the next in the list. Even if it
does nothing in terms of setting the autonomous flag, it at least returns the
successor of ssep.
*/
{
  a_source_sequence_entry_ptr     next_ssep = ssep->next;
  a_src_seq_end_of_construct_ptr  sseocp;
  a_src_seq_secondary_decl_ptr    sssdp = NULL;
  a_type_ptr                      tag_type = NULL, tp;
  a_boolean                       is_unnamed_enum_def = FALSE;

  switch (ss_entry_kind(ssep)) {
    case iek_src_seq_end_of_construct:
      sseocp = ss_entry_ptr(ssep, a_src_seq_end_of_construct_ptr);
      if (sseocp->entity.kind == (a_byte_il_entry_kind)iek_type) {
        /* ssep is the end of a tag definition. */
        tag_type = (a_type_ptr)sseocp->entity.ptr;
        if (tag_type->autonomous_primary_tag_decl ||
            tag_type->declared_in_function_prototype) {
          tag_type = NULL;
        } else if (is_immediate_enum_type(tag_type) &&
                   is_unnamed_or_originally_unnamed_tag(tag_type)) {
          is_unnamed_enum_def = TRUE;
        }  /* if */
      }  /* if */
      break;
    case iek_src_seq_secondary_decl:
      sssdp = ss_entry_ptr(ssep, a_src_seq_secondary_decl_ptr);
      if (!sssdp->autonomous_tag_decl && !sssdp->declared_in_func_prototype) {
        if (sssdp->entity.kind == (a_byte_il_entry_kind)iek_type) {
          /* ssep is a nonautonomous secondary declaration of a tag or
             typedef.  We're only interested in the former. */
          tag_type = (a_type_ptr)sssdp->entity.ptr;
          if (tag_type->kind == (a_type_kind)tk_typeref) {
            tag_type = NULL;
#if CHECKING
          } else {
            check_assertion(is_immediate_class_type(tag_type) ||
                            is_immediate_enum_type(tag_type));
#endif /* CHECKING */
          }  /* if */
        }  /* if */
      }  /* if */
      break;
    default:;
      /* Leave tag_type NULL. */
  }  /* switch */
  /* Note: except in cfront mode, an unnamed class tag cannot be made
     autonomous. */
  if (tag_type != NULL &&
      (any_cfront_mode() || is_unnamed_enum_def ||
       !is_unnamed_or_originally_unnamed_tag(tag_type))) {
    /* This is a nonautonomous tag declaration (possibly a definition).  The
       tag is kept in the IL -- but what if the entity to whose declaration it
       belongs is eliminated?  We need special handling for cases like this:
         static struct S { int i; } s;
       where s can be eliminated but struct S must be kept.  Without s in
       the IL we have to mark the entry for struct S as defined in an
       autonomous declaration.  In C++ and usually in C, the entity is
       next in the list. */
    /* Note: we only examine the first entry after the class/struct/union
       or named enum tag declaration or definition.  For instance, in a
       case like this:
         static struct S { int i; } x, y, z;
       (where x is eliminated) it will be treated as though it had originally
       been written as:
         static struct S { int i; } x;
         static struct S y, z;
       This is necessary because of complications in recognizing when a comma
       list ends.  For example,
         static struct S { int i; } x;
         struct S *f(void);
    */
    a_boolean  make_autonomous = FALSE;
#if CHECKING
    a_boolean  okay_if_not_found = C_mode() ||
                                   (sssdp != NULL &&
                                    sssdp->declared_in_func_prototype);
#endif /* CHECKING */

#if DEBUG
    if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
      fputs("checking nonautonomous tag: ", f_debug);
      db_source_sequence_entry(ssep);
    }  /* if */
#endif /* DEBUG */
check_next_ssep:
    /* Note: we may loop back to this point for the special case of an
       unnamed enum definition. */
    while (next_ssep != NULL &&
           (ss_entry_kind(next_ssep) == (an_il_entry_kind)iek_pragma
#if RECORD_MACROS_IN_IL
            || ss_entry_kind(next_ssep) == (an_il_entry_kind)iek_macro
#endif /* RECORD_MACROS_IN_IL */
                                                                      )) {
      /* No macros or pragmas that are added to the IL are eliminated;
         skip over any that intervene between the struct/enum definition
         and whatever follows. */
#if DEBUG
      if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
        fputs("skipping: ", f_debug);
        db_source_sequence_entry(next_ssep);
      }  /* if */
#endif /* DEBUG */
      next_ssep = next_ssep->next;
    }  /* while */
    if (next_ssep == NULL) {
      /* There is no successor source-sequence entry, so the declaration
         represented by ssep has to be marked as autonomous. */
      check_assertion_str2(okay_if_not_found,
                           "src_seq_check_for_non_autonomous_tag:",
                           "no next entry");
      make_autonomous = TRUE;
    } else {
      /* See what kind of entity follows the tag definition; get the type
         with which it was declared. */
      tp = type_from_src_seq_declaration(next_ssep);
      if (tp == NULL || !same_entities(find_bottom_of_type(tp), tag_type)) {
        /* This is not an entity that was declared with the tag; the tag
           should be marked as autonomous.  Sometimes this will not be
           quite right -- some weird cases in C mode, such as
             static void *x = (void *)(struct S { int i; }*)0;
           but it doesn't really make any difference. */
        /* This situation can also occur when a non-autonomous tag appears as
           a template argument of a class template (e.g., vector<class X>). */
        check_assertion_str2(okay_if_not_found ||
                             is_template_class_type(tp),
                             "src_seq_check_for_non_autonomous_tag:",
                             "type of next entry does not match");
        make_autonomous = TRUE;
      } else if (is_unnamed_enum_def) {
        /* Special handling for unnamed unnamed enum definitions. */
        if (il_entry_prefix_of(next_ssep).keep_in_il) {
          /* No need to make the enum declaration autonomous. */
        } else {
          /* Remove the entry from the source-sequence list and examine the
             next one. */
          next_ssep = drop_from_fs_src_seq_list(next_ssep);
#if CHECKING
          okay_if_not_found = TRUE;
#endif /* CHECKING */
          goto check_next_ssep;
        }  /* if */
      } else if (!il_entry_prefix_of(next_ssep).keep_in_il) {
        /* The successor source-sequence entry is not retained in the IL,
           so the declaration represented by ssep has to be marked as
           autonomous. */
        make_autonomous = TRUE;
      }  /* if */
    }  /* if */
    if (make_autonomous) {
      if (sssdp == NULL) {
        /* A tag definition -- set the flag in the type entry. */
        tag_type->autonomous_primary_tag_decl = TRUE;
      } else {
        /* Not a definition -- set the flag in the secondary decl entry. */
        sssdp->autonomous_tag_decl = TRUE;
        /* In case this was a function-prototype declaration, clear the
           flag, since apparently the function has been removed. */
        sssdp->declared_in_func_prototype = FALSE;
      }  /* if */
#if DEBUG
      if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
        fputs("marked autonomous: ", f_debug);
        db_source_sequence_entry(ssep);
      }  /* if */
#endif /* DEBUG */
    }  /* if */
  }  /* if */
  return next_ssep;
}  /* src_seq_check_for_non_autonomous_tag */


void eliminate_unneeded_source_sequence_entries(a_scope_ptr scope)
/*
Eliminate those entries on the source sequence list of the specified IL
scope that are not really needed in the IL.
*/
{
  /* Remove unneeded source-sequence entries. */
  a_source_sequence_entry_ptr     ssep, next_ssep;
  a_src_seq_secondary_decl_ptr    sssdp;

  for (ssep = scope->source_sequence_list; ssep != NULL; ssep = next_ssep) {
    /* The processing whereby the keep_in_il flag is set guarantees that
       the keep_in_il setting of the source sequence entry and that of the
       IL entry to which it corresponds will be the same. */
    if (!il_entry_prefix_of(ssep).keep_in_il) {
      a_byte_il_entry_kind  kind = ssep->entity.kind;
      check_assertion(!il_entry_prefix_of(ssep->entity.ptr).keep_in_il);
      if (kind == (a_byte_il_entry_kind)iek_src_seq_secondary_decl) {
        sssdp = (a_src_seq_secondary_decl_ptr)ssep->entity.ptr;
        kind = sssdp->entity.kind;
        check_assertion(!il_entry_prefix_of(sssdp->entity.ptr).keep_in_il);
        if (sssdp->declared_type != NULL &&
            is_function_type(sssdp->declared_type)) {
          eliminate_default_arg_object_lifetimes(sssdp->declared_type);
        }  /* if */
      } else {
        sssdp = NULL;
      }  /* if */
      if (kind == (a_byte_il_entry_kind)iek_variable ||
          kind == (a_byte_il_entry_kind)iek_routine ||
          kind == (a_byte_il_entry_kind)iek_type ||
          kind == (a_byte_il_entry_kind)iek_instantiation_directive) {
#if DEBUG
        if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
          fputs("dropping: ", f_debug);
          db_source_sequence_entry(ssep);
        }  /* if */
#endif /* DEBUG */
        next_ssep = drop_from_fs_src_seq_list(ssep);
        if (!C_mode() && sssdp != NULL &&
            kind == (a_byte_il_entry_kind)iek_routine) {
          /* ssep is a source-sequence entry representing a routine
             declaration (not a definition).  If any tag was introduced in
             its parameter list, it should be marked as autonomous.  Note
             that this is done in C++ mode only, not in C mode. */
          mark_func_prototype_decl_tags_autonomous(next_ssep);
        }  /* if */
      } else {
        /* Not removed even though the keep_in_il flag is FALSE. */
        next_ssep = ssep->next;
      }  /* if */
    } else {
      /* If this is the end of a tag-definition construct, it may be
         appropriate to change the autonomous flag in the type from FALSE
         to TRUE.  Similar processing may be done for secondary declarations
         of tags. */
      next_ssep = src_seq_check_for_non_autonomous_tag(ssep);
    }  /* if */
  }  /* for */
#if DEBUG
  if (db_active) {
    /* Display source sequence lists for debug purposes. */
    if (debug_level >= 3 || db_flag_is_set("dump_elim") ||
        db_flag_is_set("dump_ss") || db_flag_is_set("dump_ss_full")) {
      fputs("after elimination of unneeded entries, ", f_debug);
      db_ss_list_for_scope(scope);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
}  /* eliminate_unneeded_source_sequence_entries */


#endif /* MAINTAIN_NEEDED_FLAGS */
#endif /* !STANDALONE_UTILITY_PROGRAM */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1998 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
