/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
/*

cmd_line.h -- Declarations relating to cmd_line.c (relating
              to command-line parsing).

*/

/* Avoid including these declarations more than once: */
#ifndef CMD_LINE_H
#define CMD_LINE_H 1

#ifndef HOST_ENVIR_H
#include "host_envir.h"
#endif /* ifndef HOST_ENVIR_H */
#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */

typedef enum /*a_C_dialect*/ {
  /* Possible C dialects to compile. */
  C_dialect_ANSI,	/* ANSI C. */
  C_dialect_pcc,	/* UNIX pcc C. */
  C_dialect_cpp		/* C++. */
} a_C_dialect;

EXTERN a_C_dialect
		C_dialect
#if VAR_INITIALIZERS
                          = C_dialect_cpp
#endif /* VAR_INITIALIZERS */
                                          ;
			/* The C dialect to be accepted. */
EXTERN a_boolean
		strict_ansi_mode /* = FALSE */;
			/* -A option: issue warnings on nonstandard
			   features used, disable features that conflict
			   with ANSI C (i.e., asm). */
EXTERN int	init_debug_level /* = 0 */;
			/* Initial debug level: n in -dn option, or 0
			   by default. */
EXTERN a_boolean
		do_preprocessing_only /* = FALSE */;
			/* If TRUE, the compiler is to act like cpp: the
			   source is preprocessed, but not compiled. */
EXTERN a_boolean
		generate_pp_output /* = FALSE */;
			/* If TRUE, the preprocessing step should generate
			   a textual output file of the preprocessed text.
			   FALSE when do_preprocessing_only is FALSE. */
EXTERN a_boolean
		keep_comments_in_pp_output /* = FALSE */;
			/* If TRUE, comments should be retained in
			   preprocessing output.  Meaningful only when
			   generate_pp_output is TRUE. */
EXTERN a_boolean
		gen_line_info_in_pp_output /* = FALSE */;
			/* If TRUE, generate #line directives in
			   preprocessing output.  Meaningful only when
			   generate_pp_output is TRUE. */
EXTERN FILE	*f_pp_output /* = NULL */;
			/* File to which preprocessing output is written.
			   Meaningful only when generate_pp_output is
			   TRUE. */
EXTERN char	*pp_file_name /* = NULL */;
			/* Name of the preprocessing output file to be
			   opened, or NULL if no such file is needed or if
			   a default file should be used. */
EXTERN a_boolean
		list_included_files /* = FALSE */;
			/* When TRUE, write the names of #included files to
			   stdout. */
EXTERN a_boolean
		list_makefile_dependencies /* = FALSE */;
			/* When TRUE, write dependency lines for "make" to
			   stdout (for #include files encountered). */
EXTERN FILE	*f_raw_listing /* = NULL */;
			/* If non-NULL (-L option), raw source lines and
			   context information are written to this file.
			   Such information could be read later by a program
			   to generate an interspersed listing. */
EXTERN FILE	*f_xref_info /* = NULL */;
			/* If non-NULL (-X option), cross-reference information
			   is written to this file.  Such information could
			   be read and sorted later to produce a cross-
			   reference listing. */
EXTERN a_boolean
		suppress_back_end /* = FALSE */;
			/* TRUE if the back end should not be called.  The
			   -n option sets this to TRUE, and it is also TRUE
			   whenever do_preprocessing_only is TRUE. */
EXTERN a_boolean
		targ_has_signed_chars
#if VAR_INITIALIZERS
                                      = DEFAULT_TARG_HAS_SIGNED_CHARS
#endif /* VAR_INITIALIZERS */
                                                                     ;
			/* TRUE if the target has signed characters.  This
			   is selectable on the command line. */
EXTERN an_integer_kind
		plain_char_int_kind;
			/* Integer kind for a "plain" char, dependent on
			   the setting of targ_has_signed_chars. */
EXTERN long	targ_min_char,
		targ_max_char;
			/* Mininum and maximum values for values of type char,
			   dependent on the setting of
			   targ_has_signed_chars.  The more obvious names
			   targ_CHAR_MIN and targ_CHAR_MAX were not used
			   because they conflict with one another as
			   8-character external names. */
EXTERN a_boolean
		enum_types_can_be_smaller_than_int
#if VAR_INITIALIZERS
			          = DEFAULT_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* If this is TRUE, enum types will be allocated
			   the smallest of char, short, or int in which
			   the enumeration values will fit.  If FALSE,
			   int is always used. */
EXTERN a_boolean
		string_literals_shared;
			/* TRUE if string literals can be shared.  FALSE
			   if string literals are not shared because they
			   might be writable (as in pcc mode). */

typedef struct a_def_undef_string *a_def_undef_string_ptr;
typedef struct a_def_undef_string {
  /* Used to save -D (define symbol) and -U (undefined symbol) command-line
     arguments.  There are separate lists for def and undef, so the
     entry itself need not identify the function involved. */
  a_def_undef_string_ptr
		next;
			/* Next entry on this list, or NULL if this is the
			   last entry. */
  char		*text;
			/* The text of the argument (i.e., "x=1" for the
			   option "-Dx=1", "x" for "-Ux"). */
} a_def_undef_string;

EXTERN a_def_undef_string_ptr
		defs_from_cmd_line   /* = NULL */,
		undefs_from_cmd_line /* = NULL */;
			/* The list of -D and -U options from the command
			   line, defining and undefining macro symbols. */


/* Process the command line arguments. */
extern void proc_command_line(int argc, char *argv[]);
#if COMPILE_MULTIPLE_SOURCE_FILES
/* Fetch the next source file name from the command line. */
extern a_boolean get_next_source_file(void);
#endif /* COMPILE_MULTIPLE_SOURCE_FILES */

#endif /* ifndef CMD_LINE_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
