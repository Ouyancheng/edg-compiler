/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
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

#ifdef FFE
extern char *alloc_pufe(sizeof_t size);
#endif /* ifdef FFE */
/* Allocate space in "general" storage. */
extern char *alloc_general(sizeof_t size);
/* Free space in "general" storage. */
extern void free_general(a_void_ptr ptr,
                    sizeof_t   size);
/* Resize allocated space in "general" storage. */
extern char *realloc_general(char     *old_ptr,
                             sizeof_t old_size,
                             sizeof_t new_size);
/* Allocate space in a given memory region. */
extern char *alloc_in_region(a_memory_region_number number,
                             sizeof_t               size);

/* Make sure that mem_region_table is large enough. */
extern
void ensure_mem_region_table_space(a_memory_region_number region_number);

/*
Macro to allocate and return "size" bytes of storage that will last through
execution of the front end.
*/
#define alloc_fe(size) alloc_in_region(NULL_region_number, size)

/*
Macro that allocates an entry for the specified type in front end memory.
*/
#define alloc_fe_of_type(type) (type*)alloc_fe(sizeof(type))

/*
Macro that allocates an entry for the specified type in general memory.
*/
#define alloc_general_of_type(type) (type*)alloc_general(sizeof(type))

/* Allocate a block of memory to be used for memory region storage. */
extern a_void_ptr alloc_new_mem_block(sizeof_t size);
/* Allocate an additional memory block for a memory region. */
extern
a_mem_block_header_ptr alloc_mem_block(a_memory_region_number region_number,
                                       sizeof_t               min_size,
                                       char                   *desired_addr,
                                       a_boolean              small_extension);
/* Create a new memory region. */
extern a_memory_region_number new_memory_region(void);
/* Initialize a memory region without allocating the initial block. */
extern void init_memory_region_without_initial_allocation
                                      (a_memory_region_number region_number);
/* Initialize a memory region. */
extern void init_memory_region(a_memory_region_number region_number,
                               sizeof_t               min_size);
/* Check whether a memory region is still needed in the front end. */
extern void check_for_done_with_memory_region(
                                       a_memory_region_number region_number);
#if !STANDALONE_UTILITY_PROGRAM
extern void check_for_done_with_all_function_memory_regions(void);
#endif /* !STANDALONE_UTILITY_PROGRAM */
/* Free the space in a memory region. */
extern void free_memory_region(a_memory_region_number region_number);
/* Free all of the memory regions. */
extern void free_all_memory_regions(void);
/* Free the unused space in the final block of a memory region. */
extern void trim_memory_region(a_memory_region_number region_number);
#if DEBUG
/* Display the amount of memory used, for debug purposes. */
extern void show_mem_manage_space_used(unsigned long total_accounted_for);
#endif /* DEBUG */
/* One-time initialization of  memory management routines. */
extern void mem_manage_one_time_init(void);
/* Initialize memory management. */
extern void mem_manage_trans_unit_init(void);
extern void mem_manage_init(void);

#if !STANDALONE_UTILITY_PROGRAM

extern void record_mapped_mem_block(a_void_ptr	addr,
				    sizeof_t	size);

#if !USE_MMAP_FOR_MEMORY_REGIONS
extern void preallocate_pch_memory(void);
extern void free_unused_pch_memory(void);
#else /* USE_MMAP_FOR_MEMORY_REGIONS */
#define free_unused_pch_memory() /* Nothing */
extern void free_mapped_mem_blocks(void);
#endif /* !USE_MMAP_FOR_MEMORY_REGIONS */

/*
Structure used to record the memory allocations that have been done.
This is used in PCH processing so that the process that reads in
a PCH file can duplicate the sequence of memory allocations done by
the creator of the PCH.
*/
typedef struct a_mem_alloc_history *a_mem_alloc_history_ptr;
typedef struct a_mem_alloc_history {
  a_void_ptr	addr;
			/* Address at which the memory was allocated. */
  sizeof_t	size;
			/* Number of bytes allocated. */
} a_mem_alloc_history;

typedef long	a_mem_alloc_history_number;
			/* Type of an index into the
			    mem_alloc_history array. */

EXTERN a_mem_alloc_history_number
		num_of_mem_alloc_history_entries;
			/* Number of elements used in the memory allocation
			   history array. */

EXTERN a_mem_alloc_history_number
		size_of_mem_alloc_history;
			/* Number of array elements in the memory allocation
			   history array. */

EXTERN a_mem_alloc_history_number
		mem_alloc_history_entries_used;
			/* The number of entries in the mem_alloc_history
			   array for which the associated memory is
			   actually in use by the compilation. */

#if USE_MMAP_FOR_MEMORY_REGIONS
EXTERN a_mem_alloc_history_ptr
		mem_alloc_history;
			/* Pointer to an array of memory allocation history
			   entries. */

#else /* !USE_MMAP_FOR_MEMORY_REGIONS */
#define SIZE_OF_MEM_ALLOC_HISTORY 500
			/* Number of entries in the fixed size memory
			   allocation history array. */

EXTERN a_mem_alloc_history
		mem_alloc_history[SIZE_OF_MEM_ALLOC_HISTORY];
			/* Array of memory allocation history entries used
			   to store the preallocated memory blocks used
			   for PCH processing. */

EXTERN a_boolean
		exhausted_preallocated_memory;
			/* TRUE if all of the preallocated PCH memory has
			   been used, making creation of a PCH impossible. */

EXTERN a_boolean
		large_mem_block_needed;
			/* TRUE if a PCH file cannot be created because
			   a memory block that is larger than those
			   preallocated is needed. */

EXTERN a_source_position
		large_mem_block_error_pos;
			/* Error position when a large entity was
			   allocated that prevented generation of a
			   precompiled header file. */

EXTERN a_mem_alloc_history_number
		total_mem_blocks_allocated;
			/* Total number of memory blocks allocated.  This
			   may be larger than the number of memory history
			   entries when the preallocated memory has been
			   exhausted. */
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */

/*
TRUE if a new PCH may be created containing the information currently
being constructed by the compilation.  This has an effect on how
memory management is done.  The memory for regions that may need to be
written out as part of the PCH cannot be freed until after the PCH is
written.
*/
#define may_be_building_new_pch() (header_stop_position_pending)

/*
Macro that is TRUE if two memory allocation history entries are equivalent.
*/
#define equivalent_mem_alloc_history(m1, m2)				\
  ((m1).addr == (m2).addr && (m1).size == (m2).size)

#endif /* !STANDALONE_UTILITY_PROGRAM */

#if DEBUG
EXTERN unsigned long
		*allocated_in_region;
			/* Parallel array to mem_region_table.  Keeps track
			   of the allocation in each region. */
EXTERN a_memory_region_number
		size_of_allocated_in_region;
			/* Size of allocated_in_region (in entries, not 
			   bytes). */
#endif /* DEBUG */

/*
A general purpose text buffer that is automatically resized as characters
are added.  The string may or may not be null-terminated, but if it is
null-terminated, the null-terminator will be included in "size".
*/
typedef struct a_text_buffer {
  sizeof_t	allocated_size;
			/* The size in bytes of the memory allocated for the
			   buffer. */
  sizeof_t	size;
			/* The number of characters currently in the buffer. */
  sizeof_t	allocation_increment;
			/* Initially, this is the size of the initial memory
			   allocation for the buffer.  Each time the buffer
			   is reallocated, this size is doubled. */
  char		*buffer;
			/* Pointer to the buffer containing the characters. */
} a_text_buffer;

extern void db_text_buffer(char		*prefix,
			   a_text_buffer_ptr	buf);

extern a_text_buffer_ptr alloc_text_buffer(sizeof_t	allocation_increment);

extern void reset_text_buffer(a_text_buffer_ptr	buffer);

extern void expand_text_buffer(a_text_buffer_ptr	buffer,
			       sizeof_t			length);

extern void add_to_text_buffer(a_text_buffer_ptr	buffer,
			       char			*string,
			       sizeof_t			length);

/*
Add the specified string to a text buffer.
*/
#define add_string_to_text_buffer(buffer, string)			\
  (add_to_text_buffer(buffer, string, strlen(string)))

/*
Make sure that the specified buffer has at least "length" total bytes in it.
If not, expand the buffer by reallocating it.
*/
#define ensure_text_buffer_space(buf, length)			\
{ if ((length) > (buf)->allocated_size) {				\
    expand_text_buffer(buf, (sizeof_t)(length));			\
  }  /* if */							\
}  /* ensure_text_buffer_space */

extern void set_buffer_position(a_text_buffer_ptr	buffer,
				char			*pos);

/*
Add the specified character to the text buffer specifier by "buf".
*/
#define add_char_to_text_buffer(buf, ch)				\
{ ensure_text_buffer_space(buf, (buf)->size+1);			\
  (buf)->buffer[(buf)->size] = (ch);					\
  (buf)->size++;							\
}  /* add_char_to_text_buffer */

#endif /* ifndef MEM_MANAGE_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
