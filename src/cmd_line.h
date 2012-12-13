/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2012 Edison Design Group Inc.                   [_]          *
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
  optk_C_mode,
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
  optk_diag_once,
  optk_display_error_number,
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
  optk_gen_c_file_name,
  optk_msvc_target_version,
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
  optk_inline_statement_limit,
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
#if SUN_EXTENSIONS_ALLOWED
  optk_sun_mode,
  optk_sun_linker_scope,
#endif /* SUN_EXTENSIONS_ALLOWED */
  optk_dependent_name_processing,
  optk_ignore_namespace_std,
  optk_parse_nonclass_templates,
  optk_c99_mode,
  optk_c89_mode,
  optk_export_template,
  optk_stdarg_builtin,
#if ENABLE_TRANS_UNIT_TEST_MODE
  optk_trans_unit_test_mode,
#endif /* ENABLE_TRANS_UNIT_TEST_MODE */
#if GNU_EXTENSIONS_ALLOWED
  optk_gcc_mode,
  optk_gpp_mode,
  optk_gnu_version,
  optk_report_gnu_extensions,
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
#if FIXED_POINT_ALLOWED
  optk_fixed_point,
#endif /* FIXED_POINT_ALLOWED */
#if NAMED_ADDRESS_SPACES_ALLOWED
  optk_named_address_spaces,
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
  optk_edg_base_directory,
#if NAMED_REGISTERS_ALLOWED
  optk_named_registers,
#endif /* NAMED_REGISTERS_ALLOWED */
  optk_embedded_c,
#if THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
  optk_thread_local_storage,
#endif /* THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED */
#if FULLY_RESOLVED_MACRO_POSITIONS
  optk_macro_positions_in_diagnostics,
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  optk_trigraphs,
  optk_nonstandard_default_arg_deduction,
  optk_stdc_zero_in_system_headers,
  optk_template_typedefs_in_diagnostics,
  optk_defer_parse_function_templates,
  optk_uliterals,
#if MICROSOFT_EXTENSIONS_ALLOWED
  optk_default_calling_convention,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  optk_type_traits_helpers,
  optk_cpp11_mode,
  optk_list_macros,
#if DUMP_CONFIG_ENABLED
  optk_dump_configuration,
#endif /* DUMP_CONFIG_ENABLED */
  optk_signed_bit_fields,
  optk_unsigned_bit_fields,
  optk_check_concatenations,
#if UNICODE_SOURCE_SUPPORTED
  optk_unicode_source_kind,
#endif /* UNICODE_SOURCE_SUPPORTED */
  optk_lambdas,
  optk_rvalue_references,
  optk_rvalue_ctor_is_copy_ctor,
  optk_gen_move_operations,
  optk_auto_type,
  optk_auto_storage,
  optk_nonstandard_instantiation_lookup,
  optk_nullptr,
#if GNU_EXTENSIONS_ALLOWED
  optk_gnu_c89_inlining,
  optk_nonstd_gnu_keywords,
  optk_default_nocommon,
#endif /* GNU_EXTENSIONS_ALLOWED */
  optk_token_separators_in_pp_output,
  optk_cpp11_sfinae,
  optk_cpp11_sfinae_ignore_access,
  optk_variadic_templates,
#if MICROSOFT_EXTENSIONS_ALLOWED
  optk_cppcli,
  optk_preusing,
  optk_assembly_using_dir,
  optk_using_framework_directory,
  optk_mscorlib_file_name,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  optk_cpp03_mode,
  optk_func_prototype_tags,
  optk_implicit_noexcept,
#if USE_FIXED_ADDRESS_FOR_MMAP
  optk_fixed_address_for_mmap,
#endif /* USE_FIXED_ADDRESS_FOR_MMAP */
  optk_last		/* Must be last. */
} an_option_kind;

/* C_dialect is in basics.h. */

EXTERN a_boolean
		strict_ansi_mode;
			/* -A option: issue warnings on nonstandard
			   features used, disable features that conflict
			   with ANSI C (i.e., asm). */

EXTERN a_boolean
                cfront_2_1_mode;
                        /* Accept language features supported
                           by cfront release 2.1. */
EXTERN a_boolean
                cfront_3_0_mode;
                        /* Accept language features supported
                           by cfront release 3.0. */

EXTERN a_boolean
                trans_unit_test_mode;
                        /* Enable mode to test compilation of multiple
                           (possibly identical) translation units. */

/*
Macro that is TRUE if any cfront mode has been selected.
*/
#define any_cfront_mode() (cfront_2_1_mode || cfront_3_0_mode)

EXTERN a_boolean
		pcc_preprocessing_mode;
			/* TRUE if old-style (Reiser cpp) preprocessing
			   should be done. */
EXTERN a_boolean
                allow_anachronisms;
                        /* Indicates whether anachronisms should be
                           accepted.  The default is supplied by a
                           configuration parameter. */
EXTERN a_boolean
                allow_nonconst_call_anachronism;
			/* Indicates whether the anachronism of calling
			   a non-const function on a const object should
			   be accepted. */

#if DEBUG
EXTERN int	init_debug_level;
			/* Initial debug level: n in -dn option, or 0
			   by default. */
#endif /* DEBUG */
EXTERN a_boolean
		do_preprocessing_only;
			/* If TRUE, the compiler is to act like cpp: the
			   source is preprocessed, but not compiled. */
EXTERN a_boolean
                pp_output_file_needed;
                        /* If TRUE, the compiler will output information to
			   the preprocessing output file.  This could be
			   preprocessed text, makefile dependency information,
			   etc. */
EXTERN a_boolean
		generate_pp_output;
			/* If TRUE, the preprocessing step should generate
			   a textual output file of the preprocessed text. */
EXTERN a_boolean
		keep_comments_in_pp_output;
			/* If TRUE, comments should be retained in
			   preprocessing output.  Meaningful only when
			   generate_pp_output is TRUE. */
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
EXTERN a_boolean
		gen_old_style_line_dirs;
			/* If TRUE, generate old-style line directives in
			   generated C/C++ output, i.e., "# nnn" instead of
			   "#line nnn". */
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
EXTERN a_boolean
		gen_line_info_in_pp_output;
			/* If TRUE, generate #line directives in
			   preprocessing output.  Meaningful only when
			   generate_pp_output is TRUE. */
EXTERN FILE	*f_pp_output;
			/* File to which preprocessing output is written.
			   Meaningful only when generate_pp_output is
			   TRUE. */
EXTERN char	*pp_file_name;
			/* Name of the preprocessing output file to be
			   opened, or NULL if no such file is needed or if
			   a default file should be used. */
EXTERN a_boolean
		list_included_files;
			/* When TRUE, write the names of #included files to
			   the error output. */
EXTERN a_boolean
		list_makefile_dependencies;
			/* When TRUE, write dependency lines for "make" to
			   stdout (for #include files encountered). */
EXTERN a_boolean
		list_macro_definitions;
			/* When TRUE, write macro definition lines to
			   stdout. */
EXTERN FILE	*f_raw_listing;
			/* If non-NULL (-L option), raw source lines and
			   context information are written to this file.
			   Such information could be read later by a program
			   to generate an interspersed listing. */
EXTERN FILE	*f_xref_info;
			/* If non-NULL (-X option), cross-reference information
			   is written to this file.  Such information could
			   be read and sorted later to produce a cross-
			   reference listing. */
EXTERN a_boolean
		suppress_back_end;
			/* TRUE if the back end should not be called.  The
			   -n option sets this to TRUE, and it is also TRUE
			   whenever do_preprocessing_only is TRUE. */
#if DO_IL_LOWERING
EXTERN a_boolean
		suppress_il_lowering;
			/* TRUE if IL lowering should not be done. */
#endif /* DO_IL_LOWERING */
#if IL_SHOULD_BE_WRITTEN_TO_FILE
EXTERN a_boolean
		suppress_il_file_write;
			/* TRUE if the writing of the IL file should be
			   suppressed. */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

/*
If the heuristic used to determine whether a virtual function table
should be defined cannot conclusively make such a determination,
vfd_suppress indicates that the definition should NOT be made, and
vfd_force indicates that it should.
*/
typedef enum /* a_virtual_function_definition_mode */ {
  vfd_normal,
  vfd_suppress,
  vfd_force
} a_virtual_function_definition_mode;

EXTERN a_virtual_function_definition_mode
		virtual_function_table_definition;
EXTERN a_boolean
		suppress_used_before_set_warnings;
			/* TRUE if used-before-set warnings should not be
			   issued on automatic local variables that are used
			   before a value is assigned to them; FALSE by
			   default.  Set by the -j command line option. */

EXTERN a_boolean
		addr_of_bit_field_allowed;
			/* TRUE if the address of a bit field may be taken
			   (provided it has a size and alignment that matches
			   some integral type). */

EXTERN a_boolean
		exceptions_enabled;
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
		noexcept_enabled;
			/* TRUE if C++11-style noexcept specifications and the
			   noexcept operator are accepted. */

EXTERN a_boolean
		implicit_noexcept_enabled;
			/* TRUE if destructors and deallocation functions
			   (i.e., operator delete and operator delete[]) have
			   implicit noexcept specifications if no explicit
			   exception specification is provided. */

EXTERN a_boolean
		constexpr_enabled;
			/* TRUE if constexpr functions and variables are
			   accepted (a C++11 feature). */

EXTERN a_boolean
		rtti_enabled;
			/* TRUE if support for runtime type identification
			   (RTTI) is enabled.  Significant only in C++ mode.
			   RTTI cannot be enabled if the extended typeinfo
			   for it is not generated. */

#if DO_IL_LOWERING && ABI_CHANGES_FOR_RTTI
EXTERN a_boolean
		generate_rtti_typeinfo;
			/* TRUE if the typeinfo tables that support RTTI
			   should be generated.  If FALSE, typeinfo tables
			   will be generated only for types used in exceptions.
			   Must be TRUE if rtti_enabled is TRUE.  See
			   SUPPRESS_TYPEINFO_VARIABLES_WHEN_RTTI_DISABLED. */
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_RTTI */

EXTERN a_boolean
		array_new_and_delete_enabled;
			/* TRUE if support for array new and delete is
			   enabled.  Significant only in C++ mode.  They
			   cannot be enabled if the ABI changes for them
			   are not enabled. */
EXTERN a_boolean
		explicit_keyword_enabled;
			/* TRUE if the "explicit" keyword is recognized.
			   Significant only in C++ mode. */
EXTERN a_boolean
		namespaces_enabled;
			/* TRUE if support for namespaces is enabled.
			   Significant only in C++ mode. */
EXTERN a_boolean
		implicit_using_std;
			/* TRUE if the runtime should implicitly do a
			   "using namespace std".  Significant only in
			    C++ mode. */

EXTERN a_boolean
		typename_enabled;
			/* TRUE if support for typename is enabled.
			   Significant only in C++ mode. */

EXTERN a_boolean
		implicit_typename_enabled;
			/* TRUE if the front end should determine from context
			   whether a template parameter dependent name is a
			   type or nontype.  Significant only in C++ mode. */

EXTERN a_boolean
		force_implicit_typename;
			/* TRUE if implicit typename was explicitly requested
			   on the command-line and so should not be disabled
			   in some contexts. */

EXTERN a_boolean
		extern_inline_allowed;
			/* TRUE if inline functions are allowed to have
			   external linkage (as specified by the standard) and
			   FALSE if they imply internal linkage (as specified
			   in the ARM).  Significant only in C++ mode. */

EXTERN a_boolean
		floating_point_template_parameters_allowed;
			/* TRUE if template parameters of floating-point type
			   are allowed (which is nonstandard). */

EXTERN a_boolean
		vla_enabled;
			/* TRUE if support for variable length arrays (VLAs)
			   is enabled.  Controlled by command-line options
			   --[no_]vla and enabled/disabled by some modes. */

EXTERN a_boolean
		vla_deallocations_in_il;
			/* TRUE if enk_vla_dealloc nodes should be generated
			   to mark the points at which VLA objects pass out of
			   scope and may be deallocated.  Never TRUE in C++
			   mode; VLA deallocations are implicit in C++ and tied
			   to object lifetimes. */

EXTERN a_boolean
		operator_overloading_on_enums_enabled;
			/* TRUE if operator functions can be used to
			   overload operations on enums. */

EXTERN a_boolean
		string_literals_are_const;
			/* TRUE if string literals are const, i.e.,
			   array[n] of const char.  Also controls wide
			   string literals. */

EXTERN a_boolean
		class_name_injection_enabled;
			/* TRUE if class names are injected into the scope
			   of the class. */

EXTERN a_boolean
		arg_dependent_lookup_enabled;
			/* TRUE if argument dependent lookup of function
			   names should be performed. */

EXTERN a_boolean
		dependent_lookup_finds_static_functions;
			/* TRUE if dependent lookup will find static functions.
			   C++03 did not find them.  Core Issue 561 changed
			   that for C++11. */

EXTERN a_boolean
		friend_class_injection_enabled;
			/* TRUE if class names first declared in friend
			   declarations are visible. */

EXTERN a_boolean
		friend_function_injection_enabled;
			/* TRUE if function names first declared in friend
			   declarations are visible. */

EXTERN a_boolean
		do_dependent_name_processing;
			/* TRUE if special processing for dependent names
			   in templates should be done.  This also enables
			   prototype instantiations of function bodies and
			   default arguments. */

EXTERN a_boolean
		nonstandard_instantiation_lookup_enabled;
			/* TRUE if the instantiation lookup rules in effect
			   during part of the standardization of C++98
			   should be used.  See the definition of
			   DEFAULT_NONSTANDARD_INSTANTIATION_LOOKUP
			   (which is used as the initial value of this
			   variable) for more information. */

EXTERN a_boolean
		gpp_dependent_name_lookup;
			/* TRUE if special lookup rules should be used that
			   emulate the behavior of g++.  An initial lookup is
			   done that ignores dependent base classes and a
			   second pass is made that considers such bases if
			   the first lookup did not find a symbol.  In
			   addition, names declared after the point of
			   definition of the template are ignored. */

EXTERN a_boolean
		gpp_using_directive_lookup;
			/* TRUE if special rules should be used to determine
			   which using-directives should be visible during
			   template instantiations in order to emulate the
			   behavior of g++. */

EXTERN a_boolean
		parameters_visible_late;
			/* TRUE if parameters should remain invisible while
			   parsing a function declarator.  (This is the
			   behavior of g++ prior to GCC 4.4.) */

EXTERN a_boolean
		friend_class_decl_can_find_using_dir;
			/* TRUE if a friend class declaration can find names
			   made visible by using-directives.  This is used
			   in g++ and Microsoft modes. */

EXTERN a_boolean
		nonclass_prototype_instantiations;
			/* TRUE if nonclass template declarations should
			   have prototype instantiations performed on them.
			   This is initialized to the same value as
			   do_dependent_name_processing because it is a
			   prerequisite. */

EXTERN a_boolean
		defer_function_prototype_instantiations;
			/* TRUE if the prototype instantiation of a function
			   should be deferred until the first use of the
			   function.  This is useful to avoid reporting errors
			   on unused templates (because those templates are
			   accepted by other compilers).  This is only
			   tested if nonclass_prototype_instantiations
			   is TRUE.  Note that this can change the meaning of
			   certain programs because the state will be different
			   when the instantiation is done (additional default
			   arguments may be present, classes may be complete,
			   etc.).  Prototype instantiations cannot be deferred
			   in some modes.  See FUNCTION_PROTOTYPE_-
			   INSTANTIATION_DEFERRAL_ALLOWED for more
			   information. */

EXTERN a_boolean
		defer_friend_instantiation;
			/* TRUE if the semantic analysis of friend functions
			   of class templates should be deferred until the
			   function is used. */

EXTERN a_boolean
		nonstandard_using_decl_allowed;
			/* TRUE if a nonstandard nonmember using-declaration
                           that uses an unqualified name should be accepted. */

EXTERN a_boolean
		designators_allowed;
			/* TRUE if '.x' and '[expr]' designators should be
			   accepted.  Always TRUE if
			   extended_designators_allowed is TRUE. */

EXTERN a_boolean
		extended_designators_allowed;
			/* TRUE if 'x:' and '[expr ... expr]' designators
			   should be accepted. */

EXTERN a_boolean
		variadic_macros_allowed;
			/* TRUE if '#define VM(x, ...) __VA_ARGS__' should be
			   accepted. */

EXTERN a_boolean
		pragma_operator_allowed;
			/* TRUE if the _Pragma operator (originally from C99)
			   should be accepted. */

EXTERN a_boolean
		extended_variadic_macros_allowed;
			/* TRUE if '#define EVM(args ...) args' should be
			   accepted; also enables the deletion of the
			   comma in a replacement like 'x, ## __VA_ARGS__'
			   when the variadic argument is empty. */

EXTERN a_boolean
		compound_literals_allowed;
			/* TRUE if C99 compound literals, which look like a
			   cast including a brace-enclosed initializer, e.g.,
			   (int []){1, 2, 3}, should be accepted. */

EXTERN a_boolean
		fixed_point_enabled;
			/* TRUE if the fixed-point extensions of ISO TR 18037
			   (aka. "Embedded C") should be accepted. */

#if NAMED_ADDRESS_SPACES_ALLOWED
EXTERN a_boolean
		named_address_spaces_enabled;
			/* TRUE if the extension of ISO TR 18037 (aka.
			   "Embedded C") for named address spaces should be
			    accepted. */
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */

#if NAMED_REGISTERS_ALLOWED
EXTERN a_boolean
		named_registers_enabled;
			/* TRUE if the extension of ISO TR 18037 (aka.
			   "Embedded C") for named-register storage classes
			    should be accepted. */
#endif /* NAMED_REGISTERS_ALLOWED */

#if DO_IL_LOWERING
EXTERN a_boolean
		pointer_to_member_call_optimization_allowed;
			/* TRUE if optimized code can be generated for certain
			   pointer to member calls.  The C++ standard disallows
			   this optimization. */
#endif /* DO_IL_LOWERING */

EXTERN a_boolean
		no_access_check_on_friend_declarator_ids;
			/* TRUE if no access check should be performed on
			   declarator-ids of friend function declarators. */

EXTERN a_boolean
		special_subscript_cost;
			/* TRUE if the cost of the subscript operator []'s
			   integral operand is always considered a standard
			   conversion in overload resolution.  This is
			   nonstandard, but a fair number of programs
			   depend on it. */

EXTERN a_boolean
		long_preserving_rules;
			/* TRUE if the K&R rules for usual arithmetic
			   conversions involving "long" should be used.
			   This means the rules described in the K&R I book,
			   not the rules used by the pcc compiler. */

EXTERN a_def_undef_string_ptr
		defs_from_cmd_line;
			/* The list of -D and -U options from the command
			   line, defining and undefining macro symbols. */


EXTERN a_boolean
                allow_dollar_in_id_chars;
                        /* Specifies whether dollar signs are allowed
                           in identifiers.  The default is supplied by
                           a configuration parameter. */

EXTERN a_boolean
                display_compilation_time;
                        /* TRUE if compilation timing statistics should be
			   displayed. */

EXTERN a_boolean
		allow_in_class_specializations;
			/* TRUE if Microsoft and Sun in-class specializations
			   should be allowed. */

EXTERN a_boolean
		record_form_of_name_reference;
			/* TRUE if the form of all name references should be
			   recorded in the IL.  When this is FALSE, some
			   name references may still be recorded (e.g.,
			   if needed for ABI purposes). */

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
                instantiation_mode;
                        /* The default template instantiation mode. */

EXTERN a_boolean
		instantiate_before_pch_creation;
			/* TRUE if instantiations should be done before a
			    precompiled header file is created so that they
			    will not have to be done in each file that uses
			    the PCH. */

#if AUTOMATIC_TEMPLATE_INSTANTIATION
EXTERN a_boolean
                automatic_instantiation_mode;
                        /* Should automatic instantiation processing be
			   performed.  This includes both the generation of
 			   the instantiation flags and the processing of the
			   instantiation list. */

EXTERN a_boolean
		suppress_instantiation_flags;
			/* Should the instantiation flags that are normally
			   generated as part of the automatic instantiation
			   process be suppressed. */

EXTERN char	*ii_file_name;
			/* Name of the instantiation information file to
			   be used, or NULL if the default file name
			   should be used. */

EXTERN a_boolean
		instantiation_flags_in_template_info_file;
			/* TRUE if the flags used by automatic instantiation
			   should be placed in the template information file
			   instead of in the object file as variables. */

EXTERN a_boolean
		use_template_info_file;
			/* TRUE if a template information file should be
			   created for information such as the names of
			   instantiation files created in one instantiation
			   per object mode, and for instantiation flags when
			   they are not put in the object file. */

EXTERN char	*template_info_file_name;
			/* The name of a file into which the front end should
			   write a list of files that were created that contain
			   instantiations. */

EXTERN char	*exported_template_file_name;
			/* The name of a file into which the front end should
			   write information about the exported templates
			   defined by the compilation. */

EXTERN char	*definition_list_file_name;
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
                implicit_template_inclusion_mode;
                        /* Should the front end attempt to implicitly include
			   a source file (e.g., .c file) to find the
			   definition of a template. */

#if DEFAULT_IMPLICIT_TEMPLATE_INCLUSION_MODE && DEFAULT_EXPORT_TEMPLATE_ALLOWED
 #error -- DEFAULT_IMPLICIT_TEMPLATE_INCLUSION_MODE and \
           DEFAULT_EXPORT_TEMPLATE_ALLOWED cannot both be true
#endif /* DEFAULT_IMPLICIT_TEMPLATE_INCLUSION_MODE &&
          DEFAULT_EXPORT_TEMPLATE_ALLOWED */

#if DEFAULT_IMPLICIT_TEMPLATE_INCLUSION_MODE && \
    COMPILE_MULTIPLE_TRANSLATION_UNITS
 #error -- DEFAULT_IMPLICIT_TEMPLATE_INCLUSION_MODE and \
           COMPILE_MULTIPLE_TRANSLATION_UNITS cannot both be true
#endif /* DEFAULT_IMPLICIT_TEMPLATE_INCLUSION_MODE &&
          COMPILE_MULTIPLE_TRANSLATION_UNITS */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */


EXTERN a_boolean
		display_error_number;
			/* Should the diagnostic message output include the
		           error number. */


#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
EXTERN char	*gen_c_file_name;
			/* Points to a string specifying the name of the
			   generated C file to be created.  The front end
			   will generate a name if this string is NULL. */
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */

EXTERN a_boolean
		precompiled_header_processing_required;
			/* TRUE if any kind of precompiled header
			   processing is required by this compilation. */

EXTERN a_boolean
		create_precompiled_header;
			/* TRUE if this compilation should create a
			   precompiled header file. */

EXTERN a_boolean
		use_precompiled_header;
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
		automatic_pch_processing;
			/* TRUE if the compiler should automatically
			   determine whether to build and/or use a
			   precompiled header file. */

EXTERN a_boolean
		suppress_pch_messages;
			/* TRUE if messages regarding the creation and
			   use of precompiled header files should be
			   suppressed. */

EXTERN a_boolean
		verbose_pch_messages;
			/* TRUE if extra messages regarding the creation and
			   use of precompiled header files should be
			   suppressed. */

#if !USE_MMAP_FOR_MEMORY_REGIONS
EXTERN sizeof_t	pch_mem_size;
			/* Size of the preallocated PCH memory area. */
#endif /* !USE_MMAP_FOR_MEMORY_REGIONS */

EXTERN char	*pch_dir_name;
			/* Directory in which PCH files are to be stored.
			   NULL if no directory has been specified. */

EXTERN a_boolean
		restrict_enabled;
			/* TRUE if support for the restricted pointers is
			   provided.  This is TRUE if any form of the
			   restrict keyword is allowed. */

EXTERN a_boolean
		restrict_keyword_enabled;
			/* TRUE if support for the restricted pointers is
			   provided, in which case "restrict" is recognized
			   as a keyword. */

EXTERN a_boolean
		gnu_restrict_keyword_enabled;
			/* TRUE if the GNU __restrict variant of the restrict
			   keyword is recognized (this is also TRUE in some
			   Microsoft modes). */

EXTERN a_boolean
		nonstd_gnu_keywords_enabled;
			/* TRUE if GNU keywords matching identifiers otherwise
			   available to user programs (e.g., "typeof" but not
			   "__typeof") should be enabled. */

EXTERN a_boolean
		gnu_namespace_and_class_in_same_scope;
			/* TRUE if the g++ bug of allowing a namespace and
			   a class with the same name to be declared in a
			   given scope should be emulated. */

EXTERN a_boolean
		long_lifetime_temps;
			/* If FALSE, temporaries have lifetimes that end at
			   end of full expression.  If TRUE, temporaries
			   have lifetimes that end at end of scope, label,
			   or end of switch clause. */

EXTERN a_boolean
		type_traits_helpers_enabled;
			/* TRUE if support for built-in pseudo-functions (like
			   __is_union) to help implement ISO/IEC TR 19768 (aka.
			   "C++ Library TR1") should be enabled. */

EXTERN a_boolean
		explicit_conversion_functions_enabled;
			/* TRUE if support for explicit conversion function
			   members (a C++11 and C++/CLI feature) should be
			   enabled. */

EXTERN a_boolean
		explicit_enum_base_enabled;
			/* TRUE if the syntax "enum X: base-type { ... }"
			   should be accepted.  (This was originally a
			   Microsoft extension based on ECMA-372 C++/CLI, and
			   was later adopted in C++11.) */

EXTERN a_boolean
		report_explicit_enum_base_as_nonstandard;
			/* TRUE if the use of explicit enum base syntax should
			   be warned about.  (This is the case in certain
			   GNU modes.) */

EXTERN a_boolean
		enum_qualifiers_enabled;
			/* TRUE if an enumerator constant can be qualified
			   with an enumerator name.  E.g.:
			      enum E { e };  E x = E::e;
			   This is a C++11 feature originally introduced in
			   some Microsoft compilers. */

EXTERN a_boolean
		opaque_enum_decls_enabled;
			/* TRUE if C++11-style opaque enumeration declarations
			   (like "enum B: char;") should be accepted. */

EXTERN a_boolean
		lambdas_enabled;
			/* TRUE if C++11 lambdas should be accepted in C++. */

EXTERN a_boolean
		rvalue_references_enabled;
			/* TRUE if C++11 rvalue references should be accepted
			   in C++. */

EXTERN a_boolean
		rvalue_ctor_is_copy_ctor;
			/* TRUE if a move constructor/move assignment operator
			   is considered a copy constructor/copy assignment
			   operator.  If so, declaring the former disables the
			   implicit generation of the latter. */

EXTERN a_boolean
		generate_move_operations;
			/* TRUE if the front end can implicitly generate move
			   constructors and move assignment operators in some
			   classes.  This is standard behavior in C++11. */

EXTERN a_boolean
		defaulted_special_members_enabled;
			/* TRUE if special member functions can be defined
			   with the C++11 "= default" syntax. */

EXTERN a_boolean
		deleted_functions_enabled;
			/* TRUE if functions can be declared with the C++11
			   "= delete" syntax. */

EXTERN a_boolean
		local_types_as_template_args_enabled;
			/* TRUE if local and unnamed types are allowed as
			   template arguments. */

EXTERN a_boolean
		inexact_ptr_to_member_deduction_enabled;
			/* TRUE if, in template argument deduction, a base
			   vs. derived difference of the member class type
			   is allowed in some cases. */

EXTERN a_boolean
		decls_using_types_without_linkage_allowed;
			/* TRUE if an entity with linkage can be declared
			   using a type without linkage provided the
			   entity is defined in the translation unit if
			   used.  This is the rule used in C++11 and it must
			   be used when local and unnamed types can be
			   used as template arguments. */

EXTERN a_boolean
		trailing_return_types_enabled;
			/* TRUE if a function return type may trail the
			   corresponding function declarator (in syntax like
			   "(int, int)->int").  This is a C++11 extension. */

EXTERN a_boolean
		this_in_trailing_return_types_enabled;
			/* When TRUE, "this" can be used within late-specified
			   return types of member functions. */

EXTERN a_boolean
		list_init_enabled;
			/* When TRUE, C++11-style list initialization is
			   accepted. */

EXTERN a_boolean
		field_initializers_enabled;
			/* When TRUE, C++11-style field initializers are
			   accepted. */

EXTERN a_boolean
		alias_declarations_enabled;
			/* TRUE if C++11 alias-declarations and alias templates
			   are allowed. */

EXTERN a_boolean
		variadic_templates_enabled;
			/* TRUE if C++11 variadic templates (i.e., parameter
			   packs) are accepted. */

EXTERN a_boolean
		std_attributes_enabled;
			/* TRUE if C++11 attribute syntax (e.g., [[final]]) is
			   accepted. */

EXTERN a_boolean
		inline_namespaces_enabled;
			/* TRUE if C++11 inline namespaces are accepted.
			   Note that when this is FALSE, g++ mode strong
			   using directives (which are implemented using
			   a variant of the inline namespace mechanism)
			   are still allowed in g++ mode. */

EXTERN a_boolean
		gnu_attributes_enabled;
			/* TRUE if GNU attribute syntax is accepted (e.g.,
			   __attribute((noreturn))). */

EXTERN a_boolean
		ms_declspec_attributes_enabled;
			/* TRUE if Microsoft __declspec attribute syntax is
			   accepted (e.g., __declspec((dllexport))). */


#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED
EXTERN a_calling_convention
		default_calling_convention;
			/* The default calling convention.  cc_default is
			   considered compatible with this calling
			   convention. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED */

#if MICROSOFT_EXTENSIONS_ALLOWED
EXTERN a_boolean
		microsoft_64bit_pointer_extensions_enabled;
			/* TRUE if 64-bit pointer extensions (__ptr32/__ptr64
			   and __sptr/__uptr) should be accepted in Microsoft
			   modes. */
EXTERN char
		*mscorlib_file_name;
			/* If non-NULL, the name of the file to be used
			   to load mscorlib instead of using the normal
			   search mechanism.  This can be used, for example,
			   if a development version of mscorlib is to
			   be used.  If this is not an absolute path name,
			   it will be searched for using the normal
			   assembly search path mechanism. */
EXTERN a_boolean
		using_framework_directory;
			/* TRUE if assemblies should be searched for in the
			   directory .NET is installed in. */
#if WRITE_CPPCLI_PORTABLE_ASSEMBLIES
EXTERN a_boolean
		generate_portable_assemblies;
			/* TRUE if generating portable assemblies for use
			   in testing C++/CLI on non-Windows platforms. */
#endif /* WRITE_CPPCLI_PORTABLE_ASSEMBLIES */

EXTERN a_boolean
		generic_arity_overload_allowed;
			/* TRUE if C++/CLI generic classes with different arity
			   (number of generic parameters) can exist in the
			   same scope and if a generic class and non-generic
			   class can have the same name in a given scope.
			   This feature is always allowed for generics
			   imported from metadata.  This controls the
			   availability of the feature as a source feature. */

EXTERN a_boolean
		disable_access_checking_in_microsoft_enum_bases;
			/* TRUE if in Microsoft mode, the front end should
			   emulate the Microsoft behavior of not performing
			   access checking on enum base specifiers. */

EXTERN a_boolean
		pending_generic_constraint_specifier_enabled;
			/* TRUE if in C++/CLI mode, constraint clauses for a
			   generic class declaration can be replaced by "..."
			   until a redeclaration specifies the actual
			   constraints.  This extensions is always accepted
			   while processing code generated from metadata, but
			   is enabled more widely for internal testing. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
EXTERN a_boolean
		allow_nonstandard_anonymous_unions;
			/* If TRUE, a set of extensions is supported that
			   permits features similar to C++ anonymous unions
			   (1) in C mode and (2) with structs (in both C
			   and C++) and classes (in C++) as well.  This
			   functionality emulates an extension provided by
			   Microsoft and GNU compilers (in both C and C++
			   modes). */
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */

EXTERN a_boolean
		uliterals_enabled;
			/* TRUE if and only if TR 19769 literals of the forms
			   u'...', U'...', u"...", and U"..." should be
			   accepted. */

EXTERN a_boolean
		wchar_t_is_keyword;
			/* Indicates whether wchar_t is to be considered a
                           keyword.  Once command line processing has been
			   completed, this value must only be TRUE in C++
                           mode. */

EXTERN a_boolean
                char16_t_and_char32_t_are_keywords;
                        /* Indicates whether char16_t and char32_t are to be
                           considered keywords.  Once command line processing
                           has been completed, this value must only be TRUE in
                           C++ mode. */

EXTERN a_boolean
		bool_is_keyword;
			/* Indicates whether bool is to be considered a
			   keyword in C++.  Also indicates that the result
			   type of comparisons is bool.  FALSE in C99,
			   even though there is a _Bool type. */

#if USER_CONTROL_OF_STRUCT_PACKING
EXTERN a_targ_alignment
		default_max_member_alignment;
			/* If nonzero, the maximum alignment of any nonstatic
			   data member of a class, struct, or union, unless a
			   "#pragma pack" overrides it.  Its value is based
			   on command-line option "--pack_alignment".  (A zero
			   value means that a member's alignment is based
			   solely on its type.) */
#endif /* USER_CONTROL_OF_STRUCT_PACKING */

EXTERN a_boolean
                alternative_tokens_allowed;
                        /* TRUE if the C++ operator keywords (such as
			   "and", "or", "not", etc.) and digraphs should
			   be allowed.  This flag is automatically set
			   in strict mode. */

EXTERN a_boolean
		trigraphs_allowed;
			/* TRUE if trigraphs should be allowed. */

#if DO_IL_LOWERING && MINIMAL_INLINING
EXTERN a_boolean
		inlining_enabled;
			/* TRUE if minimal inlining should be done by IL
			   lowering. */

EXTERN a_host_large_unsigned
                inline_statement_limit;
                        /* The maximum number of statements that a routine
                           can contain and still be eligible for inlining.
                           The value is somewhat arbitrary, and is used to
                           prevent memory exhaustion in pathological cases. */
#endif /* DO_IL_LOWERING && MINIMAL_INLINING */

EXTERN a_boolean
		std_c99_inlining;
			/* TRUE if the standard C99 semantics should be
			   assigned to the inline keyword.  (Can only be
			   TRUE in C99 modes and only when gnu_c89_inlining
			   is FALSE.) */

EXTERN a_boolean
		gnu_c89_inlining;
			/* TRUE if the older GNU C semantics should be
			   assigned to the inline keyword.  (Can only be
			   TRUE in GNU C modes and only when std_c99_inlining
			   is FALSE.) */

EXTERN a_boolean
                SVR4_C_mode;
                        /* TRUE if SVR4 C compatibility features should be
			   recognized. */

EXTERN a_boolean
		address_of_ellipsis_allowed;
			/* TRUE if "&..." is accepted. */

EXTERN a_boolean
		allow_ellipsis_only_param_in_C_mode;
			/* TRUE if an ellipsis alone is allowed as a parameter
			   list in C mode (e.g., "void f(...)"). */

EXTERN a_boolean
		func_prototype_tags_enabled;
			/* TRUE if tags can be entered in a function prototype
			   scope.  This is standard behavior in C that can be
			   overridden (e.g., when emulating Microsoft).  In C++
			   mode, this is always FALSE. */

EXTERN a_boolean
                allow_nonconst_ref_anachronism;
                        /* TRUE if a reference to nonconst can be bound to
			   a class rvalue. */

EXTERN a_boolean
		building_runtime;
			/* TRUE if we are compiling the runtime library.
			   Causes additional predefined macros to be
			   defined. */

EXTERN a_boolean
		remove_unneeded_entities;
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
		use_nonstandard_for_init_scope;
			/* TRUE if the scope of a name declared in a C++
			   for-init statement extends to the end of the scope
			   in which the for-statement appears and FALSE if
			   it extends only to the end of the for-statement;
			   the latter is standard-conforming behavior. */

EXTERN a_boolean
		microsoft_type_dependent_for_init_scope;
			/* TRUE if the scope of a variable declared in a C++
			   for-init statement should be handled as the default
			   MSVC++ 7.1 behavior: Variables with destructors
			   follow the new rules, whereas variables without
			   destructors follow the old rules.  When TRUE,
			   use_nonstandard_for_init_scope must be FALSE. */


EXTERN a_boolean
		warning_on_for_init_difference;
			/* TRUE if the new C++ for-init scoping rules are in
			   effect and if a diagnostic should be issued when a
			   name that is visible with the new rules would be
			   hidden (by the for-init declaration itself) with
			   the old rules. */

EXTERN a_boolean
		allow_copy_assignment_op_with_base_class_param;
			/* TRUE if, in default mode, an assignment operator
			   for class A with parameter of type "B", "B&", or
			   "const B&" should be viewed as a copy assignment
			   operator when B is a base class of A.  FALSE is
			   the standard-conforming setting. */

EXTERN a_boolean
		guiding_decls_allowed;
			/* TRUE if guiding-declarations of template functions
			   are allowed. */

EXTERN a_boolean warning_on_non_template_friend;
			/* TRUE if a message should be issued indicating that
			   a friend declaration was probably intended to be
		           a guiding declaration. */

EXTERN a_boolean
		old_specializations_allowed;
			/* TRUE if old-style template specialization
			   declarations are permitted (i.e., if "template <>"
			   syntax is not required). */

EXTERN a_boolean
		impl_conv_between_c_and_cpp_function_ptrs_allowed;
			/* TRUE if implicit conversion between pointers to
			   extern "C" and extern "C++" function types is
			   permitted.  It is set to FALSE in strict mode or if
			   c_and_cpp_function_types_are_distinct is FALSE. */

EXTERN a_boolean
		check_printf_scanf_positional_args;
			/* TRUE if positional printf/scanf arguments, i.e.,
			   using dollar signs as in printf("%2$d%1$d\n", 1, 2)
			   should be recognized and checked. */

#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
EXTERN a_boolean
		multibyte_chars_in_source_enabled;
			/* TRUE if multibyte characters are allowed in
			   source code (in comments, string literals, and
			   character constants). */
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */

EXTERN a_boolean
		null_chars_allowed_in_source;
			/* TRUE if null (zero) characters should be allowed
			   in source lines. */

EXTERN a_boolean
		report_embedded_cplusplus_noncompliance;
			/* TRUE to enforce the restricted version of C++
			   called "Embedded C++" (no namespaces, templates,
			   exceptions, RTTI, new-style casts, etc.).  The
			   severity of the diagnostic issued is controlled
			   by the discretionary-error mechanism. */

EXTERN a_boolean
		report_gnu_extensions;
			/* TRUE to diagnose (with a warning) uses of certain
			   GNU extensions outside system header files. */

EXTERN a_boolean
		ptr_to_unknown_bound_array_allowed_in_param_type;
			/* TRUE if in C++ a function parameter type may
			   include a pointer to an array of unknown size.
			   The standard disallows such parameter types, but
			   they are accepted by cfront, MSVC++, and
			   (reportedly) other C++ compilers. */

EXTERN a_boolean
		ref_to_unknown_bound_array_allowed_in_param_type;
			/* TRUE if in C++ a function parameter type may
			   include a reference to an array of unknown size.
			   The standard disallows such parameter types, but
			   they are accepted by cfront.  This variable should
			   not be set to TRUE when the similar flag variable
			   ptr_to_unknown_bound_array_allowed_in_param_type
			   is FALSE. */

EXTERN a_boolean
		nonstandard_qualifier_deduction;
			/* TRUE if the nonstandard deduction using the
			   qualifier portion of a qualified name should be
			   performed.  This permits T to be deduced in
			   contexts such as A<T>::B or T::B.  The standard
			   deduction mechanism treats these as nondeduced
			   contexts that use the values of template parameters
			   that were either explicitly specified or deduced
			   elsewhere. */

EXTERN a_boolean
		nonstandard_default_arg_deduction;
			/* TRUE if default arguments should not be removed
			   from deduced function types. */

EXTERN a_boolean
		function_template_default_args_allowed;
			/* TRUE if function template parameters may have
			   default arguments. */

EXTERN a_boolean
		do_late_ovl_res_tiebreaker;
			/* TRUE if the tiebreaker processing in overload
			   resolution (e.g., to decide between "void f(int &)"
			   and "void f(const int &)") should be done late.
			   FALSE is the setting required for standard
			   conformance. */

EXTERN a_boolean
		single_ref_qual_ovl_res_tiebreaker;
			/* TRUE if, in overload resolution tiebreaker
			   processing, two matches can be compared for the
			   "addition of cv-qualifier under reference"
			   tiebreaker even if only one of them is a reference.
			   FALSE is the setting required for standard
			   conformance.  Ignored in cfront mode. */

EXTERN a_boolean
		late_template_ovl_res_tiebreaker;
			/* TRUE if, in overload resolution tiebreaker
			   processing, the template vs. non-template test
			   is to be done after all the other tests.
			   Currently (Jan. 2005) the standard requires
			   FALSE, but a core issue is being opened to
			   discuss it; TRUE makes more sense in certain
			   ways, and it's what EDG has always done. */

EXTERN a_boolean
		one_instantiation_per_object;
			/* TRUE if each externally linked function and static
			   data member should be generated in its own object
			   file. */

#if ONE_INSTANTIATION_PER_OBJECT
EXTERN char	*instantiation_dir_name;
			/* The name of the directory in which the instantiation
			   files should be created when one instantiation is
			   being put into each file. */
#endif /* ONE_INSTANTIATION_PER_OBJECT */

EXTERN a_boolean stdc_zero_in_nonstrict_mode;
			/* TRUE if __STDC__ should be defined to 0
			   in nonstrict mode and 1 in strict mode.
			   This flag affects both ANSI C and C++ mode
			   and overrides most other factors that affect the
			   setting of __STDC__.  For example, __STDC__
			   will be defined even in Microsoft mode.  The
			   special handling for stdc_zero_in_system_headers
			   is still done, however. */

EXTERN a_boolean
		stdc_zero_in_system_headers;
			/* TRUE if __STDC__ should be 0 while
			   processing system headers and 1 otherwise. */

EXTERN unsigned long
		max_pending_instantiations;
			/* The maximum number of pending instantiations
			   of a given template that may be in process
			   at a given time.  This is used to detect
			   runaway recursive instantiations. */

#if MICROSOFT_EXTENSIONS_ALLOWED
EXTERN char	*import_dir_name;
			/* The name of the directory in which files should be
			   sought for the Microsoft #import directive. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
EXTERN a_boolean
		create_microsoft_if_exists_entries;
			/* TRUE if IL entries should be created for Microsoft
			   __if_exist directives in certain contexts. */
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */

EXTERN a_boolean
		enum_types_can_be_larger_than_int;
			/* TRUE when an enumerator type can be based on an
			   integer type that is larger than an int.  Always
			   FALSE in C mode; usually TRUE in C++ mode. */

EXTERN a_boolean
		enum_types_can_be_smaller_than_int;
			/* TRUE when an enumerator type can be based on an
			   integer type that is smaller than an int.  Always
			   FALSE if targ_enum_types_can_be_smaller_than_int
			   (an ABI requirement) is FALSE. */

#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
EXTERN a_boolean
	       instantiations_permitted_in_class_src_seq_list;
			/* Flag that indicates whether a source sequence
			   entry representing a template instantiation is
			   permitted within the portion of the source
			   sequence list representing a class definition. */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

#if NEED_NAME_MANGLING && !IA64_ABI
EXTERN a_boolean
		compress_mangled_names;
			/* Indicates whether mangled names should be compressed
			   to reduce their size. */
#endif /* NEED_NAME_MANGLING && !IA64_ABI */

#if NEED_NAME_MANGLING
EXTERN sizeof_t
		max_mangled_name_length;
			/* Maximum allowed length for a mangled name.
			   Zero means no limit. */

EXTERN a_boolean
		final_name_mangling_needed;
			/* Indicates whether the final name mangling pass is
			   needed; it's needed if compression or truncation
			   are enabled. */
#endif /* NEED_NAME_MANGLING */

EXTERN char
		*include_file_suffixes;
			/* The file suffixes to be used when searching for an
			   include file name specified with no suffix.  This
			   is a colon-separated list of suffixes (but without
			   the "." delimiter). */

EXTERN char
		*curr_cmd_line_or_predef_macro_def;
			/* Non-NULL if and only if we are processing a
			   command-line macro definition option of the form
			   -D<def> or a macro definition from the
			   predefined macros file.  In those cases it
			   points to the null-terminated byte string <def>
			   or the portion of the line from the predefined
			   macro file beginning with the macro name. */

EXTERN a_boolean
		processing_predefined_macro;
			/* TRUE if curr_cmd_line_or_predef_macro points to
			   a line from the predefined macro file, FALSE
			   otherwise. */

EXTERN a_boolean
		ignore_std_namespace;
			/* TRUE when the "std" namespace is treated as a
			   synonym for the global namespace.  This is a
			   g++ compatibility feature. */

EXTERN a_boolean
		end_of_line_comments_allowed;
			/* TRUE if "//" is accepted as a comment delimiter
			   (e.g., in C++, C99, and microsoft modes).  See
			   also END_OF_LINE_COMMENTS_ALLOWED_IN_C_MODE. */

EXTERN a_boolean
		flexible_array_members_allowed;
			/* TRUE if the final field of a struct may be an
			   incomplete array type.  This is part of the C99
			   standard and is permitted as an extension in C
			   mode.  It is also permitted in Microsoft mode
			   (both C and C++). */

EXTERN a_boolean
		universal_character_names_allowed;
			/* TRUE if universal character names should be
			   accepted.  Permitted in C++ and C99 modes. */

EXTERN a_boolean
		va_copy_macro_allowed;
			/* TRUE if the va_copy macro should be accepted.
			   It is permitted in C99 mode.  This is only
			   meaningful when passing stdarg references in
			   the generated code. */

EXTERN a_boolean
		long_long_is_standard;
			/* TRUE if the long long type is should be considered
			   a standard data type (i.e., not an extension).
			   This is true in C99 mode. */

EXTERN a_boolean
		long_long_promotion_allowed;
			/* TRUE if a constant that is larger than a signed long
			   should have type long long instead of type
			   unsigned long.  This is usually FALSE except in
			   modes where long long is fully standard (e.g.,
			   C99 mode). */

#if INT128_EXTENSIONS_ALLOWED
EXTERN a_boolean
		int128_extensions_enabled;
			/* TRUE if the front end supports 128-bit integer
			   types.  Currently only meaningful in GNU modes. */
#endif /* INT128_EXTENSIONS_ALLOWED */

EXTERN a_boolean
		hex_floating_point_constants_allowed;
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
		allow_anon_types_in_anon_unions;
			/* TRUE if no diagnostic should be issued on
			   anonymous types declared in anonymous unions. */

EXTERN a_boolean
		use_nonstd_partial_ordering;
			/* TRUE if the incorrect variant of partial ordering
			   present in versions through 3.10 should be used. */

EXTERN a_boolean
		no_checking_pragmas;
			/* TRUE if, when using EXPENSIVE_CHECKING, the
			   generation of checking pragmas should be
			   suppressed. */

#if DEBUG
EXTERN a_boolean
		display_space_used;
			/* TRUE if the space used information should be
			   shown at the end of compilation. */
#endif /* DEBUG */

#if IA64_ABI
EXTERN a_boolean
		emulate_gnu_abi_bugs;
			/* TRUE if the IA-64 ABI implementation should be
			   modified to emulate early GNU implementations of
			   that ABI. */

EXTERN a_boolean
		emulate_unsafe_gnu_abi_bugs;
			/* TRUE if the IA-64 ABI implementation should
			   emulate potentially dangerous GNU implementation
			   bugs.  If TRUE, emulate_gnu_abi_bugs must also be
			   TRUE. */

EXTERN unsigned long
		gnu_abi_version;
			/* The version of GNU C++ whose ABI is to be
			   emulated.  This value must be at least 30200
			   (i.e., g++ version 3.2). */

EXTERN a_boolean
		warn_about_tail_padding_use;
			/* TRUE if a warning should be emitted when a field
			   of a derived class is placed in the tail padding
			   of its base class. */

EXTERN a_boolean
		targ_reuse_tail_padding;
			/* TRUE if the IA-64 ABI can reuse tail-padding from
			   base classes for other subobjects of the derived
			   class. */
#endif /* IA64_ABI */

EXTERN a_boolean
		packing_applies_to_base_classes;
			/* TRUE if "#pragma pack(n)" affects the alignment of
			   base classes (in addition to that of proper
			   fields). */

EXTERN a_boolean
		IEEE_handling_on_float_operation_exceptions;
			/* TRUE if exceptions in compile-time floating-point
			   conversions and operation folding (e.g., division
			   by zero) should be handled according to the IEEE
			   floating-point standard, i.e., they generate NaNs
			   and Infinities and no errors are issued. */


#if UPC_EXTENSIONS_ALLOWED

EXTERN a_boolean
		upc_mode;
			/* TRUE if UPC extensions are to be accepted. */

EXTERN a_host_large_integer
		upc_num_threads;
			/* Indicates the compile-time number of threads.
			   If zero, indicates the number is determined at
			   run time. */

#endif /* UPC_EXTENSIONS_ALLOWED */

#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
EXTERN a_boolean
		gnu_init_priority_attribute_enabled;
			/* TRUE if the GNU "init_priority" attribute should be
			   accepted. */
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */

#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
EXTERN a_boolean
		gnu_visibility_attribute_enabled;
			/* TRUE if the GNU "visibility" attribute should be
			   accepted. */
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */

#if GNU_VECTOR_TYPES_ALLOWED
EXTERN a_boolean
		permissive_gnu_vector_conversions_enabled;
			/* TRUE if certain implicit conversions between
			   incompatible vector types should be accepted. */
#endif /* GNU_VECTOR_TYPES_ALLOWED */

EXTERN a_boolean
		allow_default_arg_on_template_member_definition;
			/* TRUE if a default argument can be specified in the
			   out-of-class definition of a member function of a
			   class template. */

EXTERN a_boolean
		use_microsoft_specialization_scope;
			/* TRUE if a template instantiation scope should be
			   pushed before the class definition scope for
			   a specialized template class and for class
			   reactivation scopes of other template classes.
			   This is also used in Sun mode. */

EXTERN a_boolean
		elab_type_lookup_finds_typedefs;
			/* TRUE if the lookup done in an elaborated type
			   specifier should find typedef names.  In general,
			   this is TRUE in C++ and not in C, but it is FALSE
			   in some C++ modes. */

EXTERN a_boolean
		value_initialization_enabled;
			/* TRUE if value-initialization should be done.
			   Value-initialization was added after the C++98
			   standard and some compilers don't do it. */

EXTERN a_boolean
		emulate_msvc_value_initialization_bugs;
			/* TRUE if bugs in MSVC++ regarding
			   value-initialization should be emulated.  This
			   is desirable in products that are trying to
			   detect uninitialized values, but not in general. */
EXTERN a_boolean
		emulate_gnu_value_initialization_bugs;
			/* TRUE if bugs in g++ regarding
			   value-initialization should be emulated.  This
			   is desirable in products that are trying to
			   detect uninitialized values, but not in general. */

EXTERN a_boolean
		thread_local_storage_specifier_enabled;
			/* TRUE if the "__thread" specifier should be accepted
			   to indicate that a variable should reside in thread-
			   local storage. */

EXTERN a_boolean
		allow_nonconstant_auto_aggr_init_in_c_mode;
			/* TRUE if aggregate initializers for C mode automatic
			   variables can contain nonconstant expressions.
			   This is set to TRUE in C99, GNU C, and Microsoft C
			   modes. */

EXTERN a_boolean
		extern_template_allowed;
			/* TRUE if "extern template" can be used to suppress
			   the instantiation of entities. */

EXTERN a_boolean
		inline_template_allowed;
			/* TRUE if "inline template" can be used to emulate
			   the g++ feature used to instantiate the vtable of a
			   class. */

EXTERN a_boolean
		standard_form_of_extern_template;
			/* TRUE if the "extern template" feature should have
			   the semantics specified by the C++ standard (as
			   opposed to the semantics used by Microsoft or
		           GNU). */

EXTERN a_boolean
		check_concatenations;
			/* TRUE if macro concatenation ("a ## b") should
			   cause a diagnostic if it results in an invalid
			   token. */

EXTERN a_boolean
		equiv_typedefs_are_lookup_equivalent;
			/* TRUE if two typedefs from different namespaces that
			   refer to the same type should be considered
			   equivalent for hidden name processing lookups. */

EXTERN a_boolean
		carriage_return_is_line_terminator;
			/* TRUE if a carriage return or a carriage return
			   followed by a newline is to be treated as a line
			   terminator.  FALSE indicates that only newlines
			   are to be considered to terminate a line. */

#if USE_FIXED_ADDRESS_FOR_MMAP
EXTERN char	*fixed_address_for_mmap;
			/* The fixed address to use for mmap calls;
			   initially set to FIXED_ADDRESS_FOR_MMAP and may be
			   modified by command line option. */
#endif /* USE_FIXED_ADDRESS_FOR_MMAP */

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
                                  a_def_undef_string_ptr *du_list_end,
                                  a_boolean              is_undef);

#if MAKE_FRONT_END_CALLABLE
extern void cmd_line_cleanup(void);
#endif /* MAKE_FRONT_END_CALLABLE */

#endif /* ifndef CMD_LINE_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2012 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
