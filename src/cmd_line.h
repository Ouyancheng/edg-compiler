/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
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
#ifndef LANG_FEAT_H
#include "lang_feat.h"
#endif /* ifndef LANG_FEAT_H */

typedef enum /*a_C_dialect*/ {
  /* Possible C dialects to compile. */
  C_dialect_ANSI,	/* ANSI C. */
  C_dialect_pcc,	/* UNIX pcc C. */
  C_dialect_cplusplus	/* C++. */
} a_C_dialect;

EXTERN a_C_dialect
		C_dialect
#if VAR_INITIALIZERS
                          = C_dialect_cplusplus
#endif /* VAR_INITIALIZERS */
                                               ;
			/* The C dialect to be accepted. */
EXTERN a_boolean
		strict_ansi_mode /* = FALSE */;
			/* -A option: issue warnings on nonstandard
			   features used, disable features that conflict
			   with ANSI C (i.e., asm). */
EXTERN a_boolean
                cfront_compatibility_mode /* = FALSE */;
                        /* -b option:  accept language features supported
                            by cfront release 2.1. */
EXTERN a_boolean
		pcc_preprocessing_mode /* = FALSE */;
			/* TRUE if old-style (Reiser cpp) preprocessing
			   should be done. */
EXTERN a_boolean
                allow_anachronisms
#if VAR_INITIALIZERS
                          = DEFAULT_ALLOW_ANACHRONISMS
#endif /* VAR_INITIALIZERS */
                                                      ;
                        /* Indicates whether anachronisms should be
                           accepted.  The default is supplied by a
                           configuration parameter. */

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
#if DO_IL_LOWERING
EXTERN a_boolean
		suppress_il_lowering /* = FALSE */;
			/* TRUE if IL lowering should not be done. */
#endif /* DO_IL_LOWERING */
#if IL_SHOULD_BE_WRITTEN_TO_FILE
EXTERN a_boolean
		suppress_il_file_write /* = FALSE */;
			/* TRUE if the writing of the IL file should be
			   suppressed. */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
EXTERN a_boolean
		suppress_virtual_function_table_definition /* = FALSE */;
			/* If the heuristic used to determine whether a virtual
			   function table should be defined cannot
			   conclusively make such a determination, TRUE
			   indicates that the definition should NOT be
			   made. */
EXTERN a_boolean
		suppress_used_before_set_warnings /* = FALSE */;
			/* TRUE if used-before-set warnings should not be
			   issued on automatic local variables that are used
			   before a value is assigned to them; FALSE by
			   default.  Set by the -j command line option. */
EXTERN a_boolean
		exceptions_enabled
#if VAR_INITIALIZERS
                                    = DEFAULT_EXCEPTIONS_ENABLED
#endif /* VAR_INITIALIZERS */
                                                                ;
			/* TRUE if a C++ source program should be compiled
			   with support for exception handling.  If it is
			   FALSE, an error will be issued whenever an
			   exception construct -- a try block, a throw
			   expression, or a throw specification on a function
			   declaration -- is encountered.  The default is
			   configurable, and the -x option toggles the
			   default value.  In cfront mode it is always FALSE.
			   It has no meaning in C mode. */
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
EXTERN a_boolean
		enum_types_can_be_smaller_than_int
#if VAR_INITIALIZERS
			          = DEFAULT_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* If this is TRUE, enum types will be allocated
			   the smallest in some set of integral types into
			   which the enumeration values will fit.  If FALSE,
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


EXTERN a_boolean
                allow_dollar_in_id_chars
#if VAR_INITIALIZERS
			          = DEFAULT_ALLOW_DOLLAR_IN_ID_CHARS
#endif /* VAR_INITIALIZERS */
                                                                         ;
                        /* Specifies whether dollar signs are allowed
                           in identifiers.  The default is supplied by
                           a configuration parameter. */


typedef enum /*a_template_instantiation_mode*/ {
  /* Defines the methods of handling template instantiation.  Used to
     determine which template functions and member functions of
     template classes should be instantiated.  This specifies a general
     mode that is used for all templates.  This can be overridden by
     pragmas that can cause specific templates to be instantiated or
     to not be instantiated. */
  tim_none,	/* No instantiation should be done. */
  tim_all,	/* Instantiate template functions that have been
		   referenced and all member functions of template classes
		   that have been referenced. */ 
  tim_used,	/* Instantiate template functions that have been referenced
		   and only those member functions that have been used. */
  tim_local,	/* Similar to tim_used except the functions are given
		   internal linkage so that they can be instantiated in
		   multiple compilation units.  This is a simple mechanism
		   that can be used to get started with templates. */
  tim_can_instantiate
		/* A special mode used during processing of can_instantiate
		   pragmas.  This mode causes entries to the
		   instantiation required list to be entered with the
		   instantiation required flag set to FALSE.  This option
		   cannot be specified on the command line. */
} a_template_instantiation_mode;


EXTERN a_template_instantiation_mode
                instantiation_mode
#if VAR_INITIALIZERS
			          = tim_none
#endif /* VAR_INITIALIZERS */
                                            ;
                        /* The default template instantiation mode. */

#if AUTOMATIC_TEMPLATE_INSTANTIATION
EXTERN a_boolean
                automatic_instantiation_mode
#if VAR_INITIALIZERS
                          = DEFAULT_AUTOMATIC_INSTANTIATION_MODE
#endif /* VAR_INITIALIZERS */
                                                                ;
                        /* Should automatic instantiation processing be
			   performed.  This includes both the generation of
 			   the instantiation flags and the processing of the
			   instantiation list. */

EXTERN a_boolean	process_instantiation_list_file
#if VAR_INITIALIZERS
			          = FALSE
#endif /* VAR_INITIALIZERS */
                                         ;
                        /* When automatic_instantiation_mode is TRUE this
			   flag indicates whether there is an instantiation
			   list file to be read.  When this flag is FALSE
			   and automatic_instantiation_mode is TRUE it means
			   that no files have been assigned to this compilation
			   for automatic instantiation but the front end should
			   still generate automatic instantiation flags to
			   be passed to the back end. */

EXTERN char	*instantiation_list_filename
#if VAR_INITIALIZERS
			          = NULL
#endif /* VAR_INITIALIZERS */
                                        ;
                        /* The name of a file containing a list of names
			   of template functions and static data members to
			   be instantiated.  Intended to be used for linker
			   feedback mechanisms to provide automatic
			   instantiation. */

EXTERN FILE	*f_instantiation_information /* = NULL */;
			/* File from which the instantiation list should be
			   read.  Only valid when do_auto_instantiation is
			   TRUE. */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */

#if INSTANTIATION_BY_IMPLICIT_INCLUSION
EXTERN a_boolean
                implicit_template_inclusion_mode
#if VAR_INITIALIZERS
                          = DEFAULT_IMPLICIT_TEMPLATE_INCLUSION_MODE
#endif /* VAR_INITIALIZERS */
                                                                    ;
                        /* Should the front end attempt to implicitly include
			   a source file (e.g., .c file) to find the
			   definition of a template. */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */


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
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
