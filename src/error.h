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

error.h -- Declarations related to error reporting.

*/

/* Avoid including these declarations more than once: */
#ifndef ERROR_H
#define ERROR_H 1


/*
Internal coding used for error severities.
*/
typedef enum /*an_error_severity*/ {
  es_none,
  es_remark,
  es_warning,
  es_error,
  es_catastrophe,
  es_command_line_error,
  es_internal_error
} an_error_severity;

/* This is included after the definition of an_error_severity because
   host_envir.h needs it. */
#ifndef HOST_ENVIR_H
#include "host_envir.h"
#endif /* ifndef HOST_ENVIR_H */

/*
Symbolic codes for errors and other diagnostics.
*/
typedef enum /*an_error_code*/ {
  ec_no_error,       /* To take value 0, so other codes will be non-zero. */
  ec_last_line_incomplete,
  ec_last_line_backslash,
  ec_include_recursion,
  ec_out_of_memory,
  ec_source_file_could_not_be_opened,
  ec_comment_unclosed_at_eof,
  ec_bad_token,
  ec_unclosed_string,
  ec_nested_comment,
  ec_bad_use_of_sharp,
  ec_bad_pp_directive_keyword,
  ec_end_of_flush,
  ec_exp_file_name,
  ec_extra_text_in_pp_directive,
  ec_source_file_has_bad_format,
  ec_illegal_source_file_name,
  ec_exp_rbracket,
  ec_exp_rparen,
  ec_extra_chars_on_number,
  ec_undefined_identifier,
  ec_useless_type_qualifiers,
  ec_bad_hex_digit,
  ec_integer_too_large,
  ec_bad_octal_digit,
  ec_zero_length_string,
  ec_too_many_characters,
  ec_bad_character_value,
  ec_expr_not_constant,
  ec_exp_primary_expr,
  ec_bad_float_value,
  ec_expr_not_integral,
  ec_expr_not_arithmetic,
  ec_exp_line_number,
  ec_bad_line_number,
  ec_error_directive,
  ec_missing_pp_if,
  ec_missing_endif,
  ec_pp_else_already_appeared,
  ec_divide_by_zero,
  ec_exp_identifier,
  ec_expr_not_scalar,
  ec_incompatible_operands,
  ec_expr_not_integral_or_pointer,
  ec_expr_not_pointer,
  ec_cannot_undef_predef_macro,
  ec_cannot_redef_predef_macro,
  ec_bad_macro_redef,
  ec_mixed_function_object_pointers,
  ec_duplicate_macro_param_name,
  ec_paste_cannot_be_first,
  ec_paste_cannot_be_last,
  ec_exp_macro_param,
  ec_exp_colon,
  ec_too_few_macro_args,
  ec_too_many_macro_args,
  ec_sizeof_function,
  ec_bad_constant_operator,
  ec_bad_pp_operator,
  ec_bad_constant_function_call,
  ec_bad_sharp_in_object_like_macro,
  ec_bad_integral_operator,
  ec_integer_overflow,
  ec_negative_shift_count,
  ec_shift_count_too_large,
  ec_useless_decl,
  ec_exp_semicolon,
  ec_enum_value_out_of_int_range,
  ec_exp_rbrace,
  ec_integer_sign_change,
  ec_integer_truncated,
  ec_incomplete_type_not_allowed,
  ec_sizeof_bit_field,
  ec_address_of_constant,
  ec_init_constant_address_of_non_static,
  ec_bad_address_of_operand,
  ec_bad_indirection_operand,
  ec_empty_macro_argument,
  ec_missing_decl_specifiers,
  ec_initializer_in_param,
  ec_exp_type_specifier,
  ec_storage_class_not_allowed,
  ec_mult_storage_classes,
  ec_storage_class_not_first,
  ec_dupl_type_qualifier,
  ec_bad_combination_of_type_specifiers,
  ec_bad_param_storage_class,
  ec_bad_function_storage_class,
  ec_type_specifier_not_allowed,
  ec_array_of_function,
  ec_array_of_void,
  ec_function_returning_function,
  ec_function_returning_array,
  ec_param_id_list_needs_function_def,
  ec_function_type_must_come_from_declarator,
  ec_array_size_must_be_positive,
  ec_array_size_too_large,
  ec_empty_translation_unit,
  ec_bad_function_return_type,
  ec_bad_array_element_type,
  ec_decl_should_be_of_param,
  ec_dupl_param_name,
  ec_id_already_declared,
  ec_nonstd_forward_def_enum,
  ec_struct_too_large,
  ec_bad_bit_field_size,
  ec_bad_bit_field_type,
  ec_zero_length_bit_field_must_be_unnamed,
  ec_signed_one_bit_field,
  ec_expr_not_ptr_to_function,
  ec_exp_definition_of_tag,
  ec_code_is_unreachable,
  ec_exp_while,
  ec_label_already_defined,
  ec_label_never_defined,
  ec_continue_must_be_in_loop,
  ec_break_must_be_in_loop_or_switch,
  ec_no_value_returned_in_non_void_function,
  ec_value_returned_in_void_function,
  ec_cast_not_scalar_or_void,
  ec_bad_return_value_type,
  ec_case_label_must_be_in_switch,
  ec_default_label_must_be_in_switch,
  ec_case_label_appears_more_than_once,
  ec_default_label_appears_more_than_once,
  ec_exp_lparen,
  ec_expr_not_an_lvalue,
  ec_exp_statement,
  ec_loop_not_reachable,
  ec_block_scope_function_must_be_extern,
  ec_exp_lbrace,
  ec_expr_not_ptr_to_struct_or_union,
  ec_exp_field_name,
  ec_not_a_member,
  ec_expr_not_a_modifiable_lvalue,
  ec_address_of_register_variable,
  ec_address_of_bit_field,
  ec_too_many_arguments,
  ec_all_proto_params_must_be_named,
  ec_expr_not_pointer_to_object,
  ec_too_many_memory_regions,
  ec_bad_initializer_type,
  ec_cannot_initialize,
  ec_too_many_initializer_values,
  ec_type_must_be_compat_with_prev_def,
  ec_already_initialized,
  ec_bad_file_scope_storage_class,
  ec_typedef_cannot_be_param_name,
  ec_non_zero_int_conv_to_pointer,
  ec_expr_not_struct_or_union,
  ec_old_fashioned_assignment_operator,
  ec_old_fashioned_initializer,
  ec_expr_not_integral_constant,
  ec_expr_not_an_lvalue_or_function_designator,
  ec_decl_incompatible_with_previous_use,
  ec_external_name_clash,
  ec_routine_definition_missing,
  ec_unrecognized_pragma,
  ec_expr_not_scalar_or_void,
  ec_cannot_open_temp_file,
  ec_temp_file_dir_name_too_long,
  ec_too_few_arguments,
  ec_bad_float_constant,
  ec_incompatible_param,
  ec_function_type_not_allowed,
  ec_exp_declaration,
  ec_pointer_outside_base_object,
  ec_bad_pointer_cast,
  ec_linkage_conflict,
  ec_float_to_integer_conversion,
  ec_expr_has_no_effect,
  ec_subscript_out_of_range,
  ec_constant_string_subscript_out_of_range,
  ec_variable_declared_but_not_referenced,
  ec_routine_declared_but_not_referenced,
  ec_label_declared_but_not_referenced,
  ec_pcc_address_of_array,
  ec_mod_by_zero,
  ec_old_style_incompatible_param,
  ec_parameter_declared_but_not_referenced,
  ec_printf_arg_mismatch,
  ec_empty_include_search_path,
  ec_cast_not_integral,
  ec_cast_not_scalar,
  ec_initialization_not_reachable,
  ec_unsigned_compare_with_zero,
  ec_assign_where_compare_meant,
  ec_mixed_enum_type,
  ec_file_write_error,
  ec_bad_il_file,
  ec_cast_to_qualified_type,
  ec_unrecognized_char_escape,
  ec_undefined_preproc_id,
#if ASM_STATEMENT_ALLOWED
  ec_exp_asm_string,
#endif /* ASM_STATEMENT_ALLOWED */
#if ASM_FUNCTION_ALLOWED
  ec_asm_func_must_be_prototyped,
  ec_bad_asm_func_ellipsis,
  ec_asm_with_non_function,
  ec_asm_func_has_storage_class,
  ec_bad_asm_func_return_size,
  ec_bad_asm_func_param_size,
  ec_exp_percent,
  ec_asm_specifier_conflicts_with_prev,
  ec_extra_text_on_asm_control_line,
  ec_exp_asm_control_specifier,
  ec_asm_name_already_defined,
  ec_bad_asm_reg_name,
  ec_asm_param_may_not_be_void,
  ec_exp_asm_type,
  ec_bad_asm_type_specification,
  ec_bad_asm_type_width,
  ec_bad_asm_constant,
  ec_bad_asm_temp_type,
  ec_cannot_ref_untyped_asm_param,
  ec_cannot_ref_void_asm_return,
  ec_bad_asm_reg_spec,
  ec_asm_leaf_has_no_expansion_lines,
  ec_cannot_ref_untyped_asm_return,
  ec_bad_asm_return_type,
#endif /* ASM_FUNCTION_ALLOWED */
  ec_file_delete_error,
  ec_integer_to_float_conversion,
  ec_float_to_float_conversion,
  ec_bad_float_operation_result,
  ec_implicit_func_decl,
  ec_too_few_printf_args,
  ec_too_many_printf_args,
  ec_bad_printf_format_string,
  ec_macro_recursion,
  ec_nonstd_extra_comma,
  ec_enum_bit_field_too_small,
  ec_nonstd_bit_field_type,
  ec_decl_in_prototype_scope,
  ec_decl_of_void_ignored,
  ec_old_fashioned_field_selection,
  ec_old_fashioned_ptr_field_selection,
  ec_var_retained_incomp_type,
  ec_boolean_controlling_expr_is_constant,
  ec_switch_selector_expr_is_constant,
  ec_address_of_void,
  ec_bad_param_specifier,
  ec_bad_specifier_outside_class_decl,
  ec_dupl_decl_specifier,
  ec_base_class_not_allowed_for_union,
  ec_access_already_specified,
  ec_missing_class_definition,
  ec_name_not_member_of_class_or_base_classes,
  ec_member_ref_requires_object,
  ec_nonstatic_member_def_not_allowed,
  ec_redefinition_not_allowed,
  ec_static_member_in_local_class,
  ec_pointer_to_reference,
  ec_reference_to_reference,
  ec_reference_to_void,
  ec_array_of_reference,
  ec_missing_initializer_on_reference,
  ec_exp_comma,
  ec_type_identifier_not_allowed,
  ec_type_definition_not_allowed,
  ec_bad_type_name_redeclaration,
  ec_missing_initializer_on_const,
  ec_this_used_incorrectly,
  ec_constant_value_not_known,
  ec_missing_type_specifier,
  ec_missing_access_specifier,
  ec_not_a_class_or_struct_name,
  ec_dupl_base_class_name,
  ec_base_class_undefined
  /* +++ -- For ease of finding the insert point for new diagnostics. */
} an_error_code;

/*
Current error position, used as default in error reporting.  Set
implicitly to the start of a construct whenever one is scanned (e.g.,
when a token is gotten, error_position is set to the start of the
token).
*/
EXTERN a_source_position
		error_position;

/*
Count of remarks, warnings, errors, and catastrophic errors detected so far.
*/
EXTERN unsigned long
		total_remarks,
		total_warnings,
		total_errors,
		total_catastrophes;

EXTERN an_error_severity
		error_threshold
#if VAR_INITIALIZERS
                                = es_warning
#endif /* VAR_INITIALIZERS */
                                            ;
			/* Messages at or above this severity level should
			   be displayed; those below are suppressed. */
EXTERN unsigned long
		error_limit
#if VAR_INITIALIZERS
			    = 100
#endif /* VAR_INITIALIZERS */
				 ;
			/* Compilation is abandoned when this many errors
			   are detected. */
/*
Error routines.
*/
#if CHECKING
extern void internal_error(char *error_message);
#endif /* CHECKING */
extern void command_line_error(char *error_message);
extern void str_command_line_error(char *error_message,
                                   char *fill_in_string);
extern void pos_st_remark(an_error_code     error_code,
                          a_source_position *error_pos,
                          char              *error_string);
extern void pos_remark(an_error_code     error_code,
                       a_source_position *error_pos);
extern void remark(an_error_code error_code);
extern void pos_st_warning(an_error_code     error_code,
                           a_source_position *error_pos,
                           char              *error_string);
extern void pos_warning(an_error_code     error_code,
                        a_source_position *error_pos);
extern void str_warning(an_error_code error_code,
                        char          *error_string);
extern void warning(an_error_code error_code);
extern void pos_st_error(an_error_code     error_code,
                         a_source_position *error_pos,
                         char              *error_string);
extern void pos_error(an_error_code     error_code,
                      a_source_position *error_pos);
extern void str_error(an_error_code error_code,
                      char          *error_string);
extern void error(an_error_code error_code);
extern void pos_st_catastrophe(an_error_code     error_code,
                               a_source_position *error_pos,
                               char              *error_string);
extern void str_catastrophe(an_error_code error_code,
                            char          *error_string);
extern void catastrophe(an_error_code error_code);

/* Report a syntax error, flush to a token in the stop set. */
extern void syntax_error(an_error_code error_code);

#endif /* ifndef ERROR_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
