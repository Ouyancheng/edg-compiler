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

mem_tables.h -- Definitions of the memory management data structure.

These are definitions needed both in the front end to create the data
structure and in the back end to understand it.

*/

/* Avoid including these declarations more than once: */
#ifndef MEM_TABLES_H
#define MEM_TABLES_H 1

#ifndef HOST_ENVIR_H
#include "host_envir.h"
#endif /* ifndef HOST_ENVIR_H */

/* Definition of numbers for memory regions. */
typedef int a_memory_region_number;
#define MAX_MEMORY_REGION_NUMBER ((a_memory_region_number)INT_MAX)

#define NULL_region_number  ((a_memory_region_number)0)
#define FRONT_END_REGION_NUMBER ((a_memory_region_number)0)
/* NULL_region_number is also used for the region of information used in
the front end and not written out or otherwise passed to the back end. */
/*
FILE_SCOPE_REGION_NUMBER is the memory region number for the
file scope.  Note that in the front end one should use the global
variable file_scope_region_number if a secondary translation unit
might be involved.
*/
#define FILE_SCOPE_REGION_NUMBER ((a_memory_region_number)1)

EXTERN a_memory_region_number
		file_scope_region_number;
			/* The memory region number for the file scope.
			   Equal to FILE_SCOPE_MEMORY_REGION except when
			   processing a secondary translation unit (e.g.,
			   for export template). */

/*
Header for a block of memory.  One or more of these make up a memory
region.
*/
typedef struct a_mem_block_header *a_mem_block_header_ptr;
typedef struct a_mem_block_header {
  a_mem_block_header_ptr
		next;
			/* Pointer to the next block in the same memory region,
			   or NULL if this is the last block. */
  char		*start_of_block;
			/* Pointer to the first byte of the block (after the
			   block header). */
  char		*next_avail_in_block;
			/* Pointer to the first available byte in this
			   block. */
  char		*after_end_of_block;
			/* Pointer to just after the end of this block. */
  sizeof_t	malloc_size;
			/* If this is the start of a block allocated by malloc,
			   malloc_size is the total size of the block.  If
			   this header is in the middle of a malloc allocation,
			   malloc_size is 0. */
  a_byte_boolean
		trimmed;
			/* TRUE if this block has been trimmed by
			   trim_mem_block. */
} a_mem_block_header;

/*
Entry that precedes each IL entry and indicates some things about it.
*/
typedef struct an_il_entry_prefix *an_il_entry_prefix_ptr;
typedef struct an_il_entry_prefix {
  /* Note that if you add bits here you must adjust NUM_OF_BIT_FIELDS_IN_PREFIX
     below. */
  a_bit_field	file_scope:1;
			/* TRUE if this IL entry is allocated in the file
			   scope memory region. */
  a_bit_field	secondary_trans_unit:1;
			/* TRUE if this IL entry is in a memory region for
			   a secondary translation unit. */
  a_bit_field	il_walk_flag:1;
			/* Flipped between 0 and 1 to indicate entries that
			   have been visited on a given walk through an IL
			   tree. */
  a_bit_field	il_lowering_flag:1;
			/* Flipped from 0 to 1 by IL lowering to indicate
			   IL entries that have been visited. */
			/* This is not conditional on DO_IL_LOWERING because
			   is it also used by trans_copy.c to mark entries
			   that should be merged into their primary translation
			   unit counterparts. */
#if MAINTAIN_NEEDED_FLAGS
  a_bit_field	keep_in_il:1;
			/* TRUE if the entry should be kept in the IL tree
			   (typically, on one of the lists pointed to from
			   the scope entry).  This is important when the
			   "needed" flag is being maintained, because an
			   entry that is not actually needed must sometimes
			   be retained in the IL tree anyway, for the sake
			   of IL consistency (e.g., a file-scope entity that
			   is declared but never referenced inside a "needed"
			   function). */
#endif /* MAINTAIN_NEEDED_FLAGS */
#if IL_SHOULD_BE_WRITTEN_TO_FILE
#if ALTERNATE_IL_FILE_FORMAT
  a_bit_field	entry_written:1;
			/* TRUE once the entry has been written to the IL
			   file.  Needed for string entries, for which
			   multiple copies may be written. */
  /* In the alternate file format, each entry has an entry number.  This
     is where it is stored.   Pick a size that makes the whole prefix
     struct the same size as a long.  (This is just for efficiency;
     other sizes will work too.) */
#define NUM_OF_BIT_FIELDS_IN_PREFIX                                    \
         /*lint --e(506)*/                                             \
         (5 + ((MAINTAIN_NEEDED_FLAGS != 0)?1:0))
#if EDG_MSDOS
  /* Under MS-DOS compilers this bit field is probably bigger than
     an "int", so use "unsigned long". */
#define BITS_IN_ENTRY_NUMBER                                          \
  (sizeof(long)*CHAR_BIT - NUM_OF_BIT_FIELDS_IN_PREFIX)
  unsigned long	entry_number:BITS_IN_ENTRY_NUMBER;
#else /* !EDG_MSDOS */
#define BITS_IN_ENTRY_NUMBER                                          \
  (sizeof(int)*CHAR_BIT - NUM_OF_BIT_FIELDS_IN_PREFIX)
  unsigned int	entry_number:BITS_IN_ENTRY_NUMBER;
#endif /* EDG_MSDOS */
			/* Entry number for the IL entry. */
#endif /* ALTERNATE_IL_FILE_FORMAT */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
} an_il_entry_prefix;


/*
Macro used by clear_il_entry_prefix to clear the IL lowering flag in
an IL entry prefix only if it exists.
*/
/*
This is not conditional on DO_IL_LOWERING because is it also used
by trans_copy.c to mark entries that should be merged into their
primary translation unit counterparts.
*/
#define clear_il_lowering_flag(epp)                                   \
  (epp->il_lowering_flag = initial_value_for_il_lowering_flag)


/*
Macro used by clear_il_entry_prefix to clear the keep-in-IL flag in
an IL entry prefix only if it exists.
*/
#if MAINTAIN_NEEDED_FLAGS
#define clear_keep_in_il_flag(epp) (epp->keep_in_il = FALSE)
#else /* !MAINTAIN_NEEDED_FLAGS */
#define clear_keep_in_il_flag(epp) /* Nothing */
#endif /* MAINTAIN_NEEDED_FLAGS */


/*
Macro used by clear_il_entry_prefix to clear the entry_written flag in an
IL entry prefix only if it exists.
*/
#if IL_SHOULD_BE_WRITTEN_TO_FILE && ALTERNATE_IL_FILE_FORMAT
#define clear_entry_written_flag(epp) (epp->entry_written = FALSE)
#else/* !(IL_SHOULD_BE_WRITTEN_TO_FILE && ALTERNATE_IL_FILE_FORMAT) */
#define clear_entry_written_flag(epp) /* Nothing */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE && ALTERNATE_IL_FILE_FORMAT */


/*
Macro used by clear_il_entry_prefix to clear the entry number in an
IL entry prefix only if it exists.
*/
#if IL_SHOULD_BE_WRITTEN_TO_FILE && ALTERNATE_IL_FILE_FORMAT
#define clear_il_entry_number(epp) (epp->entry_number = 0)
#else/* !(IL_SHOULD_BE_WRITTEN_TO_FILE && ALTERNATE_IL_FILE_FORMAT) */
#define clear_il_entry_number(epp) /* Nothing */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE && ALTERNATE_IL_FILE_FORMAT */


/*
Initialize an IL entry prefix to default values.  ptr is a pointer (of
any type) to the location containing the prefix.  is_in_file_scope is TRUE if
the entry has been allocated in the file scope memory region, FALSE otherwise.
in_sec_trans_unit is TRUE if the entry has been allocated in a memory
region of a secondary translation unit, FALSE otherwise.
*/
#define clear_il_entry_prefix(ptr, is_in_file_scope, in_sec_trans_unit) \
{ an_il_entry_prefix_ptr epp = (an_il_entry_prefix_ptr)ptr;           \
  epp->file_scope = is_in_file_scope;                                 \
  epp->secondary_trans_unit = in_sec_trans_unit;		      \
  epp->il_walk_flag = 0;                                              \
  clear_il_lowering_flag(epp);                                        \
  clear_keep_in_il_flag(epp);                                         \
  clear_entry_written_flag(epp);                                      \
  clear_il_entry_number(epp);                                         \
}  /* clear_il_entry_prefix */


#if ALTERNATE_IL_FILE_FORMAT
typedef unsigned long /* Should be an unsigned type. */
		an_il_entry_number;
			/* Type of entry number when not in the prefix. */
#endif /* ALTERNATE_IL_FILE_FORMAT */
/* Amount of space to allocate for the prefix.  The size is the smallest
   multiple of HOST_ALIGNMENT_REQUIRED that is at least as large as
   the size of an_il_entry_prefix.  This preserves the necessary alignment
   for the entry itself. */
#define SPACE_FOR_IL_ENTRY_PREFIX                                     \
 ((((sizeof(an_il_entry_prefix)-1)/HOST_ALIGNMENT_REQUIRED)+1)*       \
  HOST_ALIGNMENT_REQUIRED)
/* Macro to allow reference to the IL entry prefix that precedes
   the IL entry at ptr. */
#define il_entry_prefix_of(ptr)                                       \
  (*(an_il_entry_prefix_ptr)((char *)(ptr) - SPACE_FOR_IL_ENTRY_PREFIX))

#if ORPHAN_PROCESSING_NEEDED
/* If orphan processing is needed, each IL entry in the file scope
   memory region is preceded by a next-orphaned-entry pointer (the
   pointer also precedes the an_il_entry_prefix). */
/* Amount of space to allocate for the next-orphaned-entry pointer.
   The size is the smallest multiple of HOST_ALIGNMENT_REQUIRED that is
   at least as large as the size of a "char *".  This preserves the
   necessary alignment for the entry itself. */
#define SPACE_FOR_FS_ORPHAN_POINTER                                   \
 ((((sizeof(char *)-1)/HOST_ALIGNMENT_REQUIRED)+1)*                   \
  HOST_ALIGNMENT_REQUIRED)
/*
Macro to allow reference to the next-orphaned-entry pointer that precedes
the file-scope IL entry at ptr.
*/
#define fs_orphan_pointer_of(ptr)                                     \
  (*(char **)((char *)(ptr) -                                         \
              SPACE_FOR_IL_ENTRY_PREFIX - SPACE_FOR_FS_ORPHAN_POINTER))
#else /* !ORPHAN_PROCESSING_NEEDED */
/* SPACE_FOR_FS_ORPHAN_POINTER is also used to compute the location of the
   canonical entry pointer (even if no orphan pointers are allocated). */
#define SPACE_FOR_FS_ORPHAN_POINTER 0
#endif /* ORPHAN_PROCESSING_NEEDED */

/* Amount of space to allocate for the translation unit correspondence
   pointer that is used when compiling multiple translation units.
   (Such a pointer links to the IL for the same entity in a previous
   translation unit.  By following the links the "canonical" entry for a
   certain entity can be found.)  This is allocated for file scope memory
   regions of secondary translation units.  The size is the smallest
   multiple of HOST_ALIGNMENT_REQUIRED that is at least as large as the
   size of a "char *".  This preserves the necessary alignment for the
   entry itself. */
#define SPACE_FOR_TRANS_UNIT_CORRESP_POINTER                          \
 ((((sizeof(char *)-1)/HOST_ALIGNMENT_REQUIRED)+1)*                   \
  HOST_ALIGNMENT_REQUIRED)

/*
Macro to allow reference to the translation unit correspondence pointer
that precedes the file-scope IL entry at ptr.
*/
#define trans_unit_corresp_pointer_of(ptr)                            \
  (*(char **)((char *)(ptr) -                                         \
              SPACE_FOR_IL_ENTRY_PREFIX -                             \
              SPACE_FOR_FS_ORPHAN_POINTER -                           \
              SPACE_FOR_TRANS_UNIT_CORRESP_POINTER))

/*
Return TRUE if the IL entry pointed to by ptr is in the file scope
memory region.  ptr must point to something allocated in an IL memory
region.
*/
#define in_file_scope(ptr) ((a_boolean)(il_entry_prefix_of(ptr).file_scope))

/*
Return TRUE if the IL entry pointed to by ptr is in a secondary
translation unit.  ptr must point to something allocated in an IL memory
region.
*/
#define in_secondary_trans_unit(ptr) \
  ((a_boolean)(il_entry_prefix_of(ptr).secondary_trans_unit))
			

EXTERN a_mem_block_header_ptr
		*mem_region_table;
			/* A dynamically-allocated array.  mem_region_table[i]
			   points to the last memory block header for 
			   region i. */
EXTERN a_memory_region_number
		size_of_mem_region_table;
			/* Current size of mem_region_table (number of regions,
			   not number of bytes). */
EXTERN a_memory_region_number
		highest_used_region_number;
			/* The highest memory region number used so far. */


#endif /* ifndef MEM_TABLES_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1991 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
