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

This version for UNIX, MS-DOS, VAX/VMS, and Windows NT.

*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

#if EDG_WIN32
#include <windows.h>
#endif /* EDG_WIN32 */

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
#if __ANSIC__ || __MICROSOFT_OS__
/* ANSI C allows binary modes.  So does MS-DOS. */
#define FOPEN_MODE_FOR_READ "r"
#define FOPEN_MODE_FOR_WRITE "w"
#define FOPEN_MODE_FOR_UPDATE "w+"
#define FOPEN_MODE_FOR_BINARY_WRITE "wb"
#define FOPEN_MODE_FOR_BINARY_UPDATE "w+b"
#define FOPEN_MODE_FOR_BINARY_READ "rb"
#else /* !(__ANSIC__ || __MICROSOFT_OS__) */
/* Assume UNIX (binary and text files the same). */
#define FOPEN_MODE_FOR_READ "r"
#define FOPEN_MODE_FOR_WRITE "w"
#define FOPEN_MODE_FOR_UPDATE "w+"
#define FOPEN_MODE_FOR_BINARY_WRITE "w"
#define FOPEN_MODE_FOR_BINARY_UPDATE "w+"
#define FOPEN_MODE_FOR_BINARY_READ "r"
#endif /* __ANSIC__  || __MICROSOFT_OS__ */
#endif /* READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS */

#if __MICROSOFT_OS__
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
#endif /* __MICROSOFT_OS__ */

/* ANSI signal handlers return void. Older UNIX signal handlers in general,
and SVID compliant signal handlers in particular, return int. */
#ifdef __ANSIC__
#define SIGNAL_HANDLER_RETURNS_VOID 1
#else /* !defined(__ANSIC__) */
#ifdef sun
/* SunOS 4.1 switched to the ANSI form of signal. */
#define SIGNAL_HANDLER_RETURNS_VOID 1
#endif  /* sun */
#ifdef __hpux
#define SIGNAL_HANDLER_RETURNS_VOID 1
#endif  /* __hpux */
#endif  /* __ANSIC__ */

#include <signal.h>
#ifdef SIGNAL_HANDLER_RETURNS_VOID
typedef void a_signal_handler_return_value;
#else /* !defined(SIGNAL_HANDLER_RETURNS_VOID) */
typedef int a_signal_handler_return_value;
#endif /* defined(SIGNAL_HANDLER_RETURNS_VOID) */

#include <errno.h>
#if __BSD__
/* BSD errno.h doesn't define "errno". */
EXTERN_C int errno;
#endif /* __BSD__ */

#ifndef STDLIB_H_INCLUDED
EXTERN_C char *getenv(char *name);
EXTERN_C int abort(void);
EXTERN_C void exit(int status);
#endif /* ifndef STDLIB_H_INCLUDED */

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

#if !__MICROSOFT_OS__
/* If we are not on MS-DOS, we assume that we are on some sort of
   Unix system.  Include unistd.h to get declarations for
   the system calls and library routines. */
#include <unistd.h>
#endif /* !__MICROSOFT_OS__ */

/*
Determine whether getcwd or getwd should be used to get the current
directory.  getwd is used on BSD, getcwd on other systems.
*/
#if __MICROSOFT_OS__
#define USE_GETCWD 1
#include <direct.h>
#else /* !__MICROSOFT_OS___ */
#if __BSD__
#include <sys/param.h>
#if !defined(__cplusplus)
EXTERN_C char* getwd(char *pathname);
#endif /* !defined(__cplusplus) */
#define USE_GETCWD 0
#else /* !__BSD__ */
#include <unistd.h>
#define USE_GETCWD 1
#endif /* __BSD__ */
#endif /* __MICROSOFT_OS__ */

#if __MICROSOFT_OS__
/* Function definitions for MS-DOS compilers. */
#if __TURBOC__ || __ZTC__
/* Neither Turbo C nor Zortech have the getpid call.  Since MSDOS does
   not have multiple tasks, just return a 1. */
static int getpid(void)
{
  return (1);
}
#endif /* __TURBOC__ */
#else /* __MICROSOFT_OS__ */
/* Function definitions for non MS-DOS compilers. */
#ifdef __cplusplus
#include <time.h>
#else /* ifndef __cplusplus */
#if __BSD__
/* The SUN does not have the getpid(), unlink(), and time() calls defined 
   in include files. This is only done when not using a C++ compiler.
   In C++ we assume these are supplied by unistd.h, which is included
   above. */
EXTERN_C int getpid(void);
/* Unlink (delete) a file. */
EXTERN_C int unlink(const char *path);
EXTERN_C time_t time(time_t* tloc);
#endif /* __BSD__ */
#endif /* ifdef __cplusplus */
#endif /* __MICROSOFT_OS__ */

/*
Define some maximum lengths for MS-DOS file name handling stuff.  Note that the
defined lengths INCLUDE the terminating null character.  "__MAXDRIVE__" is the
length of the disk drive letter which includes the trailing ":".  "__MAXDIR__"
is the length of the directory part of the path including a trailing slash.
"__MAXFILE__" is the length of the base file name.  "__MAXEXT__" is the length
of the extension including the leading ".".
*/
#if __MICROSOFT_OS__
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
#endif /* __MICROSOFT_OS__ */


/*
Define a macro which takes a complete file path and breaks it up into the parts
as described above.
*/
#if __MICROSOFT_OS__
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
#endif /* __MICROSOFT_OS__ */


static a_directory_name_entry_ptr
		avail_directory_name_entries = NULL;
			/* Available list of directory name entries. */

static a_directory_name_entry_ptr
		template_search_path_tail;
			/* End of the search path to find exported templates.
			   The name strings are in general storage. */

static a_text_buffer_ptr
		dir_and_file_buffer;
			/* A text buffer used by combine_dir_and_file_name.*/


static void free_directory_name_entry(a_directory_name_entry_ptr dnep)
/*
Add dnep to the available list of directory name entries.
*/
{
  dnep->next = avail_directory_name_entries;
  avail_directory_name_entries = dnep;
}  /* free_directory_name_entry */


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
  entry_ptr->system_include_dir = FALSE;
  entry_ptr->next     = NULL;
  return (entry_ptr);
}  /* alloc_directory_name_entry */


void add_to_specified_include_search_path(
			char				*dir_name,
			a_boolean			system_include_dir,
			a_directory_name_entry_ptr	*search_path,
			a_directory_name_entry_ptr	*end_search_path)
/*
Add the indicated directory to the end of the specified include file search
path.  If system_include_dir is TRUE the directory should be marked as
a "system" include directory.  The directory name string should be
allocated in general memory.
*/
{
  a_directory_name_entry_ptr new_search_path;

  new_search_path = alloc_directory_name_entry();
  new_search_path->dir_name = dir_name;
  new_search_path->system_include_dir = system_include_dir;
  new_search_path->next     = NULL;
  if (*search_path == NULL) {
    *search_path = new_search_path;
  } else {
    (*end_search_path)->next = new_search_path;
  }  /* if */
  *end_search_path = new_search_path;
}  /* add_to_specified_include_search_path */


void add_to_include_search_path(char		*dir_name,
				a_boolean	system_include_dir)
/*
Add the indicated directory to the end of the include file search
path.  If system_include_dir is TRUE the directory should be marked as
a "system" include directory.
*/
{
  add_to_specified_include_search_path(dir_name, system_include_dir,
                                       &incl_search_path,
                                       &end_incl_search_path);
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
  add_to_include_search_path(usr_include, /*system_include_dir=*/TRUE);
#endif /* NO_USR_INCLUDE */
#if __VMS__
  /* For VMS, add the current directory to the search path. */
  add_to_front_of_include_search_path("");
#endif /* __VMS__ */
}  /* add_default_include_search_path */


void change_primary_include_search_dir(char *dir_name)
/*
Replace the directory name in the primary include file search path
entry by "dir_name".  The directory name string should be allocated in
general memory.
*/
{
  incl_search_path->dir_name = dir_name;
}  /* change_primary_include_search_dir */


void push_primary_include_search_dir(char *dir_name)
/*
dir_name is the directory of a source file that has just been pushed onto
the input stack.  Adjust the include search path as appropriate, e.g.,
by adding the directory to the front of the search path.  dir_name must
be allocated in general memory.

Note that it is not specified within the ANSI C standard or the ARM what the
search rules should be for nested includes.  By default, the search for
nested includes begins in the source directory of the current input file
(not the source directory of the primary input file).  This is the approach
generally taken by C compilers on UNIX systems.  A "stack-model" variation of
this approach (as employed by Microsoft C compilers) follows from setting
stack_referenced_include_directories to TRUE.
*/
{
  /* The "-I-" option disables these changes. */
  if (put_dir_of_each_opened_source_file_on_incl_search_path) {
    if (stack_referenced_include_directories) {
      /* The new directory becomes the primary include search directory, but
         the current one remains in the search path. */
      add_to_front_of_include_search_path(dir_name);
    } else {
      /* The name in the current primary include search directory (the head of
         list of directory name entries) is simply replaced by dir_name. */
      change_primary_include_search_dir(dir_name);
    }  /* if */
  }  /* if */
}  /* push_primary_include_search_dir */


void pop_primary_include_search_dir(char *dir_name)
/*
The directory name in the primary include file search path should revert to
"dir_name", as the result of popping an include file from the source
input stack.
*/
{
  /* The "-I-" option disables these changes. */
  if (put_dir_of_each_opened_source_file_on_incl_search_path) {
    if (stack_referenced_include_directories) {
      /* The entry of the current primary include search directory is removed
         from the search path, and the resulting primary include search
         directory will correspond to dir_name. */
      a_directory_name_entry_ptr  dnep;

      dnep = incl_search_path;
      incl_search_path = incl_search_path->next;
      check_assertion(incl_search_path != NULL &&
                      (strcmp(incl_search_path->dir_name,dir_name) == 0));
      free_directory_name_entry(dnep);
    } else {
      /* The name in the current primary include search directory (the head of
         list of directory name entries) is simply replaced by dir_name. */
      change_primary_include_search_dir(dir_name);
    }  /* if */
  }  /* if */
}  /* pop_primary_include_search_dir */


void add_to_template_search_path(char		*dir_name)
/*
Add the indicated directory to the end of the template file search
path.  The directory name string should be allocated in general memory.
*/
{
  a_directory_name_entry_ptr dnep;

  dnep = alloc_directory_name_entry();
  dnep->dir_name = dir_name;
  dnep->next     = NULL;
  if (template_search_path == NULL) {
    template_search_path = dnep;
  } else {
    template_search_path_tail->next = dnep;
  }  /* if */
  template_search_path_tail = dnep;
}  /* add_to_template_search_path */


static char *end_of_directory_name(char *file_name)
/*
Return a pointer to the end of the directory part of the indicated file
name, or NULL if there is no directory part.
Note that the following must work for FILE_NAME_FOR_STDIN, which is
used to represent stdin; it must return  NULL.
*/
{
  char *last_slash;
#if __MICROSOFT_OS__
  char *last_backslash;
#endif /* __MICROSOFT_OS__ */

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
    last_slash = strrchr(file_name, DIRECTORY_SEPARATOR);
#if __MICROSOFT_OS__
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
#endif /* __MICROSOFT_OS__ */
#endif /* __VMS__ */
  }  /* if */
  return(last_slash);
}  /* end_of_directory_name */


static char *start_of_file_name(char *file_name)
/*
Return the first character of the file name portion of "file_name"
(i.e., the part after an optional directory name).
*/
{
  char	*result;

  result = end_of_directory_name(file_name);
  /* If there is a directory, use the character after the end of the
     directory name; otherwise, return the file name passed in. */
  result = result == NULL ? file_name : result + 1;
  return result;
}  /* start_of_file_name */


static char *end_of_base_name(char *file_name)
/*
Given a simple file name (with no directory name), return a pointer to
the last character of the file name before the suffix, if any.
*/
{
  char	*last_dot;
  char	*name_end;

  if ((last_dot = strrchr(file_name, '.')) == NULL) {
    /* No suffix, end of base name is the same as end of file name. */
    name_end = file_name + strlen(file_name) - 1;
  } else {
    /* End of base name is before the suffix. */
    name_end = last_dot - 1;
  }  /* if */
  return name_end;
}  /* end_of_base_name */


char *suffix_of(char		*file_name)
/*
Find the suffix of "file_name".  Return a pointer to the beginning
of the suffix.  If the file has no suffix, a pointer to the null-terminator
of the file name is returned.
*/
{
  char	*ptr;

  ptr = end_of_directory_name(file_name);
  if (ptr == NULL) {
    /* There is no directory, use the file name passed in. */
    ptr = file_name;
  } else {
    /* Use the character after the last "/" of the directory name. */
    ptr++;
  }  /* if */
  ptr = end_of_base_name(ptr) + 1;
  return ptr;
}  /* suffix_of */


#if !STANDALONE_UTILITY_PROGRAM
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

#endif /* !STANDALONE_UTILITY_PROGRAM */

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


#if __MICROSOFT_OS__
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
#endif /* __MICROSOFT_OS__ */


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
#if __MICROSOFT_OS__
  /* Check for and truncate file names that are too long for MSDOS 
     to handle. */
  truncate_msdos_filename(der_name);
#endif /* __MICROSOFT_OS__ */
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "derived name = \"%s\".\n", der_name);
  }  /* if */
#endif /* DEBUG */
  return(der_name);
}  /* derived_name */


a_text_buffer_ptr combine_dir_and_file_name(
				char			*dir_name,
				char			*file_name,
				a_text_buffer_ptr	buffer)
/*
Combine the given directory name and file name to make a full path name,
and return a pointer to it.  If buffer is not NULL, the name is constructed
in buffer, otherwise it is constructed in a default buffer (that will be
overwritten by the next call that uses it).
*/
{
  sizeof_t  dir_length;
  a_boolean need_to_add_slash;

  /* If no buffer was specified by the caller, use a default buffer. */
  if (buffer == NULL) {
    /* Allocate the default buffer the first time that it is needed. */
    if (dir_and_file_buffer == NULL) {
      dir_and_file_buffer = alloc_text_buffer(256);
    }  /* if */
    buffer = dir_and_file_buffer;
  }  /* if */
  /* Clear the buffer. */
  reset_text_buffer(buffer);
  /* If the directory name is the current directory, then produce
     a joined name that is just the file name.  This makes for nicer-looking
     file names. */
  dir_length = strlen(dir_name);
  if (dir_length == 0) {
    add_string_to_text_buffer(buffer, file_name);
  } else {
    /* See if a slash will have to be added between the two. */
#if __VMS__
    need_to_add_slash = FALSE;
#else /* !__VMS__ */
    need_to_add_slash = (dir_name[dir_length-1] != DIRECTORY_SEPARATOR);
#if __MICROSOFT_OS__
    /* Under MSDOS, both kinds of slashes need to be checked. */
    need_to_add_slash = need_to_add_slash && (dir_name[dir_length-1] != '\\');
#endif /* __MICROSOFT_OS__ */
#endif /* __VMS__ */
    /* Copy the directory name. */
    add_string_to_text_buffer(buffer, dir_name);
    if (need_to_add_slash) {
      /* Add the slash following the directory name. */
#if __MICROSOFT_OS__
      char	separator_char;
      if (strchr(dir_name, DIRECTORY_SEPARATOR) != NULL) {
	/* The original path uses regular UNIX-style slashes; use one to splice
	   the file and path to make it look consistent. */
        separator_char = DIRECTORY_SEPARATOR;
      } else {
	/* The directory name does not have any UNIX-style slashes or has no
	   slashes at all.  In either case, under MSDOS, use an MSDOS-style
	   slash. */
        separator_char = '\\';
      }  /* if */
      add_char_to_text_buffer(buffer, separator_char);
#else /* __MICROSOFT_OS__ */
      add_char_to_text_buffer(buffer, DIRECTORY_SEPARATOR);
#endif /* __MICROSOFT_OS__ */
    }  /* if */
    /* Add the file name to the directory name. */
    add_string_to_text_buffer(buffer, file_name);
  }  /* if */
  /* Add a null terminator. */
  add_char_to_text_buffer(buffer, '\0');
  return buffer;
}  /* combine_dir_and_file_name */

#if !STANDALONE_UTILITY_PROGRAM

void replace_file_name_suffix(char		*new_suffix,
                              a_text_buffer_ptr	file_name_buffer)
/*
Replace the suffix of a file name with a specified suffix.  The
replacement is done in place in file_name_buffer, which is expanded if
necessary.  This routine may be called iteratively.
*/
{
  sizeof_t   curr_file_name_size;
  sizeof_t   new_suffix_length;
  char	     *suffix_loc;
#define SUFFIX_DELIMITER '.'

  db_enter(5, "replace_file_name_suffix");
#if DEBUG
  if (db_flag_is_set("replace_file_name_suffix")) {
    fprintf(f_debug, "current file_name = \"%s\", new suffix = \"%s\"\n",
            file_name_buffer->buffer, new_suffix);
  }  /* if */
#endif /* DEBUG */
  /* Determine the size of file_name, excluding the trailing NULL. */
  curr_file_name_size = file_name_buffer->size - 1;
  new_suffix_length = strlen(new_suffix);
  check_assertion(curr_file_name_size > 0);
  check_assertion(file_name_buffer->buffer[curr_file_name_size] == '\0');
  suffix_loc = suffix_of(file_name_buffer->buffer);
  /* Update the buffer to specify that characters should be added
     at the position specified by suffix_loc. */
  set_buffer_position(file_name_buffer, suffix_loc);
  if (new_suffix_length > 0) {
    /* Add the delimiter. */
    add_char_to_text_buffer(file_name_buffer, SUFFIX_DELIMITER);
    /* Add the new suffix to the new filename. */
    add_to_text_buffer(file_name_buffer, new_suffix, new_suffix_length);
  }  /* if */
  /* Terminate the string. */
  add_char_to_text_buffer(file_name_buffer, '\0');
#if DEBUG
  if (db_flag_is_set("replace_file_name_suffix")) {
    fprintf(f_debug, "new file name = \"%s\"\n", file_name_buffer->buffer);
  }  /* if */
#endif /* DEBUG */
  db_exit();
#undef SUFFIX_DELIMITER
}  /* replace_file_name_suffix */

#endif /* !STANDALONE_UTILITY_PROGRAM */


static char *get_file_modification_time_string(char	*file_name)
/*
Return the last modification time of "file_name" as a date/time string.
If the file does not exist, or is not a regular file, return NULL.
When a string is the static buffer returned by the ctime function,
which will be overwritten when ctime is called again.
*/
{
  time_t	mod_time;
  char		*time_str = NULL;

  if (get_file_modification_time(file_name, &mod_time)) {
    time_str = ctime(&mod_time);
  }  /* if */
  return time_str;
}  /* get_file_modification_time_string */


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


a_boolean okay_as_output_file(char *file_name)
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
      mode = (char *)(binary_file ? FOPEN_MODE_FOR_BINARY_UPDATE :
                                    FOPEN_MODE_FOR_UPDATE);
    } else {
      mode = (char *)(binary_file ? FOPEN_MODE_FOR_BINARY_WRITE
                                  : FOPEN_MODE_FOR_WRITE);
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
    mode = (char *)(binary_file ? FOPEN_MODE_FOR_BINARY_READ :
                                  FOPEN_MODE_FOR_READ);
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

#if __VMS__
EXTERN_C int delete(char *file_name);
#endif /* __VMS__ */

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
  status = delete(file_name);
#else /* !__VMS__ */
  status = unlink(file_name);
#endif /* __VMS__ */
#endif /* __ANSIC__ */
  if (status != 0) {
    str_catastrophe(ec_file_delete_error, file_name);
  }  /* if */
}  /* delete_file */


#if __MICROSOFT_OS__
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
#endif /* __MICROSOFT_OS__ */


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
#if __MICROSOFT_OS__
    /* On a Microsoft OS, first use the TMP environment variable, if set. */
    temp_dir = getenv("TMP");
#endif /* __MICROSOFT_OS__ */
    if (temp_dir == NULL) temp_dir = getenv("TMPDIR");
    if (temp_dir == NULL || strlen(temp_dir) == 0) temp_dir = DEFAULT_TMPDIR;
  }  /* if */
  dir_len = strlen(temp_dir);
  /* See if a slash must be added to the directory name. */
  need_slash = (temp_dir[dir_len-1] != DIRECTORY_SEPARATOR);
#if __MICROSOFT_OS__
  /* Under MS-DOS we don't need to add a slash if the path already ends with
     a backslash. */
  if (need_slash && temp_dir[dir_len-1] == '\\') need_slash = FALSE;
#endif /* __MICROSOFT_OS__ */
  do {
    /* Put together the name dir + "/edg" + seed + "_" + process id.  See if
       that will fit in the buffer. */
    if (dir_len + need_slash + 24 > TEMP_NAME_BUFFER_SIZE) {
      str_catastrophe(ec_temp_file_dir_name_too_long, temp_dir);
    }  /* if */
    (void)sprintf(buffer, "%s%sedg%lu_%ld", temp_dir, 
                  need_slash ? DIRECTORY_SEPARATOR_STRING : "", seed++,
                  (long)getpid());
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
#if __MICROSOFT_OS__
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
#else /* !__MICROSOFT_OS__ */
  /* Delete the file now, so it will disappear when closed. */
  (void)unlink(buffer);
#endif /* __MICROSOFT_OS__ */
  return(temp_file);
}  /* open_temp_file */


void close_temp_file(FILE *temp_file)
/*
Close and delete the indicated temporary file.
*/
{
  (void)fclose(temp_file);
#if __MICROSOFT_OS__
  { a_temp_file_name_ptr tfnp, prev_tfnp;
    /* Find the entry for this file on the list of open files. */
    for (prev_tfnp = NULL, tfnp = open_temp_files;
         tfnp != NULL;
         prev_tfnp = tfnp, tfnp = tfnp->next) {
      if (tfnp->file == temp_file) {
        /* Found it; delete the file and remove the entry from the list.
           The space for the entry is just lost, but that's not a big issue. */
        (void)remove(tfnp->name);
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
#else /* !__MICROSOFT_OS__ */
  /* The file was unlinked when opened, and therefore was deleted automatically
     when closed. */
#endif /* __MICROSOFT_OS__ */
}  /* close_temp_file */


#if __MICROSOFT_OS__
static void close_all_temp_files(void)
/*
Close and delete all open temporary files.
*/
{
  while (open_temp_files != NULL) {
    close_temp_file(open_temp_files->file);
  }  /* while */
}  /* close_all_temp_files */
#endif /* __MICROSOFT_OS__ */


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
#if EXIT_ON_INTERNAL_ERROR
      exit(RC_CATASTROPHE);
#else /* !EXIT_ON_INTERNAL_ERROR */
      (void)fflush(stderr);
      abort();
#endif /* EXIT_ON_INTERNAL_ERROR */
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


/*
In C++, signal handlers must be extern "C".
*/
BEGIN_EXTERN_C_BLOCK

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

#if DEBUG && !EDG_WIN32

/*ARGSUSED*/ /* <-- Because "sig" is not used. */
static a_signal_handler_return_value abort_on_cpu_limit(int sig)
/*
Routine set up as a signal handler, called to terminate compilation 
with an internal error on receipt of a signal.
*/
{
#if !USING_DRIVER
  /* Print newline to make console output clean. */
  fprintf(stderr, "\n");
#endif /* !USING_DRIVER */
  fprintf(stderr, "Internal error: CPU time limit exceeded.\n");
  term_compilation(es_internal_error);
  /*NOTREACHED*/
}  /* abort_on_cpu_limit */

#endif /* DEBUG && !EDG_WIN32 */

/*
The Sun C++ compiler requires a ... as the second parameter of the
signal handler function.  Define a type to which the real signal handler
function pointer will be cast.
*/
#if defined(__SUNPRO_CC) && __BSD__
typedef a_signal_handler_return_value a_signal_handler(int p, ...);
#else /* !(defined(__SUNPRO_CC) && __BSD__) */
/* Standard type for a signal handler. */
typedef a_signal_handler_return_value a_signal_handler(int p);
#endif /* defined(__SUNPRO_CC) && __BSD__ */

END_EXTERN_C_BLOCK

static void set_signal_handlers(void)
/*
Enable any signal handlers necessary to catch signals that may come up during
execution of the front end (for example, SIGINT).
*/
{
  if (signal(SIGINT, SIG_IGN) != SIG_IGN) {
    /* Only reset the signal if it is not already being ignored.  This is
       to prevent a compilation in the background from being terminated by
       an interrupt intended for the foreground process on older Unix
       systems that lack job control. */
    (void)signal(SIGINT, (a_signal_handler *)term_on_signal);
  }  /* if */
  (void)signal(SIGTERM, (a_signal_handler *)term_on_signal);
#ifdef SIGXFSZ
  /* On SVR4 systems, ignore the signal sent when the file size limit
     is exceeded.  Note that the write operation will still fail, so the
     error will be reported where the file is written. */
  (void)signal(SIGXFSZ, SIG_IGN);
#endif /* SIGXFSZ */
#if DEBUG && !EDG_WIN32
  /* Catch the signal that the CPU limit has been exceeded. */
  (void)signal(SIGXCPU, (a_signal_handler *)abort_on_cpu_limit);
#endif /* DEBUG && !EDG_WIN32 */
#if __MICROSOFT_OS__
  /* Under MS-DOS, establish an atexit routine to close and delete all
     temporary files. */
  if (atexit(close_all_temp_files) != 0) {
#if CHECKING
    internal_error("set_signal_handlers: could not set atexit handler");
#endif /* CHECKING */
  }  /* if */
#endif /* __MICROSOFT_OS__ */
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
  temp = (temp * 1000) / CLOCKS_PER_SEC;
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


/*
Change to the specified directory, make sure the operation succeeded.
Not used in some configurations.
*/
#define chdir_with_check(dir_name) \
{ if (chdir(dir_name) != 0) { \
    str_catastrophe(ec_cannot_chdir, (dir_name)); \
  }  /* if */ \
}  /* chdir_with_check */


/* Header comment for is_directory */
/*
Determine whether the specified path name is the name of a valid
directory.
*/
#if defined(S_ISDIR) || defined(S_IFDIR)
/*
UNIX Version.

When the macro S_ISDIR is defined, we assume that the stat system call
is available and can be used to determine whether a file name is
a directory.
*/
a_boolean is_directory(char   *file_name)
{
  a_boolean	result = FALSE;
  struct stat   buf;

  /* Check the file type.  Use the stat call instead of fstat because some
     implementations do not have the _file field in the structure. */
  if (stat(file_name, &buf) == 0) {
    /* Use the POSIX S_ISDIR if it is defined.  Otherwise use the
       non-POSIX test using S_IFDIR. */
#ifdef S_ISDIR
    result = S_ISDIR(buf.st_mode);
#else /* ifndef S_ISDIR */
    result = ((buf.st_mode & S_IFDIR) != 0);
#endif /* ifdef S_ISDIR */
  }  /* if */
  return result;
}  /* is_directory */

/* Define a flag that indicates that a definition of is_directory has
   been supplied. */
#define IS_DIRECTORY_DEFINED

#else /* defined(S_ISDIR) || defined(S_IFDIR) */
#if EDG_WIN32
/*
WIN32 (e.g., Windows-NT) version.
*/
a_boolean is_directory(char *file_name)
{
  DWORD    attr;

  attr = GetFileAttributes(file_name);
  return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}  /* is_directory */
/* Define a flag that indicates that a definition of is_directory has
   been supplied. */
#define IS_DIRECTORY_DEFINED

#endif /* EDG_WIN32 */
#endif /* defined(S_ISDIR) || defined(S_IFDIR) */

#ifndef IS_DIRECTORY_DEFINED
a_boolean is_directory(char *file_name)
/*
This is a portable version of is_directory that should work on any
system that supports chdir.
*/
{
  a_boolean  result = FALSE;

  /* Try to change to the specified directory. */
  if (chdir(file_name) == 0) {
    /* The operation succeeded. */
    result = TRUE;
  }  /* if */
  /* Return to the original directory. */
  chdir_with_check(current_directory_name);
  return result;
}  /* is_directory */
#endif /* ifndef IS_DIRECTORY_DEFINED */

#if __MICROSOFT_OS__

a_boolean has_drive_specification(char *file_name)
/*
Test whether or not a file name includes a drive specification.
*/
{
  a_boolean	result;

  result = isalpha((unsigned char)(file_name)[0]) && ((file_name)[1] == ':');
  return result;
}  /* has_drive_specification */

#endif /* __MICROSOFT_OS__ */

a_boolean is_absolute_file_name(char *file_name)
/*
Test whether or not a file name is absolute (a full path name).
*/
{
#if __MICROSOFT_OS__
  return ((file_name)[0] == DIRECTORY_SEPARATOR) ||
         ((file_name)[0] == '\\') || 
         has_drive_specification(file_name);
#else /* !__MICROSOFT_OS__ */
  return (file_name)[0] == DIRECTORY_SEPARATOR;
#endif /* __MICROSOFT_OS__ */
}


/* Header comment for get_file_name_from_dir. */
/*
Get the name of the next file in the current directory that has the
indicated suffix.  If first is TRUE, then this is the first call.
Returns a pointer to the file name string.  The value returned is
only valid until the next call of this routine.  A NULL pointer is
returned when there are no more directory entries.  dir_name indicates
the directory in which to search (or NULL for the current directory),
and curr_dir_name is the current directory name.
*/
#if EDG_WIN32
/*
WIN32 (e.g., Windows-NT) version.
*/

#include <tchar.h>
#include <dos.h>
#include <io.h>

char *get_file_name_from_dir(a_boolean	first,
			     char	*dir_name,
			     char	*suffix,
			     char	*curr_dir_name)
{
  static long			handle;
  static struct _tfinddata_t	fileinfo;
  char				*result;
  static char			pattern[10];

  if (dir_name != NULL) {
    chdir_with_check(dir_name);
  }  /* if */
  if (first) {
    /* Convert the suffix (e.g., ".xxx" into a pattern for use by the
       Windows-NT routine (e.g., "*.xxx"). */
    check_assertion(strlen(suffix) <= 8);
    sprintf(pattern, "*%s", suffix);
    /* On the first call, use the _findfirst call that specifies which
       files are to be returned.  "handle" is saved in a static variable
       that is used on subsequent calls to get the remaining directory
       entries. */
    handle = _tfindfirst(pattern, &fileinfo);
    if (handle < 0) {
      /* Directory could not be opened, or is empty. */
      result = NULL;
    } else {
      result = fileinfo.name;
    }  /* if */
  } else {
    /* On subsequent calls, use _findnext to find the next file that
       matches the pattern. */
    if (_tfindnext(handle, &fileinfo) < 0) {
      /* Returns -1 when there are no more files. */
      result = NULL;
    } else {
      result = fileinfo.name;
    }  /* if */
  }  /* if */
  if (dir_name != NULL) {
    chdir_with_check(curr_dir_name);
  }  /* if */
  return result;
}  /* get_file_name_from_dir */
#else /* !EDG_WIN32 */
#if EDG_MSDOS
/*
DOS version.
*/

#include <dos.h>

char *get_file_name_from_dir(a_boolean	first,
			     char	*dir_name,
			     char	*suffix,
			     char	*curr_dir_name)
/*
See comment above.
*/
{
  static struct _find_t	fileinfo;
  char			*result;
  static char		pattern[10];

  if (dir_name != NULL) {
    chdir_with_check(dir_name);
  }  /* if */
  if (first) {
    /* Convert the suffix (e.g., ".xxx" into a pattern for use by the
       find-first routine (e.g., "*.xxx"). */
    check_assertion(strlen(suffix) <= 8);
    sprintf(pattern, "*%s", suffix);
    /* On the first call, use the _dos_findfirst call that specifies which
       files are to be returned.  The _A_RDONLY attribute causes
       both normal and read-only files to be returned. */
    if (_dos_findfirst(pattern, _A_RDONLY, &fileinfo) != 0) {
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
  if (dir_name != NULL) {
    chdir_with_check(curr_dir_name);
  }  /* if */
  return result;
}  /* get_file_name_from_dir */
#else /* !EDG_MSDOS */

#if __VMS__
/*
VMS version.  A VMS specific version is not supplied in the standard
distribution.
*/
/*ARGSUSED*/ /* <-- Because all arguments are unused in the VMS version. */
char *get_file_name_from_dir(a_boolean	first,
			     char	*dir_name,
			     char	*suffix,
			     char	*curr_dir_name)
/*
See comment above.

A VMS specific version is not supplied.  The VMS version (but without
automatic PCH support) can be used by getting rid of the #error directive
below.
*/
{
 #error -- a VMS specific version of get_file_name_from_dir must be supplied
  return (char *)NULL;
}  /* get_file_name_from_dir */
#else /* !__VMS__ */

/*
UNIX Version.
*/
#include <sys/types.h>
#include <dirent.h>
#ifndef __AIX__
#ifndef __osf__
#ifndef __linux__
/* This file should not be included on IBM AIX, Digital UNIX, or Linux. */
#include <sys/dirent.h>
#endif /* ifndef __linux__ */
#endif /* ifndef __osf__ */
#endif /* ifndef __AIX__ */

/*ARGSUSED*/ /* <-- Because "curr_dir_name" is not used. */
char *get_file_name_from_dir(a_boolean	first,
			     char	*dir_name,
			     char	*suffix,
			     char	*curr_dir_name)
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
    /* If no directory name was specified, use the current directory. */
    if (dir_name == NULL) dir_name = ".";
    dir = opendir(dir_name);
    check_assertion(dir != NULL);
  }  /* if */
  for (;;) {
    char	*ptr;
    dir_entry = readdir(dir);
    if (dir_entry == NULL) {
      /* The last entry was read. */
      (void)closedir(dir);
      result = NULL;
      break;
    }  /* if */
    result = dir_entry->d_name;
    /* Make sure the suffix matches the value passed by the caller. */
    ptr = strchr(result, '.');
    if (ptr != NULL && strcmp(ptr, suffix) == 0) break;
  }  /* for */
  return result;
}  /* get_file_name_from_dir */
#endif /* !__VMS__ */
#endif /* EDG_MSDOS */
#endif /* EDG_WIN32 */


static char *get_curr_dir_name(void)
/*
Get the current directory name and return it in the temporary string
buffer.
*/
{
#if USE_GETCWD
  /* The temporary buffer may not be allocated yet.  Make sure there
     is some space allocated. */
  ensure_temp_text_buffer_space(256);
  for (;;) {
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


#if MODULE_ID_NEEDED
/*
Routines used by lower_init.c and c_gen_be.c to generate module IDs
used to create unique external names.
*/

void change_non_id_characters(char *str)
/*
Change any non-identifier characters in the indicated string to underscores.
*/
{
  for (; *str != '\0'; str++) if (!isalnum((unsigned char)*str)) *str = '_';
}  /* change_non_id_characters */


static char	*module_id /* = NULL */;
			/* A string used to qualify static names that are put
			   out as external names to make them unique. */

void set_module_id(char *new_module_id)
/*
Set the module ID for the current translation unit to the value specified
by new_module_id.
*/
{
  /* Make sure a module ID has not already been assigned. */
  check_assertion(module_id == NULL);
  module_id = new_module_id;
}  /* set_module_id */


char *make_module_id(void)
/*
Make a string that is based on the name of the current module and is used to
qualify static names that are put out as external names, to make them unique.
Set module_id to the string.
*/
{
  char			*file_name;
  sizeof_t		file_name_len;
  a_scope_ptr		scope = il_header.primary_scope;
  a_variable_ptr	variable;
  a_routine_ptr		routine;
  char			*external_name = NULL;
  char			*str1;
  char			*str2;

  /* Only generate the module ID the first time that this routine is called
     for a given translation unit. */
  if (module_id == NULL) {
    file_name = curr_translation_unit->source_file->file_name;
    /* Find an externally visible variable or routine definition whose name
       can be used as part of the module ID. */
    for (variable = scope->variables;
         variable != NULL; variable = variable->next) {
      /* Only consider variables that are defined.  Make sure that the
         init_kind is not none -- this eliminates tentative definitions.
         Also ignore variables whose names begin with "__"; one such case
         is typeinfo variables, and when this routine is called from IL
         lowering they may yet become static variables. */
      if (variable->storage_class == (a_storage_class)sc_unspecified &&
          variable->init_kind != (an_init_kind)initk_none &&
	  variable->source_corresp.name[0] != '_' &&
	  variable->source_corresp.name[1] != '_') {
        /* Don't use template static data members.  Some implementations
           may generate these in multiple files. */
        if (variable->is_template_static_data_member) continue;
        external_name = variable->source_corresp.name;
        check_assertion(external_name != NULL);
        break;
      }  /* if */
    }  /* for */
    if (external_name == NULL) {
      /* No external variable was found, look for an external routine. */
      for (routine = scope->routines;
           routine != NULL; routine = routine->next) {
        if (routine->storage_class == (a_storage_class)sc_unspecified) {
          /* Don't use template functions.  Some implementations
             may generate these in multiple files. */
          if (routine->is_template_function) continue;
          external_name = routine->source_corresp.name;
          check_assertion(external_name != NULL);
          break;
        }  /* if */
      }  /* for */
    }  /* if */
    if (external_name == NULL) {
      /* In the very unlikely event that the file does not define any
         externally visible variables or routines, use the time of
         compilation and current directory name. */
      str1 = get_file_modification_time_string(file_name);
      str2 = current_directory_name;
    } else {
      str1 = external_name;
      str2 = NULL;
    }  /* if */
    { /* The identifier is made of the primary source file name plus
         either the name of an externally defined variable or routine,
         or (if no such entity is available) the current directory and
         time and date.  If the entity name or time/date/directory is
         longer than 8 characters, a CRC of the string is used in place
         of the string.  Non-identifier characters are replaced with
         underscores. */
      char	crc_buf[9];
      int	len1;
      int	len2;
      len1 = strlen(str1);
      len2 = str2 == NULL ? 0 : strlen(str2);
      if ((len1 + len2 + (int)(len2 != 0)) > 8) {
        /* The string (not including the file name) is longer than 8
           characters.  Use a CRC of the string instead. */
        unsigned long	crc;
	crc = crc_32(str1, (unsigned long)0);
	if (len2 != 0) crc = crc_32(str2, crc);
        sprintf(crc_buf, "%08lx", crc);
        str1 = crc_buf;
        len1 = 8;
        str2 = NULL;
        len2 = 0;
      }  /* if */
      /* Exclude the directory portion of the file name. */
      { char	*end_of_dir;
        end_of_dir = end_of_directory_name(file_name);
        if (end_of_dir != NULL) file_name = end_of_dir+1;
      }
      file_name_len = strlen(file_name);
      module_id = alloc_general(file_name_len + 1 +
                                len1 + len2 + (int)(len2 != 0) + 1);
      (void)strcpy(module_id, file_name);
      (void)strcat(module_id, "_");
      (void)strcat(module_id, str1);
      if (str2 != NULL) {
        (void)strcat(module_id, "_");
        (void)strcat(module_id, str2);
      }  /* if */
      /* Change non-identifier characters to "_". */
      change_non_id_characters(module_id);
    }
  }  /* if */
  return module_id;
}  /* make_module_id */

#endif /* MODULE_ID_NEEDED */
#if USE_MMAP_FOR_MEMORY_REGIONS

#if EDG_WIN32

static HANDLE	f_mmap_file;
			/* The file handle for the mapped IL file. */

static HANDLE	f_mapped_input;
			/* The file handle of the PCH input file as
			   opened for file mapping purposes. */

static HANDLE	f_map_object;
			/* The file handle of the map object associated with
			   the mapped input file. */

void open_mapped_il_temp_file(void)
/*
Open a temporary file to be used for allocation of file mapped
memory for IL memory blocks.
*/
{
  char		temp_dir[MAX_PATH];
  char		temp_file_name[MAX_PATH];

  db_enter(3, "open_mapped_il_temp_file");
  if (GetTempPath(MAX_PATH, temp_dir) == 0 ||
      GetTempFileName(temp_dir, "edg", 0, temp_file_name) == 0) {
    catastrophe(ec_cannot_build_temp_file_name);
  }  /* if */
  f_mmap_file = CreateFile(temp_file_name, GENERIC_READ | GENERIC_WRITE,
                           /*fdwShareMode=*/0, (LPSECURITY_ATTRIBUTES)NULL,
                           CREATE_ALWAYS,
                           FILE_ATTRIBUTE_TEMPORARY |
                                                  FILE_FLAG_DELETE_ON_CLOSE,
                           NULL);
  if (f_mmap_file == INVALID_HANDLE_VALUE) {
    str_catastrophe(ec_cannot_open_temp_file, temp_file_name);
  }  /* if */
  db_exit();
}  /* open_mapped_il_temp_file */


void open_mapped_input_file(char *file_name)
/*
Open a file that contains memory region information that will be mapped
into the address space of the current process.  This is used to reactivate
a precompiled header file.  This file will already have been opened using
fopen, so this open must be done in shared mode.
*/
{
  f_mapped_input = CreateFile(file_name, GENERIC_READ,
                              FILE_SHARE_READ, /*lpsa=*/NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
  check_assertion_str(f_mapped_input != INVALID_HANDLE_VALUE,
                      "CreateFile of mapped input file failed");
  if (f_mapped_input == INVALID_HANDLE_VALUE) {
    /* This shouldn't happen because the file must have already been
       successfully opened as a normal input file before this routine is
       called. */
    str_command_line_error(ec_cl_cannot_open_pch_input_file,
                           file_name);
  }  /* if */
  f_map_object = CreateFileMapping(f_mapped_input, NULL,
                                   PAGE_WRITECOPY, 0, 0, NULL);
  check_assertion_str(f_map_object != INVALID_HANDLE_VALUE,
                      "CreateFileMapping failed");
  if (f_map_object == INVALID_HANDLE_VALUE) {
    catastrophe(ec_unable_to_get_mapped_memory);
  }  /* if */
}  /* open_mapped_input_file */


void close_mapped_input_file(void)
/*
Close the mapped input file and the associated map object.
*/
{
  if (!CloseHandle(f_mapped_input)) {
    unexpected_condition_str("CloseHandle of mapped input failed");
  }  /* if */
  if (!CloseHandle(f_map_object)) {
    unexpected_condition_str("CloseHandle of map object failed");
  }  /* if */
}  /* close_mapped_input_file */


#if !USE_FIXED_ADDRESS_FOR_MMAP
/*ARGSUSED*/ /* <-- Because "curr_size" is only used when
                    USE_FIXED_ADDRESS_FOR_MMAP is TRUE. */
#endif /* !USE_FIXED_ADDRESS_FOR_MMAP */
a_void_ptr map_file_region(sizeof_t	curr_size,
		           sizeof_t	incremental_size,
			   long		file_offset)
/*
Expand a memory mapped file.  This routine assumes that curr_size bytes
have already been allocated and mapped, and that incremental_size bytes
should be added.  incremental_size must be a multiple of the host
page size.
*/
{
  a_void_ptr		addr = NULL;
  sizeof_t		size;
  DWORD			new_pos;
  DWORD			bytes_written;
  HANDLE		f_map;
#if USE_FIXED_ADDRESS_FOR_MMAP
  a_void_ptr		map_address;
#endif /* USE_FIXED_ADDRESS_FOR_MMAP */

  db_enter(4, "map_file_region");
  size = curr_size + incremental_size;
  /* The file must be large enough to contain the mapped area. */
  new_pos = SetFilePointer(f_mmap_file, (long)size, (PLONG)NULL, FILE_BEGIN);
  if (new_pos != 0xffffffff) {
    /* Write a character at the last allocated position. */
    if (WriteFile(f_mmap_file, &new_pos, 1, &bytes_written,
                  (LPOVERLAPPED)NULL)) {
      f_map = CreateFileMapping(f_mmap_file,
                                (LPSECURITY_ATTRIBUTES)NULL,
                                PAGE_READWRITE, (DWORD)0, (DWORD)0,
                                (LPTSTR)NULL);
      if (f_map != INVALID_HANDLE_VALUE) {
#if USE_FIXED_ADDRESS_FOR_MMAP
        map_address = ((char *)FIXED_ADDRESS_FOR_MMAP) + curr_size;
        addr = MapViewOfFileEx(f_map, FILE_MAP_WRITE, (DWORD)0,
                               (DWORD)file_offset, incremental_size,
                               map_address);
#else /* !USE_FIXED_ADDRESS_FOR_MMAP */
        addr = MapViewOfFile(f_map, FILE_MAP_WRITE, (DWORD)0,
                             (DWORD)file_offset, incremental_size);
#endif /* USE_FIXED_ADDRESS_FOR_MMAP */
      }  /* if */
#if DEBUG
      if (debug_level >= 4) {
        fprintf(f_debug, "Allocated %lu bytes of mmap memory at %p\n",
                (unsigned long)incremental_size, addr);
      }  /* if */
#endif /* DEBUG */
    }  /* if */
  }  /* if */
  db_exit();
  return addr;
}  /* map_file_region */


/*ARGSUSED*/ /* <-- Because "file" is not used. */
a_void_ptr map_input_file_to_region(FILE		*file,
                                    sizeof_t		offset,
				    sizeof_t		size,
				    a_void_ptr		address)
/*
Map the data pointed to by "file", starting at "offset" bytes,
for "size" bytes to the address specified by "address".
This mapping is done as a FILE_MAP_COPY mapping so that any changes to
the data will be local.  This is used to map a section of a PCH
file to a memory region.
*/
{
  a_void_ptr	result_addr;

  result_addr = MapViewOfFileEx(f_map_object, FILE_MAP_COPY, (DWORD)0,
                                (DWORD)offset, size, address);
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "Allocated %lu bytes of mmap memory at %p\n",
            (unsigned long)size, address);
  }  /* if */
  if (debug_level >= 1 && result_addr == NULL) {
    fprintf(f_debug, "Map failed: address=%p, size=%lu, offset=%lu\n",
            address, (unsigned long)size, (unsigned long)offset);
  }  /* if */
#endif /* DEBUG */
  return result_addr;
}  /* map_input_file_to_region */


/*ARGSUSED*/ /* <-- Because "size" is not used. */
void unmap_memory(a_void_ptr	addr,
	          sizeof_t	size)
/*
Unmap a block of previously mapped memory.
*/
{
  if (!UnmapViewOfFile(addr)) {
    unexpected_condition_str("unmap_memory: UnmapViewOfFile failed\n");
  }  /* if */
}  /* unmap_memory */


static int get_page_size(void)
/*
Return the size of a host page.  When map_file_region is called,
incremental_size must be a multiple of the page size.
*/
{
  /* Windows-NT addresses and file offsets must be multiples of
     64K. */
  return 65536;
}  /* get_page_size */


#else /* !EDG_WIN32 */
#include <sys/mman.h>

#if __BSD__
EXTERN_C int getpagesize(void);
#endif /* __BSD__ */

#if defined(__hpux) || defined(__AIX__)
/* HP and IBM require that unistd.h be included, and use _SC_PAGE_SIZE
   in place of _SC_PAGESIZE. */ 
#include <unistd.h>
#define _SC_PAGESIZE _SC_PAGE_SIZE
#endif /* defined(__hpux) || defined(__AIX__) */

static FILE*	f_mmap_file;
			/* The file descriptor for the mmap file. */

static int	mmap_file_number;
			/* The file number of the mmap file. */

static int get_page_size(void)
/*
Return the size of a host page.  When map_file_region is called,
incremental_size must be a multiple of the page size.
*/
{
  int	page_size;
#if __BSD__ || defined(__linux__) || defined(__FreeBSD__)
  page_size = getpagesize();
#else /* !(__BSD__  || __linux__ || __FreeBSD__) */
  page_size = sysconf(_SC_PAGESIZE);
#endif /* __BSD__  || __linux__ || __FreeBSD \__ */
  check_assertion_str2(page_size > 0, "get_page_size:", "invalid page size");
  return page_size;
}  /* get_page_size */


#if !USE_FIXED_ADDRESS_FOR_MMAP
/*ARGSUSED*/ /* <-- Because "curr_size" is only used when
                    USE_FIXED_ADDRESS_FOR_MMAP is TRUE. */
#endif /* !USE_FIXED_ADDRESS_FOR_MMAP */
a_void_ptr map_file_region(sizeof_t	curr_size,
		           sizeof_t	incremental_size,
			   long		file_offset)
/*
Expand a memory mapped file.  This routine assumes that curr_size bytes
have already been allocated and mapped, and that incremental_size bytes
should be added.  incremental_size must be a multiple of the host
page size.
*/
{
  caddr_t		addr = NULL;
  sizeof_t		size;
#if USE_FIXED_ADDRESS_FOR_MMAP
  a_void_ptr		map_address;
#endif /* USE_FIXED_ADDRESS_FOR_MMAP */

  db_enter(4, "map_file_region");
  size = file_offset + incremental_size;
  /* The file must be large enough to contain the mapped area. */
  if (fseek(f_mmap_file, (long)size, SEEK_SET) == 0) {
    /* Write a character at the last allocated position and
       make sure the write to the file is actually done. */
    if (fputc(0, f_mmap_file) != EOF && fflush(f_mmap_file) == 0) {
#if USE_FIXED_ADDRESS_FOR_MMAP
      /* Suppress the CodeCenter warning that would be issued because we
         build an address that is not yet valid. */
      /*SUPPRESS 25 */  /*SUPPRESS 26 */
      map_address = ((char *)FIXED_ADDRESS_FOR_MMAP) + curr_size;
      /* Suppress the CodeCenter warning that an invalid pointer is being
         passed. */
      /*SUPPRESS 71 */
      addr = (caddr_t)mmap((caddr_t)map_address,
                           incremental_size,
                           PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_FIXED,
                           mmap_file_number, (off_t)file_offset);
#else /* !USE_FIXED_ADDRESS_FOR_MMAP */
      addr = (caddr_t)mmap((char*)0, incremental_size,
                           PROT_WRITE | PROT_READ, MAP_PRIVATE,
                           mmap_file_number, (off_t)file_offset);
#endif /* USE_FIXED_ADDRESS_FOR_MMAP */
#if DEBUG
      if (debug_level >= 4) {
        fprintf(f_debug, "Allocated %lu bytes of mmap memory at %p\n",
                (unsigned long)incremental_size, addr);
      }  /* if */
#endif /* DEBUG */
      /* mmap returns (caddr_t)-1 if the operation fails. */
      if (addr == (caddr_t)-1) addr = NULL;
    }  /* if */
  }  /* if */
  db_exit();
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
  int		fd = fileno(file);
  a_void_ptr	result_addr;

  result_addr = (a_void_ptr)mmap((caddr_t)address, size,
                            PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_FIXED,
                            fd, (off_t)offset);
  /* mmap returns (cresult_addr_t)-1 if the operation fails. */
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "Allocated %lu bytes of mmap memory at %p\n",
            (unsigned long)size, address);
  }  /* if */
  if (debug_level >= 1 && result_addr == (caddr_t)-1) {
    fprintf(f_debug, "Map failed: address=%p, size=%lu, offset=%lu\n",
            address, (unsigned long)size, (unsigned long)offset);
  }  /* if */
#endif /* DEBUG */
  if (result_addr == (caddr_t)-1) result_addr = NULL;
  return result_addr;
}  /* map_input_file_to_region */


#if __BSD__
#if !defined(__cplusplus)
/* Some BSD systems (e.g., SunOS 4.1.3) don't declare munmap. */
EXTERN_C int munmap(caddr_t addr, sizeof_t size);
#endif /* !defined(__cplusplus) */
#endif /* __BSD__ */


void unmap_memory(a_void_ptr	addr,
	          sizeof_t	size)
/*
Unmap a block of previously mapped memory.
*/
{
  if (munmap((caddr_t)addr, size) != 0) {
    unexpected_condition_str("unmap_memory: munmap failed\n");
  }  /* if */
}  /* unmap_memory */


void open_mapped_il_temp_file(void)
/*
Open a temporary file to be used for allocation of file mapped
memory for IL memory blocks.
*/
{
  db_enter(3, "open_mapped_il_temp_file");
  f_mmap_file = open_temp_file(/*binary_file=*/TRUE);
  check_assertion(f_mmap_file != NULL);
  mmap_file_number = fileno(f_mmap_file);
  db_exit();
}  /* open_mapped_il_temp_file */

#endif /* EDG_WIN32 */

static int	page_size = 0;
			/* The size of a host page.  Memory mapped blocks must
			   be requested in increments of this size. */


sizeof_t do_page_alignment(sizeof_t size)
/*
Return "size" adjusted as needed to be a multiple of the system page size.
*/
{
  sizeof_t	size2;

  /* On the first call of this routine, get the host page size. */
  if (page_size == 0) {
    page_size = get_page_size();
    /* The host allocation increment must be a multiple of the page size. */
    check_assertion_str(HOST_ALLOCATION_INCREMENT % page_size == 0,
                        "invalid HOST_ALLOCATION_INCREMENT for page size");
  }  /* if */
  size2 = (size / (sizeof_t)page_size) * page_size;
  if (size2 < size) size2 += page_size;
  return size2;
}  /* do_page_alignment */


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

#if SVR4_TRAP_NULL_POINTER_REFERENCES
#if !USE_MMAP_FOR_MEMORY_REGIONS
/* This will already have been included if we are using mmap. */
#include "mman.h"
#endif /* !USE_MMAP_FOR_MEMORY_REGIONS */

static void svr4_trap_null_pointer_references(void)
/*
Some systems don't trap NULL pointer references.  This routine may
be used on SVR4 systems to have NULL pointer references result in
a memory fault.
*/
{
  int	*p;
  p = 0;
  /* Access page zero.  This needs to be done before the mprotect call. */
  p = (int*)(*p);
  /* Protect the first 4k bytes of memory. */
  if (mprotect(0, 4096, PROT_NONE) == -1) {
    unexpected_condition_str("mprotect failed");
  }  /* if */
}  /* svr4_trap_null_pointer_references */
#endif /* SVR4_TRAP_NULL_POINTER_REFERENCES */

#if DEBUG
#if !EDG_WIN32

#if __BSD__
#include <sys/time.h>
#endif /* __BSD__ */
#include <sys/resource.h>

void set_cpu_time_limit(int	seconds)
/*
Set the maximum amount of CPU time that can be used by the compilation.
Used for debugging purposes.
*/
{
  struct rlimit	limit;
  limit.rlim_cur = seconds;
  limit.rlim_max = seconds;
  (void)setrlimit(RLIMIT_CPU, &limit);
}  /* set_cpu_time_limit */

#endif /* !EDG_WIN32 */
#endif /* DEBUG */

#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED

int mbc_length(char      *ptr,
               a_boolean *err)
/*
Return the length of the multibyte character sequence beginning at ptr.
If the sequence there is invalid, set *err to TRUE if err is non-NULL,
and return 1.
*/
{
  int len;

  if (err != NULL) *err = FALSE;
#if USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING
  /* Use custom code for SJIS instead of the C library routines. */
  { unsigned char ch = (unsigned char)*ptr;
    /* Check for codes that indicate the first character of a two-character
       sequence.  Note that the test functions are macros that can be
       replaced as necessary to deal with the fact that different
       implementations of SJIS use different character ranges. */
    if (is_first_char_of_sjis_two_char_sequence(ch)) {
      /* Two-character sequence.  Check validity of second character. */
      ch = (unsigned char)(ptr[1]);
      if (is_valid_sjis_second_char(ch)) {
        len = 2;
      } else {
        /* Invalid sequence.  Advance bytewise. */
        if (err != NULL) *err = TRUE;
        len = 1;
      }  /* if */
    } else {
      /* One-character sequence. */
      len = 1;
    }  /* if */
  }
#else /* !USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING */
  /* Use standard C library routines. */
  len = mblen(ptr, MB_CUR_MAX);
  if (len < 0) {
    /* Invalid multibyte sequence.  Advance bytewise. */
    if (err != NULL) *err = TRUE;
    len = 1;
  }  /* if */
#endif /* USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING */

  return len;
}  /* mbc_length */

#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED

int mbc_to_wide_char(char          *mb,
                     unsigned long *wc,
                     a_boolean     *err)
/*
Convert a multibyte character sequence pointed to by mb to a single wide
character returned in *wc.  Return the number of characters in the
multibyte character sequence.  If the multibyte character sequence is
invalid, set *err to TRUE if err is non-NULL, and return 1.
*/
{
  int       numch;
  a_boolean local_err = FALSE;

#if USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING
  /* Use custom code for SJIS instead of the C library routines. */
  numch = mbc_length(mb, &local_err);
  if (local_err) {
    /* Bad multibyte character. */
    numch = 1;
    *wc = 0;
  } else {
    *wc = (unsigned char)mb[0];
    if (numch != 1) {
      *wc = (*wc << targ_char_bit) | (unsigned char)mb[1];
      check_assertion(numch == 2);
    }  /* if */
  }  /* if */
#else /* !USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING */
  /* Use a standard C library routine to do the multibyte character
     sequence to wide character conversion. */
  { wchar_t wchar;
    numch = mbtowc(&wchar, mb, MB_CUR_MAX);
    if (numch < 0) {
      /* Invalid multibyte character sequence. */
      numch = 1;
      *wc = 0;
      local_err = TRUE;
    } else {
      *wc = wchar;
    }  /* if */
  }
#endif /* USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING */
  if (err != NULL) *err = local_err;
  return numch;
}  /* mbc_to_wide_char */

#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */

unsigned long extract_wide_char_from_string(char *str)
/*
str points at a wide character represented as a sequence of normal chars.
Extract the wide character value and return it.
*/
{
  unsigned long wc = 0;
  unsigned char ch;
  int           i;

  for (i = 0; i < (int)targ_sizeof_wchar_t; i++) {
    if (targ_little_endian) {
      ch = (unsigned char)str[(targ_sizeof_wchar_t - 1) - i];
    } else {
      ch = (unsigned char)str[i];
    }  /* if */
    wc <<= targ_char_bit;
    wc |= ch;
  }  /* for */
  return wc;
}  /* extract_wide_char_from_string */

#if !STANDALONE_UTILITY_PROGRAM

/*
Macro that returns TRUE if "ch" is a directory separator character.
*/
#if __MICROSOFT_OS__
#define is_dir_separator(ch)						\
  ((ch) == DIRECTORY_SEPARATOR ||					\
   (ch) == '\\')
#else /* __MICROSOFT_OS__ */
#define is_dir_separator(ch)						\
  ((ch) == DIRECTORY_SEPARATOR)
#endif /* __MICROSOFT_OS__ */

static void append_dir_name(a_text_buffer_ptr	buf,
			    char		*dir_name)
/*
Add "dir_name" to the end of the directory name specified by "buf".
*/
{
  char		*ptr = dir_name;
  char		*dir_start;
  int		length;
  a_boolean	starts_with_separator;

  while (*ptr != '\0') {
    /* Skip past any delimiter characters. */
    starts_with_separator = is_dir_separator(*ptr);
    while (is_dir_separator(*ptr)) ptr++;
    /* Save the position of the start of the directory name. */
    dir_start = ptr;
    /* Find the end of the directory. */
    while (*ptr != '\0' && !is_dir_separator(*ptr)) ptr++;
    length = ptr - dir_start;
    if (length == 1 && *dir_start == '.') {
      /* "." for the current directory.  Ignore it. */
    } else if (length == 2 &&
               strncmp(dir_start, "..", 2) == 0) {
      /* ".." (parent directory).  Remove the last directory component from
         the buffer. */
      /* Get a pointer to the end of the buffer so far. */
      char	*buf_ptr = &buf->buffer[buf->size - 1];
      char	*orig_buf_ptr = buf_ptr;
      if (buf->size == 0) {
        /* We are already at the start of the buffer. */
#if __MICROSOFT_OS__
      } else if (buf->size == 2 && has_drive_specification(buf->buffer)) {
        /* On Windows, we are back to something like "C:".  Don't go any
           further. */
#endif /* __MICROSOFT_OS */
      } else {
        /* Back up the start of the previous directory component. */
        while (!is_dir_separator(*buf_ptr)) --buf_ptr;
        buf->size -= orig_buf_ptr - buf_ptr + 1;
      }  /* if */
    } else if (length > 0) {
      /* If there was a separator, or if we already have a directory name,
         add a separator now.  The separator needs to be suppressed at
         the start of a Windows file name that has a drive specification. */
      if (starts_with_separator || buf->size != 0) {
        add_char_to_text_buffer(buf, DIRECTORY_SEPARATOR);
      }  /* if */
      /* Add the directory name to the buffer. */
      add_to_text_buffer(buf, dir_start, (sizeof_t)(length));
    }  /* if */
  }   /* while */
}  /* append_dir_name */


static char *normalize_dir_name(char			*dir_name,
				a_text_buffer_ptr	buf,
				a_boolean		is_partial_file_name)
/*
Convert "dir_name" into a canonical form so that it can be compared with
other normalized directory names.

For example, if the current directory is "/a/b", then a directory named
"../c" will be normalized to "/a/c".  Note that even absolute path
names must be normalized as "/a/b/../c" should produce the same result
as "/a/c".

buf is a text buffer that is used to construct the normalized file name.

is_partial_file_name is TRUE if the file names are not known to be relative
to the current directory.
*/
{
  reset_text_buffer(buf);
  if (!is_absolute_file_name(dir_name) && !is_partial_file_name) {
    /* A relative file name.  Start with the current directory name. */
    append_dir_name(buf, current_directory_name);
  } else {
#if __MICROSOFT_OS__
    /* If the path is absolute, but lacks a drive specification, use the
       drive from the current directory name. */
    if (!has_drive_specification(dir_name)) {
      check_assertion(has_drive_specification(current_directory_name));
      add_to_text_buffer(buf, current_directory_name, 2);
    }  /* if */
#endif /* __MICROSOFT_OS__ */
  }  /* if */
  /* Add the specified directory name. */
  append_dir_name(buf, dir_name);
  /* Terminate the string. */
  add_char_to_text_buffer(buf, '\0');
  return buf->buffer;
}  /* normalize_dir_name */


static a_text_buffer_ptr
		dir_buffer1;
static a_text_buffer_ptr
		dir_buffer2;
				/* Text buffers used to construct a
				   normalized directory name. */


int compare_dir_names(char	*dir1,
		      char	*dir2,
		      a_boolean	is_partial_file_name)
/*
Compare the directory names specified by dir1 and dir2.  Return zero if
they are the same.

is_partial_file_name is TRUE if the file names are not known to be relative
to the current directory.
*/
{
  int	result;

  /* The first time this routine is called, allocate text buffers used
     to construct the normalized directory names. */
  if (dir_buffer1 == NULL) {
    dir_buffer1 = alloc_text_buffer(128);
    dir_buffer2 = alloc_text_buffer(128);
  }  /* if */
  dir1 = normalize_dir_name(dir1, dir_buffer1, is_partial_file_name);
  dir2 = normalize_dir_name(dir2, dir_buffer2, is_partial_file_name);
  result = compare_file_chars(dir1, dir2);
  return result;
}  /* compare_dir_names */


int f_compare_file_names(char		*file1,
	 		 char		*file2,
		         a_boolean	ignore_delimiters,
			 a_boolean	is_partial_file_name)
/*
Return zero if file1 and file2 name the same file.  ignore_delimiters
is TRUE if the file names are from #include directives and still have
the '"' or '<' delimiters.  is_partial_file_name is TRUE if the
file names are not known to be relative to the current directory.
*/
{
  char		*start1 = file1;
  char		*start2 = file2;
  char		*file_start1;
  char		*file_start2;
  char		*end1;
  char		*end2;
  a_boolean	match = FALSE;
  char		saved_delim1;
  char		saved_delim2;

  /* If we are ignoring delimiters, temporarily replace the trailing
     delimiter with a null. */
  if (ignore_delimiters) {
    end1 = start1 + strlen(file1) - 1;
    saved_delim1 = *end1;
    *end1 = '\0';
    end2 = start2 + strlen(file2) - 1;
    saved_delim2 = *end2;
    *end2 = '\0';
    /* Increment the starting point past the delimiters. */
    start1++;
    start2++;
  }  /* if */
  /* Find the start of the actual file name component of the two files. */
  file_start1 = start_of_file_name(start1);
  file_start2 = start_of_file_name(start2);
  if (compare_file_chars(file_start1, file_start2) == 0) {
    /* Only the directory names only if the file name components match. */
    char	*dir1;
    char	*dir2;
    /* Normalize the directory names so that "./x.h" and "x.h" will
       compare equal. */
    dir1 = directory_of(start1);
    dir2 = directory_of(start2);
    if (compare_dir_names(dir1, dir2, is_partial_file_name) == 0) {
      match = TRUE;
    }  /* if */
  }  /* if */
  if (ignore_delimiters) {
    /* Restore the original delimiter characters. */
    *end1 = saved_delim1;
    *end2 = saved_delim2;
  }  /* if */
  /* Convert the boolean result into a strcmp-like result value. */
  return match ? 0 : 1;
}  /* f_compare_file_names */

#endif /* !STANDALONE_UTILITY_PROGRAM */

void host_envir_one_time_init(void)
/*
Do one-time initialization related to host specific processing.  This
is done after command line processing.
*/
{
#if !STANDALONE_UTILITY_PROGRAM
  /* Note that these are translation unit variables, but they are not
     reinitialized for each translation unit.  They retain the value set
     for the primary translation unit, but are overwritten when loading
     exported template files. */
  register_trans_unit_variable(incl_search_path);
  register_trans_unit_variable(sys_incl_search_path);
#if MODULE_ID_NEEDED
  register_trans_unit_variable(module_id);
#endif /* MODULE_ID_NEEDED */
#endif /* !STANDALONE_UTILITY_PROGRAM */
}  /* host_envir_one_time_init */


void host_envir_trans_unit_init(void)
/*
Initialize variables that are specific to a given translation unit.
*/
{
#if MODULE_ID_NEEDED
  module_id = NULL;
#endif /* MODULE_ID_NEEDED */
}  /* host_envir_trans_unit_init */


void host_envir_early_init(void)
/*
One time initialization that must take place early on in the front end.
This is done before command line processing.
*/
{
  char  *ptr;
  /* Set handlers for unusual abort signals. */
  set_signal_handlers();
#if SVR4_TRAP_NULL_POINTER_REFERENCES
  svr4_trap_null_pointer_references();
#endif /* SVR4_TRAP_NULL_POINTER_REFERENCES */
  /* Get the current directory name. */
  ptr = get_curr_dir_name();
  current_directory_name = (char *)alloc_general((sizeof_t)strlen(ptr) + 1);
  (void)strcpy(current_directory_name, ptr);
  preinclude_file_name = NULL;
  template_search_path = NULL;
  template_search_path_tail = NULL;
}  /* host_envir_early_init */


void host_envir_init(void)
/*
Initialize static variables related to the host specific routines. 
This is done as a subroutine (rather than relying on static initialization)
so that it can be redone to compile more than one source file in a single
invocation of the front end.
*/
{
  dir_and_file_buffer = NULL;
}  /* host_envir_init */

/*
The host_util.h file is used to define functions that are used by both
the front end, and utility programs such as the prelinker.  Include the
file here to define these functions for the front end.
*/

#include "host_util.h"


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1991 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
