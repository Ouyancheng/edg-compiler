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

/*
List of all IL entry kinds:
*/
/* If you change this, also change sizeof_il_entry below. */
typedef enum /*an_il_entry_kind*/ {
  iek_none,		/* Skip zero value; it's used as a marker. */
  iek_source_file,	/* a_source_file */
  iek_constant,		/* a_constant */
  iek_param_type,	/* a_param_type */
  iek_routine_type_supplement,
			/* a_routine_type_supplement */
  iek_based_type_list_member,
			/* a_based_type_list_member */
  iek_type,		/* a_type */
  iek_variable,		/* a_variable */
#ifdef CIL
  iek_field,		/* a_field */
#endif /* ifdef CIL */
  iek_routine,		/* a_routine */
  iek_label,		/* a_label */
  iek_expr_node,	/* an_expr_node */
#ifdef CIL
  iek_switch_clause,	/* a_switch_clause */
#endif /* ifdef CIL */
  iek_block,		/* a_block */
  iek_statement,	/* a_statement */
  iek_scope,		/* a_scope */
  iek_id_name,          /* String giving the name of an identifier. */
  iek_string_text,	/* Text of a string literal. */
  iek_other_text,	/* Text of a file name or similar information. */
#ifdef FIL
  iek_internal_complex_value,
			/* an_internal_complex_value */
  iek_bound_info_entry,	/* a_bound_info_entry */
  iek_do_loop,		/* a_do_loop */
  iek_label_list_entry,	/* a_label_list_entry */
  iek_io_specifier,	/* an_io_specifier */
  iek_io_list_item,	/* an_io_list_item */
  iek_namelist_group_member,
			/* a_namelist_group_member */
  iek_namelist_group,	/* a_namelist_group */
  iek_input_output_description,
			/* an_input_output_description */
  iek_entry_param,	/* an_entry_param */
  iek_entry_description,/* an_entry_description */
#endif /* ifdef FIL */
#ifdef CIL
  iek_dynamic_init,	/* a_dynamic_init */
  iek_access_adjustment,/* an_access_adjustment */
  iek_overriding_virtual_function,
			/* an_overriding_virtual_function */
  iek_derivation_step,  /* a_derivation_step */
  iek_base_class,	/* a_base_class */
  iek_class_list_entry, /* a_class_list_entry */
  iek_class_type_supplement,
			/* a_class_type_supplement */
  iek_constructor_init, /* a_constructor_init */
#endif /* ifdef CIL */
  iek_last		/* Marks the end of the list. */
} an_il_entry_kind;

/*
It is necessary to maintain a list of IL entries that are allocated in
the file scope memory region but accessed from the function scope
region.  These lists are walked during IL file writing and reading
and when displaying the IL to ensure that all IL entries are
visited.  Note, the first_entry and last_entry point to the first
byte of the IL entry.  The address of the next entry in the linked list
precedes the IL entry (entry_ptr - sizeof(char *)).
*/
typedef struct an_orphaned_il_entry_list {
  char *first_entry;	/* Pointer to the first IL entry of a specific
			   kind in a linked list. */
  char *last_entry;	/* Pointer to the last IL entry of a specific
			   kind in a linked list. */
} an_orphaned_il_entry_list;

EXTERN an_orphaned_il_entry_list
		 orphaned_file_scope_il_entries[(int)iek_last];

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
