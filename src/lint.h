/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1998 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

lint.h -- Lint suppression directives.

Included from basic_hdrs.h in every compilation.

*/

/* Options for FlexeLint. */
/*lint -esym(767,fread_with_check)*/
/*lint -esym(756,a*_dummy_typedef)*/
/* Entities not used in certain configurations: */
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
#if COMPILE_MULTIPLE_SOURCE_FILES
/*lint -esym(769,ec_cl_too_many_arguments)*/
#endif /* COMPILE_MULTIPLE_SOURCE_FILES */
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
#if USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING
/*lint -esym(769,ec_bad_multibyte_char_locale)*/
#endif /* USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1998 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
