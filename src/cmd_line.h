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
  optk_ii_file_name,
  optk_template_info_file,
  optk_definition_list_file_name,
  optk_exported_template_file_name,
  optk_template_directory,
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
  optk_time_limit,
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
  optk_pch_verbose,
#if !USE_MMAP_FOR_MEMORY_REGIONS
  optk_pch_mem,
#endif /* !USE_MMAP_FOR_MEMORY_REGIONS */
  optk_pch_dir,
  optk_restrict,
  optk_long_lifetime_temps,
#if MICROSOFT_EXTENSIONS_ALLOWED
  optk_microsoft_mode,
  optk_microsoft_version,
  optk_microsoft_bugs,
#if NEAR_AND_FAR_ALLOWED
  optk_microsoft_16_mode,
#endif /* NEAR_AND_FAR_ALLOWED */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEAR_AND_FAR_ALLOWED
  optk_far_data_pointers,
  optk_far_code_pointers,
#endif /* NEAR_AND_FAR_ALLOWED */
  optk_wchar_t_is_keyword,
#if USER_CONTROL_OF_STRUCT_PACKING
  optk_pack_alignment,
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
  optk_alternative_tokens,
#if DO_IL_LOWERING && MINIMAL_INLINING
  optk_inlining,
#endif /* DO_IL_LOWERING && MINIMAL_INLINING */
  optk_SVR4_C_mode,
  optk_brief_diagnostics,
  optk_nonconst_ref_anachronism,
  optk_no_preproc_only,
  optk_rtti,
  optk_building_runtime,
  optk_bool_is_keyword,
  optk_array_new_and_delete,
  optk_explicit,
  optk_namespaces,
  optk_implicit_using_std,
  optk_remove_unneeded_entities,
  optk_typename,
  optk_implicit_typename,
  optk_special_subscript_cost,
  optk_suppress_instantiation_flags,
  optk_old_style_preprocessing,
  optk_old_for_init,
  optk_for_init_diff_warning,
  optk_distinct_template_signatures,
  optk_guiding_decls,
  optk_old_specializations,
  optk_wrap_diagnostics,
#if IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE
  optk_implicit_extern_c_type_conversion,
#endif /* IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE */
  optk_long_preserving_rules,
  optk_extern_inline,
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
  optk_multibyte_chars,
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
  optk_embedded_cplusplus,
#if VLA_ALLOWED
  optk_vla,
#endif /* VLA_ALLOWED */
  optk_enum_overloading,
  optk_nonstandard_qualifier_deduction,
#if ONE_INSTANTIATION_PER_OBJECT
  optk_one_instantiation_per_object,
  optk_instantiation_dir,
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  optk_late_tiebreaker,
  optk_preinclude,
  optk_preinclude_macros,
  optk_pending_instantiations,
#if MICROSOFT_EXTENSIONS_ALLOWED
  optk_import_dir,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  optk_const_string_literals,
  optk_class_name_injection,
  optk_arg_dependent_lookup,
  optk_friend_injection,
  optk_nonstandard_using_decl,
  optk_system_include_dir,
  optk_designators,
  optk_extended_designators,
  optk_variadic_macros,
  optk_extended_variadic_macros,
  optk_include_file_suffixes,
  optk_compound_literals,
  optk_base_assign_op_is_default,
  optk_sun_mode,
  optk_dependent_name_processing,
  optk_ignore_namespace_std,
  optk_parse_nonclass_templates,
  optk_c99_mode,
  optk_export_template,
  optk_stdarg_builtin,
#if ENABLE_TRANS_UNIT_TEST_MODE
  optk_trans_unit_test_mode,
#endif /* ENABLE_TRANS_UNIT_TEST_MODE */
#if GNU_EXTENSIONS_ALLOWED
  optk_gcc_mode,
  optk_short_enums,
#endif /* GNU_EXTENSIONS_ALLOWED */
#if DEBUG
  optk_debug_name,
#if MAINTAIN_ALLOCATION_SEQUENCE_NUMBER
  optk_debug_alloc_seq,
#endif /* MAINTAIN_ALLOCATION_SEQUENCE_NUMBER */
#endif /* DEBUG */
  optk_long_long,
  optk_context_limit,
  optk_set_flag,
#if UPC_EXTENSIONS_ALLOWED
  optk_upc_mode,
  optk_upc_strict_access,
  optk_upc_threads,
#endif /* UPC_EXTENSIONS_ALLOWED */
  optk_last		/* Must be last. */
} an_option_kind;

/* C_dialect is in basics.h. */

EXTERN a_boolean
		strict_ansi_mode /* = FALSE */;
			/* -A option: issue warnings on nonstandard
			   features used, disable features that conflict
			   with ANSI C (i.e., asm). */
EXTERN a_boolean
                sun_mode
#if VAR_INITIALIZERS
                         = DEFAULT_SUN_COMPATIBILITY
#endif /* VAR_INITIALIZERS */
                                                    ;
                        /* Accept language features supported
                           by Sun CC release 5.0. */

EXTERN a_boolean
                cfront_2_1_mode /* = FALSE */;
                        /* Accept language features supported
                           by cfront release 2.1. */
EXTERN a_boolean
                cfront_3_0_mode /* = FALSE */;
                        /* Accept language features supported
                           by cfront release 3.0. */

EXTERN a_boolean
                trans_unit_test_mode /* = FALSE */;
                        /* Enable mode to test compilation of multiple
                           (possibly identical) translation units. */

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
EXTERN a_boolean
                allow_nonconst_call_anachronism
#if VAR_INITIALIZERS
                          = DEFAULT_ALLOW_NONCONST_CALL_ANACHRONISM
#endif /* VAR_INITIALIZERS */
                                                                   ;
			/* Indicates whether the anachronism of calling
			   a non-const function on a const object should
			   be accepted. */

#if DEBUG
EXTERN int	init_debug_level /* = 0 */;
			/* Initial debug level: n in -dn option, or 0
			   by default. */
#endif /* DEBUG */
EXTERN a_boolean
		do_preprocessing_only /* = FALSE */;
			/* If TRUE, the compiler is to act like cpp: the
			   source is preprocessed, but not compiled. */
EXTERN a_boolean
                pp_output_file_needed /* = FALSE */;
                        /* If TRUE, the compiler will output information to
			   the preprocessing output file.  This could be
			   preprocessed text, makefile dependency information,
			   etc. */
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
EXTERN enum {vfd_normal, vfd_suppress, vfd_force} /*lint !e659*/
		/*lint -esym(769,vfd_normal)*/
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
		addr_of_bit_field_allowed
#if VAR_INITIALIZERS
                                          = ADDR_OF_BIT_FIELD_ALLOWED
#endif /* VAR_INITIALIZERS */
                                                                     ;
			/* TRUE if the address of a bit field may be taken
			   (provided it has a size and alignment that matches
			   some integral type). */

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
		ignore_exception_specifications
#if VAR_INITIALIZERS
                                                = FALSE
#endif /* VAR_INITIALIZERS */
                                                       ;
			/* TRUE if an exception specification on a function
			   declarator is recognized but ignored; meaningful
			   only in C++ when exceptions_enabled is TRUE. */

EXTERN a_boolean
		rtti_enabled
#if VAR_INITIALIZERS
                             =
#if RTTI_ENABLING_POSSIBLE
                               DEFAULT_RTTI_ENABLED
#else /* !RTTI_ENABLING_POSSIBLE */
                               FALSE
#endif /* RTTI_ENABLING_POSSIBLE */
#endif /* VAR_INITIALIZERS */
                                                   ;
			/* TRUE if support for runtime type identification
			   (RTTI) is enabled.  Significant only in C++ mode.
			   RTTI cannot be enabled if the extended typeinfo
			   for it is not generated. */

#if DO_IL_LOWERING && ABI_CHANGES_FOR_RTTI
EXTERN a_boolean
		generate_rtti_typeinfo
#if VAR_INITIALIZERS
                                       = TRUE
#endif /* VAR_INITIALIZERS */
                                             ;
			/* TRUE if the typeinfo tables that support RTTI
			   should be generated.  If FALSE, typeinfo tables
			   will be generated only for types used in exceptions.
			   Must be TRUE if rtti_enabled is TRUE.  See
			   SUPPRESS_TYPEINFO_VARIABLES_WHEN_RTTI_DISABLED. */
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_RTTI */

EXTERN a_boolean
		array_new_and_delete_enabled
#if VAR_INITIALIZERS
                             =
#if ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE
                               DEFAULT_ARRAY_NEW_AND_DELETE_ENABLED
#else /* !ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE */
                               FALSE
#endif /* ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE */
#endif /* VAR_INITIALIZERS */
                                                                   ;
			/* TRUE if support for array new and delete is
			   enabled.  Significant only in C++ mode.  They
			   cannot be enabled if the ABI changes for them
			   are not enabled. */
EXTERN a_boolean
		explicit_keyword_enabled
#if VAR_INITIALIZERS
                                         = DEFAULT_EXPLICIT_KEYWORD_ENABLED
#endif /* VAR_INITIALIZERS */
                                                                           ;
			/* TRUE if the "explicit" keyword is recognized.
			   Significant only in C++ mode. */
EXTERN a_boolean
		namespaces_enabled
#if VAR_INITIALIZERS
                                   = DEFAULT_NAMESPACES_ENABLED
#endif /* VAR_INITIALIZERS */
                                                               ;
			/* TRUE if support for namespaces is enabled.
			   Significant only in C++ mode. */
EXTERN a_boolean
		implicit_using_std
#if VAR_INITIALIZERS
                                   = DEFAULT_IMPLICIT_USING_STD
#endif /* VAR_INITIALIZERS */
                                                                ;
			/* TRUE if the runtime should implicitly do a
			   "using namespace std".  Significant only in
			    C++ mode. */

EXTERN a_boolean
		typename_enabled
#if VAR_INITIALIZERS
                                  = DEFAULT_TYPENAME_ENABLED
#endif /* VAR_INITIALIZERS */
                                                               ;
			/* TRUE if support for typename is enabled.
			   Significant only in C++ mode. */

EXTERN a_boolean
		implicit_typename_enabled
#if VAR_INITIALIZERS
                                          = DEFAULT_IMPLICIT_TYPENAME_ENABLED
#endif /* VAR_INITIALIZERS */
                                                               ;
			/* TRUE if the front end should determine from context
			   whether a template parameter dependent name is a
			   type or nontype.  Significant only in C++ mode. */

EXTERN a_boolean
		extern_inline_allowed
#if VAR_INITIALIZERS
                                      = DEFAULT_EXTERN_INLINE_ALLOWED
#endif /* VAR_INITIALIZERS */
                                                                     ;
			/* TRUE if inline functions are allowed to have
			   external linkage (as specified by the standard) and
			   FALSE if they imply internal linkage (as specified
			   in the ARM).  Significant only in C++ mode. */

EXTERN a_boolean
		floating_point_template_parameters_allowed
#if VAR_INITIALIZERS
                          = DEFAULT_FLOATING_POINT_TEMPLATE_PARAMETERS_ALLOWED
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* TRUE if template parameters of floating-point type
			   are allowed (which is nonstandard). */

EXTERN a_boolean
		vla_enabled
#if VAR_INITIALIZERS
                            = DEFAULT_VLA_ENABLED
#endif /* VAR_INITIALIZERS */
                                                 ;
			/* TRUE if support for variable length arrays (VLAs)
			   is enabled.  Always FALSE in C++ mode.  Controlled
			   by command-line options --[no_]vla. */

EXTERN a_boolean
		vla_dealloc_statements_in_il
#if VAR_INITIALIZERS
                                             = VLA_DEALLOC_STATEMENTS_IN_IL
#endif /* VAR_INITIALIZERS */
                                                                           ;
			/* TRUE if stmk_vla_dealloc statements should be
			   generated to mark the points at which VLA objects
			   pass out of scope and may be deallocated.  Always
			   FALSE when vla_enabled is FALSE. */

EXTERN a_boolean
		operator_overloading_on_enums_enabled
#if VAR_INITIALIZERS
                                       = DEFAULT_OPERATOR_OVERLOADING_ON_ENUMS
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* TRUE if operator functions can be used to
			   overload operations on enums. */

EXTERN a_boolean
		string_literals_are_const
#if VAR_INITIALIZERS
                                           = DEFAULT_STRING_LITERALS_ARE_CONST
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* TRUE if string literals are const, i.e.,
			   array[n] of const char.  Also controls wide
			   string literals. */

EXTERN a_boolean
		class_name_injection_enabled
#if VAR_INITIALIZERS
                                             = DEFAULT_CLASS_NAME_INJECTION
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* TRUE if class names are injected into the scope
			   of the class. */

EXTERN a_boolean
		arg_dependent_lookup_enabled
#if VAR_INITIALIZERS
                                             = DEFAULT_ARG_DEPENDENT_LOOKUP
#endif /* VAR_INITIALIZERS */
                                                                           ;
			/* TRUE if argument dependent lookup of function
			   names should be performed. */

EXTERN a_boolean
		friend_injection_enabled
#if VAR_INITIALIZERS
                                             = DEFAULT_FRIEND_INJECTION
#endif /* VAR_INITIALIZERS */
                                                                       ;
			/* TRUE if names first declared in friend declarations
			   are visible. */

EXTERN a_boolean
		do_dependent_name_processing
#if VAR_INITIALIZERS
                                        = DEFAULT_DEPENDENT_NAME_PROCESSING
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* TRUE if special processing for dependent names
			   in templates should be done.  This also enables
			   prototype instantiations of function bodies and
			   default arguments. */

EXTERN a_boolean
		force_dependent_name_rules_for_base_class_lookup;
			/* TRUE if the portion of dependent name lookup that
			   involves the lookup of names in dependent base
			   classes should be performed even if full dependent
			   name processing is not being done. */

EXTERN a_boolean
		nonclass_prototype_instantiations
#if VAR_INITIALIZERS
                                        = DEFAULT_DEPENDENT_NAME_PROCESSING
#endif /* VAR_INITIALIZERS */
                                                                           ;
			/* TRUE if nonclass template declarations should
			   have prototype instantiations performed on them.
			   This is initialized to the same value as
			   do_dependent_name_processing because it is a
			   prerequisite. */

EXTERN a_boolean
		defer_friend_instantiation;
			/* TRUE if the semantic analysis of friend functions
			   of class templates should be deferred until the
			   function is used. */

EXTERN a_boolean
		nonstandard_using_decl_allowed
#if VAR_INITIALIZERS
                                      = DEFAULT_NONSTANDARD_USING_DECL_ALLOWED
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* TRUE if a nonstandard nonmember using-declaration
                           that uses an unqualified name should be accepted. */

EXTERN a_boolean
		designators_allowed
#if VAR_INITIALIZERS
                                             =
#if DESIGNATED_INITIALIZER_ENABLING_POSSIBLE
                                               DEFAULT_DESIGNATORS_ALLOWED
#else /* !DESIGNATED_INITIALIZER_ENABLING_POSSIBLE */
                                               FALSE
#endif /* DESIGNATED_INITIALIZER_ENABLING_POSSIBLE */
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* TRUE if '.x' and '[expr]' designators should be
			   accepted. */

EXTERN a_boolean
		extended_designators_allowed
#if VAR_INITIALIZERS
                                        =
#if DESIGNATED_INITIALIZER_ENABLING_POSSIBLE
                                          DEFAULT_EXTENDED_DESIGNATORS_ALLOWED
#else /* !DESIGNATED_INITIALIZER_ENABLING_POSSIBLE */
                                          FALSE
#endif /* DESIGNATED_INITIALIZER_ENABLING_POSSIBLE */
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* TRUE if 'x:' and '[expr ... expr]' designators
			   should be accepted. */

EXTERN a_boolean
		variadic_macros_allowed
#if VAR_INITIALIZERS
                                             = DEFAULT_VARIADIC_MACROS_ALLOWED
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* TRUE if '#define VM(x, ...) __VA_ARGS__' should be
			   accepted. */

EXTERN a_boolean
		extended_variadic_macros_allowed
#if VAR_INITIALIZERS
                                    = DEFAULT_EXTENDED_VARIADIC_MACROS_ALLOWED
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* TRUE if '#define EVM(args ...) args' should be
			   accepted. */

EXTERN a_boolean
		compound_literals_allowed
#if VAR_INITIALIZERS
                                          = DEFAULT_COMPOUND_LITERALS_ALLOWED
#endif /* VAR_INITIALIZERS */
                                                                             ;
			/* TRUE if C99 compound literals, which look like a
			   cast including a brace-enclosed initializer, e.g.,
			   (int []){1, 2, 3}, should be accepted. */

EXTERN a_boolean
		pointer_to_member_call_optimization_allowed
#if VAR_INITIALIZERS
                         = DEFAULT_POINTER_TO_MEMBER_CALL_OPTIMIZATION_ALLOWED
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* TRUE if optimized code can be generated for certain
			   pointer to member calls.  The C++ standard disallows
			   this optimization. */

EXTERN a_boolean
		special_subscript_cost
#if VAR_INITIALIZERS
                                       = DEFAULT_SPECIAL_SUBSCRIPT_COST
#endif /* VAR_INITIALIZERS */
                                                                       ;
			/* TRUE if the cost of the subscript operator []'s
			   integral operand is always considered a standard
			   conversion in overload resolution.  This is
			   nonstandard, but a fair number of programs
			   depend on it. */

EXTERN a_boolean
		long_preserving_rules
#if VAR_INITIALIZERS
                                      = DEFAULT_LONG_PRESERVING_RULES
#endif /* VAR_INITIALIZERS */
                                                                     ;
			/* TRUE if the K&R rules for usual arithmetic
			   conversions involving "long" should be used.
			   This means the rules described in the K&R I book,
			   not the rules used by the pcc compiler. */

EXTERN an_integer_kind
		plain_char_int_kind;
			/* Integer kind for a "plain" char, dependent on
			   the setting of targ_has_signed_chars. */
EXTERN a_boolean
		string_literals_shared;
			/* TRUE if string literals can be shared.  FALSE
			   if string literals are not shared because they
			   might be writable (as in pcc mode). */

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

EXTERN a_boolean
		suppress_instantiation_flags /* = FALSE */;
			/* Should the instantiation flags that are normally
			   generated as part of the automatic instantiation
			   process be suppressed. */

EXTERN char	*ii_file_name /* = NULL */;
			/* Name of the instantiation information file to
			   be used, or NULL if the default file name
			   should be used. */

EXTERN a_boolean
		instantiation_flags_in_template_info_file
#if VAR_INITIALIZERS
                          = INSTANTIATION_FLAGS_IN_TEMPLATE_INFO_FILE
#endif /* VAR_INITIALIZERS */
                                                                             ;
			/* TRUE if the flags used by automatic instantiation
			   should be placed in the template information file
			   instead of in the object file as variables. */

EXTERN a_boolean
		use_template_info_file
#if VAR_INITIALIZERS
                          = USE_TEMPLATE_INFO_FILE
#endif /* VAR_INITIALIZERS */
                                                                             ;
			/* TRUE if a template information file should be
			   created for information such as the names of
			   instantiation files created in one instantiation
			   per object mode, and for instantiation flags when
			   they are not put in the object file. */

EXTERN char	*template_info_file_name /* = NULL*/;
			/* The name of a file into which the front end should
			   write a list of files that were created that contain
			   instantiations. */

EXTERN char	*exported_template_file_name /* = NULL*/;
			/* The name of a file into which the front end should
			   write information about the exported templates
			   defined by the compilation. */

EXTERN char	*definition_list_file_name /* = NULL*/;
			/* The name of a file containing a list of functions
			   and static data members that are defined in the
			   objects and libraries with which the current file
			   is being linked.  This is used in automatic
		 	   instantiation mode to determine whether a given
			   entity can be instantiated in this file without
			   creating a conflict or an unneeded instantiation. */
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

#if DEFAULT_IMPLICIT_TEMPLATE_INCLUSION_MODE && DEFAULT_EXPORT_TEMPLATE_ALLOWED
 #error -- DEFAULT_IMPLICIT_TEMPLATE_INCLUSION_MODE and \
           DEFAULT_EXPORT_TEMPLATE_ALLOWED cannot both be tue
#endif /* DEFAULT_IMPLICIT_TEMPLATE_INCLUSION_MODE &&
          DEFAULT_EXPORT_TEMPLATE_ALLOWED */

#if DEFAULT_IMPLICIT_TEMPLATE_INCLUSION_MODE && \
    COMPILE_MULTIPLE_TRANSLATION_UNITS
 #error -- DEFAULT_IMPLICIT_TEMPLATE_INCLUSION_MODE and \
           COMPILE_MULTIPLE_TRANSLATION_UNITS cannot both be tue
#endif /* DEFAULT_IMPLICIT_TEMPLATE_INCLUSION_MODE &&
          COMPILE_MULTIPLE_TRANSLATION_UNITS */


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

EXTERN a_boolean
		verbose_pch_messages /* = FALSE */;
			/* TRUE if extra messages regarding the creation and
			   use of precompiled header files should be
			   suppressed. */

#if !USE_MMAP_FOR_MEMORY_REGIONS
EXTERN sizeof_t	pch_mem_size;
			/* Size of the preallocated PCH memory area. */
#endif /* !USE_MMAP_FOR_MEMORY_REGIONS */

EXTERN char	*pch_dir_name /* = NULL*/;
			/* Directory in which PCH files are to be stored.
			   NULL if no directory has been specified. */

EXTERN a_boolean
		restrict_enabled
#if VAR_INITIALIZERS
                                 = DEFAULT_RESTRICT_ENABLED
#endif /* VAR_INITIALIZERS */
                                                           ;
			/* TRUE if support for the restricted pointers is
			   provided, in which case "restrict" is recognized
			   as a keyword. */

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
EXTERN a_calling_convention
		default_calling_convention
#if VAR_INITIALIZERS
                                           = (a_calling_convention)cc_cdecl
#endif /* VAR_INITIALIZERS */
                                                                           ;
			/* The default calling convention.  cc_default is
			   considered compatible with this calling
			   convention. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
EXTERN a_boolean
		allow_nonstandard_anonymous_unions
#if VAR_INITIALIZERS
                                = DEFAULT_ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
#endif /* VAR_INITIALIZERS */
                                                                            ;
			/* If TRUE, a set of extensions is supported that
			   permits features similar to C++ anonymous unions
			   (1) in C mode and (2) with structs (in both C
			   and C++) and classes (in C++) as well.  This
			   functionality emulates an extension provided by
			   Microsoft C and C++ compilers. */
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */

EXTERN a_boolean
		wchar_t_is_keyword
#if VAR_INITIALIZERS
                             =
#if WCHAR_T_ENABLING_POSSIBLE
                               DEFAULT_WCHAR_T_IS_KEYWORD
#else /* !WCHAR_T_ENABLING_POSSIBLE */
                               FALSE
#endif /* WCHAR_T_ENABLING_POSSIBLE */
#endif /* VAR_INITIALIZERS */
                                                               ;
			/* Indicates whether wchar_t is to be considered a
                           keyword.  Once command line processing has been
			   completed, this value must only be TRUE in C++
                           mode. */

EXTERN a_boolean
		bool_is_keyword
#if VAR_INITIALIZERS
                             =
#if BOOL_ENABLING_POSSIBLE
                               DEFAULT_BOOL_IS_KEYWORD
#else /* !BOOL_ENABLING_POSSIBLE */
                               FALSE
#endif /* BOOL_ENABLING_POSSIBLE */
#endif /* VAR_INITIALIZERS */
                                                               ;
			/* Indicates whether bool is to be considered a
			   keyword in C++.  Also indicates that the result
			   type of comparisons is bool.  FALSE in C99,
			   even though there is a _Bool type. */

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
                alternative_tokens_allowed
#if VAR_INITIALIZERS
                                         = DEFAULT_ALTERNATIVE_TOKENS_ALLOWED
#endif /* VAR_INITIALIZERS */
                                                                           ;
                        /* TRUE if the C++ operator keywords (such as
			   "and", "or", "not", etc.) and digraphs should
			   be allowed.  This flag is automatically set
			   in strict mode. */

#if DO_IL_LOWERING && MINIMAL_INLINING
EXTERN a_boolean
		inlining_enabled
#if VAR_INITIALIZERS
                                 = TRUE
#endif /* VAR_INITIALIZERS */
                                       ;
			/* TRUE if minimal inlining should be done by IL
			   lowering. */
#endif /* DO_IL_LOWERING && MINIMAL_INLINING */

EXTERN a_boolean
                SVR4_C_mode
#if VAR_INITIALIZERS
                            = DEFAULT_SVR4_C_MODE
#endif /* VAR_INITIALIZERS */
                                                 ;
                        /* TRUE if the C++ operator keywords (such as
			   "and", "or", "not", etc.) and digraphs should
			   be allowed. */

EXTERN a_boolean
		address_of_ellipsis_allowed
#if VAR_INITIALIZERS
                            = DEFAULT_ADDRESS_OF_ELLIPSIS_ALLOWED
#endif /* VAR_INITIALIZERS */
                                           ;
			/* TRUE if "&..." is accepted. */

EXTERN a_boolean
		allow_ellipsis_only_param_in_C_mode
#if VAR_INITIALIZERS
                            = DEFAULT_ALLOW_ELLIPSIS_ONLY_PARAM_IN_C_MODE
#endif /* VAR_INITIALIZERS */
                                                   ;
			/* TRUE if an ellipsis alone is allowed as a parameter
			   list in C mode (e.g., "void f(...)"). */

EXTERN a_boolean
                allow_nonconst_ref_anachronism
#if VAR_INITIALIZERS
                            = DEFAULT_ALLOW_NONCONST_REF_ANACHRONISM
#endif /* VAR_INITIALIZERS */
                                               ;
                        /* TRUE if a reference to nonconst can be bound to
			   a class rvalue. */

EXTERN a_boolean
		building_runtime /* = FALSE*/;
			/* TRUE if we are compiling the runtime library.
			   Causes additional predefined macros to be
			   defined. */

EXTERN a_boolean
		remove_unneeded_entities
#if VAR_INITIALIZERS
                                         = DEFAULT_REMOVE_UNNEEDED_ENTITIES
#endif /* VAR_INITIALIZERS */
                                                                           ;
			/* When TRUE unneeded entities may be pruned from the
			   IL tree; otherwise, pruning is suppressed even if
			   entities are determined to be unneeded. Always
			   FALSE when MAINTAIN_NEEDED_FLAGS is FALSE.
			   Otherwise, controlled by command line option
			   --[no_]remove_unneeded_entities; also FALSE if
			   templates appear in the source program and
			   template instantiation is not under the control of
			   the front end (e.g., when the C++-generating back
			   end is used).  Value persists through compilation
			   of multiple files; used to reset global variable
			   okay_to_eliminate_unneeded_il_entries each time a
			   new translation unit is started. */

EXTERN a_boolean
		use_nonstandard_for_init_scope
#if VAR_INITIALIZERS
                                   = DEFAULT_USE_NONSTANDARD_FOR_INIT_SCOPE
#endif /* VAR_INITIALIZERS */
                                                                           ;
			/* TRUE if the scope of a name declared in a C++
			   for-init statement extends to the end of the scope
			   in which the for-statement appears and FALSE if
			   it extends only to the end of the for-statement;
			   the latter is standard-conforming behavior. */


EXTERN a_boolean
		warning_on_for_init_difference
#if VAR_INITIALIZERS
                                   = DEFAULT_WARNING_ON_FOR_INIT_DIFFERENCE
#endif /* VAR_INITIALIZERS */
                                                                           ;
			/* TRUE if the new C++ for-init scoping rules are in
			   effect and if a diagnostic should be issued when a
			   name that is visible with the new rules would be
			   hidden (by the for-init declaration itself) with
			   the old rules. */

EXTERN a_boolean
		allow_copy_assignment_op_with_base_class_param
#if VAR_INITIALIZERS
                    = DEFAULT_ALLOW_COPY_ASSIGNMENT_OP_WITH_BASE_CLASS_PARAM
#endif /* VAR_INITIALIZERS */
                                                                            ;
			/* TRUE if, in default mode, an assignment operator
			   for class A with parameter of type "B", "B&", or
			   "const B&" should be viewed as a copy assignment
			   operator when B is a base class of A.  FALSE is
			   the standard-conforming setting. */

EXTERN a_boolean
		guiding_decls_allowed
#if VAR_INITIALIZERS
                                      = DEFAULT_GUIDING_DECLS_ALLOWED
#endif /* VAR_INITIALIZERS */
                                                                     ;
			/* TRUE if guiding-declarations of template functions
			   are allowed. */

EXTERN a_boolean
		old_specializations_allowed
#if VAR_INITIALIZERS
                                       = DEFAULT_OLD_SPECIALIZATIONS_ALLOWED
#endif /* VAR_INITIALIZERS */
                                                                            ;
			/* TRUE if old-style template specialization
			   declarations are permitted (i.e., if "template <>"
			   syntax is not required). */

EXTERN a_boolean
		impl_conv_between_c_and_cpp_function_ptrs_allowed
#if VAR_INITIALIZERS
                 = DEFAULT_IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_ALLOWED
#endif /* VAR_INITIALIZERS */
                                                                            ;
			/* TRUE if implicit conversion between pointers to
			   extern "C" and extern "C++" function types is
			   permitted.  It is set to FALSE in strict mode or if
			   c_and_cpp_function_types_are_distinct is FALSE. */

#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
EXTERN a_boolean
		multibyte_chars_in_source_enabled
#if VAR_INITIALIZERS
                 = DEFAULT_MULTIBYTE_CHARS_IN_SOURCE_ENABLED
#endif /* VAR_INITIALIZERS */
                                                            ;
			/* TRUE if multibyte characters are allowed in
			   source code (in comments, string literals, and
			   character constants). */
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */

EXTERN a_boolean
		null_chars_allowed_in_source
#if VAR_INITIALIZERS
                                      = DEFAULT_NULL_CHARS_ALLOWED_IN_SOURCE
#endif /* VAR_INITIALIZERS */
                                                                            ;
			/* TRUE if null (zero) characters should be allowed
			   in source lines. */

EXTERN a_boolean
		report_embedded_cplusplus_noncompliance /* = FALSE */;
			/* TRUE to enforce the restricted version of C++
			   called "Embedded C++" (no namespaces, templates,
			   exceptions, RTTI, new-style casts, etc.).  The
			   severity of the diagnostic issued is controlled
			   by the discretionary-error mechanism. */

EXTERN a_boolean
		ptr_to_unknown_bound_array_allowed_in_param_type
#if VAR_INITIALIZERS
                  = DEFAULT_PTR_TO_UNKNOWN_BOUND_ARRAY_ALLOWED_IN_PARAM_TYPE
#endif /* VAR_INITIALIZERS */
                                                                            ;
			/* TRUE if in C++ a function parameter type may
			   include a pointer or reference to an array of
			   unknown size.  The standard disallows such param
			   types, but they are accepted by cfront, MSVC++,
			   and (reportedly) other C++ compilers. */

EXTERN a_boolean
		nonstandard_qualifier_deduction
#if VAR_INITIALIZERS
                  = DEFAULT_NONSTANDARD_QUALIFIER_DEDUCTION
#endif /* VAR_INITIALIZERS */
                                                                            ;
			/* TRUE if the nonstandard deduction using the
			   qualifier portion of a qualified name should be
			   performed.  This permits T to be deduced in
			   contexts such as A<T>::B or T::B.  The standard
			   deduction mechanism treats these as nondeduced
			   contexts that use the values of template parameters
			   that were either explicitly specified or deduced
			   elsewhere. */

EXTERN a_boolean
		do_late_ovl_res_tiebreaker
#if VAR_INITIALIZERS
                                          = DEFAULT_DO_LATE_OVL_RES_TIEBREAKER
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* TRUE if the tiebreaker processing in overload
			   resolution (e.g., to decide between "void f(int &)"
			   and "void f(const int &)") should be done late.
			   FALSE is the setting required for standard
			   conformance. */

EXTERN a_boolean
		single_ref_qual_ovl_res_tiebreaker
#if VAR_INITIALIZERS
                                  = DEFAULT_SINGLE_REF_QUAL_OVL_RES_TIEBREAKER
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* TRUE if, in overload resolution tiebreaker
			   processing, two matches can be compared for the
			   "addition of cv-qualifier under reference"
			   tiebreaker even if only one of them is a reference.
			   FALSE is the setting required for standard
			   conformance.  Ignored in cfront mode. */

EXTERN a_boolean
		one_instantiation_per_object
#if VAR_INITIALIZERS
                                             = FALSE
#endif /* VAR_INITIALIZERS */
                                                    ;
			/* TRUE if each externally linked function and static
			   data member should be generated in its own object
			   file. */

#if ONE_INSTANTIATION_PER_OBJECT
EXTERN char	*instantiation_dir_name /* = NULL*/;
			/* The name of the directory in which the instantiation
			   files should be created when one instantiation is
			   being put into each file. */
#endif /* ONE_INSTANTIATION_PER_OBJECT */

EXTERN a_boolean stdc_zero_in_nonstrict_mode
#if VAR_INITIALIZERS
                                             = STDC_ZERO_IN_NONSTRICT_MODE
#endif /* VAR_INITIALIZERS */
									  ;
			/* TRUE if __STDC__ should be defined to 0
			   in nonstrict mode and 1 in strict mode.
			   This flag affects both ANSI C and C++ mode
			   and overrides other factors that affect the
			   setting of __STDC__.  For example, __STDC__
			   will be defined even in Microsoft mode. */

EXTERN unsigned long
		max_pending_instantiations
#if VAR_INITIALIZERS
                                         = DEFAULT_MAX_PENDING_INSTANTIATIONS
#endif /* VAR_INITIALIZERS */
									     ;
			/* The maximum number of pending instantiations
			   of a given template that may be in process
			   at a given time.  This is used to detect
			   runaway recursive instantiations. */

#if MICROSOFT_EXTENSIONS_ALLOWED
EXTERN char	*import_dir_name /* = NULL */;
			/* The name of the directory in which files should be
			   sought for the Microsoft #import directive. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

EXTERN a_boolean
		enum_types_can_be_larger_than_int /* = FALSE */;
			/* TRUE when an enumerator type can be based on an
			   integer type that is larger than an int.  Always
			   FALSE in C mode; usually TRUE in C++ mode. */

EXTERN a_boolean
		enum_types_can_be_smaller_than_int /* = FALSE */;
			/* TRUE when an enumerator type can be based on an
			   integer type that is smaller than an int.  Always
			   FALSE if targ_enum_types_can_be_smaller_than_int
			   (an ABI requirement) is FALSE. */

#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
EXTERN a_boolean
	       instantiations_permitted_in_class_src_seq_list
#if VAR_INITIALIZERS
                    = DEFAULT_INSTANTIATIONS_PERMITTED_IN_CLASS_SRC_SEQ_LIST
#endif /* VAR_INITIALIZERS */
                                                                            ;
			/* Flag that indicates whether a source sequence
			   entry representing a template instantiation is
			   permitted within the portion of the source
			   sequence list representing a class definition. */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

#if NEED_NAME_MANGLING
EXTERN a_boolean
		compress_mangled_names
#if VAR_INITIALIZERS
                                       = DEFAULT_COMPRESS_MANGLED_NAMES
#endif /* VAR_INITIALIZERS */
                                                                       ;
			/* Indicates whether mangled names should be compressed
			   to reduce their size. */
#endif /* NEED_NAME_MANGLING */

#if NEED_NAME_MANGLING
EXTERN sizeof_t
		max_mangled_name_length
#if VAR_INITIALIZERS
                                        = DEFAULT_MAX_MANGLED_NAME_LENGTH
#endif /* VAR_INITIALIZERS */
                                                                         ;
			/* Maximum allowed length for a mangled name.
			   Zero means no limit. */
#endif /* NEED_NAME_MANGLING */

EXTERN char
		*include_file_suffixes
#if VAR_INITIALIZERS
                                        = DEFAULT_INCLUDE_FILE_SUFFIX_LIST
#endif /* VAR_INITIALIZERS */
                                                                          ;
			/* The file suffixes to be used when searching for an
			   include file name specified with no suffix.  This
			   is a colon-separated list of suffixes (but without
			   the "." delimiter). */

EXTERN char
		*curr_command_line_macro_def
#if VAR_INITIALIZERS
                                             = NULL
#endif /* VAR_INITIALIZERS */
                                                   ;
			/* Non-NULL if and only if we are processing a
			   command-line macro definition option of the form
			   -D<def>.  In that case it points to the null-
			   terminated byte string <def>. */

EXTERN a_boolean
		ignore_std_namespace
#if VAR_INITIALIZERS
                                      = FALSE
#endif /* VAR_INITIALIZERS */
                                              ;
			/* TRUE when the "std" namespace is treated as a
			   synonym for the global namespace.  This is a
			   g++ compatibility feature. */

EXTERN a_boolean
		end_of_line_comments_allowed /* = FALSE */;
			/* TRUE if "//" is accepted as a comment delimiter
			   (e.g., in C++, C99, and microsoft modes).  See
			   also END_OF_LINE_COMMENTS_ALLOWED_IN_C_MODE. */

EXTERN a_boolean
		flexible_array_members_allowed /* = FALSE */;
			/* TRUE if the final field of a struct may be an
			   incomplete array type.  This is part of the C99
			   standard and is permitted as an extension in C
			   mode.  It is also permitted in Microsoft mode
			   (both C and C++). */

EXTERN a_boolean
		universal_character_names_allowed /* = FALSE*/;
			/* TRUE if universal character names should be
			   accepted.  Permitted in C++ and C99 modes. */

EXTERN a_boolean
		va_copy_macro_allowed /* = FALSE*/;
			/* TRUE if the va_copy macro should be accepted.
			   It is permitted in C99 mode.  This is only
			   meaningful when passing stdarg references in
			   the generated code. */

EXTERN a_boolean
		long_long_is_standard /* = FALSE*/;
			/* TRUE if the long long type is should be considered
			   a standard data type (i.e., not an extension).
			   This is true in C99 mode. */

EXTERN a_boolean
		long_long_promotion_allowed /* = FALSE*/;
			/* TRUE if a value that is larger than a signed long
			   should promote to long long instead of promoting
			   to unsigned long.  This is usually FALSE except in
			   C99 mode. */

EXTERN a_boolean
		hex_floating_point_constants_allowed /* = FALSE*/;
			/* TRUE if hexadecimal floating point constants
			   are allowed (e.g., 0xabc.def).  This is true in
			   C99 mode. */

EXTERN a_boolean
		export_template_allowed;
			/* TRUE if the use of exported templates
			   is permitted. */

EXTERN a_boolean
		export_keyword_enabled;
			/* TRUE if the export keyword is recognized.  This
			   can be TRUE even if export_template_allowed is
			   FALSE.  In such cases, the syntax is be accepted
			   but a diagnostic is given indicating that the
			   feature is not enabled. */

EXTERN a_boolean
		suppress_inline_corresp_check;
			/* TRUE if the bodies of inline templates should not
			   be compared by the correspondence checking
			   routines. */

EXTERN a_boolean
		IEEE_handling_on_float_operation_exceptions
#if VAR_INITIALIZERS
                                                = TARG_HAS_IEEE_FLOATING_POINT
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* TRUE if exceptions in compile-time floating-point
			   conversions and operation folding (e.g., division
			   by zero) should be handled according to the IEEE
			   floating-point standard, i.e., they generate NaNs
			   and Infinities and no errors are issued. */


#if UPC_EXTENSIONS_ALLOWED

EXTERN a_host_large_integer
		upc_num_threads
#if VAR_INITIALIZERS
			= 0
#endif /* VAR_INITIALIZERS */
			   ;
			/* Indicates the compile-time number of threads.
			   If zero, indicates the number is determined at
			   run time. */

#endif /* UPC_EXTENSIONS_ALLOWED */


/* Process the command line arguments. */
extern void proc_command_line(int argc, char *argv[]);
#if COMPILE_MULTIPLE_SOURCE_FILES
/* Fetch the next source file name from the command line. */
extern a_boolean get_next_source_file(void);
#endif /* COMPILE_MULTIPLE_SOURCE_FILES */
#if COMPILE_MULTIPLE_TRANSLATION_UNITS
extern void proc_secondary_translation_units(void);
#endif /* COMPILE_MULTIPLE_TRANSLATION_UNITS */

extern void cmd_line_early_init(void);

extern void add_to_def_undef_list(char                   *str,
                                  a_def_undef_string_ptr *du_list,
                                  a_def_undef_string_ptr *du_list_end);

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
