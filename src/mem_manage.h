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

mem_manage.h -- Declarations relating to mem_manage.c (having to do with
                memory management).

*/

/* Avoid including these declarations more than once. */
#ifndef MEM_MANAGE_H
#define MEM_MANAGE_H 1

#ifndef HOST_ENVIR_H
#include "host_envir.h"
#endif /* ifndef HOST_ENVIR_H */

/* Basic memory management data structures are defined in mem_tables.h. */
#ifndef MEM_TABLES_H
#include "mem_tables.h"
#endif /* ifndef MEM_TABLES_H */

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */


#if DO_IL_LOWERING || IL_SHOULD_BE_WRITTEN_TO_FILE
/*
Static variables of a function scope or block scope are allocated in the
file scope memory region; the same is done for local types.  These IL entries
are not part of the IL tree at the file scope.  A separate list of these is
maintained to provide a means to walk these entries when the IL is lowered
or the IL is written to a file for the file scope memory region.
*/
typedef struct a_group_of_local_scope_entities_allocated_in_file_scope
		 *a_group_of_local_scope_entities_allocated_in_file_scope_ptr;
typedef struct a_group_of_local_scope_entities_allocated_in_file_scope {
  a_group_of_local_scope_entities_allocated_in_file_scope_ptr
		next;
			/* Pointer to the next block_file_scope_list_entry
			   in this linked list. */
  a_variable_ptr
		static_variables;
			/* Pointer to the first of a list of static
			   variables of a function or block scope. */
  a_type_ptr	local_types;
			/* Pointer to the first of a list of local types
			   of a function or block scope. */
} a_group_of_local_scope_entities_allocated_in_file_scope;

EXTERN a_group_of_local_scope_entities_allocated_in_file_scope_ptr
		local_scope_entities_allocated_in_file_scope;
			/* Pointer to a linked list of entries that point to
			   lists of static variable and type IL entries,
			   in the file scope memory region, declared at a
			   function or block scope. */
#endif /* DO_IL_LOWERING || IL_SHOULD_BE_WRITTEN_TO_FILE */

/* Allocate space in "front end" storage. */
extern char *alloc_fe(sizeof_t size);
#ifdef FFE
extern char *alloc_pufe(sizeof_t size);
#endif /* ifdef FFE */
/* Allocate space in "general" storage. */
extern char *alloc_general(sizeof_t size);
/* Resize allocated space in "general" storage. */
extern char *realloc_general(char     *old_ptr,
                             sizeof_t old_size,
                             sizeof_t new_size);
/* Allocate space in a given memory region. */
extern char *alloc_in_region(a_memory_region_number number,
                             sizeof_t               size);
/* Create a new memory region. */
extern a_memory_region_number new_memory_region(void);
/* Initialize a memory region. */
extern void init_memory_region(a_memory_region_number region_number);
/* Indicate a memory region is no longer needed in the front end. */
extern void done_with_memory_region(a_memory_region_number region_number);
/* Free the space in a memory region. */
extern void free_memory_region(a_memory_region_number region_number);
#if DEBUG
/* Display the amount of memory used, for debug purposes. */
extern void show_mem_manage_space_used(unsigned long total_accounted_for);
#endif /* DEBUG */
#if DO_IL_LOWERING || IL_SHOULD_BE_WRITTEN_TO_FILE
extern void preserve_local_scope_entities_allocated_in_file_scope(
	                      		a_memory_region_number region_number);
#endif /* DO_IL_LOWERING || IL_SHOULD_BE_WRITTEN_TO_FILE */
/* Initialize memory management. */
extern void mem_manage_init(void);

#endif /* ifndef MEM_MANAGE_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1991 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
