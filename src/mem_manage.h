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


/*
Determine (if possible) the number of low-order zero bits required in
aligned addresses on the host.  This allows use of masking instead of
a remainder operation.
*/
#if HOST_ALIGNMENT_REQUIRED == 1
#define ALIGNMENT_BITS 0
#else
#if HOST_ALIGNMENT_REQUIRED == 2
#define ALIGNMENT_BITS 0x1
#else
#if HOST_ALIGNMENT_REQUIRED == 4
#define ALIGNMENT_BITS 0x3
#else
#if HOST_ALIGNMENT_REQUIRED == 8
#define ALIGNMENT_BITS 0x7
#else
#if HOST_ALIGNMENT_REQUIRED == 16
#define ALIGNMENT_BITS 0xf
#else
#if HOST_ALIGNMENT_REQUIRED == 32
#define ALIGNMENT_BITS 0x1f
#else
/* Alignment will have to be established with "%". */
#define ALIGNMENT_BITS (-1)
#endif /* == 32 */
#endif /* == 16 */
#endif /* == 8 */
#endif /* == 4 */
#endif /* == 2 */
#endif /* == 1 */

#if ALIGNMENT_BITS == 0
/* No alignment required. */
#define align_expr(value) 0
#else /* ALIGNMENT_BITS != 0 */
#if ALIGNMENT_BITS == -1
/* Alignment requirement is not a recognized small power of two.  Use "%". */
#define align_expr(value) ((value) % HOST_ALIGNMENT_REQUIRED)
#else /* ALIGNMENT_BITS >= 0 */
/* Alignment requirement is a recognized small power of two.  Use masking. */
#define align_expr(value) ((value) & ALIGNMENT_BITS)
#endif /* ALIGNMENT_BITS == -1 */
#endif /* ALIGNMENT_BITS == 0 */


/*
Macro that will round up its argument -- if required -- to make it evenly
divisible by HOST_ALIGNMENT_REQUIRED.
*/
#if ALIGNMENT_BITS == 0

#define do_host_alignment(value) /* Nothing -- no alignment required. */

#else /* ALIGNMENT_BITS != 0 */

#define do_host_alignment(value)                                      \
{ register int excess_bytes = (int)(align_expr(value));               \
  if (excess_bytes != 0) {                                            \
    value += HOST_ALIGNMENT_REQUIRED - excess_bytes;                  \
  }  /* if */                                                         \
}  /* do_host_alignment */

#endif /* ALIGNMENT_BITS == 0 */


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
extern void init_memory_region(a_memory_region_number region_number,
                               sizeof_t               min_size);
/* Indicate a memory region is no longer needed in the front end. */
extern void done_with_memory_region(a_memory_region_number region_number);
/* Free the space in a memory region. */
extern void free_memory_region(a_memory_region_number region_number);
#if !IL_SHOULD_BE_WRITTEN_TO_FILE || !ALTERNATE_IL_FILE_FORMAT
/* Free the unused space in the final block of a memory region. */
extern void trim_memory_region(a_memory_region_number region_number);
#endif /* !IL_SHOULD_BE_WRITTEN_TO_FILE || !ALTERNATE_IL_FILE_FORMAT */

/* purify_discard_memory is used to indicate that a piece of memory
   is no longer needed but need not be freed.  This is used to prevent
   purify from complaining about memory that is deliberately
   discarded. */
#ifdef USING_PURIFY
#define purify_discard_memory(ptr) (void)free(ptr)
#else /* !USING_PURIFY */
#define purify_discard_memory(ptr) /* */
#endif /* USING_PURIFY */

#if DEBUG
/* Display the amount of memory used, for debug purposes. */
extern void show_mem_manage_space_used(unsigned long total_accounted_for);
#endif /* DEBUG */
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
