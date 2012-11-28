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

lint.h -- Lint suppression directives.

Included from basic_hdrs.h in every compilation.

*/

/* Avoid including these declarations more than once. */
#ifndef LINT_H
#define LINT_H 1

/* Options for FlexeLint. */
/*lint -esym(767,fread_with_check)*/
/*lint -esym(756,a*_dummy_typedef)*/
/*lint -esym(755,size_of_type)*/
/*lint -esym(755,typeref_is_const_qualified)*/
/*lint -esym(755,typeref_is_volatile_qualified)*/
/*lint -esym(755,typeref_is_restrict_qualified)*/
/*lint -esym(755,DSI_NO_INPUT_FLAGS)*/
/*lint -esym(755,DSO_NO_OUTPUT_FLAGS)*/
/*lint -esym(756,a_simple_source_position_ptr)*/
/*lint -esym(755,set_macro_inv_record_ptr_to_index)*/
/*lint -esym(755,cpp0x_mode)*/
/* Entities not used in certain configurations: */
/*lint -esym(755,EXTERN_C)*/
/*lint -esym(750,chdir_with_check)*/
/*lint -esym(769,ec_cannot_chdir)*/
/*lint -esym(769,ec_cannot_open_pch_input_file_reason)*/
/*lint -esym(769,ec_cannot_open_temp_file_reason)*/
/*lint -esym(759,change_non_id_characters)*/
/*lint -esym(765,change_non_id_characters)*/
/*lint -esym(759,type_from_src_seq_declaration)*/
/*lint -esym(765,type_from_src_seq_declaration)*/
/*lint -esym(759,form_type_qualifier)*/
/*lint -esym(765,form_type_qualifier)*/
/*lint -esym(759,file_name_in_external_encoding)*/
/*lint -esym(765,file_name_in_external_encoding)*/
/*lint -esym(759,form_pm_constant)*/
/*lint -esym(765,form_pm_constant)*/
/*lint -esym(759,form_lvalue_address_constant)*/
/*lint -esym(765,form_lvalue_address_constant)*/
/*lint -esym(759,alloc_new_mem_block)*/
/*lint -esym(765,alloc_new_mem_block)*/
/*lint -esym(759,alloc_mem_block)*/
/*lint -esym(765,alloc_mem_block)*/
/*lint -esym(759,init_memory_region_without_initial_allocation)*/
/*lint -esym(765,init_memory_region_without_initial_allocation)*/
/*lint -esym(759,trim_memory_region)*/
/*lint -esym(765,trim_memory_region)*/
/*lint -esym(759,prep_arg_passed_via_copy_constructor)*/
/*lint -esym(765,prep_arg_passed_via_copy_constructor)*/
/*lint -esym(759,make_predeclared_function_symbol)*/
/*lint -esym(765,make_predeclared_function_symbol)*/
/*lint -esym(759,check_target_configuration)*/
/*lint -esym(765,check_target_configuration)*/
/*lint -esym(769,ec_cannot_build_temp_file_name)*/
/*lint -esym(552,total_remarks)*/
/*lint -esym(552,restrict_enabled)*/
/*lint -esym(552,logical_char_info_entries_used)*/
/*lint -esym(551,caching_tokens)*/
/*lint -esym(759,crc_32)*/
/*lint -esym(765,crc_32)*/
/*lint -esym(759,add_to_inline_namespace_list)*/
/*lint -esym(765,add_to_inline_namespace_list)*/
/*lint -esym(714,db_format_integer_value)*/
/*lint -esym(714,db_prefix)*/
/*lint -esym(714,db_prefix_ptr)*/
/*lint -esym(757,db_long_double)*/
/*lint -esym(714,db_long_double)*/
/*lint -esym(714,db_sym)*/
/*lint -esym(714,db_hash_statistics)*/
/*lint -esym(714,db_corresp)*/
/*lint -esym(714,db_stop_tokens)*/
/*lint -esym(714,db_template_param_list)*/
/*lint -esym(714,db_text_buffer)*/
/*lint -esym(714,db_scope_pragmas)*/
/*lint -esym(714,db_translation_unit)*/
/*lint -esym(714,db_translation_unit_stack)*/
/*lint -esym(714,db_variable)*/
/*lint -esym(714,db_line_for_seq)*/
/*lint -esym(714,db_name_reference)*/
/*lint -esym(714,f_db_sym_has_traced_name)*/
/*lint -esym(714,db_top_of_scope_stack)*/
/*lint -esym(714,db_pack_tokens)*/
/*lint -esym(714,db_seq_number_lookup_table)*/
/*lint -esym(714,db_internal_float_value)*/
/*lint -esym(714,db_context_stack)*/
/*lint -esym(714,db_hide_by_sig_list)*/
/*lint -esym(714,db_init_component)*/
/*lint -esym(714,free_template_decl_info)*/
/*lint -esym(759,f_db_sym_has_traced_name)*/
/*lint -esym(759,free_template_decl_info)*/
/*lint -esym(765,f_db_sym_has_traced_name)*/
/*lint -esym(765,free_template_decl_info)*/
/*lint -esym(755,db_sym_has_traced_name)*/
/*lint -esym(759,int_kind_name_full)*/
/*lint -esym(765,int_kind_name_full)*/
/*lint -esym(755,expect_error_str)*/
/*lint -esym(755,expect_error_str2)*/
/*lint -esym(755,check_assertion_or_expect_error)*/
/*lint -esym(755,check_assertion_or_expect_error_str)*/
/*lint -esym(755,check_assertion_or_expect_error_str2)*/
/*lint -esym(759, fetch_host_fp_value)*/
/*lint -esym(765, fetch_host_fp_value)*/
/*lint -esym(759,insert_string_into_token_stream)*/
/*lint -esym(765,insert_string_into_token_stream)*/
/*lint -esym(714,insert_string_into_token_stream)*/
/*lint -esym(755,alloc_cil_of_type)*/
/*lint -esym(714,enter_assert_predicate)*/
/*lint -esym(759,enter_assert_predicate)*/
/*lint -esym(765,enter_assert_predicate)*/
/*lint -esym(755,PREC_PRIMARY)*/
/*lint -esym(769,ec_bad_multibyte_char_locale)*/
/*lint -esym(759,clear_type)*/
/*lint -esym(765,clear_type)*/
/*lint -esym(755,END_EXTERN_C_BLOCK_IN_CPP_FILE)*/
/*lint -esym(755,EXTERN_C_BLOCK_IN_CPP_FILE)*/
/*lint -esym(755,EXTERN_C_IN_CPP_FILE)*/
/*lint -esym(755,is_cli_generic_instance_type)*/
/*lint -esym(759,put_str_into_text_buffer)*/
/*lint -esym(765,put_str_into_text_buffer)*/
/*lint -esym(755,complete_template_instance_is_needed)*/
/*lint -esym(759,is_template_dependent_type_or_cli_generic_param)*/
/*lint -esym(765,is_template_dependent_type_or_cli_generic_param)*/
/*lint -esym(756,an_other_pragma_function_ptr)*/
/*lint -esym(759,constant_is_shareable)*/
/*lint -esym(765,constant_is_shareable)*/
/*lint -esym(759,routine_and_node_from_function_expr)*/
/*lint -esym(765,routine_and_node_from_function_expr)*/
#if !RECORD_MACRO_INVOCATIONS
/*lint -esym(755,copy_simple_position_to_full_position)*/
#endif /* !RECORD_MACRO_INVOCATIONS */

/* Used in the main programs for the standalone c_gen_be, cp_gen_be, and
   il_display: */
/*lint -esym(769,ec_cl_back_end_requires_il_file)*/
/*lint -esym(769,ec_cl_could_not_open_il_file)*/
/*lint -esym(769,ec_cl_il_display_requires_il_file_name)*/

#if !C_ANACHRONISMS_ALLOWED
/*lint -esym(769,ec_old_fashioned_assignment_operator)*/
/*lint -esym(769,ec_old_fashioned_initializer)*/
#endif /* !C_ANACHRONISMS_ALLOWED */
#if !ASM_FUNCTION_ALLOWED
/*lint -esym(755,DSI_ASM_ALLOWED)*/
/*lint -esym(769,ec_asm_func_must_be_prototyped)*/
/*lint -esym(769,ec_bad_asm_func_ellipsis)*/
/*lint -esym(769,ec_asm_not_allowed)*/
/*lint -esym(769,ec_bad_asm_function_def)*/
/*lint -esym(769,ec_nonstd_asm_function)*/
/*lint -esym(769,ec_nonstd_asm_decl_within_template)*/
#endif /* !ASM_FUNCTION_ALLOWED */
#ifndef HOSTID
/*lint -esym(769,ec_cl_incorrect_host_id)*/
#endif /* ifndef HOSTID */
#if COMPILE_MULTIPLE_SOURCE_FILES || COMPILE_MULTIPLE_TRANSLATION_UNITS
/*lint -esym(769,ec_cl_too_many_arguments)*/
#endif /* COMPILE_MULTIPLE_SOURCE_FILES ||
          COMPILE_MULTIPLE_TRANSLATION_UNITS */
#if !COMPILE_MULTIPLE_SOURCE_FILES
/*lint -esym(769,ec_cl_output_file_incompatible_with_multiple_inputs)*/
/*lint -esym(769,ec_cl_pch_incompatible_with_multiple_inputs)*/
/*lint -esym(769,ec_cl_ii_file_name_incompatible_with_multiple_inputs)*/
/*lint -esym(769,
       ec_cl_one_instantiation_per_object_incompatible_with_multiple_inputs)*/
#endif /* !COMPILE_MULTIPLE_SOURCE_FILES */
#if USE_MMAP_FOR_MEMORY_REGIONS
/*lint -esym(769,ec_cl_invalid_pch_size)*/
/*lint -esym(769,ec_cl_pch_must_be_first)*/
/*lint -esym(769,ec_out_of_memory_during_pch_allocation)*/
/*lint -esym(769,ec_not_enough_preallocated_memory)*/
/*lint -esym(769,ec_program_entity_too_large_for_pch)*/
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
#if !BACK_END_IS_C_GEN_BE || !ISSUE_WARNING_ON_LONG_DOUBLE_AS_DOUBLE
/*lint -esym(769,ec_double_for_long_double)*/
#endif /* !BACK_END_IS_C_GEN_BE || !ISSUE_WARNING_ON_LONG_DOUBLE_AS_DOUBLE */
#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
/*lint -esym(769,ec_different_return_type_on_virtual_function_override)*/
#endif /* ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
#if !MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
/*lint -esym(769,ec_bad_multibyte_char)*/
#endif /* !MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
#if UNICODE_SOURCE_SUPPORTED
/*lint -esym(755,mbc_length)*/
/*lint -esym(755,mbc_scan_init)*/
#else /*!UNICODE_SOURCE_SUPPORTED */
/*lint -esym(769,a_unicode_source_kind_tag::usk_utf8)*/
/*lint -esym(769,a_unicode_source_kind_tag::usk_utf16LE)*/
/*lint -esym(769,a_unicode_source_kind_tag::usk_utf16BE)*/
/*lint -esym(769,ec_cl_unrecognized_unicode_source_kind)*/
/*lint -esym(769,ec_bad_unicode_char_in_pp_output)*/
#endif /* UNICODE_SOURCE_SUPPORTED */
#if !NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
/*lint -esym(769,ec_non_unicode_char_in_ident)*/
/*lint -esym(769,ec_non_unicode_char_in_header)*/
/*lint -esym(769,ec_invalid_locale)*/
/*lint -esym(769,ec_bad_unicode_char_in_string)*/
#endif /* !NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
#if !BACK_END_IS_C_GEN_BE
/*lint -esym(759,form_char)*/
/*lint -esym(765,form_char)*/
/*lint -esym(759,traverse_type_tree)*/
/*lint -esym(765,traverse_type_tree)*/
#endif /* !BACK_END_IS_C_GEN_BE */
#if DO_IL_LOWERING && DO_FULL_PORTABLE_EH_LOWERING
/*lint -esym(750,RDF_INDIRECT,RDF_THIS_PARAM_OFFSET)*/
#endif /* DO_IL_LOWERING && DO_FULL_PORTABLE_EH_LOWERING */
#if !DO_IL_LOWERING
/*lint -esym(552,virtual_function_table_definition)*/
/*lint -esym(759,alloc_unshared_constant_full)*/
/*lint -esym(765,alloc_unshared_constant_full)*/
/*lint -esym(714,alloc_unshared_constant_in_region)*/
/*lint -esym(759,alloc_unshared_constant_in_region)*/
/*lint -esym(765,alloc_unshared_constant_in_region)*/
/*lint -esym(759,clear_expr_node)*/
/*lint -esym(765,clear_expr_node)*/
/*lint -esym(759,implicit_cast)*/
/*lint -esym(765,implicit_cast)*/
/*lint -esym(759,find_disambiguator)*/
/*lint -esym(765,find_disambiguator)*/
/*lint -esym(759,find_local_static_variable_init)*/
/*lint -esym(765,find_local_static_variable_init)*/
/*lint -esym(714,find_assoc_pragma)*/
/*lint -esym(759,find_assoc_pragma)*/
/*lint -esym(765,find_assoc_pragma)*/
/*lint -esym(714,add_to_end_of_destructions_list)*/
/*lint -esym(759,add_to_end_of_destructions_list)*/
/*lint -esym(765,add_to_end_of_destructions_list)*/
/*lint -esym(759,alloc_node_for_allocated_constant)*/
/*lint -esym(765,alloc_node_for_allocated_constant)*/
/*lint -esym(759,new_or_delete_type_requires_array_handling)*/
/*lint -esym(765,new_or_delete_type_requires_array_handling)*/
/*lint -esym(759,int_kind_for_size_and_alignment)*/
/*lint -esym(765,int_kind_for_size_and_alignment)*/
/*lint -esym(714,rout_is_inline_template_function)*/
/*lint -esym(759,rout_is_inline_template_function)*/
/*lint -esym(765,rout_is_inline_template_function)*/
/*lint -esym(759,copy_list_of_expr_trees)*/
/*lint -esym(765,copy_list_of_expr_trees)*/
/*lint -esym(759,copy_node)*/
/*lint -esym(765,copy_node)*/
/*lint -esym(714,copy_statement)*/
/*lint -esym(759,copy_statement)*/
/*lint -esym(765,copy_statement)*/
/*lint -esym(759,copy_constant_full)*/
/*lint -esym(765,copy_constant_full)*/
/*lint -esym(759,set_class_keep_definition_in_il)*/
/*lint -esym(765,set_class_keep_definition_in_il)*/
/*lint -esym(759,set_expr_node_kind)*/
/*lint -esym(765,set_expr_node_kind)*/
/*lint -esym(759,set_node_operator)*/
/*lint -esym(765,set_node_operator)*/
/*lint -esym(759,set_scope_kind)*/
/*lint -esym(765,set_scope_kind)*/
/*lint -esym(759,set_statement_kind)*/
/*lint -esym(765,set_statement_kind)*/
/*lint -esym(759,is_default_operator_delete)*/
/*lint -esym(765,is_default_operator_delete)*/
/*lint -esym(714,num_array_elements)*/
/*lint -esym(759,num_array_elements)*/
/*lint -esym(765,num_array_elements)*/
/*lint -esym(714,num_vector_elements)*/
/*lint -esym(759,num_vector_elements)*/
/*lint -esym(765,num_vector_elements)*/
/*lint -esym(759,strip_rvalue_base_class_casts)*/
/*lint -esym(765,strip_rvalue_base_class_casts)*/
/*lint -esym(714,is_or_was_nullptr_type)*/
/*lint -esym(759,is_or_was_nullptr_type)*/
/*lint -esym(765,is_or_was_nullptr_type)*/
/*lint -esym(765,do_type_name_mangling)*/
/*lint -esym(714,make_comma_node_if_necessary)*/
/*lint -esym(759,make_comma_node_if_necessary)*/
/*lint -esym(765,make_comma_node_if_necessary)*/
#endif /* !DO_IL_LOWERING */
#if !BACK_END_IS_CP_GEN_BE
/*lint -esym(714,is_address_of_string_constant)*/
/*lint -esym(759,is_address_of_string_constant)*/
/*lint -esym(765,is_address_of_string_constant)*/
/*lint -esym(759,form_unknown_function_constant)*/
/*lint -esym(765,form_unknown_function_constant)*/
/*lint -esym(751,a_template_param_map_level)*/
/*lint -esym(759,form_uuidof_reference)*/
/*lint -esym(765,form_uuidof_reference)*/
/*lint -esym(759,form_typeid_reference)*/
/*lint -esym(765,form_typeid_reference)*/
/*lint -esym(759,decltype_arg)*/
/*lint -esym(765,decltype_arg)*/
/*lint -esym(769,ec_generated_c_plus_plus)*/
/*lint -esym(759,get_param_for_param_ref)*/
/*lint -esym(765,get_param_for_param_ref)*/
/*lint -esym(769,an_attribute_location_tag::al_id_equivalent_as_postfix)*/
#if MICROSOFT_EXTENSIONS_ALLOWED
/*lint -esym(759,form_calling_convention)*/
/*lint -esym(765,form_calling_convention)*/
/*lint -esym(759,form_pointer_modifiers)*/
/*lint -esym(765,form_pointer_modifiers)*/
/*lint -esym(759,find_ms_attribute_for_entity)*/
/*lint -esym(765,find_ms_attribute_for_entity)*/
/*lint -esym(714,find_ms_attribute_for_entity)*/
/*lint -esym(759,form_property_or_event_name_as_qualifier_if_needed)*/
/*lint -esym(765,form_property_or_event_name_as_qualifier_if_needed)*/
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* !BACK_END_IS_CP_GEN_BE */
#if !DEBUG
/*lint -esym(749,pfs_last)*/
#endif /* !DEBUG */
#if !INCLUDE_EDG_TEST_PRAGMAS
/*lint -esym(528,add_other_pragma_kind_description)*/
#endif /* !INCLUDE_EDG_TEST_PRAGMAS */
#ifdef __sun
/* Solaris stdio.h doesn't define fileno in strict mode (fileno is not
   ANSI/ISO C; it's in POSIX). */
#ifndef __linux__
extern int fileno(FILE *);
#endif /* ifndef __linux__ */
/*lint -esym(526,fileno)*/
/*lint -esym(526,isnan)*/
/*lint -esym(526,finite)*/
/*lint -esym(752,finite)*/
/*lint -esym(752,isnan)*/
#endif /* ifdef __sun */
#if !GNU_EXTENSIONS_ALLOWED
/*lint -esym(769,ec_noreturn_function_does_return)*/
/*lint -esym(769,ec_invalid_empty_initializer_list)*/
/*lint -esym(759,constant_rvalue_pointer_full)*/
/*lint -esym(765,constant_rvalue_pointer_full)*/
#if TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES
/*lint -esym(759,field_alignment_for)*/
/*lint -esym(765,field_alignment_for)*/
#endif /* TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES */
#endif /* !GNU_EXTENSIONS_ALLOWED */
#if !(GNU_EXTENSIONS_ALLOWED && GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED)
/*lint -esym(769,ec_bad_variable_for_init_priority)*/
/*lint -esym(769,ec_init_priority_reserved)*/
/*lint -esym(769,ec_ctor_dtor_priority_reserved)*/
#endif /* !(GNU_EXTENSIONS_ALLOWED && GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED) */
#if CHECKING && DEBUG
/*lint -esym(759,trace_entry)*/
#endif /* CHECKING && DEBUG */
#if CHECKING && DEBUG && ALTERNATE_IL_FILE_FORMAT
/*lint -esym(765, trace_entry)*/
/*lint -esym(714, trace_entry)*/
#endif /* CHECKING && DEBUG && ALTERNATE_IL_FILE_FORMAT */
#if ALTERNATE_IL_FILE_FORMAT
/*lint -esym(769,ec_intermediate_language_7)*/
#else /* !ALTERNATE_IL_FILE_FORMAT */
/*lint -esym(756,a_prefix_entry_number)*/
#endif /* ALTERNATE_IL_FILE_FORMAT */
#if DEBUG
/*lint -esym(765, db_sym_list)*/
/*lint -esym(714, db_sym_list)*/
/*lint -esym(714, db_scheduled_routine_moves)*/
/*lint -esym(765, db_attribute_list)*/
/*lint -esym(714, db_attribute_list)*/
#endif /* DEBUG */
#if !UPC_EXTENSIONS_ALLOWED
/*lint -esym(769,ec_unrecognized_upc_pragma)*/
/*lint -esym(769,ec_mismatched_shared_block_size)*/
/*lint -esym(769,ec_ambiguous_block_size_spec)*/
/*lint -esym(769,ec_shared_block_size_must_be_positive)*/
/*lint -esym(769,ec_multiple_block_sizes)*/
/*lint -esym(769,ec_nonshared_strict_relaxed)*/
/*lint -esym(769,ec_threads_constant_not_allowed)*/
/*lint -esym(769,ec_shared_block_size_too_large)*/
/*lint -esym(769,ec_function_returning_shared)*/
/*lint -esym(769,ec_shared_nonthreads_dim)*/
/*lint -esym(769,ec_shared_inside_struct)*/
/*lint -esym(769,ec_shared_parameter)*/
/*lint -esym(769,ec_threads_dimension_requires_definite_block_size)*/
/*lint -esym(769,ec_bad_shared_storage_class)*/
/*lint -esym(769,ec_nonshared_blocksizeof)*/
/*lint -esym(769,ec_nested_upc_forall)*/
/*lint -esym(769,ec_exit_forall)*/
/*lint -esym(769,ec_unexpected_upc_shared_specifier)*/
/*lint -esym(769,ec_bad_affinity)*/
/*lint -esym(769,ec_shared_affinity_type)*/
/*lint -esym(769,ec_upc_shared_void_comparison)*/
/*lint -esym(769,ec_cl_upc_requires_ansi_c_dialect)*/
#endif /* !UPC_EXTENSIONS_ALLOWED */
/*lint -esym(769,a_builtin_function_kind_tag::bfk_fsqrt)*/
#if !CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
/*lint -esym(759,find_progenitor_symbol)*/
/*lint -esym(765,find_progenitor_symbol)*/
/*lint -esym(769,ec_cfront_name_lookup_bug)*/
#endif /* !CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */
#if !ASSIGNMENT_TO_THIS_ALLOWED
/*lint -esym(759,add_constructor_wrapper_code)*/
/*lint -esym(765,add_constructor_wrapper_code)*/
/*lint -esym(769,ec_assignment_to_this)*/
/*lint -esym(759,is_this_parameter_operand)*/
/*lint -esym(765,is_this_parameter_operand)*/
#endif /* !ASSIGNMENT_TO_THIS_ALLOWED */
#if !NEW_CAN_BE_FOLDED_INTO_CTOR
/*lint -esym(759,initial_processing_on_destructible_initialization)*/
/*lint -esym(765,initial_processing_on_destructible_initialization)*/
#endif /* !NEW_CAN_BE_FOLDED_INTO_CTOR */
#if IA64_ABI
/*lint -esym(755,first_derivation_is_direct)*/
/*lint -esym(759,repr_for_ptr_to_data_member_constant)*/
/*lint -esym(765,repr_for_ptr_to_data_member_constant)*/
/*lint -esym(759,repr_for_ptr_to_member_function_constant)*/
/*lint -esym(765,repr_for_ptr_to_member_function_constant)*/
/*lint -esym(759,make_typeinfo_type)*/
/*lint -esym(765,make_typeinfo_type)*/
/*lint -esym(759,make_ctor_implied_arg_list)*/
/*lint -esym(765,make_ctor_implied_arg_list)*/
/*lint -esym(759,make_dtor_implied_arg_list)*/
/*lint -esym(765,make_dtor_implied_arg_list)*/
/*lint -esym(759,do_type_name_mangling)*/
/*lint -esym(765,do_type_name_mangling)*/
/*lint -esym(769,ec_mangled_name_too_long)*/
/*lint -esym(759,vtbl_addr_from_construction_vtbls_array)*/
/*lint -esym(765,vtbl_addr_from_construction_vtbls_array)*/
#else /* !IA64_ABI */
/*lint -esym(759,add_cast_to_char_star)*/
/*lint -esym(765,add_cast_to_char_star)*/
/*lint -esym(759,type_info_names)*/
/*lint -esym(765,type_info_names)*/
/*lint -esym(759,expr_list_has_side_effects)*/
/*lint -esym(765,expr_list_has_side_effects)*/
/*lint -esym(714,expr_list_has_side_effects)*/
/*lint -esym(769,ec_field_uses_tail_padding)*/
/*lint -esym(769,ec_base_uses_tail_padding)*/
/*lint -esym(769,ec_size_affected_by_tail_padding)*/
/*lint -esym(769,ec_gnu_may_use_bit_padding)*/
/*lint -esym(769,ec_no_gnu_virtual_base_gap)*/
/*lint -esym(769,ec_gnu_virtual_base_gap)*/
/*lint -esym(759,build_construction_vtbls_pointer)*/
/*lint -esym(765,build_construction_vtbls_pointer)*/
/*lint -esym(769,ec_no_default_delete_in_virtual_dtor)*/
/*lint -esym(759,should_drop_const_on_this_param_variable)*/
/*lint -esym(765,should_drop_const_on_this_param_variable)*/
/*lint -esym(759,make_vtbl_entry_type)*/
/*lint -esym(765,make_vtbl_entry_type)*/
#endif /* IA64_ABI */
#if !INSTANTIATE_EXTERN_INLINE || !IA64_ABI
/*lint -esym(759,get_mangled_function_name_full)*/
/*lint -esym(765,get_mangled_function_name_full)*/
#endif /* !INSTANTIATE_EXTERN_INLINE || !IA64_ABI */
#if !CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
/*lint -esym(769,ec_cfront_multiple_nested_types)*/
/*lint -esym(769,ec_cfront_global_defined_after_nested_type)*/
#endif /* !CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
#if !LOWER_EXTERN_INLINE
/*lint -esym(759,make_file_scope_temporary)*/
/*lint -esym(765,make_file_scope_temporary)*/
#endif /* !LOWER_EXTERN_INLINE */
#if !REDEFINE_EXTNAME_PRAGMA_ENABLED
/*lint -esym(769,ec_bad_linkage_for_redefine_extname)*/
#endif /* !REDEFINE_EXTNAME_PRAGMA_ENABLED */
/*lint -esym(759,traverse_statement)*/
/*lint -esym(765,traverse_statement)*/
/*lint -esym(759,traverse_statement_list)*/
/*lint -esym(765,traverse_statement_list)*/
#if MICROSOFT_EXTENSIONS_ALLOWED
/*lint -esym(769,an_ms_attribute_kind_tag::msak_last)*/
/*lint -esym(755,MSAT_ANY_TYPE)*/
/*lint -esym(769,a_microsoft_pragma_comment_type_tag::mpct_compiler)*/
/*lint -esym(769,a_microsoft_pragma_comment_type_tag::mpct_exestr)*/
/*lint -esym(769,a_microsoft_pragma_comment_type_tag::mpct_lib)*/
/*lint -esym(769,a_microsoft_pragma_comment_type_tag::mpct_linker)*/
/*lint -esym(769,a_microsoft_pragma_comment_type_tag::mpct_user)*/
/*lint -esym(769,a_microsoft_pragma_comment_type_tag::mpct_last)*/
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if !FIXED_POINT_ALLOWED
/*lint -esym(759,fixed_point_enabled)*/
/*lint -esym(765,fixed_point_enabled)*/
/*lint -esym(769,ec_nonstd_fixed_point_suffix)*/
/*lint -esym(769,ec_cl_fixed_point_option_only_in_C)*/
/*lint -esym(769,ec_integer_may_not_fit_in_fixed_point_result)*/
/*lint -esym(769,ec_bad_fixed_point_value)*/
/*lint -esym(769,ec_inexact_fxp_conversion)*/
/*lint -esym(769,ec_operation_may_not_fit_in_fixed_point_result)*/
/*lint -esym(769,ec_implicit_fixed_point_to_floating_point_conversion)*/
/*lint -esym(769,ec_no_classification_for_fixed_point_type)*/
/*lint -esym(769,ec_fixed_template_parameter)*/
/*lint -esym(769,ec_float_to_fixed_conversion)*/
/*lint -esym(769,ec_inexact_fixed_conversion)*/
/*lint -esym(769,ec_fixed_sign_change)*/
/*lint -esym(769,ec_integer_to_fixed_conversion)*/
/*lint -esym(769,ec_bad_fixed_operation_result)*/
/*lint -esym(769,ec_fixed_to_float_conversion)*/
/*lint -esym(769,ec_fixed_to_integer_conversion)*/
/*lint -esym(769,ec_fixed_to_fixed_conversion)*/
/*lint -esym(759,number_of_bits_in_mantissa)*/
/*lint -esym(765,number_of_bits_in_mantissa)*/
/*lint -esym(759,init_mantissa)*/
/*lint -esym(765,init_mantissa)*/
/*lint -esym(759,round_hex_fp_value)*/
/*lint -esym(765,round_hex_fp_value)*/
/*lint -esym(759,conv_hex_string_to_mantissa_and_exponent)*/
/*lint -esym(765,conv_hex_string_to_mantissa_and_exponent)*/
/*lint -esym(759,shift_left_mantissa)*/
/*lint -esym(765,shift_left_mantissa)*/
/*lint -esym(759,shift_right_mantissa)*/
/*lint -esym(765,shift_right_mantissa)*/
/*lint -esym(759,get_integer_attributes)*/
/*lint -esym(765,get_integer_attributes)*/
/*lint -esym(759,trunc_and_set_integer)*/
/*lint -esym(765,trunc_and_set_integer)*/
/*lint -esym(759,mantissa_is_zero)*/
/*lint -esym(765,mantissa_is_zero)*/
/*lint -esym(759,conv_mantissa_to_floating_point)*/
/*lint -esym(765,conv_mantissa_to_floating_point)*/
/*lint -esym(759,value_of_integer_value)*/
/*lint -esym(765,value_of_integer_value)*/
/*lint -esym(769,tok_fract)*/
/*lint -esym(769,tok_accum)*/
#endif /* !FIXED_POINT_ALLOWED */
#if !NAMED_ADDRESS_SPACES_ALLOWED
/*lint -esym(759,named_address_spaces_enabled)*/
/*lint -esym(765,named_address_spaces_enabled)*/
/*lint -esym(769,ec_cl_named_address_spaces_option_only_in_C)*/
/*lint -esym(769,ec_multiple_named_address_spaces)*/
/*lint -esym(769,ec_bad_storage_class_for_named_address_space_variable)*/
/*lint -esym(769,ec_type_with_named_address_space_not_allowed)*/
/*lint -esym(769,ec_named_address_space_on_function_type)*/
/*lint -esym(769,ec_field_type_cannot_be_qualified_with_named_address_space)*/
/*lint -esym(769,ec_named_address_space_not_allowed)*/
#endif /* !NAMED_ADDRESS_SPACES_ALLOWED */
#if !NAMED_REGISTERS_ALLOWED
/*lint -esym(769,ec_cl_named_registers_option_only_in_C)*/
/*lint -esym(769,ec_named_register_not_allowed)*/
/*lint -esym(769,ec_register_storage_class_conflict)*/
/*lint -esym(769,ec_aliased_variable_cannot_have_register_storage_class)*/
/*lint -esym(769,ec_register_in_use)*/
/*lint -esym(769,ec_missing_named_register_storage_class)*/
/*lint -esym(769,ec_previous_decl_at)*/
/*lint -esym(769,ec_register_too_small)*/
/*lint -esym(769,ec_no_named_register_for_array)*/
#endif /* !NAMED_REGISTERS_ALLOWED */
#if !(FIXED_POINT_ALLOWED && NAMED_ADDRESS_SPACES_ALLOWED && \
      NAMED_REGISTERS_ALLOWED)
/*lint -esym(769,
          ec_embedded_c_option_incompatible_with_individual_feature_options)*/
#endif /* !(FIXED_POINT_ALLOWED && NAMED_ADDRESS_SPACES_ALLOWED && ...) */
#if DO_IL_LOWERING
#if VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS
/*lint -esym(759,add_to_end_of_pending_stmk_init_statements_list)*/
/*lint -esym(765,add_to_end_of_pending_stmk_init_statements_list)*/
/*lint -esym(759,assign_expr_to_temp)*/
/*lint -esym(765,assign_expr_to_temp)*/
#endif /* VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS */
#if FIXED_POINT_ALLOWED && !LOWER_FIXED_POINT
/*lint -esym(759, fixed_point_type_used_in_primary_IL)*/
/*lint -esym(765, fixed_point_type_used_in_primary_IL)*/
/*lint -esym(759, make_lvalue_reusable_copy_full)*/
/*lint -esym(765, make_lvalue_reusable_copy_full)*/
#endif /* FIXED_POINT_ALLOWED && !LOWER_FIXED_POINT */
/*lint -esym(714,function_scope_for_local_type)*/
/*lint -esym(759,function_scope_for_local_type)*/
/*lint -esym(765,function_scope_for_local_type)*/
#endif /* DO_IL_LOWERING */
#if DECL_MODIFIERS_IN_USE && \
    !(MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED)
/*lint -esym(759,update_extended_decl_info_for_class)*/
/*lint -esym(765,update_extended_decl_info_for_class)*/
#endif /* DECL_MODIFIERS_IN_USE && !(MICROSOFT_EXTENSIONS_ALLOWED || ...) */
#if ABI_COMPATIBILITY_VERSION >= 306
/*lint -esym(769,ec_cl_vla_option_only_in_C)*/
#endif /* ABI_COMPATIBILITY_VERSION >= 306 */
#if ABI_COMPATIBILITY_VERSION >= 402
/*lint -esym(769,ec_sfinae_requires_newer_abi_version)*/
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
#if ABI_COMPATIBILITY_VERSION >= 402 || !NEED_NAME_MANGLING
/*lint -esym(769,ec_cppcli_requires_newer_abi_version)*/
#endif /* ABI_COMPATIBILITY_VERSION >= 402 || !NEED_NAME_MANGLING */
#if !FULLY_RESOLVED_MACRO_POSITIONS
/*lint -esym(769,ec_in_macro_expansion_at)*/
#endif /* !FULLY_RESOLVED_MACRO_POSITIONS */
#if !RECORD_MACRO_INVOCATIONS
/*lint -esym(769,ec_name_of_unknown_macro)*/
/*lint -esym(769,ec_in_expansion_of_macro)*/
/*lint -esym(769,ec_macro_context_lines_skipped)*/
/*lint -esym(769,ec_in_expansion_of_macro_last)*/
#endif /* !RECORD_MACRO_INVOCATIONS */
#if !GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
/*lint -esym(769,ec_if_exists_not_allowed)*/
/*lint -esym(769,ec_if_exists_not_closed)*/
#endif /* !GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
#if !GNU_EXTENSIONS_ALLOWED
/*lint -esym(769,a_builtin_operation_kind_tag::bok_types_compatible)*/
/*lint -esym(769,ec_gnu_attr_on_template_redecl)*/
/*lint -esym(769,ec_gnu_attr_on_template_redecl_but_original_kept)*/
/*lint -esym(769,ec_no_packing_of_non_POD_field)*/
/*lint -esym(769,ec_missing_gnu_inline_attr_on_redeclaration)*/
/*lint -esym(769,ec_pragma_gcc_system_header_in_primary_file)*/
/*lint -esym(769,ec_3rd_arg_of_assume_aligned_must_be_integral)*/
#endif /* !GNU_EXTENSIONS_ALLOWED */
/*lint -esym(759,is_wide_string_constant)*/
/*lint -esym(765,is_wide_string_constant)*/
/*lint -esym(714,is_wide_string_constant)*/
/*lint -esym(755,extract_wide_char_from_string)*/
/*lint -esym(755,wide_string_type)*/
#if !MICROSOFT_EXTENSIONS_ALLOWED
/*lint -esym(769,ec_cl_unrecognized_calling_convention)*/
/*lint -esym(769,ec_cl_calling_convention_list)*/
/*lint -esym(769,ec_microsoft_interface)*/
/*lint -esym(769,a_builtin_operation_kind_tag::bok_has_finalizer)*/
/*lint -esym(769,a_builtin_operation_kind_tag::bok_is_delegate)*/
/*lint -esym(769,a_builtin_operation_kind_tag::bok_is_interface_class)*/
/*lint -esym(769,a_builtin_operation_kind_tag::bok_is_ref_array)*/
/*lint -esym(769,a_builtin_operation_kind_tag::bok_is_ref_class)*/
/*lint -esym(769,a_builtin_operation_kind_tag::bok_is_sealed)*/
/*lint -esym(769,a_builtin_operation_kind_tag::bok_is_simple_value_class)*/
/*lint -esym(769,a_builtin_operation_kind_tag::bok_is_value_class)*/
/*lint -esym(759,curr_token_is_identifier_string)*/
/*lint -esym(765,curr_token_is_identifier_string)*/
/*lint -esym(759,is_assignment_operator_for_copy)*/
/*lint -esym(765,is_assignment_operator_for_copy)*/
/*lint -esym(759,mark_init_component_list_as_permanently_allocated)*/
/*lint -esym(765,mark_init_component_list_as_permanently_allocated)*/
#endif /* !MICROSOFT_EXTENSIONS_ALLOWED */
#if !(GNU_EXTENSIONS_ALLOWED && LOWER_COMPLEX)
/*lint -esym(759, lower_c99_constant_expr)*/
/*lint -esym(765, lower_c99_constant_expr)*/
#endif /* !(GNU_EXTENSIONS_ALLOWED && LOWER_COMPLEX) */
#if !LOWER_COMPLEX
/*lint -esym(759,set_complex_constant)*/
/*lint -esym(765,set_complex_constant)*/
/*lint -esym(714,set_complex_constant)*/
#endif /* !LOWER_COMPLEX */
/*lint -esym(759,alignment_of_variable)*/
/*lint -esym(765,alignment_of_variable)*/
/*lint -esym(714,alignment_of_variable)*/
#if !(GNU_EXTENSIONS_ALLOWED && GNU_BUILTIN_SYNC_FUNCTIONS_ALLOWED)
/*lint -esym(769,ec_bad_type_for_gnu_sync_function)*/
/*lint -esym(769,ec_invalid_gnu_sync_size)*/
/*lint -esym(769,ec_extra_arguments_ignored)*/
/*lint -esym(769,ec_first_arg_must_be_integer_constant)*/
#endif /* !(GNU_EXTENSIONS_ALLOWED && GNU_BUILTIN_SYNC_FUNCTIONS_ALLOWED) */
#if RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
/*lint -esym(769,ec_asm_modifier_ignored)*/
/*lint -esym(769,ec_bad_asm_constraint_modifier)*/
/*lint -esym(769,ec_bad_asm_constraint_letter)*/
/*lint -esym(769,ec_missing_constraint_letter)*/
/*lint -esym(769,ec_asm_output_must_have_output_mod)*/
/*lint -esym(769,ec_asm_input_must_not_have_output_mod)*/
/*lint -esym(769,ec_register_used_twice)*/
/*lint -esym(769,ec_register_used_and_clobbered)*/
/*lint -esym(769,ec_fixed_register_used)*/
/*lint -esym(769,ec_match_limit_for_symbolic_asm_operand)*/
#endif /* RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
#if INTEGER_VALUE_REPR_IS_A_HOST_INTEGER
/*lint -esym(759,conv_float_string_to_integer_value)*/
/*lint -esym(765,conv_float_string_to_integer_value)*/
/*lint -esym(714,conv_float_string_to_integer_value)*/
#endif /* INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
#if !GNU_VISIBILITY_ATTRIBUTE_ALLOWED
/*lint -esym(769,ec_unrecognized_gcc_pragma)*/
/*lint -esym(769,ec_unrecognized_gcc_visibility_pragma)*/
/*lint -esym(769,ec_gnu_visibility_conflict)*/
/*lint -esym(769,ec_unrecognized_visibility)*/
/*lint -esym(769,ec_attribute_does_not_apply_to_type)*/
/*lint -esym(769,ec_ELF_visibility_pop_mismatch)*/
/*lint -esym(769,ec_ELF_visibility_stack_empty)*/
#endif /* !GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
#if !GNU_VECTOR_TYPES_ALLOWED
/*lint -esym(
  769,ec_vector_size_attribute_requires_integral_floating_or_enum_type)*/
/*lint -esym(769,ec_vector_size_too_large)*/
/*lint -esym(769,ec_vector_size_must_be_power_of_two)*/
/*lint -esym(769,ec_vector_size_must_be_multiple_of_element_size)*/
/*lint -esym(769,ec_mixed_vector_scalar_operation)*/
/*lint -esym(769,ec_vectors_must_have_same_size)*/
/*lint -esym(769,ec_dependent_vector_size)*/
/*lint -esym(769,ec_vector_size_with_dependent_element_type)*/
/*lint -esym(769,ec_vector_size_attribute_not_allowed)*/
/*lint -esym(769,ec_vector_size_attribute_on_complex_type)*/
/*lint -esym(769,ec_vector_size_must_be_integer_constant)*/
/*lint -esym(769,ec_vector_element_type_mismatch)*/
/*lint -esym(769,ec_vector_operation_requires_integer_vector)*/
/*lint -esym(769,ec_vector_size_attribute_on_enum_type)*/
/*lint -esym(769,ec_incompatible_vectors_conversion)*/
/*lint -esym(769,ec_vector_template_parameter)*/
#endif /* GNU_VECTOR_TYPES_ALLOWED */
/*lint -esym(759,find_local_scope)*/
/*lint -esym(765,find_local_scope)*/
/*lint -esym(714,find_local_scope)*/
/*lint -esym(759,node_operands_have_correct_lvalueness)*/
/*lint -esym(765,node_operands_have_correct_lvalueness)*/
/*lint -esym(714,node_operands_have_correct_lvalueness)*/
/*lint -esym(759,tree_has_correct_lvalueness)*/
/*lint -esym(765,tree_has_correct_lvalueness)*/
/*lint -esym(714,tree_has_correct_lvalueness)*/
/*lint -esym(759,f_get_parent_scope_of)*/
/*lint -esym(765,f_get_parent_scope_of)*/
/*lint -esym(714,f_get_parent_scope_of)*/
/*lint -esym(755,get_parent_scope_of)*/
/*lint -esym(759,node_includes_lvalue_to_rvalue_conv)*/
/*lint -esym(765,node_includes_lvalue_to_rvalue_conv)*/
/* Suppress spurious data access warnings. */
/*lint -efunc(670,macro_invocation)*/
/*lint -efunc(690,macro_invocation)*/
#if !(GNU_EXTENSIONS_ALLOWED && TARG_HAS_IEEE_FLOATING_POINT)
/*lint -esym(769,ec_call_requires_one_argument)*/
/*lint -esym(769,ec_call_requires_floating_point_argument)*/
#endif /* !(GNU_EXTENSIONS_ALLOWED && TARG_HAS_IEEE_FLOATING_POINT) */
/*lint -esym(759, node_is_pointer_with_restrict_semantics)*/
/*lint -esym(765, node_is_pointer_with_restrict_semantics)*/
/*lint -esym(714, node_is_pointer_with_restrict_semantics)*/

/*lint -esym(769,an_attribute_arg_kind_tag::aak_last)*/
/*lint -esym(769,an_attribute_family_tag::af_ms_declspec)*/
/*lint -esym(769,an_attribute_location_tag::al_base_specifier)*/
/*lint -esym(769,an_attribute_location_tag::al_trailing_return)*/
/*lint -esym(769,an_attribute_location_tag::al_other)*/
/*lint -esym(769,an_attribute_location_tag::al_last)*/
/*lint -esym(769,an_expr_node_kind_tag::enk_runtime_sizeof)*/
#if !(EDG_WIN32 && MICROSOFT_EXTENSIONS_ALLOWED)
/*lint -esym(552,using_framework_directory)*/
/*lint -esym(769,ec_win32_api_error)*/
#endif /* !(EDG_WIN32 && MICROSOFT_EXTENSIONS_ALLOWED) */
/*lint -esym(769,a_cpp_cli_feature_tag::*)*/
/*lint -esym(769,a_cpp_cli_import_flag_tag::*)*/
#if READ_CPPCLI_PORTABLE_ASSEMBLIES && !WRITE_CPPCLI_PORTABLE_ASSEMBLIES
/*lint -esym(759,clear_portable_assembly_header)*/
/*lint -esym(765,clear_portable_assembly_header)*/
#endif /* READ_CPPCLI_PORTABLE_ASSEMBLIES && !WRITE_CPPCLI_PORTABLE_ASSEM... */
#if MICROSOFT_EXTENSIONS_ALLOWED
/*lint -esym(769,a_cli_symbol_kind_tag::csk_system_int16)*/
/*lint -esym(769,a_cli_symbol_kind_tag::csk_system_uint16)*/
/*lint -esym(769,a_cli_symbol_kind_tag::csk_system_int32)*/
/*lint -esym(769,a_cli_symbol_kind_tag::csk_system_uint32)*/
/*lint -esym(769,a_cli_symbol_kind_tag::csk_system_int32_is_long)*/
/*lint -esym(769,a_cli_symbol_kind_tag::csk_system_uint32_is_long)*/
/*lint -esym(769,a_cli_symbol_kind_tag::csk_system_int64)*/
/*lint -esym(769,a_cli_symbol_kind_tag::csk_system_uint64)*/
/*lint -esym(769,a_cli_symbol_kind_tag::csk_system_single)*/
/*lint -esym(769,a_cli_symbol_kind_tag::csk_system_double)*/
/*lint -esym(769,a_cli_symbol_kind_tag::csk_system_double_is_long)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_addition)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_addition_assignment)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_address_of)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_assign)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_bitwise_and)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_bitwise_and_assignment)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_bitwise_or)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_bitwise_or_assignment)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_comma)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_decrement)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_division)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_division_assignment)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_equality)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_exclusive_or)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_exclusive_or_assignment)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_false)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_function_call)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_greater_than)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_greater_than_or_equal)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_increment)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_inequality)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_left_shift)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_left_shift_assignment)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_less_than)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_less_than_or_equal)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_logical_and)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_logical_not)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_logical_or)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_member_selection)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_modulus)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_modulus_assignment)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_multiply)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_multiplication_assignment)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_ones_complement)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_pointer_dereference)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_pointer_to_member_selection)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_right_shift)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_right_shift_assignment)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_signed_right_shift)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_subscript)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_subtraction)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_subtraction_assignment)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_true)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_unary_negation)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_unary_plus)*/
/*lint -esym(769,a_cli_operator_kind_tag::cok_unsigned_right_shift)*/
/*lint -esym(769,
             a_cli_operator_kind_tag::cok_unsigned_right_shift_assignment)*/
#if !BACK_END_IS_CP_GEN_BE
/*lint -esym(759,cli_managed_class_tag_keyword)*/
/*lint -esym(765,cli_managed_class_tag_keyword)*/
/*lint -esym(714,cli_managed_class_tag_keyword)*/
#endif /* !BACK_END_IS_CP_GEN_BE */
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
/*lint -esym(759,is_class_struct_type)*/
/*lint -esym(765,is_class_struct_type)*/
/*lint -esym(714,is_class_struct_type)*/
/*lint -esym(769,ec_cli_get_accessor_missing)*/
/*lint -esym(769,ec_cli_set_accessor_missing)*/
/*lint -esym(769,ec_conflicting_properties)*/
/*lint -esym(769,ec_property_set)*/
/*lint -esym(769,ec_property)*/
/*lint -esym(769,ec_cli_entity_not_loaded)*/
/*lint -esym(769,ec_for_each_missing_function)*/
/*lint -esym(769,ec_for_each_function_takes_args)*/
/*lint -esym(769,ec_for_each_function_const_violation)*/
/*lint -esym(769,ec_for_each_function_access_violation)*/
/*lint -esym(769,ec_for_each_static_function)*/
/*lint -esym(769,ec_for_each_no_matching_overload)*/
/*lint -esym(769,ec_for_each_invalid_return_type_for_move_next)*/
/*lint -esym(769,ec_for_each_incompatible_type)*/
/*lint -esym(769,ec_for_each_incompatible_iterator)*/
/*lint -esym(769,ec_for_each_missing_field)*/
/*lint -esym(769,ec_for_each_getenumerator_return_type_invalid)*/
/*lint -esym(769,ec_exp_in)*/
/*lint -esym(757,insert_temporary_initialization)*/
/*lint -esym(759,process_simple_assignment)*/
/*lint -esym(765,process_simple_assignment)*/
/*lint -esym(769,ec_no_suitable_synthesis_assignment_operator)*/
/*lint -esym(769,ec_not_a_generic_param)*/
/*lint -esym(769,ec_not_generic_param_of_curr_decl)*/
/*lint -esym(769,ec_invalid_constraint)*/
/*lint -esym(769,ec_invalid_event_use)*/
/*lint -esym(769,ec_event_without_raise_invoked)*/
/*lint -esym(769,ec_bad_event_compound_assignment)*/
/*lint -esym(769,ec_managed_nullptr_not_allowed)*/
/*lint -esym(769,ec_typeid_of_managed_type)*/
/*lint -esym(769,ec_cli_typeid_of_managed_pointer)*/
/*lint -esym(769,ec_name_before_typeid_not_type)*/
/*lint -esym(769,ec_reserved_dispose)*/
/*lint -esym(769,ec_reserved_finalize)*/
/*lint -esym(769,ec_invalid_idisposable_dispose)*/
/*lint -esym(769,ec_invalid_object_finalize)*/
/*lint -esym(769,ec_finalize_does_not_override_object_finalize)*/
/*lint -esym(769,ec_cast_interior_ptr_to_ptr)*/
/*lint -esym(769,ec_cppcli_explicit_conversion_only_in_ref_and_value_classes)*/
/*lint -esym(769,ec_cppcli_explicit_conversion_is_virtual)*/
/*lint -esym(769,ec_invalid_gcnew_type)*/
/*lint -esym(769,ec_gcnew_used_with_placement_syntax)*/
/*lint -esym(769,ec_new_used_on_unsuitable_value_type)*/
/*lint -esym(769,ec_new_used_on_managed_class_type)*/
/*lint -esym(769,ec_new_used_on_handle_or_tracking_reference_type)*/
/*lint -esym(769,ec_cli_array_must_have_new_or_array_init)*/
/*lint -esym(769,ec_gcnew_bad_type_used_with_array_init)*/
/*lint -esym(769,ec_gcnew_used_with_auto_syntax)*/
/*lint -esym(769,ec_too_many_array_bounds)*/
/*lint -esym(769,ec_too_few_array_bounds)*/
/*lint -esym(769,ec_too_few_generic_args)*/
/*lint -esym(769,ec_too_many_generic_args)*/
/*lint -esym(769,ec_generic_class)*/
/*lint -esym(769,ec_no_matching_arity)*/
/*lint -esym(759,convert_arg_operand_list_to_expr_list)*/
/*lint -esym(765,convert_arg_operand_list_to_expr_list)*/
/*lint -esym(769,ec_bad_function_for_delegate)*/
/*lint -esym(769,ec_ambiguous_function_for_delegate)*/
/*lint -esym(769,ec_mismatched_function_for_delegate)*/
/*lint -esym(769,ec_missing_delegate_object)*/
/*lint -esym(769,ec_nonmanaged_function_for_delegate)*/
/*lint -esym(769,ec_superfluous_delegate_object)*/
/*lint -esym(769,ec_incompatible_delegate_object)*/
/*lint -esym(769,ec_address_of_managed_member_function)*/
/*lint -esym(769,ec_bad_delegate_init_list)*/
/*lint -esym(769,ec_interface_not_implemented)*/
/*lint -esym(769,ec_gcnew_of_native_array)*/
/*lint -esym(769,ec_cli_interface_cannot_have_assignment)*/
/*lint -esym(769,ec_sealed_cli_interface)*/
/*lint -esym(769,ec_destructor_or_finalizer_with_named_override)*/
/*lint -esym(769,ec_override_name_is_destructor_or_finalizer)*/
/*lint -esym(769,ec_named_override_requires_managed_type)*/
/*lint -esym(769,ec_named_override_type_mismatch)*/
/*lint -esym(769,ec_static_constructor_with_named_override)*/
/*lint -esym(769,ec_static_conversion_function_must_have_one_parameter)*/
/*lint -esym(769,ec_branch_into_finally)*/
/*lint -esym(769,ec_return_from_finally)*/
/*lint -esym(769,ec_missing_finally)*/
/*lint -esym(769,ec_managed_object_not_thrown_by_handle)*/
/*lint -esym(769,ec_managed_object_not_caught_by_handle)*/
/*lint -esym(769,ec_break_cannot_be_in_finally_block)*/
/*lint -esym(769,ec_continue_cannot_be_in_finally_block)*/
/*lint -esym(769,ec_duplicate_constraint)*/
/*lint -esym(769,ec_multiple_class_constraints)*/
/*lint -esym(769,ec_multiple_constraint_clauses)*/
/*lint -esym(769,ec_initonly_static_data_member_not_initialized)*/
/*lint -esym(769,ec_cli_param_array_must_be_last_parameter)*/
/*lint -esym(769,ec_default_arg_used_in_param_array_function)*/
/*lint -esym(769,ec_ellipsis_after_param_array)*/
/*lint -esym(769,ec_parameter_array_on_operator_function)*/
/*lint -esym(769,ec_microsoft_inline_not_allowed_here)*/
/*lint -esym(769,ec_data_member_with_interface_type)*/
/*lint -esym(769,ec_variable_with_interface_type)*/
/*lint -esym(769,ec_parameter_with_interface_type)*/
/*lint -esym(769,ec_return_type_is_interface)*/
/*lint -esym(769,ec_array_of_generic_param)*/
/*lint -esym(769,ec_ptr_handle_or_ref_to_generic_param)*/
/*lint -esym(759,update_base_class_derivation)*/
/*lint -esym(765,update_base_class_derivation)*/
/*lint -esym(759,adjust_deletion_counts)*/
/*lint -esym(765,adjust_deletion_counts)*/
/*lint -esym(755,is_field_node)*/
/*lint -esym(769,ec_ref_class_initonly_field)*/
/*lint -esym(769,ec_ref_bound_to_initonly_field)*/
/*lint -esym(769,ec_address_of_initonly_field)*/
/*lint -esym(769,ec_modification_of_initonly_field)*/
/*lint -esym(769,ec_modification_of_static_initonly_field)*/
/*lint -esym(769,ec_member_function_call_on_initonly_field)*/
/*lint -esym(769,ec_expr_not_pointer_nor_handle)*/
/*lint -esym(769,ec_generic_selection_with_points_to)*/
/*lint -esym(769,ec_invalid_specific_ref_class_base)*/
/*lint -esym(769,ec_generic_class_must_be_managed)*/
/*lint -esym(769,ec_sealed_constraint)*/
/*lint -esym(769,ec_dynamic_cast_to_value_generic)*/
/*lint -esym(769,ec_override_with_constraint_mismatch)*/
/*lint -esym(769,ec_deprecated_access_specifier)*/
/*lint -esym(769,ec_static_accessor_in_nonstatic_property_or_event)*/
/*lint -esym(769,ec_invalid_type_constraint)*/
/*lint -esym(769,ec_both_ref_and_value_constraints)*/
/*lint -esym(769,ec_circular_constraints)*/
/*lint -esym(769,ec_invalid_generic_arg)*/
/*lint -esym(769,ec_bad_assembly_info_attribute)*/
/*lint -esym(769,ec_ref_class_not_satisfied)*/
/*lint -esym(769,ec_value_class_not_satisfied)*/
/*lint -esym(769,ec_gcnew_and_abstract)*/
/*lint -esym(769,ec_gcnew_and_no_ctor)*/
/*lint -esym(769,ec_gcnew_and_no_gcnew)*/
/*lint -esym(769,ec_type_not_satisfied)*/
/*lint -esym(769,ec_constraint_mismatch)*/
/*lint -esym(769,ec_standard_array_member_in_managed_class)*/
/*lint -esym(769,ec_handle_member_in_standard_class)*/
/*lint -esym(769,ec_tracking_reference_member_in_standard_class)*/
/*lint -esym(769,ec_reinterpret_cast_of_handle)*/
/*lint -esym(769,ec_generic_type_in_template_arg)*/
/*lint -esym(769,ec_expr_not_pointer_or_array_handle)*/
/*lint -esym(769,ec_unrecognized_ms_attr)*/
/*lint -esym(769,ec_standard_class_member_in_managed_class)*/
/*lint -esym(769,ec_ref_or_interface_class_member_in_standard_class)*/
/*lint -esym(769,ec_template_delegate)*/
/*lint -esym(769,ec_invalid_generic_specialization)*/
/*lint -esym(769,ec_generic_in_template)*/
/*lint -esym(769,ec_template_in_generic)*/
/*lint -esym(769,ec_static_literal_field)*/
/*lint -esym(769,ec_standard_class_nested_in_managed_class)*/
/*lint -esym(769,ec_clrcall_requires_cppcli)*/
/*lint -esym(769,ec_vararg_clrcall)*/
/*lint -esym(769,ec_bad_use_of_function_modifier)*/
/*lint -esym(769,ec_override_with_trivial_property_or_event)*/
/*lint -esym(552,scanning_macro_name)*/
/*lint -esym(769,ec_exp_id_in_for_each_decl)*/
/*lint -esym(769,ec_missing_notequal_on_for_each_type)*/
/*lint -esym(769,ec_missing_incr_on_for_each_type)*/
/*lint -esym(769,ec_missing_indirect_on_for_each_type)*/
/*lint -esym(769,ec_nonpublic_implicit_interface_match)*/
/*lint -esym(769,ec_managed_member_function_cannot_have_ellipsis_parameter)*/
/*lint -esym(769,ec_invalid_prev_decl_iterator)*/
/*lint -esym(769,ec_generic_parameter_requires_clrcall)*/
/*lint -esym(769,ec_generic_parameter_does_not_permit_varargs)*/
/*lint -esym(759,set_up_overload_set_traversal*/
/*lint -esym(765,set_up_overload_set_traversal*/
/*lint -esym(759,next_symbol_in_overload_set*/
/*lint -esym(765,next_symbol_in_overload_set*/
/*lint -esym(769,ec_virtual_required_for_base_override)*/
/*lint -esym(769,ec_virtual_required_for_interface_implementation)*/
/*lint -esym(769,ec_initonly_volatile_not_allowed)*/
/*lint -esym(769,ec_member_name_reserved_by_cli_operator)*/
/*lint -esym(769,ec_tracking_ref_to_constant)*/
/*lint -esym(769,ec_tracking_reference_to_system_string)*/
/*lint -esym(769,ec_use_of_generic_class_with_pending_constraint)*/
/*lint -esym(769,ec_invalid_entity_for_pending_constraint)*/
/*lint -esym(769,ec_template_in_managed_class)*/
/*lint -esym(769,ec_interface_cannot_have_member_generics)*/
/*lint -esym(769,ec_class_metadata_not_representable)*/
/*lint -esym(769,ec_exp_ellipsis)*/
/*lint -esym(769,ec_implements_requires_interface)*/
/*lint -esym(769,ec_implements_must_precede_virtual_functions)*/
/*lint -esym(769,ec_missing_implements_list)*/
/*lint -esym(769,ec_unused_dereference_of_ref_class)*/
/*lint -esym(769,ec_using_or_access_declaration_in_managed_class)*/
/*lint -esym(769,ec_cli_abstract_member_function_definition)*/
/*lint -esym(769,ec_nonstatic_addressof_operator_in_managed_class)*/
/*lint -esym(769,ec_interface_nonstatic_data_member)*/
/*lint -esym(769,ec_pure_specifier_on_sealed_member)*/
/*lint -esym(769,ec_final_managed_class)*/
/*lint -esym(769,ec_cast_to_cli_interface_class)*/
/*lint -esym(769,ec_new_of_cli_interface_class)*/
/*lint -esym(769,ec_enum_type_replacement)*/
/*lint -esym(769,ec_ptr_to_member_of_handle_type)*/
/*lint -esym(769,ec_arrow_star_operator_in_managed_class)*/
/*lint -esym(769,ec_bad_assembly_index)*/
/*lint -esym(769,ec_attribute_conflict)*/
/*lint -esym(759,free_attachments_to_operand)*/
/*lint -esym(765,free_attachments_to_operand)*/
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if !MINIMAL_INLINING
/*lint -esym(769,ec_too_large_to_inline)*/
#endif /* !MINIMAL_INLINING */
#if !USER_CONTROL_OF_STRUCT_PACKING
/*lint -esym(769,ec_exp_rparen_and_pragma_ignored)*/
#endif /* !USER_CONTROL_OF_STRUCT_PACKING */
#if GNU_EXTENSIONS_ALLOWED && !BACK_END_IS_CP_GEN_BE
/*lint -esym(759,is_transparent_union_type)*/
/*lint -esym(765,is_transparent_union_type)*/
#endif /* GNU_EXTENSIONS_ALLOWED && !BACK_END_IS_CP_GEN_BE */
#if !EXTRA_SOURCE_POSITIONS_IN_IL
/*lint -esym(714,is_effective_diagnostic)*/
/*lint -esym(759,is_effective_diagnostic)*/
/*lint -esym(765,is_effective_diagnostic)*/
/*lint -esym(769,ec_unsequenced_use_of_variable)*/
#endif /* !EXTRA_SOURCE_POSITIONS_IN_IL */

#endif /* ifndef LINT_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2012 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
