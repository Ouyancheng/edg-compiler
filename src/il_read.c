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

il_read.c -- Read the intermediate language.

*/

#include "basics.h"
#include "host_envir.h"

/* Everything in this file has to do with reading the IL file. */
#if IL_SHOULD_BE_WRITTEN_TO_FILE
/* This code is needed if calling the back end directly in the same program
   as the front end, or if this is the compilation of a standalone utility
   program (which has to read an IL file). */
#if BACK_END_SHOULD_BE_CALLED || STANDALONE_UTILITY_PROGRAM

#if !ORPHAN_PROCESSING_NEEDED
??=error -- ORPHAN_PROCESSING_NEEDED must be set if IL reading is needed.
#endif /* !ORPHAN_PROCESSING_NEEDED */

#include "il_file.h"
#include "il_read.h"
#include "il.h"
#include "il_walk.h"
#include "error.h"
#include "version.h"
#include "mem_manage.h"

#ifdef __CENTERLINE__
extern int centerline_untype(void *, unsigned int);
#endif /* ifdef __CENTERLINE__ */


static FILE	*f_il_input;
			/* Intermediate language file. */

static a_boolean
		reading_file_scope_il;
			/* TRUE if reading IL for the file scope, FALSE if
			   reading IL for a function scope. */
#if ALTERNATE_IL_FILE_FORMAT
static an_il_entry_number
		entry_count_array[(unsigned int)iek_last],
		fs_entry_count_array[(unsigned int)iek_last];
			/* Array giving, for each IL entry kind, the number
			   of entries of that kind.  For string entries, the
			   number is the total size of strings of that kind.
			   The "fs_" array is for the file scope, the other is
			   for a function scope. */
#if CHECKING && DEBUG
static a_byte	*entry_read_array[(unsigned int)iek_last];
			/* Array of pointers, for each IL entry kind,
			   pointing to a boolean array denoting which
			   entries of that kind have been read. */

/*
Macro to set an element of the entry_read_array to 1.  kind is the entry
kind, index is the entry number.
*/
#define set_entry_read_array(kind, index)                             \
 (entry_read_array[kind][(index) / CHAR_BIT] |= (1 << ((index) % CHAR_BIT)))
/*
Macro to extract an element of the entry_read_array.  kind is the entry kind,
index is the entry number.
*/
#define get_entry_read_array(kind, index)                             \
 ((entry_read_array[kind][(index) / CHAR_BIT] & (1 << ((index) % CHAR_BIT))) \
  != 0)

#endif /* CHECKING && DEBUG */
#else /* !ALTERNATE_IL_FILE_FORMAT */

typedef struct a_block_remap_entry *a_block_remap_entry_ptr;
typedef struct a_block_remap_entry {
  /* Entry that indicates the old and new addresses for a memory block,
     used in remapping old addresses to new addresses. */
  a_block_remap_entry_ptr
		next;
			/* Pointer to the next entry, or NULL if last. */
  char		*old_start_addr,
		*old_after_end_addr,
		*new_start_addr;
} a_block_remap_entry;

static a_block_remap_entry_ptr
		block_remap_list,
		fs_block_remap_list;
			/* List of entries that give the old and new
			   addresses for memory blocks.  block_remap_list
			   gives the remapping for the current memory
			   region, fs_block_remap_list for the file scope
			   memory region (or NULL while reading the file
			   scope memory region). */
#endif /* ALTERNATE_IL_FILE_FORMAT */


/*
Interface to fread.  Read "size" bytes from f_il_input and put them at
"*ptr".  If the read is unsuccessful, generate a catastrophic error.
*/
/* Macro to inform CenterLine's environment that the area has been
   reused, to avoid wrong-type errors. */
#ifdef __CENTERLINE__
#define conditional_centerline_untype(ptr, size)                      \
  (void)centerline_untype((void *)(ptr), (unsigned int)(size))
#else /* !defined(__CENTERLINE__) */
#define conditional_centerline_untype(ptr, size) /* Nothing */
#endif /* ifdef __CENTERLINE__ */
#define fread_with_check(ptr, size)                                   \
{ if (fread((ptr), size_t_arg(size), 1, f_il_input) != 1) {           \
    catastrophe(ec_bad_il_file);                                      \
  }  /* if */                                                         \
  conditional_centerline_untype(ptr, size);                           \
}  /* fread_with_check */

#if ALTERNATE_IL_FILE_FORMAT

/*
Helper macro to do the CHECKING code for m_remap_entry_number.
*/
#if CHECKING
#define non_fs_remap_checking()                                       \
{ if (reading_file_scope_il) {                                        \
    /* There shouldn't be any function-scope numbers in the file scope. */ \
    internal_error("non_fs_remap_checking: func number in file scope"); \
  }  /* if */                                                         \
}  /* non_fs_remap_checking */
#else /* !CHECKING */
#define non_fs_remap_checking() /* Nothing */
#endif /* CHECKING */

/*
Macro to remap an entry number (entry_number), from the file scope if
is_in_file_scope is TRUE, of kind entry_kind, and to return a pointer
to the entry in ptr (a "char *" pointer).  This is a macro to avoid
a few levels of subroutine call, which is necessary because this gets
used a lot.
*/
#define m_remap_entry_number()                                        \
{ char               **entry_array_base_array_ptr;                    \
  sizeof_t           gross_entry_size, prefix_size;                   \
  if (is_in_file_scope) {                                             \
    /* This is a file-scope entry number. */                          \
    gross_entry_size = fs_entry_length_with_prefix[(int)entry_kind];  \
    prefix_size = fs_length_of_entry_prefix[(int)entry_kind];         \
    entry_array_base_array_ptr = fs_entry_array_base_array;           \
  } else {                                                            \
    /* This is a function-scope entry number. */                      \
    gross_entry_size = entry_length_with_prefix[(int)entry_kind];     \
    prefix_size = length_of_entry_prefix[(int)entry_kind];            \
    non_fs_remap_checking();                                          \
    entry_array_base_array_ptr = entry_array_base_array;              \
  }  /* if */                                                         \
  /* Compute the entry address by multiplying the entry number minus  \
     one by the size of the entry, and adding the base address of     \
     the array of entries of that kind. */                            \
  ptr = entry_array_base_array_ptr[(int)entry_kind] +                 \
                                 (entry_number-1) * gross_entry_size; \
  /* Move past the prefix and orphan pointer to the entry proper. */  \
  ptr += prefix_size;                                                 \
}  /* m_remap_entry_number */

#endif /* ALTERNATE_IL_FILE_FORMAT */
#if ALTERNATE_IL_FILE_FORMAT

static char *remap_entry_number_to_ptr(an_il_entry_number entry_number,
                                       a_boolean          is_in_file_scope,
                                       an_il_entry_kind   entry_kind)
/*
Return a pointer to the IL entry of kind entry_kind whose entry number
is entry_number.  The entry number is for the file scope if is_in_file_scope
is TRUE.
*/
{
  char *ptr;

  /* The macro does the real remapping work. */
  m_remap_entry_number();
  return ptr;
}  /* remap_entry_number_to_ptr */

#endif /* ALTERNATE_IL_FILE_FORMAT */
#if ALTERNATE_IL_FILE_FORMAT

static char *remap_encoded_number_to_ptr(
                                        an_encoded_entry_number encoded_number,
                                        an_il_entry_kind        entry_kind)
/*
Return a pointer to the IL entry of kind entry_kind whose encoded entry
number is encoded_number.
*/
{
  char               *ptr;
  a_boolean	     is_in_file_scope;
  an_il_entry_number entry_number;

  if (encoded_number == 0) {
    /* A zero encoded entry number means a NULL pointer. */
    ptr = NULL;
  } else {
    /* Function-scope entry numbers have a tag bit turned on. */
    is_in_file_scope = ((encoded_number & FUNC_ENTRY_NUMBER_BIT) == 0);
    entry_number = encoded_number & ~FUNC_ENTRY_NUMBER_BIT;
    /* The macro does the real remapping work. */
    m_remap_entry_number();
  }  /* if */
  return ptr;
}  /* remap_encoded_number_to_ptr */

#endif /* ALTERNATE_IL_FILE_FORMAT */
#if ALTERNATE_IL_FILE_FORMAT

static char *remap_ptr_to_ptr(char             *old_ptr,
                              an_il_entry_kind entry_kind)
/*
Remap a pointer in entry number form to a real pointer.  The entry pointed
to has kind entry_kind.
*/
{
  char                    *ptr;
  a_boolean               is_in_file_scope;
  an_il_entry_number      entry_number;
  an_encoded_entry_number encoded_number = (an_encoded_entry_number)old_ptr;

  if (encoded_number == 0) {
    /* A zero encoded entry number means a NULL pointer. */
    ptr = NULL;
  } else {
    /* Function-scope entry numbers have a tag bit turned on. */
    is_in_file_scope = ((encoded_number & FUNC_ENTRY_NUMBER_BIT) == 0);
    entry_number = encoded_number & ~FUNC_ENTRY_NUMBER_BIT;
    /* The macro does the real remapping work. */
    m_remap_entry_number();
  }  /* if */
  return ptr;
}  /* remap_ptr_to_ptr */

#endif /* ALTERNATE_IL_FILE_FORMAT */
#if !ALTERNATE_IL_FILE_FORMAT

/*ARGSUSED*/  /* <--- entry_kind is not used. */
static char *ptr_remap_function(char             *old_ptr,
                                an_il_entry_kind entry_kind)
/*
Given an old-value pointer, return the new value for it.  This is called
to convert pointers written out at one location and read back in at
another to the proper new value.
*/
{
  char                    *new_ptr;
  a_block_remap_entry_ptr brep, prev_brep;

  /* A NULL pointer stays a NULL pointer. */
  if (old_ptr == NULL) {
    new_ptr = NULL;
  } else {
    /* Find the entry in the block_remap_list or fs_block_remap_list whose
       range includes old_ptr. */
    for (prev_brep = NULL, brep = block_remap_list;
         brep != NULL;
         prev_brep = brep, brep = brep->next) {
       /* See if the address falls within the address range for the block. */
      if (ptr_in_range(old_ptr, brep->old_start_addr,
                                brep->old_after_end_addr)) {
        /* Found the entry.  Move it to the front of the list, so that
           frequently-used entries will be found quickly. */
        if (prev_brep != NULL) {
          prev_brep->next = brep->next;
          brep->next = block_remap_list;
          block_remap_list = brep;
        }  /* if */
        goto found_entry;
      }  /* if */
    }  /* for */
    for (prev_brep = NULL, brep = fs_block_remap_list;
         brep != NULL;
         prev_brep = brep, brep = brep->next) {
      if (ptr_in_range(old_ptr, brep->old_start_addr,
                                brep->old_after_end_addr)) {
        /* Found the entry.  Move it to the front of the list, so that
           frequently-used entries will be found quickly. */
        if (prev_brep != NULL) {
          prev_brep->next = brep->next;
          brep->next = fs_block_remap_list;
          fs_block_remap_list = brep;
        }  /* if */
        goto found_entry;
      }  /* if */
    }  /* for */
#if CHECKING
    /* The address was not found in any of the entries, so it points outside
       of all known memory regions. */
    internal_error("ptr_remap_function: address not in any mem block");
#endif /* CHECKING */
found_entry:
    /* Determine the new address. */
    new_ptr = brep->new_start_addr + (old_ptr - brep->old_start_addr);
  }  /* if */
  return (new_ptr);     
}  /* ptr_remap_function */
#endif /* !ALTERNATE_IL_FILE_FORMAT */


void read_memory_region(a_memory_region_number region_number)
/*
Read the indicated memory region from the IL file, and do whatever is
necessary to make it directly accessible in memory.
*/
{
  a_memory_region_number    temp_region_number;
#if ALTERNATE_IL_FILE_FORMAT
  an_il_entry_number        *entry_count_array_ptr;
  char                      **entry_array_base_array_ptr;
  a_byte                    byte_entry_kind;
  an_il_entry_kind          entry_kind;
  an_il_entry_number        entry_number;
  sizeof_t                  entry_length;
  char                      *entry_ptr;
  a_boolean                 is_string_entry;
#if CHECKING
  an_il_entry_number        count_of_entries_read[(unsigned int)iek_last];
  an_il_entry_number        trimmed_entry_number;
#endif /* CHECKING */
#else /* !ALTERNATE_IL_FILE_FORMAT */
  a_mem_block_header_ptr    old_hdr;
  a_scope_ptr               old_region_scope_entry;
  sizeof_t                  total_bytes;
  a_block_remap_entry_ptr   remap_entry;
  a_boolean                 first_block;
#endif /* ALTERNATE_IL_FILE_FORMAT */

  db_enter(2, "read_memory_region");
#if DEBUG
  if (debug_level >= 2) {
    fprintf(f_debug, "Reading IL memory region %ld\n", (long)region_number);
  }  /* if */
#endif /* DEBUG */
#if CHECKING
  if (region_number <= 0 ||
      region_number > highest_used_region_number) {
    internal_error("read_memory_region: bad region number");
  }  /* if */
#endif /* CHECKING */
  reading_file_scope_il = (region_number == FILE_SCOPE_REGION_NUMBER);
  /* Position the file at the start of the data for the region, as
     indicated by the file index. */
  if (fseek(f_il_input, index_for_il_file[region_number], SEEK_SET) != 0) {
    catastrophe(ec_bad_il_file);
  }  /* if */
  /* Read the region number to verify that we're in the right place. */
  fread_with_check((char *)&temp_region_number, sizeof(temp_region_number));
  if (temp_region_number != region_number) {
    catastrophe(ec_bad_il_file);
  }  /* if */
#if ALTERNATE_IL_FILE_FORMAT
  /* Alternate IL file format.  The information on the region is
       region number
       array giving count of entries required for each entry kind
       for each entry----|entry kind
                         |entry number
                         |entry length, only for string entries
                         |orphaned IL entry link (file scope only)
                         |the entry itself
       zero entry kind, indicating the end of the list of entries
  */
  /* Read the array of entry counts.  Use either the file-scope array or
     the function-scope array depending on the region we are reading. */
  if (reading_file_scope_il) {
    entry_count_array_ptr = fs_entry_count_array;
    entry_array_base_array_ptr = fs_entry_array_base_array;
  } else {
    entry_count_array_ptr = entry_count_array;
    entry_array_base_array_ptr = entry_array_base_array;
  }  /* if */
  /* The first entry of the array is skipped. */
  fread_with_check((char *)(&entry_count_array_ptr[1]),
                   sizeof(entry_count_array)-sizeof(an_il_entry_number));
  /* Allocate the space needed for the indicated number of entries.
     The space for all entries of a given kind is allocated contiguously,
     so it's in effect an array of entries of that kind. */
  init_memory_region(region_number, (sizeof_t)0);
  for (byte_entry_kind = 1+(unsigned int)iek_none;
       byte_entry_kind < (unsigned int)iek_last;
       byte_entry_kind++) {
    sizeof_t gross_entry_size;

    if (reading_file_scope_il) {
      gross_entry_size = fs_entry_length_with_prefix[byte_entry_kind];
    } else {
      gross_entry_size = entry_length_with_prefix[byte_entry_kind];
    }  /* if */
    entry_array_base_array_ptr[byte_entry_kind] =
             alloc_in_region(region_number,
                             (sizeof_t)(entry_count_array_ptr[byte_entry_kind]*
                                        gross_entry_size));
#if CHECKING && DEBUG
    /* Allocate a boolean array to track reading each IL entry of this kind
       into this region. */
    {
      sizeof_t index, byte_count;
      /* Allocate one bit per entry. */
      byte_count = (entry_count_array_ptr[byte_entry_kind] / CHAR_BIT) + 1;
      entry_read_array[byte_entry_kind] = (a_byte *)
                                    alloc_in_region(region_number, byte_count);
      /* Clear the array to zero, indicating that no entries have been
         read. */
      for (index = 0; index < byte_count; index++) {
        entry_read_array[byte_entry_kind][index] = 0;
      }  /* for */
    }
#endif /* CHECKING && DEBUG */
  }  /* for */
  /* Establish the remapping function (from entry number to pointer) to
     be used by the IL walk routines. */
  walk_remap_func = remap_ptr_to_ptr;
  if (reading_file_scope_il) {
    /* In the alternate file format, the IL tree is not walked on the reading
       end -- each IL entry's pointers are remapped as the entry is read.
       Therefore header pointers (like these) are remapped as a separate
       step here. */
    remap_il_header_pointers();
    remap_first_ptr_of_orphaned_file_scope_entry_array();
    remap_last_ptr_of_orphaned_file_scope_entry_array();
  }  /* if */
  /* Remember the location of the primary scope entry.  It's the LAST scope
     entry, because local scopes (prototype scopes, block scopes) get processed
     first in the IL walk. */
  il_header.region_scope_entry[region_number] = (a_scope_ptr)
      remap_entry_number_to_ptr(entry_count_array_ptr[(unsigned int)iek_scope],
                                reading_file_scope_il,
                                iek_scope);
#if CHECKING
  /* Zero the array used to count entries as they are read, to check that
     all of them are read.  Note that the [0] entry is zeroed just to make
     debug output look pretty. */
  for (byte_entry_kind = (unsigned int)iek_none;
       byte_entry_kind < (unsigned int)iek_last;
       byte_entry_kind++) {
    count_of_entries_read[byte_entry_kind] = 0;
  }  /* for */
#endif /* CHECKING */
  /* Read the entries and place each one at the right location. */
  for (;;) {
    fread_with_check((char *)&byte_entry_kind, 1);
    /* An entry kind of zero indicates the end of the list of entries. */
    if (byte_entry_kind == 0) break;
    entry_kind = (an_il_entry_kind)byte_entry_kind;
    /* Read the entry number. */
    fread_with_check((char *)&entry_number, sizeof(entry_number));
#if DEBUG
    if (debug_level >= 5) {
      fprintf(f_debug, "Read IL entry from file: kind = %u, number = %lu\n",
              byte_entry_kind, (unsigned long)entry_number);
    }  /* if */
#endif /* DEBUG */
#if CHECKING
    if (byte_entry_kind <= (unsigned int)iek_none ||
        byte_entry_kind >= (unsigned int)iek_last) {
      internal_error("read_memory_region: bad entry kind");
    }  /* if */
    /* Remove the tag bit from a function scope entry number in order
       to check it. */
    if (reading_file_scope_il != 
        ((entry_number & FUNC_ENTRY_NUMBER_BIT) == 0)) {
      internal_error(
                   "read_memory_region: func scope bit wrong in entry number");
    }  /* if */
    trimmed_entry_number = entry_number & ~FUNC_ENTRY_NUMBER_BIT;
    if (trimmed_entry_number <= 0 ||
        trimmed_entry_number > entry_count_array_ptr[byte_entry_kind]) {
      internal_error("read_memory_region: bad entry number");
    }  /* if */
#endif /* CHECKING */
    /* Determine the address of the entry within the array of entries of
       that kind. */
    entry_ptr = remap_encoded_number_to_ptr(entry_number, entry_kind);
    /* Set the entry prefix appropriately. */
    clear_il_entry_prefix(&il_entry_prefix_of(entry_ptr),
                          reading_file_scope_il);
    /* If this is a string entry, read the length.  Otherwise, compute the
       length from the entry kind. */
    is_string_entry = is_string_entry_kind(entry_kind);
    if (is_string_entry) {
      fread_with_check((char *)&entry_length, sizeof(entry_length));
#if DEBUG
      if (debug_level >= 5) {
        fprintf(f_debug, "Read IL entry from file: length = %lu\n",
                (unsigned long)entry_length);
      }  /* if */
#endif /* DEBUG */
#if CHECKING
      { sizeof_t gross_entry_size;
        /* Count the entries read to see if we get all of them.  String
           entries are counted by character.  Include the space for
           the prefix. */
        gross_entry_size = entry_length + SPACE_FOR_IL_ENTRY_PREFIX;
        /* Except on the last entry, add space for alignment. */
        if ((trimmed_entry_number + entry_length) <=
                                      entry_count_array_ptr[byte_entry_kind]) {
          do_host_alignment(gross_entry_size);
        }  /* if */
        count_of_entries_read[byte_entry_kind] += gross_entry_size;
#if DEBUG
        { sizeof_t count, index;
          /* Record the space for the string entry as having been read.
             Also mark the space for the prefix (preceding the entry) and
             any space for alignment (following the entry). */
          index = trimmed_entry_number - SPACE_FOR_IL_ENTRY_PREFIX;
          for (count = 0; count < gross_entry_size; count++, index++) {
            set_entry_read_array(byte_entry_kind, index);
          }  /* for */
        }  /* if */
#endif /* DEBUG */
      }
#endif /* CHECKING */
    } else {
      /* Non-string entry. */
      entry_length = sizeof_il_entry[byte_entry_kind];
#if CHECKING
      /* Count the entries read to see if we get all of them. */
      count_of_entries_read[byte_entry_kind]++;
#if DEBUG
      /* Mark this entry as having been read. */
      set_entry_read_array(byte_entry_kind, trimmed_entry_number);
#endif /* DEBUG */
#endif /* CHECKING */
      /* If reading the file scope, read in the orphaned IL entry pointer
         into the area preceding the entry. */
      if (reading_file_scope_il) {
        char               *orphan_ptr;
        an_il_entry_number orphan_number;
        fread_with_check((char *)&orphan_ptr, sizeof(orphan_ptr));
        orphan_number = (an_il_entry_number)orphan_ptr;
        /* Remap the entry number to a pointer immediately. */
        fs_orphan_pointer_of(entry_ptr) =
                                     remap_encoded_number_to_ptr(orphan_number,
                                                                 entry_kind);
      }  /* if */
    }  /* if */
    /* Read the entry into the right spot. */
    fread_with_check(entry_ptr, entry_length);
    if (!is_string_entry) {
      /* Change the pointers in the entry from entry numbers to real
         pointers. */
      remap_pointers_in_il_entry(entry_ptr, entry_kind);
    }  /* if */
  }  /* for */
  /* Zero entry kind indicating end of list has been encountered. */
#if CHECKING
  {
    a_boolean errors = FALSE;

    /* Check that all the entries expected have in fact been read. */
    for (byte_entry_kind = 1+(unsigned int)iek_none;
         byte_entry_kind < (unsigned int)iek_last;
         byte_entry_kind++) {
      if (count_of_entries_read[byte_entry_kind] !=
          entry_count_array_ptr[byte_entry_kind]) {
        /* Not all expected entries were read. */
#if DEBUG
        sizeof_t entry_count, index;

        if (!errors) {
          (void)fprintf(f_debug,
                        "IL entry write-read difference: region number %3ld\n",
                        (long)region_number);
        }  /* if */
        
        (void)fprintf(f_debug,
                 "     entry kind =%3ld (%s), written = %ld, read = %ld\n",
                      (long)byte_entry_kind,
                      il_entry_kind_names[byte_entry_kind],
                      (long)entry_count_array_ptr[byte_entry_kind],
                      (long)count_of_entries_read[byte_entry_kind]);
        entry_count = entry_count_array_ptr[byte_entry_kind];
        if (is_string_entry_kind((an_il_entry_kind)byte_entry_kind)) {
          /* A form of string entry. */
          index = 1;
          while (index <= entry_count) {
            if (get_entry_read_array(byte_entry_kind, index) == FALSE) {
              /* Have found the beginning of a missing string. */
              (void)fprintf(f_debug, "        missing entry = %ld",
                           (long)index);
              /* Skip over the missing bytes of the string.  Note that
                 missing back to back strings will appear in the debug
                 output as a single large string. */
              for (index++;
                   index <= entry_count &&
                         get_entry_read_array(byte_entry_kind, index) == FALSE;
                   index++) {
              }  /* for */
              (void)fprintf(f_debug, " - %ld\n", (long)index-1);
            }  /* if */
            index++;
          } /* while */
        } else {
          for (index = 1; index <= entry_count; index++) {
            if (get_entry_read_array(byte_entry_kind, index) == FALSE) {
              (void)fprintf(f_debug, "        missing entry = %ld\n",
                            (long)index);
            }  /* if */
          }  /* for */
        }  /* if */
           
#endif /* DEBUG */
        errors = TRUE;
      }  /* if */
    }  /* for */
    if (errors) {
      internal_error("read_memory_region: not all expected entries were read");
    }  /* if */
  }
#endif /* CHECKING */
  
#else /* !ALTERNATE_IL_FILE_FORMAT */

  /* Standard IL file format.  The information on the region is
       region number
       memory address that the first block header came from
       memory address of the primary scope entry
       total size in bytes of the information following (all blocks)
       for each block----|header
                         |block itself
  */
  /* Continue reading the region header (old address of first block, old
     address of the primary scope entry, and total size of the region
     blocks). */
  fread_with_check((char *)&old_hdr, sizeof(old_hdr));
  fread_with_check((char *)&old_region_scope_entry, sizeof(a_scope_ptr));
  fread_with_check((char *)&total_bytes, sizeof(total_bytes));
  block_remap_list = NULL;
  if (reading_file_scope_il) fs_block_remap_list = NULL;
  /* Loop for each block of the memory region. */
  first_block = TRUE;
#if DEBUG
  if (debug_level >= 3) {
    fprintf(f_debug, "read_memory_region: at top of block read loop\n");
    fprintf(f_debug, "total_bytes = %lu\n", (unsigned long)total_bytes);
  }  /* if */
#endif /* DEBUG */
  do {
    a_mem_block_header old_block_header;
    sizeof_t           block_size, block_used;
    char               *new_start_of_block;

    /* Read a block.  Start by reading the header. */
    fread_with_check((char *)&old_block_header, sizeof(old_block_header));
#if DEBUG
    if (debug_level >= 4) {
      fprintf(f_debug, "start_of_block = %lx\n",
                       (unsigned long)old_block_header.start_of_block);
      fprintf(f_debug, "next_avail_in_block = %lx\n",
                       (unsigned long)old_block_header.next_avail_in_block);
      fprintf(f_debug, "after_end_of_block = %lx\n",
                       (unsigned long)old_block_header.after_end_of_block);
    }  /* if */
#endif /* DEBUG */
    total_bytes -= sizeof(old_block_header);
    block_size = old_block_header.after_end_of_block -
                 old_block_header.start_of_block;
    block_used = old_block_header.next_avail_in_block -
                 old_block_header.start_of_block;
#if DEBUG
    if (debug_level >= 4) {
      fprintf(f_debug, "block_used  = %lu\n", (unsigned long)block_used);
      fprintf(f_debug, "block_size  = %lu\n", (unsigned long)block_size);
      fprintf(f_debug, "total_bytes = %lu\n", (unsigned long)total_bytes);
    }  /* if */
#endif /* DEBUG */
#if CHECKING
    /* Do some sanity checking. */
    if (block_used > total_bytes || block_used > block_size) {
      internal_error("read_memory_region: bad block header");
    }  /* if */
#endif /* CHECKING */
    if (first_block) {
      /* Create the region on the first block, and make the initial
         allocation at least as large as the first block. */
      init_memory_region(region_number, block_size);
      first_block = FALSE;
    }  /* if */
    /* Allocate the space for the block. */
    new_start_of_block = alloc_in_region(region_number, block_used);
    /* Free any extra space allocated at the end of the block. */
    trim_memory_region(region_number);
    /* Read the block into the allocated space. */
    fread_with_check(new_start_of_block, block_used);
    total_bytes -= block_used;
    /* The memory block was at one address when written out, and is
       probably at a different address now that it has been read in.
       The pointers will have to be remapped.  Build an entry that
       describes the old and new addresses, to be used when doing the
       remapping. */
    remap_entry = (a_block_remap_entry_ptr)
                                    alloc_general(sizeof(a_block_remap_entry));
    remap_entry->next = block_remap_list;
    block_remap_list = remap_entry;
    remap_entry->old_start_addr = old_block_header.start_of_block;
    remap_entry->old_after_end_addr = old_block_header.next_avail_in_block;
    remap_entry->new_start_addr = new_start_of_block;
  } while (total_bytes > 0);
#if DEBUG
  if (debug_level >= 2) {
    fprintf(f_debug, "at end of block read loop\n");
  }  /* if */
  if (debug_level >= 2) {
    /* See how many of the blocks ended up at their original addresses. */
    unsigned long           num_same = 0, num_different = 0;
    a_block_remap_entry_ptr remap_entry;

    for (remap_entry = block_remap_list;
         remap_entry != NULL;
         remap_entry = remap_entry->next) {
      if (remap_entry->old_start_addr == remap_entry->new_start_addr) {
        num_same++;
      } else {
        num_different++;
      }  /* if */
    }  /* for */
    fprintf(f_debug, "Blocks at same/different addresses: %lu/%lu\n",
                     num_same, num_different);
  }  /* if */
#endif /* DEBUG */
  /* Change the address of the primary scope entry to a "new" address. */
  il_header.region_scope_entry[region_number] = (a_scope_ptr)
                             ptr_remap_function((char *)old_region_scope_entry,
                                                iek_scope);
  /* Walk the IL tree for the region and update all pointers,
     changing their old addresses to new addresses. */
  if (reading_file_scope_il) {
    /* The memory region is the file scope region. */
    /* Remap the "last" pointers in the orphaned file scope IL entry table.
       The "first" pointers are left alone for now; they will be remapped
       by the call of walk_orphaned_file_scope_il_entries at the end
       of the file-scope IL walk. */
    walk_remap_func = ptr_remap_function;
    remap_last_ptr_of_orphaned_file_scope_entry_array();
    /* Walk the file scope IL tree. */
    walk_file_scope_il((an_entry_process_function_ptr)NULL,
                       (a_string_entry_process_function_ptr)NULL,
                       ptr_remap_function);
    /* Save the remap list for this region as the file-scope remap list. */
    fs_block_remap_list = block_remap_list;
  } else {
    /* The memory region is a function scope. */
    walk_routine_scope_il(region_number,
                          (an_entry_process_function_ptr)NULL,
                          (a_string_entry_process_function_ptr)NULL,
                          ptr_remap_function);
  }  /* if */
#endif /* ALTERNATE_IL_FILE_FORMAT */

  db_exit();
}  /* read_memory_region */


static void il_read_init(void)
/*
Initialization routine for IL reading.
*/
{
#if ALTERNATE_IL_FILE_FORMAT
  /* Build a table of sizes of IL entries including the prefix. */
  { sizeof_t entry_size, prefix_size, fs_prefix_size;
    int      entry_kind;

    for (entry_kind = 0; entry_kind < (unsigned int)iek_last; entry_kind++) {
      if (is_string_entry_kind((an_il_entry_kind)entry_kind)) {
        /* For string entries the "entry number" is really a byte offset, and
           the "entry size" is 1.  Things like space for the prefix have
           been accounted for in the entry numbers/byte offsets assigned. */
        fs_prefix_size = prefix_size = 0;
      } else {
        /* Non-string entry. */
        prefix_size = SPACE_FOR_IL_ENTRY_PREFIX;
        /* If the entry is in the file scope memory region, it is preceded
           by an orphan pointer. */
        fs_prefix_size = prefix_size + SPACE_FOR_FS_ORPHAN_POINTER;
      }  /* if */
      entry_size = sizeof_il_entry[entry_kind];
      entry_length_with_prefix   [entry_kind] = entry_size + prefix_size;
      fs_entry_length_with_prefix[entry_kind] = entry_size + fs_prefix_size;
      length_of_entry_prefix     [entry_kind] = prefix_size;
      fs_length_of_entry_prefix  [entry_kind] = fs_prefix_size;
    }  /* for */
  }
#endif /* ALTERNATE_IL_FILE_FORMAT */
}  /* il_read_init */


void il_read(FILE *il_file)
/*
Read the file-scope intermediate language from the indicated file and
build the in-memory version.
*/
{
  a_file_position         index_pos, file_scope_pos;
  char                    check_string[LEN_IL_FILE_MAGIC_STRING],
                          magic_string[LEN_IL_FILE_MAGIC_STRING];
  a_memory_region_number  new_size_of_mem_region_table;
  a_scope_ptr             *old_il_header_region_scope_entry;

  db_enter(1, "il_read");

  /* Save the file identifier. */
  f_il_input = il_file;
  /* Initialize. */
  il_read_init();
  /* Call a routine in il.c that will reset any information that will
     be invalidated as a result of reading the IL file. */
  il_reset();

  /* The file layout is as follows:
       magic string that identifies an IL file
       number of regions
       file offset to the file index table
       file offset to the start of the file scope region
       il_header
       orphaned_file_scope_il_entries array
       for each region---|region number
                         |information on the region (see read_memory_region)
       NULL_region_number (0) -- indicating no more regions
       file index table (array giving the file offset for each region)
  */
  /* Seek to the beginning of the file. */
  if (fseek(f_il_input, 0L, SEEK_SET) != 0) {
    catastrophe(ec_bad_il_file);
  }  /* if */
  /* Read and check the magic string. */
  (void)sprintf(magic_string, IL_FILE_MAGIC_STRING, IL_VERSION_NUMBER);
  fread_with_check(check_string, sizeof(check_string));
  if (strcmp(check_string, magic_string) != 0) {
    catastrophe(ec_bad_il_file);
  }  /* if */
  /* Read the number of regions. */
  fread_with_check((char *)&highest_used_region_number,
                   sizeof(highest_used_region_number));
  /* Quick check for bad IL file. */
  if (highest_used_region_number < 1) catastrophe(ec_bad_il_file);
  /* Read the file offset for the file index. */
  fread_with_check((char *)&index_pos, sizeof(index_pos));
  /* Read the position of the file scope memory region (and throw it away). */
  fread_with_check((char *)&file_scope_pos, sizeof(file_scope_pos));
  /* Read the IL header. */
  /* Save the region_scope_entry pointer in case it points to allocated
     storage. */
  old_il_header_region_scope_entry = il_header.region_scope_entry;
  fread_with_check((char *)&il_header, sizeof(il_header));
  il_header.region_scope_entry = old_il_header_region_scope_entry;
#ifdef CIL
  /* Make sure that the signedness of "plain" char is set correctly in
     int_kind_is_signed.  Doing this here ensures that it won't be overlooked
     in standalone utility programs. */
  int_kind_is_signed[(int)ik_char] = il_header.plain_chars_are_signed;
#endif /* ifdef CIL */
  /* Read the orphaned_file_scope_il_entries array. */
  fread_with_check((char *)orphaned_file_scope_il_entries,
                   sizeof(orphaned_file_scope_il_entries));
  /* Allocate index tables of the right size for the number of regions, or
     use them if they are already allocated. */
  new_size_of_mem_region_table = highest_used_region_number+1;
  if (size_of_mem_region_table < new_size_of_mem_region_table) {
    mem_region_table = (a_mem_block_header_ptr *)
                       realloc_general((char *)mem_region_table,
                                       (sizeof_t)(size_of_mem_region_table*
                                              sizeof(a_mem_block_header_ptr)),
                                       (sizeof_t)(new_size_of_mem_region_table*
                                              sizeof(a_mem_block_header_ptr)));
    il_header.region_scope_entry = (a_scope_ptr *)
                       realloc_general((char *)il_header.region_scope_entry,
                                       (sizeof_t)(size_of_mem_region_table*
                                                         sizeof(a_scope_ptr)),
                                       (sizeof_t)(new_size_of_mem_region_table*
                                                         sizeof(a_scope_ptr)));
    index_for_il_file = (a_file_position *)
                       realloc_general((char *)index_for_il_file,
                                       (sizeof_t)(size_of_mem_region_table*
                                                     sizeof(a_file_position)),
                                       (sizeof_t)(new_size_of_mem_region_table*
                                                     sizeof(a_file_position)));
    size_of_mem_region_table = new_size_of_mem_region_table;
  }  /* if */
  /* Clear the index tables. */
  /* Depending on NULL represented as zero bits here. */
  memzero((char *)mem_region_table,
          size_t_arg(size_of_mem_region_table*sizeof(a_mem_block_header_ptr)));
  memzero((char *)il_header.region_scope_entry,
          size_t_arg(size_of_mem_region_table*sizeof(a_scope_ptr)));
  memzero((char *)index_for_il_file,
          size_t_arg(size_of_mem_region_table*sizeof(a_file_position)));
  /* Read the file index. */
  if (fseek(f_il_input, index_pos, SEEK_SET) != 0) {
    catastrophe(ec_bad_il_file);
  }  /* if */
  fread_with_check((char *)&index_for_il_file[FILE_SCOPE_REGION_NUMBER],
                   (sizeof_t)(sizeof(a_file_position)*
                              highest_used_region_number));
  /* Check that we are now at end of file. */
  if (getc(f_il_input) != EOF) {
    catastrophe(ec_bad_il_file);
  }  /* if */
  /* Read in the file-scope region. */
  read_memory_region(FILE_SCOPE_REGION_NUMBER);
  db_exit();
}  /* il_read */

#endif /* BACK_END_SHOULD_BE_CALLED || STANDALONE_UTILITY_PROGRAM */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1991 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
