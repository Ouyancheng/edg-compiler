/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
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

/*
List of all command-line option kinds.
*/
typedef enum /*an_option_kind*/ {
  optk_none,
  optk_strict_ansi_error,
  optk_strict_ansi_warning,
  optk_preprocess_only_no_line_dirs,
  optk_preprocess_only_emit_line_dirs,
  optk_keep_comments_in_pp_output,
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
  optk_old_line_dirs,
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
  optk_C_dialect_pcc,
  optk_list_makefile_dependencies,
  optk_list_include_files,
#if DO_IL_LOWERING && IL_SHOULD_BE_WRITTEN_TO_FILE
  optk_write_unlowered_il,
#endif /* DO_IL_LOWERING && IL_SHOULD_BE_WRITTEN_TO_FILE */
  optk_cplusplus_anachronisms,
  optk_cfront_2_1_mode,
  optk_cfront_3_0_mode,
  optk_front_end_only,
  optk_use_signed_chars,
  optk_template_instantiation_mode,
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  optk_automatic_template_instantiation,
#endif /* !AUTOMATIC_TEMPLATE_INSTANTIATION */
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
  optk_implicit_template_inclusion,
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
  optk_virtual_function_table_definition,
  optk_allow_dollar_in_id_chars,
  optk_display_compilation_time,
  optk_display_compiler_version,
  optk_suppress_warnings,
  optk_enable_remarks,
  optk_C_dialect_ANSI,
  optk_C_dialect_cplusplus,
  optk_exception_handling,
  optk_suppress_used_before_set_warnings,
  optk_include_directory,
  optk_define_macro,
  optk_undefine_macro,
  optk_set_error_limit,
  optk_generate_raw_listing,
  optk_generate_cross_reference,
  optk_stderr_file_name,
  optk_output_file_name,
#if BACK_END_IS_C_GEN_BE
  optk_module_list_for_union_init,
#endif /* !BACK_END_IS_C_GEN_BE */
#if DEBUG
  optk_debug,
#endif /* DEBUG */
  optk_diag_suppress,
  optk_diag_remark,
  optk_diag_warning,
  optk_diag_error,
  optk_display_error_number,
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
  optk_gen_c_file_name,
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
  optk_create_pch,
  optk_use_pch,
  optk_pch,
  optk_pch_messages,
#if !USE_MMAP_FOR_MEMORY_REGIONS
  optk_pch_mem,
#endif /* !USE_MMAP_FOR_MEMORY_REGIONS */
  optk_pch_dir,
#if RESTRICT_ALLOWED
  optk_restrict,
#endif /* RESTRICT_ALLOWED */
  optk_long_lifetime_temps,
#if MICROSOFT_EXTENSIONS_ALLOWED
  optk_microsoft_mode,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  optk_wchar_t_is_keyword,
#if USER_CONTROL_OF_STRUCT_PACKING
  optk_pack_alignment,
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
  optk_alternate_tokens,
#if MINIMAL_INLINING
  optk_inlining,
#endif /* MINIMAL_INLINING */
  optk_SVR4_C_mode,
  optk_last		/* Must be last. */
} an_option_kind;

/* C_dialect is in basics.h. */

EXTERN a_boolean
		strict_ansi_mode /* = FALSE */;
			/* -A option: issue warnings on nonstandard
			   features used, disable features that conflict
			   with ANSI C (i.e., asm). */
EXTERN a_boolean
                cfront_2_1_mode /* = FALSE */;
                        /*  accept language features supported
                            by cfront release 2.1. */
EXTERN a_boolean
                cfront_3_0_mode /* = FALSE */;
                        /*  accept language features supported
                            by cfront release 3.0. */

/*
Macro that is TRUE if any cfront mode has been selected.
*/
#define any_cfront_mode() (cfront_2_1_mode || cfront_3_0_mode)

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
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
EXTERN a_boolean
		gen_old_style_line_dirs /* = FALSE */;
			/* If TRUE, generate old-style line directives in
			   generated C/C++ output, i.e., "# nnn" instead of
			   "#line nnn". */
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
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
EXTERN enum {vfd_normal, vfd_suppress, vfd_force}
		virtual_function_table_definition /* = vfd_normal */;
			/* If the heuristic used to determine whether a virtual
			   function table should be defined cannot
			   conclusively make such a determination, vfd_suppress
			   indicates that the definition should NOT be
			   made, and vfd_force indicates that it should. */
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
EXTERN an_integer_kind
		plain_char_int_kind;
			/* Integer kind for a "plain" char, dependent on
			   the setting of targ_has_signed_chars. */
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

EXTERN a_boolean
                display_compilation_time
#if VAR_INITIALIZERS
			          = FALSE
#endif /* VAR_INITIALIZERS */
                                                                         ;
                        /* TRUE if compilation timing statistics should be
			   displayed. */

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

EXTERN a_boolean
		display_error_number /* = FALSE */;
			/* Should the diagnostic message output include the
		           error number. */


#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
EXTERN char	*gen_c_file_name /* = NULL */;
			/* Points to a string specifying the name of the
			   generated C file to be created.  The front end
			   will generate a name if this string is NULL. */
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */

EXTERN a_boolean
		precompiled_header_processing_required /* = FALSE */;
			/* TRUE if any kind of precompiled header
			   processing is required by this compilation. */

EXTERN a_boolean
		create_precompiled_header /* = FALSE */;
			/* TRUE if this compilation should create a
			   precompiled header file. */

EXTERN a_boolean
		use_precompiled_header /* = FALSE */;
			/* TRUE if this compilation should use a specified
			   precompiled header file. */

EXTERN char	*pch_input_file_name;
			/* When use_precompiled_header is TRUE, this specifies
			   the name of the precompiled header file to be
			   used. */

EXTERN char	*pch_output_file_name;
			/* When create_precompiled_header is TRUE, this
                           specifies the name of the precompiled header
                           file to be created. */

EXTERN a_boolean
		automatic_pch_processing /* = FALSE */;
			/* TRUE if the compiler should automatically
			   determine whether to build and/or use a
			   precompiled header file. */

EXTERN a_boolean
		suppress_pch_messages /* = FALSE */;
			/* TRUE if messages regarding the creation and
			   use of precompiled header files should be
			   suppressed. */

#if !USE_MMAP_FOR_MEMORY_REGIONS
EXTERN sizeof_t	pch_mem_size;
			/* Size of the preallocated PCH memory area. */
#endif /* !USE_MMAP_FOR_MEMORY_REGIONS */

EXTERN char	*pch_dir_name /* = NULL*/;
			/* Directory in which PCH files are to be stored.
			   NULL if no directory has been specified. */

#if RESTRICT_ALLOWED
EXTERN a_boolean
		restrict_recognized
#if VAR_INITIALIZERS
                                    = TRUE
#endif /* VAR_INITIALIZERS */
                                          ;
			/* TRUE if the restrict token should be recognized.
			   When this flag is FALSE, "restrict" is not entered
			   into the symbol table. */
#endif /* RESTRICT_ALLOWED */

EXTERN a_boolean
		long_lifetime_temps
#if VAR_INITIALIZERS
                                    = FALSE
#endif /* VAR_INITIALIZERS */
                                           ;
			/* If FALSE, temporaries have lifetimes that end at
			   end of full expression.  If TRUE, temporaries
			   have lifetimes that end at end of scope, label,
			   or end of switch clause. */

#if MICROSOFT_EXTENSIONS_ALLOWED
EXTERN a_boolean
		microsoft_mode
#if VAR_INITIALIZERS
                               = DEFAULT_MICROSOFT_MODE
#endif /* VAR_INITIALIZERS */
                                                       ;
			/* TRUE if microsoft extensions are to be accepted. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if MICROSOFT_KEYWORDS_ALLOWED
EXTERN a_calling_convention
		default_calling_convention
#if VAR_INITIALIZERS
                                           = (a_calling_convention)cc_cdecl
#endif /* VAR_INITIALIZERS */
                                                                           ;
			/* The default calling convention.  cc_default is
			   considered compatible with this calling
			   convention. */
#endif /* MICROSOFT_KEYWORDS_ALLOWED */

EXTERN a_boolean
		wchar_t_is_keyword
#if VAR_INITIALIZERS
                                   = DEFAULT_WCHAR_T_IS_KEYWORD
#endif /* VAR_INITIALIZERS */
                                                               ;
			/* Indicates whether wchar_t is to be considered a
                           keyword.  Once command line processing has been
			   completed, this value must only be TRUE in C++
                           mode. */

#if USER_CONTROL_OF_STRUCT_PACKING
EXTERN a_targ_alignment
		default_max_member_alignment /* = 0*/;
			/* If nonzero, the maximum alignment of any nonstatic
			   data member of a class, struct, or union, unless a
			   "#pragma pack" overrides it.  Its value is based
			   on command-line option "--pack_alignment".  (A zero
			   value means that a member's alignment is based
			   solely on its type.) */
#endif /* USER_CONTROL_OF_STRUCT_PACKING */

EXTERN a_boolean
                alternate_tokens_allowed
#if VAR_INITIALIZERS
                                         = DEFAULT_ALTERNATE_TOKENS_ALLOWED
#endif /* VAR_INITIALIZERS */
                                                                           ;
                        /* TRUE if the C++ operator keywords (such as
			   "and", "or", "not", etc.) and digraphs should
			   be allowed.  This flag is automatically set
			   in strict mode. */

#if MINIMAL_INLINING
EXTERN a_boolean
		inlining_enabled
#if VAR_INITIALIZERS
                                 = TRUE
#endif /* VAR_INITIALIZERS */
                                       ;
			/* TRUE if minimal inlining should be done by IL
			   lowering. */
#endif /* MINIMAL_INLINING */

EXTERN a_boolean
                SVR4_C_mode
#if VAR_INITIALIZERS
                            = DEFAULT_SVR4_C_MODE
#endif /* VAR_INITIALIZERS */
                                                ;
                        /* TRUE if the C++ operator keywords (such as
			   "and", "or", "not", etc.) and digraphs should
			   be allowed. */


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
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
