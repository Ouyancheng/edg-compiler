/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

lint.h -- Lint suppression directives.

Included from basic_hdrs.h in every compilation.

*/

/* Options for FlexeLint. */
/*lint -esym(767,fread_with_check)*/
/*lint -esym(756,a*_dummy_typedef)*/
/*lint -esym(755,typeref_is_const_qualified)*/
/*lint -esym(755,typeref_is_volatile_qualified)*/
/*lint -esym(755,typeref_is_restrict_qualified)*/
/* Entities not used in certain configurations: */
/*lint -esym(755,EXTERN_C)*/
/*lint -esym(750,chdir_with_check)*/
/*lint -esym(769,ec_cannot_chdir)*/
/*lint -esym(759,change_non_id_characters)*/
/*lint -esym(765,change_non_id_characters)*/
/*lint -esym(759,type_from_src_seq_declaration)*/
/*lint -esym(765,type_from_src_seq_declaration)*/
/*lint -esym(759,form_type_qualifier)*/
/*lint -esym(765,form_type_qualifier)*/
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
/*lint -esym(759,crc_32)*/
/*lint -esym(765,crc_32)*/
/*lint -esym(714,db_format_integer_value)*/
/*lint -esym(714,db_sym)*/
/*lint -esym(714,db_corresp)*/
/*lint -esym(714,db_stop_tokens)*/
/*lint -esym(714,db_text_buffer)*/
/*lint -esym(714,db_scope_pragmas)*/
/*lint -esym(714,db_translation_unit)*/
/*lint -esym(714,db_translation_unit_stack)*/
/*lint -esym(759,int_kind_name_full)*/
/*lint -esym(765,int_kind_name_full)*/
/*lint -esym(755,expect_error_str)*/
/*lint -esym(755,expect_error_str2)*/
/*lint -esym(755,check_assertion_or_expect_error)*/
/*lint -esym(755,check_assertion_or_expect_error_str)*/
/*lint -esym(755,check_assertion_or_expect_error_str2)*/
/*lint -esym(759, fetch_host_fp_value)*/
/*lint -esym(765, fetch_host_fp_value)*/

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
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
#if USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING
/*lint -esym(769,ec_bad_multibyte_char_locale)*/
#endif /* USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING */
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
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
/*lint -esym(759,add_base_class_casts)*/
/*lint -esym(765,add_base_class_casts)*/
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
/*lint -esym(759,copy_unshared_constant_full)*/
/*lint -esym(765,copy_unshared_constant_full)*/
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
#endif /* !DO_IL_LOWERING */
#if !BACK_END_IS_CP_GEN_BE
/*lint -esym(759,is_address_of_string_constant)*/
/*lint -esym(765,is_address_of_string_constant)*/
/*lint -esym(759,form_unknown_function_constant)*/
/*lint -esym(765,form_unknown_function_constant)*/
/*lint -esym(751,a_template_param_map_level)*/
/*lint -esym(759,form_uuidof_reference)*/
/*lint -esym(765,form_uuidof_reference)*/
#endif /* !BACK_END_IS_CP_GEN_BE */
#if !DEBUG
/*lint -esym(749,pfs_last)*/
#endif /* !DEBUG */
#if !INCLUDE_EDG_TEST_PRAGMAS
/*lint -esym(528,add_other_pragma_kind_description)*/
#endif /* !INCLUDE_EDG_TEST_PRAGMAS */
#ifdef SOLARIS
/* Solaris stdio.h doesn't define fileno in strict mode (fileno is not
   ANSI/ISO C; it's in POSIX). */
extern int fileno(FILE *);
/*lint -esym(526,fileno)*/
#endif /* ifdef SOLARIS */
#ifdef sun
/*lint -esym(526,isnan)*/
/*lint -esym(526,finite)*/
#endif /* ifdef sun */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
