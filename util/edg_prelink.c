/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1993 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

Prelink utility for template instantiation.

*/

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <malloc.h>
#include "basics.h"
#include "host_envir.h"
#include "target.h"
#include "edg_prelink.h"
#include "decode.h"

#define DEBUG 1

/*
The getopt.h include file will provide either the declarations needed
to use the system getopt routine or, if no system version is available,
the body of our own version of the getopt routine.
*/
#include "getopt.h"

/* Forward declarations of pointer types required before their definitions. */
typedef struct a_pl_input_file *a_pl_input_file_ptr;
typedef struct a_pl_object_file *a_pl_object_file_ptr;

/* An element of a list of input files that can instantiation a
   given symbol. */
typedef struct a_pl_instantiation_site *a_pl_instantiation_site_ptr;
typedef struct a_pl_instantiation_site {
  a_pl_instantiation_site_ptr
		next;
			/* Next instantiation site for the symbol. */
  a_pl_input_file_ptr
		input_file;
			/* Pointer to an input file that can generate
			   the instantiation. */
} a_pl_instantiation_site;


/* Description of a symbol from an object file.  This structure is
   used for both the representation of the symbol from the object file
   and a description of the symbol in the global symbol table.  Most of
   the fields are only used by global symbol table symbols. */
typedef struct a_pl_symbol *a_pl_symbol_ptr;
typedef struct a_pl_symbol {
  a_pl_symbol_ptr
		next;
			/* The next symbol associated with an object file. */
  a_pl_symbol_ptr
		next_in_symbol_table;
			/* The next symbol in the global symbol table list. */
  a_pl_symbol_ptr
		next_in_info_file;
			/* The next symbol in an instantiation info file. */
  a_pl_input_file_ptr
		instantiation_file;
			/* The input file responsible for instantiating this
			   symbol. */
  a_pl_instantiation_site_ptr
		possible_instantiation_sites;
			/* List of input files capable of instantiating a
			   template. */
  a_pl_symbol_ptr
		global_sym;
			/* Pointer to the global symbol table entry for this
			   name.  This is present for symbols associated
			   with object files.  This allows you to revisit
			   a symbol without having to look it up again. 
			   For special symbols (such as __CBI__ names) this
			   points to the global symbol for the symbol to which
			   the special symbol refers (e.g., __CBI__xxx points
			   to the global symbol for xxx). */
  long		number_of_references;
			/* The number of object files that have been linked
			   into the executable that reference the symbol.
			   A reference can be either an undefined entry
			   from the object file or a TIR entry. */
  a_pl_input_file_ptr
		last_referenced_from;
			/* Pointer to the last input file that referenced the
			   symbol.  This is used to prevent the referenced flag
			   from being updated more than once in cases where
			   the input file has both an undefined symbol
			   entry and a TIR flag. */
  char		*name;
			/* Name of the symbol. */
  a_byte_boolean
		referenced;
			/* The symbol has been referenced by an object file
			   that has been linked into the executable.
			   A reference can be either an undefined entry
			   from the object file or a TIR entry. */
  a_byte_boolean
		defined;
			/* The symbol is defined.  This is set when an
			   object file that is included in the link defines
		           the symbol.  Not set for tentative definitions. */
  a_byte_boolean
		tentative_definition;
			/* The symbol has a tentative definition.  This is
			   set when an object file that is included in the link
			   provides a "common" or tentative definition of
			   the symbol. */
  a_byte_boolean
		multiple_definition;
			/* Set if multiple object files included in the link
			   define the symbol. */
  a_byte_boolean
		is_template;
			/* Set if an object file that is included in the link
			   contains a CBI or TIR flag for the symbol. */
  a_byte_boolean
		can_be_instantiated;
			/* Set if an object file that is included in the link
			   contains a CBI flag for the symbol. */
  a_byte_boolean
		do_not_instantiate;
			/* Set if an object file that is included in the link
			   contains a DNI flag for the symbol.  This flag
			   will prevent the prelinker from assigning the
			   symbol to any file, not just the file that contains
			   the DNI flag. */
  a_byte_boolean
		instantiated;
			/* Indicates that the symbol has been instantiated.
			   This is set when it has been determined that
			   an object file has provided the necessary
			   instantiation and is also set when a symbol
			   is assigned to an object file for instantiation. */
  a_pl_input_file_ptr
		defined_in;
			/* The input file in which the symbol was defined. */
} a_pl_symbol;



/* Structure that represents a single object file.  This could be a
   normal object file or an object within an archive. */
typedef struct a_pl_object_file {
  a_pl_object_file_ptr
		next;
			/* For an archive, points to the next object file
			   in the archive. */
  char		*filename;
			/* Name of the object file.  Same as the input
			   filename for an object (.o) file. */
  a_pl_symbol_ptr
		symbols;
			/* Points to a list of symbols associated with this
			   object file. */
  a_byte_boolean
		included_in_output;
			/* TRUE when the file is part of the resulting
			   output.  This is always TRUE for .o files but
			   is only TRUE for objects in an archive if the
			   object is needed to resolve a reference. */
} a_pl_object_file;


/* Structure that represents a single input (.o or .a) file. */
typedef struct a_pl_input_file {
  a_pl_input_file_ptr
		next;
			/* Next entry on the list. */
  char		*filename;
			/* Name of the input file. */
  char		*info_filename;
			/* Name of the instantiation information file
			   associated with this file (if one exists).
			   NULL otherwise. */
  a_pl_object_file_ptr
		objects;
			/* When is_archive is FALSE this points to a single
			   object file.  When is_archive is TRUE this points
			   to a list of object files. */
  a_pl_symbol_ptr
		info_list;
			/* List of symbols to be instantiated in this file. */
  a_byte_boolean
		is_archive;
			/* TRUE if the input file is an archive (.a) file. */
  a_byte_boolean
		info_file_updated;
			/* TRUE if the instantiation information file needs
			   to be rewritten because changes have been made
			   to the info list. */
  a_byte_boolean
		recompile;
			/* TRUE if, because of changes in the instantiation
			   list, the file needs to be recompiled. */
  
} a_pl_input_file;

/* Available lists for dynamically allocated structures. */
static a_pl_input_file_ptr		avail_pl_input_files = NULL;
static a_pl_object_file_ptr		avail_pl_object_files = NULL;
static a_pl_symbol_ptr			avail_pl_symbols = NULL;
static a_pl_instantiation_site_ptr	avail_pl_instantiation_sites = NULL;

/* List of input files to be processed. */
static a_pl_input_file_ptr	pl_input_files = NULL;
static a_pl_input_file_ptr	pl_input_file_tail = NULL;

/* Pointer to the file where the "nm" command output can be read. */
static FILE			*pl_command_output;

/* Pointer to the head of the global symbol list. */
static a_pl_symbol_ptr		pl_symbol_table_head = NULL;

/* Pointer to a dynamically allocated buffer in which filenames can
   be manipulated.  The actual size is based on the longest filename
   specified on the command line. */
static char			*pl_filename_buffer;

/* Determines whether assignment information should be displayed. */
static a_boolean		verbose = TRUE;

/* TRUE if info files should be updated but compilations not done. */
static a_boolean		suppress_compilation = FALSE;

/* TRUE if we should give up after a certain number of iterations under
   the assumption that we've run into an instantiation loop. */
static a_boolean		limit_recursion = TRUE;

typedef enum /* an_nm_format_kind */ {
	nmfk_default,
		/* SunOS 4.1. */
	nmfk_solaris,
		/* Solaris 2. */
	nmfk_SGI,
		/* Silicon Graphics. */
	nmfk_SVR4,
		/* Motorola 88K SVR4. */
        nmfk_HPUX,
		/* HP/UX. */
        nmfk_CLIX,
		/* Clipper (Intergraph). */
	nmfk_lst
} an_nm_format_kind;

/* The kind of nm output that is expected. */
static an_nm_format_kind	nm_format = nmfk_default;

/* TRUE if we should simply ignore invalid nm output lines. */
static a_boolean		ignore_invalid_nm_output = FALSE;

/* String that is used as the prefix of all diagnostic messages generated
   by the prelinker. */
static char *message_prefix;

/* TRUE if external names have an extra underscore prefix.  Can be
   modified by a command line option. */
static a_boolean		skip_underscore_prefix
                                    = TARG_EXTERNAL_NAMES_GET_UNDERSCORE_ADDED;


#if DEBUG
static int pl_debug_level = 0;
static void pl_db_symbol(a_pl_symbol_ptr psp,
                         char            *prefix_string);
#endif /* DEBUG */

/*
Lines from "nm" are read into this buffer for analysis.
*/
#define PL_INPUT_LINE_SIZE 32767
typedef char		a_pl_input_line[PL_INPUT_LINE_SIZE];
static a_pl_input_line	pl_input_line;

/* The symbol table used for simulating the link. */
#define PL_SYMBOL_TABLE_SIZE	10007
static a_pl_symbol_ptr	pl_symbol_table[PL_SYMBOL_TABLE_SIZE];

/* The multiplier used in the hash algorithm that generates an index
   in the hash table from an identifier name string.  Do not change
   without investigating the hash table performance that results.
   Prime values are likely to work better than non-prime values. */
#define PL_HASH_FACTOR 73


void pl_internal_error(char*   error_string)
/*
Prints an internal error message and exits with a catastrophic error
exit status.
*/
{
  fprintf(stderr, "%s: %s\n", message_prefix, error_string);
  exit (RC_CATASTROPHE);
}  /* pl_internal_error */


typedef enum /*a_pl_error_code*/ {
  pl_ec_no_longer_needed,
  pl_ec_assigned_to_file,
  pl_ec_message_prefix,
  pl_ec_executing,
  pl_ec_unrecognized_option,
  pl_ec_error,
  pl_ec_out_of_memory,
  pl_ec_invalid_input,
  pl_ec_bad_instantiation_information_file,
  pl_ec_invalid_nm_format_option,
  pl_ec_command_line_error,
  pl_ec_instantiation_loop,
  pl_ec_lib_file_not_found,
  pl_ec_error_occurred_during_name_decoding
} a_pl_error_code;


static char *pl_error_text(a_pl_error_code error_code)
/*
For a given error code, return a pointer to the associated error
string.
*/
{
  char*	m;
  switch (error_code) {
  case pl_ec_no_longer_needed:
    m = "%s: %s no longer needed in %s\n";
    break;
  case pl_ec_assigned_to_file:
    m = "%s: %s assigned to file %s\n";
    break;
  case pl_ec_message_prefix:
    m = "C++ prelinker";
    break;
  case pl_ec_executing:
    m = "%s: executing: %s\n";
    break;
  case pl_ec_unrecognized_option:
    m = "unrecognized option: %s\n";
    break;
  case pl_ec_error:
    m = "%s: error: ";
    break;
  case pl_ec_out_of_memory:
    m = "out of memory";
    break;
  case pl_ec_invalid_input:
    m = "invalid input format";
    break;
  case pl_ec_bad_instantiation_information_file:
    m = "bad instantiation information file -- instantiation assigned to more than one file";
    break;
  case pl_ec_invalid_nm_format_option:
    m = "invalid nm format option";
    break;
  case pl_ec_command_line_error:
    m = "command line error";
    break;
  case pl_ec_instantiation_loop:
    m = "instantiation loop";
    break;
  case pl_ec_lib_file_not_found:
    m = "library \"%s\" does not exist in the specified library directories\n";
    break;
  case pl_ec_error_occurred_during_name_decoding:
    m = "an error occurred during name decoding of \"%s\"";
    break;
  default:
    pl_internal_error("invalid error code");
  }  /* switch */
  return m;
}  /* pl_error_text */


static void pl_error(a_pl_error_code	error_code,
                     char		*insertion_string)
/*
Prints an error message and exits with an error exit status.  A string
may be inserted into the message by passing a pointer to the string
to be inserted in inseration_string.  This will only be used if
the error text contains a corresponding %s.  If the message contains such
a %s, insertion_string must not be NULL.
*/
{
  fprintf(stderr, pl_error_text(pl_ec_error), message_prefix);
  fprintf(stderr, pl_error_text(error_code), insertion_string);
  fprintf(stderr, "\n");
  exit (RC_ERROR);
}

static char *pl_malloc_with_check(sizeof_t size)
/*
Interface to malloc that allocates "size" bytes.  Checks for failure of 
allocation and generates a catastrophic error.
*/
{
  char *ptr;

  if ((ptr = (char *)malloc(size)) == NULL) {
    pl_error(pl_ec_out_of_memory, (char *)NULL);
  } /* if */
  return (ptr);
}  /* pl_malloc_with_check */


static a_pl_input_file_ptr alloc_pl_input_file(void)
/*
Allocate an input file, initialize it, and return a pointer to it.
*/
{
  a_pl_input_file_ptr		pifp;

  if (avail_pl_input_files != NULL) {
    pifp = avail_pl_input_files;
    avail_pl_input_files = pifp->next;
  } else {
    pifp = (a_pl_input_file_ptr)pl_malloc_with_check(sizeof(a_pl_input_file));
  }  /* if */
  pifp->next = NULL;
  pifp->filename = NULL;
  pifp->info_filename = NULL;
  pifp->info_list = NULL;
  pifp->objects = NULL;
  pifp->is_archive = FALSE;
  pifp->info_file_updated = FALSE;
  pifp->recompile = FALSE;
  return pifp;
}  /* alloc_pl_input_file */


static void free_pl_input_file(a_pl_input_file_ptr pifp)
/*
Return an input file to the available list.
*/
{
  pifp->next = avail_pl_input_files;
  avail_pl_input_files = pifp;
}  /* free_pl_input_file */


static a_pl_object_file_ptr alloc_pl_object_file(void)
/*
Allocate an object file, initialize it, and return a pointer to it.
*/
{
  a_pl_object_file_ptr		pofp;

  if (avail_pl_object_files != NULL) {
    pofp = avail_pl_object_files;
    avail_pl_object_files = pofp->next;
  } else {
    pofp =
          (a_pl_object_file_ptr)pl_malloc_with_check(sizeof(a_pl_object_file));
  }  /* if */
  pofp->next = NULL;
  pofp->filename = NULL;
  pofp->symbols = NULL;
  pofp->included_in_output = FALSE;
  return pofp;
}  /* alloc_pl_object_file */


static void free_pl_object_file(a_pl_object_file_ptr pofp)
/*
Return an object file to the available list.
*/
{
  pofp->next = avail_pl_object_files;
  avail_pl_object_files = pofp;
}  /* free_pl_object_file */


static a_pl_symbol_ptr alloc_pl_symbol(void)
/*
Allocate a symbol, initialize it, and return a pointer to it.
*/
{
  a_pl_symbol_ptr		psp;

  if (avail_pl_symbols != NULL) {
    psp = avail_pl_symbols;
    avail_pl_symbols = psp->next;
  } else {
    psp = (a_pl_symbol_ptr)pl_malloc_with_check(sizeof(a_pl_symbol));
  }  /* if */
  psp->next = NULL;
  psp->next_in_symbol_table = NULL;
  psp->next_in_info_file = NULL;
  psp->global_sym = NULL;
  psp->number_of_references = 0;
  psp->instantiation_file = NULL;
  psp->last_referenced_from = NULL;
  psp->possible_instantiation_sites = NULL;
  psp->referenced = FALSE;
  psp->defined = FALSE;
  psp->tentative_definition = FALSE;
  psp->multiple_definition = FALSE;
  psp->is_template = FALSE;
  psp->can_be_instantiated = FALSE;
  psp->do_not_instantiate = FALSE;
  psp->instantiated = FALSE;
  psp->defined_in = NULL;
  return psp;
}  /* alloc_pl_symbol */


static a_pl_instantiation_site_ptr alloc_pl_instantiation_site(void)
/*
Allocate an instantiation_site, initialize it, and return a pointer to it.
*/
{
  a_pl_instantiation_site_ptr		pisp;

  if (avail_pl_instantiation_sites != NULL) {
    pisp = avail_pl_instantiation_sites;
    avail_pl_instantiation_sites = pisp->next;
  } else {
    pisp = (a_pl_instantiation_site_ptr)
                       pl_malloc_with_check(sizeof(a_pl_instantiation_site));
  }  /* if */
  pisp->next = NULL;
  pisp->input_file = NULL;
  return pisp;
}  /* alloc_pl_instantiation_site */


static void free_pl_instantiation_site(a_pl_instantiation_site_ptr pisp)
/*
Return an instantiation_site to the available list.
*/
{
  pisp->next = avail_pl_instantiation_sites;
  avail_pl_instantiation_sites = pisp;
}  /* free_pl_instantiation_site */


static void free_pl_symbol(a_pl_symbol_ptr psp)
/*
Return an input file to the available list.
*/
{
  psp->next = avail_pl_symbols;
  avail_pl_symbols = psp;
}  /* free_pl_symbol */


a_boolean pl_read_input_line(FILE* input_file)
/*
Reads a line of input from input_file.  Returns TRUE if a line of
input is being returned.  Returns FALSE at end-of-file.  Sets "line_size"
to the number of characters read not including the trailing null character.
*/
{
  register char*    buffer_pos = &pl_input_line[0];
  register int      size = 0;
  register int      ch;
  a_boolean         result;

  while ((ch = getc(input_file)), ch != EOF && ch != '\n') {
    if (++size > PL_INPUT_LINE_SIZE) {
      pl_internal_error("pl_read_input_line: input line too long.");
    }  /* if */
    *buffer_pos++ = ch;
  }  /* while */
  
  /* Terminate string with a null character. */
  *buffer_pos++ = '\0';

  /* Determine whether to return end-of-file (FALSE). */
  result = TRUE;
  if (ch == EOF && size == 0) result = FALSE;

  return (result);
}  /* pl_read_input_line */



static char *pl_copy_string(char *source)
/*
Allocate space for a copy of the string and make a copy.  Return a pointer
to the copy.
*/
{
  char	*dest;
  dest = (char *)malloc(strlen(source) + 1);
  strcpy(dest, source);
  return dest;
}  /* pl_copy_string */


static void pl_invalid_input(void)
/*
Issue an invalid input error and exit.
*/
{
  if (!ignore_invalid_nm_output) pl_error(pl_ec_invalid_input, (char *)NULL);
}  /* pl_invalid_input */


#define NAME_DECODE_BUFFER_SIZE 32767
static char *pl_decoded_name(char* encoded_name)
/*
Return a pointer to a temporary buffer containing a decoded name.
*/
{
  a_boolean	error;
  a_boolean	buffer_overflow;
  char		*result;
  static char	decode_buffer[NAME_DECODE_BUFFER_SIZE];

  decode_identifier(encoded_name, decode_buffer, NAME_DECODE_BUFFER_SIZE,
                    &error, &buffer_overflow);
  result = decode_buffer;
  if (error) {
    pl_error(pl_ec_error_occurred_during_name_decoding, encoded_name);
  }  /* if */
  return result;
}  /* pl_decoded_name */


static a_boolean pl_scan_solaris_nm_line(char	**name1,
				         char	**name2,
				         char	*type,
				         char	**symbol_name)
/*
Read the output of the nm command.  This routine is written to accept
the output of the nm command on Solaris using the -p -x and -R options.

The nm output is expected to look like:

s.o:

0000000000 f s.o:s.c
0000000000 t s.o:static_func
0000000000 n s.o:DATA.
0000000008 n s.o:RDATA.
0000000000 b s.o:static_int
0000000000 D s.o:glob_def_int
0000000000 U s.o:extern_func
0000000004 D s.o:common_int
0000000000 U s.o:extern_int
0000000008 T s.o:defined_func


/usr/lib/libbsdmalloc.a[malloc.bsd43.o]:

0000000000 f /usr/lib/libbsdmalloc.a:malloc.bsd43.o:malloc.bsd43.c
0000000356 t /usr/lib/libbsdmalloc.a:malloc.bsd43.o:morecore
0000000004 b /usr/lib/libbsdmalloc.a:malloc.bsd43.o:pagesz
0000000000 b /usr/lib/libbsdmalloc.a:malloc.bsd43.o:pagebucket
0000000920 t /usr/lib/libbsdmalloc.a:malloc.bsd43.o:findbucket
0000000008 b /usr/lib/libbsdmalloc.a:malloc.bsd43.o:nextf
0000000528 T /usr/lib/libbsdmalloc.a:malloc.bsd43.o:free
0000000000 T /usr/lib/libbsdmalloc.a:malloc.bsd43.o:malloc
0000000592 T /usr/lib/libbsdmalloc.a:malloc.bsd43.o:realloc

/usr/lib/libbsdmalloc.a[xyz.o]:

0000000000 U /usr/lib/libbsdmalloc.a:xyz.o:sbrk
0000000000 U /usr/lib/libbsdmalloc.a:xyz.o:getpagesize
0000000000 U /usr/lib/libbsdmalloc.a:xyz.o:.div
0000000000 D /usr/lib/libbsdmalloc.a:xyz.o:realloc_srchlen
0000000000 U /usr/lib/libbsdmalloc.a:xyz.o:bcopy

Returns TRUE if the line contains symbol information; returns FALSE
if the line is a blank line, or a header line that should not be
processed further.
*/
{
  a_boolean	result = TRUE;
  char		*pos;
  char		*rest_of_line;
  char		ch;

  /* Clear the pointers to the returned values. */
  *name1 = *name2 = *symbol_name = NULL;
  /* Find the first colon which terminates either the archive or the
     file name. */
  pos = strchr(pl_input_line, ':');
  if (pos == NULL) {
    /* Ignore blank lines.  A nonblank line that doesn't contain
       a colon is an error. */
    if (pl_input_line[0] != '\0') pl_invalid_input();
    result = FALSE;
  } else if (*(pos+1) == '\0') {
    /* A line that just contains a string like "xxx:".  This is an
       archive header that should be ignored. */
    result = FALSE;
  } else {
    /* Skip over the first field which is expected to contain the
       value field.  Skip to a blank. */
    pos = pl_input_line;
    while((ch = *pos), ch != ' ' && ch != '\0') pos++;
    /* Look for blank after value. */
    if (*pos++ != ' ') pl_invalid_input();
    /* Now look for a nonblank. */
    while (*pos == ' ') pos++;
    /* Get the type code. */
    *type = *pos++;
    if (!isalpha((unsigned char)(*type))) pl_invalid_input();
    /* Look for blank after type. */
    if (*pos++ != ' ') pl_invalid_input();
    /* Now look for a nonblank. */
    while (*pos == ' ') pos++;
    /* Note that the Solaris format is not expected to contain
       extra leading underscores. */
    rest_of_line = pos;
    pos = strchr(rest_of_line, ':');
    /* Replace the first colon with a NULL. */
    *pos = '\0';
    *name1 = rest_of_line;
    rest_of_line = pos + 1;
    pos = strchr(rest_of_line, ':');
    if (pos == NULL) {
      /* No second name exists. */
      *name2 = NULL;
    } else {
      /* Replace the second colon with a NULL to terminate the file
         name. */
      *pos = '\0';
      *name2 = rest_of_line;
      rest_of_line = pos + 1;
    }  /* if */
    *symbol_name = rest_of_line;
  }  /* if */
  return result;
}  /* pl_scan_solaris_nm_line */


static a_boolean pl_scan_alternate_nm_line(char	**name1,
				      char	**name2,
				      char	*type,
				      char	**symbol_name)
/*
Read the output of the nm command.  This routine is written to accept
the output of the nm command on systems such as HP/UX and Motorola 88000 SVR4
using the -p, -x and -r options.

For HP/UX the nm output is expected to look like:

t.o:
t.o:                0000000200 T  main
t.o:                0000000000 U  _main
t.o:                0000000240 t  __ct__1AFv
t.o:                0000000000 U  __nw__FUi
t.o:                0000000000 U  printf
t.o:                0000000000 U  __dl__FPv

t2.o:
t2.o:               0000000004 c  __TIR____ct__10A__pt__2_iFv
t2.o:               0000000004 c  __CBI____ct__10A__pt__2_iFv
t2.o:               0000000200 T  main
t2.o:               0000000248 T  __cgi__t2_c_Fri_Sep_10_15_09_24_1993_
t2.o:               0000000000 U  _main
t2.o:               0000000000 U  __ct__10A__pt__2_iFv

.../lib/libC.a[main.o]:
.../lib/libC.a:      0000000004 c  __head
.../lib/libC.a:      0000000200 T  _main
.../lib/libC.a:      0000000000 U  __call_ctors__Fv
.../lib/libC.a:      0000000232 T  __cgi__main_c_Fri_Sep_10_14_43_17_1993_

.../lib/libC.a[placenew.o]:
.../lib/libC.a:      0000000200 T  __nw__FUiPv
.../lib/libC.a:      0000000216 T  __cgi__placenew_c_Fri_Sep_10_14_43_20_1993_

For SVR4 the nm output is expected to look like:

s.o:

0000000000 D s.o:glob_def_int
0000000036 T s.o:defined_func
0000000000 U s.o:extern_int
0000000000 U s.o:extern_func
0000000008 D s.o:common_int


/usr/lib/libapg.a[new.o]:

0000000000 U new.o:_getpid
0000000000 U new.o:_kill


/usr/lib/libapg.a[hack.o]:

0000000000 U hack.o:_fcvt
0000000000 U hack.o:_ecvt
0000000000 U hack.o:_fcntl


For CLIX the nm output is expected to look like:

s.o:
s.o:                00000000 t _static_func
s.o:                00000004 T _defined_func
s.o:                00000000 U _extern_int
s.o:                00000000 U _extern_func
s.o:                00000096 d _static_int
s.o:                00000100 D _glob_def_int
s.o:                00000004 C _common_int

/usr/lib/liby.a[libmai.o]:
/usr/lib/liby.a:    00000000 T _main
/usr/lib/liby.a:    00000000 U _yyparse

/usr/lib/liby.a[libzer.o]:
/usr/lib/liby.a:    00000000 T _yyerror
/usr/lib/liby.a:    00000000 U __iob
/usr/lib/liby.a:    00000000 U _fprintf


Returns TRUE if the line contains symbol information; returns FALSE
if the line is a blank line, or a header line that should not be
processed further.
*/
{
  a_boolean	result = TRUE;
  char		*pos;
  char		*rest_of_line;
  char		ch;
  static char	*name1_buffer = NULL;
  static char	*name2_buffer = NULL;
  static a_boolean
		name2_is_NULL = FALSE;

  /* On the first call allocate a buffer that can be used to store the
     archive name. */
  if (name1_buffer == NULL) {
    name1_buffer = pl_malloc_with_check(PL_INPUT_LINE_SIZE);
    name2_buffer = pl_malloc_with_check(PL_INPUT_LINE_SIZE);
  }  /* if */
  /* Clear the pointers to the returned values. */
  *name1 = *name2 = *symbol_name = NULL;
  /* Find the first colon which terminates either the archive or the
     file name. */
  pos = strchr(pl_input_line, ':');
  if (pos == NULL) {
    /* Ignore blank lines.  A nonblank line that doesn't contain
       a colon is an error. */
    if (pl_input_line[0] != '\0') pl_invalid_input();
    result = FALSE;
  } else if (strchr(pl_input_line, ' ') == NULL) {
    char	*bracket_pos;
    /* A line that just contains a string like "xxx.o:" or "libx.a[xxx.o]:".
       This is an archive header.  Extract the archive and object file names.
       After the names are extracted the line is not processed. */
    result = FALSE;
    /* Find the "[" that separates the archive name from the object
       file name.  Use the position of the colon if no bracket is found. */
    bracket_pos = strchr(pl_input_line, '[');
    if (bracket_pos != NULL) pos = bracket_pos;
    /* Replace the "[" (or ":") with a NULL. */
    *pos = '\0';
    (void)strcpy(name1_buffer, pl_input_line);
    rest_of_line = pos + 1;
    name2_is_NULL = (bracket_pos == NULL);
    if (!name2_is_NULL) {
      /* Find the "]" that terminates the object file name. */
      pos = strchr(rest_of_line, ']');
      *pos = '\0';
      (void)strcpy(name2_buffer, rest_of_line);
     }  /* if */
  } else {
    /* Return pointers to the names from the header line. */
    *name1 = name1_buffer;
    *name2 = name2_is_NULL ? NULL : name2_buffer;
    /* On HP/UX and some other systems the file name begins each line,
       skip past this file name. */
    if (nm_format == nmfk_HPUX || nm_format == nmfk_CLIX) {
      rest_of_line = pos + 1;
    } else {
      rest_of_line = pl_input_line;
    }  /* if */
    /* The value field may optionally be preceded by one or more blanks.
       Skip over any blanks that appear here. */
    pos = rest_of_line;
    while (*pos == ' ') pos++;
    /* Skip over the first field which is expected to contain the
       value field.  Skip to a blank. */
    while((ch = *pos), ch != ' ' && ch != '\0') pos++;
    /* Look for blank after value. */
    if (*pos++ != ' ') pl_invalid_input();
    /* Now look for a nonblank. */
    while (*pos == ' ') pos++;
    /* Get the type code. */
    *type = *pos++;
    if (!isalpha((unsigned char)(*type))) pl_invalid_input();
    if (nm_format == nmfk_HPUX) {
      /* HP/UX uses lower case letters for some types. */
      switch (*type) {
        case 'c':	*type = 'C'; break;
      }  /* switch */
    }  /* if */
    /* Look for blank after type. */
    if (*pos++ != ' ') pl_invalid_input();
    if (nm_format == nmfk_CLIX) {
      /* Skip passed extra underscore at the start of every symbol if an
         underscore is present.   This is only done for CLIX. */
      if (skip_underscore_prefix && *pos == '_') pos++;
    }  /* if */
    rest_of_line = pos;
    /* If the name was not at the start of the line then it is expected
       to appear here.  Skip past the name. */
    if (nm_format != nmfk_HPUX && nm_format != nmfk_CLIX) {
      pos = strchr(rest_of_line, ':');
      rest_of_line = pos + 1;
    }  /* if */
    *symbol_name = rest_of_line;
  }  /* if */
  return result;
}  /* pl_scan_alternate_nm_line */


static a_boolean pl_scan_default_nm_line(char	**name1,
					 char	**name2,
					 char	*type,
					 char	**symbol_name)
/*
Read the output of the nm command.  This routine is written to accept
the output of the nm command on SunOS and may have to be modified
for other systems.

Default nm output is expected to look like:

i1.o:00000001 C ___CBI__f__10A__pt__2_dFv
i1.o:0000018c T _f__10A__pt__2_iFv
i1.o:0000016c T _f__Fi
i1.o:00000250 T _g__10A__pt__2_dFv
i1.o:000001ac T _g__10A__pt__2_iFv
i1.o:         U _i2__Fv
i1.o:00000110 T _main
i1.o:         U _x__Fv
i2.o:00000001 C ___CBI__f__10A__pt__2_cFv
i2.o:00000328 T ___cgi__i2_c_Mon_Oct__4_16_54_51_1993_
i2.o:         U ___nw__FUi
i2.o:000001e0 T _f__10A__pt__2_cFv
i2.o:00000284 T _f__10A__pt__2_fFv
i2.o:         U _f__Ff
i2.o:         U _f__Fi
i2.o:00000200 T _g__10A__pt__2_cFv
i2.o:000002a4 T _g__10A__pt__2_fFv
i2.o:00000110 T _i2__Fv

libi.a:
libi.a:x.o:00000004 C ___CBI__f__10A__pt__2_dFv
libi.a:x.o:00000004 C ___CBI__g__10A__pt__2_dFv
libi.a:x.o:00000004 C ___TIR__f__10A__pt__2_dFv
libi.a:x.o:00000004 C ___TIR__g__10A__pt__2_dFv
libi.a:x.o:0000012c T ___cgi__x_c_Tue_Aug_17_14_11_40_1993_
libi.a:y.o:00000110 T _x__Fv
libi.a:y.o:00000004 C ___CBI__f__10A__pt__2_dFv
libi.a:y.o:00000004 C ___CBI__g__10A__pt__2_dFv
libi.a:y.o:00000004 C ___TIR__f__10A__pt__2_dFv
libi.a:y.o:00000004 C ___TIR__g__10A__pt__2_dFv
libi.a:y.o:00000110 T _y__Fv

SGI nm output is expected to look like:

Z.a:A.o:        00000120 t __dt__1AFv
Z.a:A.o:        000001c4 t set__1AFi
Z.a:A.o:        000001d8 t __dt__1BFv
Z.a:A.o:        00000000 T foo__Fv
Z.a:A.o:        00000000 U __nw__FUi
Z.a:A.o:        0000003c D __ptbl_vec__A_C_foo_
Z.a:A.o:        000000cc T main
Z.a:A.o:        00000000 U printf
Z.a:A.o:        00000000 U __dl__FPv
Z.a:A.o:        00000000 U _gp_disp
Z.a:Q.o:        00000000 b foo
Z.a:Q.o:        00000000 t func__Fv
Z.a:Q.o:        00000000 D Name__4Test
Z.a:Q.o:        00000000 U z__4Test
Z.a:Q.o:        00000028 T __ct__4TestFv
Z.a:Q.o:        00000000 U __nw__FUi
Z.a:Q.o:        00000008 D bar
Z.a:Q.o:        000000a8 T main
Z.a:Q.o:        00000000 U _gp_disp
X.o:    00000320 t __dt__1BFv
X.o:    00000000 T __dt__1AFv
X.o:    00000000 U __dl__FPv
X.o:    00000000 D bar
X.o:    0000008c T main
X.o:    00000000 U __nw__FUi
X.o:    00000000 U printf
X.o:    00000030 D __vtbl__1B__X_C
X.o:    00000048 D __vtbl__1A
X.o:    00000000 U _gp_disp


Returns TRUE if the line contains symbol information; returns FALSE
if the line is a blank line, or a header line that should not be
processed further.
*/
{
  a_boolean	result = TRUE;
  char		*pos;
  char		*rest_of_line;
  char		ch;

  /* Clear the pointers to the returned values. */
  *name1 = *name2 = *symbol_name = NULL;
  /* Find the first colon which terminates either the archive or the
     file name. */
  pos = strchr(pl_input_line, ':');
  if (pos == NULL) {
    /* Ignore blank lines.  A nonblank line that doesn't contain
       a colon is an error. */
    if (pl_input_line[0] != '\0') pl_invalid_input();
    result = FALSE;
  } else if (*(pos+1) == '\0') {
    /* A line that just contains a string like "xxx:".  This is an
       archive header that should be ignored. */
    result = FALSE;
  } else {
    /* Replace the first colon with a NULL. */
    *pos = '\0';
    *name1 = pl_input_line;
    rest_of_line = pos + 1;
    pos = strchr(rest_of_line, ':');
    if (pos == NULL) {
      /* No second name exists. */
      *name2 = NULL;
    } else {
      /* Replace the second colon with a NULL to terminate the file
         name. */
      *pos = '\0';
      *name2 = rest_of_line;
      rest_of_line = pos + 1;
    }  /* if */
    pos = rest_of_line;
    if (nm_format == nmfk_SGI) {
      /* The value field may optionally be preceded by one or more blanks.
         Skip over any blanks that appear here. */
      while (*pos == ' ') pos++;
    }  /* if */
    /* Skip over the first field which is expected to contain the
       value field.  Skip to a blank. */
    while((ch = *pos), ch != ' ' && ch != '\0') pos++;
    /* Look for blank after value. */
    if (*pos++ != ' ') pl_invalid_input();
    /* Now look for a nonblank. */
    while (*pos == ' ') pos++;
    /* Get the type code. */
    *type = *pos++;
    if (!isalpha((unsigned char)(*type))) pl_invalid_input();
    /* Look for blank after type. */
    if (*pos++ != ' ') pl_invalid_input();
    /* Skip passed extra underscore at the start of every symbol if an
       underscore is present.  */
    if (skip_underscore_prefix && *pos == '_') pos++;
    *symbol_name = pos;
    if (nm_format == nmfk_SGI) {
      /* The symbol name may optionally have some extra information after it:
         ex.o:   0000034c T __ct__17A__pt__9_7istreamFv (multiext)
         We scan the rest of the line, and if we hit a blank before reaching
         the end of the line, the blank is replaced with a NULL */
      while((ch = *pos), ch != ' ' && ch != '\0') pos++;
      if (ch == ' ') *pos = '\0';
    }  /* if */
  }  /* if */
  return result;
}  /* pl_scan_default_nm_line */


static void pl_read_nm_output(void)
/*
This routine reads the output of the nm command and builds a data
structure containing the information.  The base of the structure
is a linked list of input files that can be object files or archive files.
Each input file points to one or more object files.  An input file
that is an object file points to a single object file structure
while an archive points to a linked list of object files.
Each object file points to a linked list of symbols that are referenced
or defined in that object file.
*/
{
  char			*input_filename = NULL;
  char			*object_filename = NULL;
  a_boolean		is_archive = FALSE;
  a_pl_object_file_ptr	objects_tail;
  a_pl_object_file_ptr	pofp;
  a_pl_input_file_ptr	pifp;

  while (pl_read_input_line(pl_command_output)) {
    char		*name1;
    char		*name2;
    char		type;
    char		*symbol_name;
    a_pl_symbol_ptr	psp;
    a_boolean		process_line;
#if DEBUG
    if (pl_debug_level >= 4) {
      fprintf(stderr, "%s\n", pl_input_line);
    }  /* if */
#endif /* DEBUG */

    if (nm_format == nmfk_solaris) {
      process_line = pl_scan_solaris_nm_line(&name1, &name2, &type,
                                             &symbol_name);
    } else if (nm_format == nmfk_SVR4 ||
               nm_format == nmfk_HPUX ||
               nm_format == nmfk_CLIX) {
      process_line = pl_scan_alternate_nm_line(&name1, &name2, &type,
                                               &symbol_name);
    } else {
      /* SGI uses a variant of the default format. */
      process_line = pl_scan_default_nm_line(&name1, &name2, &type,
                                             &symbol_name);
    }  /* if */
    /* Is this a line that should be skipped such as a blank line or
       header line? */
    if (!process_line) continue;
    /* See if this is the start of a new input file. */
    if (input_filename == NULL ||
        strcmp(input_filename, name1) != 0) {
      /* The start of a new input file. */
      input_filename = pl_copy_string(name1);
      /* The input file is an archive if there is also an object name in
         the input line. */
      is_archive = name2 != NULL;
      pifp = alloc_pl_input_file();
      pifp->is_archive = is_archive;
      pifp->filename = input_filename;
      objects_tail = NULL;
      /* Add this entry to the list of input files. */
      if (pl_input_files == NULL) pl_input_files = pifp;
      if (pl_input_file_tail != NULL) pl_input_file_tail->next = pifp;
      pl_input_file_tail = pifp;
      if (is_archive) {
        object_filename = NULL;
        /* Continue execution with the next line which will be the
           first object file in the archive. */
        continue;
      } else {
        /* Allocate an object file structure and link it to the input
           file. */
        pofp = alloc_pl_object_file();
        pifp->objects = pofp;
        pofp->filename = pl_copy_string(input_filename);
      }  /* if */
    }  /* if */
    /* See if this is the start of a new object file within an archive. */
    if (is_archive) {
      if (object_filename == NULL ||
          strcmp(object_filename, name2) != 0) {
        /* This is a new object file within the archive. */
        object_filename = pl_copy_string(name2);
        pofp = alloc_pl_object_file();
        pofp->filename = object_filename;
        /* Add this object file to the list of objects pointed to by the
           input file entry. */
        if (pifp->objects == NULL) pifp->objects = pofp;
        if (objects_tail != NULL) objects_tail->next = pofp;
        objects_tail = pofp;
      }  /* if */
    }  /* if */
    /* See if this is a type of line for which the symbol information should
       be recorded. */
    if (type != 'B' &&
        type != 'D' &&
        type != 'T' &&
        type != 'U' &&
        type != 'C') {
       /* Not a type of symbol that we need to process.  Only global
          symbols are processed. */
     } else {
      psp = alloc_pl_symbol();
      psp->name = pl_copy_string(symbol_name);
      /* Set symbol flags. */
      switch (type) {
        case 'B':  /* BSS symbol */
        case 'D':  /* data symbol */
        case 'T':  /* text symbol */
          psp->defined = TRUE;
          break;
        case 'U':  /* undefined symbol */
          psp->referenced = TRUE;
          break;
        case 'C':  /* common (tentative definition) */
          psp->tentative_definition = TRUE;
          break;
        default:
          break;
      }  /* switch */
      /* Link onto front of list associated with the current object file. */
      psp->next = pofp->symbols;
      pofp->symbols = psp;
    }  /* if */
  }  /* while */
  return;
}  /* pl_read_nm_output */


static void add_possible_instantiation_site(a_pl_symbol_ptr	psp,
					    a_pl_input_file_ptr	pifp)
/*
Add an input file to a list of files that can instantiate a given symbol.
*/
{
  a_pl_instantiation_site_ptr	pisp;

  /* It is possible for this routine to be called more than once for
     the same input file.  Since the input files are processed one at
     a time we can detect this by checking whether the current input
     file is already pointed to by the first entry. */
  if (psp->possible_instantiation_sites == NULL ||
      psp->possible_instantiation_sites->input_file != pifp) {
    pisp = alloc_pl_instantiation_site();
    pisp->input_file = pifp;
    pisp->next = psp->possible_instantiation_sites;
    psp->possible_instantiation_sites = pisp;
  }  /* if */
}  /* add_possible_instantiation_site */


static a_pl_symbol_ptr pl_find_symbol(char		*name,
                                      a_pl_symbol_ptr	other_sym,
				      a_boolean		add)
/*
Find a symbol entry with the specified name.  Add the name to the
list if an entry does not already exist.
*/
{
  register unsigned            hash_value = 0;
  register char                *ptr;
  a_pl_symbol_ptr	       prev_sym_ptr;
  a_pl_symbol_ptr              sym_ptr    = NULL;
  int                          bucket_number;
  int			       length;

  /* If the symbol pointer passed from the caller already contains a pointer
     to the global symbol then simply return that value.  Otherwise,
     look it up in the symbol table. */
  if (other_sym != NULL && other_sym->global_sym != NULL) {
    sym_ptr = other_sym->global_sym;
    goto symbol_found;
  }  /* if */
  length = strlen(name);
  /* Hash the symbol's name.  This involves taking the name's
     first, last, and middle 3 characters.  Of course, if the name has
     fewer than 5 characters, take the entire name. */
  if (length > 5) {
    ptr = name + (length >> 1) - 1;
    hash_value = (int)*name;
    hash_value = (hash_value * PL_HASH_FACTOR) +
                                             (int)*(name + length - 1);
    hash_value = (hash_value * PL_HASH_FACTOR) + (int)*ptr++;
    hash_value = (hash_value * PL_HASH_FACTOR) + (int)*ptr++;
    hash_value = (hash_value * PL_HASH_FACTOR) + (int)*ptr;
  } else {
    register int i;
    ptr = name;
    for (i = 0; i < length; i++) {
      hash_value = (hash_value * PL_HASH_FACTOR) + (int)*ptr++;
    }  /* for */
  }  /* if */

  /* Look in the symbol bucket saving the position in case this symbol needs
     to be added. */
  bucket_number = hash_value % PL_SYMBOL_TABLE_SIZE;
  if ((sym_ptr = pl_symbol_table[bucket_number]) != NULL) {
    prev_sym_ptr = NULL;
    do {
      if (strcmp(name, sym_ptr->name) == 0) {
        /* We have a match. */
        /* Relink the symbol header at the front of the list of headers,
           so that frequently-used headers will be found quickly. */
        if (prev_sym_ptr != NULL) {
          prev_sym_ptr->next = sym_ptr->next;
          sym_ptr->next = pl_symbol_table[bucket_number];
          pl_symbol_table[bucket_number] = sym_ptr;
        }  /* if */
        goto symbol_found;
      }  /* if */
      prev_sym_ptr = sym_ptr;
    } while ((sym_ptr = sym_ptr->next) != NULL);
  }  /* if */

  /* Exiting this loop indicates that the symbol does not exist in the table;
     allocate a symbol header for it. */
  if (add) {
    sym_ptr = alloc_pl_symbol();
    /* Add this to the list of symbols in the global symbol table. */
    sym_ptr->next_in_symbol_table = pl_symbol_table_head;
    pl_symbol_table_head = sym_ptr;

    /* Link the new header onto the front of the appropriate bucket of the
       symbol table. */
    sym_ptr->next = pl_symbol_table[bucket_number];
    pl_symbol_table[bucket_number] = sym_ptr;
    sym_ptr->name = pl_copy_string(name);
  }  /* if */

symbol_found:
  if (sym_ptr != NULL) {
    if (other_sym != NULL && other_sym->global_sym == NULL) {
      /* Record a pointer to the global symbol in the symbol passed by the
         caller. */
      other_sym->global_sym = sym_ptr;
    }  /* if */
  }  /* if */
  return sym_ptr;
}  /* pl_find_symbol */


static void pl_add_predefined_names(void)
/*
This routine is used to introduce names that the linker predefines.
This is not strictly needed because the prelinker doesn't issue
undefined errors.  This can be used if you want the prelinker
to detect such conditions.
*/
{
  char			*name;
  int			pos = 0;
  a_pl_symbol_ptr	sym;

  for (;;) {
    name = pl_predefined_names[pos++]; 
    if (name == NULL) break;
    sym = pl_find_symbol(name, (a_pl_symbol_ptr)NULL, /*add=*/TRUE);
    sym->defined = TRUE;
  }  /* for */
}  /* pl_add_predefined_names */


static void pl_add_symbols_from_object(a_pl_object_file_ptr pofp,
				       a_pl_input_file_ptr  input_file)
/*
Add all of the symbols from a given object file to the global symbol
table.  This handles regular symbols and also processes symbols that
have special meaning for the prelinker.  The special symbols begin
with the prefixes __TIR__, __CBI__, and __DNI__.  When a special symbol
is processed the related symbol (the symbol name with the prefix
removed) is updated to reflect the information provided by the special
symbol.
*/
{
  a_pl_symbol_ptr	psp;

  psp = pofp->symbols;
  while (psp != NULL) {
    a_pl_symbol_ptr	sym;
    a_boolean		is_special_symbol = FALSE;
    if (psp->name[0] == '_' && psp->name[1] == '_') {
      if (strncmp(psp->name, PL_INSTANCE_REQUIRED_PREFIX,
                  PL_INSTANCE_REQUIRED_PREFIX_LEN) == 0) {
        /* The template instance required (TIR) symbol indicates that
           this symbol is required when linking this program. This symbol
           is needed because it is not possible to determine whether
	   a symbol defined in a given file is also referenced in that file.
           The TIR information is needed to detect situations where an
           instantiation is no longer required.  The TIR flag tells
           the prelinker that the symbol is referenced and that the
           symbol is one that can be defined by a generated instantiation. */
        is_special_symbol = TRUE;
        sym = pl_find_symbol(&psp->name[PL_INSTANCE_REQUIRED_PREFIX_LEN],
                             psp, /*add=*/TRUE);
        sym->is_template = TRUE;
        sym->referenced = TRUE;
        if (sym->last_referenced_from != input_file) {
          /* Keep track of the number of references.  Only update the counter
             for the first reference in this file.  There can be multiple
             references as a result of the special symbols. */
          sym->last_referenced_from = input_file;
          sym->number_of_references++;
        }  /* if */
      } else if (strncmp(psp->name, PL_DO_NOT_INSTANTIATE_PREFIX,
                  PL_DO_NOT_INSTANTIATE_PREFIX_LEN) == 0) {
        /* The "do not instantiate" (DNI) symbol is generated as a result
           of a "do_not_instantiate" pragma in a source program.  The
           DNI symbol tells the prelinker that it may not assign the
           symbol to be instantiated by any file.  In other words,
           the user must either provide a specific definition of the symbol
           or must ensure that it is instantiated using an instantiate
           pragma or an instantiation mode such as -tused. */
        is_special_symbol = TRUE;
        sym = pl_find_symbol(&psp->name[PL_DO_NOT_INSTANTIATE_PREFIX_LEN],
                             psp, /*add=*/TRUE);
        sym->do_not_instantiate = TRUE;
      } else if (!input_file->is_archive &&
                 strncmp(psp->name, PL_CAN_BE_INSTANTIATED_PREFIX,
                  PL_CAN_BE_INSTANTIATED_PREFIX_LEN) == 0) {
        /* The "can_be_instantiated" prefix indicates that the name is
           a template and the template can be instantiated in this file.
           Don't consider this to be a possible instantiation site if
           the input file is an archive. */
        is_special_symbol = TRUE;
        sym = pl_find_symbol(&psp->name[PL_CAN_BE_INSTANTIATED_PREFIX_LEN],
                             psp, /*add=*/TRUE);
        sym->is_template = TRUE;
        if (!input_file->is_archive) {
          sym->can_be_instantiated = TRUE;
          /* Add the current input file to the list of files that could
             instantiate the symbol. */
          add_possible_instantiation_site(sym, input_file);
        }  /* if */
      }  /* if */
    }  /* if */
    if (!is_special_symbol) {
      /* The symbol is not a special symbol.  Update the global symbol
         table to reflect the kind of reference or definition. */
      sym = pl_find_symbol(psp->name, psp, /*add=*/TRUE);
      if (psp->referenced) {
        sym->referenced = TRUE;
        if (sym->last_referenced_from != input_file) {
          /* Keep track of the number of references.  Only update the counter
             for the first reference in this file.  There can be multiple
             references as a result of the special symbols. */
          sym->last_referenced_from = input_file;
          sym->number_of_references++;
        }  /* if */
      }  /* if */
      if (psp->defined) {
        if (sym->defined) {
         sym->multiple_definition = TRUE;
        } else {
          sym->defined_in = input_file;
          sym->defined = TRUE;
       }  /* if */
      }  /* if */
      sym->tentative_definition |= psp->tentative_definition;
    }  /* if */
    psp = psp->next;
  }  /* while */
  pofp->included_in_output = TRUE;
}  /* pl_add_symbols_from_object */


static a_boolean pl_any_symbols_referenced(a_pl_object_file_ptr pofp)
/*
Determine whether any of the symbols from an object file is needed
to resolve an undefined reference or a tentative definition.
*/
{
  a_pl_symbol_ptr	psp;
  a_boolean		result = FALSE;

  psp = pofp->symbols;
  while (psp != NULL) {
    a_pl_symbol_ptr	sym;
    if (psp->defined || psp->tentative_definition) {
      /* Only look the symbol up if this is a definition. */
      sym = pl_find_symbol(psp->name, psp, /*add=*/FALSE);
      if (sym != NULL && !sym->defined) {
        /* A previously undefined symbol may be resolved by a definition or
           a tentative definition.  A tentative definition may only be
           resolved by a nontentative definition. */
        if (sym->referenced ||
            (sym->tentative_definition && psp->defined)) {
          result = TRUE;
          break;
        }  /* if */
      }  /* if */
    }  /* if */
    psp = psp->next;
  }  /* while */
  return result;
}  /* pl_any_symbols_referenced */


static void pl_prelink(void)
/*
Simulate a link operation.  For each object (.o) file add all of
the names to the symbol table.
*/
{
  a_pl_input_file_ptr	pifp;

  pifp = pl_input_files;
  while (pifp != NULL) {
    a_pl_object_file_ptr	pofp;
    a_boolean			file_used_from_archive = FALSE;
    pofp = pifp->objects;
    if (!pifp->is_archive) {
      /* This is a .o file.  Add all of its symbols to the symbol table. */
      pl_add_symbols_from_object(pofp, pifp);
    } else {
      while (pofp != NULL) {
        if (!pofp->included_in_output) {
          /* If the object file is not already part of the linked object
             check whether any of its symbols resolve previously
             unresolved references. */
          if (pl_any_symbols_referenced(pofp)) {
            /* This file is needed.  Add the symbols to the global symbol
               table and set the flag that indicates a new file has been
               added from this archive. */
            pl_add_symbols_from_object(pofp, pifp);
            file_used_from_archive = TRUE;
          }  /* if */
        }  /* if */
        pofp = pofp->next;
      }  /* while */
    }  /* if */
    /* If a file was added from this archive then rescan the files in
       the archive to see if any additional files need to be added.
       This would be needed when a file in an archive references an
       file earlier in the archive. */
    if (!file_used_from_archive) pifp = pifp->next;
  }  /* while */
}  /* pl_prelink */


static void pl_read_instantiation_info_files(void)
/*
Read the existing instantiation assignment information from the
.ii files associated with the object files being processed.
*/
{
  a_pl_input_file_ptr	pifp;
  FILE			*ii_file;

  pifp = pl_input_files;
  while (pifp != NULL) {
    if (!pifp->is_archive) {
      /* Build the name of the instantiation info file.  The input filename
         is expected to be something like xyz.o.  We strip off the suffix
         and add the suffix for the instantiation information file. */
      char	*last_dot;
      strcpy(pl_filename_buffer, pifp->filename);
      last_dot = strrchr(pl_filename_buffer, '.');
      if (last_dot == NULL) {
        /* No suffix -- set last_dot as if a suffix had followed the name. */
        last_dot = pl_filename_buffer + strlen(pl_filename_buffer);
      }  /* if */
      strcpy(last_dot, INSTANTIATION_INFO_SUFFIX);
      ii_file = fopen(pl_filename_buffer, "r");
#if DEBUG
      if (pl_debug_level >= 2) {
        fprintf(stderr, "Opening %s, result=%d\n", pl_filename_buffer,
                ii_file != NULL);
      }  /* if */
#endif /* DEBUG */
      if (ii_file != NULL) {
        int	i;
        pifp->info_filename = pl_copy_string(pl_filename_buffer);
        /* Skip over the reserved lines. */
        for (i = 0; i < INSTANTIATION_INFO_LINES_RESERVED; ++i) {
          pl_read_input_line(ii_file);
        }  /* for */
        /* Read the instantiation list. */
        while (pl_read_input_line(ii_file)) {
          a_pl_symbol_ptr	sym;
          sym = pl_find_symbol(pl_input_line, (a_pl_symbol_ptr)NULL,
			       /*add=*/TRUE);
          if (sym->instantiation_file != NULL) {
            /* The symbol is in the instantiation list of more than one file.
	       This should not happen. */
	    pl_error(pl_ec_bad_instantiation_information_file, (char *)NULL);
          }  /* if */
          sym->instantiation_file = pifp;
          /* Add this to the front of the list of instantiation entries
             associated with this file. */
          sym->next_in_info_file = pifp->info_list;
          pifp->info_list = sym;
        }  /* while */
        fclose(ii_file);
      }  /* if */
    }  /* if */
    pifp = pifp->next;
  }  /* while */
}  /* pl_read_instantiation_info_files */


static a_boolean pl_check_for_ii_file(char *filename)
/*
Check for the existence of a .ii file.
*/
{
  FILE			*ii_file;
  char			*last_dot;

  /* Build the name of the instantiation info file.  The input filename
     is expected to be something like xyz.o.  We strip off the suffix
     and add the suffix for the instantiation information file. */
  strcpy(pl_filename_buffer, filename);
  last_dot = strrchr(pl_filename_buffer, '.');
  if (last_dot == NULL) {
    /* No suffix -- set last_dot as if a suffix had followed the name. */
    last_dot = pl_filename_buffer + strlen(pl_filename_buffer);
  }  /* if */
  strcpy(last_dot, INSTANTIATION_INFO_SUFFIX);
  ii_file = fopen(pl_filename_buffer, "r");
  if (ii_file != NULL) fclose(ii_file);
  return (ii_file != NULL);  
}  /* pl_check_for_ii_file  */


static a_boolean pl_can_instantiate(a_pl_input_file_ptr	pifp,
				 a_pl_symbol_ptr	psp)
/*
Returns TRUE if the input file is a possible instantiation site
of the symbol.
*/
{
  a_pl_instantiation_site_ptr	pisp;
  a_boolean			result = FALSE;

  pisp = psp->possible_instantiation_sites;
  while (pisp != NULL) {
    if (pisp->input_file == pifp) {
      result = TRUE;
      break;
    }  /* if */
    pisp = pisp->next;
  }  /* while */
  return result;
}  /* pl_can_instantiate */


static a_boolean pl_determine_actions(void)
/*
Once the link has been performed go through each of the input object
files and determine whether any instantiations need to be added to
or removed from the file.  We first go through the existing instantiations
and find any that need to be removed.  We then go through the symbols
referenced in the file and see if any of them need to be instantiated.
If so, they are assigned to the first file found that is capable of
generating an instantiation.  If the instantiation list is modified
the file is flagged as requiring recompilation.
*/
{
  a_pl_input_file_ptr	pifp;
  a_boolean		done = TRUE;

  pifp = pl_input_files;
  while (pifp != NULL) {
    if (!pifp->is_archive) {
      a_pl_symbol_ptr	psp;
      a_pl_symbol_ptr	prev_psp;
      /* For each of the entries on the instantiations list, see if the
         symbol is defined in the file. */
      psp = pifp->info_list;
      prev_psp = NULL;
      while (psp != NULL) {
        a_boolean	remove_from_info_file = FALSE;
        a_boolean	recompile_file = FALSE;
        if (psp->multiple_definition || psp->do_not_instantiate) {
          /* An existing instantiation should be removed.  A symbol will
	     be multiply defined when a new specialization has been
             supplied for an instantiation previously assigned to a file.
             The do_not_instantiate flag may now be set (because a pragma
             was added to a file).  Remove the instantiation from
	     the list and recompile the file. */
          remove_from_info_file = TRUE;
          recompile_file = TRUE;
        } else if (psp->defined_in == NULL || psp->defined_in != pifp) {
          /* Either the symbol is undefined or it is now defined in a
             different file.  In either case it should be removed from the
             instantiation list for this file.  This will be the case
             when a file that was assigned a given instantiation no
             longer requires that particular instantiation. */
          remove_from_info_file = TRUE;
        } else if (!psp->is_template) {
          /* The symbol no longer represents a template.  Remove it from the
             instantiation information file. */
          remove_from_info_file = TRUE;
        } else if (!psp->referenced) {
          /* The symbol was referenced by another file and now is not.
             Recompile the file because it may not be needed at all. */
#if 0
          /* Should we provide an option that is not quite so pedantic
             about immediately removing unneeded references. */
#endif /* 0 */
          remove_from_info_file = TRUE;
          recompile_file = TRUE;
        } else {
          /* Mark this symbol has having been instantiated. */
          psp->instantiated = TRUE;
        }  /* if */
        if (remove_from_info_file) {
          /* Either the symbol is undefined or it is now defined in a
             different file.  In either case it should be removed from the
             instantiation list for this file.  This will be the case
             when a file that was assigned a given instantiation no
             longer requires that particular instantiation.  Remove the
             symbol from the info list for this input file. */
          if (prev_psp != NULL) {
            prev_psp->next_in_info_file = psp->next_in_info_file;
          } else {
            pifp->info_list = psp->next_in_info_file;
          }  /* if */
          psp->instantiation_file = NULL;
          pifp->info_file_updated = TRUE;
          pifp->recompile = recompile_file;
          done = FALSE;
          if (verbose) {
            fprintf(stdout, pl_error_text(pl_ec_no_longer_needed),
                    message_prefix, pl_decoded_name(psp->name),
                    pifp->filename);
          }  /* if */
        }  /* if */
        /* Don't update the previous pointer if the current item was
           actually removed from the list. */
        if (!remove_from_info_file) prev_psp = psp;
        psp = psp->next_in_info_file;
      }  /* while */
      /* Do a very simple assignment of instantiations to files.  Just
         go through the list of possible instantiations and instantiate
         anything that hasn't already been handled. */
      psp = pifp->objects->symbols;
      while (psp != NULL) {
        a_pl_symbol_ptr	sym = psp->global_sym;
#if DEBUG
        if (pl_debug_level >= 4) {
          fprintf(stderr, "File: %s, Symbol: %s\n", pifp->filename,
		  sym == NULL ? "null" : sym->name);
          pl_db_symbol(sym, "        ");
        }  /* if */
#endif /* DEBUG */
        if (sym != NULL && sym->is_template &&
             !sym->instantiated && !sym->do_not_instantiate &&
             sym->can_be_instantiated &&
            (sym->referenced || sym->tentative_definition) && !sym->defined &&
            pl_can_instantiate(pifp, sym)) {
          /* Add this symbol to the list of symbols in the info file list.
             Set the instantiation flag and indicate that the info file has
             been updated and the source file associated with the info
             file must be recompiled. */
          sym->next_in_info_file = pifp->info_list;
          pifp->info_list = sym;
          sym->instantiated = TRUE;
          pifp->info_file_updated = TRUE;
          pifp->recompile = TRUE;
          done = FALSE;
          if (verbose) {
            fprintf(stdout, pl_error_text(pl_ec_assigned_to_file),
                    message_prefix, pl_decoded_name(sym->name),
                    pifp->filename);
          }  /* if */
        }  /* if */
        psp = psp->next;
      }  /* while */
    }  /* if */
    pifp = pifp->next;
  }  /* while */
  return done;
}  /* pl_determine_actions */


static int pl_recompile_file(char		 *command_line)
/*
Execute the command to recompile a file.
*/
{
  static char	*shell_format_string = "%s";
  int		length;
  char		*command;
  int		result;

  length = strlen(shell_format_string) + strlen(command_line);
  command = (char *)pl_malloc_with_check(length);
  sprintf(command, shell_format_string, command_line);
  fprintf(stdout, pl_error_text(pl_ec_executing), message_prefix, command);
  fflush(stdout);
  result = system(command);
  free(command);
  return result;
}  /* pl_recompile_file */


static int pl_update_info_files(void)
/*
If the list of instantiates for a given instantiation information file
has changed then write the updated list of instantiations to the file.
*/
{

  a_pl_input_file_ptr		pifp;
  int				return_status = 0;
  int				i;

  /* We allocate one additional array element because it is possible
     for there to be zero reserved lines. */
  char *reserved_lines[INSTANTIATION_INFO_LINES_RESERVED + 1];

  pifp = pl_input_files;
  while (pifp != NULL) {
    if (pifp->info_file_updated) {
      a_pl_symbol_ptr	psp;
      FILE		*ii_file;
      /* Open the input file in read mode to read the header information. */
      if (pifp->info_filename == NULL) {
        fprintf(stderr, "Input file %s has instantiations but no instantiation information file.\n", pifp->filename);
        pl_internal_error("Instantiation information file is missing");
      }  /* if */
      ii_file = fopen(pifp->info_filename, "r");
      if (ii_file == NULL) {
        fprintf(stderr, "File %s is missing\n", pifp->info_filename);
        pl_internal_error("Instantiation information file is missing");
      }  /* if */
      /* Read the reserved lines and save them to be rewritten later. */
      for (i = 0; i < INSTANTIATION_INFO_LINES_RESERVED; ++i) {
        pl_read_input_line(ii_file);
        reserved_lines[i] = pl_copy_string(pl_input_line);
      }  /* for */
      fclose(ii_file);
      /* Truncate the original file so that it can be rewritten. */
      ii_file = fopen(pifp->info_filename, "w");
      if (ii_file == NULL) {
        pl_internal_error("Could not reopen instantiation information file.");
      }  /* if */
      /* Rewrite the reserved lines. */
      for (i = 0; i < INSTANTIATION_INFO_LINES_RESERVED; ++i) {
        fprintf(ii_file, "%s\n", reserved_lines[i]);
      }  /* for */
      /* Write the instantiation list to the file. */
      psp = pifp->info_list;
      while (psp != NULL) {
        fprintf(ii_file, "%s\n", psp->name);
        psp = psp->next_in_info_file;
      }  /* while */
      fclose(ii_file);
      if (!suppress_compilation) {
	/* This depends on the command line being in the first reserved
	   line. */
#if PL_REMOVE_OBJECT_FILE_BEFORE_RECOMPILATION
        (void)unlink(pifp->filename);
#endif /* PL_REMOVE_OBJECT_FILE_BEFORE_RECOMPILATION */
        return_status = pl_recompile_file(reserved_lines[0]);
        /* Stop if an error occurs. */
        if (return_status != 0) break;
      }  /* if */
      /* Free the space occupied by the reserved lines. */
      for (i = 0; i < INSTANTIATION_INFO_LINES_RESERVED; ++i) {
        free(reserved_lines[i]);
      }  /* for */
    }  /* if */
    pifp = pifp->next;
  }  /* while */
  return return_status;
}  /* pl_update_info_files */


#if DEBUG
static void pl_db_symbol(a_pl_symbol_ptr psp,
                         char            *prefix_string)
/*
Display a symbol.
*/
{
  a_pl_instantiation_site_ptr	pisp;
  fprintf(stderr, "%sSymbol: %s", prefix_string, psp->name);
  if (psp->defined) fprintf(stderr, " defined");
  if (psp->referenced) fprintf(stderr, " referenced");
  if (psp->tentative_definition) {
    fprintf(stderr, " tentative_definition");
  }  /* if */
  if (psp->multiple_definition) {
    fprintf(stderr, " multiple_definition");
  }  /* if */
  if (psp->is_template) fprintf(stderr, " is_template");
  if (psp->can_be_instantiated) fprintf(stderr, " can_be_instantiated");
  if (psp->do_not_instantiate) fprintf(stderr, " do_not_instantiate");
  pisp = psp->possible_instantiation_sites;
  if (pisp != NULL) {
    fprintf(stderr, " Instantiation sites:");
    for (; pisp != NULL; pisp = pisp->next) {
      fprintf(stderr, " %s", pisp->input_file->filename);
    }  /* for */
  }  /* if */
  fprintf(stderr, "\n");
}  /* pl_db_symbol */


static void pl_db_input_files(void)
/*
Display the internal representation of the "nm" output.
*/
{
  a_pl_input_file_ptr	pifp;

  pifp = pl_input_files;
  while (pifp != NULL) {
    a_pl_object_file_ptr	pofp;
    a_pl_symbol_ptr		psp;
    fprintf(stderr, "Input file: %s\n", pifp->filename);
    pofp = pifp->objects;
    while (pofp != NULL) {
      fprintf(stderr, "  Object file: %s\n", pofp->filename);
      psp = pofp->symbols;
      while (psp != NULL) {
        pl_db_symbol(psp, "    ");
        psp = psp->next;
      }  /* while */
      pofp = pofp->next;
    }  /* while */
    psp = pifp->info_list;
    if (psp != NULL) {
      fprintf(stderr, "  Instantiation list:\n");
    }  /* if */
    while (psp != NULL) {
      pl_db_symbol(psp, "    ");
      psp = psp->next_in_info_file;
    }  /* while */
    pifp = pifp->next;
  }  /* while */
}  /* pl_db_input_files */


static void pl_db_global_symbols(a_boolean	all)
/*
Display all of the symbols in the global symbol table.
*/
{
  a_pl_symbol_ptr	psp;

  fprintf(stderr, "Global symbol table:\n");
  psp = pl_symbol_table_head;
  while (psp != NULL) {
    if (all ||
        (((psp->referenced && !(psp->defined || psp->tentative_definition)) ||
          psp->multiple_definition) && psp->is_template)
#if 0
        /* Enabling this code displays symbols that are unreferenced or
           referenced only from the file in which they are defined. */
                                 ||
        (psp->defined && !psp->referenced && !psp->tentative_definition)
#endif /* 0 */
                                                                        ) {
      pl_db_symbol(psp, "  ");
    }  /* if */
    psp = psp->next_in_symbol_table;
  }  /* while */
}  /* pl_db_global_symbols */
#endif /* DEBUG */


static void pl_free_all(void)
/*
Free all dynamically allocated data.
*/
{
  a_pl_input_file_ptr	pifp;
  a_pl_input_file_ptr	last_pifp;
  a_pl_symbol_ptr	psp;

  pifp = pl_input_files;
  while (pifp != NULL) {
    a_pl_object_file_ptr	pofp;
    a_pl_object_file_ptr	last_pofp;
    pofp = pifp->objects;
    while (pofp != NULL) {
      a_pl_symbol_ptr	last_psp;
      psp = pofp->symbols;
      while (psp != NULL) {
        last_psp = psp;
        psp = psp->next;
        free(last_psp->name);
        free_pl_symbol(last_psp);
      }  /* while */
      last_pofp = pofp;
      pofp = pofp->next;
      free(last_pofp->filename);
      free_pl_object_file(last_pofp);
    }  /* while */
    last_pifp = pifp;
    pifp = pifp->next;
    free(last_pifp->filename);
    if (last_pifp->info_filename != NULL) free(last_pifp->info_filename);
    free_pl_input_file(last_pifp);
  }  /* while */

  psp = pl_symbol_table_head;
  while (psp != NULL) {
    a_pl_symbol_ptr	        last_psp;
    a_pl_instantiation_site_ptr	pisp;
    pisp = psp->possible_instantiation_sites;
    while (pisp != NULL) {
      a_pl_instantiation_site_ptr	last_pisp;
      last_pisp = pisp;
      pisp = pisp->next;
      free_pl_instantiation_site(last_pisp);
    }  /* while */
    last_psp = psp;
    psp = psp->next_in_symbol_table;
    free(last_psp->name);
    free_pl_symbol(last_psp);
  }  /* while */
}  /* pl_free_all */


int main(int argc, char *argv[])
{
  char		*filename;
  char		*command;
  int		arg;
  int		cmd_line_size = 0;
  int		longest_filename = 0;
  int		return_status = 0;
  a_boolean	done = FALSE;
  a_boolean	any_ii_files = FALSE;
  extern char	*optarg;
  extern int	optind;
  int		optchar;
  long		number_of_iterations = 0;
  char		*nm_command = NULL;
  char		**L_directories;
  char		**library_filenames;
  int		num_of_L_directories = 0;
  int		num_of_library_filenames = 0;
  int		i;

  /* This must be done before any messages are issued. */
  message_prefix = pl_error_text(pl_ec_message_prefix);

  /* Allocate arrays to hold pointers to -L directory names and library
     names specified by -l options.  We don't know how many of these will
     appear on the command line so we will simply use the argument count
     as the number of elements. */
  L_directories = (char**)pl_malloc_with_check(argc * sizeof(char*));
  library_filenames = (char**)pl_malloc_with_check(argc * sizeof(char*));

  /* Process command-line options. */
  /* Suppress getopt's error on non-recognized option. */
  opterr = 0;
#define OPTION_LIST "inrvuc:d:f:l:L:"
  while ((optchar = getopt(argc, argv, OPTION_LIST)) != EOF) {
    switch (optchar) {
      case 'c':
        /* Specify the nm command to be used instead of the default
           value. */
        nm_command = optarg;
        break;
      case 'f':
        /* Specifies the nm line format to be expected. */
        if (strcmp(optarg, "solaris") == 0) {
          nm_format = nmfk_solaris;
        } else if (strcmp(optarg, "SGI") == 0) {
          nm_format = nmfk_SGI;
          ignore_invalid_nm_output = TRUE;
          skip_underscore_prefix = FALSE;
        } else if (strcmp(optarg, "SVR4") == 0) {
          nm_format = nmfk_SVR4;
        } else if (strcmp(optarg, "HPUX") == 0) {
          nm_format = nmfk_HPUX;
        } else if (strcmp(optarg, "CLIX") == 0) {
          nm_format = nmfk_CLIX;
        } else {
          pl_error(pl_ec_invalid_nm_format_option, (char *)NULL);
        }  /* if */
        break;
      case 'i':
        /* Ignore nm output lines that are not formatted properly. */
        ignore_invalid_nm_output = TRUE;
        break;
      case 'l':
        /* Library names (e.g., -lstd). */
        library_filenames[num_of_library_filenames++] = optarg;
        break;
      case 'L':
        /* Library directory names (e.g., -L/edg/cpfe/lib). */
        L_directories[num_of_L_directories++] = optarg;
        break;
      case 'n':
        /* Update the instantiation list files but don't recompile the
           files. */
        suppress_compilation = TRUE;
        break;
      case 'r':
        /* Don't stop after a certain number of iterations. */
        limit_recursion = FALSE;
        break;
      case 'u':
        /* Specify whether names have an extra underscore that should
           be ignored.  The option selects the opposite of the default. */
        skip_underscore_prefix = !TARG_EXTERNAL_NAMES_GET_UNDERSCORE_ADDED;
        break;
      case 'v':
        /* Verbose mode. */
        verbose = TRUE;
        break;
      case 'd':
#if DEBUG
        /* Set the debug level */
        pl_debug_level = atoi(optarg);
        break;        
#endif /* DEBUG */
      default:
        if (optind >= argc) optind = argc-1;
        optarg = argv[optind];
        fprintf(stderr, pl_error_text(pl_ec_unrecognized_option), optarg);
        pl_error(pl_ec_command_line_error, (char*)NULL);
        break;
    }  /* switch */
  }  /* while */
  /* Determine the nm command to be used. */
  if (nm_command != NULL) {
    /* A command was specified on the command line. */
  } else if (nm_format == nmfk_solaris) {
    nm_command = solaris_nm_command;
  } else if (nm_format == nmfk_SGI) {
    nm_command = SGI_nm_command;
  } else if (nm_format == nmfk_CLIX) {
    nm_command = CLIX_nm_command;
  } else if (nm_format == nmfk_SVR4 ||
             nm_format == nmfk_HPUX) {
    nm_command = alternate_nm_command;
  } else {
    /* Use the default command. */
    nm_command = default_nm_command;
  }  /* if */

  /* Search for any libraries specified using the -L option. */
  for (i = 0; i < num_of_library_filenames; ++i) {
    /* Use pl_input_line as a buffer in which to build file names used when
       searching for the library file name. */
    char	*string_buffer = pl_input_line;
    a_boolean	found = FALSE;
    int		j;
    for (j = 0; j < num_of_L_directories; ++j) {
      FILE	*lib_file;
      sprintf(string_buffer, "%s/lib%s.a", L_directories[j],
              library_filenames[i]);
#if DEBUG
      if (pl_debug_level >= 3) {
        fprintf(stderr, "Looking for %s\n", string_buffer);
      }  /* if */
#endif /* DEBUG */
      if ((lib_file = fopen(string_buffer, "r")) != NULL) {
        /* Replace the original library file name string with a pointer to
           the complete path name. */
        library_filenames[i] = pl_copy_string(string_buffer);
        found = TRUE;
        fclose(lib_file);
        break;
      }  /* if */
    }  /* for */
    /* If the library was not found then issue an error and exit. */
    if (!found) {
      fprintf(stderr, pl_error_text(pl_ec_lib_file_not_found),
              library_filenames[i]);
      pl_error(pl_ec_command_line_error, (char *)NULL);
    }  /* if */
  }  /* for */

  /* Determine the length of the command line.  Go through the list of file
     names in the command line and the list of library file names constructed
     from the -l options. */
  for (arg = optind; arg < argc; arg++) {
    /* Examine the file names from the command line. */
    int	arg_size = strlen(argv[arg]);
    cmd_line_size += arg_size + 1;
    if (arg_size > longest_filename) longest_filename = arg_size;
  }  /* for */
  for (i = 0; i < num_of_library_filenames; ++i) {
    /* Examine the library file names constructed from the -l options. */
    int	arg_size = strlen(library_filenames[i]);
    cmd_line_size += arg_size + 1;
    if (arg_size > longest_filename) longest_filename = arg_size;
  }  /* for */
  cmd_line_size += strlen(nm_command) + strlen(nm_command_suffix);
  /* Allocate a buffer for the command line. */
  command = (char *)pl_malloc_with_check(cmd_line_size + 1);
  /* Allocate a buffer than can be used to manipulate filenames.  The
     buffer is 32 characters longer than the longest filename on the
     command line.  The extra space is provided to allow substitution
     of suffixes, etc. */
  pl_filename_buffer = (char *)pl_malloc_with_check(longest_filename + 32);
  /* Construct the nm command. */
  strcpy(command, nm_command);
  /* Append the file names specified on the command line. */
  for (arg = optind; arg < argc; arg++) {
    filename = argv[arg];
    strcat(command, " ");
    strcat(command, filename);
    any_ii_files |= pl_check_for_ii_file(filename);
  }  /* for */
  /* Append the constructed library file names. */
  for (i = 0; i < num_of_library_filenames; ++i) {
    strcat(command, " ");
    strcat(command, library_filenames[i]);
  }  /* for */
  strcat(command, nm_command_suffix);
#if DEBUG
  if (pl_debug_level >= 2) fprintf(stderr, "%s\n", command);
#endif /* DEBUG */

  if (any_ii_files) {
    do {
      pl_input_files = NULL;
      pl_input_file_tail = NULL;
      pl_symbol_table_head = NULL;
      memzero((char *)pl_symbol_table, sizeof(pl_symbol_table));

      pl_command_output = popen(command, "r");

      pl_read_nm_output();
      pclose(pl_command_output);

      /* Read the information from any existing .ii files. */
      pl_read_instantiation_info_files();

#if DEBUG
      if (pl_debug_level >= 4) {
        pl_db_input_files();
      }  /* if */
#endif /* DEBUG */

      /* Add to the symbol table any names that the linker predefines. */
      pl_add_predefined_names();

      pl_prelink();

#if DEBUG
      if (pl_debug_level >= 4) {
        pl_db_global_symbols(/*all=*/FALSE);
      }  /* if */
#endif /* DEBUG */

      /* Determine what actions, if any, are needed. */
      done = pl_determine_actions();

      /* Write the modified info files back to the disk. */
      return_status = pl_update_info_files();
      if (limit_recursion && ++number_of_iterations == PL_MAX_ITERATIONS) {
        pl_error(pl_ec_instantiation_loop, (char *)NULL);
      }  /* if */
      if (return_status != 0 || suppress_compilation) done = TRUE;
      if (!done) pl_free_all();
    } while (!done);
  }  /* if */

#ifdef USING_PURIFY
  /* When using purify, free memory that would otherwise be reported as
     leaked. */
  pl_free_all();
  free(command);
  for (i = 0; i < num_of_library_filenames; ++i) {
    free(library_filenames[i]);
  }  /* for */
  free(L_directories);
  free(library_filenames);
#endif /* USING_PURIFY */

  return (return_status);
}  /* main */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
