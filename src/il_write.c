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

il_write.c -- Write the intermediate language to a file.

*/

#include "basics.h"
#include "host_envir.h"

/* Everything in this file has to do with writing the IL file. */
#if IL_SHOULD_BE_WRITTEN_TO_FILE

#include "il_file.h"
#include "il_walk.h"
#include "il_write.h"
#include "il.h"
#include "error.h"
#include "version.h"

static a_boolean
		writing_file_scope_il;
			/* TRUE if writing the file-scope IL, FALSE if writing
			   IL for a function scope. */

#if ALTERNATE_IL_FILE_FORMAT
static an_il_entry_number
		entry_numbers_array[(int)iek_last],
		fs_entry_numbers_array[(int)iek_last];
			/* Arrays giving the number of entries of each
			   kind, for the current scope and the file scope.
			   For string entries, the total size of strings
			   of that kind.  Used to track the number of entries
			   and also to assign entry numbers.  Each table
			   type has entry numbers starting from 1. */
typedef char	*a_char_ptr;
			/* Useful to indicate "char *" as a type in calling
			   chg_pointer. */
#define ENTRY_WRITTEN_TAG (((unsigned long)LONG_MAX>>1)+1)
			/* Bit turned on in the entry numbers preceding
			   IL entries to indicate that the IL entry has
			   been written to the IL file. */
#if CHECKING && DEBUG
#if __CENTERLINE__
/* Centerline debugging variables used to locate a missing (unwritten) IL entry
   by entry kind and entry number within that kind in a specific memory
   region.  By setting the variables 	centerline_memory_region_number,
   centerline_il_entry_kind and centerline_il_entry_number after loading
   il_write.c into the Centerline environment, Centerline will stop
   (centerline_stop()) when the entry number is assigned for the specified
   IL entry.

        1. load il_write.c
        2. stop in assign_entry_number
        3. run the test compilation
        4. when Centerline stops in assign_entry_number:
            a. set the 3 Centerline variable values
            b. remove the stop at the entry of assign_entry_number()
        5. continue

   The contents of the IL entry and its position on the IL tree as shown
   by the stack trace can help to determine the cause of the error.
*/
a_memory_region_number
		centerline_memory_region_number;
			/* Memory region of the omitted IL entry. */
an_il_entry_kind
		centerline_il_entry_kind;
			/* IL entry kind of the omitted IL entry. */
an_il_entry_number
		centerline_il_entry_number;
			/* IL entry number of the omitted IL entry. */
a_memory_region_number
		_centerline_region_number;
			/* Hidden variable used by write_memory_region()
			   to record the memory region number currently
			   being written. */
#endif /* __CENTERLINE__ */
#endif /* CHECKING && DEBUG */
#endif /* ALTERNATE_IL_FILE_FORMAT */


#if ALTERNATE_IL_FILE_FORMAT
#if CHECKING && DEBUG
static void display_il_entry_kind_and_ptr (
				char             *entry_ptr,
				an_il_entry_kind entry_kind)
/*
Print additional diagnostic information about the IL entry that is
triggering an internal error.
*/
{
  char *s;

  s = retrieve_il_entry_kind_name(entry_kind);
  (void)fprintf(f_debug, 
                "IL info: entry kind =%3ld (%s), \n",
                (long)entry_kind, s, entry_ptr);
  (void)fprintf(f_debug, "         entry_ptr = 0x%lx\n", entry_ptr);
#if __CENTERLINE__
  (void)fprintf(f_debug, "         memory region = %4ld\n",
                _centerline_region_number);
#endif /* __CENTERLINE__ */
}  /* display_il_entry_kind_and_ptr */

#endif /* CHECKING && DEBUG */

static an_il_entry_number *assign_entry_number(
                                       char               *entry_ptr,
                                       an_il_entry_kind   entry_kind,
                                       a_boolean          is_string_entry,
                                       sizeof_t           entry_length,
                                       an_il_entry_number *p_entry_number)
/*
Assign an entry number to the entry pointed to entry_ptr if it does
not already have one.  Return a pointer to the entry number location,
and also the entry number in *p_entry_number.  The entry is of kind
entry_kind, and if it is a string, has length as given by entry_length.
*/
{
  an_il_entry_number *count_ptr;
  a_boolean          is_file_scope_entry;
  an_il_entry_number *enp = (an_il_entry_number *)
                                      (entry_ptr - sizeof(an_il_entry_number));
  int                num_entries = 1;

#if ORPHAN_PROCESSING_NEEDED
  /* If the IL entry is in the file-scope region, the additional pointer
     for the orphaned IL entry list must be accommodated.  This pointer is
     between the IL entry number and the beginning of the IL entry. */
  if (in_file_scope(entry_ptr)) {
    /* The IL entry number pointer must be decremented by the size of a
       pointer. */
    enp = (an_il_entry_number *)((char *)enp - sizeof(char *));
  }
#endif /* ORPHAN_PROCESSING_NEEDED */

  /* String entries can be referenced from several places, possibly in
     different regions.  A string entry is written in the same region
     as the entry that references it, because there are file-scope strings
     pointed to from function-scope entries, and there's no way to link
     the strings into the file scope so that they will be found on an IL
     walk of the file scope.  Therefore, such strings must be given an
     entry number in the function-scope region.  However, if there are
     references from several regions, that may mean that several different
     entry numbers will have to be assigned. */
  if (is_string_entry) {
    num_entries = entry_length;
    /* If the entry is a string entry and has an assigned entry number, ... */
    if (*enp != 0) {
      if (writing_file_scope_il) {
        /* We're writing the file scope, so an entry number in a function
           scope is out of date. */
        if ((*enp & FUNC_ENTRY_NUMBER_TAG) != 0) *enp = 0;
      } else {
        /* We're writing a function scope. */
#if CHECKING
        /* An entry number in the file scope is impossible (we should only
           assign such a number while writing the file scope, and we only do
           that after all the function scopes have been written). */
        if ((*enp & FUNC_ENTRY_NUMBER_TAG) == 0) {
#if DEBUG
          display_il_entry_kind_and_ptr(entry_ptr, entry_kind);
#endif /* DEBUG */
          internal_error("assign_entry_number: file-scope num in func scope");
        }  /* if */
#endif /* CHECKING */
        /* If the entry has already been written, assume that it was assigned
           and written in a previous function scope.  This assumes that a
           string entry will not be referenced twice within one function scope
           (note that multiple references within the file scope ARE possible,
           like in a_source_file references to file names). */
        if ((*enp & ENTRY_WRITTEN_TAG) != 0) *enp = 0;
      }  /* if */
    }  /* if */
#ifdef FFE
  } else if (entry_kind == iek_bound_info_entry) {
    /* Bound information entries are allocated as a variable-length array
       of fixed-length entries.  The entire array is preceded by the space
       in which to store the entry number, but the entries are contiguous. */
    if (array_bound_walk_index != 0) {
      /* This is an entry after the first.  Find the space preceding the
         array by using the current index number, provided by the il_walk
         routines. */
      enp = (an_il_entry_number *)((char *)((a_bound_info_entry_ptr)entry_ptr -
                                                      array_bound_walk_index) -
                                            sizeof(an_il_entry_number));
      /* Return the right entry number, but do not change *enp; it's
         supposed to keep the entry number of the first entry in the array. */
      *p_entry_number = (*enp + array_bound_walk_index) & ~ENTRY_WRITTEN_TAG;
      goto end_of_routine;
    }  /* if */
    /* The first entry.  The processing is fairly normal, except that
       we increment the count of entries to account for all of the
       entries in the array, so we can use consecutive entry numbers
       for them.  That ensures they're contiguous when read back in. */
    num_entries = num_walk_array_bounds;
#endif /* ifdef FFE */
  }  /* if */
  /* Only assign a number if the entry does not already have one.  A zero
     means the entry number has not been assigned yet. */
  if (*enp == 0) {
    /* Use the next available number from the array of entry counts.  Use
       the file-scope array if the entry is in the file scope, the
       function-scope array otherwise.  Note that we can encounter a 
       file-scope entry while scanning a function scope, but not the
       other way around.  Special case: a string entry is considered to
       be in the region of the entry that points to it (see comment above). */
    if (writing_file_scope_il) {
      /* Writing the file scope, so only file-scope items should appear. */
      is_file_scope_entry = TRUE;
#if CHECKING
      if (!in_file_scope(entry_ptr)) {
#if DEBUG
        display_il_entry_kind_and_ptr(entry_ptr, entry_kind);
#endif /* DEBUG */
        internal_error(
         "assign_entry_number: non-file-scope ptr referenced from file scope");
      }  /* if */
#endif /* CHECKING */
    } else {
      /* Writing a function scope, so both file-scope and function-scope
         items may appear. */
      if (is_string_entry) {
        /* A string entry is considered to be in the region of the entry
           that points to it, i.e., a function scope region in this case. */
        is_file_scope_entry = FALSE;
      } else {
        is_file_scope_entry = in_file_scope(entry_ptr);
      }  /* if */
    }  /* if */
    count_ptr = &(is_file_scope_entry ?
                fs_entry_numbers_array : entry_numbers_array)[(int)entry_kind];
    /* Use the next entry number for this entry. */
    *enp = *count_ptr + 1;
    /* For function-scope entries, turn on the bit in the number that
       distinguishes function entry numbers from file-scope numbers. */
    if (!is_file_scope_entry) {
      *enp |= FUNC_ENTRY_NUMBER_TAG;
    }  /* if */
    /* Increment the table entry by 1, or by length for string entries. */
    *count_ptr += num_entries;
  }  /* if */
  *p_entry_number = *enp;
#ifdef FFE
end_of_routine:
#endif /* ifdef FFE */
#if __CENTERLINE__ && CHECKING && DEBUG
  if (entry_kind == centerline_il_entry_kind &&
      ((*p_entry_number) & ~ENTRY_WRITTEN_TAG & ~FUNC_ENTRY_NUMBER_TAG) ==
                   centerline_il_entry_number) {
    /* Correct IL entry kind and IL entry number.  Check if correct
       memory region. */
    if ((*p_entry_number & FUNC_ENTRY_NUMBER_TAG) != 0 ) {
      /* Currently writing a function scope memory region. */
      if ( _centerline_region_number == centerline_memory_region_number ) {
        centerline_stop();
      }  /* if */
    } else {
      /* Currently writing the file scope memory region.  Check if that
         is the desired stop point. */
      if (centerline_memory_region_number == FILE_SCOPE_REGION_NUMBER) {
        centerline_stop();
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* __CENTERLINE__ && CHECKING && DEBUG */
  return (enp);
}  /* assign_entry_number */


static char *remap_ptr_to_entry_number(char             *entry_ptr,
                                       an_il_entry_kind entry_kind)
/*
Convert entry_ptr, a pointer to an IL entry of type entry_kind, to the
corresponding entry number, and return that number cast to "char *".
*/
{
  an_il_entry_number *enp, entry_number;

  if (entry_ptr == NULL) {
    /* A NULL pointer is represented by a zero entry number. */
    entry_number = 0;
  } else {
    /* Test for entry number already assigned.  This test is  mostly for
       speed, since most entries will have numbers assigned by the
       time we get here.  The only case where an entry number would not have
       been assigned is for a non-string entry in the file scope that is
       referenced from an entry in a function scope. */
    enp = (an_il_entry_number *)(entry_ptr - sizeof(an_il_entry_number));
#if ORPHAN_PROCESSING_NEEDED
    /* If the entry_ptr points into the file scope memory region, the
       orphaned IL entry pointer must be skipped over. */
    if (in_file_scope(entry_ptr)) {
      enp = (an_il_entry_number *)((char *)enp - sizeof(char *));
    }  /* if */    
#endif /* ORPHAN_PROCESSING_NEEDED */
    if (*enp == 0) {
#if CHECKING
      if (is_string_entry_kind(entry_kind)) {
        /* All string entries should have entry numbers already.  See
          write_entry.  We can't handle them here because we don't have the
          length. */
#if DEBUG
        display_il_entry_kind_and_ptr(entry_ptr, entry_kind);
#endif /* DEBUG */
        internal_error("remap_ptr_to_entry_number: string entry");
#ifdef FFE
      } else if (entry_kind == iek_bound_info_entry) {
        /* Likewise, bound information entries should already have been
           processed.  We can't handle them here because we don't know where
           the entry falls within the array of entries (see the variable
           array_bound_walk_index). */
#if DEBUG
        display_il_entry_kind_and_ptr(entry_ptr, entry_kind);
#endif /* DEBUG */
        internal_error("remap_ptr_to_entry_number: bound info entry");
#endif /* ifdef FFE */
      }  /* if */
#endif /* CHECKING */
      (void)assign_entry_number(entry_ptr, entry_kind,
                                /*is_string_entry=*/FALSE,(sizeof_t)0,
                                &entry_number);
    } else {
      /* The entry already has an entry number. */
      entry_number = *enp;
    }  /* if */
    /* Drop the "entry written" tag if it's set. */
    entry_number &= ~ENTRY_WRITTEN_TAG;
  }  /* if */
  /* For Centerline-C -- suppress warning about bad pointer.  Version 3.0
     warning number. */
  /*SUPPRESS 80*/
  return ((char *)entry_number);
}  /* remap_ptr_to_entry_number */
#endif /* ALTERNATE_IL_FILE_FORMAT */


void start_il_file(void)
/*
Write the initial information to the IL file, if there is one.
*/
{
  a_memory_region_number zero_region_number = 0;
  a_file_position        zero_file_position = 0;

  /* Note that the file was opened already in cmd_line.c.  f_il_output
     remains NULL if no IL file is being written, as when preprocessing
     only is being done. */
  if (f_il_output != NULL) {
    /* Write a string that identifies the file as an IL file.  The front
       end version number is inserted into the string. */
    (void)fprintf(f_il_output, IL_FILE_MAGIC_STRING, IL_VERSION_NUMBER);
    /* Write the null at the end of the magic string. */
    putc('\0', f_il_output);
    /* Leave space for the number of regions, the offset to the file index,
       the offset to the file-scope region, and the il_header struct.
       These will be filled in when the information is known at the end of 
       file (see finish_il_file and write_memory_region). */
    (void)fwrite((char *)&zero_region_number, sizeof(zero_region_number), 1,
                 f_il_output);
    (void)fwrite((char *)&zero_file_position, sizeof(zero_file_position), 1,
                 f_il_output);
    (void)fwrite((char *)&zero_file_position, sizeof(zero_file_position), 1,
                 f_il_output);
    (void)fwrite((char *)&il_header, sizeof(il_header), 1, f_il_output);
#if ORPHAN_PROCESSING_NEEDED
    /* Leave space for the orphaned_file_scope_il_entries[]. */
    (void)fwrite((char *)orphaned_file_scope_il_entries,
                 sizeof(orphaned_file_scope_il_entries), 1, f_il_output);
#endif /* ORPHAN_PROCESSING_NEEDED */
  }  /* if */
#if ALTERNATE_IL_FILE_FORMAT
  /* Clear the array giving the count of entries of each kind for the
     file scope. */
  { int int_entry_kind;
    for (int_entry_kind = (int)iek_none;
         int_entry_kind < (int)iek_last;
         int_entry_kind++) {
      fs_entry_numbers_array[int_entry_kind] = 0;
    }  /* if */
  }
  /* We need a constant that is at least as big as the largest size of
     a (non-string) IL entry.  We take a guess by adding the sizes of
     two of the largest entries, and check here that we're okay. */
#define MAX_SIZEOF_IL_ENTRY (sizeof(a_type)+sizeof(a_constant))
#if CHECKING
  { int int_entry_kind;
    for (int_entry_kind = (int)iek_none+1;
         int_entry_kind < (int)iek_last;
         int_entry_kind++) {
      if (sizeof_il_entry[int_entry_kind] > MAX_SIZEOF_IL_ENTRY) {
        internal_error("start_il_file: MAX_SIZEOF_IL_ENTRY is defined wrong");
      }  /* if */
    }  /* if */
  }
#endif /* CHECKING */
#endif /* ALTERNATE_IL_FILE_FORMAT */
}  /* start_il_file */


void finish_il_file(void)
/*
Finish writing the IL file, if there is one.
*/
{
  a_memory_region_number end_flag = NULL_region_number;
  a_file_position        index_pos;

  if (f_il_output != NULL) {
    /* If the intermediate language is being written to a file, write the
       end of the file (no-more-regions flag plus the file index), then
       go back and fill in the information at the beginning of the file.
       Recall that the beginning of the file looks like:
         magic string that identifies an IL file (already written properly)
         number of regions
         file offset to the file index table
         file offset to the start of the file scope region
         il_header
    */
#if ORPHAN_PROCESSING_NEEDED
    /*   orphaned_file_scope_il_entries[]
    */
#endif /* ORPHAN_PROCESSING_NEEDED */
    /* and that zeroes were written in all but the first item
       when the file was begun (see start_il_file).  il_header was
       written again by write_memory_region when the file-scope memory
       region was written.
    */
#if ORPHAN_PROCESSING_NEEDED
    /* The orphaned_file_scope_il_entries[] was also written again by
       write_memory_region when the file-scope memory region was written.
    */
#endif /* ORPHAN_PROCESSING_NEEDED */	
    /* Write a zero region number that indicates the end of the list
       of regions. */
    (void)fwrite((char *)&end_flag, sizeof(end_flag), 1, f_il_output);
    /* Write the file index table at the end of the file. */
    index_pos = ftell(f_il_output);
    (void)fwrite((char *)&index_for_il_file[FILE_SCOPE_REGION_NUMBER],
                 highest_used_region_number*sizeof(a_file_position),
                 1, f_il_output);
    /* Seek back to just after the "magic" string at the beginning of the
       file. */
    if (fseek(f_il_output,(long)LEN_IL_FILE_MAGIC_STRING,SEEK_SET) != 0) {
      str_catastrophe(ec_file_write_error, "intermediate language");
    }  /* if */
    /* Write the number of regions. */
    (void)fwrite((char *)&highest_used_region_number,
                 sizeof(highest_used_region_number), 1, f_il_output);
    /* Write the file offset for the file index table. */
    (void)fwrite((char *)&index_pos, sizeof(index_pos), 1, f_il_output);
    /* Write the file offset for the file-scope memory region. */
    (void)fwrite((char *)&index_for_il_file[FILE_SCOPE_REGION_NUMBER],
		 sizeof(a_file_position), 1, f_il_output);
    /* Flush the IL file and check for errors on it. */
    if (fflush(f_il_output) || ferror(f_il_output)) {
      str_catastrophe(ec_file_write_error, "intermediate language");
    }  /* if */
#if !BACK_END_SHOULD_BE_CALLED
    /* If the file is being passed to the back end in another program
       (i.e., it's not being used further in the front end), close the
       IL file. */
    if (fclose(f_il_output)) {
      str_catastrophe(ec_file_write_error, "intermediate language");
    }  /* if */
#endif /* !BACK_END_SHOULD_BE_CALLED */
  }  /* if */
}  /* finish_il_file */


void cancel_il_file(void)
/*
Called on detection of any errors.  Stops further writing of the IL file.
*/
{
  if (f_il_output != NULL) {
    /* Close and delete the IL file. */
#if BACK_END_SHOULD_BE_CALLED
    if (il_file_name == NULL) {
      close_temp_file(f_il_output);
    } else {
#endif /* BACK_END_SHOULD_BE_CALLED */
      (void)fclose(f_il_output);
      delete_file(il_file_name);
#if BACK_END_SHOULD_BE_CALLED
    }  /* if */
#endif /* BACK_END_SHOULD_BE_CALLED */
    /* Prevent further writing to the IL file. */
    f_il_output = NULL;
    il_file_name = NULL;
  }  /* if */
}  /* cancel_il_file */


#if ALTERNATE_IL_FILE_FORMAT
static void write_entry(char             *entry_ptr,
                        an_il_entry_kind entry_kind,
                        sizeof_t         entry_length)
/*
Called during IL tree traversal to write an entry to the IL file.  Called
directly for string entries, and from write_nonstring_entry for all
other entries.  entry_kind indicates the kind of entry, and entry_length
its length.
*/
{
  a_byte             byte_entry_kind;
  an_il_entry_number entry_number, *enp;
  a_boolean          is_string_entry = is_string_entry_kind(entry_kind);
  char               entry_copy[MAX_SIZEOF_IL_ENTRY];

  /* Give this entry an entry number if it does not have one yet.  The entry
     number is stored just ahead of the entry. */
  enp = assign_entry_number(entry_ptr, entry_kind, is_string_entry,
                            entry_length, &entry_number);

  /* Check the "already written" flag in the entry number.  For strings,
     that's okay, since the same string can be pointed to from different
     places.  For non-string entries, it indicates an internal error. */
  if ((entry_number & ENTRY_WRITTEN_TAG) != 0) {
#if CHECKING
    if (!is_string_entry) {
#if DEBUG
      display_il_entry_kind_and_ptr(entry_ptr, entry_kind);
#endif /* DEBUG */
      internal_error("write_entry: non-string entry already written");
    }  /* if */
#endif /* CHECKING */
    goto end_of_routine;
  }  /* if */
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug,
           "Writing IL entry to file: kind = %d, number = %lu, length = %lu\n",
           (int)entry_kind, (unsigned long)entry_number,
           (unsigned long)entry_length);
  }  /* if */
#endif /* DEBUG */
  /* For non-string entries, the pointers must be remapped.  Make a copy
     so the original pointers can be restored after the write. */
  /* Note that entry_copy may not be suitably aligned for direct processing,
     so we have to copy, change the pointers in place, then copy back.
     We can't just change the copy. */
  if (!is_string_entry) {
    memcpy(entry_copy, entry_ptr, (int)entry_length);
    remap_pointers_in_il_entry(entry_ptr, entry_kind,
                               remap_ptr_to_entry_number);
  }  /* if */
  /* Write the entry kind. */
  byte_entry_kind = (int)entry_kind;
  (void)fwrite((char *)&byte_entry_kind, sizeof(byte_entry_kind), 1,
               f_il_output);
  /* Write the entry number. */
  (void)fwrite((char *)&entry_number, sizeof(entry_number), 1, f_il_output);
  /* For strings, write the length. */
#if ORPHAN_PROCESSING_NEEDED
  /* For non-string entries in the file scope memory region, write the
     orphaned file scope IL entry chain pointer. */
#endif /* ORPHAN_PROCESSING_NEEDED */
  if (is_string_entry) {
    (void)fwrite((char *)&entry_length, sizeof(entry_length), 1, f_il_output);
#if ORPHAN_PROCESSING_NEEDED
  } else {
    if (writing_file_scope_il) {
      /* Must remap the orphaned file scope IL entry chain pointer.  Use
         a local copy of the pointer. */
      char *orphan_ptr = remap_ptr_to_entry_number(
                           *(char **)(entry_ptr - sizeof(char *)), entry_kind);

      (void)fwrite((char *)&orphan_ptr, sizeof(char *), 1, f_il_output);
    }  /* if */
#endif /* ORPHAN_PROCESSING_NEEDED */
  }  /* if */
  /* Write the entry itself. */
  /* The (int) cast is for lint on non-ANSI systems.  Under ANSI C, fwrite is
     prototyped and the value will be cast back to size_t, so it's harmless. */
  if (fwrite(entry_ptr, (int)entry_length, 1, f_il_output) != 1) {
    /* Error on write.  This check supplements the check done when the
       file is closed. */
    str_catastrophe(ec_file_write_error, "intermediate language");
  }  /* if */
  if (!is_string_entry) {
    /* Restore the original pointers. */
    memcpy(entry_ptr, entry_copy, (int)entry_length);
  }  /* if */
  /* Set the "entry written" flag. */
  *enp |= ENTRY_WRITTEN_TAG;
end_of_routine:;
}  /* write_entry */


static void write_nonstring_entry(char             *entry_ptr,
                                  an_il_entry_kind entry_kind)
/*
Called during IL tree traversal to write a non-string entry to the IL file.
*/
{
  write_entry(entry_ptr, entry_kind, sizeof_il_entry[(int)entry_kind]);
}  /* write_nonstring_entry */
#endif /* ALTERNATE_IL_FILE_FORMAT */


void write_memory_region(a_memory_region_number region_number)
/*
Write the indicated memory region to the file f_il_output.
*/
{
  char            il_header_copy[sizeof(il_header)];
  a_file_position end_pos;
#if ALTERNATE_IL_FILE_FORMAT && ORPHAN_PROCESSING_NEEDED
  char		  orphaned_file_scope_il_entries_copy
                             [sizeof(orphaned_file_scope_il_entries)];
#endif /* ALTERNATE_IL_FILE_FORMAT && ORPHAN_PROCESSING_NEEDED */

  db_enter(2, "write_memory_region");
  /* Check that the file should in fact be created, which is indicated
     by its having been opened.  If the front end is just doing
     preprocessing, for example, no intermediate language file is 
     written. */
  if (f_il_output != NULL) {
#if DEBUG
    if (debug_level >= 2) {
      fprintf(f_debug, "Writing out memory region %lu\n",
                       (unsigned long)region_number);
    }  /* if */
#endif /* DEBUG */
#if ALTERNATE_IL_FILE_FORMAT
    /* Using alternate IL file format. */
    /* The information written for a region is:
         region number
         array giving, for each entry type, the number of entries of
           that type
         for each entry ----|entry type
                            |entry number
                            |entry length (only for string entries)
    */
#if ORPHAN_PROCESSING_NEEDED
    /*                   or |orphaned IL entry link (file scope only)
    */
#endif /* ORPHAN_PROCESSING_NEEDED */
    /*                      |the entry itself
         zero byte indicating the end of the list.
    */
#else /* !ALTERNATE_IL_FILE_FORMAT */
    /* Using standard IL file format. */
    /* The information written for a region is:
         region number
         memory address that the first block header came from
         memory address of the primary scope entry
         total size in bytes of the information following (all blocks)
         for each block ----|header
                            |block itself
       The header/block sequence is written in such a way that it can be
       read back in with a single read of the indicated total size, and
       so that the headers and blocks will end up properly aligned if
       read in to a properly aligned area.  The header, the start of
       the data in the block, and the next available location are
       already guaranteed to be correctly aligned (see alloc_in_region). */
#endif /* ALTERNATE_IL_FILE_FORMAT */
    /* Remember the current file position as the position of the region
       by saving it in the file index. */
    index_for_il_file[region_number] = ftell(f_il_output);
    /* Write the region number. */
    (void)fwrite((char *)&region_number,
                 sizeof(region_number), 1, f_il_output);
    writing_file_scope_il = (region_number == FILE_SCOPE_REGION_NUMBER);
#if ALTERNATE_IL_FILE_FORMAT
    /* Alternate file format. */
#if CHECKING && DEBUG && __CENTERLINE__
    _centerline_region_number = region_number;
#endif /* CHECKING && DEBUG && __CENTERLINE__ */
    { a_file_position  count_array_pos;
      int              int_entry_kind;
      char             zero = 0;

      /* For a function scope, clear the array of entry counts. */
      if (!writing_file_scope_il) {
        for (int_entry_kind = (int)iek_none;
             int_entry_kind < (int)iek_last;
             int_entry_kind++) {
          entry_numbers_array[int_entry_kind] = 0;
        }  /* if */
      }  /* if */
      /* Write the array of entry counts.  At this point, the write is only
         to reserve the proper amount of space.  The correct values will
         only be known when the region has been fully processed.  Since
         we are writing to fill space, the distinction between the file-scope
         array and the function array is unimportant. */
      count_array_pos = ftell(f_il_output);
      /* The first entry of the array is skipped. */
      (void)fwrite((char *)(&entry_numbers_array[1]),
                   sizeof(entry_numbers_array)-sizeof(an_il_entry_number), 1,
                   f_il_output);
      /* Walk the IL tree for the region, and write the entries. */
      if (writing_file_scope_il) {
        /* The memory region is the file scope region. */
        walk_file_scope_il(write_nonstring_entry, write_entry,
                           (a_remap_function_ptr)NULL);
      } else {
        /* The memory region is a function scope. */
        walk_routine_scope_il(region_number,
                              write_nonstring_entry, write_entry,
                              (a_remap_function_ptr)NULL);
      }  /* if */
      /* Write a zero entry kind, to indicate the end of the list of
         entries. */
      (void)fwrite(&zero, 1, 1, f_il_output);
      /* Save the end-of-file position. */
      end_pos = ftell(f_il_output);
      /* Go back and write the array of entry counts.  This time it matters
         which one we write. */
      if (fseek(f_il_output, count_array_pos, SEEK_SET) != 0) {
        str_catastrophe(ec_file_write_error, "intermediate language");
      }  /* if */
      /* The first entry of the array is skipped. */
      (void)fwrite((char *)&(writing_file_scope_il ?
                              fs_entry_numbers_array : entry_numbers_array)[1],
                   sizeof(entry_numbers_array)-sizeof(an_il_entry_number), 1,
                   f_il_output);
      /* Reposition the file at the end to leave it properly positioned
         for future writes.  SEEK_END is not used because ANSI doesn't 
         guarantee it for binary files. */
      if (fseek(f_il_output, end_pos, SEEK_SET) != 0) {
        str_catastrophe(ec_file_write_error, "intermediate language");
      }  /* if */
    }
#else /* !ALTERNATE_IL_FILE_FORMAT */
    /* Standard IL file format. */
    { sizeof_t               total_bytes;
      a_mem_block_header_ptr hdr;

#if ORPHAN_PROCESSING_NEEDED
      /* For all memory regions, walk the IL tree for the region to catch
         all orphaned file scope IL entry references. */
      if (writing_file_scope_il) {
        /* The memory region is the file scope region. */
        walk_file_scope_il((an_entry_process_function_ptr)NULL,
                           (a_string_entry_process_function_ptr)NULL,
                           (a_remap_function_ptr)NULL);
      } else {
        /* The memory region is a function scope. */
        walk_routine_scope_il(region_number,
                              (an_entry_process_function_ptr)NULL,
                              (a_string_entry_process_function_ptr)NULL,
                              (a_remap_function_ptr)NULL);
      }  /* if */
#endif /* ORPHAN_PROCESSING_NEEDED */
      /* Determine the total size of all the blocks.  This includes the 
         headers as well as the block contents.  Note that we write out only
         to next_avail_in_block, not to after_end_of_block, since that's
         the only part with data in it, and it's correctly aligned. */
      total_bytes = 0;
      for (hdr = mem_region_table[region_number];
           hdr != NULL;
           hdr = hdr->next) {
        total_bytes += hdr->next_avail_in_block - (char *)hdr;
      }  /* for */
      hdr = mem_region_table[region_number];
      /* Write the original address of the first block, the original address
         of the primary scope entry, and the total size of the blocks
         following. */
      (void)fwrite((char *)&hdr, sizeof(hdr), 1, f_il_output);
      (void)fwrite((char *)&il_header.region_scope_entry[region_number],
                   sizeof(a_scope_ptr), 1, f_il_output);
      (void)fwrite((char *)&total_bytes, sizeof(total_bytes), 1, f_il_output);
      /* Write the blocks. */
      for (; hdr != NULL; hdr = hdr->next) {
        if (fwrite((char *)hdr, (int)(hdr->next_avail_in_block - (char *)hdr),
                   1, f_il_output) != 1) {
          /* Error on write.  This check supplements the check done when the
             file is closed. */
          str_catastrophe(ec_file_write_error, "intermediate language");
        }  /* if */
      }  /* for */
    }
#endif /* ALTERNATE_IL_FILE_FORMAT */
    if (region_number == FILE_SCOPE_REGION_NUMBER) {
      /* If we have just written the file-scope memory region, go
         back and rewrite il_header at the beginning of the file.  This
         must be done now rather than in finish_il_file because the
         file-scope storage may get freed and we may need to be
         able to check addresses in il_header to see if they're valid
         file-scope addresses.
      */
#if ORPHAN_PROCESSING_NEEDED
      /* Also the orphaned_file_scope_il_entries array must be written now
         for the same reason.
      */
#endif /* ORPHAN_PROCESSING_NEEDED */
     /*  Recall that the beginning of the file looks like:
           magic string that identifies an IL file (already written properly)
           number of regions (written as 0)
           file offset to the file index table (written as 0)
           file offset to the start of the file scope region (written as 0)
           il_header (written as 0)
      */
#if ORPHAN_PROCESSING_NEEDED
      /* and
           orphaned_file_scope_il_entries[]
      */
#endif /* ORPHAN_PROCESSING_NEEDED */
      /* Save the current (end of file) position. */
      end_pos = ftell(f_il_output);
      /* Seek to where the il_header was written. */
      if (fseek(f_il_output,
                (long)(LEN_IL_FILE_MAGIC_STRING+
                       sizeof(a_memory_region_number)+
                       2*sizeof(a_file_position)),
                SEEK_SET) != 0) {
        str_catastrophe(ec_file_write_error, "intermediate language");
      }  /* if */
      /* Save il_header; it gets modified, written, then restored. */
      memcpy(il_header_copy, (char *)&il_header, sizeof(il_header));
#if ALTERNATE_IL_FILE_FORMAT
      /* In the alternate form, the pointers in the header must be remapped to
         entry numbers. */
      remap_il_header_pointers(remap_ptr_to_entry_number);
#endif /* ALTERNATE_IL_FILE_FORMAT */
      /* The region_scope_entry pointer must be reconstructed on the other
         end.  Clear it to NULL to avoid confusion. */
      il_header.region_scope_entry = NULL;
      (void)fwrite((char *)&il_header, sizeof(il_header), 1, f_il_output);
      /* Restore il_header. */
      memcpy((char *)&il_header, il_header_copy, sizeof(il_header));
#if ORPHAN_PROCESSING_NEEDED
#if ALTERNATE_IL_FILE_FORMAT
      /* Save a copy of the orphaned IL entry array; it gets modified,
         written, then restored. */
      memcpy(orphaned_file_scope_il_entries_copy,
             (char *)orphaned_file_scope_il_entries,
             sizeof(orphaned_file_scope_il_entries));
      /* The pointers in the orphaned IL entry table must be remapped to
         entry_numbers. */
      remap_orphaned_file_scope_entry_array_ptrs(
                                   remap_ptr_to_entry_number);

#endif /* ALTERNATE_IL_FILE_FORMAT */
      /* Copy the orphaned_file_scope_il_entries array to the file. */
      (void)fwrite((char *)orphaned_file_scope_il_entries,
                   sizeof(orphaned_file_scope_il_entries), 1, f_il_output);
#if ALTERNATE_IL_FILE_FORMAT
      /* Restore the orphaned IL entry table. */
      memcpy((char *)orphaned_file_scope_il_entries,
             orphaned_file_scope_il_entries_copy,
             sizeof(orphaned_file_scope_il_entries));
#endif /* ALTERNATE_IL_FILE_FORMAT */
#endif /* ORPHAN_PROCESSING_NEEDED */
      /* Restore the position at the end of the file.  SEEK_END is not
         used because ANSI doesn't guarantee it for binary files. */
      if (fseek(f_il_output, end_pos, SEEK_SET) != 0) {
        str_catastrophe(ec_file_write_error, "intermediate language");
      }  /* if */
    }  /* if */
  }  /* if */

  db_exit();
}  /* write_memory_region */

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
