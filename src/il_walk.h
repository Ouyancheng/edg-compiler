/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1991 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

il_walk.h -- Declarations related to il_walk.c (walking the intermediate
             language tree).

*/

/* Avoid including these declarations more than once. */
#ifndef IL_WALK_H
#define IL_WALK_H 1

/* None of this is needed if not writing IL to a file. */
#if IL_WALK_NEEDED

#ifndef HOST_ENVIR_H
#include "host_envir.h"
#endif /* ifndef HOST_ENVIR_H */
#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */

/* Macro to test whether or not an entry kind is a string kind. */
#define is_string_entry_kind(entry_kind) \
  ((entry_kind) == iek_id_name || (entry_kind) == iek_string_text || \
   (entry_kind) == iek_other_text)

/* Array giving, for each IL entry kind, the size of the entry in bytes.
   For string type entries, 1.  This must match the order of the
   enumeration an_il_entry_kind. */
EXTERN sizeof_t	sizeof_il_entry[(int)iek_last+1]
#if VAR_INITIALIZERS
= {
  0 /* iek_none */,
  sizeof(a_source_file),
  sizeof(a_constant),
  sizeof(a_param_type),
  sizeof(a_routine_type_supplement),
  sizeof(a_based_type_list_member),
  sizeof(a_type),
  sizeof(a_variable),
#ifdef CIL
  sizeof(a_field),
#endif /* ifdef CIL */
  sizeof(a_routine),
  sizeof(a_label),
  sizeof(an_expr_node),
#ifdef CIL
  sizeof(a_switch_clause),
#endif /* ifdef CIL */
  sizeof(a_block),
  sizeof(a_statement),
  sizeof(a_scope),
  1 /* iek_id_name */,
  1 /* iek_string_text */,
  1 /* iek_other_text */,
#ifdef FIL
  sizeof(an_internal_complex_value),
  sizeof(a_bound_info_entry),
  sizeof(a_do_loop),
  sizeof(a_label_list_entry),
  sizeof(an_io_specifier),
  sizeof(an_io_list_item),
  sizeof(a_namelist_group_member),
  sizeof(a_namelist_group),
  sizeof(an_input_output_description),
  sizeof(an_entry_param),
  sizeof(an_entry_description),
#endif /* ifdef FIL */
#ifdef CIL
  sizeof(a_dynamic_init),
  sizeof(an_access_adjustment),
  sizeof(an_overriding_virtual_function),
  sizeof(a_derivation_step),
  sizeof(a_base_class),
  sizeof(a_class_list_entry),
  sizeof(a_class_type_supplement),
  sizeof(a_constructor_init),
#endif /* ifdef CIL */
  0 /* iek_last */
}
#endif /* VAR_INITIALIZERS */
;

#ifdef FFE
/*
Array bound information entries cause some problems, because they are
laid out as a variable-length array of fixed-length entries.  In the
alternate file format, there is no room preceding each entry for the
storage of the entry number.  To deal with this, the IL walk routines
make the current index in the array of bound info entries available
so that one can tell which entry one is dealing with.  num_walk_array_bounds
indicates the total number of entries.
*/
EXTERN int	array_bound_walk_index;
EXTERN int	num_walk_array_bounds;
#endif /* ifdef FFE */


/* Type of function called to process each non-string entry.  First arg
   is the (new) pointer to the entry and second is the kind of entry. */
typedef void an_entry_process_function(char *, an_il_entry_kind);
typedef an_entry_process_function *an_entry_process_function_ptr;
/* Type of function called to process each string entry.  First arg
   is the (new) pointer to the entry, second is the kind of entry, and
   third is the string length in bytes. */
typedef void a_string_entry_process_function(char *, an_il_entry_kind, 
                                             sizeof_t);
typedef a_string_entry_process_function *a_string_entry_process_function_ptr;
/* Type of function called to remap an old entry pointer to a new entry
   pointer. */
typedef char *a_remap_function(char *, an_il_entry_kind);
typedef a_remap_function *a_remap_function_ptr;

/* Walk the intermediate language tree for the file scope. */
extern void walk_file_scope_il(
             an_entry_process_function_ptr       entry_process_function,
             a_string_entry_process_function_ptr string_entry_process_function,
             a_remap_function_ptr                remap_function);

/* Walk the intermediate language tree for a routine scope. */
extern void walk_routine_scope_il(
             a_memory_region_number              region_number,
             an_entry_process_function_ptr       entry_process_function,
             a_string_entry_process_function_ptr string_entry_process_function,
             a_remap_function_ptr                remap_function);

extern void remap_pointers_in_il_entry(char                 *entry_ptr,
                                       an_il_entry_kind     entry_kind,
                                       a_remap_function_ptr remap_function);

extern void remap_il_header_pointers(a_remap_function_ptr remap_function);


extern void walk_orphaned_file_scope_il_entries(
             an_entry_process_function_ptr       entry_process_function,
             a_string_entry_process_function_ptr string_entry_process_function,
             a_remap_function_ptr                remap_function);

extern void remap_orphaned_file_scope_entry_array_ptrs(
                                       a_remap_function_ptr remap_function);
#endif /* IL_WALK_NEEDED */

#if IL_SHOULD_BE_WRITTEN_TO_FILE || DEBUG || NEED_IL_DISPLAY
extern char *retrieve_il_entry_kind_name(an_il_entry_kind entry_kind);

#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE || DEBUG || NEED_IL_DISPLAY */
                     
#endif /* ifndef IL_WALK_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1991 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
