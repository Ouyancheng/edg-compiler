/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
/*

il_walk.c -- Routines to walk the intermediate language tree.

*/

#include "basics.h"
#include "host_envir.h"

/* None of this is needed if not writing IL to a file. */
#if IL_SHOULD_BE_WRITTEN_TO_FILE

#include "il_walk.h"
#include "il.h"
#include "error.h"

#if ALTERNATE_IL_FILE_FORMAT
#include "il_file.h"
#endif /* ALTERNATE_IL_FILE_FORMAT */


static an_entry_process_function_ptr
		entry_process_func;
			/* The function to be called for each non-string entry.
			   NULL if no function is to be called. */
static a_string_entry_process_function_ptr
		string_entry_process_func;
			/* The function to be called for each string entry.
			   NULL if no function is to be called. */
static a_remap_function_ptr
		remap_func;
			/* The function to be used to remap each pointer
			   from an old value to a new value.  NULL if no
			   remapping is to be done. */
static a_boolean
		walk_subtree;
			/* If TRUE, walk_entry_and_subtree should walk the
			   subtree of the entry it is processing.  If FALSE,
			   just the entry is processed. */
static a_boolean
		walking_file_scope;
			/* TRUE if walking the file-scope IL, FALSE if
			   walking the IL for a function scope. */
#define NOT_SET_YET (-1)
static int	flag_value_meaning_visited = NOT_SET_YET;
			/* Value to be placed in the il_walk_flag field
			   to indicate that an entry has been visited.
			   The value alternates between 0 and 1.  NOT_SET_YET
			   (-1) means the value has not been set yet for the
			   current walk. */

typedef char	*a_char_ptr;
			/* Useful to indicate "char *" as a type in calling
			   remap_ptr or walk_ptr. */


/* Declarations required because of forward references. */
static void walk_entry_and_subtree(char             *entry_ptr,
                                   an_il_entry_kind entry_kind);
static void walk_string_entry(char             *entry_ptr,
                              an_il_entry_kind entry_kind,
                              sizeof_t         entry_length);

/*
Macro to remap a pointer from an "old" value to a "new" value.  ptr is
the pointer, ptr_type the type of ptr, and entry_kind is the kind of entry
pointed to.
*/
#define remap_ptr(ptr, ptr_type, entry_kind) \
{ if (remap_func != NULL) { \
    (ptr) = (ptr_type)remap_func((char *)(ptr), (entry_kind)); \
  } \
}

/*
Like remap_ptr, but used for "next" pointers in entries.  These are
remapped only if not processing subtrees (if subtrees are being processed,
walk_list handles the remapping when doing the parent of this entry).
*/
#define remap_next_ptr(ptr, ptr_type, entry_kind) \
{ if (!walk_subtree) remap_ptr((ptr), ptr_type, (entry_kind)); }

/*
Macro to remap a pointer to its new value, walk the subtree of the pointer
(if appropriate), and process the entry pointed to.  ptr is the pointer,
ptr_type is the type of ptr, and entry_kind is the kind of entry pointed to.
*/
#define walk_ptr(ptr, ptr_type, entry_kind) \
{ remap_ptr((ptr), ptr_type, (entry_kind)); \
  if (walk_subtree) walk_entry_and_subtree((char *)(ptr), (entry_kind)); \
}  /* walk_ptr */

/*
Macro similar to walk_ptr, but used for string entries.  ptr is the pointer
to the entry, entry_kind is the kind of entry pointed to, and entry_length is
the string length for iek_string_text entries, unused otherwise.
*/
#define walk_string_ptr(ptr, entry_kind, entry_length) \
{ remap_ptr((ptr), a_char_ptr, (entry_kind)); \
  if (walk_subtree) walk_string_entry((char *)(ptr), (entry_kind), \
                                      (sizeof_t)(entry_length)); \
}  /* walk_string_ptr */

/*
Process a list, each entry linked to the next by the "next" field.
ptr is the pointer to the list, ptr_type is the type of ptr, and entry_kind
is the kind of entries on the list.  If walking subtrees, each entry is
processed; if not, ptr is remapped but the list is not traversed.
*/
#define walk_list(ptr, ptr_type, entry_kind) \
{ if (walk_subtree) { \
    ptr_type *ptr_ptr = &(ptr); \
    for (; *ptr_ptr != NULL; ptr_ptr = &(*ptr_ptr)->next) { \
      walk_ptr(*ptr_ptr, ptr_type, (entry_kind)); \
    }  /* for */ \
  } else { \
    remap_ptr((ptr), ptr_type, (entry_kind)); \
  }  /* if */ \
}  /* walk_list */

/*
Process the source correspondence field pointed to by ptr.
*/
#define walk_source_corresp(ptr) walk_string_ptr((ptr).name, iek_id_name, 0)


static void walk_constant(a_constant_ptr ptr)
/*
Process the indicated constant entry.
*/
{
  walk_source_corresp(ptr->source_corresp);
  remap_next_ptr(ptr->next, a_constant_ptr, iek_constant);
  walk_ptr(ptr->type, a_type_ptr, iek_type);
  switch (ptr->kind) {
    case ck_error:
    case ck_integer:
    case ck_float:
      /* No pointers. */
      break;
    case ck_string:
      walk_string_ptr(ptr->variant.string.value, iek_string_text,
                      ptr->variant.string.length);
      break;
#ifdef FFE
    case ck_complex:
      walk_ptr(ptr->variant.complex_value, an_internal_complex_value_ptr,
               iek_internal_complex_value);
      break;
#endif /* ifdef FFE */
#ifdef CFE
    case ck_address:
      switch (ptr->variant.address.kind) {
        case abk_routine:
          /* Routines will be visited from the scope. */
          remap_ptr(ptr->variant.address.variant.routine, a_routine_ptr,
                    iek_routine);
          break;
        case abk_variable:
          /* Variables will be visited from the scope. */
          remap_ptr(ptr->variant.address.variant.variable, a_variable_ptr,
                    iek_variable);
          break;
        case abk_constant:
          /* Constants might not be on the scope constant list, so visit
             their subtrees. */
          walk_ptr(ptr->variant.address.variant.constant, a_constant_ptr,
                   iek_constant);
          break;
#if CHECKING
        default:
          internal_error("walk_constant: bad address const kind");
#endif /* CHECKING */
      }  /* switch */
      break;
#endif /* ifdef CFE */
    case ck_aggregate:
      walk_list(ptr->variant.aggregate.first_constant, a_constant_ptr,
                iek_constant);
      remap_ptr(ptr->variant.aggregate.last_constant, a_constant_ptr,
                iek_constant);
      break;
#ifdef FFE
    case ck_init_position:
      break;
    case ck_init_repeat:
      walk_ptr(ptr->variant.init_repeat.constant, a_constant_ptr,
               iek_constant);
      break;
#endif /* ifdef FFE */
#if CHECKING
    default:
      internal_error("walk_constant: bad constant kind");
#endif /* CHECKING */
  }  /* switch */
}  /* walk_constant */


static void walk_based_type_array(a_based_type_array_ptr ptr)
/*
Process the indicated based type array.
*/
{
  int element_num;

  for (element_num = 0; element_num < (int)bta_last; element_num++) {
    walk_ptr(*ptr, a_type_ptr, iek_type);
    ptr++;
  }  /* for */
}  /* walk_based_type_array */


static void walk_type(a_type_ptr ptr)
/*
Process the indicated type entry.
*/
{
  walk_source_corresp(ptr->source_corresp);
  remap_next_ptr(ptr->next, a_type_ptr, iek_type);
  walk_ptr(ptr->based_type_array, a_based_type_array_ptr,
           iek_based_type_array);
  switch (ptr->kind) {
    case tk_error:
    case tk_unknown:
    case tk_void:
    case tk_float:
#ifdef FFE
    case tk_fcharacter:
    case tk_hollerith:
    case tk_complex:
    case tk_stmt_label:
    case tk_format:
    case tk_association:
    case tk_unspec_routine:
    case tk_blockdata:
#endif /* ifdef FFE */
      /* No pointers. */
      break;
    case tk_integer:
#ifdef CFE
      walk_list(ptr->variant.integer.enum_constant_list, a_constant_ptr,
                iek_constant);
#endif /* ifdef CFE */
      break;
    case tk_pointer:
      walk_ptr(ptr->variant.pointer_type_pointed_to, a_type_ptr, iek_type);
      break;
#ifdef CFE
    case tk_array:
      walk_ptr(ptr->variant.array.element_type, a_type_ptr, iek_type);
      break;
    case tk_struct:
    case tk_union:
      walk_list(ptr->variant.class_struct_union.field_list, a_field_ptr,
                iek_field);
      break;
    case tk_typeref:
      walk_ptr(ptr->variant.typeref.type, a_type_ptr, iek_type);
      break;
#endif /* ifdef CFE */
    case tk_routine:
      walk_ptr(ptr->variant.routine.return_type, a_type_ptr, iek_type);
      walk_ptr(ptr->variant.routine.extra_info, a_routine_type_supplement_ptr,
               iek_routine_type_supplement);
      break;
#ifdef FFE
    case tk_farray:
      walk_ptr(ptr->variant.farray.element_type, a_type_ptr, iek_type);
      remap_ptr(ptr->variant.farray.bound_info, a_bound_info_entry_ptr,
               iek_bound_info_entry);
      if (walk_subtree) {
        /* Walk each of the bound info entries; make the index in the array
           available to facilitate writing these entries in the alternate
           file format (the problem is that there is no room for the
           entry number preceding each entry). */
        int save_array_bound_walk_index = array_bound_walk_index;
        int save_num_walk_array_bounds = num_walk_array_bounds;
        a_bound_info_entry_ptr biptr;
        num_walk_array_bounds = 2*ptr->variant.farray.number_of_dimensions;
        for (array_bound_walk_index = 0,
                                        biptr = ptr->variant.farray.bound_info;
             array_bound_walk_index < num_walk_array_bounds;
             array_bound_walk_index++, biptr++) {
          walk_entry_and_subtree((char *)biptr, iek_bound_info_entry);
        }  /* for */
        array_bound_walk_index = save_array_bound_walk_index;
        num_walk_array_bounds = save_num_walk_array_bounds;
      }  /* if */
      break;
#endif /* ifdef FFE */
#if CHECKING
    default:
      internal_error("walk_type: bad type kind");
#endif /* CHECKING */
  }  /* switch */
}  /* walk_type */


static void walk_scope(a_scope_ptr ptr)
/*
Process the indicated scope.
*/
{
  remap_next_ptr(ptr->next, a_scope_ptr, iek_scope);
  /* "assoc_routine" and "assoc_block" are done after the declarations. */
  walk_list(ptr->parameters, a_variable_ptr, iek_variable);
#ifdef FFE
  walk_ptr (ptr->function_result_var, a_variable_ptr, iek_variable);
#endif /* ifdef FFE */
  walk_list(ptr->constants, a_constant_ptr, iek_constant);
  walk_list(ptr->types, a_type_ptr, iek_type);
  walk_list(ptr->variables, a_variable_ptr, iek_variable);
  walk_list(ptr->labels, a_label_ptr, iek_label);
  walk_list(ptr->routines, a_routine_ptr, iek_routine);
#ifdef CFE
  walk_list(ptr->scopes, a_scope_ptr, iek_scope);
#endif /* ifdef CFE */
#ifdef FFE
  walk_list(ptr->entries, an_entry_description_ptr, iek_entry_description);
  walk_list(ptr->namelist_groups, a_namelist_group_ptr, iek_namelist_group);
#endif /* ifdef FFE */
  remap_ptr(ptr->assoc_routine, a_routine_ptr, iek_routine);
  walk_ptr(ptr->assoc_block, a_statement_ptr, iek_statement);
}  /* walk_scope */


static void walk_entry_and_subtree(char             *entry_ptr,
                                   an_il_entry_kind entry_kind)
/*
Process the entry at entry_ptr (which is of kind indicated by entry_kind),
and walk the subtree under that entry.  If entry_ptr is NULL, do nothing.
The subtree is processed only if walk_subtree (a global static) is TRUE.
This routine should not be called for string entries (see walk_string_entry).
This routine should be called by way of the macro walk_ptr.  If we are
currently walking through a function scope (rather than the file scope), 
and the entry pointer is to an entry in the file scope, just return
(that entry was or will be visited in the file scope walk).
*/
{
  /* Ignore NULL pointers. */
  if (entry_ptr != NULL) {
    /* Only check for having visited this entry already if walking subtrees. */
    if (walk_subtree) {
      /* If we are walking through a function scope, and the entry here is
         in the file scope, just return. */
      if (!walking_file_scope && in_file_scope(entry_ptr)) goto end_of_routine;
      /* See if this entry has been reached already, and if so, don't process
         it or its subtree.  This is indicated by the il_walk_flag field of the
         source_correspondence entry, for those entries that have one.  Note
         that only the declarative entries have potential recursion, so it's
         only there that this trick is necessary.  For other entries, only
         pointers "down" are visited, and that ensures that each entry is
         only visited once.  One exception -- param_type entries are shared,
         and they have an explicit il_walk_flag field. */
      switch (entry_kind) {
        case iek_constant:
        case iek_type:
        case iek_variable:
#ifdef CFE
        case iek_field:
#endif /* ifdef CFE */
        case iek_routine:
        case iek_label:
#ifdef FFE
        case iek_namelist_group:
#endif /* ifdef FFE */
          /* Entry has a source correspondence field.  Check the il_walk_flag
             to see if the entry has already been visited.  If not, set the
             flag and process the entry.  If this is the first entry visited,
             the current value of the flag is complemented to give the value
             to be used on this walk. */
          if (flag_value_meaning_visited == NOT_SET_YET) {
            flag_value_meaning_visited = !((a_constant_ptr)entry_ptr)->
                                                   source_corresp.il_walk_flag;
          } else if (((a_constant_ptr)entry_ptr)->source_corresp.il_walk_flag==
                     flag_value_meaning_visited) {
            /* Entry has already been visited. */
            goto end_of_routine;
          }  /* if */
          /* Set the flag to indicate that this entry has been visited. */
          ((a_constant_ptr)entry_ptr)->source_corresp.il_walk_flag =
                                                    flag_value_meaning_visited;
          break;
        case iek_param_type:
          /* param_type entry has an explicit il_walk_flag because several
             routine types can share the same param_type list.  Code is like
             the case above, except that we will have reached a type entry
             before getting here, so we need not check for the NOT_SET_YET
             case. */
          if (((a_param_type_ptr)entry_ptr)->il_walk_flag ==
              flag_value_meaning_visited) {
            /* Entry has already been visited. */
            goto end_of_routine;
          }  /* if */
          /* Set the flag to indicate that this entry has been visited. */
          ((a_param_type_ptr)entry_ptr)->il_walk_flag =
                                                    flag_value_meaning_visited;
          break;
        case iek_source_file:
        case iek_routine_type_supplement:
        case iek_based_type_array:
        case iek_expr_node:
#ifdef CFE
        case iek_switch_clause:
#endif /* ifdef CFE */
        case iek_block:
        case iek_statement:
        case iek_scope:
#ifdef FFE
        case iek_internal_complex_value:
        case iek_bound_info_entry:
        case iek_do_loop:
        case iek_label_list_entry:
        case iek_io_specifier:
        case iek_io_list_item:
        case iek_namelist_group_member:
        case iek_input_output_description:
        case iek_entry_param:
        case iek_entry_description:
#endif /* ifdef FFE */
          /* These entries do not have an il_walk_flag. */
          break;
#if CHECKING
        case iek_id_name:
        case iek_string_text:
        case iek_other_text:
          /* String entries should go to walk_string_entry. */
        case iek_none:
        case iek_last:
        default:
          internal_error("walk_entry_and_subtree: bad entry kind (1)");
#endif /* CHECKING */
      }  /* switch */
    }  /* if */
#if DEBUG
    if (debug_level >= 5) {
      char *s;
      switch (entry_kind) {
        case iek_source_file:   s = "source file";             break;
        case iek_constant:      s = "constant";                break;
        case iek_param_type:    s = "param type";              break;
        case iek_routine_type_supplement:
                                s = "routine type supplement"; break;
        case iek_based_type_array:
                                s = "based type array";        break;
        case iek_type:          s = "type";                    break;
        case iek_variable:      s = "variable";                break;
        case iek_routine:       s = "routine";                 break;
        case iek_label:         s = "label";                   break;
        case iek_expr_node:     s = "expr node";               break;
#ifdef CFE
        case iek_field:         s = "field";                   break;
        case iek_switch_clause: s = "switch clause";           break;
#endif /* ifdef CFE */
        case iek_block:         s = "block";                   break;
        case iek_statement:     s = "statement";               break;
        case iek_scope:         s = "scope";                   break;
#ifdef FFE
        case iek_internal_complex_value:
				s = "internal complex value";  break;
        case iek_bound_info_entry:
				s = "bound info entry";        break;
        case iek_do_loop:	s = "do loop";                 break;
        case iek_label_list_entry:
				s = "label list entry";        break;
        case iek_io_specifier:	s = "io specifier";            break;
        case iek_io_list_item:	s = "io list item";            break;
        case iek_namelist_group_member:
				s = "namelist group member";   break;
        case iek_namelist_group:s = "namelist group";          break;
        case iek_input_output_description:
				s = "input output description";break;
        case iek_entry_param:	s = "entry param";             break;
        case iek_entry_description:
				s = "entry description";       break;
#endif /* ifdef FFE */
        default:                s = "<bad kind>";              break;
      }  /* switch */
      fprintf(f_debug, "Walking IL tree, entry kind = %s\n", s);
    }  /* if */
#endif /* DEBUG */
    /* For each pointer in the entry, remap it and walk the subtree.
       In general, linked lists are traversed while processing the entry
       that contains the head-of-list pointer.  This is done to avoid
       using recursion to process very long lists (the stack space 
       requirements could be ridiculous).  It also means the "next"
       pointers should not be processed or remapped during the processing
       of the entries that contain them, because they've already been
       handled.  Of course if the subtrees are not being walked
       the "next" fields must be processed as they are encountered. */
    switch (entry_kind) {
      case iek_source_file:
        {
          a_source_file_ptr ptr = (a_source_file_ptr)entry_ptr;
          walk_string_ptr(ptr->file_name, iek_other_text, 0);
          walk_string_ptr(ptr->full_name, iek_other_text, 0);
          walk_list(ptr->first_child_file, a_source_file_ptr, iek_source_file);
          remap_ptr(ptr->last_child_file, a_source_file_ptr, iek_source_file);
          remap_next_ptr(ptr->next, a_source_file_ptr, iek_source_file);
        }
        break;
      case iek_constant:
        walk_constant((a_constant_ptr)entry_ptr);
        break;
      case iek_param_type:
        {
          a_param_type_ptr ptr = (a_param_type_ptr)entry_ptr;
          remap_next_ptr(ptr->next, a_param_type_ptr,
                                       iek_param_type);
          walk_ptr(ptr->type, a_type_ptr, iek_type);
        }
        break;
      case iek_routine_type_supplement:
        {
          a_routine_type_supplement_ptr ptr =
                                      (a_routine_type_supplement_ptr)entry_ptr;
          walk_list(ptr->param_type_list, a_param_type_ptr, iek_param_type);
#ifdef CFE
          walk_ptr(ptr->prototype_scope, a_scope_ptr, iek_scope);
#endif /* ifdef CFE */
          remap_ptr(ptr->assoc_routine, a_routine_ptr, iek_routine);
        }
        break;
      case iek_based_type_array:
        walk_based_type_array((a_based_type_array_ptr)entry_ptr);
        break;
      case iek_type:
        walk_type((a_type_ptr)entry_ptr);
        break;
      case iek_variable:
        {
          a_variable_ptr ptr = (a_variable_ptr)entry_ptr;
          walk_source_corresp(ptr->source_corresp);
          remap_next_ptr(ptr->next, a_variable_ptr,
                                       iek_variable);
          walk_ptr(ptr->type, a_type_ptr, iek_type);
          walk_ptr(ptr->initializer, a_constant_ptr, iek_constant);
#ifdef FFE
          remap_ptr(ptr->base_var, a_variable_ptr, iek_variable);
          remap_ptr(ptr->function_result_var_function, a_routine_ptr,
                    iek_routine);
#endif /* ifdef FFE */
        }
        break;
#ifdef CFE
      case iek_field:
        {
          a_field_ptr ptr = (a_field_ptr)entry_ptr;
          walk_source_corresp(ptr->source_corresp);
          remap_next_ptr(ptr->next, a_field_ptr, iek_field);
          walk_ptr(ptr->type, a_type_ptr, iek_type);
        }
        break;
#endif /* ifdef CFE */
      case iek_routine:
        {
          a_routine_ptr ptr = (a_routine_ptr)entry_ptr;
          walk_source_corresp(ptr->source_corresp);
          remap_next_ptr(ptr->next, a_routine_ptr, iek_routine);
          walk_ptr(ptr->type, a_type_ptr, iek_type);
          /* assoc_scope points to a different memory region and is not
             walked automatically.  The entry_process_func can arrange
             to call walk_routine_scope_il if it wants to. */
#ifdef FFE
          walk_ptr(ptr->local_routine_scope, a_scope_ptr, iek_scope);
#endif /* ifdef FFE */
        }
        break;
      case iek_label:
        {
          a_label_ptr ptr = (a_label_ptr)entry_ptr;
          walk_source_corresp(ptr->source_corresp);
          remap_next_ptr(ptr->next, a_label_ptr, iek_label);
#ifdef FFE
          switch (ptr->kind) {
            case lk_unknown:
            case lk_specification:
              /* No pointers. */
              break;
            case lk_executable:
	    case lk_else_or_elseif:
#endif /* ifdef FFE */
              remap_ptr(ptr->variant.exec_stmt, a_statement_ptr,
                        iek_statement);
#ifdef FFE
              break;
            case lk_format:
              walk_ptr(ptr->variant.format_constant, a_constant_ptr,
                       iek_constant);
              break;
#if CHECKING
            default:
              internal_error("walk_entry_and_subtree: bad label kind");
#endif /* CHECKING */
          }  /* switch */
#endif /* ifdef FFE */
        }
        break;
      case iek_expr_node:
        /* Expression nodes account for about half of all IL entries, so do
           not call a subroutine for them. */
        {
          an_expr_node_ptr ptr = (an_expr_node_ptr)entry_ptr;
          walk_ptr(ptr->type, a_type_ptr, iek_type);
          remap_next_ptr(ptr->next, an_expr_node_ptr, iek_expr_node);
          switch (ptr->kind) {
            case enk_error:
              /* No pointers. */
              break;
            case enk_operation:
              walk_list(ptr->variant.operation.operands, an_expr_node_ptr,
                        iek_expr_node);
              break;
            case enk_constant:
              walk_ptr(ptr->variant.constant, a_constant_ptr, iek_constant);
              break;
            case enk_variable:
            case enk_variable_address:
#ifdef FFE
            case enk_char_variable_length:
#endif /* ifdef FFE */
              /* Variables are handled from the scope that contains them.  Do
                 not visit them here. */
              remap_ptr(ptr->variant.variable, a_variable_ptr, iek_variable);
              break;
            case enk_routine_address:
              /* Functions are handled from the scope that contains them.  Do
                 not visit them here. */
              remap_ptr(ptr->variant.routine, a_routine_ptr, iek_routine);
              break;
#ifdef CFE
            case enk_field:
              /* Fields are handled in processing the tag that contains
                 them. */
              remap_ptr(ptr->variant.field, a_field_ptr, iek_field);
              break;
#endif /* ifdef CFE */
#ifdef FFE
            case enk_stmt_label_value:
              remap_ptr(ptr->variant.stmt_label_value, a_label_ptr, iek_label);
              break;
#endif /* ifdef FFE */
#if CHECKING
            default:
              internal_error("walk_entry_and_subtree: bad expr node kind");
#endif /* CHECKING */
          }  /* switch */
        }
        break;
#ifdef CFE
      case iek_switch_clause:
        {
          a_switch_clause_ptr ptr = (a_switch_clause_ptr)entry_ptr;
          remap_next_ptr(ptr->next, a_switch_clause_ptr,
                                       iek_switch_clause);
          walk_list(ptr->constant_list, a_constant_ptr, iek_constant);
          walk_list(ptr->statements, a_statement_ptr, iek_statement);
        }
        break;
#endif /* ifdef CFE */
      case iek_block:
#ifdef CFE
        {
          a_block_ptr ptr = (a_block_ptr)entry_ptr;
          /* The associated scope, if any, will appear on the list of local
             scopes for the current scope.  Therefore, here we just remap
             the pointer but do not walk the subtree. */
          remap_ptr(ptr->assoc_scope, a_scope_ptr, iek_scope);
        }
#endif /* ifdef CFE */
        break;
      case iek_statement:
        /* Statements account for more than 10% of the IL nodes (they're the
           second most common, after expr nodes), so do not call a subroutine
           for them. */
        {
          a_statement_ptr ptr = (a_statement_ptr)entry_ptr;
          remap_next_ptr(ptr->next, a_statement_ptr, iek_statement);
          walk_ptr(ptr->expr, an_expr_node_ptr, iek_expr_node);
          switch (ptr->kind) {
            case stmk_expr:
            case stmk_return:
#ifdef FFE
            case stmk_alt_return:
#endif /* ifdef FFE */
              /* No pointers. */
              break;
            case stmk_if:
              walk_ptr(ptr->variant.if_stmt.then_statement, a_statement_ptr,
                       iek_statement);
              walk_ptr(ptr->variant.if_stmt.else_statement, a_statement_ptr,
                       iek_statement);
              break;
            case stmk_while:
#ifdef CFE
            case stmk_end_test_while:
#endif /* ifdef CFE */
              walk_ptr(ptr->variant.loop_statement, a_statement_ptr,
                       iek_statement);
              break;
            case stmk_goto:
            case stmk_label:
              remap_ptr(ptr->variant.label, a_label_ptr, iek_label);
              break;
            case stmk_block:
              /* Do extra_info before statements to get declarations out
                 before the statements that use them. */
              walk_ptr(ptr->variant.block.extra_info, a_block_ptr, iek_block);
              walk_list(ptr->variant.block.statements, a_statement_ptr,
                        iek_statement);
              break;
#ifdef CFE
            case stmk_switch:
              walk_list(ptr->variant.switch_stmt.clause_list,
                        a_switch_clause_ptr, iek_switch_clause);
              walk_ptr(ptr->variant.switch_stmt.body_statement,
                       a_statement_ptr, iek_statement);
              break;
            case stmk_init:
              walk_ptr(ptr->variant.init_variable, a_variable_ptr,
                       iek_variable);
              break;
            case stmk_asm:
              walk_ptr(ptr->variant.asm_string, a_constant_ptr, iek_constant);
              break;
#endif /* ifdef CFE */
#ifdef FFE
            case stmk_fentry:
              remap_ptr(ptr->variant.fentry.assoc_routine, a_routine_ptr,
                        iek_routine);
              walk_list(ptr->variant.fentry.prologue, a_statement_ptr,
                        iek_statement);
              break;
            case stmk_ido:
            case stmk_fdo:
              walk_ptr(ptr->variant.do_stmt.loop_statement, a_statement_ptr,
                       iek_statement);
              walk_ptr(ptr->variant.do_stmt.do_info, a_do_loop_ptr,
                       iek_do_loop);
              break;
            case stmk_iarith_if:
            case stmk_farith_if:
            case stmk_computed_goto:
            case stmk_assigned_goto:
              walk_list(ptr->variant.label_list, a_label_list_entry_ptr,
                        iek_label_list_entry);
              break;
            case stmk_stop:
            case stmk_pause:
              walk_ptr(ptr->variant.stop_pause_string, a_constant_ptr,
                       iek_constant);
              break;
            case stmk_set_array_shape:
              remap_ptr(ptr->variant.array_variable, a_variable_ptr,
                        iek_variable);
              break;
            case stmk_input_output:
              walk_ptr(ptr->variant.input_output,
                       an_input_output_description_ptr,
                       iek_input_output_description);
              break;
#endif /* ifdef FFE */
#if CHECKING
            default:
              internal_error("walk_entry_and_subtree: bad statement kind");
#endif /* CHECKING */
          }  /* switch */
        }
        break;
      case iek_scope:
        walk_scope((a_scope_ptr)entry_ptr);
        break;
#ifdef FFE
      case iek_internal_complex_value:
        /* No pointers. */
        break;
      case iek_bound_info_entry:
        {
          a_bound_info_entry_ptr ptr = (a_bound_info_entry_ptr)entry_ptr;
          if (ptr->kind == (a_bound_kind)bk_adjustable) {
            walk_ptr(ptr->variant.adjustable_bound, an_expr_node_ptr,
                     iek_expr_node);
          }  /* if */
        }
        break;
      case iek_do_loop:
        {
          a_do_loop_ptr ptr = (a_do_loop_ptr)entry_ptr;
          remap_ptr(ptr->variable, a_variable_ptr, iek_variable);
          walk_ptr(ptr->initial_value, an_expr_node_ptr, iek_expr_node);
          walk_ptr(ptr->final_value, an_expr_node_ptr, iek_expr_node);
          walk_ptr(ptr->increment, an_expr_node_ptr, iek_expr_node);
        }
        break;
      case iek_label_list_entry:
        {
          a_label_list_entry_ptr ptr = (a_label_list_entry_ptr)entry_ptr;
          remap_next_ptr(ptr->next, a_label_list_entry_ptr,
                         iek_label_list_entry);
          remap_ptr(ptr->label, a_label_ptr, iek_label);
        }
        break;
      case iek_io_specifier:
        {
          an_io_specifier_ptr ptr = (an_io_specifier_ptr)entry_ptr;
          remap_next_ptr(ptr->next, an_io_specifier_ptr, iek_io_specifier);
          switch (ptr->transfer) {
            case iost_label:
              remap_ptr(ptr->variant.label, a_label_ptr, iek_label);
              break;
            case iost_expr_in:
            case iost_var_out:
              walk_ptr(ptr->variant.expr, an_expr_node_ptr, iek_expr_node);
              break;
#if CHECKING
            default:
              internal_error(
                          "walk_entry_and_subtree: bad io specifier transfer");
#endif /* CHECKING */
          }  /* switch */
        }
        break;
      case iek_io_list_item:
        {
          an_io_list_item_ptr ptr = (an_io_list_item_ptr)entry_ptr;
          remap_next_ptr(ptr->next, an_io_list_item_ptr, iek_io_list_item);
          switch (ptr->kind) {
            case iol_expr:
            case iol_variable:
              walk_ptr(ptr->variant.expr, an_expr_node_ptr, iek_expr_node);
              break;
            case iol_array:
              remap_ptr(ptr->variant.array_var, a_variable_ptr, iek_variable);
              break;
            case iol_implied_do:
              remap_ptr(ptr->variant.implied_do.variable, a_variable_ptr,
                        iek_variable);
              walk_ptr(ptr->variant.implied_do.initial_value, an_expr_node_ptr,
                       iek_expr_node);
              walk_ptr(ptr->variant.implied_do.final_value, an_expr_node_ptr,
                       iek_expr_node);
              walk_ptr(ptr->variant.implied_do.increment, an_expr_node_ptr,
                       iek_expr_node);
              walk_list(ptr->variant.implied_do.list, an_io_list_item_ptr,
                        iek_io_list_item);
              break;
#if CHECKING
            default:
              internal_error("walk_entry_and_subtree: bad io list item kind");
#endif /* CHECKING */
          }  /* switch */
        }
        break;
      case iek_namelist_group_member:
        {
          a_namelist_group_member_ptr ptr =
                                        (a_namelist_group_member_ptr)entry_ptr;
          remap_next_ptr(ptr->next, a_namelist_group_member_ptr,
                         iek_namelist_group_member);
          remap_ptr(ptr->variable, a_variable_ptr, iek_variable);
        }
        break;
      case iek_namelist_group:
        {
          a_namelist_group_ptr ptr = (a_namelist_group_ptr)entry_ptr;
          walk_source_corresp(ptr->source_corresp);
          remap_next_ptr(ptr->next, a_namelist_group_ptr, iek_namelist_group);
          walk_list(ptr->member_list, a_namelist_group_member_ptr,
                    iek_namelist_group_member);
        }
        break;
      case iek_input_output_description:
        {
          an_input_output_description_ptr ptr =
                                    (an_input_output_description_ptr)entry_ptr;
          if (ptr->unit_kind == (an_io_unit_kind)iou_external ||
              ptr->unit_kind == (an_io_unit_kind)iou_internal) {
            walk_ptr(ptr->unit_expr, an_expr_node_ptr, iek_expr_node);
          }  /* if */
          if (ptr->kind == (an_io_statement_kind)ios_encode ||
              ptr->kind == (an_io_statement_kind)ios_decode) {
            walk_ptr(ptr->encode_decode_length, an_expr_node_ptr,
                     iek_expr_node);
          }  /* if */
          switch (ptr->format_kind) {
            case iof_none:
            case iof_error:
            case iof_list_directed:
            case iof_unformatted:
              /* No pointers. */
              break;
            case iof_format_label:
              remap_ptr(ptr->format.label, a_label_ptr, iek_label);
              break;
            case iof_assigned_var:
            case iof_char_expr:
              walk_ptr(ptr->format.expr, an_expr_node_ptr, iek_expr_node);
              break;
            case iof_namelist_directed:
              remap_ptr(ptr->format.namelist_group, a_namelist_group_ptr,
                        iek_namelist_group);
              break;
#if CHECKING
            default:
              internal_error(
                  "walk_entry_and_subtree: bad input output format kind");
#endif /* CHECKING */
          }  /* switch */
          walk_list(ptr->specifier_list, an_io_specifier_ptr,
                    iek_io_specifier);
          walk_list(ptr->item_list, an_io_list_item_ptr,
                    iek_io_list_item);
        }
        break;
      case iek_entry_param:
        {
          an_entry_param_ptr ptr = (an_entry_param_ptr)entry_ptr;
          remap_next_ptr(ptr->next, an_entry_param_ptr, iek_entry_param);
          remap_ptr(ptr->param_var, a_variable_ptr, iek_variable);
        }
        break;
      case iek_entry_description:
        {
          an_entry_description_ptr ptr = (an_entry_description_ptr)entry_ptr;
          remap_next_ptr(ptr->next, an_entry_description_ptr,
                         iek_entry_description);
          remap_ptr(ptr->assoc_routine, a_routine_ptr, iek_routine);
          walk_list(ptr->parameters, an_entry_param_ptr, iek_entry_param);
          walk_ptr(ptr->function_result_var, a_variable_ptr, iek_variable);
        }
        break;
#endif /* ifdef FFE */
#if CHECKING
      case iek_id_name:
      case iek_string_text:
      case iek_other_text:
        /* String entries should go to walk_string_entry */
      case iek_none:
      case iek_last:
      default:
        internal_error("walk_entry_and_subtree: bad entry kind (2)");
#endif /* CHECKING */
    }  /* switch */
    /* Call the routine to process the entry if there is such a routine. */
    if (entry_process_func != NULL) entry_process_func(entry_ptr, entry_kind);
end_of_routine:;
  }  /* if */
}  /* walk_entry_and_subtree */


static void walk_string_entry(char             *entry_ptr,
                              an_il_entry_kind entry_kind,
                              sizeof_t         entry_length)
/*
Process the entry at entry_ptr (which is of kind indicated by entry_kind,
and has length given by "entry_length" if its kind is iek_string_text).
If entry_ptr is NULL, do nothing.  This routine should be called only for
string entries.  This routine should be called by way of the macro
walk_string_ptr.  Note that the entry is processed even if it is in the
file scope and a function scope is being traversed.  This is because
there's no way to link string entries into a scope other than by pointing
to them in the normal way, so string entries are considered honorary
members of the scope from which they are referenced for purposes of tree
walking.
*/
{
  /* Ignore NULL pointers. */
  if (entry_ptr != NULL) {
#if DEBUG
    if (debug_level >= 5) {
      char *s;
      switch (entry_kind) {
        case iek_id_name:       s = "id name";                 break;
        case iek_string_text:   s = "string text";             break;
        case iek_other_text:    s = "other text";              break;
        default:                s = "<bad kind>";              break;
      }  /* switch */
      fprintf(f_debug, "Walking IL tree, string entry kind = %s\n", s);
    }  /* if */
#endif /* DEBUG */
    /* Call the routine to process string entries only if there is one. */
    if (string_entry_process_func != NULL) {
      /* For entries other than iek_string_text, the length must be computed.
         Note that it includes the final null byte (that's the "+ 1"). */
      if (entry_kind != iek_string_text) entry_length = strlen(entry_ptr) + 1;
      string_entry_process_func(entry_ptr, entry_kind, entry_length);
    }  /* if */
  }  /* if */
}  /* walk_string_entry */


void walk_file_scope_il(
             an_entry_process_function_ptr       entry_process_function,
             a_string_entry_process_function_ptr string_entry_process_function,
             a_remap_function_ptr                remap_function)
/*
Walk the intermediate language tree for the file scope.  Begin with il_header
and visit the whole file-scope tree, but do not go down into the information
about each function.  Process each non-string entry by calling
entry_process_function on that entry, and each string entry by calling
string_entry_process_function on that entry.  Remap each pointer to a new
value by calling remap_function.  entry_process_function,
string_entry_process_function, or remap_function can be NULL to indicate
that the corresponding function is unnecessary.

The remapping function is used when reading in an IL file.  The IL tree
was in memory in some way, and was written out exactly the way it
looked.  Now it has been read back in, and each memory block is probably
at a different location than when written out.  All of the pointers
need to be updated from their "old" values to the proper "new" values.
That is what the remap function does.
*/
{
  db_enter(4, "walk_file_scope_il");
  /* Save the function pointers so they don't have to be passed around. */
  entry_process_func = entry_process_function;
  string_entry_process_func = string_entry_process_function;
  remap_func = remap_function;
  walk_subtree = TRUE;
  walking_file_scope = TRUE;
  flag_value_meaning_visited = NOT_SET_YET;
#ifdef FFE
  array_bound_walk_index = 0;
#endif /* ifdef FFE */

  /* Process the IL header.  Note that all of these pointers are to the
     file scope memory region. */
  walk_ptr(il_header.primary_source_file, a_source_file_ptr, iek_source_file);
  walk_ptr(il_header.primary_scope, a_scope_ptr, iek_scope);
  remap_ptr(il_header.main_routine, a_routine_ptr, iek_routine);
  walk_string_ptr(il_header.compiler_version, iek_other_text, 0);
  walk_string_ptr(il_header.time_of_compilation, iek_other_text, 0);
  /* region_scope_entry should not be walked. */
  db_exit();
}  /* walk_file_scope_il */


void walk_routine_scope_il(
             a_memory_region_number              region_number,
             an_entry_process_function_ptr       entry_process_function,
             a_string_entry_process_function_ptr string_entry_process_function,
             a_remap_function_ptr                remap_function)
/*
Walk the intermediate language tree for a routine scope.  Begin with the
scope entry for region region_number, and visit the whole scope tree.
Process each non-string entry by calling entry_process_function on that
entry, and each string entry by calling string_entry_process_function on
that entry.  Remap each pointer to a new value by calling remap_function.
entry_process_function, string_entry_process_function, or remap_function
can be NULL to indicate that the corresponding function is unnecessary.
*/
{
  an_entry_process_function_ptr       prev_entry_process_func =
                                           entry_process_func;
  a_string_entry_process_function_ptr prev_string_entry_process_func =
                                           string_entry_process_func;
  a_remap_function_ptr                prev_remap_func =
                                           remap_func;
  a_boolean                           prev_walking_file_scope =
                                           walking_file_scope;
  int                                 prev_flag_value_meaning_visited =
                                           flag_value_meaning_visited;

  db_enter(4, "walk_routine_scope_il");
  /* Save the function pointers so they don't have to be passed around. */
  entry_process_func = entry_process_function;
  string_entry_process_func = string_entry_process_function;
  remap_func = remap_function;
  walk_subtree = TRUE;
  /* Walking a routine scope, not the file scope. */
  walking_file_scope = FALSE;
  flag_value_meaning_visited = NOT_SET_YET;
#ifdef FFE
  array_bound_walk_index = 0;
#endif /* ifdef FFE */

  /* Process the scope and its subtree. */
  walk_entry_and_subtree((char *)il_header.region_scope_entry[region_number],
                         iek_scope);

  /* Restore the previous values of the function pointers etc. */
  entry_process_func = prev_entry_process_func;
  string_entry_process_func = prev_string_entry_process_func;
  remap_func = prev_remap_func;
  walking_file_scope = prev_walking_file_scope;
  flag_value_meaning_visited = prev_flag_value_meaning_visited;

  db_exit();
}  /* walk_routine_scope_il */


void remap_pointers_in_il_entry(char                 *entry_ptr,
                                an_il_entry_kind     entry_kind,
                                a_remap_function_ptr remap_function)
/*
Remap each pointer in the IL entry at entry_ptr (which has kind entry_kind).
remap_function is the pointer transformation function to be used.  The
subtree of the entry is not processed.
*/
{
  an_entry_process_function_ptr       prev_entry_process_func =
                                           entry_process_func;
  a_remap_function_ptr                prev_remap_func =
                                           remap_func;
  a_boolean                           prev_walk_subtree =
                                           walk_subtree;

  entry_process_func = NULL;
  remap_func = remap_function;
  walk_subtree = FALSE;
  /* string_entry_process_func, walking_file_scope, and
     flag_value_meaning_visited do not need to be set. */

  if (is_string_entry_kind(entry_kind)) {
    /* String entries have no pointers and require no processing. */
  } else {
    walk_entry_and_subtree(entry_ptr, entry_kind);
  }  /* if */

  /* Restore the previous values of the function pointers etc. */
  entry_process_func = prev_entry_process_func;
  remap_func = prev_remap_func;
  walk_subtree = prev_walk_subtree;
}  /* remap_pointers_in_il_entry */


void remap_il_header_pointers(a_remap_function_ptr remap_function)
/*
Remap the pointers in il_header by running them through remap_function.
*/
{
  a_remap_function_ptr prev_remap_func = remap_func;

  remap_func = remap_function;

  remap_ptr(il_header.primary_source_file, a_source_file_ptr, iek_source_file);
  remap_ptr(il_header.primary_scope, a_scope_ptr, iek_scope);
  remap_ptr(il_header.main_routine, a_routine_ptr, iek_routine);
  remap_ptr(il_header.compiler_version, a_char_ptr, iek_other_text);
  remap_ptr(il_header.time_of_compilation, a_char_ptr, iek_other_text);
  /* region_scope_entry should not be changed; it's not a pointer into
     IL memory in the usual way.  It's changed explicitly as needed. */

  remap_func = prev_remap_func;
}  /* remap_il_header_pointers. */

#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
                     

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
