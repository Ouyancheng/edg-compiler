/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1993 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

walk_entry.h -- Routines used by il_walk.c to walk IL entries.

Placed in a separate file so they can be included twice:

1)  With DO_SUBTREE_WALK TRUE, the routines walk not only the entry itself
    but also its subtree.

2)  With DO_SUBTREE_WALK FALSE, the routines walk just the entry itself.
    This is used for remapping of pointers.

*/

/*
Macro to remap a pointer from an "old" value to a "new" value.  ptr is
the pointer, ptr_type the type of ptr, and entry_kind is the kind of entry
pointed to.
*/
#undef remap_ptr
#define remap_ptr(ptr, ptr_type, entry_kind) \
{ if (walk_remap_func != NULL) { \
    (ptr) = (ptr_type)walk_remap_func((char *)(ptr), (entry_kind)); \
  }  /* if */ \
}  /* remap_ptr */

/*
Like remap_ptr, but used for "next" pointers in entries.  These are
remapped only if not processing subtrees (if subtrees are being processed,
walk_list handles the remapping when doing the parent of this entry).
*/
#undef remap_next_ptr
#if DO_SUBTREE_WALK
#define remap_next_ptr(ptr, ptr_type, entry_kind) /* Nothing */
#else /* !DO_SUBTREE_WALK */
#define remap_next_ptr(ptr, ptr_type, entry_kind) \
  remap_ptr((ptr), ptr_type, (entry_kind))
#endif /* DO_SUBTREE_WALK */

/*
Macro to remap a pointer to its new value, walk the subtree of the pointer
(if appropriate), and process the entry pointed to.  ptr is the pointer,
ptr_type is the type of ptr, and entry_kind is the kind of entry pointed to.
*/
#undef walk_ptr
#if DO_SUBTREE_WALK
#define walk_ptr(ptr, ptr_type, entry_kind) \
{ remap_ptr((ptr), ptr_type, (entry_kind)); \
  if ((ptr) != NULL) walk_entry_and_subtree((char *)(ptr), (entry_kind)); \
}  /* walk_ptr */
#else /* !DO_SUBTREE_WALK */
#define walk_ptr(ptr, ptr_type, entry_kind) \
  remap_ptr((ptr), ptr_type, (entry_kind))
#endif /* DO_SUBTREE_WALK */

/*
Macro similar to walk_ptr, but used for string entries.  ptr is the pointer
to the entry, entry_kind is the kind of entry pointed to, and entry_length is
the string length for iek_string_text entries, unused otherwise.
*/
#undef walk_string_ptr
#if DO_SUBTREE_WALK
#define walk_string_ptr(ptr, entry_kind, entry_length) \
{ remap_ptr((ptr), a_char_ptr, (entry_kind)); \
  walk_string_entry((char *)(ptr), (entry_kind), (sizeof_t)(entry_length)); \
}  /* walk_string_ptr */
#else /* !DO_SUBTREE_WALK */
#define walk_string_ptr(ptr, entry_kind, entry_length) \
  remap_ptr((ptr), a_char_ptr, (entry_kind))
#endif /* DO_SUBTREE_WALK */

/*
Process a list, each entry linked to the next by the "next" field.
ptr is the pointer to the list, ptr_type is the type of ptr, and entry_kind
is the kind of entries on the list.  If walking subtrees, each entry is
processed; if not, ptr is remapped but the list is not traversed.
*/
#undef walk_list
#if DO_SUBTREE_WALK
#define walk_list(ptr, ptr_type, entry_kind) \
{ ptr_type *ptr_ptr = &(ptr); \
  for (; *ptr_ptr != NULL; ptr_ptr = &(*ptr_ptr)->next) { \
    walk_ptr(*ptr_ptr, ptr_type, (entry_kind)); \
  }  /* for */ \
}  /* walk_list */
#else /* !DO_SUBTREE_WALK */
#define walk_list(ptr, ptr_type, entry_kind) \
  remap_ptr((ptr), ptr_type, (entry_kind))
#endif /* DO_SUBTREE_WALK */

/*
Process the source correspondence field pointed to by ptr.
*/
/* Macro to remap class_of_which_a_member only if it exists. */
#undef remap_class_of_which_a_member
#ifdef CFE
#define remap_class_of_which_a_member(ptr) \
  remap_ptr((ptr).class_of_which_a_member, a_type_ptr, iek_type)
#else /* !defined(CFE) */
#define remap_class_of_which_a_member(ptr) /* Nothing */
#endif /* ifdef CFE */

#if GENERATE_SOURCE_SEQUENCE_LISTS
#define remap_source_sequence_entry(ptr) \
  remap_ptr((ptr).source_sequence_entry, a_source_sequence_entry_ptr, \
            iek_source_sequence_entry);
#else /* !GENERATE_SOURCE_SEQUENCE_LISTS */
#define remap_source_sequence_entry(ptr) /* Nothing */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

#undef walk_source_corresp
#define walk_source_corresp(ptr) \
{ (ptr).assoc_info = NULL; \
  walk_string_ptr((ptr).name, iek_id_name, 0); \
  remap_class_of_which_a_member(ptr); \
  remap_source_sequence_entry(ptr); \
}  /* walk_source_corresp */


/* The name is provided by a macro so it can be two different things, i.e.,
    walk_entry_and_subtree and remap_pointers_in_il_entry.  Note that both
    are made external even though only remap_pointers_in_il_entry really needs
    to be. */
void WALK_ENTRY_ROUTINE_NAME(char             *entry_ptr,
                             an_il_entry_kind entry_kind)
/*
Process the entry at entry_ptr (which is of kind indicated by entry_kind)
by remapping its pointers, and, if DO_SUBTREE_WALK is TRUE, walking its
subtree and calling the entry_process_func.  When DO_SUBTREE_WALK is
TRUE, if the entry has already been seen, or if the pointer crosses into
the file scope, do not process it (but record an orphan in the latter case).
*/
{
#if DO_SUBTREE_WALK
  /* Only check for having visited this entry already if walking subtrees. */
  {
    an_il_entry_prefix_ptr epp = &il_entry_prefix_of(entry_ptr);
    /* If we are walking through a function scope, and the entry here is
       in the file scope, just return. */
    if (!walking_file_scope && epp->file_scope) {
      /* Add non-string file scope IL entries referenced from a
         function scope to the orphaned IL entries lists. */
      add_orphaned_file_scope_il_entry(entry_ptr, entry_kind);
      goto end_of_routine;
    }  /* if */
    /* See if this entry has been reached already, and if so, don't process
       it or its subtree.  This is indicated by the il_walk_flag field of the
       entry prefix. */
    if (epp->il_walk_flag == flag_value_meaning_visited) {
      /* Entry has already been visited. */
      goto end_of_routine;
    }  /* if */
    /* Set the flag to indicate that this entry has been visited. */
    epp->il_walk_flag = flag_value_meaning_visited;
  }
#endif /* DO_SUBTREE_WALK */
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "Walking IL tree, entry kind = %s\n",
                     il_entry_kind_names[(int)entry_kind]);
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
        walk_string_ptr(ptr->name_as_written, iek_other_text, 0);
        walk_list(ptr->first_child_file, a_source_file_ptr, iek_source_file);
        remap_ptr(ptr->last_child_file, a_source_file_ptr, iek_source_file);
        remap_next_ptr(ptr->next, a_source_file_ptr, iek_source_file);
      }
      break;
    case iek_constant:
      {
        a_constant_ptr ptr = (a_constant_ptr)entry_ptr;
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
                remap_ptr(ptr->variant.address.variant.variable,
                          a_variable_ptr, iek_variable);
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
          case ck_ptr_to_member:
            remap_ptr(ptr->variant.ptr_to_member.casting_base_class,
                      a_base_class_ptr, iek_base_class);
            if (ptr->variant.ptr_to_member.is_function_ptr) {
              remap_ptr(ptr->variant.ptr_to_member.variant.routine,
                        a_routine_ptr, iek_routine);
            } else {
              remap_ptr(ptr->variant.ptr_to_member.variant.field, a_field_ptr,
                          iek_field);
            }  /* if */
            break;
          case ck_dynamic_init:
              walk_ptr(ptr->variant.dynamic_init, a_dynamic_init_ptr,
                       iek_dynamic_init);
            break;
#endif /* ifdef CFE */
          case ck_aggregate:
            walk_list(ptr->variant.aggregate.first_constant, a_constant_ptr,
                      iek_constant);
            remap_ptr(ptr->variant.aggregate.last_constant, a_constant_ptr,
                      iek_constant);
            break;
          case ck_init_repeat:
            walk_ptr(ptr->variant.init_repeat.constant, a_constant_ptr,
                     iek_constant);
            break;
#ifdef FFE
          case ck_init_position:
            break;
#endif /* ifdef FFE */
#if CHECKING
          default:
            internal_error("walk_entry_and_subtree: bad constant kind");
#endif /* CHECKING */
        }  /* switch */
      }
      break;
    case iek_param_type:
      {
        a_param_type_ptr ptr = (a_param_type_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_param_type_ptr, iek_param_type);
        walk_ptr(ptr->type, a_type_ptr, iek_type);
        walk_ptr(ptr->default_arg_expr, an_expr_node_ptr, iek_expr_node);
      }
      break;
    case iek_routine_type_supplement:
      {
        a_routine_type_supplement_ptr ptr =
                                      (a_routine_type_supplement_ptr)entry_ptr;
        /* The param type list is special in that it is a list that can be
           pointed to by more than one list header (i.e., from multiple
           routine type supplements).  Remap the pointer, then check whether
           the first entry has already been visited.  If so, do nothing more;
           otherwise, walk the list. */
        remap_ptr(ptr->param_type_list, a_param_type_ptr, iek_param_type);
#if DO_SUBTREE_WALK
        { a_param_type_ptr first_param;
          first_param = ptr->param_type_list;
          if (first_param == NULL ||
              il_entry_prefix_of(first_param).il_walk_flag ==
                                                  flag_value_meaning_visited) {
            /* The list has already been visited, or it's an empty list. */
          } else {
            /* Walk the first entry and then the rest of the list.  This is
               sort of an exploded version of walk_list without the initial
               remap_ptr. */
            walk_entry_and_subtree((char *)first_param, iek_param_type);
            walk_list(first_param->next, a_param_type_ptr, iek_param_type);
          }  /* if */
        }
#endif /* DO_SUBTREE_WALK */
#ifdef CFE
        walk_ptr(ptr->implicit_this_param_type, a_type_ptr, iek_type);
        walk_ptr(ptr->prototype_scope, a_scope_ptr, iek_scope);
        walk_ptr(ptr->throw_specification, a_throw_specification_ptr,
                 iek_throw_specification);
#endif /* ifdef CFE */
        remap_ptr(ptr->assoc_routine, a_routine_ptr, iek_routine);
      }
      break;
    case iek_based_type_list_member:
      {
        a_based_type_list_member_ptr ptr =
                                       (a_based_type_list_member_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_based_type_list_member_ptr,
                       iek_based_type_list_member);
        walk_ptr(ptr->based_type, a_type_ptr, iek_type);
      }
      break;
    case iek_type:
      {
        a_type_ptr ptr = (a_type_ptr)entry_ptr;
        walk_source_corresp(ptr->source_corresp);
        remap_next_ptr(ptr->next, a_type_ptr, iek_type);
        walk_list(ptr->based_types, a_based_type_list_member_ptr,
                  iek_based_type_list_member);
#if DO_IL_LOWERING
        /* ptr->typeinfo_var not processed. */
#endif /* DO_IL_LOWERING */
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
            if (ptr->variant.integer.enum_type) {
              walk_list(ptr->variant.integer.enum_info.constant_list,
                        a_constant_ptr, iek_constant);
            } else {
              walk_ptr(ptr->variant.integer.enum_info.affiliated_type,
                       a_type_ptr, iek_type);
            }  /* if */
#endif /* ifdef CFE */
            break;
          case tk_pointer:
            walk_ptr(ptr->variant.pointer.type, a_type_ptr, iek_type);
            break;
#ifdef CFE
          case tk_array:
            if (ptr->variant.array.is_variable_size_array) {
              walk_ptr(ptr->variant.array.variant.element_count_expr,
                       an_expr_node_ptr, iek_expr_node);
            }  /* if */
            walk_ptr(ptr->variant.array.element_type, a_type_ptr, iek_type);
            break;
          case tk_class:
          case tk_struct:
          case tk_union:
            walk_list(ptr->variant.class_struct_union.field_list, a_field_ptr,
                      iek_field);
            walk_ptr(ptr->variant.class_struct_union.extra_info,
                     a_class_type_supplement_ptr, iek_class_type_supplement);
            break;
          case tk_typeref:
            walk_ptr(ptr->variant.typeref.type, a_type_ptr, iek_type);
#if DO_IL_LOWERING
            /* ptr->variant.typeref.orig_type not processed. */
#endif /* DO_IL_LOWERING */
            break;
          case tk_ptr_to_member:
            remap_ptr(ptr->variant.ptr_to_member.class_of_which_a_member,
                      a_type_ptr, iek_type);
            walk_ptr(ptr->variant.ptr_to_member.type, a_type_ptr, iek_type);
            break;
#endif /* ifdef CFE */
          case tk_routine:
            walk_ptr(ptr->variant.routine.return_type, a_type_ptr, iek_type);
            walk_ptr(ptr->variant.routine.extra_info,
                     a_routine_type_supplement_ptr,
                     iek_routine_type_supplement);
            break;
#ifdef FFE
          case tk_farray:
            walk_ptr(ptr->variant.farray.element_type, a_type_ptr, iek_type);
            remap_ptr(ptr->variant.farray.bound_info, a_bound_info_entry_ptr,
                     iek_bound_info_entry);
#if DO_SUBTREE_WALK
            /* Walk each of the bound info entries; make the index in the array
               available to facilitate writing these entries in the alternate
               file format (the problem is that there is no room for the
               entry number preceding each entry). */
            {
              int save_array_bound_walk_index = array_bound_walk_index;
              int save_num_walk_array_bounds = num_walk_array_bounds;
              a_bound_info_entry_ptr biptr;
              num_walk_array_bounds =
                                  2 * ptr->variant.farray.number_of_dimensions;
              for (array_bound_walk_index = 0,
                                        biptr = ptr->variant.farray.bound_info;
                   array_bound_walk_index < num_walk_array_bounds;
                   array_bound_walk_index++, biptr++) {
                walk_entry_and_subtree((char *)biptr, iek_bound_info_entry);
              }  /* for */
              array_bound_walk_index = save_array_bound_walk_index;
              num_walk_array_bounds = save_num_walk_array_bounds;
            }
#endif /* DO_SUBTREE_WALK */
            break;
#endif /* ifdef FFE */
#if CHECKING
          case tk_template_param:
            /* Front end only. */
          default:
            internal_error("walk_entry_and_subtree: bad type kind");
#endif /* CHECKING */
        }  /* switch */
      }
      break;
    case iek_variable:
      {
        a_variable_ptr ptr = (a_variable_ptr)entry_ptr;
        walk_source_corresp(ptr->source_corresp);
        remap_next_ptr(ptr->next, a_variable_ptr, iek_variable);
        walk_ptr(ptr->type, a_type_ptr, iek_type);
        remap_ptr(ptr->assoc_param_type, a_param_type_ptr, iek_param_type);
        switch (ptr->init_kind) {
          case initk_none:
          case initk_zero:
            /* No pointers. */
            break;
          case initk_static:
            walk_ptr(ptr->initializer.constant, a_constant_ptr, iek_constant);
            break;
          case initk_dynamic:
            walk_ptr(ptr->initializer.dynamic, a_dynamic_init_ptr,
                     iek_dynamic_init);
            break;
#if CHECKING
          default:
            internal_error("walk_entry_and_subtree: bad variable init kind");
#endif  /* CHECKING */
        }  /* switch */
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
    case iek_throw_specification:
      {
        a_throw_specification_ptr ptr = (a_throw_specification_ptr)entry_ptr;
        walk_list(ptr->throw_spec_type_list, a_throw_spec_type_ptr,
                  iek_throw_spec_type);
      }
      break;
    case iek_throw_spec_type:
      {
        a_throw_spec_type_ptr ptr = (a_throw_spec_type_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_throw_spec_type_ptr, iek_throw_spec_type);
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
#ifdef CFE
        walk_list(ptr->befriending_classes, a_class_list_entry_ptr,
                  iek_class_list_entry);
#endif /* ifdef CFE */
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
            remap_ptr(ptr->variant.exec_stmt, a_statement_ptr, iek_statement);
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
#ifdef CFE
        remap_ptr(ptr->parent_block, a_statement_ptr, iek_statement);
#endif /* ifdef CFE */
      }
      break;
    case iek_expr_node:
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
          case enk_temp_init:
            walk_ptr(ptr->variant.init.dynamic_init,
                     a_dynamic_init_ptr, iek_dynamic_init);
            break;
          case enk_new_delete:
            walk_ptr(ptr->variant.new_delete, a_new_delete_supplement_ptr,
                     iek_new_delete_supplement);
            break;
          case enk_throw:
            walk_ptr(ptr->variant.throw_info, a_throw_supplement_ptr,
                     iek_throw_supplement);
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
    case iek_for_loop:
      {
        a_for_loop_ptr ptr = (a_for_loop_ptr)entry_ptr;
        walk_ptr(ptr->initialization, a_statement_ptr, iek_statement);
        walk_ptr(ptr->increment, an_expr_node_ptr, iek_expr_node);
      }
      break;
    case iek_switch_clause:
      {
        a_switch_clause_ptr ptr = (a_switch_clause_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_switch_clause_ptr, iek_switch_clause);
        walk_list(ptr->constant_list, a_constant_ptr, iek_constant);
        walk_list(ptr->statements, a_statement_ptr, iek_statement);
      }
      break;
    case iek_handler:
      {
        a_handler_ptr ptr = (a_handler_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_handler_ptr, iek_handler);
        /* The associated parameter, if any, will appear on the variables
           list of the current scope.  Therefore, here we just remap the
           pointer but do not walk the subtree. */
        remap_ptr(ptr->parameter, a_variable_ptr, iek_variable);
        walk_ptr(ptr->statement, a_statement_ptr, iek_statement);
        walk_ptr(ptr->dynamic_init, a_dynamic_init_ptr, iek_dynamic_init);
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
        remap_ptr(ptr->parent_block, a_statement_ptr, iek_statement);
#if GENERATE_SOURCE_SEQUENCE_LISTS
        remap_ptr(ptr->final_source_sequence_entry,
                  a_source_sequence_entry_ptr, iek_source_sequence_entry);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
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
#if GENERATE_SOURCE_SEQUENCE_LISTS
        remap_ptr(ptr->source_sequence_entry, a_source_sequence_entry_ptr,
                  iek_source_sequence_entry);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        walk_ptr(ptr->expr, an_expr_node_ptr, iek_expr_node);
        switch (ptr->kind) {
          case stmk_expr:
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
          case stmk_return:
            walk_ptr(ptr->variant.return_dynamic_init, a_dynamic_init_ptr,
                     iek_dynamic_init);
            break;
          case stmk_block:
            /* Do extra_info before statements to get declarations out
               before the statements that use them. */
            walk_ptr(ptr->variant.block.extra_info, a_block_ptr, iek_block);
            walk_list(ptr->variant.block.statements, a_statement_ptr,
                      iek_statement);
            break;
#ifdef CFE
          case stmk_for:
            walk_ptr(ptr->variant.for_loop.extra_info, a_for_loop_ptr,
                     iek_for_loop);
            walk_ptr(ptr->variant.for_loop.statement, a_statement_ptr,
                     iek_statement);
            break;
          case stmk_switch:
            walk_list(ptr->variant.switch_stmt.clause_list,
                      a_switch_clause_ptr, iek_switch_clause);
            walk_ptr(ptr->variant.switch_stmt.body_statement,
                     a_statement_ptr, iek_statement);
            break;
          case stmk_init:
            remap_ptr(ptr->variant.dynamic_init, a_dynamic_init_ptr,
                      iek_dynamic_init);
            break;
          case stmk_asm:
            remap_ptr(ptr->variant.asm_entry, an_asm_entry_ptr,
                      iek_asm_entry);
            break;
          case stmk_try_block:
            walk_ptr(ptr->variant.try_block.statement, a_statement_ptr,
                     iek_statement);
            walk_list(ptr->variant.try_block.handlers, a_handler_ptr,
                      iek_handler);
            break;
#if GENERATE_SOURCE_SEQUENCE_LISTS
          case stmk_decl:
            remap_ptr(ptr->variant.last_declaration,
                      a_source_sequence_entry_ptr, iek_source_sequence_entry);
            break;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
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
      {
        a_scope_ptr ptr = (a_scope_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_scope_ptr, iek_scope);
        switch (ptr->kind) {
          case sck_file:
#ifdef FFE
          case sck_stmt_function:
#endif  /* ifdef FFE */
            /* No pointers */
            break;
#ifdef CFE
          case sck_block:
            /* Call remap_ptr on the handler entry since it is also on a list
               pointed to from the try-block statement. */
            remap_ptr(ptr->variant.assoc_handler, a_handler_ptr, iek_handler);
            /* Also see assoc_block below. */
            break;
          case sck_func_prototype:
          case sck_class_struct_union:
            remap_ptr(ptr->variant.assoc_type, a_type_ptr, iek_type);
            break;
#endif  /* ifdef CFE */
          case sck_function:
            /* "ptr", which points to the routine associated with this scope,
               is done after the declarations. */
            walk_list(ptr->variant.routine.parameters, a_variable_ptr,
                      iek_variable);
#ifdef CFE
            walk_list(ptr->variant.routine.constructor_inits,
                      a_constructor_init_ptr, iek_constructor_init);
            walk_ptr(ptr->variant.routine.this_param_variable, a_variable_ptr,
                    iek_variable);
#endif  /* ifdef CFE */
#ifdef FFE
            walk_ptr (ptr->variant.routine.function_result_var, a_variable_ptr,
                      iek_variable);
#endif /* ifdef FFE */
            break;
#if CHECKING
          case sck_template_declaration:
          case sck_template_instantiation:
            /* Front end only. */
          default:
            internal_error("walk_entry_and_subtree: bad scope kind");
#endif  /* CHECKING */
        }  /* switch */
        /* "assoc_block" is done after the declarations. */
        walk_list(ptr->constants, a_constant_ptr, iek_constant);
#ifdef CFE
        if (walking_file_scope) {
          walk_list(ptr->types, a_type_ptr, iek_type);
          walk_list(ptr->variables, a_variable_ptr, iek_variable);
        } else {
          /* The local "types" and static "variables" at function scope or
             block scope within a function are in the file scope memory region.
             They will be processed during the file scope memory region
             walk because a_scope_orphaned_list_header entry for these lists
             would have been created. */
          remap_ptr(ptr->types, a_type_ptr, iek_type);
          remap_ptr(ptr->variables, a_variable_ptr, iek_variable);
        }  /* if */
        walk_list(ptr->nonstatic_variables, a_variable_ptr, iek_variable);
#else /* ifndef CFE */
        walk_list(ptr->types, a_type_ptr, iek_type);
        walk_list(ptr->variables, a_variable_ptr, iek_variable);
#endif /* ifdef CFE */
        walk_list(ptr->labels, a_label_ptr, iek_label);
        walk_list(ptr->routines, a_routine_ptr, iek_routine);
#ifdef CFE
        walk_list(ptr->scopes, a_scope_ptr, iek_scope);
        walk_list(ptr->asm_entries, an_asm_entry_ptr, iek_asm_entry);
        walk_list(ptr->dynamic_inits, a_dynamic_init_ptr, iek_dynamic_init);
#endif /* ifdef CFE */
#ifdef FFE
        walk_list(ptr->entries, an_entry_description_ptr,
                  iek_entry_description);
        walk_list(ptr->namelist_groups, a_namelist_group_ptr,
                  iek_namelist_group);
#endif /* ifdef FFE */
        if (ptr->kind == (a_scope_kind)sck_function) {
          remap_ptr(ptr->variant.routine.ptr, a_routine_ptr, iek_routine);
          walk_ptr(ptr->assoc_block, a_statement_ptr, iek_statement);
        } else {
          remap_ptr(ptr->assoc_block, a_statement_ptr, iek_statement);
        }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
        walk_list(ptr->source_sequence_list, a_source_sequence_entry_ptr,
                  iek_source_sequence_entry);
        if (!walking_file_scope) {
          /* The src_seq_sublist_list, which appears only on function scopes,
             is not walked at this time: it is handled during orphan list
             processing. */
          remap_ptr(ptr->src_seq_sublist_list, a_src_seq_sublist_ptr,
                    iek_src_seq_sublist);
        }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      }
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
        walk_list(ptr->specifier_list, an_io_specifier_ptr, iek_io_specifier);
        walk_list(ptr->item_list, an_io_list_item_ptr, iek_io_list_item);
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
#ifdef CFE
    case iek_dynamic_init:
      {
        a_dynamic_init_ptr ptr = (a_dynamic_init_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_dynamic_init_ptr, iek_dynamic_init);
        remap_ptr(ptr->variable, a_variable_ptr, iek_variable);
        remap_ptr(ptr->destructor, a_routine_ptr, iek_routine);
        switch (ptr->kind) {
          case dik_none:
          case dik_bitwise_copy:
            /* No pointers. */
            break;
          case dik_constant:
          case dik_nonconstant_aggregate:
            walk_ptr(ptr->variant.constant, a_constant_ptr, iek_constant);
            break;
          case dik_expression:
          case dik_call_returning_class_via_cctor:
            walk_ptr(ptr->variant.expression, an_expr_node_ptr, iek_expr_node);
            break;
          case dik_constructor:
            remap_ptr(ptr->variant.constructor.ptr, a_routine_ptr,
                      iek_routine);
            walk_list(ptr->variant.constructor.args, an_expr_node_ptr,
                      iek_expr_node);
            break;
#if CHECKING
          default:
            internal_error("walk_entry_and_subtree: bad dynamic init kind");
#endif /* CHECKING */
        }  /* switch */
      }
      break;
    case iek_access_adjustment:
      {
        an_access_adjustment_ptr ptr = (an_access_adjustment_ptr)entry_ptr;
        remap_next_ptr(ptr->next, an_access_adjustment_ptr,
                       iek_access_adjustment);
        switch (ptr->kind) {
          case aak_field:
            remap_ptr(ptr->variant.field, a_field_ptr, iek_field);
            break;
          case aak_variable:
            remap_ptr(ptr->variant.variable, a_variable_ptr, iek_variable);
            break;
          case aak_routine:
            remap_ptr(ptr->variant.routine, a_routine_ptr, iek_routine);
            break;
          case aak_type:
            remap_ptr(ptr->variant.type, a_type_ptr, iek_type);
            break;
          case aak_constant:
            remap_ptr(ptr->variant.constant, a_constant_ptr, iek_constant);
            break;
#if CHECKING
          default:
            internal_error(
                         "walk_entry_and_subtree: bad access adjustment kind");
#endif /* CHECKING */
        }  /* switch */
      }
      break;
    case iek_overriding_virtual_function:
      {
        an_overriding_virtual_function_ptr ptr =
                                 (an_overriding_virtual_function_ptr)entry_ptr;
        remap_next_ptr(ptr->next, an_overriding_virtual_function_ptr,
                       iek_overriding_virtual_function);
        remap_ptr(ptr->overriding_function, a_routine_ptr, iek_routine);
        remap_ptr(ptr->primary_function, a_routine_ptr, iek_routine);
        remap_ptr(ptr->base_class, a_base_class_ptr, iek_base_class);
      }
      break;
    case iek_derivation_step:
      {
        a_derivation_step_ptr ptr = (a_derivation_step_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_derivation_step_ptr,
                       iek_derivation_step);
        remap_ptr(ptr->base_class, a_base_class_ptr, iek_base_class);
      }
      break;
    case iek_base_class_derivation:
      {
        a_base_class_derivation_ptr ptr =
                                       (a_base_class_derivation_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_base_class_derivation_ptr,
                       iek_base_class_derivation);
        walk_list(ptr->path, a_derivation_step_ptr, iek_derivation_step);
      }
      break;
    case iek_base_class:
      {
        a_base_class_ptr ptr = (a_base_class_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_base_class_ptr, iek_base_class);
        remap_ptr(ptr->type, a_type_ptr, iek_type);
        remap_ptr(ptr->derived_class, a_type_ptr, iek_type);
#if CFRONT_OBJECT_CODE_COMPATIBILITY
        remap_ptr(ptr->data_section_base_class, a_base_class_ptr,
                  iek_base_class);
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
        remap_ptr(ptr->pointer_base_class, a_base_class_ptr, iek_base_class);
        walk_list(ptr->derivation, a_base_class_derivation_ptr,
                  iek_base_class_derivation);
        walk_list(ptr->overriding_virtual_functions,
                  an_overriding_virtual_function_ptr,
                  iek_overriding_virtual_function);
#if DO_IL_LOWERING
        /* ptr->virtual_function_table_var not processed. */
#endif /* DO_IL_LOWERING */
      }
      break;
    case iek_class_list_entry:
      {
        a_class_list_entry_ptr ptr = (a_class_list_entry_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_class_list_entry_ptr,
                       iek_class_list_entry);
        remap_ptr(ptr->class_type, a_type_ptr, iek_type);
      }
      break;
    case iek_routine_list_entry:
      {
        a_routine_list_entry_ptr ptr = (a_routine_list_entry_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_routine_list_entry_ptr,
                       iek_routine_list_entry);
        remap_ptr(ptr->routine, a_routine_ptr, iek_routine);
      }
      break;
    case iek_class_type_supplement:
      {
        a_class_type_supplement_ptr ptr =
                                        (a_class_type_supplement_ptr)entry_ptr;
        walk_list(ptr->base_classes, a_base_class_ptr, iek_base_class);
        switch (ptr->anonymous_union_kind) {
          case auk_none:
          case auk_variable:
            break;
          case auk_field:
            remap_ptr(ptr->anonymous_union_field, a_field_ptr, iek_field);
            break;
#if CHECKING
          default:
            internal_error("walk_entry_and_subtree: bad anonymous union kind");
#endif /* CHECKING */
        } /* switch */
        remap_ptr(ptr->virtual_function_info_base_class, a_base_class_ptr,
                  iek_base_class);
        walk_list(ptr->access_adjustments, an_access_adjustment_ptr,
                  iek_access_adjustment);
        walk_list(ptr->befriending_classes, a_class_list_entry_ptr,
                  iek_class_list_entry);
        walk_list(ptr->friend_routines, a_routine_list_entry_ptr,
                  iek_routine_list_entry);
        walk_list(ptr->friend_classes, a_class_list_entry_ptr,
                  iek_class_list_entry);
        walk_ptr(ptr->assoc_scope, a_scope_ptr, iek_scope);
        walk_list(ptr->template_arg_list, a_template_arg_ptr,
                  iek_template_arg);
#if NEW_CAN_BE_FOLDED_INTO_CTOR
        remap_ptr(ptr->assoc_operator_new_routine, a_routine_ptr, iek_routine);
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
#if DELETE_CAN_BE_FOLDED_INTO_DTOR
        remap_ptr(ptr->assoc_operator_delete_routine, a_routine_ptr,
                  iek_routine);
#endif /* DELETE_CAN_BE_FOLDED_INTO_DTOR */
#if DO_IL_LOWERING
        /* ptr->virtual_function_table_var not processed. */
        /* ptr->type_as_subobject not processed. */
#endif /* DO_IL_LOWERING */
      }
      break;
    case iek_constructor_init:
      {
        a_constructor_init_ptr ptr = (a_constructor_init_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_constructor_init_ptr,
                       iek_constructor_init);
        switch (ptr->kind) {
          case cik_virtual_base_class:
          case cik_direct_base_class:
            remap_ptr(ptr->variant.base_class, a_base_class_ptr,
                      iek_base_class);
            break;
          case cik_field:
            remap_ptr(ptr->variant.field, a_field_ptr, iek_field);
            break;
#if CHECKING
          default:
            internal_error(
                          "walk_entry_and_subtree: bad constructor init kind");
#endif /* CHECKING */
        }  /* switch */
        walk_ptr(ptr->initializer, a_dynamic_init_ptr, iek_dynamic_init);
      }
      break;
    case iek_asm_entry:
      {
        an_asm_entry_ptr ptr = (an_asm_entry_ptr)entry_ptr;
        remap_next_ptr(ptr->next, an_asm_entry_ptr, iek_asm_entry);
        walk_ptr(ptr->asm_string, a_constant_ptr, iek_constant);
      }
      break;
    case iek_template_arg:
      {
        a_template_arg_ptr ptr = (a_template_arg_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_template_arg_ptr, iek_template_arg);
        if (ptr->is_type) {
          walk_ptr(ptr->variant.type, a_type_ptr, iek_type);
        } else {
          walk_ptr(ptr->variant.constant, a_constant_ptr, iek_constant);
        }  /* if */
      }
      break;
    case iek_new_delete_supplement:
      {
        a_new_delete_supplement_ptr ptr =
                                        (a_new_delete_supplement_ptr)entry_ptr;
        walk_ptr(ptr->type, a_type_ptr, iek_type);
        walk_ptr(ptr->routine, a_routine_ptr, iek_routine);
        walk_list(ptr->arg, an_expr_node_ptr, iek_expr_node);
        walk_ptr(ptr->dynamic_init, a_dynamic_init_ptr, iek_dynamic_init);
        walk_ptr(ptr->delete_routine, a_routine_ptr, iek_routine);
      }
      break;
    case iek_throw_supplement:
      {
        a_throw_supplement_ptr ptr = (a_throw_supplement_ptr)entry_ptr;
        walk_ptr(ptr->type, a_type_ptr, iek_type);
        walk_ptr(ptr->dynamic_init, a_dynamic_init_ptr, iek_dynamic_init);
        walk_list(ptr->accessible_base_classes, an_accessible_base_class_ptr,
                  iek_accessible_base_class);
      }
      break;
    case iek_accessible_base_class:
      {
        an_accessible_base_class_ptr ptr =
                                      (an_accessible_base_class_ptr)entry_ptr;
        
        remap_next_ptr(ptr->next, an_accessible_base_class_ptr,
                       iek_accessible_base_class);
        remap_ptr(ptr->base_class, a_base_class_ptr, iek_base_class);
      }
      break;
#endif /* ifdef CFE */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    case iek_source_sequence_entry:
      {
        a_source_sequence_entry_ptr ptr =
                                       (a_source_sequence_entry_ptr)entry_ptr;
        an_il_entry_kind            kind = (an_il_entry_kind)ptr->entity.kind;

        remap_next_ptr(ptr->next, a_source_sequence_entry_ptr,
                       iek_source_sequence_entry);
        remap_ptr(ptr->prev, a_source_sequence_entry_ptr,
                  iek_source_sequence_entry);
        if (kind == iek_src_seq_secondary_decl ||
            kind == iek_src_seq_end_of_construct) {
          walk_ptr(ptr->entity.ptr, a_char_ptr, kind);
        } else {
          remap_ptr(ptr->entity.ptr, a_char_ptr, kind);
        }  /* if */
      }
      break;
    case iek_src_seq_secondary_decl:
      {
        a_src_seq_secondary_decl_ptr ptr =
                                      (a_src_seq_secondary_decl_ptr)entry_ptr;
        remap_ptr(ptr->entity.ptr, a_char_ptr,
                  (an_il_entry_kind)ptr->entity.kind);
      }
      break;
    case iek_src_seq_end_of_construct:
      {
        a_src_seq_end_of_construct_ptr ptr =
                                    (a_src_seq_end_of_construct_ptr)entry_ptr;
        remap_ptr(ptr->entity.ptr, a_char_ptr,
                  (an_il_entry_kind)ptr->entity.kind);
      }
      break;
    case iek_src_seq_sublist:
      {
        a_src_seq_sublist_ptr ptr = (a_src_seq_sublist_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_src_seq_sublist_ptr, iek_src_seq_sublist);
        walk_list(ptr->source_sequence_list, a_source_sequence_entry_ptr,
                  iek_source_sequence_entry);
        remap_ptr(ptr->last_source_sequence_entry, a_source_sequence_entry_ptr,
                  iek_source_sequence_entry);
      }
      break;
#if COMMENTS_IN_SOURCE_SEQUENCE_LISTS
    case iek_comment:
      /* No pointers. */
      break;
#endif /* COMMENTS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    case iek_scope_orphaned_list_header:
      {
        a_scope_orphaned_list_header_ptr ptr =
                                   (a_scope_orphaned_list_header_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_scope_orphaned_list_header_ptr,
                       iek_scope_orphaned_list_header);
        walk_list(ptr->orphaned_types, a_type_ptr, iek_type);
        walk_list(ptr->orphaned_variables, a_variable_ptr, iek_variable);
        walk_list(ptr->orphaned_src_seq_sublists, a_src_seq_sublist_ptr,
                  iek_src_seq_sublist);
      }
      break;
#if CHECKING
    case iek_id_name:
    case iek_string_text:
    case iek_other_text:
      /* String entries should go to walk_string_entry */
    case iek_none:
    case iek_last:
    default:
      internal_error("walk_entry_and_subtree: bad entry kind");
#endif /* CHECKING */
  }  /* switch */
#if DO_SUBTREE_WALK
  /* Call the routine to process the entry if there is such a routine. */
  if (entry_process_func != NULL) entry_process_func(entry_ptr, entry_kind);
end_of_routine:;
#endif /* DO_SUBTREE_WALK */
}  /* walk_entry_and_subtree */

#undef remap_class_of_which_a_member
#undef walk_source_corresp

#if !DO_SUBTREE_WALK
#if REMAP_ONLY_ROUTINES_NEEDED

void remap_il_header_pointers(void)
/*
Remap the pointers in il_header by running them through walk_remap_func.
The subtree is not processed.
*/
{
  remap_ptr(il_header.primary_source_file, a_source_file_ptr, iek_source_file);
  remap_ptr(il_header.primary_scope, a_scope_ptr, iek_scope);
  remap_ptr(il_header.main_routine, a_routine_ptr, iek_routine);
  remap_ptr(il_header.compiler_version, a_char_ptr, iek_other_text);
  remap_ptr(il_header.time_of_compilation, a_char_ptr, iek_other_text);
  remap_ptr(il_header.scope_orphaned_list_headers,
            a_scope_orphaned_list_header_ptr, iek_scope_orphaned_list_header);
  /* region_scope_entry should not be changed; it's not a pointer into
     IL memory in the usual way.  It's changed explicitly as needed. */
}  /* remap_il_header_pointers. */

#endif /* REMAP_ONLY_ROUTINES_NEEDED */
#endif /* !DO_SUBTREE_WALK */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1993 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
