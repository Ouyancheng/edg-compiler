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

#ifndef HOST_ENVIR_H
#include "host_envir.h"
#endif /* ifndef HOST_ENVIR_H */

/* None of this is needed if not walking the IL. */
#if IL_WALK_NEEDED || MAINTAIN_NEEDED_FLAGS || NEED_DECLARATIVE_WALK

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */

/* Type of function called to process each non-string entry.  First arg
   is the (new) pointer to the entry and second is the kind of entry. */
typedef void an_entry_process_function(char *, an_il_entry_kind);
typedef an_entry_process_function *an_entry_process_function_ptr;

#if IL_WALK_NEEDED || MAINTAIN_NEEDED_FLAGS

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
/* Type of function called to test for termination in the IL walk.
   First parameter is the pointer to the entry and second is the kind
   of entry.  Returned value of TRUE means prune the walk at this entry. */
typedef a_boolean a_walk_termination_test_function(char *, an_il_entry_kind);
typedef a_walk_termination_test_function *a_walk_termination_test_function_ptr;

/*
If this flag is TRUE, the IL walk routines that allow remapping of the
pointers in an entry in isolation (i.e., not as part of an IL tree walk) are
compiled.  Note that the top-level routine remap_pointers_in_il_entry is
compiled regardless of the setting of this flag, because it is used
by the trans_copy.c code.
*/
#if IL_SHOULD_BE_WRITTEN_TO_FILE && ALTERNATE_IL_FILE_FORMAT
#define REMAP_ONLY_ROUTINES_NEEDED TRUE
#else /* !(IL_SHOULD_BE_WRITTEN_TO_FILE && ...) */
#define REMAP_ONLY_ROUTINES_NEEDED FALSE
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE && ... */

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
EXTERN unsigned long array_bound_walk_index;
EXTERN unsigned long num_walk_array_bounds;
#endif /* ifdef FFE */

EXTERN a_remap_function_ptr
		walk_remap_func;
			/* The function to be used to remap each pointer
			   from an old value to a new value.  NULL if no
			   remapping is to be done. */
EXTERN a_boolean
		walking_file_scope;
			/* TRUE if walking the file-scope IL, FALSE if
			   walking the IL for a function scope. */
EXTERN a_boolean
		walking_secondary_trans_unit;
			/* TRUE if we are walking an IL tree in a secondary
			   translation unit, FALSE if we are walking the
			   IL in a primary translation unit. */
EXTERN unsigned int
		flag_value_meaning_visited;
			/* Value to be placed in the il_walk_flag field
			   to indicate that an entry has been visited.
			   The value alternates between 0 and 1. */
EXTERN a_boolean
		clear_fe_pointers_during_walk;
			/* If TRUE, pointers to front end information should
			   be cleared during the IL walk. */

#if IL_WALK_NEEDED 

/* Walk the intermediate language tree for the file scope. */
extern void walk_file_scope_il(
            an_entry_process_function_ptr        entry_process_function,
            a_string_entry_process_function_ptr  string_entry_process_function,
            a_remap_function_ptr                 remap_function,
            a_walk_termination_test_function_ptr termination_test_function,
            a_boolean                            clear_fe_pointers);

/* Walk the intermediate language tree for a routine scope. */
extern void walk_routine_scope_il(
            a_memory_region_number               region_number,
            an_entry_process_function_ptr        entry_process_function,
            a_string_entry_process_function_ptr  string_entry_process_function,
            a_remap_function_ptr                 remap_function,
            a_walk_termination_test_function_ptr termination_test_function,
            a_boolean                            clear_fe_pointers);

#endif /* IL_WALK_NEEDED */

#if MAINTAIN_NEEDED_FLAGS && !STANDALONE_UTILITY_PROGRAM
#if ONE_INSTANTIATION_PER_OBJECT
extern void set_per_instantiation_needed_flag(char             *entry_ptr,
                                              an_il_entry_kind entry_kind,
                                              unsigned long    bit_number);
#endif /* ONE_INSTANTIATION_PER_OBJECT */

extern void mark_as_needed(char             *entry_ptr,
                           an_il_entry_kind entry_kind);

extern void mark_as_needed_like(char                    *entry_ptr,
                                an_il_entry_kind        entry_kind,
                                a_source_correspondence *model_scp,
                                a_boolean               set_class_defn_needed);

extern void remark_as_needed(char             *entry_ptr,
                             an_il_entry_kind entry_kind);

extern void mark_to_keep_in_il(char             *entry_ptr,
                               an_il_entry_kind entry_kind);

extern void remark_routine_definition_needed(a_routine_ptr rout);

extern void set_class_keep_definition_in_il(a_type_ptr type);

extern void walk_subtrees_of_local_entities(a_scope_ptr scope);

EXTERN a_boolean
		end_of_file_scope_needed_flags_phase;
			/* TRUE during the phase at the end of the file scope
			   that deals with walking the subtrees of variables
			   and classes to set needed flags. */
#endif /* MAINTAIN_NEEDED_FLAGS && !STANDALONE_UTILITY_PROGRAM */

extern void walk_entry_and_subtree(char             *entry_ptr,
                                   an_il_entry_kind entry_kind);

extern void remap_pointers_in_il_entry(char             *entry_ptr,
                                       an_il_entry_kind entry_kind);

#if REMAP_ONLY_ROUTINES_NEEDED
extern void remap_il_header_pointers(void);

extern void remap_first_ptr_of_orphaned_file_scope_entry_array(void);
#endif /* REMAP_ONLY_ROUTINES_NEEDED */

#if IL_SHOULD_BE_WRITTEN_TO_FILE
extern void remap_last_ptr_of_orphaned_file_scope_entry_array(void);
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

extern void il_walk_init(void);

#endif /* IL_WALK_NEEDED || MAINTAIN_NEEDED_FLAGS */

#if NEED_DECLARATIVE_WALK

extern void walk_declarative_entities_in_scope(
                         a_scope_ptr                   scope,
                         an_entry_process_function_ptr entry_process_function);

#endif /* NEED_DECLARATIVE_WALK */

#endif /* IL_WALK_NEEDED || MAINTAIN_NEEDED_FLAGS || NEED_DECLARATIVE_WALK */
                     
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
