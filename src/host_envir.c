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

host_envir.c -- Host-environment-dependent routines.

(These have to do with operating system and file name differences.)

This version for UNIX (UNIX is a trademark of Unix System Laboratories),
MS-DOS, or VAX/VMS.

*/

/* Header files common to all files. */
#include "fe_common.h"

#if HDRSTOP_RECOGNIZED
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* HDRSTOP_RECOGNIZED */

/*
Argument strings for fopen.
*/
#if READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS
/* Read source files in binary mode for some MS-DOS cases.  Carriage return
   and control-Z are handled explicitly. */
#define FOPEN_MODE_FOR_READ "rb"
#define FOPEN_MODE_FOR_WRITE "w"
#define FOPEN_MODE_FOR_UPDATE "w+"
#define FOPEN_MODE_FOR_BINARY_WRITE "wb"
#define FOPEN_MODE_FOR_BINARY_UPDATE "w+b"
#define FOPEN_MODE_FOR_BINARY_READ "rb"
#else /* !READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS */
#if __ANSIC__ || __MSDOS__
/* ANSI C allows binary modes.  So does MS-DOS. */
#define FOPEN_MODE_FOR_READ "r"
#define FOPEN_MODE_FOR_WRITE "w"
#define FOPEN_MODE_FOR_UPDATE "w+"
#define FOPEN_MODE_FOR_BINARY_WRITE "wb"
#define FOPEN_MODE_FOR_BINARY_UPDATE "w+b"
#define FOPEN_MODE_FOR_BINARY_READ "rb"
#else /* !(__ANSIC__ || __MSDOS__) */
/* Assume UNIX (binary and text files the same). */
#define FOPEN_MODE_FOR_READ "r"
#define FOPEN_MODE_FOR_WRITE "w"
#define FOPEN_MODE_FOR_UPDATE "w+"
#define FOPEN_MODE_FOR_BINARY_WRITE "w"
#define FOPEN_MODE_FOR_BINARY_UPDATE "w+"
#define FOPEN_MODE_FOR_BINARY_READ "r"
#endif /* __ANSIC__  || __MSDOS__ */
#endif /* READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS */

#if __MSDOS__
/* Include file layout for MS-DOS compilers. */
#if __TURBOC__
/* Need dir.h for lengths of path components. */
#include <dir.h>
#else /* __TURBOC__ */
#if __MSC__
/* Need process.h for definition of getpid(). */
#include <process.h>
#endif /* __MSC__ */
#endif /* __TURBOC__ */
#endif /* __MSDOS__ */

/* ANSI signal handlers return void. Older UNIX signal handlers in general,
and SVID compliant signal handlers in particular, return int. */
#ifdef __ANSIC__
#define SIGNAL_HANDLER_RETURNS_VOID 1
#endif  /* __ANSIC__ */
#ifdef sun
/* SunOS 4.1 switched to the ANSI form of signal. */
#define SIGNAL_HANDLER_RETURNS_VOID 1
#endif  /* sun */
#ifdef __hpux
#define SIGNAL_HANDLER_RETURNS_VOID 1
#endif  /* __hpux */

#include <signal.h>
#ifdef SIGNAL_HANDLER_RETURNS_VOID
typedef void a_signal_handler_return_value;
#else /* !defined(SIGNAL_HANDLER_RETURNS_VOID) */
typedef int a_signal_handler_return_value;
#endif /* defined(SIGNAL_HANDLER_RETURNS_VOID) */

#include <errno.h>
#if __ANSIC__
/* Files that are included for functions that conform to ANSI C libraries. */
#include <stdlib.h>
#else /* __ANSIC__ */
EXTERN_C char *getenv(char *name);
EXTERN_C int abort(void);
EXTERN_C void exit(int status);
#if __BSD__
/* BSD errno.h doesn't define "errno". */
EXTERN_C int errno;
#endif /* __BSD__ */
#endif /* __ANSIC__ */

/*
Header files needed to use the system routines to get the elapsed clock
time and CPU time.  The ANSI routines are used when possible; otherwise
the UNIX routines are assumed to be available.

Note: If you are not using an ANSI C library make sure that
CLOCK_FREQUENCY is defined properly below.
*/
#if __ANSIC__
#include <time.h>
#ifndef CLOCKS_PER_SEC
 #error -- Compiling in __ANSIC__ mode but CLOCKS_PER_SEC is not
	    defined in time.h.
#endif /* defined(CLOCKS_PER_SEC) */
#else /* !__ANSIC__ */
#include <sys/types.h>
#include <sys/times.h>
#ifdef sun
#include <sys/param.h>
#define CLOCK_FREQUENCY HZ
#else /* !defined(sun) */
#define CLOCK_FREQUENCY 60
#endif /* defined(sun) */
#endif /* __ANSIC__ */

/*
Determine whether getcwd or getwd should be used to get the current
directory.  getwd is used on BSD, getcwd on other systems.
*/
#if __MSDOS__
#define USE_GETCWD 1
#include <direct.h>
#else /* !__MSDOS___ */
#if __BSD__
#include <sys/param.h>
extern char* getwd(char *pathname);
#define USE_GETCWD 0
#else /* !__BSD__ */
#include <unistd.h>
#define USE_GETCWD 1
#endif /* __BSD__ */
#endif /* __MSDOS__ */

#if __MSDOS__
/* Function definitions for MS-DOS compilers. */
#if __TURBOC__ || __ZTC__
/* Neither Turbo C nor Zortech have the getpid call.  Since MSDOS does
   not have multiple tasks, just return a 1. */
static int getpid(void)
{
  return (1);
}
#endif /* __TURBOC__ */
#else /* __MSDOS__ */
#ifdef __cplusplus
#include <sysent.h>
#else /* !define(__cplusplus) */
/* Function definitions for non MS-DOS compilers. */
/* The SUN does not have the getpid(), unlink(), and time() calls defined 
   in include files. */
#if __BSD__
EXTERN_C int getpid(void);
/* Unlink (delete) a file. */
EXTERN_C int unlink(char *path);
EXTERN_C time_t time(time_t* tloc);
#endif /* __BSD__ */
#endif /* __cplusplus */
#endif /* __MSDOS__ */

/*
Define some maximum lengths for MS-DOS file name handling stuff.  Note that the
defined lengths INCLUDE the terminating null character.  "__MAXDRIVE__" is the
length of the disk drive letter which includes the trailing ":".  "__MAXDIR__"
is the length of the directory part of the path including a trailing slash.
"__MAXFILE__" is the length of the base file name.  "__MAXEXT__" is the length
of the extension including the leading ".".
*/
#if __MSDOS__
#if __TURBOC__
#define __MAXDRIVE__ MAXDRIVE
#define __MAXDIR__   MAXDIR
#define __MAXFILE__  MAXFILE
#define __MAXEXT__   MAXEXT

#else /* __TURBOC__ */
#if __MSC__
#define __MAXDRIVE__ _MAX_DRIVE
#define __MAXDIR__   _MAX_DIR
#define __MAXFILE__  _MAX_FNAME
#define __MAXEXT__   _MAX_EXT
#else /* __MSC__ */
#if __ZTC__
/* Zortech doesn't currently (version 3.0) define these values. */
#define __MAXDRIVE__ 3
#define __MAXDIR__   66
#define __MAXFILE__  9
#define __MAXEXT__   5
#else /* __ZTC */
error -- unknown MS-DOS compiler.
#endif /* __ZTC */
#endif /* __MSC__ */
#endif /* __TURBOC__ */
#endif /* __MSDOS__ */


/*
Define a macro which takes a complete file path and breaks it up into the parts
as described above.
*/
#if __MSDOS__
#if __TURBOC__
#define split_path(path, drive, dir, file, ext) \
	  (void)fnsplit(path, drive, dir, file, ext)
#define merge_path(path, drive, dir, file, ext) \
	  fnmerge(path, drive, dir, file, ext)

#else /* __TURBOC__ */
#if __MSC__
#define split_path(path, drive, dir, file, ext) \
	  _splitpath(path, drive, dir, file, ext)
/* MSC does not have a function to assemble a path from its components, so use
   "strcpy" and "strcat" to reassemble. */
#define merge_path(path, drive, dir, file, ext) \
	  (void)strcpy(path, drive);		\
	  strcat(path, dir);			\
	  strcat(path, file);			\
	  strcat(path, ext);
#else /* __MSC__ */
#if __ZTC__
static void split_path(char *path,
                      char *drive,
                      char *dir,
                      char *file,
                      char *ext)
/*
Zortech doesn't provide a version of split_path so we use our own version.
This routine is similar to the Borland "fnsplit" and the Microsoft
"_splitpath" except that this routine does not accept NULL values for
path name components and doesn't return a value as the Borland version
does.
*/
{
  char		*colon_pos;
  char		*last_slash_pos;
  char		*start_pos = path;
  char		*dot_pos;
  sizeof_t	length;

  /* Initialize the components of the string in case they are not
     explicitly specified. */
  strcpy(drive, "");
  strcpy(dir, "");
  strcpy(file, "");
  strcpy(ext, "");
  /* Get the drive.  Copy the drive to the destination -- the colon is
     copied too. */
  colon_pos = strchr(path, ':');
  if (colon_pos != NULL) {
    length = colon_pos - path + 1;
    length = (length < __MAXDRIVE__) ? length : __MAXDRIVE__ - 1;
    strncat(drive, start_pos, length);
    /* If the drive name was truncated add the terminating colon. */
    if (length == (__MAXDRIVE__ - 1)) {
      drive[length-1] = ':';
    }  /* if */
    start_pos = colon_pos + 1;
  }  /* if */
  /* See if there is a directory name.  If so, copy it to the destination
     including leading and trailing slashes. */
  last_slash_pos = strrchr(start_pos, '\\');
  if (last_slash_pos != NULL) {
    length = last_slash_pos - start_pos + 1;
    length = (length < __MAXDIR__) ? length : __MAXDIR__ - 1;
    strncat(dir, start_pos, length);
    start_pos = last_slash_pos + 1;
  }  /* if */
  /* Find the '.' that ends the filename if one exists.  Copy the file name
     and extension.  The '.' is part of the extension. */
  dot_pos = strchr(start_pos, '.');
  if (dot_pos != NULL) {
    /* An extension exists. */
    length = dot_pos - start_pos;
    length = (length < __MAXFILE__) ? length : __MAXFILE__ - 1;
    strncat(file, start_pos, length);
    length = __MAXEXT__ - 1;
    strncat(ext, dot_pos, length);
  } else {
    /* No extension exists. */
    length = __MAXFILE__ - 1;
    strncat(file, start_pos, length);
  }  /* if */
}  /* split_path */


/* Zortech does not have a function to assemble a path from its components,
   so use  "strcpy" and "strcat" to reassemble. */
#define merge_path(path, drive, dir, file, ext) \
	  (void)strcpy(path, drive);		\
	  strcat(path, dir);			\
	  strcat(path, file);			\
	  strcat(path, ext);
#else /* __ZTC */
error -- unknown MS-DOS compiler.
#endif /* __ZTC */
#endif /* __MSC__ */
#endif /* __TURBOC__ */
#endif /* __MSDOS__ */


static a_directory_name_entry_ptr
		avail_directory_name_entries = NULL;
			/* Available list of directory name entries. */


#if STACK_REFERENCED_INCLUDE_DIRECTORIES
static void free_directory_name_entry(a_directory_name_entry_ptr dnep)
/*
Add dnep to the available list of directory name entries.
*/
{
  dnep->next = avail_directory_name_entries;
  avail_directory_name_entries = dnep;
}  /* free_directory_name_entry */
#endif /* STACK_REFERENCED_INCLUDE_DIRECTORIES */


static a_directory_name_entry_ptr alloc_directory_name_entry(void)
/*
Allocate a new directory name entry, and set its fields to default values.
The space is allocated in general (not IL or FE) memory.
*/
{
  a_directory_name_entry_ptr entry_ptr;

  if (avail_directory_name_entries == NULL) {
    entry_ptr = (a_directory_name_entry_ptr)
                                 alloc_general(sizeof(a_directory_name_entry));
  } else {
    entry_ptr = avail_directory_name_entries;
    avail_directory_name_entries = avail_directory_name_entries->next;
  }  /* if */
  entry_ptr->dir_name = NULL;
  entry_ptr->next     = NULL;
  return (entry_ptr);
}  /* alloc_directory_name_entry */


void add_to_include_search_path(char *dir_name)
/*
Add the indicated directory to the end of the include file search
path.  The directory name string should be allocated in general memory.
*/
{
  a_directory_name_entry_ptr new_search_path;

  new_search_path = alloc_directory_name_entry();
  new_search_path->dir_name = dir_name;
  new_search_path->next     = NULL;
  if (incl_search_path == NULL) {
    incl_search_path = new_search_path;
  } else {
    end_incl_search_path->next = new_search_path;
  }  /* if */
  end_incl_search_path = new_search_path;
}  /* add_to_include_search_path */


void add_to_front_of_include_search_path(char *dir_name)
/*
Add the indicated directory to the front of the include file search
path.  The directory name string should be allocated in general memory.
*/
{
  a_directory_name_entry_ptr new_search_path;

  new_search_path = alloc_directory_name_entry();
  new_search_path->dir_name = dir_name;
  new_search_path->next     = incl_search_path;
  if (incl_search_path == NULL) end_incl_search_path = new_search_path;
  incl_search_path = new_search_path;
}  /* add_to_front_of_include_search_path */


void add_default_include_search_path(void)
/*
Add the list of directories to be used as a default search path for
include files to the end of the search path lists.
*/
{
#if NO_USR_INCLUDE
  /* On some systems, the standard system include directory should not
     be included.  This is usually because the compiler is being used
     as a cross-compiler and all the "system" include files should be
     explicitly included from somewhere else. */
#else /* !NO_USR_INCLUDE */
  char *usr_include;

  /* Add the default directory to the end of the normal search path. */
  usr_include = getenv("USR_INCLUDE");
  if (usr_include == NULL) usr_include = DEFAULT_USR_INCLUDE;
  add_to_include_search_path(usr_include);
#endif /* NO_USR_INCLUDE */
#if __VMS__
  /* For VMS, add the current directory as a second search directory
     after the directory containing the primary source file (or
     the alternate first directory in pcc mode). */
  add_to_front_of_include_search_path("");
#endif /* __VMS__ */
}  /* add_default_include_search_path */

#if NEED_CHANGE_PRIMARY_INCLUDE_SEARCH_DIR

void change_primary_include_search_dir(char *dir_name)
/*
Replace the directory name in the primary include file search path
entry by "dir_name".  The directory name string should be allocated in
general memory.
*/
{
  incl_search_path->dir_name = dir_name;
}  /* change_primary_include_search_dir */

#endif /* NEED_CHANGE_PRIMARY_INCLUDE_SEARCH_DIR */

void push_primary_include_search_dir(char *dir_name)
/*
The directory name in the primary include file search path should become
"dir_name", as the result of pushing a new include file onto the source
input stack.  The directory name string should be allocated in general memory.

Note that it is not specified within the ANSI C standard or the ARM what the
search rules should be for nested includes.  By default, the search for
nested includes begins in the source directory of the current input file
(not the source directory of the primary input file).  This is the approach
generally taken C compilers on UNIX systems.  A "stack-model" variation of
this approach (as employed by Microsoft C compilers) follows from setting
STACK_REFERENCED_INCLUDE_DIRECTORIES to TRUE.  For a default of ignoring the
directory of the include file and starting with the directory of the primary
input file instead, call change_primary_include_search_dir only in pcc mode;
a similar change would be required in pop_primary_include_search_dir.
*/
{
#if !STACK_REFERENCED_INCLUDE_DIRECTORIES
  /* The name in the current primary include search directory (the head of
     list of directory name entries) is simply replaced by dir_name. */
  change_primary_include_search_dir(dir_name);
#else /* STACK_REFERENCED_INCLUDE_DIRECTORIES */
  /* The new directory becomes the primary include search directory, but the
     current one remains in the search path. */
  add_to_front_of_include_search_path(dir_name);
#endif /* STACK_REFERENCED_INCLUDE_DIRECTORIES */
}  /* push_primary_include_search_dir */


void pop_primary_include_search_dir(char *dir_name)
/*
The directory name in the primary include file search path should revert to
"dir_name", as the result of popping an include file from the source
input stack.
*/
{
#if !STACK_REFERENCED_INCLUDE_DIRECTORIES
  /* The name in the current primary include search directory (the head of
     list of directory name entries) is simply replaced by dir_name. */
  change_primary_include_search_dir(dir_name);
#else /* STACK_REFERENCED_INCLUDE_DIRECTORIES */
  /* The entry of the current primary include search directory is removed
     from the search path, and the resulting primary include search directory
     will correspond to dir_name. */
  a_directory_name_entry_ptr  dnep;

  dnep = incl_search_path;
  incl_search_path = incl_search_path->next;
  check_assertion(incl_search_path != NULL &&
                  (strcmp(incl_search_path->dir_name,dir_name) == 0));
  free_directory_name_entry(dnep);
#endif /* STACK_REFERENCED_INCLUDE_DIRECTORIES */
}  /* pop_primary_include_search_dir */


static char *end_of_directory_name(char *file_name)
/*
Return a pointer to the end of the directory part of the indicated file
name, or NULL if there is no directory part.
Note that the following must work for FILE_NAME_FOR_STDIN, which is
used to represent stdin; it must return  NULL.
*/
{
  char *last_slash;
#if __MSDOS__
  char *last_backslash;
#endif /* __MSDOS__ */

  if (strcmp(file_name, FILE_NAME_FOR_STDIN) == 0) {
    /* Special pseudo-name used for stdin; no directory. */
    last_slash = NULL;
  } else {
#if __VMS__
    /* VMS -- Check for "[]" for directory, or ":" for disk or node name. */
    last_slash = strrchr(file_name, ']');
    if (last_slash == NULL) last_slash = strrchr(file_name, ':');
#else /* !__VMS__ */
    /* UNIX-like system -- check for last slash. */
    last_slash = strrchr(file_name, '/');
#if __MSDOS__
    /* MSDOS -- Allow backslash as an alternative to "/", and check for ":"
       of disk name. */
    last_backslash = strrchr(file_name, '\\');
    if (last_slash == NULL || last_backslash > last_slash) {
      last_slash = last_backslash;
    }  /* if */
    if (last_slash == NULL && strlen(file_name) >= 2 && file_name[1] == ':') {
      /* Disk name is specified, as in "c:abc". */
      last_slash = file_name+1;
    }  /* if */
#endif /* __MSDOS__ */
#endif /* __VMS__ */
  }  /* if */
  return(last_slash);
}  /* end_of_directory_name */


char *directory_of(char *file_name)
/*
Return a string that is the directory name for the given file.  If the
file has no explicit directory, return a representation for the current 
directory.  This routine builds an internal list of directory name strings
and attempts to reuse them to avoid allocating the same string over and
over.  The string returned will be allocated in the intermediate language
memory region.
*/
{
  /* dir_name_list is in host_envir.h so it can be initialized by fe_init. */
  a_directory_name_entry_ptr curr_dir_name;

  char     *last_slash;
  sizeof_t dir_name_length;
  char     *dir_name;

  last_slash = end_of_directory_name(file_name);
  if (last_slash == NULL) {
    /* No directory name, use "" meaning the current directory. */
    dir_name_length = 0;
  } else {
    /* There is a directory name.  Save its length including punctuation. */
    dir_name_length = last_slash - file_name + 1;
  }  /* if */
  /* Look for an existing name on the list that can be reused. */
  /* We assume that a compilation is not going to use a large number of
     include file directories, so we don't need anything fancier than a
     linear linked list here. */
  for (curr_dir_name = dir_name_list;
       curr_dir_name != NULL;
       curr_dir_name = curr_dir_name->next) {
    dir_name = curr_dir_name->dir_name;
    if (strlen(dir_name) == dir_name_length &&
        strncmp(dir_name, file_name, size_t_arg(dir_name_length)) == 0) {
      goto found_dir_name;
    }  /* if */
  }  /* for */
  /* No reusable name found.  Allocate a copy of the directory name in the
     file-scope IL region. */
  dir_name = (char *)alloc_il((sizeof_t)(dir_name_length+1));
  if (dir_name_length > 0) {
    (void)memcpy(dir_name, file_name, size_t_arg(dir_name_length));
  }  /* if */
  dir_name[dir_name_length] = '\0';
  /* Put this new name on the list for future reuse. */
  curr_dir_name = alloc_directory_name_entry();
  curr_dir_name->dir_name = dir_name;
  curr_dir_name->next = dir_name_list;
  dir_name_list = curr_dir_name;
found_dir_name:;
  return(dir_name);
}  /* directory_of */


char *gs_directory_of(char *file_name)
/*
Return a string that is the directory name for the given file.  If the
file has no explicit directory, return a representation for the current 
directory.  The string returned will be allocated in general storage, not
in IL storage.  No pooling of strings is done.
*/
{
  char     *last_slash;
  sizeof_t dir_name_length;
  char     *dir_name;

  last_slash = end_of_directory_name(file_name);
  if (last_slash == NULL) {
    /* No directory name, use "" meaning the current directory. */
    dir_name_length = 0;
  } else {
    /* There is a directory name.  Save its length including punctuation. */
    dir_name_length = last_slash - file_name + 1;
  }  /* if */
  /* Allocate a copy of the directory name in general memory. */
  dir_name = (char *)alloc_general((sizeof_t)(dir_name_length+1));
  if (dir_name_length > 0) {
    (void)memcpy(dir_name, file_name, size_t_arg(dir_name_length));
  }  /* if */
  dir_name[dir_name_length] = '\0';
  return(dir_name);
}  /* gs_directory_of */

#if NEED_DERIVED_NAME

#if __MSDOS__
static void truncate_msdos_filename(char *filename)
/*
Truncate the base and extension parts of an MSDOS filename so that it fits into
the maximum specified by "__MAXFILE__" and "__MAXEXT__".  Note that these
lengths include the null terminator.  This routine can truncate the front of
the base if necessary.
*/
{
  char drive[__MAXDRIVE__];
  char dir[__MAXDIR__];
  char file[__MAXFILE__];
  char ext[__MAXEXT__];

  /* Split the name into its parts. */
  split_path(filename, drive, dir, file, ext);
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
  /* See if the file name ends with the suffix used for C output. */
  { char *C_file_suffix = GEN_C_FILE_SUFFIX;
    if (strlen(filename) > strlen(C_file_suffix) &&
        strcmp(filename + strlen(filename) - strlen(C_file_suffix),
               C_file_suffix) == 0) {
      sizeof_t IL_pre_suffix_len = strchr(C_file_suffix, '.') - C_file_suffix;
      /* See if the suffix was truncated. */
      if (memcmp(file+strlen(file)-IL_pre_suffix_len, C_file_suffix,
                 size_t_arg(IL_pre_suffix_len)) != 0) {
        /* Yes, it was truncated.  Re-truncate so as to preserve the "_int"
           part of the base name.  For example, "abcdef_int" is truncated to
           "abcd_int". */
        (void)memcpy(file+__MAXFILE__-1-IL_pre_suffix_len,
                     C_file_suffix, size_t_arg(IL_pre_suffix_len));
        file[__MAXFILE__-1] = '\0';
      }  /* if */
    }  /* if */
  }
#endif /* BACK_END_IS_C_GEN_BE || ... */
  /* Truncate the filename part at the end (this may be unnecessary). */
  file[__MAXFILE__-1] = '\0';
  /* Truncate the extension part at the end (this may be unnecessary). */
  ext[__MAXEXT__-1] = '\0';
  /* Put the file name back together.  The resulting name is never longer than
     the original. */
  merge_path(filename, drive, dir, file, ext);
}  /* truncate_msdos_filename */
#endif /* __MSDOS__ */

#endif /* NEED_DERIVED_NAME */
#if NEED_DERIVED_NAME

char *derived_name(char *file_name,
                   char *suffix)
/*
Return a string that is the base name of file_name with the given suffix
appended.  The string is allocated in general storage, NOT in an
intermediate language memory region, so it must be copied if it is to
be passed to the back end.
*/
{
  char     *last_slash, *last_dot, *name_start, *name_end;
  sizeof_t der_name_length, suffix_length, base_name_length;
  char     *der_name;

  /* Find the base name by removing the directory and suffix. */
  last_slash = end_of_directory_name(file_name);
  if (last_slash == NULL) {
    /* No directory name, start of file name is start of base name. */
    name_start = file_name;
  } else {
    /* Start of base name is after the directory name. */
    name_start = last_slash + 1;
  }  /* if */
  /* Find suffix, if any. */
  if ((last_dot = strrchr(name_start, '.')) == NULL) {
    /* No suffix, end of base name is the same as end of file name. */
    name_end = name_start + strlen(name_start) - 1;
  } else {
    /* End of base name is before the suffix. */
    name_end = last_dot - 1;
  }  /* if */
  /* Copy the base name and suffix into the derived name. */
  suffix_length = strlen(suffix);
  base_name_length = name_end - name_start + 1;
  der_name_length = base_name_length + suffix_length;
  der_name = (char *)alloc_general(der_name_length+1);
  (void)memcpy(der_name, name_start, size_t_arg(base_name_length));
  (void)memcpy(&der_name[base_name_length], suffix, size_t_arg(suffix_length));
  der_name[der_name_length] = '\0';
#if __MSDOS__
  /* Check for and truncate file names that are too long for MSDOS 
     to handle. */
  truncate_msdos_filename(der_name);
#endif /* __MSDOS__ */
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "derived name = \"%s\".\n", der_name);
  }  /* if */
#endif /* DEBUG */
  return(der_name);
}  /* derived_name */

#endif /* NEED_DERIVED_NAME */

char *combine_dir_and_file_name (char *dir_name,
                                 char *file_name,
				 char *buffer,
				 int  buffer_size)
/*
Combine the given directory name and file name to make a full path name,
and return a pointer to it.  If possible, this routine will return an already
allocated string (e.g., file_name if the directory name is null); otherwise,
it will place the name in *buffer (whose length is given by buffer_size) if
buffer != NULL and the name will fit; failing that, it will call alloc_il
to allocate the space in the intermediate language memory region.
*/
{
  char      *temp_file_name;
  sizeof_t  dir_length, total_length;
  a_boolean need_to_add_slash;

  /* If the directory name is the current directory, then produce
     a joined name that is just the file name.  This makes for nicer-looking
     file names. */
  dir_length = strlen(dir_name);
  if (dir_length == 0) {
    temp_file_name = file_name;
  } else {
    /* See if a slash will have to be added between the two. */
#if __VMS__
    need_to_add_slash = FALSE;
#else /* !__VMS__ */
    need_to_add_slash = (dir_name[dir_length-1] != '/');
#if __MSDOS__
    /* Under MSDOS, both kinds of slashes need to be checked. */
    need_to_add_slash = need_to_add_slash && (dir_name[dir_length-1] != '\\');
#endif /* __MSDOS__ */
#endif /* __VMS__ */
    total_length = dir_length + strlen(file_name) + need_to_add_slash + 1;
    /* See if the buffer is provided and if the required string will fit 
       in it. */
    if (buffer != NULL && ((sizeof_t)buffer_size) >= total_length) {
      /* The buffer is supplied and it is big enough.  Use it. */
      temp_file_name = buffer;
    } else {
      /* Allocate the needed space. */
      temp_file_name = (char *)alloc_il((sizeof_t)total_length);
    }  /* if */
    /* Copy the directory name. */
    (void)memcpy(temp_file_name, dir_name, size_t_arg(dir_length));
    if (need_to_add_slash) {
      /* Add the slash following the directory name. */
#if __MSDOS__
      if (strchr(dir_name, '/') != NULL) {
	/* The original path uses regular UNIX-style slashes; use one to splice
	   the file and path to make it look consistent. */
        temp_file_name[dir_length++] = '/';
      } else {
	/* The directory name does not have any UNIX-style slashes or has no
	   slashes at all.  In either case, under MSDOS, use an MSDOS-style
	   slash. */
        temp_file_name[dir_length++] = '\\';
      }  /* if */
#else /* __MSDOS__ */
      temp_file_name[dir_length++] = '/';
#endif /* if __MSDOS__ */
    }  /* if */
    /* Add the file name to the directory name. */
    (void)strcpy(&temp_file_name[dir_length], file_name);
  }  /* if */
  return(temp_file_name);
}  /* combine_dir_and_file_name */


#if INSTANTIATION_BY_IMPLICIT_INCLUSION
char *replace_file_name_suffix(char  *new_suffix,
                               char  *file_name,
                               char  *buffer,
                               int   buffer_size,
                               char  **suffix_loc)
/*
Replace the suffix of a file name with a specified suffix.  Try to replace
the file name in place.  This can be done if file_name is within buffer and
buffer has enough addition space for the possibly enlarged name or if the
new suffix takes no more space than the suffix it replaces; otherwise
storage will be allocated for the new name.  This routine may be called
iteratively.  On second and subsequent calls, suffix_loc points to the
place in file_name where the suffix begins.
*/
{
  char       *ch, *new_file_name;
  a_boolean  suffix_delim_required = FALSE;
  sizeof_t   curr_file_name_size, curr_suffix_length;
  sizeof_t   new_file_name_base_size, new_file_name_size;
#define SUFFIX_DELIMITER '.'

  db_enter(5, "replace_file_name_suffix");
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "current file_name = \"%s\", new suffix = \"%s\"\n",
            file_name, new_suffix);
  }  /* if */
#endif /* DEBUG */
  /* Determine the size of file_name, excluding the trailing NULL. */
  curr_file_name_size = strlen(file_name);
  check_assertion(curr_file_name_size > 0);
  check_assertion(file_name[curr_file_name_size] == '\0');
  if (*suffix_loc != NULL) {
    /* This name has already had a new suffix added, so we can use *suffix_loc
       saved from last time. */
    /* Determine the length of the current suffix. */
    check_assertion(*(*suffix_loc-1) == SUFFIX_DELIMITER);
    curr_suffix_length = &file_name[curr_file_name_size] - *suffix_loc;
  } else {
    /* *suffix_loc is NULL, so this is the first attempt to replace the suffice
       on this file name. */
    /* Find the start of the suffix by backing up from the end of file_name
       until the suffix delimiter is located.  Start from the character
       position immediately before the trailing NULL.   This search is
       intended to handle file names of the following formats "aaa.xxx",
       "aaa/bbb.xxx", "aaa.", and "aaa/bbb."; in each case *suffix_loc should
       point to the character position immediately following the period.  In
       addition, it should point to the position just past the end of the file
       name if no delimiter if found before reaching either the start of
       file_name or a '/' -- i.e., cases like "aaa" and "aaa/bbb", to which
       the suffix (with delimiter) will simply be appended. */
    curr_suffix_length = 0;
    for (ch = &file_name[curr_file_name_size-1]; ch >= file_name; --ch) {
      if (*ch == SUFFIX_DELIMITER) {
        /* Make *suffix_loc point just past the delimiter. */
        *suffix_loc = ch + 1;
        break;
      }  /* if */
      if (*ch == '/' || ch == file_name) {
        /* file_name has no suffix.  A delimiter character will be added to
           the end of file_name and then the suffix will be appended. */
        suffix_delim_required = TRUE;
        *suffix_loc = &file_name[curr_file_name_size];
        curr_suffix_length = 0;
        break;
      }  /* if */
      /* Increment curr_suffix_length for each iteration of the loop. */
      ++curr_suffix_length;
    }  /* for */
  }  /* if */
  /* The base size of the new file name is the size when the current
     file name without its suffix. */
  new_file_name_base_size = curr_file_name_size - curr_suffix_length;
  /* The total size of the new file name is the base size plus the new
     suffix plus 1 for the delimiter, if required. */
  new_file_name_size = new_file_name_base_size + strlen(new_suffix) +
                       (sizeof_t)(suffix_delim_required ? 1 : 0);
  if ((file_name == buffer && new_file_name_size > (sizeof_t)buffer_size) ||
      (file_name != buffer && new_file_name_size > curr_file_name_size)) {
#if DEBUG
    if (debug_level >= 5) {
      fprintf(f_debug, "allocating new storage, size = %d\n",
                       (int)(new_file_name_size+1));
    }  /* if */
#endif /* DEBUG */
    /* We need to allocate new storage for the file name. */
    new_file_name = (char *)alloc_il(new_file_name_size+1);
    /* Copy the file name, minus the current suffix. */
    (void)memcpy(new_file_name, file_name,
                 size_t_arg(new_file_name_base_size));
    *suffix_loc = &new_file_name[new_file_name_base_size];
  } else {
    /* We can do the replacement "in place". */
    new_file_name = file_name;
  }  /* if */
  if (suffix_delim_required) {
    /* Add the delimiter, if required. */
    **suffix_loc = SUFFIX_DELIMITER;
    (*suffix_loc)++;
  };
  /* Add the new suffix to new_file_name. */
  strcpy(*suffix_loc, new_suffix);
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "new file name = \"%s\"\n", new_file_name);
  }  /* if */
#endif /* DEBUG */
  db_exit();
  /* Return a pointer to the new file name. */
  return new_file_name;
#undef SUFFIX_DELIMITER
}  /* replace_file_name_suffix */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */


a_boolean get_file_modification_time(char   *file_name,
                                     time_t *time)
/*
Determine whether a file exists, and if so, return the last modification
time.  Return TRUE if the file exists and is a regular file, FALSE otherwise.
*/
{
  a_boolean	is_regular = FALSE;
  struct stat   buf;

  /* Check the file type.  Use the stat call instead of fstat because some
     implementations do not have the _file field in the structure. */
  if (stat(file_name, &buf) == 0) {
    /* Use the POSIX S_ISREG if it is defined.  Otherwise use the
       non-POSIX test using S_IFREG. */
#ifdef S_ISREG
    is_regular = S_ISREG(buf.st_mode);
#else /* ifndef S_ISREG */
    is_regular = ((buf.st_mode & S_IFREG) != 0);
#endif /* ifdef S_ISREG */
    if (is_regular && time != NULL) *time = buf.st_mtime;
  } else {
    /* If the file doesn't exist, set the time to zero just to be neat. */
    if (time != NULL) *time = 0;
  }  /* if */
  return is_regular;
}  /* get_file_modification_time */


a_boolean is_regular_file(char *file_name)
/*
Return TRUE if the specified file is a regular file (i.e., not a
directory or some other kind of special file).
*/
{
  return get_file_modification_time(file_name, (time_t *)NULL);
}  /* is_regular_file */


FILE *open_source_file(char          *file_name,
                       a_boolean     *not_found,
                       a_boolean     *bad_format,
                       a_boolean     *bad_name)
/*
Open the given file as a source input file, and return a pointer to the
file block, or NULL if the file cannot be opened.  In the error case,
one of the three flags is set to indicate the type of error: file not found,
file found but it has a format inappropriate for a source file, or syntax
of the file name is bad.
*/
{
  FILE        *temp_file;

#if DEBUG
  if (debug_level >= 2) {
    fprintf(f_debug, "About to open %s\n", file_name);
  }  /* if */
#endif /* DEBUG */
  *not_found = *bad_format = *bad_name = FALSE;
  if (strlen(file_name) == 0) {
    *bad_name = TRUE;
    temp_file = NULL;
  } else if ((temp_file = fopen(file_name, FOPEN_MODE_FOR_READ)) == NULL) {
    *not_found = TRUE;
  }  else {
    /* File opened okay. */
    /* Check the file type. */
    if (!is_regular_file(file_name)) {
      /* Not a "regular" file. */
      *bad_format = TRUE;
      (void)fclose(temp_file);
      temp_file = NULL;
    }  /* if */
  }  /* if */
  return(temp_file);
}  /* open_source_file */


FILE *reopen_source_file(char *file_name)
/*
Reopen the source file of the given name, and return a pointer to
the file block, or NULL if the file cannot be opened.  Since the file has
previously been opened, the open should fail only under unusual and
serious circumstances, such as the file having been deleted during
the compilation.  This is a subroutine (instead of just an fopen call)
so that any necessary system-specific code can be inserted.
*/
{
  return(fopen(file_name, FOPEN_MODE_FOR_READ));
}  /* reopen_source_file */


static a_boolean okay_as_output_file(char *file_name)
/*
Return TRUE if the given file name is acceptable as an output file.
This involves (potentially) not just checks on the file system permissions,
but checks on whether the file name suffix is something a compiler should be
writing.  This helps avoid problems with clobbering of input files.
*/
{
  a_boolean okay = FALSE;
  char      *last_slash, *name_start, *last_dot;

  if (primary_source_file_name != NULL &&
      strcmp(file_name, primary_source_file_name) == 0) {
    /* Name is the same as the primary source file name, so it's not okay.
       This catches cases where the primary source file does not have a
       .c suffix.  Note, however, that it won't catch cases where the
       source file name and output file name are the same file but
       written in different ways, as for example with different but
       equivalent directory names. */
    okay = FALSE;
  } else {
    /* Find the base name by removing the directory and suffix. */
    last_slash = end_of_directory_name(file_name);
    if (last_slash == NULL) {
      /* No directory name, start of file name is start of base name. */
      name_start = file_name;
    } else {
      /* Start of base name is after the directory name. */
      name_start = last_slash + 1;
    }  /* if */
    /* Find suffix, if any. */
    if ((last_dot = strrchr(name_start, '.')) == NULL) {
      /* No suffix. */
      okay = TRUE;
    } else {
      /* Has suffix.  Check for ".f", ".c", and ".a" and disallow those. */
      if (strcmp(last_dot, ".a") == 0 ||
          strcmp(last_dot, ".f") == 0 ||
          (strcmp(last_dot, ".c") == 0)) {
        okay = FALSE;
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
        /* However, the generated C suffix is allowed if using c_gen_be. */
        { int	suffix_start = strlen(name_start) - sizeof(GEN_C_FILE_SUFFIX);
          if (suffix_start >= 0 &&
              strcmp(name_start + suffix_start + 1, GEN_C_FILE_SUFFIX) == 0) {
            okay = TRUE;
          }  /* if */
       }
#endif /* BACK_END_IS_C_GEN_BE || ... */
      } else {
        okay = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  
  return(okay);
}  /* okay_as_output_file */


FILE *open_output_file(char          *file_name,
                       a_boolean     binary_file,
                       a_boolean     update_mode,
                       a_boolean     *cannot_open,
                       a_boolean     *bad_name)
/*
Open the given file as an output file, and return a pointer to the
file block, or NULL if the file cannot be opened.  In the error case,
one of the two flags is set to indicate the type of error: file could
not be opened or the file name is bad (incorrectly formed or has an
illegal suffix).  binary_file is TRUE if the file should be opened as
a binary file instead of a text file.  update_mode is TRUE if the file
should be opened in update mode so it can be read as well as written.
*/
{
  FILE *temp_file;
  char *mode;

#if DEBUG
  if (debug_level >= 2) {
    fprintf(f_debug, "About to open output file %s\n", file_name);
  }  /* if */
#endif /* DEBUG */
  *cannot_open = *bad_name = FALSE;
  if (!okay_as_output_file(file_name)) {
    *bad_name = TRUE;
    temp_file = NULL;
  } else {
    if (update_mode) {
      mode = binary_file ? FOPEN_MODE_FOR_BINARY_UPDATE :
                           FOPEN_MODE_FOR_UPDATE;
    } else {
      mode = binary_file ? FOPEN_MODE_FOR_BINARY_WRITE : FOPEN_MODE_FOR_WRITE;
    }  /* if */
    temp_file = fopen(file_name, mode);
    if (temp_file == NULL) *cannot_open = TRUE;
  }  /* if */

  return(temp_file);
}  /* open_output_file */


FILE *open_input_file(char          *file_name,
                      a_boolean     binary_file)
/*
Open the given file as an input file, and return a pointer to the
file block, or NULL if the file cannot be opened.  binary_file is TRUE if
the file should be opened as a binary file instead of a text file.
If the file can be opened and is a regular file, return the FILE pointer,
otherwise return NULL.
*/
{
  FILE *temp_file;
  char *mode;

#if DEBUG
  if (debug_level >= 2) {
    fprintf(f_debug, "About to open input file %s\n", file_name);
  }  /* if */
#endif /* DEBUG */
  if (!is_regular_file(file_name)) {
    temp_file = NULL;
  } else {
    mode = binary_file ? FOPEN_MODE_FOR_BINARY_READ :
                         FOPEN_MODE_FOR_READ;
    temp_file = fopen(file_name, mode);
  }  /* if */
  return(temp_file);
}  /* open_input_file */


void reopen_error_output_file(char          *file_name,
                              a_boolean     *cannot_open,
                              a_boolean     *bad_name)
/*
Reopen stderr (the standard error output file).  If the file cannot be
opened, stderr is left as it was, and one of the two flags is set to
indicate the type of error: file could not be opened or the file name
is bad (incorrectly formed or has an illegal suffix).
*/
{
  *cannot_open = *bad_name = FALSE;
  if (!okay_as_output_file(file_name)) {
    *bad_name = TRUE;
  } else {
#if __VMS__
    /* Under VMS, if we opened the file twice we would create two versions,
       so we don't do that. */
#else /* !__VMS__ */
    /* Open first as a normal file, so if the open fails we still have
       stderr as it was. */
    { FILE *temp_file = fopen(file_name, FOPEN_MODE_FOR_WRITE);
      if (temp_file == NULL) {
        *cannot_open = TRUE;
      } else {
        (void)fclose(temp_file);
      }  /* if */
    }
#endif /* __VMS__ */
    if (!*cannot_open) {
      if (freopen(file_name, FOPEN_MODE_FOR_WRITE, stderr) == NULL) {
        /* Unlikely but possible -- something's changed since the file was
           opened before.  We probably will terminate without being able
           to write a message, since stderr is closed. */
        *cannot_open = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
}  /* reopen_error_output_file */


#if IL_SHOULD_BE_WRITTEN_TO_FILE || AUTOMATIC_TEMPLATE_INSTANTIATION
void delete_file(char *file_name)
/*
Delete the file with the indicated name.  It shouldn't be open currently.
*/
{
  int status;
#if __ANSIC__
  status = remove(file_name);
#else /* __ANSIC__ */
#if __VMS__
  EXTERN_C int delete();
  status = delete(file_name);
#else /* !__VMS__ */
  EXTERN_C int unlink();
  status = unlink(file_name);
#endif /* __VMS__ */
#endif /* __ANSIC__ */
  if (status != 0) {
    str_catastrophe(ec_file_delete_error, file_name);
  }  /* if */
}  /* delete_file */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE || AUTOMATIC_TEMPLATE_INSTANTIATION */


#if NEED_TEMP_FILES
#if __MSDOS__
/*
Data structure used to keep a list of open temporary files under MS-DOS,
in order to get their names to close them.
*/
typedef struct a_temp_file_name *a_temp_file_name_ptr;
typedef struct a_temp_file_name {
  a_temp_file_name_ptr
		next;	/* Next entry on the list, or NULL if this is the
			   last entry. */
  char		*name;	/* Null-terminated file name string. */
  FILE		*file;	/* File pointer. */
} a_temp_file_name;
static a_temp_file_name_ptr
		open_temp_files = NULL;
			/* List of all temp files currently open. */
			/* This doesn't have to be reset by fe_init. */
#endif /* __MSDOS__ */


FILE *open_temp_file(a_boolean binary_file)
/*
Open a temporary text file, and return a pointer to its file block.  The
file should be a binary file if binary_file is TRUE.
*/
{
  static char *temp_dir = NULL;
  static unsigned long seed = 0;
#define TEMP_NAME_BUFFER_SIZE 150
  char        buffer[TEMP_NAME_BUFFER_SIZE];
  a_boolean   need_slash;
  sizeof_t    dir_len;
  FILE        *temp_file;
  int         retry_count = 20;
  struct stat buf;

  /* Get the value of the "TMPDIR" environment variable, the directory to
     be used for temporary files.  Get it only once (temp_dir is static). */
  if (temp_dir == NULL) {
    temp_dir = getenv("TMPDIR");
    if (temp_dir == NULL || strlen(temp_dir) == 0) temp_dir = DEFAULT_TMPDIR;
  }  /* if */
  dir_len = strlen(temp_dir);
  /* See if a slash must be added to the directory name. */
  need_slash = (temp_dir[dir_len-1] != '/');
#if __MSDOS__
  /* Under MS-DOS we don't need to add a slash if the path already ends with
     a backslash. */
  if (need_slash && temp_dir[dir_len-1] != '\\') need_slash = TRUE;
#endif /* __MSDOS__ */
  do {
    /* Put together the name dir + "/edg" + seed + "_" + process id.  See if
       that will fit in the buffer. */
    if (dir_len + need_slash + 24 > TEMP_NAME_BUFFER_SIZE) {
      str_catastrophe(ec_temp_file_dir_name_too_long, temp_dir);
    }  /* if */
    (void)sprintf(buffer, "%s%sedg%lu_%d", temp_dir, 
					   need_slash ? "/" : "", seed++,
                                           getpid());
#if DEBUG
    if (debug_level >= 4) {
      fprintf(f_debug, "Opening temporary file %s\n", buffer);
    }  /* if */
#endif /* DEBUG */
    /* Check to see if the file exists already.  If so, go on to the next
       seed value. */
    if (stat(buffer, &buf) == 0) {
      /* The file exists already. */
    } else {
      /* The file does not exist.  Try opening it. */
      temp_file = fopen(buffer, binary_file ? FOPEN_MODE_FOR_BINARY_UPDATE :
                                              FOPEN_MODE_FOR_UPDATE);
      if (temp_file != NULL) goto have_file;
    }  /* if */
    /* Retry with incremented file names a certain number of times.  After
       that, give up (the problem may be that the directory name is bad). */
  } while (retry_count-- > 0);
  str_catastrophe(ec_cannot_open_temp_file, buffer);
have_file:;
#if __MSDOS__
  /* Can't delete the file now, so add it to the list of files to be cleaned
     up. */
  /* Use general storage for the allocation, because it may have to survive
     into a back end called in the same program as the front end. */
  { a_temp_file_name_ptr new_entry =
                 (a_temp_file_name_ptr)alloc_general(sizeof(a_temp_file_name));
    new_entry->name = strcpy(alloc_general((sizeof_t)(strlen(buffer)+1)),
                             buffer);
    new_entry->file = temp_file;
    new_entry->next = open_temp_files;
    open_temp_files = new_entry;
  }
#else /* !__MSDOS__ */
  /* Delete the file now, so it will disappear when closed. */
  (void)unlink(buffer);
#endif /* __MSDOS__ */
  return(temp_file);
}  /* open_temp_file */


void close_temp_file(FILE *temp_file)
/*
Close and delete the indicated temporary file.
*/
{
  (void)fclose(temp_file);
#if __MSDOS__
  { a_temp_file_name_ptr tfnp, prev_tfnp;
    /* Find the entry for this file on the list of open files. */
    for (prev_tfnp = NULL, tfnp = open_temp_files;
         tfnp != NULL;
         prev_tfnp = tfnp, tfnp = tfnp->next) {
      if (tfnp->file == temp_file) {
        /* Found it; delete the file and remove the entry from the list.
           The space for the entry is just lost, but that's not a big issue. */
        (void)unlink(tfnp->name);
        if (prev_tfnp == NULL) {
          /* First entry on list. */
          open_temp_files = tfnp->next;
        } else {
          prev_tfnp->next = tfnp->next;
        }  /* if */
        goto close_done;
      }  /* if */
    }  /* for */
  }
#if CHECKING
  internal_error("close_temp_file: file not on list");
#endif /* CHECKING */
close_done:;
#else /* !__MSDOS__ */
  /* The file was unlinked when opened, and therefore was deleted automatically
     when closed. */
#endif /* __MSDOS__ */
}  /* close_temp_file */


#if __MSDOS__
static void close_all_temp_files(void)
/*
Close and delete all open temporary files.
*/
{
  while (open_temp_files != NULL) {
    close_temp_file(open_temp_files->file);
  }  /* while */
}  /* close_all_temp_files */
#endif /* __MSDOS__ */
#endif /* NEED_TEMP_FILES */


#if COMPILE_MULTIPLE_SOURCE_FILES
void identify_source_file(void)
/*
Identify the source file being compiled, when more than one file name
appears on the command line.
*/
{
#if !USING_DRIVER
  if (more_than_one_source_file) {
    fprintf(stderr, "%s:\n", primary_source_file_name);
  }  /* if */
#endif /* !USING_DRIVER */
}  /* identify_source_file */
#endif /* COMPILE_MULTIPLE_SOURCE_FILES */


#if STANDALONE_UTILITY_PROGRAM
DOES_NOT_RETURN normal_termination(void)
/*
Terminate a utility program normally.  This is used by the C-generating back 
end and il_display when they are compiled as standalone programs.
This routine does not return.
*/
{
  exit(RC_NORMAL);
  /*NOTREACHED*/
}  /* normal_termination */
#endif /* STANDALONE_UTILITY_PROGRAM */


void write_signoff(void)
/*
Write a compilation signoff message, giving the count of errors.
Only write the signoff if there ARE errors, and if we are supposed to.
*/
{
#if WRITE_SIGNOFF_MESSAGE && !STANDALONE_UTILITY_PROGRAM
  if (total_errors + total_catastrophes > 0) {
    if (total_errors > 0) {
      fprintf(stderr, "%lu error%s", total_errors,
                      (total_errors != 1) ? "s" : "");
      if (total_catastrophes > 0) {
        fputs(" and ", stderr);
      }  /* if */
    }  /* if */
    if (total_catastrophes > 0) {
      fprintf(stderr, "%lu catastrophic error%s", total_catastrophes,
                      (total_catastrophes != 1) ? "s" : "");
    }  /* if */
    if (primary_source_file_name != NULL &&
        strlen(primary_source_file_name) != 0 &&
        strcmp(primary_source_file_name, FILE_NAME_FOR_STDIN) != 0) {
      fprintf(stderr, " detected in the compilation of \"%s\".\n",
                      primary_source_file_name);
    } else {
      /* Source file name is not known. */
      fputs(" detected in this compilation.\n", stderr);
    }  /* if */
  }  /* if */
#endif /* WRITE_SIGNOFF_MESSAGE && !STANDALONE_UTILITY_PROGRAM */
}  /* write_signoff */


DOES_NOT_RETURN exit_compilation(an_error_severity severity)
/*
Exit the compilation.  severity indicates the severity of the most
severe diagnostic issued in this compilation.  This routine does not return.
*/
{
#if !USING_DRIVER && !STANDALONE_UTILITY_PROGRAM
  /* For the more serious severities, write a message about the abrupt
      termination. */
  if (severity == es_catastrophe || severity == es_command_line_error) {
    fprintf(stderr, "Compilation terminated.\n");
  } else if (severity == es_internal_error) {
    fprintf(stderr, "Compilation aborted.\n");
  }  /* if */
#endif /* !USING_DRIVER && !STANDALONE_UTILITY_PROGRAM */

  /* Actually terminate the compilation with the proper return code. */
  switch (severity) {
    case es_none:
    case es_remark:
      exit(RC_NORMAL);
    case es_warning:
      exit(RC_WARNING);
    case es_error:
      exit(RC_ERROR);
    case es_catastrophe:
    case es_command_line_error:
      exit(RC_CATASTROPHE);
    case es_internal_error:
    default:
      (void)fflush(stderr);
      abort();
  }  /* switch */
  /*NOTREACHED*/
}  /* exit_compilation */


DOES_NOT_RETURN term_compilation(an_error_severity severity)
/*
Terminate the compilation.  This is always called as the last thing in the
compilation.  severity indicates the error severity.  This routine does
not return.
*/
{
  /* Write a signoff message (with count of errors) if necessary. */
  write_signoff();
  /* Exit from the compilation. */
  exit_compilation(severity);
  /*NOTREACHED*/
}  /* term_compilation */


/*ARGSUSED*/ /* <-- Because "sig" is not used. */
static a_signal_handler_return_value term_on_signal(int sig)
/*
Routine set up as a signal handler, called to terminate compilation on
receipt of a signal.
*/
{
#if !USING_DRIVER
  /* Print newline to make console output clean. */
  fprintf(stderr, "\n");
#endif /* !USING_DRIVER */
  term_compilation(es_catastrophe);
  /*NOTREACHED*/
}  /* term_on_signal */


void set_signal_handlers(void)
/*
Enable any signal handlers necessary to catch signals that may come up during
execution of the front end (for example, SIGINT).
*/
{
  (void)signal(SIGINT, term_on_signal);
  (void)signal(SIGTERM, term_on_signal);
#if __MSDOS__ && NEED_TEMP_FILES
  /* Under MS-DOS, establish an atexit routine to close and delete all
     temporary files. */
  if (atexit(close_all_temp_files) != 0) {
#if CHECKING
    internal_error("set_signal_handlers: could not set atexit handler");
#endif /* CHECKING */
  }  /* if */
#endif /* __MSDOS__ && NEED_TEMP_FILES */
}  /* set_signal_handlers */

#ifdef NEED_SIZE_T_ARG_ERROR

/* The extern declaration for this routine is in basics.h. */
true_size_t size_t_arg_error(void)
/*
Called from the macro size_t_arg to abort the front end if a size is being
truncated.  This is used on machines that have a small size_t (say, 16 bits)
when we choose to make sizeof_t something longer.
*/
{
  catastrophe(ec_program_too_large);
  /* The return statement suppresses warnings from compilers that don't
     process lint comments. */
  /*NOTREACHED*/
  return 0;
}  /* size_t_arg_error */

#endif /* ifdef NEED_SIZE_T_ARG_ERROR */


int smemcmp(char     *s1,
            char     *s2,
            sizeof_t length)
/*
Like the standard memcmp, but guaranteed to compare the characters
sequentially and read no more characters than necessary.  Used when one
does not know that both strings are at least as long as the indicated length.
*/
{
  int cmp = 0;

  for (; length > 0; length--) {
    cmp = ((unsigned char)(*s1++) - (unsigned char)(*s2++));
    if (cmp != 0) break;
  }  /* for */
  return cmp;
}  /* smemcmp */


#if __VMS__
/* VMS doesn't have block copy routines, so define them. */

int memcmp(register char *s1, register char *s2, register true_size_t length)
{
  return smemcmp(s1, s2, (sizeof_t)length);
}  /* memcmp */


char *memcpy(register char *to,
             register char *from,
             register true_size_t length)
{
  register char *origto = to;
  while (length--) *to++ = *from++;
  return (origto);
}  /* memcpy */


char *memset(register char *ptr,
             register int val,
             register true_size_t length)
{
  while (length--) *ptr++ = val;
} /* memset */

#endif /* __VMS__ */


static a_cpu_time get_cpu_time(void)
/*
Returns the amount of CPU time used by this process (and any child processes)
since the start of the compilation.  The value is converted to milliseconds.
*/
{
#if __ANSIC__
  /* This version of the routine uses the ANSI/ISO C compliant version of the
     clock function. */
  clock_t	cpu_time;
  double	temp;

  cpu_time = clock();
  /* clock() returns a value in units of CLOCKS_PER_SEC.  Convert this value
     to milliseconds. */
  temp = cpu_time;
  temp = (cpu_time * 1000) / CLOCKS_PER_SEC;
  cpu_time = (clock_t)temp;
  return cpu_time;
#else /* !__ANSIC__ */
  /* This version uses the UNIX routines to get the CPU time. */
  clock_t	cpu_time = 0;
  double	temp;
  struct tms	buffer;

  if (times(&buffer) != -1) {
    cpu_time = buffer.tms_utime + buffer.tms_stime +
               buffer.tms_cutime + buffer.tms_cstime;
    /* times() returns a value in unspecified units.  CLOCK_FREQUENCY should
       be defined by host_envir.h to the appropriate value.  Convert this
       value to milliseconds. */
    temp = cpu_time;
    temp = (cpu_time * 1000) / CLOCK_FREQUENCY;
    cpu_time = temp;
  }  /* if */
  return cpu_time;
#endif /* __ANSIC__ */
}  /* get_cpu_time */


static a_real_time get_time(void)
/*
Return the current wall clock time normalized to seconds.
*/
{
  return ((a_real_time)time((time_t*)NULL));
}  /* get_time */


void get_timer(a_timer	*timer)
/*
Gets the current CPU time and wall clock time and returns the result
to the caller.
*/
{
  timer->cpu_time = get_cpu_time();
  timer->real_time = get_time();
}  /* get_timer */

static void calc_time_difference(a_timer_ptr	start_time,
			         a_timer_ptr	end_time,
			         double		*cpu_time,
			         double		*real_time)
/*
Given a starting and ending timer, return the elapsed time and CPU
time in seconds.
*/
{
#if __ANSIC__
  /* If available, use the ANSI routine to compute the difference between
     the two real times. */
  *real_time = difftime(end_time->real_time, start_time->real_time);
#else /* !__ANSIC__ */
  /* The real times are assumed to be stored in seconds. */
  *real_time = end_time->real_time - start_time->real_time;
#endif /* __ANSIC__ */
  *cpu_time = ((double)(end_time->cpu_time) -
              ((double)start_time->cpu_time)) / 1000;
}  /* calc_time_difference */


void display_time_used(char		*message,
		       a_timer_ptr	start_time,
		       a_timer_ptr	end_time)
/*
Display the difference in CPU time and elapsed time between two timers.
*/
{
  double	cpu_time;
  double	real_time;

  calc_time_difference(start_time, end_time, &cpu_time, &real_time);
  fprintf(stderr, "%-30s %10.2f (CPU) %10.2f (elapsed)\n", message,
          cpu_time, real_time);
}  /* display_time_used */


void get_file_identifier(char			*file_name,
			 a_file_identifier_ptr	id)
/*
Return an identifier that can be used to determine whether two files
are the same.  On systems that have inode numbers, the identifier
contains the device and inode numbers.  On other systems, the file name
must be used to do the comparison.  On these systems, the file identifier
is simply used to store the length of the file name.  The length is
used as an initial test before comparing the strings.
*/
{
#if STAT_INFORMATION_INCLUDES_INODE
  struct stat	buf;
  if (strcmp(file_name, FILE_NAME_FOR_STDIN) == 0) {
    id->st_dev = 0;
    id->st_ino = 0;
    id->is_stdin = TRUE;
  } else if (stat(file_name, &buf) == 0) {
    id->st_dev = buf.st_dev;
    id->st_ino = buf.st_ino;
    id->is_stdin = FALSE;
  } else {
    unexpected_condition_str2("get_file_identifier:", "stat() failed");
  }  /* if */  
#else /* !STAT_INFORMATION_INCLUDES_INODE */
  /* We are using the file name for comparison.  Just store the length
     in the identifier to be used as an initial test. */
  *id = strlen(file_name);
#endif /* STAT_INFORMATION_INCLUDES_INODE */
}  /* get_file_identifier */


/* Header comment for get_file_name_from_curr_dir. */
/*
Get the name of the next file in the current directory.  If first
is TRUE, then this is the first call.  Returns a pointer to the file
name string.  The value returned is only valid until the next call of
this routine.  A NULL pointer is returned when there are no more directory
entries.
*/
#if __WIN32__
/*
WIN32 (e.g., Windows-NT) version.
*/

#include <io.h>

char *get_file_name_from_curr_dir(a_boolean first)
{
  static long			handle;
  static struct _finddata_t	fileinfo;
  char				*result;

  if (first) {
    /* On the first call, use the _findfirst call that specifies which
       files are to be returned.  "handle" is saved in a static variable
       that is used on subsequent calls to get the remaining directory
       entries. */
    handle = _findfirst("*.*", &fileinfo);
    if (handle < 0) {
      /* Directory could not be opened, or is empty. */
      result = NULL;
    } else {
      result = fileinfo.name;
    }  /* if */
  } else {
    /* On subsequent calls, use _findnext to find the next file that
       matches the pattern. */
    if (_findnext(handle, &fileinfo) < 0) {
      /* Returns -1 when there are no more files. */
      result = NULL;
    } else {
      result = fileinfo.name;
    }  /* if */
  }  /* if */
  return result;
}  /* get_file_name_from_curr_dir */
#else /* !__WIN32__ */
#if __MSDOS__
/*
DOS version.
*/

#include <dos.h>

char *get_file_name_from_curr_dir(a_boolean first)
{
  static struct _find_t	fileinfo;
  char			*result;

  if (first) {
    /* On the first call, use the _dos_findfirst call that specifies which
       files are to be returned.  The _A_RDONLY attribute causes
       both normal and read-only files to be returned. */
    if (_dos_findfirst("*.*", _A_RDONLY, &fileinfo) != 0) {
      /* Directory could not be opened, or is empty. */
      result = NULL;
    } else {
      result = fileinfo.name;
    }  /* if */
  } else {
    /* On subsequent calls, use _dos_findnext to find the next file that
       matches the pattern. */
    if (_dos_findnext(&fileinfo) != 0) {
      /* Returns non-zero when there are no more files. */
      result = NULL;
    } else {
      result = fileinfo.name;
    }  /* if */
  }  /* if */
  return result;
}  /* get_file_name_from_curr_dir */
#else /* !__MSDOS__ */

/*
UNIX Version.
*/
#include <sys/types.h>
#include <dirent.h>
#include <sys/dirent.h>

char *get_file_name_from_curr_dir(a_boolean first)
/*
See comment above.
*/
{
  static DIR		*dir;
  static struct dirent	*dir_entry;
  char			*result;

  if (first) {
    /* Open the directory and save the directory pointer in a static
       variable that can be used on subsequent calls. */
    dir = opendir(".");
    check_assertion(dir != NULL);
  }  /* if */
  dir_entry = readdir(dir);
  if (dir_entry == NULL) {
    /* The last entry was read. */
    (void)closedir(dir);
    result = NULL;
  } else {
    result = dir_entry->d_name;
  }  /* if */
  return result;
}  /* get_file_name_from_curr_dir */
#endif /* __MSDOS__ */
#endif /* __WIN32__ */


char *get_curr_dir_name(void)
/*
Get the current directory name and return it in the temporary string
buffer.
*/
{
#if USE_GETCWD
  for (;;) {
    /* The temporary buffer may not be allocated yet.  Make sure there
       is some space allocated. */
    ensure_temp_text_buffer_space(256);
    if (getcwd(temp_text_buffer, size_temp_text_buffer) == NULL) {
      if (errno == ERANGE) {
        /* We know the buffer is too small, but we don't know how much
           more space we need.  Add a little space and try again. */
        ensure_temp_text_buffer_space(size_temp_text_buffer + 256);
        continue;
      }  /* if */
    }  /* if */
    break;
  }  /* for */
  return temp_text_buffer;
#else /* !USE_GETCWD */
  /* Make sure there is enough space for the largest path name that can
     be returned. */
  ensure_temp_text_buffer_space(MAXPATHLEN);
  (void)getwd(temp_text_buffer);
  return temp_text_buffer;
#endif /* USE_GETCWD */
}  /* get_curr_dir_name */


#if USE_MMAP_FOR_MEMORY_REGIONS
#include <sys/mman.h>

#if __BSD__
extern int getpagesize(void);
#endif /* __BSD__ */

static int	page_size = 0;
			/* The size of a host page.  Memory mapped blocks must
			   be requested in increments of this size. */


static int get_page_size(void)
/*
Return the size of a host page.  When map_file_region is called,
incremental_size must be a multiple of the page size.
*/
{
  int	page_size;
#if __BSD__
  page_size = getpagesize();
#else /* !__BSD__ */
  page_size = sysconf(_SC_PAGESIZE);
#endif /* __BSD__ */
  check_assertion_str2(page_size > 0, "get_page_size:", "invalid page size");
  return page_size;
}  /* get_page_size */


sizeof_t do_page_alignment(sizeof_t size)
/*
Return "size" adjusted as needed to be a multiple of the system page size.
*/
{
  sizeof_t	size2;

  /* On the first call of this routine, get the host page size. */
  if (page_size == 0) page_size = get_page_size();
  size2 = (size / page_size) * page_size;
  if (size2 < size) size2 += page_size;
  return size2;
}  /* do_page_alignment */


a_void_ptr map_file_region(FILE		*file,
                           sizeof_t	curr_size,
		           sizeof_t	incremental_size)
/*
Expand a memory mapped file.  This routine assumes that curr_size bytes
have already been allocated and mapped, and that incremental_size bytes
should be added.
*/
{
  int		fd = file->_file;
  caddr_t	addr = NULL;
  sizeof_t	size;

  size = curr_size + incremental_size;
  /* The file must be large enough to contain the mapped area. */
  if (fseek(file, (long)size, SEEK_SET) == 0) {
    /* Write a character at the last allocated position and
       make sure the write to the file is actually done. */
    if (fputc(0, file) != EOF && fflush(file) == 0) {
      /* An extra byte is added to the size to stop CodeCenter from complaining
         about the after_end_of_block comparison in mem_manage.c. */
      addr = (a_void_ptr)mmap((char*)0, incremental_size + 1,
                              PROT_WRITE | PROT_READ, MAP_PRIVATE,
                              fd, (off_t)curr_size);
#if DEBUG
      if (debug_level >= 5) {
        fprintf(f_debug, "Allocated %lu bytes of mmap memory at %p\n",
                (unsigned long)incremental_size, addr);
      }  /* if */
#endif /* DEBUG */
      /* mmap returns (caddr_t)-1 if the operation fails. */
      if (addr == (caddr_t)-1) addr = NULL;
    }  /* if */
  }  /* if */
  return addr;
}  /* map_file_region */


a_void_ptr map_input_file_to_region(FILE		*file,
                                    sizeof_t		offset,
				    sizeof_t		size,
				    a_void_ptr		address)
/*
Map the data pointed to by "file", starting at "offset" bytes,
for "size" bytes to the address specified by "address".
This mapping is done as a private mapping so that any changes to
the data will be local.  This is used to map a section of a PCH
file to a memory region.
*/
{
  int		fd = file->_file;
  a_void_ptr	result_addr;

  result_addr = (a_void_ptr)mmap(address, size,
                            PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_FIXED,
                            fd, (off_t)offset);
  /* mmap returns (cresult_addr_t)-1 if the operation fails. */
#if CHECKING
  if (result_addr == (caddr_t)-1) {
    fprintf(f_debug, "Map failed: address=%p, size=%lu, offset=%lu\n",
            address, (unsigned long)size, (unsigned long)offset);
  }  /* if */
#endif /* CHECKING */
  if (result_addr == (caddr_t)-1) result_addr = NULL;
  return result_addr;
}  /* map_input_file_to_region */


sizeof_t seek_to_page_alignment(FILE *file)
/*
Seeks to the next position in the file that is a multiple of the
host page size.  This is used to ensure that data written to a file
is at an offset that can be used as an argument to mmap.  Return the
current file position.
*/
{
  sizeof_t	curr_pos;

  curr_pos = (sizeof_t)ftell(file);
  curr_pos = do_page_alignment(curr_pos);
  if (fseek(file, (long)curr_pos, SEEK_SET) != 0) {
    unexpected_condition_str("seek_to_page_alignment: fseek error");
  }  /* if */
  return curr_pos;
}  /* seek_to_page_alignment */

#endif /* USE_MMAP_FOR_MEMORY_REGIONS */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1991 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
