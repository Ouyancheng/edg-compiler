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

mem_manage.c -- Memory management routines.

*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

#ifndef STDLIB_H_INCLUDED
#if __BSD__
extern char *malloc(unsigned size);
extern int free(char *); /* int to match old-style definition. */
extern char *realloc(char *ptr, unsigned size);
#else /* __SYSV__ */
#include <malloc.h>
#endif /* __BSD__ */
#endif /* ifndef STDLIB_H_INCLUDED */

#if IL_SHOULD_BE_WRITTEN_TO_FILE
#include "il_file.h"
#if !STANDALONE_UTILITY_PROGRAM
#include "il_write.h"
#endif /* !STANDALONE_UTILITY_PROGRAM */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

#if !STANDALONE_UTILITY_PROGRAM
#include "pch.h"
#endif /* !STANDALONE_UTILITY_PROGRAM */

#ifdef USING_PURIFY
#include "purify.h"

static a_boolean
		purify_is_active;
			/* TRUE when a purify'd version of the executable
			   is being used.  This causes the memory allocation
			   routines to allocate the memory in a way that
			   can be tracked by Purify. */
#endif /* USING_PURIFY */


static a_mem_block_header_ptr
		reusable_blocks_list;
			/* List of memory blocks freed and available for
			   reuse.  These are only partial blocks; full blocks
			   are actually freed with free. */

static a_boolean
		okay_to_free_mem_blocks;
			/* TRUE if it is okay to free (using free())
			   memory region blocks that are no longer needed.
			   This is set FALSE if other memory (for example,
			   mmap memory) is being used for memory region
			   blocks. */


/*
Size of a_mem_block_header after adjustment so that the storage following
it will be properly aligned.  This is a constant.
*/
static sizeof_t adjusted_header_size = (sizeof_t)(sizeof(a_mem_block_header) +
  ((sizeof(a_mem_block_header) % HOST_ALIGNMENT_REQUIRED) == 0 ?
     0 : (HOST_ALIGNMENT_REQUIRED -
          (sizeof(a_mem_block_header) % HOST_ALIGNMENT_REQUIRED))));


#if DEBUG
/*
Record of memory allocated, for space tracking purposes.
*/
static unsigned long
		total_mem_allocated;
			/* Total memory allocated via malloc, minus total
			   memory freed via free. */
static unsigned long
		max_mem_allocated;
			/* The high-water mark, the largest value that
			   total_mem_allocated ever had. */
static unsigned long
		total_general_mem_allocated;
			/* The part of total_mem_allocated that was
			   allocated in general storage, i.e., by
			   alloc_general and realloc_general. */
static unsigned long
		total_mem_used;
			/* Total memory allocated by alloc_in_region.
			   Reset for each input file. */
static unsigned long
		num_alignment_bytes_allocated;
			/* Number of bytes wasted in alignment cracks.
			   Reset for each input file. */

#if USE_MMAP_FOR_MEMORY_REGIONS
static unsigned long
		num_mapped_bytes_allocated;
			/* Number of bytes of memory that are mapped to
			   files. */

static unsigned long
		num_mapped_bytes_from_pch;
			/* Number of bytes of memory that have been mapped
			   from a precompiled header file. */
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
#endif /* DEBUG */


#if DEBUG
static void adjust_record_of_total_allocation(long amount)
/*
Record that an additional "amount" bytes of memory have been allocated.
"amount" is negative to indicate space being freed.  "Allocated"
and "freed" here mean via malloc/free, not by some mechanism on top of that.
*/
{
  /* Do "increment" carefully, since one variable is unsigned and the
     other is not. */
  total_mem_allocated = (long)total_mem_allocated + amount;
  /* Keep track of the high-water mark. */
  if (total_mem_allocated > max_mem_allocated) {
    max_mem_allocated = total_mem_allocated;
  }  /* if */
}  /* adjust_record_of_total_allocation */
#endif /* DEBUG */

static char *malloc_with_check(sizeof_t size)
/*
Interface to malloc that allocates "size" bytes.  Checks for failure of 
allocation and generates a catastrophic error.
*/
{
  char *ptr;

  db_enter(5, "malloc_with_check");
  if ((ptr = (char *)malloc((true_size_t)size_t_arg(size))) == NULL) {
    catastrophe(ec_out_of_memory);
  } /* if */
#if DEBUG
  /* Track total allocation. */
  /* Can't do this conditionally on db_active since db_active is not yet
     set when command line processing is done. */
  adjust_record_of_total_allocation((long)size);
  if (debug_level >= 5) {
    fprintf(f_debug, "malloc_with_check: allocating %lu, total = %lu\n",
                     (unsigned long)size,
                     (unsigned long)total_mem_allocated);
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return (ptr);
}  /* malloc_with_check */


#if !DEBUG
/*ARGSUSED*/ /* <-- old_size is not used if !DEBUG. */
#endif /* DEBUG */
static char *realloc_with_check(char     *old_ptr,
                                sizeof_t old_size,
                                sizeof_t new_size)
/*
Interface to realloc: reallocate the block pointed to by "old_ptr" to give
it the new size "new_size".  If "old_ptr" is NULL, works like 
malloc_with_check.  "old_size" is present to help with tracking of space used.
*/
{
  char *ptr;

  /* Don't count on realloc allowing a first parameter of NULL to imply
     malloc-like behavior.  The SVID doesn't define realloc that way. */
  if (old_ptr == NULL) {
    ptr = malloc_with_check(new_size);
  } else {
    if ((ptr = (char *)realloc(old_ptr,
                               (true_size_t)size_t_arg(new_size))) == NULL) {
      catastrophe(ec_out_of_memory);
    } /* if */
#if DEBUG
    /* Track total allocation. */
    /* Can't do this conditionally on db_active since db_active is not yet
       set when command line processing is done. */
    adjust_record_of_total_allocation((long)(new_size - old_size));
    if (debug_level >= 5) {
      fprintf(f_debug,
         "realloc_with_check: new size = %lu, old size = %lu, total = %lu\n",
                         (unsigned long)new_size,
                         (unsigned long)old_size,
                         (unsigned long)total_mem_allocated);
    }  /* if */
#endif /* DEBUG */
  }  /* if */
  return (ptr);
}  /* realloc_with_check */


#if !STANDALONE_UTILITY_PROGRAM

#define MEM_ALLOC_HISTORY_INCREMENTAL_ALLOCATION 500
			/* Initial and incremental allocation sizes for
			   mem_alloc_history.  */


/* Forward declaration. */
static char *realloc_with_check(char     *old_ptr,
                                sizeof_t old_size,
                                sizeof_t new_size);

static void add_mem_alloc_history_entry(a_void_ptr	addr,
		                        sizeof_t	size)
/*
Add an entry to the memory allocation history array.
*/
{
  a_mem_alloc_history_ptr	mahp;
  db_enter(5, "add_mem_alloc_history_entry");
#if USE_MMAP_FOR_MEMORY_REGIONS
  if (num_of_mem_alloc_history_entries == size_of_mem_alloc_history) {
    /* There is no more space in the array, allocate a larger array. */
    a_mem_alloc_history_number	old_size;
    a_mem_alloc_history_number	new_size;
    old_size = size_of_mem_alloc_history;
    new_size = old_size + MEM_ALLOC_HISTORY_INCREMENTAL_ALLOCATION;
    size_of_mem_alloc_history = new_size;
    mem_alloc_history = (a_mem_alloc_history_ptr)realloc_with_check
                          ((char *)mem_alloc_history,
	                   (sizeof_t)(old_size * sizeof(a_mem_alloc_history)),
		           (sizeof_t)(new_size * sizeof(a_mem_alloc_history)));
  }  /* if */
#else /* !USE_MMAP_FOR_MEMORY_REGIONS */
  if (num_of_mem_alloc_history_entries == SIZE_OF_MEM_ALLOC_HISTORY) {
    /* This condition should be handled by the caller. */
    unexpected_condition_str2("add_mem_alloc_history_entry:",
			      "too many memory history entries");
  }  /* if */
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
  mahp = &mem_alloc_history[num_of_mem_alloc_history_entries++];
  mahp->addr = addr;
  mahp->size = size;
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "Added mem_alloc_history, addr: %p, size: %lu\n",
            addr, (unsigned long)size);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* add_mem_alloc_history_entry */


#if !USE_MMAP_FOR_MEMORY_REGIONS
void preallocate_pch_memory(void)
/*
Preallocate the memory to be used for PCH memory region storage when
memory mapping is not available.  The command line processing routine
is responsible for making sure that pch_mem_size is not large enough
to cause the memory allocation history table to overflow.
*/
{
  sizeof_t			size_allocated = 0;

  /* If no value was provided on the command line, use the default value. */
  if (pch_mem_size == 0) pch_mem_size = DEFAULT_PREALLOCATED_PCH_MEM_SIZE;
  while (size_allocated < pch_mem_size) {
    a_void_ptr	ptr;
    ptr = (a_void_ptr)malloc(HOST_ALLOCATION_INCREMENT);
    if (ptr == NULL) {
      catastrophe(ec_out_of_memory_during_pch_allocation);
    }  /* if */
    size_allocated += HOST_ALLOCATION_INCREMENT;
    add_mem_alloc_history_entry(ptr, HOST_ALLOCATION_INCREMENT);
#if DEBUG
    /* Track total allocation. */
    /* Can't do this conditionally on db_active since db_active is not yet
       set when command line processing is done. */
    adjust_record_of_total_allocation((long)HOST_ALLOCATION_INCREMENT);
#endif /* DEBUG */
  }  /* while */
}  /* preallocate_pch_memory */


void free_unused_pch_memory(void)
/*
We have completed any PCH processing that is required.  Release any
preallocated memory that has not been used.
*/
{
  a_mem_alloc_history_number	n;

  /* Free any unused preallocated blocks. */
  for (n = mem_alloc_history_entries_used;
       n < num_of_mem_alloc_history_entries; ++n) {
    (void)free(mem_alloc_history[n].addr);
#if DEBUG
    adjust_record_of_total_allocation(-(long)(mem_alloc_history[n].size));
#endif /* DEBUG */
  }  /* for */
  /* Set the number of entries in existence to the number used so far.
     This will cause any new memory blocks to malloc new memory instead
     of trying to use the preallocated memory. */
  num_of_mem_alloc_history_entries = mem_alloc_history_entries_used;
}  /* free_unused_pch_memory */
#endif /* !USE_MMAP_FOR_MEMORY_REGIONS */


#if USE_MMAP_FOR_MEMORY_REGIONS
static a_boolean
		mmap_initialized;
			/* TRUE when the file used for mmap has been
			   opened. */

static sizeof_t	mmap_size_allocated;
			/* The number of bytes of mapped memory that have
			   been allocated.  This is usually the same as
			   mmap_file_offset, except when a precompiled
			   header file is in use.  The memory mapped from
			   the PCH file is included in mmap_size_allocated,
			   but not in mmap_file_offset. */

static long	mmap_file_offset;
			/* The offset into the mmap file of the next block
			   to be allocated. */

void record_mapped_mem_block(a_void_ptr	addr,
			     sizeof_t	size)
/*
Create a memory allocation history entry for a block of memory
mapped to a file.  This is used when regions from a PCH input
file are mapped into the address space of subsequent compilation.
*/
{
  /* Record this allocation in the memory allocation history array. */
  add_mem_alloc_history_entry(addr, size);
  /* The number of entries actually used is always the same as the
     number of entries that exist in mmap mode. */
  mem_alloc_history_entries_used = num_of_mem_alloc_history_entries;
#if DEBUG
  /* Record the total amount of allocated memory that was allocated via
     memory mapped files. */
  num_mapped_bytes_allocated += size;
  num_mapped_bytes_from_pch += size;
  mmap_size_allocated += size;
  adjust_record_of_total_allocation((long)size);
#endif /* DEBUG */
}  /* record_mapped_mem_block */


void free_mapped_mem_blocks(void)
/*
Unmap the memory blocks that have been mapped.
*/
{
  a_mem_alloc_history_number	n;

  for (n = 0; n < num_of_mem_alloc_history_entries; ++n) {
    sizeof_t	size = mem_alloc_history[n].size;
    unmap_memory(mem_alloc_history[n].addr, size);
#if DEBUG
    /* Record the total amount of allocated memory that was allocated via
       memory mapped files. */
    num_mapped_bytes_allocated -= size;
    adjust_record_of_total_allocation(-(long)size);
#endif /* DEBUG */
  }  /* for */
  num_of_mem_alloc_history_entries = 0;
  mem_alloc_history_entries_used = 0;
  /* Reset the number of bytes allocated in the memory mapped file so that
     any new allocations will start over from the beginning of the file. */
  mmap_size_allocated = 0;
  mmap_file_offset = 0;
}  /* free_mapped_mem_blocks */


a_void_ptr alloc_new_mem_block(sizeof_t size)
/*
Allocate a block of memory to be used for memory region storage.  This
version uses a memory mapped file to obtain the storage.  This is done
so that the memory region storage may be obtained in a separate
range of addresses.  This, in turn, simplifies the processing needed
to ensure that the precompiled header processing routines can read
the memory regions into the same addresses that were used when the
PCH was created.
*/
{
  a_void_ptr	addr;

  if (!mmap_initialized) {
    /* On the first call, open the file that will be mapped. */
    open_mapped_il_temp_file();
    mmap_size_allocated = 0;
    mmap_initialized = TRUE;
    mmap_file_offset = 0;
  }  /* if */
  addr = map_file_region(mmap_size_allocated, size, mmap_file_offset);
  if (addr == NULL) {
    catastrophe(ec_unable_to_get_mapped_memory);
  }  /* if */
  mmap_size_allocated += size;
  mmap_file_offset += size;
  /* Record this allocation in the memory allocation history array. */
  add_mem_alloc_history_entry(addr, size);
  /* The number of entries actually used is always the same as the
     number of entries that exist in mmap mode. */
  mem_alloc_history_entries_used = num_of_mem_alloc_history_entries;
#if DEBUG
  /* Record the total amount of allocated memory that was allocated via
     memory mapped files. */
  num_mapped_bytes_allocated += size;
  adjust_record_of_total_allocation((long)size);
  if (debug_level >= 5) {
    fprintf(f_debug, "Allocated %lu bytes of mapped memory at %p\n",
            (unsigned long)size, addr);
  }  /* if */
#endif /* DEBUG */
  return addr;
}  /* alloc_new_mem_block */

#else /* !USE_MMAP_FOR_MEMORY_REGIONS */

a_void_ptr alloc_new_mem_block(sizeof_t size)
/*
Allocate a block of memory to be used for memory region storage.  This
version uses a fixed block of memory allocated at the beginning of
the compilation.  This is done to maximize the probability that
the memory addresses used in one compilation will match the addresses
used in a subsequent compilation, which is necessary when using
precompiled headers.  When the preallocated memory is exhausted,
additional memory is obtained using malloc and any use of
precompiled headers is suppressed.
*/
{
  a_void_ptr		addr;
  static a_boolean	additional_allocation_needed = FALSE;

  if (!additional_allocation_needed) {
    if (mem_alloc_history_entries_used == num_of_mem_alloc_history_entries) {
      /* All of the preallocated memory has been used. */
      exhausted_preallocated_memory = TRUE;
      additional_allocation_needed = TRUE;
    } else if (size != HOST_ALLOCATION_INCREMENT) {
      /* A memory block is required that is larger than any of the ones that
         have been preallocated. */
      large_mem_block_needed = TRUE;
      large_mem_block_error_pos = error_position;
      additional_allocation_needed = TRUE;
    }  /* if */
    if (additional_allocation_needed) {
      /* A condition occurred which makes it impossible to create a
         precompiled header file. */
      suppress_creation_of_pch();
      /* Free any unused preallocated blocks. */
      free_unused_pch_memory();
    }  /* if */
  }  /* if */
  if (additional_allocation_needed) {
    /* On this call, or a previous call, we needed to use memory other
       than that was preallocated.  All subsequent allocations should
       just be done by malloc. */
    addr = (a_void_ptr)malloc_with_check(size);
  } else {
    /* Get the next entry from the preallocated list. */
    addr = mem_alloc_history[mem_alloc_history_entries_used++].addr;
  }  /* if */
  total_mem_blocks_allocated++;
  return addr;
}  /* alloc_new_mem_block */
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
#endif /* !STANDALONE_UTILITY_PROGRAM */
 

a_mem_block_header_ptr alloc_mem_block(a_memory_region_number region_number,
                                       sizeof_t               min_size,
                                       char                   *desired_addr,
                                       a_boolean              small_extension)
/*
Add a new memory block to the existing blocks for the indicated region.
The memory block must have at least "min_size" bytes available in it.
If desired_addr is not NULL, look for a memory block for which the
actual start_of_block is at the specified address.  If small_extension
is TRUE, this allocation is a small extension on a memory region; allocate
a smaller-sized block.  Return a pointer to the block header.
*/
{
  a_mem_block_header_ptr hdr, prev_hdr;
  sizeof_t               alloc_size, needed_size, default_size;
  a_void_ptr             alloc_addr;
  a_mem_block_header_ptr hdr_found = NULL;
  a_mem_block_header_ptr prev_hdr_found = NULL;

  db_enter(5, "alloc_mem_block");
  /* Determine the desirable default allocation size. */
  if (small_extension) {
    default_size = (sizeof_t)2048;
  } else {
    default_size = HOST_ALLOCATION_INCREMENT;
  }  /* if */
  /* Reuse a previously-allocated piece if possible.  Such a piece was
     the wasted space on the end of a previous block. */
  if (reusable_blocks_list != NULL) {
    needed_size = min_size + adjusted_header_size;
    for (prev_hdr = NULL, hdr = reusable_blocks_list;
         hdr != NULL;
         prev_hdr = hdr, hdr = hdr->next) {
      /* See if the area is big enough (it almost always will be). */
      /* Suppress the CodeCenter warning caused because after_end_of_block
         may be pointing to memory that is not allocated, or is part of a
         different allocation. */
      /*SUPPRESS 22*/
      alloc_size = hdr->after_end_of_block - hdr->start_of_block +
                   adjusted_header_size;
      if (alloc_size >= needed_size) {
        if (hdr->start_of_block == desired_addr ||
            (hdr_found == NULL &&
             /* Don't waste a large available block as an extension for
                a memory region, because we are unlikely to use it up. */
             (!small_extension || alloc_size <= default_size))) {
           /* We've found a candidate, or if this is the desired address,
              we've found a definite match.  Save a pointer to this block */
           hdr_found = hdr;
           prev_hdr_found = prev_hdr;
           if (desired_addr == NULL ||
               hdr->start_of_block == desired_addr) break;
        }  /* if */
      }  /* if */
    }  /* for */
    if (hdr_found != NULL) {
      /* We've found an acceptable piece.  Take it out of the list
         and use it. */
      if (prev_hdr_found == NULL) {
        reusable_blocks_list = hdr_found->next;
      } else {
        prev_hdr_found->next = hdr_found->next;
      }  /* if */
#if DEBUG
      if (debug_level >= 5) {
        fprintf(f_debug, "alloc_mem_block: reusing block, size = %lu\n",
                         (unsigned long)alloc_size);
      }  /* if */
#endif /* DEBUG */
      hdr = hdr_found;
      goto have_hdr;
    }  /* if */
  }  /* if */
  /* No piece available for reuse, so allocate a new one. */
  alloc_size = min_size + adjusted_header_size;
#ifdef USING_PURIFY
  if (!purify_is_active) {
#endif /* USING_PURIFY */
    /* Use the default allocation size unless the minimum required
       size is bigger than that (that's possible for incredibly large
       string literals formed by token concatenation). */
    if (alloc_size < default_size) alloc_size = default_size;
#ifdef USING_PURIFY
  } else {
    /* Don't use the default allocation size when using Purify.  Just
       allocate a block of the proper size. */
    /* If the minimum size is zero, allocate HOST_ALIGNMENT_REQUIRED bytes
       of storage beyond what is used by the header, which is the
       minimum possible allocation. */
    if (min_size == 0) alloc_size += HOST_ALIGNMENT_REQUIRED;
  }  /* if */
#endif /* USING_PURIFY */
  /* Make sure the block size preserves alignment of the end (this is
     just so that we're not allocating space at the end that can hardly 
     ever be used). */
  do_host_alignment(alloc_size);
#if STANDALONE_UTILITY_PROGRAM
  alloc_addr = malloc_with_check(alloc_size);
#else /* !STANDALONE_UTILITY_PROGRAM */
  if (precompiled_header_processing_required) {
#if USE_MMAP_FOR_MEMORY_REGIONS
    /* When using mmap, make sure the size is a multiple of the host
       page size. */
    alloc_size = do_page_alignment(alloc_size);
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
    alloc_addr = alloc_new_mem_block(alloc_size);
  } else {
    alloc_addr = malloc_with_check(alloc_size);
  }  /* if */
#endif /* STANDALONE_UTILITY_PROGRAM */
  /* Fill in the block header. */
  hdr = (a_mem_block_header_ptr)alloc_addr;
  /* malloc_size non-zero indicates that this block came directly from
     malloc. */
  hdr->malloc_size = alloc_size;
  hdr->start_of_block = (char *)alloc_addr + adjusted_header_size;
  hdr->after_end_of_block = (char *)alloc_addr + alloc_size;
have_hdr:
  /* Everything in the block is available. */
  hdr->next_avail_in_block = hdr->start_of_block;
  hdr->trimmed = FALSE;
#ifdef USING_PURIFY
  /* When using Purify, reserve the smallest possible piece of memory
     at the beginning of the block.  This is done to prevent what
     looks like an unused memory block from being created. */
  if (min_size == 0 && purify_is_active) {
    hdr->next_avail_in_block += HOST_ALIGNMENT_REQUIRED;
  }  /* if */
#endif /* USING_PURIFY */
  /* Link the block into the region. */
  hdr->next = mem_region_table[region_number];
  mem_region_table[region_number] = hdr;
  db_exit();
  return (hdr);
}  /* alloc_mem_block */


static void free_complete_block(a_mem_block_header_ptr hdr)
/*
Free a block that was allocated by malloc.
*/
{
#if DEBUG
  /* Can't do this conditionally on db_active since db_active is not yet
     set when command line processing is done. */
  adjust_record_of_total_allocation(-((long)hdr->malloc_size));
  if (debug_level >= 5) {
    fprintf(f_debug, "free_complete_block: freeing block of size %lu\n",
                     (unsigned long)hdr->malloc_size);
  }  /* if */
#endif /* DEBUG */
  free((char *)hdr);
}  /* free_complete_block */


static void free_mem_block(a_mem_block_header_ptr hdr)
/*
Free the storage associated with the indicated memory block.
*/
{
  a_mem_block_header_ptr test_hdr, prev_hdr;

  db_enter(5, "free_mem_block");
  if (okay_to_free_mem_blocks && hdr->malloc_size > 0 &&
      hdr->malloc_size == (sizeof_t)(hdr->after_end_of_block - (char *)hdr)) {
    /* Blocks that are complete blocks as originally allocated by malloc
       can be freed by calling free. */
    free_complete_block(hdr);
  } else {
    /* Other blocks cannot be freed that way.  Instead, they are put on
       a linked list of freed blocks.  As blocks are added, they are
       checked against the existing freed blocks so that adjacent pieces
       can be reunited.  If pieces reunite into a complete block, the 
       block can be freed by calling free.  Make sure that the block
       that is being added is not from a different allocation
       (e.g., from a different malloc call).  Memory blocks from
       different low level allocations cannot be merged. */
    for (prev_hdr = NULL, test_hdr = reusable_blocks_list;
         test_hdr != NULL;
         test_hdr = test_hdr->next) {
      /* Suppress the CodeCenter warning caused because after_end_of_block
         may be point to memory that is not allocated, or is part of a
         different allocation. */
      /*SUPPRESS 29*/
      if ((test_hdr->after_end_of_block == (char *)hdr &&
           hdr->malloc_size == 0) ||
          (hdr->after_end_of_block == (char *)test_hdr &&
           test_hdr->malloc_size == 0)) {
        /* The block on the list is adjacent to the new block.  Remove
           the test_hdr block from the list and join the two blocks together
           as a bigger block pointed to by hdr.  Also back up the loop 
           pointers to get the proper next iteration of the loop. */
        if (prev_hdr == NULL) {
          reusable_blocks_list = test_hdr->next;
        } else {
          prev_hdr->next = test_hdr->next;
        }  /* if */
        if (test_hdr->after_end_of_block == (char *)hdr) {
          /* The new block follows the test_hdr block. */
          test_hdr->after_end_of_block = hdr->after_end_of_block;
          hdr = test_hdr;
        } else {
          /* The test_hdr block follows the new block. */
          hdr->after_end_of_block = test_hdr->after_end_of_block;
        }  /* if */
        /* Is the aggregate block now a complete block?  If so, free it and
           leave the loop. */
        if (okay_to_free_mem_blocks && hdr->malloc_size > 0 &&
            hdr->malloc_size ==
                           (sizeof_t)(hdr->after_end_of_block - (char *)hdr)) {
          free_complete_block(hdr);
          goto freed_it;
        }  /* if */
        /* prev_hdr must be left as it is for the next iteration. */
      } else {
        /* Normal case; block on list is not adjacent to new block. */
        prev_hdr = test_hdr;        
      }  /* if */
    }  /* for */
    /* Add the resulting block to the list. */
    hdr->next = reusable_blocks_list;
    reusable_blocks_list = hdr;
freed_it:;
  }  /* if */
  db_exit();
}  /* free_mem_block */


static void trim_mem_block(a_mem_block_header_ptr hdr)
/*
Free any unallocated space remaining in the indicated memory block.
*/
{
  sizeof_t               space_remaining_in_block;
  a_mem_block_header_ptr new_hdr;
  char                   *alloc_addr;

  db_enter(5, "trim_mem_block");
  /* Save the remaining space only if it's big enough. */
  space_remaining_in_block = hdr->after_end_of_block -
                             hdr->next_avail_in_block;
  /* The criterion for "big enough" is really not for very much space.
     Even very small blocks can be reused, at a minor cost in
     execution time if the file scope region gets too fragmented. */
  if (space_remaining_in_block >= sizeof(a_mem_block_header) +
                                  10*sizeof(a_constant)) {
    /* Remaining space is "big enough" that it's worth saving.  We know
       next_avail_in_block is properly aligned because of the way that
       alloc_in_region works.  Fabricate a header for the space, then 
       free it. */
    alloc_addr = hdr->next_avail_in_block;
    new_hdr = (a_mem_block_header_ptr)alloc_addr;
    /* Indicate that the area didn't come directly from malloc. */
    new_hdr->malloc_size = 0;
    new_hdr->next_avail_in_block = new_hdr->start_of_block =
                                  alloc_addr + adjusted_header_size;
    new_hdr->after_end_of_block = alloc_addr + space_remaining_in_block;
    new_hdr->trimmed = FALSE;
    /* Free the block. */
    free_mem_block(new_hdr);
    /* Trim the original block so it does not include the freed space. */
    hdr->after_end_of_block = alloc_addr;
  }  /* if */
  hdr->trimmed = TRUE;
  db_exit();
}  /* trim_mem_block */


void ensure_mem_region_table_space(a_memory_region_number region_number)
/*
Make sure that the memory region tables are large enough to hold
the number of entries indicated by region_number.
*/
{
  a_memory_region_number old_size;

  if (region_number >= size_of_mem_region_table) {
    /* mem_region_table must be created or enlarged. */
    /* Add enough entries to cover a pretty large compilation (each function
       compiled uses one entry). */
    old_size = size_of_mem_region_table;
    size_of_mem_region_table = region_number + 500;
    mem_region_table = (a_mem_block_header_ptr *)realloc_with_check(
                          (char *)mem_region_table,
                          (sizeof_t)(old_size*sizeof(a_mem_block_header_ptr)),
                          (sizeof_t)(size_of_mem_region_table*
                                              sizeof(a_mem_block_header_ptr)));
    /* Depending on NULL represented as zero bits here. */
    memzero((char *)&mem_region_table[old_size],
            size_t_arg((size_of_mem_region_table-old_size)*
                       sizeof(a_mem_block_header_ptr)));
    /* region_scope_entry is a parallel array to mem_region_table, and must
       be similarly expanded. */
    il_header.region_scope_entry = (a_scope_ptr *)realloc_with_check(
                          (char *)il_header.region_scope_entry,
                          (sizeof_t)(old_size*sizeof(a_scope_ptr)),
                          (sizeof_t)(size_of_mem_region_table*
                                                         sizeof(a_scope_ptr)));
    /* Depending on NULL represented as zero bits here. */
    memzero((char *)&il_header.region_scope_entry[old_size],
            size_t_arg((size_of_mem_region_table-old_size)*
                       sizeof(a_scope_ptr)));
#if IL_SHOULD_BE_WRITTEN_TO_FILE
    /* ... and also index_for_il_file. */
    index_for_il_file = (a_file_position *)realloc_with_check(
                          (char *)index_for_il_file,
                          (sizeof_t)(old_size*sizeof(a_file_position)),
                          (sizeof_t)(size_of_mem_region_table*
                                                    sizeof(a_file_position))); 
    memzero((char *)&index_for_il_file[old_size],
            size_t_arg((size_of_mem_region_table-old_size)*
                       sizeof(a_file_position)));
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
  }  /* if */
#if DEBUG
  /* ... and also allocated_in_region.  Note that this allocation might
         be out of sync with the others. */
  /* Can't do this conditionally on db_active since db_active is not yet
     set when command line processing is done. */
  if (size_of_allocated_in_region < size_of_mem_region_table) {
    allocated_in_region = (unsigned long *)realloc_with_check(
                          (char *)allocated_in_region,
                          (sizeof_t)(size_of_allocated_in_region*
                                                        sizeof(unsigned long)),
                          (sizeof_t)(size_of_mem_region_table*
                                                       sizeof(unsigned long)));
    /* Zero the new entries. */
    memzero((char *)&allocated_in_region[size_of_allocated_in_region],
            size_t_arg((size_of_mem_region_table-size_of_allocated_in_region)*
                       sizeof(unsigned long)));
    size_of_allocated_in_region = size_of_mem_region_table;
  }  /* if */
#endif /* DEBUG */
}  /* ensure_mem_region_table_space */


void init_memory_region_without_initial_allocation
                        (a_memory_region_number region_number)
/*
Initialize the indicated region number.
*/
{
  ensure_mem_region_table_space(region_number);
  mem_region_table[region_number] = NULL;
  /* Keep track of the highest memory region number used. */
  if (region_number > highest_used_region_number) {
    highest_used_region_number = region_number;
  }  /* if */
}  /* init_memory_region_without_initial_allocation */


void init_memory_region(a_memory_region_number region_number,
                        sizeof_t               min_size)
/*
Initialize the indicated region number.  Allocate at least min_size bytes
as the initial allocation for the region.  In general, new_memory_region
should be called instead.  init_memory_region is called directly for the
special "front end" memory region.
*/
{
  init_memory_region_without_initial_allocation(region_number);
  /* Allocate the initial memory block. */
  (void)alloc_mem_block(region_number, min_size, (char *)NULL,
                        /*small_extension=*/FALSE);
}  /* init_memory_region */


a_memory_region_number new_memory_region(void)
/*
Create a new memory region and return its memory region number.
A new region is used for each function's executable code and data.
*/
{
  a_memory_region_number region_number;

  db_enter(5, "new_memory_region");
  if (highest_used_region_number == MAX_MEMORY_REGION_NUMBER) {
    /* Too many regions (extremely unlikely). */
    catastrophe(ec_program_too_large);
  }  /* if */
  region_number = ++highest_used_region_number;
#if DEBUG
  if (debug_level >= 2) {
    fprintf(f_debug, "New memory region, number %ld.\n", (long)region_number);
  }  /* if */
#endif /* DEBUG */

  init_memory_region(region_number, (sizeof_t)0);

  db_exit();
  return (region_number);
}  /* new_memory_region */


char *alloc_in_region(a_memory_region_number region_number,
                      sizeof_t               size)
/*
Allocate "size" bytes in memory region "region_number", and return a
pointer to them.  Generate a catastrophic error and do not return if
the storage cannot be allocated.  Memory region 0 (NULL_region_number)
is used for allocation of general front end memory (i.e., not IL).
*/
{
  char                   *temp_ptr;
  a_mem_block_header_ptr hdr;
#if DEBUG
  sizeof_t               orig_size = size;
#endif /* DEBUG */
  /* Round up the size if necessary to preserve alignment.  Note that
     aside from keeping the data correctly aligned, this also keeps the
     next available address properly aligned, which is important in
     trim_mem_block. */
  do_host_alignment(size);

  /* See if enough space remains in the current block.  If not, get
     a new block.  Note that we add the required host alignment to the
     requested allocation size.  This is done to ensure that no piece
     of memory ends precisely at the end of low-level allocation.  On
     some systems this can cause memory faults by system routines that
     seem to make the assumption that this won't occur. */
  hdr = mem_region_table[region_number];
  /* Suppress the CodeCenter warning caused because after_end_of_block
     may be point to memory that is not allocated, or is part of a
     different allocation. */
  /*SUPPRESS 22*/
  if ((size + HOST_ALIGNMENT_REQUIRED) >
      (sizeof_t)(hdr->after_end_of_block - hdr->next_avail_in_block)) {
    /* Not enough space remaining in current block.  Free any unused
       space at the end of the current last block, and start a new block.
       If the memory region has already been trimmed, allocate only a
       small extension.  This comes up when per-instantiation needed flag
       entries are added to a function after it has been trimmed. */
    a_boolean small_extension = hdr->trimmed;
    if (!small_extension) trim_mem_block(hdr);
    hdr = alloc_mem_block(region_number, size + HOST_ALIGNMENT_REQUIRED,
                          (char *)NULL, small_extension);
  }  /* if */

  /* Take the required space out of the current block. */
  temp_ptr = hdr->next_avail_in_block;
  hdr->next_avail_in_block += size;

#if DEBUG
  /* Track total allocation. */
  total_mem_used += size;
  num_alignment_bytes_allocated += (size - orig_size);
  /* Can't do this conditionally on db_active since db_active is not yet
     set when command line processing is done. */
  allocated_in_region[region_number] += size;
#endif /* DEBUG */
#ifdef TRACE_ALLOC
  trace_alloc_check(temp_ptr);
#endif /* TRACE_ALLOC */
  return temp_ptr;
}  /* alloc_in_region */


#ifdef FFE

char *alloc_pufe(sizeof_t size)
/*
Allocate and return "size" bytes of storage that will last through
compilation of one subprogram in the front end.
*/
{
  /* At the moment, this does the same thing as alloc_fe. */
  return (alloc_in_region(NULL_region_number, size));
}  /* alloc_pufe */

#endif /* ifdef FFE */

char *alloc_general(sizeof_t size)
/*
Allocate and return "size" bytes of general storage.  This differs from
alloc_fe in that the storage will last through execution of the back end
if the back end is executed in the same program.
*/
{
  char *ptr = malloc_with_check(size);
#if DEBUG
  total_general_mem_allocated += size;
#endif /* DEBUG */
  return ptr;
}  /* alloc_general */


#if !DEBUG
/*ARGSUSED*/ /* <-- size is not used if !DEBUG. */
#endif /* DEBUG */
void free_general(a_void_ptr	ptr,
                  sizeof_t	size)
/*
Free a block of memory to general storage.
*/
{
  free((char*)ptr);
#if DEBUG
  total_general_mem_allocated -= size;
#endif /* DEBUG */
}  /* free_general */


char *realloc_general(char     *old_ptr,
                      sizeof_t old_size,
                      sizeof_t new_size)
/*
Reallocate the area pointed to by old_ptr, which currently has size old_size,
so that it will have size new_size.  Return a pointer to the new area.
The old space must have been allocated in general storage (by alloc_general
or realloc_general).  If old_ptr == NULL, this routine acts like
alloc_general.
*/
{
  char *ptr = realloc_with_check(old_ptr, old_size, new_size);
#if DEBUG
  total_general_mem_allocated -= old_size;
  total_general_mem_allocated += new_size;
#endif /* DEBUG */
  return ptr;
}  /* realloc_general */


void free_memory_region(a_memory_region_number region_number)
/*
Free the space for the entire memory region indicated by region_number.
This is presumably being done because the associated information is no longer
needed (e.g., it has been written out to the IL file).
*/
{
  a_mem_block_header_ptr hdr, next_hdr;

  db_enter(5, "free_memory_region");
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "free_memory_region: region %lu, size = %lu\n",
                     (unsigned long)region_number,
                     (unsigned long)allocated_in_region[region_number]);
  }  /* if */
#endif /* DEBUG */
  /* Traverse the list of blocks and free each one. */
  for (hdr = mem_region_table[region_number]; hdr != NULL;) {
    next_hdr = hdr->next;
    free_mem_block(hdr);
    hdr = next_hdr;
  }  /* for */
  mem_region_table[region_number] = NULL;
  il_header.region_scope_entry[region_number] = NULL;
  db_exit();
}  /* free_memory_region */


void free_all_memory_regions(void)
/*
Free all of the memory regions that have been used, including the front
end memory region.
*/
{
  a_memory_region_number region_number;
  for (region_number = highest_used_region_number;
       region_number != NULL_region_number;
       region_number--) {
    free_memory_region(region_number);
  }  /* for */
  /* Free the front end memory region. */
  free_memory_region(NULL_region_number);
}  /* free_all_memory_regions */


void trim_memory_region(a_memory_region_number region_number)
/*
Trim the current (last) block of the indicated memory region to free
any unused space.
*/
{
  trim_mem_block(mem_region_table[region_number]);
}  /* trim_memory_region */


void check_for_done_with_memory_region(a_memory_region_number region_number)
/*
We're done creating the indicated memory region in the front end.  Determine
whether the front end has any further use for it and/or whether it has to be
kept around in order possibly to be written to a PCH file.  If either is
TRUE, the memory region can be trimmed (since it in any case is not going to
grow any larger), but it must be kept around.  If not, it may be possible to
dispose of it, depending on whether the IL is passed to the back end in
memory or with an IL file.
*/
{
  a_boolean      keep_memory;
#if !STANDALONE_UTILITY_PROGRAM
  a_scope_ptr    scope;
  a_routine_ptr  rout;
#endif /* !STANDALONE_UTILITY_PROGRAM */

  db_enter(5, "check_for_done_with_memory_region");
#if DEBUG
  if (debug_level >= 1) {
    fprintf(f_debug,
            "check_for_done_with_memory_region: region %lu, size = %lu\n",
            (unsigned long)region_number,
            (unsigned long)allocated_in_region[region_number]);
  }  /* if */
#endif /* DEBUG */
#if STANDALONE_UTILITY_PROGRAM
  /* In a standalone program the memory is always freed. */
  keep_memory = FALSE;
#else /* !STANDALONE_UTILITY_PROGRAM */
  scope = il_header.region_scope_entry[region_number];
  check_assertion(scope != NULL);
  rout = (scope->kind == (a_scope_kind)sck_function) ?
                                      scope->variant.routine.ptr : NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode && rout != NULL &&
      (rout->decl_modifiers & DM_DLLIMPORT)) {
    /* __declspec(dllimport) functions are deallocated elsewhere. */
    keep_memory = TRUE;
  } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Do not insert code here. */
  if (rout != NULL && rout->is_trivial_default_constructor) {
    /* Always free the memory for the generated definition of a trivial
       default constructor.  It is an incidental byproduct of front-end
       processing (to detect some constraint violations) and is never needed
       by the back end. */
    keep_memory = FALSE;
  } else {
#if !IL_SHOULD_BE_WRITTEN_TO_FILE
    /* The IL is passed to the back end in memory, so it is always kept. */
    keep_memory = TRUE;
#else /* IL_SHOULD_BE_WRITTEN_TO_FILE */
    /* Communication with the back end is via a file.  The memory
       is freed after it's been written to the IL file. */
    keep_memory = FALSE;
    if (may_be_building_new_pch()) {
      /* We are still considering whether to build a PCH file, so keep this
         region around so we can use it in generating the PCH file.
         check_for_done_with_memory_region will be called again once we've
         written the PCH or decided not to write one.  We can still trim the
         unused portion of the memory block at this time, though. */
      keep_memory = TRUE;
#if MINIMAL_INLINING
    } else if (inlining_enabled && rout != NULL && rout->is_inline) {
      /* Keep the region for an inline function so it can be used to
         do inlining. */
      keep_memory = TRUE;
#endif /* MINIMAL_INLINING */
#if ONE_INSTANTIATION_PER_OBJECT
    } else if (one_instantiation_per_object &&
               rout != NULL && rout->is_inline) {
      /* In one-instantiation-per-object mode, keep an inline function
         around so that its body can be swept for each instantiation that
         needs it. */
      keep_memory = TRUE;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if MAINTAIN_NEEDED_FLAGS
    } else if (rout != NULL &&
               (!rout->keep_definition_in_il || !rout->definition_needed)) {
      /* This memory region so far looks as if it's unneeded.  Hold on
         to it for now.  If we make it to the end of the compilation with
         the memory region still unneeded, we will have the option of
         freeing it at that point. */
      keep_memory = TRUE;
#endif /* MAINTAIN_NEEDED_FLAGS */
    }  /* if */
#if DEBUG
    if (rout != NULL && (debug_level >= 3 || db_flag_is_set("needed_flags"))) {
      fprintf(f_debug, "check_for_done_with_memory_region: ");
      fprintf(f_debug, "%s memory region for ",
              keep_memory ? "keeping" : "writing/freeing");
      db_name(&rout->source_corresp);
      fprintf(f_debug, "\n");
    }  /* if */
#endif /* DEBUG */
    if (!keep_memory) {
      /* Write the region to the file and free it. */
      write_memory_region(region_number);
    }  /* if */
#endif /* !IL_SHOULD_BE_WRITTEN_TO_FILE */
  }  /* if */
#endif /* !STANDALONE_UTILITY_PROGRAM */
  if (keep_memory) {
    /* Keep the memory for the region.  Trim the region to reclaim unused
       storage at the end of the last block.  Unused storage at the ends
       of blocks other than the last was previously reclaimed. */
    trim_memory_region(region_number);
  } else {
    /* Free the memory for the region. */
    free_memory_region(region_number);
  }  /* if */
  db_exit();
}  /* check_for_done_with_memory_region */

#if !STANDALONE_UTILITY_PROGRAM

void check_for_done_with_all_function_memory_regions(void)
/*
This routine is called at the end of the compilation to write out
function memory regions that were not previously written out.
Such routines remain in the IL tree (a) if unneeded entities are not
being eliminated, (b) if they are marked with keep_definition_in_il but
not definition_needed, or (c) if they are inline.
*/
{
  db_enter(5, "check_for_done_with_all_function_memory_regions");
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  {
  /* Loop through the memory regions.  Skip the front end and file scope
     memory regions. */
  a_memory_region_number  n = FILE_SCOPE_REGION_NUMBER + 1;
  for (; n <= highest_used_region_number; ++n) {
    if (mem_region_table[n] == NULL) {
      /* This memory has already been freed. */
    } else {
      a_scope_ptr   sp = il_header.region_scope_entry[n];
      /* Skip the file scope memory regions of secondary translation units. */
      if (sp->kind != (a_scope_kind)sck_file) {
        a_routine_ptr rout;
        check_assertion(sp->kind == (a_scope_kind)sck_function);
        rout = sp->variant.routine.ptr;
        check_assertion_str2(!rout->is_trivial_default_constructor,
                           "check_for_done_with_all_function_memory_regions:",
                           "trivial default constructor");
#if DEBUG
        if (debug_level >= 3 || db_flag_is_set("needed_flags")) {
          fprintf(f_debug,
                  "check_for_done_with_all_function_memory_regions: ");
          fprintf(f_debug, "writing/freeing memory region for ");
          db_name(&rout->source_corresp);
          fprintf(f_debug, "\n");
        }  /* if */
#endif /* DEBUG */
        write_memory_region(n);
        free_memory_region(n);
      }  /* if */
    }  /* if */
  }  /* for */
  }
#endif /* !IL_SHOULD_BE_WRITTEN_TO_FILE */
  db_exit();
}  /* check_for_done_with_all_function_memory_regions */

#endif /* !STANDALONE_UTILITY_PROGRAM */

#if DEBUG
#if !STANDALONE_UTILITY_PROGRAM
void show_mem_manage_space_used(unsigned long total_accounted_for)
/*
Display the total amounts of memory used, for debug purposes.
total_accounted_for is the amount of space that is accounted for by
usage counts in other files.
*/
{
  a_memory_region_number region_number;
  a_mem_block_header_ptr hdr;
  unsigned long          total_used, total_unallocated = 0;
  unsigned long		 total_in_freed_blocks = 0;

  fprintf(f_debug, "\nAllocated space in all categories:\n");
  fprintf(f_debug, "%25s %8s %8s %8lu\n", "Total of above", "", "",
                   total_accounted_for);
  fprintf(f_debug, "%25s %8s %8s %8lu\n", "Skipped for alignment", "", "",
                   num_alignment_bytes_allocated);
#if USE_MMAP_FOR_MEMORY_REGIONS
  fprintf(f_debug, "%25s %8s %8s %8lu\n", "File mapped memory", "", "",
                   num_mapped_bytes_allocated);
  fprintf(f_debug, "%25s %8s %8s %8lu (included in previous line)\n",
          "Mapped from PCH", "", "", num_mapped_bytes_from_pch);
  fprintf(f_debug, "%25s %8s %8s %8ld\n",
          "Mapped IL file size", "", "", mmap_file_offset);
#else /* !USE_MMAP_FOR_MEMORY_REGIONS */
  if (precompiled_header_processing_required) {
    fprintf(f_debug, "%25s %8s %8s %8lu\n",
            "Preallocated PCH memory", "", "", (unsigned long)pch_mem_size);
  }  /* if */
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
  total_accounted_for += num_alignment_bytes_allocated;
  /* total_mem_used only counts space allocated in memory regions, so
     it does not include what's in total_general_mem_allocated. */
  total_used = total_mem_used + total_general_mem_allocated;
  fprintf(f_debug, "%25s %8s %8s %8lu\n", "Not listed", "", "",
                   total_used - total_accounted_for);
  fprintf(f_debug, "%25s %8s %8s %8lu\n", "Total used", "", "",
                   total_used);
  /* Size the unallocated parts of memory blocks. */
  for (region_number = NULL_region_number;
       region_number <= highest_used_region_number;
       region_number++) {
    for (hdr = mem_region_table[region_number]; hdr != NULL; hdr = hdr->next) {
      total_unallocated += hdr->after_end_of_block - hdr->next_avail_in_block;
    }  /* for */
  }  /* for */
  fprintf(f_debug, "%25s %8s %8s %8lu\n", "Avail in used mem blocks", "", "",
                   total_unallocated);
  /* Size the memory blocks on the available list. */
  for (hdr = reusable_blocks_list; hdr != NULL; hdr = hdr->next) {
    total_in_freed_blocks += hdr->after_end_of_block -
                                                      hdr->next_avail_in_block;
  }  /* for */
  fprintf(f_debug, "%25s %8s %8s %8lu\n", "Avail in freed mem blocks", "", "",
                   total_in_freed_blocks);
  fprintf(f_debug, "%25s %8s %8s %8lu\n", "Max mem alloc", "", "",
                   max_mem_allocated);
}  /* show_mem_manage_space_used */
#endif /* !STANDALONE_UTILITY_PROGRAM */
#endif /* DEBUG */


void mem_manage_one_time_init(void)
/*
Do one-time initialization of variables related to the mem_manage routines.
*/
{
  okay_to_free_mem_blocks = TRUE;
#if !STANDALONE_UTILITY_PROGRAM
  /* Save variables from mem_manage.h and mem_manage.c that are needed for
     precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
    /* highest_used_region_number is saved directly in the PCH file, and
       therefore doesn't need to be saved here. */
#if DEBUG
      pch_saved_var_array_elem(total_mem_used),
      pch_saved_var_array_elem(num_alignment_bytes_allocated),
#endif /* DEBUG */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
#if USE_MMAP_FOR_MEMORY_REGIONS
  /* When doing precompiled header processing, we allocate memory blocks
     in mapped memory, which cannot be freed. */
  okay_to_free_mem_blocks = !precompiled_header_processing_required;
  mem_alloc_history = NULL;
  mmap_initialized = FALSE;
  mmap_size_allocated = 0;
  mmap_file_offset = 0;
#else /* !USE_MMAP_FOR_MEMORY_REGIONS */
  exhausted_preallocated_memory = FALSE;
  large_mem_block_needed = FALSE;
  total_mem_blocks_allocated = 0;
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
  num_of_mem_alloc_history_entries = 0;
  size_of_mem_alloc_history = 0;
  mem_alloc_history_entries_used = 0;
#endif /* !STANDALONE_UTILITY_PROGRAM */
#if DEBUG
  allocated_in_region = NULL;
  size_of_allocated_in_region = 0;
  total_mem_allocated = 0;
  max_mem_allocated = 0;
  total_general_mem_allocated = 0;
#if USE_MMAP_FOR_MEMORY_REGIONS
  num_mapped_bytes_allocated = 0;
  num_mapped_bytes_from_pch = 0;
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
#endif /* DEBUG */
#ifdef USING_PURIFY
  /* Call the Purify runtime routine to determine whether this executable
     has been processed using Purify. */
  purify_is_active = purify_is_running();
#endif /* USING_PURIFY */
  reusable_blocks_list = NULL;
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  index_for_il_file = NULL;
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
  mem_region_table = NULL;
  size_of_mem_region_table = 0;
  /* Register variables that must be saved and restored when switching
     between translation units. */
  register_trans_unit_variable(file_scope_region_number);
}  /* mem_manage_one_time_init */


void mem_manage_trans_unit_init(void)
/*
Initialize static variables related to the mem_manage routines that
must be initialized for each translation unit.
*/
{
  if (!is_primary_translation_unit) {
    /* For secondary translation units, continue numbering memory regions
       from where we left off.  The file scope memory region is the next
       available region. */
    file_scope_region_number = highest_used_region_number+1;
    init_memory_region(file_scope_region_number, (sizeof_t)0);
  }  /* if */
}  /* mem_manage_trans_unit_init */


void mem_manage_init(void)
/*
Initialize static variables related to the mem_manage routines that
must be initialized for each compilation.
*/
{
  highest_used_region_number = NULL_region_number;
  file_scope_region_number = FILE_SCOPE_REGION_NUMBER;
#if DEBUG
  total_mem_used = 0;
  num_alignment_bytes_allocated = 0;
#endif /* DEBUG */
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  if (index_for_il_file !=  NULL) {
    /* This pointer will be non-NULL on all but the first compilation when
       multiple compilations are being processed.  Clear the array of
       values left over from a previous compilation. */
    memzero((char *)index_for_il_file,
            size_t_arg((size_of_mem_region_table)*sizeof(a_file_position)));
  }  /* if */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
  /* Initialize the memory region for general front end storage. */
  init_memory_region(NULL_region_number, (sizeof_t)0);
  /* Initialize the memory region for file scope IL information. */
  init_memory_region(FILE_SCOPE_REGION_NUMBER, (sizeof_t)0);
}  /* mem_manage_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
