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

il_walk.c -- Routines to walk the intermediate language tree.

*/


#include "basics.h"
#include "host_envir.h"

#if IL_WALK_NEEDED

#if !ORPHAN_PROCESSING_NEEDED
??=error -- ORPHAN_PROCESSING_NEEDED must be set if IL walking is needed.
#endif /* !ORPHAN_PROCESSING_NEEDED */

#include "lang_feat.h"
#include "il_walk.h"
#include "il.h"
#include "error.h"
#include "mem_manage.h"


static an_entry_process_function_ptr
		entry_process_func;
			/* The function to be called for each non-string entry.
			   NULL if no function is to be called. */
static a_string_entry_process_function_ptr
		string_entry_process_func;
			/* The function to be called for each string entry.
			   NULL if no function is to be called. */
static a_boolean
		walking_file_scope;
			/* TRUE if walking the file-scope IL, FALSE if
			   walking the IL for a function scope. */
static unsigned int
		flag_value_meaning_visited;
			/* Value to be placed in the il_walk_flag field
			   to indicate that an entry has been visited.
			   The value alternates between 0 and 1. */
typedef char	*a_char_ptr;
			/* Useful to indicate "char *" as a type in calling
			   remap_ptr or walk_ptr. */


/* Declarations required because of forward references. */
static void walk_string_entry(char             *entry_ptr,
                              an_il_entry_kind entry_kind,
                              sizeof_t         entry_length);


/* Build a routine to walk entries and their subtrees. */
#define DO_SUBTREE_WALK TRUE
#define WALK_ENTRY_ROUTINE_NAME walk_entry_and_subtree
#include "walk_entry.h"

/* Note that at this point the macros walk_ptr et al. are defined in
   a way that is appropriate for walking entries and their subtrees.
   The code in this file needs that.  At the very end of this file,
   walk_entry.h is included again if needed, and that changes those
   macros to the no-subtree-walk versions, which we don't want to
   get accidentally. */

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
string entries are considered honorary members of the scope from which
they are referenced for purposes of tree walking.
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


/*
Process a list of identical type orphaned file scope IL entries linked
together by the orphaned pointer preceding the IL entry structure.  Each
IL entry will be processed as an individual IL entry.  walk_ptr will be used
to process each entry in the list.  ptr is the pointer to the first IL entry,
ptr_type is the type of the pointer and entry_kind is the kind of entries.
*/
#define walk_orphan_entry_list(ptr, ptr_type, entry_kind) \
{ ptr_type *orph_ptr = (ptr_type *)&(ptr); \
  for (; *orph_ptr != NULL; \
       orph_ptr = (ptr_type *)&fs_orphan_pointer_of(*orph_ptr)) { \
    walk_ptr(*orph_ptr, ptr_type, entry_kind) \
  }  /* for */ \
}  /* walk_orphan_entry_list */

/*
Local macro to ease stepping through the orphaned_file_scopes_il_entries
array and process the lists of IL entries of each type pointed to by the
"first_entry" of each array element.
*/
#define walk_orphan_entry_list_for_entry_kind(ptr_type, entry_kind) \
  walk_orphan_entry_list( \
               orphaned_file_scope_il_entries[(int)entry_kind].first_entry, \
               ptr_type, entry_kind)


static void walk_orphaned_file_scope_il_entries(void)
/*
For each IL entry kind, process any orphaned file scope IL entries chained
to the orphaned_file_scope_il_entries table.  As function scopes were walked,
any file scope IL entries referenced were added onto the list of orphaned
IL entries.  These IL entries may not be and probably are not referenced
from the file scope IL tree.  Walk through the lists of orphaned IL entries
of each kind.  
*/
{
  db_enter(4, "walk_orphaned_file_scope_il_entries");

  /* Process the list of individual IL entries of each IL type. */
  walk_orphan_entry_list_for_entry_kind(a_source_file_ptr, iek_source_file);
  walk_orphan_entry_list_for_entry_kind(a_constant_ptr, iek_constant);
  walk_orphan_entry_list_for_entry_kind(a_param_type_ptr, iek_param_type);
  walk_orphan_entry_list_for_entry_kind(a_routine_type_supplement_ptr,
                                        iek_routine_type_supplement);
  walk_orphan_entry_list_for_entry_kind(a_based_type_list_member_ptr,
                                        iek_based_type_list_member);
  walk_orphan_entry_list_for_entry_kind(a_type_ptr, iek_type);
  walk_orphan_entry_list_for_entry_kind(a_variable_ptr, iek_variable);
#ifdef CFE
  walk_orphan_entry_list_for_entry_kind(a_field_ptr, iek_field);
  walk_orphan_entry_list_for_entry_kind(a_throw_specification_ptr,
                                        iek_throw_specification);
  walk_orphan_entry_list_for_entry_kind(a_throw_spec_type_ptr,
                                        iek_throw_spec_type);
#endif /* ifdef CFE */
  walk_orphan_entry_list_for_entry_kind(a_routine_ptr, iek_routine);
  walk_orphan_entry_list_for_entry_kind(a_label_ptr, iek_label);
  walk_orphan_entry_list_for_entry_kind(an_expr_node_ptr, iek_expr_node);
#ifdef CFE
  walk_orphan_entry_list_for_entry_kind(a_for_loop_ptr, iek_for_loop);
  walk_orphan_entry_list_for_entry_kind(a_switch_clause_ptr,
                                        iek_switch_clause);
  walk_orphan_entry_list_for_entry_kind(a_handler_ptr, iek_handler);
#endif /* ifdef CFE */
  walk_orphan_entry_list_for_entry_kind(a_block_ptr, iek_block);
  walk_orphan_entry_list_for_entry_kind(a_statement_ptr, iek_statement);
  walk_orphan_entry_list_for_entry_kind(a_scope_ptr, iek_scope);
  /* The string types iek_id_name, iek_string_text, and iek_other_text
     are not maintained on an orphan list.  String types at the file
     scope that are referenced from a function scope are written in that
     function scope region. */
#ifdef FFE
  walk_orphan_entry_list_for_entry_kind(an_internal_complex_value_ptr,
                                        iek_internal_complex_value);
  walk_orphan_entry_list_for_entry_kind(a_bound_info_entry_ptr,
                                        iek_bound_info_entry);
  walk_orphan_entry_list_for_entry_kind(a_do_loop_ptr, iek_do_loop);
  walk_orphan_entry_list_for_entry_kind(a_label_list_entry_ptr,
                                        iek_label_list_entry);
  walk_orphan_entry_list_for_entry_kind(an_io_specifier_ptr, iek_io_specifier);
  walk_orphan_entry_list_for_entry_kind(an_io_list_item_ptr, iek_io_list_item);
  walk_orphan_entry_list_for_entry_kind(a_namelist_group_member_ptr,
                                        iek_namelist_group_member);
  walk_orphan_entry_list_for_entry_kind(a_namelist_group_ptr,
                                        iek_namelist_group);
  walk_orphan_entry_list_for_entry_kind(an_input_output_description_ptr,
                                        iek_input_output_description);
  walk_orphan_entry_list_for_entry_kind(an_entry_param_ptr, iek_entry_param);
  walk_orphan_entry_list_for_entry_kind(an_entry_description_ptr,
                                        iek_entry_description);
#endif /* ifdef FFE */
#ifdef CFE
  walk_orphan_entry_list_for_entry_kind(a_dynamic_init_ptr, iek_dynamic_init);
  walk_orphan_entry_list_for_entry_kind(an_access_adjustment_ptr,
                                        iek_access_adjustment);
  walk_orphan_entry_list_for_entry_kind(an_overriding_virtual_function_ptr,
                                        iek_overriding_virtual_function);
  walk_orphan_entry_list_for_entry_kind(a_derivation_step_ptr,
                                        iek_derivation_step);
  walk_orphan_entry_list_for_entry_kind(a_base_class_derivation_ptr,
                                        iek_base_class_derivation);
  walk_orphan_entry_list_for_entry_kind(a_base_class_ptr, iek_base_class);
  walk_orphan_entry_list_for_entry_kind(a_class_list_entry_ptr,
                                        iek_class_list_entry);
  walk_orphan_entry_list_for_entry_kind(a_routine_list_entry_ptr,
                                        iek_routine_list_entry);
  walk_orphan_entry_list_for_entry_kind(a_class_type_supplement_ptr,
                                        iek_class_type_supplement);
  walk_orphan_entry_list_for_entry_kind(a_constructor_init_ptr,
                                        iek_constructor_init);
  walk_orphan_entry_list_for_entry_kind(an_asm_entry_ptr, iek_asm_entry);
  walk_orphan_entry_list_for_entry_kind(a_template_arg_ptr, iek_template_arg);
  walk_orphan_entry_list_for_entry_kind(a_new_delete_supplement_ptr,
                                        iek_new_delete_supplement);
  walk_orphan_entry_list_for_entry_kind(a_throw_supplement_ptr,
                                        iek_throw_supplement);
  walk_orphan_entry_list_for_entry_kind(an_accessible_base_class_ptr,
                                        iek_accessible_base_class);
#endif /* ifdef CFE */
  /* Note that no orphan list walking is needed for iek_source_sequence_entry
     nor for its subordinate entries like iek_src_seq_secondary_decl,
     iek_src_seq_end_of_construct, and iek_comment, since such entries will
     never appear on a orphan list. */

  db_exit();
}  /* walk_orphaned_file_scope_il_entries */


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
  walk_remap_func = remap_function;
  walking_file_scope = TRUE;
#ifdef FFE
  array_bound_walk_index = 0;
#endif /* ifdef FFE */

  /* Process the IL header.  Note that all of these pointers are to the
     file scope memory region. */
  /* Remap and walk the first pointer in two steps, so that we can find
     out what the proper il_walk_flag setting is. */
  remap_ptr(il_header.primary_scope, a_scope_ptr, iek_scope);
  flag_value_meaning_visited =
                     !il_entry_prefix_of(il_header.primary_scope).il_walk_flag;
  walk_entry_and_subtree((char *)il_header.primary_scope, iek_scope);
  walk_ptr(il_header.primary_source_file, a_source_file_ptr, iek_source_file);
  remap_ptr(il_header.main_routine, a_routine_ptr, iek_routine);
  walk_string_ptr(il_header.compiler_version, iek_other_text, 0);
  walk_string_ptr(il_header.time_of_compilation, iek_other_text, 0);
  /* region_scope_entry should not be walked. */
  /* Walk through the orphaned_il_entry_list IL entries that are only
     referenced in "il_header". */
  walk_list(il_header.orphaned_il_list, an_orphaned_il_list_ptr,
            iek_orphaned_il_list);
  /* Walk through the orphaned IL entries referenced from 
     function scopes, but in the file scope memory region. */
  walk_orphaned_file_scope_il_entries();
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
                                           walk_remap_func;
  a_boolean                           prev_walking_file_scope =
                                           walking_file_scope;
  int                                 prev_flag_value_meaning_visited =
                                           flag_value_meaning_visited;
  a_scope_ptr                         scope;

  db_enter(4, "walk_routine_scope_il");
  /* Save the function pointers so they don't have to be passed around. */
  entry_process_func = entry_process_function;
  string_entry_process_func = string_entry_process_function;
  walk_remap_func = remap_function;
  /* Walking a routine scope, not the file scope. */
  walking_file_scope = FALSE;
  scope = il_header.region_scope_entry[region_number];
  flag_value_meaning_visited = !il_entry_prefix_of(scope).il_walk_flag;
#ifdef FFE
  array_bound_walk_index = 0;
#endif /* ifdef FFE */

  /* Process the scope and its subtree. */
  walk_entry_and_subtree((char *)scope, iek_scope);

  /* Restore the previous values of the function pointers etc. */
  entry_process_func = prev_entry_process_func;
  string_entry_process_func = prev_string_entry_process_func;
  walk_remap_func = prev_remap_func;
  walking_file_scope = prev_walking_file_scope;
  flag_value_meaning_visited = prev_flag_value_meaning_visited;

  db_exit();
}  /* walk_routine_scope_il */


/*
Macro to remap an orphan IL entry pointer from an "old" value to a "new"
value.  This macro is similar to remap_ptr, but all pointers are processed
as (char *),  ptr is the pointer, and entry_kind is the kind of entry
pointed to.
*/
#define remap_orphan_ptr(ptr, entry_kind) \
{ if (walk_remap_func != NULL) { \
    (ptr) = (char *)walk_remap_func((char *)(ptr), (entry_kind)); \
  }  /* if */ \
}  /* remap_orphan_ptr */

#if REMAP_ONLY_ROUTINES_NEEDED

void remap_first_ptr_of_orphaned_file_scope_entry_array(void)
/*
Remap the "first" pointers in the orphaned_file_scope_il_entries array by
running them through walk_remap_func.
*/
{
#define remap_orphan_entry_first(kind) \
  remap_orphan_ptr(orphaned_file_scope_il_entries[(int)(kind)].first_entry, \
                   (kind))

  remap_orphan_entry_first(iek_source_file);
  remap_orphan_entry_first(iek_constant);
  remap_orphan_entry_first(iek_param_type);
  remap_orphan_entry_first(iek_routine_type_supplement);
  remap_orphan_entry_first(iek_based_type_list_member);
  remap_orphan_entry_first(iek_type);
  remap_orphan_entry_first(iek_variable);
#ifdef CFE
  remap_orphan_entry_first(iek_field);
  remap_orphan_entry_first(iek_throw_specification);
  remap_orphan_entry_first(iek_throw_spec_type);
#endif /* ifdef CFE */
  remap_orphan_entry_first(iek_routine);
  remap_orphan_entry_first(iek_label);
  remap_orphan_entry_first(iek_expr_node);
#ifdef CFE
  remap_orphan_entry_first(iek_for_loop);
  remap_orphan_entry_first(iek_switch_clause);
  remap_orphan_entry_first(iek_handler);
#endif /* ifdef CFE */
  remap_orphan_entry_first(iek_block);
  remap_orphan_entry_first(iek_statement);
  remap_orphan_entry_first(iek_scope);
  /* The string types iek_id_name, iek_string_text, and iek_other_text
     are not maintained on an orphan list.  String types at the file
     scope that are referenced from a function scope are written in that
     function scope region. */
#ifdef FFE
  remap_orphan_entry_first(iek_internal_complex_value);
  remap_orphan_entry_first(iek_bound_info_entry);
  remap_orphan_entry_first(iek_do_loop);
  remap_orphan_entry_first(iek_label_list_entry);
  remap_orphan_entry_first(iek_io_specifier);
  remap_orphan_entry_first(iek_io_list_item);
  remap_orphan_entry_first(iek_namelist_group_member);
  remap_orphan_entry_first(iek_namelist_group);
  remap_orphan_entry_first(iek_input_output_description);
  remap_orphan_entry_first(iek_entry_param);
  remap_orphan_entry_first(iek_entry_description);
#endif /* ifdef FFE */
#ifdef CFE
  remap_orphan_entry_first(iek_dynamic_init);
  remap_orphan_entry_first(iek_access_adjustment);
  remap_orphan_entry_first(iek_overriding_virtual_function);
  remap_orphan_entry_first(iek_derivation_step);
  remap_orphan_entry_first(iek_base_class_derivation);
  remap_orphan_entry_first(iek_base_class);
  remap_orphan_entry_first(iek_class_list_entry);
  remap_orphan_entry_first(iek_routine_list_entry);
  remap_orphan_entry_first(iek_class_type_supplement);
  remap_orphan_entry_first(iek_constructor_init);
  remap_orphan_entry_first(iek_asm_entry);
  remap_orphan_entry_first(iek_template_arg);
  remap_orphan_entry_first(iek_new_delete_supplement);
  remap_orphan_entry_first(iek_throw_supplement);
  remap_orphan_entry_first(iek_accessible_base_class);
#endif /* ifdef CFE */
  /* Note that nothing is needed for iek_source_sequence_entry nor for its
     subordinate entries like iek_comment, iek_src_seq_secondary_decl, and
     iek_src_seq_end_of_construct, since such entries will never appear on a
     orphan list. */

#undef remap_orphan_entry_first
}  /* remap_first_ptr_of_orphaned_file_scope_entry_array */

#endif /* REMAP_ONLY_ROUTINES_NEEDED */

void remap_last_ptr_of_orphaned_file_scope_entry_array(void)
/*
Remap the "last" pointers in the orphaned_file_scope_il_entries array by
running them through walk_remap_func.
*/
{
#define remap_orphan_entry_last(kind) \
  remap_orphan_ptr(orphaned_file_scope_il_entries[(int)(kind)].last_entry, \
                   (kind))

  remap_orphan_entry_last(iek_source_file);
  remap_orphan_entry_last(iek_constant);
  remap_orphan_entry_last(iek_param_type);
  remap_orphan_entry_last(iek_routine_type_supplement);
  remap_orphan_entry_last(iek_based_type_list_member);
  remap_orphan_entry_last(iek_type);
  remap_orphan_entry_last(iek_variable);
#ifdef CFE
  remap_orphan_entry_last(iek_field);
  remap_orphan_entry_last(iek_throw_specification);
  remap_orphan_entry_last(iek_throw_spec_type);
#endif /* ifdef CFE */
  remap_orphan_entry_last(iek_routine);
  remap_orphan_entry_last(iek_label);
  remap_orphan_entry_last(iek_expr_node);
#ifdef CFE
  remap_orphan_entry_last(iek_for_loop);
  remap_orphan_entry_last(iek_switch_clause);
  remap_orphan_entry_last(iek_handler);
#endif /* ifdef CFE */
  remap_orphan_entry_last(iek_block);
  remap_orphan_entry_last(iek_statement);
  remap_orphan_entry_last(iek_scope);
  /* The string types iek_id_name, iek_string_text, and iek_other_text
     are not maintained on an orphan list.  String types at the file
     scope that are referenced from a function scope are written in that
     function scope region. */
#ifdef FFE
  remap_orphan_entry_last(iek_internal_complex_value);
  remap_orphan_entry_last(iek_bound_info_entry);
  remap_orphan_entry_last(iek_do_loop);
  remap_orphan_entry_last(iek_label_list_entry);
  remap_orphan_entry_last(iek_io_specifier);
  remap_orphan_entry_last(iek_io_list_item);
  remap_orphan_entry_last(iek_namelist_group_member);
  remap_orphan_entry_last(iek_namelist_group);
  remap_orphan_entry_last(iek_input_output_description);
  remap_orphan_entry_last(iek_entry_param);
  remap_orphan_entry_last(iek_entry_description);
#endif /* ifdef FFE */
#ifdef CFE
  remap_orphan_entry_last(iek_dynamic_init);
  remap_orphan_entry_last(iek_access_adjustment);
  remap_orphan_entry_last(iek_overriding_virtual_function);
  remap_orphan_entry_last(iek_derivation_step);
  remap_orphan_entry_last(iek_base_class_derivation);
  remap_orphan_entry_last(iek_base_class);
  remap_orphan_entry_last(iek_class_list_entry);
  remap_orphan_entry_last(iek_routine_list_entry);
  remap_orphan_entry_last(iek_class_type_supplement);
  remap_orphan_entry_last(iek_constructor_init);
  remap_orphan_entry_last(iek_asm_entry);
  remap_orphan_entry_last(iek_template_arg);
  remap_orphan_entry_last(iek_new_delete_supplement);
  remap_orphan_entry_last(iek_throw_supplement);
  remap_orphan_entry_last(iek_accessible_base_class);
#endif /* ifdef CFE */
  /* Note that nothing is needed for iek_source_sequence_entry nor for its
     subordinate entries like iek_comment, iek_src_seq_secondary_decl, and
     iek_src_seq_end_of_construct, since such entries will never appear on a
     orphan list. */

#undef remap_orphan_entry_last
}  /* remap_last_ptr_of_orphaned_file_scope_entry_array */


/* If necessary, build a routine to walk entries in isolation (i.e.,
   not as part of a tree walk).  This is built from the same source
   using different macros. */
#if REMAP_ONLY_ROUTINES_NEEDED
#undef DO_SUBTREE_WALK
#define DO_SUBTREE_WALK FALSE
#undef WALK_ENTRY_ROUTINE_NAME
#define WALK_ENTRY_ROUTINE_NAME remap_pointers_in_il_entry
#include "walk_entry.h"
#endif /* REMAP_ONLY_ROUTINES_NEEDED */

  /* Note that at this point the macros walk_ptr et al. are defined
     in their no-subtree-walk versions.  That's not usually what is
     wanted, so do not add code here unless you expressly want to
     use those. */

#endif /* IL_WALK_NEEDED */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
