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

error.h -- Declarations related to error reporting.

*/

/* Avoid including these declarations more than once: */
#ifndef ERROR_H
#define ERROR_H 1
#ifndef LANG_FEAT_H
#include "lang_feat.h"
#endif /* LANG_FEAT_H */

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
Symbolic codes for errors and other diagnostics.  If an error message
is removed, its error code should be preserved and not reassigned
to a new message to facilitate the use of message catalogs.
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
  ec_class_too_large,
  ec_struct_too_large,
  ec_bad_bit_field_size,
  ec_bad_bit_field_type,
  ec_zero_length_bit_field_must_be_unnamed,
  ec_signed_one_bit_field,
  ec_expr_not_ptr_to_function,
  ec_exp_definition_of_tag,
  ec_code_is_unreachable,
  ec_exp_while,
  ec_nonstd_default_arg,
  ec_never_defined,
  ec_continue_must_be_in_loop,
  ec_break_must_be_in_loop_or_switch,
  ec_no_value_returned_in_non_void_function,
  ec_value_returned_in_void_function,
  ec_cast_to_bad_type,
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
  ec_expr_not_ptr_to_class,
  ec_expr_not_ptr_to_struct_or_union,
  ec_exp_member_name,
  ec_exp_field_name,
  ec_not_a_member,
  ec_not_a_field,
  ec_expr_not_a_modifiable_lvalue,
  ec_address_of_register_variable,
  ec_address_of_bit_field,
  ec_too_many_arguments,
  ec_all_proto_params_must_be_named,
  ec_expr_not_pointer_to_object,
  ec_program_too_large,
  ec_bad_initializer_type,
  ec_cannot_initialize,
  ec_too_many_initializer_values,
  ec_not_compatible_with_previous_decl,
  ec_already_initialized,
  ec_bad_file_scope_storage_class,
  ec_type_cannot_be_param_name,
  ec_typedef_cannot_be_param_name,
  ec_non_zero_int_conv_to_pointer,
  ec_expr_not_class,
  ec_expr_not_struct_or_union,
  ec_old_fashioned_assignment_operator,
  ec_old_fashioned_initializer,
  ec_expr_not_integral_constant,
  ec_expr_not_an_lvalue_or_function_designator,
  ec_decl_incompatible_with_previous_use,
  ec_external_name_clash,
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
  ec_bad_cast,
  ec_linkage_conflict,
  ec_float_to_integer_conversion,
  ec_expr_has_no_effect,
  ec_subscript_out_of_range,
  ec_constant_string_subscript_out_of_range,
  ec_declared_but_not_referenced,
  ec_pcc_address_of_array,
  ec_mod_by_zero,
  ec_old_style_incompatible_param,
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
  ec_exp_asm_string,
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
  ec_bad_param_specifier,
  ec_bad_specifier_outside_class_decl,
  ec_dupl_decl_specifier,
  ec_base_class_not_allowed_for_union,
  ec_access_already_specified,
  ec_missing_class_definition,
  ec_name_not_member_of_class_or_base_classes,
  ec_member_ref_requires_object,
  ec_nonstatic_member_def_not_allowed,
  ec_already_defined,
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
  ec_bad_base_class,
  ec_no_access_to_name,
  ec_ambiguous_name,
  ec_old_style_parameter_list,
  ec_declaration_after_statements,
  ec_inaccessible_base_class,
  ec_not_a_base_class_member,
  ec_access_adjustment_in_private_section,
  ec_increasing_access_not_allowed,
  ec_restricting_access_not_allowed,
  ec_improperly_terminated_macro_call,
  ec_bad_access_decl_name_is_hidden,
  ec_id_must_be_class_name,
  ec_bad_friend_decl,
  ec_value_returned_in_constructor,
  ec_bad_destructor_decl,
  ec_class_and_member_name_conflict,
  ec_global_qualifier_not_allowed,
  ec_name_not_found_in_file_scope,
  ec_qualified_name_not_allowed,
  ec_null_reference,
  ec_brace_initialization_not_allowed,
  ec_ambiguous_base_class,
  ec_ambiguous_derived_class,
  ec_derived_class_from_virtual_base,
  ec_no_matching_constructor,
  ec_ambiguous_copy_constructor,
  ec_no_default_constructor,
  ec_not_a_field_or_base_class,
  ec_indirect_nonvirtual_base_class_not_allowed,
  ec_bad_union_field,
  ec_overloaded_function_types_too_similar,
  ec_bad_rvalue_array,
  ec_exp_operator,
  ec_inherited_member_not_allowed,
  ec_indeterminate_overloaded_function,
  ec_bound_function_must_be_called,
  ec_duplicate_typedef,
  ec_function_redefinition,
  ec_overloaded_function_incompatible_type,
  ec_no_matching_function,
  ec_type_def_not_allowed_in_func_type_decl,
  ec_default_arg_not_at_end,
  ec_default_arg_already_defined,
  ec_ambiguous_overloaded_function,
  ec_ambiguous_constructor,
  ec_bad_default_arg_type,
  ec_return_type_cannot_distinguish_functions,
  ec_no_user_defined_conversion,
  ec_function_qualifier_not_allowed,
  ec_virtual_static_not_allowed,
  ec_unqual_function_with_qual_object,
  ec_too_many_virtual_functions,
  ec_bad_return_type_on_virtual_function_override,
  ec_ambiguous_virtual_function_override,
  ec_pure_specifier_on_nonvirtual_function,
  ec_bad_pure_specifier,
  ec_bad_data_member_initialization,
  ec_abstract_class_object_not_allowed,
  ec_function_returning_abstract_class,
  ec_duplicate_friend_decl,
  ec_inline_and_nonfunction,
  ec_inline_not_allowed,
  ec_bad_storage_class_with_inline,
  ec_bad_member_storage_class,
  ec_local_class_function_def_missing,
  ec_inaccessible_special_function,
  ec_direct_derivation_less_accessible,
  ec_missing_const_copy_constructor,
  ec_definition_of_implicitly_declared_function,
  ec_no_suitable_copy_constructor,
  ec_linkage_specifier_not_allowed,
  ec_bad_linkage_specifier,
  ec_incompatible_linkage_specifier,
  ec_overloaded_function_linkage,
  ec_ambiguous_default_constructor,
  ec_temp_used_for_ref_init,
  ec_nonmember_operator_not_allowed,
  ec_static_member_operator_not_allowed,
  ec_too_many_args_for_conversion,
  ec_too_many_args_for_operator,
  ec_too_few_args_for_operator,
  ec_no_args_with_class_type,
  ec_default_arg_expr_not_allowed,
  ec_ambiguous_user_defined_conversion,
  ec_no_matching_operator_function,
  ec_ambiguous_operator_function,
  ec_bad_arg_type_for_operator_new,
  ec_bad_return_type_for_op_new,
  ec_bad_return_type_for_op_delete,
  ec_bad_first_arg_type_for_operator_delete,
  ec_bad_second_arg_type_for_operator_delete,
  ec_type_must_be_object_type,
  ec_base_class_already_initialized,
  ec_base_class_init_anachronism,
  ec_member_already_initialized,
  ec_missing_base_class_or_member_name,
  ec_assignment_to_this,
  ec_overload_anachronism,
  ec_anon_union_member_access,
  ec_anon_union_member_function,
  ec_anon_union_storage_class,
  ec_missing_initializer_on_fields,
  ec_cannot_initialize_fields,
  ec_no_ctor_but_const_or_ref_member,
  ec_var_with_uninitialized_member,
  ec_var_with_uninitialized_field,
  ec_missing_const_assignment_operator,
  ec_no_suitable_assignment_operator,
  ec_ambiguous_assignment_operator,
  ec_const_volatile_not_allowed,
  ec_missing_typedef_name,
  ec_missing_object_name,
  ec_virtual_not_allowed,
  ec_static_not_allowed,
  ec_bound_function_cast_anachronism,
  ec_expr_not_ptr_to_member,
  ec_extra_semicolon,
  ec_nonstd_const_member,
  ec_delete_of_const_pointer,
  ec_no_matching_new_function,
  ec_delete_already_declared,
  ec_no_match_for_addr_of_overloaded_function,
  ec_delete_count_anachronism,
  ec_bad_return_type_for_op_arrow,
  ec_cast_to_abstract_class,
  ec_bad_use_of_main,
  ec_initializer_not_allowed_on_array_new,
  ec_member_function_redecl_outside_class,
  ec_ptr_to_incomplete_class_type_not_allowed,
  ec_ref_to_nested_function_var,
  ec_single_arg_postfix_incr_decr_anachronism,
  ec_bad_access_adjustment_with_overloading,
  ec_bad_default_assignment,
  ec_nonstd_array_cast,
  ec_class_with_op_new_but_no_op_delete,
  ec_class_with_op_delete_but_no_op_new,
  ec_base_class_with_nonvirtual_dtor,
  ec_no_access_to_constructors,
  ec_member_function_redeclaration,
  ec_inline_main,
  ec_class_and_member_function_name_conflict,
  ec_nested_class_anachronism,
  ec_too_many_params_for_destructor,
  ec_bad_constructor_param,
  ec_incomplete_function_return_type,
  ec_protected_access_problem,
  ec_param_not_allowed,
  ec_asm_not_allowed,
  ec_no_conversion_function,
  ec_delete_of_incomplete_class,
  ec_no_constructor_for_conversion,
  ec_ambiguous_constructor_for_conversion,
  ec_ambiguous_conversion_function,
  ec_ambiguous_conversion_to_builtin,
  ec_const_member,
  ec_reference_member,
  ec_ambiguous_function_add_on,
  ec_builtin_operator_add_on,
  ec_ambiguous_by_inheritance_add_on,
  ec_addr_of_constructor_or_destructor,
  ec_dollar_used_in_identifier,
  ec_nonconst_ref_init_anachronism,
  ec_qualifier_in_member_declaration,
  ec_mixed_enum_type_anachronism,
  ec_new_array_size_must_be_nonnegative,
  ec_return_ref_init_requires_temp,
  ec_cfront_nonconst_ref_init,
  ec_enum_not_allowed,
  ec_qualifier_dropped_in_ref_init,
  ec_bad_nonconst_ref_init,
  ec_delete_of_function_pointer,
  ec_bad_conversion_function_decl,
  ec_nonglobal_template_declaration,
  ec_exp_lt,
  ec_exp_gt,
  ec_missing_template_param,
  ec_missing_template_arg_list,
  ec_too_few_template_args,
  ec_too_many_template_args,
  ec_not_a_type_arg,
  ec_not_used_in_template_function_params,
  ec_cfront_multiple_nested_types,
  ec_cfront_global_defined_after_nested_type,
  ec_template_param_declared_but_not_referenced,
  ec_ambiguous_ptr_to_overloaded_function,
  ec_nonstd_long_long,
  ec_nonstd_friend_decl,
  ec_return_type_on_conversion_function,
  ec_template_detected_during_header,
  ec_template_instantiation_context,
  ec_compiler_generated_function_context,
  ec_runaway_recursive_instantiation,
  ec_bad_template_declaration,
  ec_bad_nontype_template_arg,
  ec_init_needing_temp_not_allowed,
  ec_decl_hides_function_parameter,
  ec_nonconst_ref_init_from_rvalue,
  ec_implicit_static_data_member_definition,
  ec_template_not_allowed,
  ec_not_a_class_template,
  ec_static_data_member_anon_union,
  ec_function_template_named_main,
  ec_union_nonunion_mismatch,
  ec_local_type_in_template_arg,
  ec_tag_kind_incompatible_with_declaration,
  ec_name_not_tag_in_file_scope,
  ec_not_a_tag_member,
  ec_ptr_to_member_typedef,
  ec_bad_use_of_ptr_to_member_typedef,
  ec_empty_initializer_list,                                    /* removed */
  ec_nonexternal_entity_in_template_arg,
  ec_id_must_be_class_or_type_name,
  ec_destructor_name_mismatch,
  ec_destructor_type_mismatch,
  ec_called_function_redeclared_inline,
  ec_vacuous_destructor_name_mismatch,
  ec_bad_storage_class_on_template_decl,
  ec_no_access_to_type_cfront_mode,
  ec_return_type_not_allowed,
  ec_invalid_instantiation_pragma_argument,
  ec_not_instantiatable_entity,
  ec_compiler_generated_function_cannot_be_instantiated,
  ec_inline_function_cannot_be_instantiated,
  ec_pure_virtual_function_cannot_be_instantiated,
  ec_instantiation_requested_no_definition_supplied,
  ec_instantiation_requested_and_specific_definition,
  ec_no_constructor,
  ec_template_param_only_used_in_default_args,
  ec_no_match_for_type_of_overloaded_function,
  ec_nonstd_void_param_list,
  ec_cfront_name_lookup_bug,
  ec_redeclaration_of_template_param_name,
  ec_decl_hides_template_parameter,
  ec_must_be_prototype_instantiation,
  ec_conversion_to_type_not_allowed,
  ec_bad_extra_arg_for_postfix_operator,
  ec_function_type_required,
  ec_operator_name_not_allowed,
  ec_specific_def_must_be_global,
  ec_nonstd_member_function_address,
  ec_too_few_template_params,
  ec_too_many_template_params,
  ec_template_operator_delete,
  ec_class_template_same_name_as_templ_param,
  ec_bad_constructor_name,
  ec_unnamed_type_in_template_arg,
  ec_enum_type_not_allowed,
  ec_qualified_reference_type,
  ec_incompatible_assignment_operands,
  ec_unsigned_compare_with_negative,
  ec_converting_to_incomplete_class,
  ec_missing_initializer_on_unnamed_const,
  ec_unnamed_object_with_uninitialized_field,
  ec_nonstd_pp_directive,
  ec_unexpected_template_arg_list,
  ec_missing_initializer_list,
  ec_incompatible_ptr_to_member_selection_operands,
  ec_self_friendship,
  ec_period_used_as_qualifier,
  ec_const_function_anachronism,
  ec_dependent_stmt_is_declaration,
  ec_void_param_not_allowed,
  ec_template_function_declaration_context,
  ec_template_class_argument_list_context,
  ec_bad_templ_arg_expr_operator,
  ec_missing_handler,
  ec_missing_exception_declaration,
  ec_masked_by_default_handler,
  ec_masked_by_handler,
  ec_local_type_used_in_exception,
  ec_redundant_throw_type,
  ec_incompatible_throw_specification,
  ec_previously_empty_throw_list,
  ec_previously_omitted_throw_type,
  ec_previously_included_throw_type,
  ec_no_exception_support,
  ec_omitted_throw_specification,
  ec_cannot_create_instantiation_information_file,
  ec_non_arith_operation_in_templ_arg,
  ec_local_type_in_nonlocal_var,
  ec_local_type_in_function,
  ec_branch_past_initialization,
  ec_name_at_decl_position,
  ec_branch_into_handler,
  ec_used_before_set,
  ec_set_but_not_used,
  ec_bad_scope_for_definition,
  ec_throw_specification_not_allowed,
  ec_template_and_instance_linkage_conflict,
  ec_conversion_function_not_usable,
  ec_tag_kind_incompatible_with_template_parameter,
  ec_template_operator_new,
  ec_bad_access_decl_ambiguous_name,
  ec_bad_member_type_in_ptr_to_member,
  ec_ellipsis_on_operator_function,
  ec_unimplemented_keyword,
  ec_cl_invalid_macro_definition,
  ec_cl_invalid_macro_undefinition,
  ec_cl_invalid_preprocessor_output_file,
  ec_cl_cannot_open_preprocessor_output_file,
  ec_cl_il_file_must_be_specified,
  ec_cl_invalid_il_output_file,
  ec_cl_cannot_open_il_output_file,
  ec_cl_invalid_C_output_file,
  ec_cl_cannot_open_C_output_file,
  ec_cl_error_in_debug_option_argument,
  ec_cl_invalid_option,
  ec_cl_back_end_requires_il_file,
  ec_cl_could_not_open_il_file,
  ec_cl_invalid_number,
  ec_cl_incorrect_host_id,
  ec_cl_invalid_instantiation_mode,
  ec_cl_missing_include_directory,
  ec_cl_invalid_error_limit,
  ec_cl_invalid_raw_listing_output_file,
  ec_cl_cannot_open_raw_listing_output_file,
  ec_cl_invalid_xref_output_file,
  ec_cl_cannot_open_xref_output_file,
  ec_cl_invalid_error_output_file,
  ec_cl_cannot_open_error_output_file,
  ec_cl_vtbl_option_only_in_cplusplus,
  ec_cl_anachronism_option_only_in_cplusplus,
  ec_cl_instantiation_option_only_in_cplusplus,
  ec_cl_auto_instantiation_option_only_in_cplusplus,
  ec_cl_implicit_inclusion_option_only_in_cplusplus,
  ec_cl_exceptions_option_only_in_cplusplus,
  ec_cl_strict_ansi_incompatible_with_pcc,
  ec_cl_strict_ansi_incompatible_with_cfront,
  ec_cl_missing_source_file_name,
  ec_cl_output_file_incompatible_with_multiple_inputs,
  ec_cl_too_many_arguments,
  ec_cl_no_output_file_needed,
  ec_cl_il_display_requires_il_file_name,
  ec_void_template_parameter,
  ec_too_many_unused_instantiations,
  ec_cl_strict_ansi_incompatible_with_anachronisms,
  ec_void_throw,
  ec_cl_tim_local_conflicts_with_auto_instantiation,
  ec_abstract_class_param_type,
  ec_array_of_abstract_class,
  ec_float_template_parameter,
  ec_pragma_must_precede_declaration,
  ec_pragma_must_precede_statement,
  ec_pragma_must_precede_decl_or_stmt,
  ec_pragma_may_not_be_used_here,
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


EXTERN an_error_severity
                strict_ansi_error_severity
#if VAR_INITIALIZERS
			                   = es_warning
#endif /* VAR_INITIALIZERS */
                                                     ;
                        /* Strict ANSI mode violations are reported at this
                           error severity.  This must either be es_error
                           or es_warning. */


EXTERN an_error_severity
                anachronism_error_severity
#if VAR_INITIALIZERS
#if DEFAULT_ALLOW_ANACHRONISMS
                                           = es_warning
#else /* DEFAULT_ALLOW_ANACHRONISMS */
			                   = es_error
#endif /* DEFAULT_ALLOW_ANACHRONISMS */
#endif /* VAR_INITIALIZERS */
                                                     ;
                        /* Use of anachronisms are reported at this
                           error severity.  It is expected that this will
                           either be es_error or es_warning.  This can be
                           modified by a command line option. */



/*
Error routines.
*/
#if CHECKING
extern DOES_NOT_RETURN internal_error(char *error_message);
extern DOES_NOT_RETURN assertion_failed(char *filename,
			                int  line_number,
					char *string1,
					char *string2);

/* Macro to test an assertion and generate an internal error if
   the condition is not TRUE.  The macro expands to nothing when checking
   code is not being used. */
#define check_assertion(test)						\
  if (!(test)) {							\
    assertion_failed(__FILE__, __LINE__,				\
                     (char *)NULL, (char *)NULL);			\
  }
/* Macro that generates an assertion failed internal error.  Intended to
   be used in the else clause of an if statement or the default case of a
   switch statement that is not intended to be reached. */
#define unexpected_condition()						\
  assertion_failed(__FILE__, __LINE__, (char *)NULL, (char *)NULL)
/* Macros that are the same as above except that a string describing the
   assertion is provided. */
#define check_assertion_str(test, string)				\
  if (!(test)) assertion_failed(__FILE__, __LINE__, string, (char *)NULL);
#define unexpected_condition_str(string)  				\
  assertion_failed(__FILE__, __LINE__, string, (char *)NULL)
/* Macros that are the same as above except that two strings are provided.
   this is simply done to make it easier to use long strings as arguments. */
#define check_assertion_str2(test, string1, string2)			\
  if (!(test)) assertion_failed(__FILE__, __LINE__, string1, string2);
#define unexpected_condition_str2(string1, string2) 			\
  assertion_failed(__FILE__, __LINE__, string1, string2)
#else /* !CHECKING */
#define check_assertion(test) /* Nothing */
#define check_assertion_str(test, string) /* Nothing */
#define check_assertion_str2(test, string1, string2) /* Nothing */
#define unexpected_condition()    /* Nothing */
#define unexpected_condition_str(string)    /* Nothing */
#define unexpected_condition_str2(string1, string2)    /* Nothing */
#endif /* CHECKING */
/* Make sure "a_symbol", "a_type" and "a_source_file" are known as struct
   tags before their uses below.  Otherwise, the declarations would be in
   the prototype scopes.  The "struct" form is used instead of the typedef
   name to avoid having to include symbol_tbl.h and il_def.h in this file. */
typedef struct a_symbol a_symbol_dummy_typedef;
typedef struct a_type a_type_dummy_typedef;
typedef struct a_source_file a_source_file_dummy_typedef;


extern char *format_type_string(struct a_type *type,
                                sizeof_t      *len_ptr);
#if !STANDALONE_UTILITY_PROGRAM
extern void clear_file_index_list(void);
extern void error_init(void);
extern a_line_number initialize_file_index(struct a_source_file *src_file);
extern a_line_number update_file_index(struct a_source_file *src_file,
                                       a_line_number        physical_line,
                                       long                 file_pos);
#endif /* !STANDALONE_UTILITY_PROGRAM */
extern DOES_NOT_RETURN command_line_error(an_error_code error_code);
extern DOES_NOT_RETURN str_command_line_error(an_error_code error_code,
                                              char          *fill_in_string);
extern void pos_st_diagnostic(an_error_severity error_severity,
                              an_error_code     error_code,
                              a_source_position *error_pos,
                              char              *error_string);
extern void pos_diagnostic(an_error_severity  error_severity,
                           an_error_code      error_code,
                           a_source_position  *error_pos);
extern void diagnostic(an_error_severity  error_severity,
                       an_error_code      error_code);
extern void pos_ty_diagnostic(an_error_severity  error_severity,
                              an_error_code      error_code,
                              a_source_position  *error_pos,
                              struct a_type      *type);
extern void pos_ty2_diagnostic(an_error_severity  error_severity,
                               an_error_code      error_code,
                               a_source_position  *error_pos,
                               struct a_type      *type1,
                               struct a_type      *type2);
extern void type_diagnostic(an_error_severity  error_severity,
                            an_error_code      error_code,
                            struct a_type      *type);
extern void pos_sy_diagnostic(an_error_severity  error_severity,
                              an_error_code      error_code,
                              a_source_position  *error_pos,
                              struct a_symbol    *symbol);
extern void sym_diagnostic(an_error_severity  error_severity,
                           an_error_code      error_code,
                           struct a_symbol    *symbol);
extern void pos_st_remark(an_error_code     error_code,
                          a_source_position *error_pos,
                          char              *error_string);
extern void pos_remark(an_error_code     error_code,
                       a_source_position *error_pos);
extern void remark(an_error_code error_code);
extern void pos_ty_remark(an_error_code     error_code,
                          a_source_position *error_pos,
                          struct a_type     *type);
extern void pos_ty2_remark(an_error_code     error_code,
                           a_source_position *error_pos,
                           struct a_type     *type1,
                           struct a_type     *type2);
extern void type_remark(an_error_code error_code,
                        struct a_type *type);
#if !STANDALONE_UTILITY_PROGRAM
extern void pos_sy_remark(an_error_code     error_code,
                          a_source_position *error_pos,
                          struct a_symbol   *symbol);
extern void sym_remark(an_error_code   error_code,
                       struct a_symbol *symbol);
#endif /* !STANDALONE_UTILITY_PROGRAM */
extern void pos_st_warning(an_error_code     error_code,
                           a_source_position *error_pos,
                           char              *error_string);
extern void pos_warning(an_error_code     error_code,
                        a_source_position *error_pos);
extern void str_warning(an_error_code error_code,
                        char          *error_string);
extern void warning(an_error_code error_code);
extern void pos_ty_warning(an_error_code     error_code,
                           a_source_position *error_pos,
                           struct a_type     *type);
extern void pos_ty2_warning(an_error_code     error_code,
                            a_source_position *error_pos,
                            struct a_type     *type1,
                            struct a_type     *type2);
extern void pos_opt_ty2_warning(an_error_code     error_code,
                                a_source_position *error_pos,
                                struct a_type     *type1,
                                struct a_type     *type2);
extern void type_warning(an_error_code error_code,
                         struct a_type *type);
#if !STANDALONE_UTILITY_PROGRAM
#if 0
/* This routine is not currently used by the compiler. */
extern void pos_syty_warning(an_error_code     error_code,
                             a_source_position *error_pos,
                             struct a_symbol   *symbol,
                             struct a_type     *type);
#endif /* 0 */
extern void pos_sy_warning(an_error_code     error_code,
                           a_source_position *error_pos,
                           struct a_symbol   *symbol);
extern void sym_warning(an_error_code   error_code,
                        struct a_symbol *symbol);
#endif /* !STANDALONE_UTILITY_PROGRAM */
extern void pos_st_error(an_error_code     error_code,
                         a_source_position *error_pos,
                         char              *error_string);
extern void pos_stty_error(an_error_code     error_code,
                           a_source_position *error_pos,
                           char              *error_string,
                           struct a_type     *type);
extern void pos_error(an_error_code     error_code,
                      a_source_position *error_pos);
extern void str_error(an_error_code error_code,
                      char          *error_string);
extern void error(an_error_code error_code);
extern void pos_ty_error(an_error_code     error_code,
                         a_source_position *error_pos,
                         struct a_type     *type);
extern void pos_ty2_error(an_error_code     error_code,
                          a_source_position *error_pos,
                          struct a_type     *type1,
                          struct a_type     *type2);
extern void pos_opt_ty2_error(an_error_code     error_code,
                              a_source_position *error_pos,
                              struct a_type     *type1,
                              struct a_type     *type2);
extern void type_error(an_error_code error_code,
                       struct a_type *type);
#if !STANDALONE_UTILITY_PROGRAM
extern void pos_stsy_error(an_error_code     error_code,
                           a_source_position *error_pos,
                           char              *error_string,
                           struct a_symbol   *symbol);
extern void pos_sy_error(an_error_code     error_code,
                         a_source_position *error_pos,
                         struct a_symbol   *symbol);
extern void pos_sy2_error(an_error_code     error_code,
                          a_source_position *error_pos,
                          struct a_symbol   *symbol1,
                          struct a_symbol   *symbol2);
extern void pos_syty_error(an_error_code     error_code,
                           a_source_position *error_pos,
                           struct a_symbol   *symbol,
                           struct a_type     *type);
extern void sym_error(an_error_code   error_code,
                      struct a_symbol *symbol);
#endif /* !STANDALONE_UTILITY_PROGRAM */
extern DOES_NOT_RETURN pos_st_catastrophe(an_error_code     error_code,
                                          a_source_position *error_pos,
                                          char              *error_string);
extern DOES_NOT_RETURN str_catastrophe(an_error_code error_code,
                                       char          *error_string);
extern DOES_NOT_RETURN catastrophe(an_error_code error_code);

/* Interfaces for producing multiple message diagnostics. */
extern void pos_start_diagnostic(an_error_severity  error_severity,
                                 an_error_code      error_code,
                                 a_source_position  *error_pos);
extern void pos_start_error(an_error_code     error_code,
                            a_source_position *error_pos);
extern void pos_st_start_error(an_error_code     error_code,
                               a_source_position *error_pos,
                               char              *error_string);
extern void pos_ty_start_error(an_error_code     error_code,
                               a_source_position *error_pos,
                               struct a_type     *type);
extern void pos_ty2_start_error(an_error_code     error_code,
                                a_source_position *error_pos,
                                struct a_type     *type1,
                                struct a_type     *type2);
extern void ty_add_diag_info(an_error_code error_code,
                             struct a_type *type);
extern void str_add_diag_info(an_error_code error_code,
                              char          *error_string);
extern void add_diag_info(an_error_code error_code);
#if !STANDALONE_UTILITY_PROGRAM
extern void pos_sy_start_error(an_error_code     error_code,
                               a_source_position *error_pos,
                               struct a_symbol   *symbol);
extern void pos_stsy_start_error(an_error_code     error_code,
                                 a_source_position *error_pos,
                                 char              *error_string,
                                 struct a_symbol   *symbol);
extern void pos_sy_start_warning(an_error_code     error_code,
                                 a_source_position *error_pos,
                                 struct a_symbol   *symbol);
#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
extern void pos_sy2_warning(an_error_code     error_code,
                            a_source_position *error_pos,
                            struct a_symbol   *symbol1,
                            struct a_symbol   *symbol2);
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */
extern void sym_add_diag_info(an_error_code   error_code,
                              struct a_symbol *symbol);
#endif /* !STANDALONE_UTILITY_PROGRAM */
extern void end_error(void);

/* Report a syntax error, flush to a token in the stop set. */
extern void syntax_error(an_error_code error_code);

#endif /* ifndef ERROR_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
