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
#if IL_WALK_NEEDED || NEED_DECLARATIVE_WALK

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */

/* Type of function called to process each non-string entry.  First arg
   is the (new) pointer to the entry and second is the kind of entry. */
typedef void an_entry_process_function(char *, an_il_entry_kind);
typedef an_entry_process_function *an_entry_process_function_ptr;

#if IL_WALK_NEEDED

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

/*
If this flag is TRUE, the routines that allow remapping of the pointers
in an entry in isolation (i.e., not as part of an IL tree walk) are
compiled.
*/
#define REMAP_ONLY_ROUTINES_NEEDED ALTERNATE_IL_FILE_FORMAT

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

#if REMAP_ONLY_ROUTINES_NEEDED
extern void remap_pointers_in_il_entry(char             *entry_ptr,
                                       an_il_entry_kind entry_kind);

extern void remap_il_header_pointers(void);

extern void remap_first_ptr_of_orphaned_file_scope_entry_array(void);

#endif /* REMAP_ONLY_ROUTINES_NEEDED */

extern void remap_last_ptr_of_orphaned_file_scope_entry_array(void);

#endif /* IL_WALK_NEEDED */

#if NEED_DECLARATIVE_WALK

extern void walk_declarative_entities_in_scope(
                         a_scope_ptr                   scope,
                         an_entry_process_function_ptr entry_process_function);

#endif /* NEED_DECLARATIVE_WALK */

#endif /* IL_WALK_NEEDED || NEED_DECLARATIVE_WALK */
                     
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
