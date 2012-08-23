/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2011 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

il_alloc.h -- Declarations related to allocation of intermediate language
              entries.

*/

/* Avoid including these declarations more than once. */
#ifndef IL_ALLOC_H
#define IL_ALLOC_H 1

extern char *alloc_il(sizeof_t size);

extern char *alloc_primary_file_scope_il(sizeof_t size);

#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
a_scope_orphaned_list_header_ptr alloc_scope_orphaned_list_header(
                                                 a_routine_ptr   assoc_routine,
                                                 a_scope_number  scope_number);
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */

extern a_source_file_ptr alloc_source_file(void);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_cli_metadata_file_ptr alloc_cli_metadata_file(void);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if ONE_INSTANTIATION_PER_OBJECT
extern a_per_instantiation_needed_flags_entry_ptr
           alloc_per_instantiation_needed_flags_entry(a_boolean at_file_scope);
#endif /* ONE_INSTANTIATION_PER_OBJECT */

extern void set_template_param_constant_kind(
                                      a_constant                     *cp,
                                      a_template_param_constant_kind kind);

extern void set_constant_kind(a_constant           *cp,
                              a_constant_repr_kind kind);

extern void clear_constant(a_constant           *cp,
                           a_constant_repr_kind kind);

extern a_constant_ptr alloc_constant(a_constant_repr_kind kind);

extern a_constant_ptr fs_constant(a_constant_repr_kind kind);

extern a_param_type_ptr alloc_param_type(a_type_ptr type);

extern a_derivation_step_ptr alloc_derivation_step(void);

extern a_base_class_derivation_ptr alloc_base_class_derivation(void);

#if DO_IL_LOWERING && IA64_ABI
extern a_vcall_offset_entry_ptr alloc_vcall_offset_entry(void);
#endif /* DO_IL_LOWERING && IA64_ABI */

extern an_overriding_virtual_function_ptr
                                       alloc_overriding_virtual_function(void);

extern a_template_arg_ptr alloc_template_arg(a_templ_arg_kind	kind);

extern void free_template_arg_list(a_template_arg_ptr  tap);

extern a_base_class_ptr alloc_base_class(void);

extern a_class_list_entry_ptr alloc_list_entry_for_class_full(
                                                 a_source_correspondence *scp);

extern a_class_list_entry_ptr alloc_list_entry_for_class(void);

extern a_routine_list_entry_ptr alloc_list_entry_for_routine(void);

extern a_based_type_list_member_ptr alloc_based_type_list_member(
                                               a_based_type_kind  kind,
                                               a_type_ptr         base_type);

extern void clear_class_type_definition_fields(a_type_ptr  class_type);

extern void set_type_kind(a_type_ptr  pte,
                          a_type_kind kind);

extern void clear_type(a_type_ptr  pte,
                       a_type_kind kind);

extern a_type_ptr alloc_type(a_type_kind kind);

extern void set_dynamic_init_kind(a_dynamic_init_ptr  dip,
                                  a_dynamic_init_kind kind);

extern a_dynamic_init_ptr alloc_dynamic_init(a_dynamic_init_kind kind);

extern a_local_static_variable_init_ptr alloc_local_static_variable_init(void);

extern a_vla_dimension_ptr alloc_vla_dimension(void);

extern a_variable_ptr alloc_variable(a_storage_class  storage_class);

extern a_field_ptr alloc_field(void);

extern an_exception_specification_ptr alloc_exception_specification(void);

extern an_exception_specification_type_ptr
                                  alloc_exception_specification_type(void);

extern void set_routine_special_kind(a_routine_ptr           rp,
                                     a_special_function_kind special_kind);

extern a_routine_ptr alloc_routine(void);

extern an_asm_entry_ptr alloc_asm_entry(void);

#if ASM_SUPPORT_NEEDED
extern char *alloc_asm_function_body(sizeof_t  len);
#endif /* ASM_SUPPORT_NEEDED */

#if MICROSOFT_EXTENSIONS_ALLOWED
extern an_ms_attribute_ptr alloc_ms_attribute(void);

extern
an_ms_attribute_arg_ptr alloc_ms_attribute_arg(an_ms_attribute_arg_kind	kind);

extern a_property_index_type_ptr alloc_property_index_type(void);

extern a_property_or_event_descr_ptr alloc_property_or_event_descr(void);

extern a_generic_constraint_ptr alloc_generic_constraint(void);

extern
void clear_generic_constraint_clause(a_generic_constraint_clause_ptr gccp);

extern a_generic_constraint_clause_ptr alloc_generic_constraint_clause(void);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
extern an_ms_if_exists_ptr alloc_ms_if_exists(void);
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */

extern a_lambda_ptr alloc_lambda(void);

extern a_lambda_capture_ptr alloc_lambda_capture(void);

extern a_seq_number_lookup_entry_ptr alloc_seq_number_lookup_entry(void);

#if GNU_EXTENSIONS_ALLOWED
#if !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
extern an_asm_operand_constraint_ptr alloc_asm_operand_constraint(
                                            an_asm_operand_constraint_kind ck);
#endif /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */

extern an_asm_operand_ptr alloc_asm_operand(void);

extern a_named_register_list_ptr alloc_named_register_list(void);
#endif /* GNU_EXTENSIONS_ALLOWED */

extern a_label_ptr alloc_label(void);

extern void set_expr_node_kind(an_expr_node_ptr  node,
                               an_expr_node_kind kind);

extern void clear_expr_node(an_expr_node_ptr  node,
                            an_expr_node_kind kind);

extern a_local_expr_node_ref_ptr alloc_local_expr_node_ref(void);

extern an_expr_node_ptr alloc_expr_node(an_expr_node_kind node_kind);

#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
extern void set_lowered_eh_construct_node_kind(
                                             an_expr_node_ptr node,
                                             a_lowered_eh_construct_kind kind);

extern an_expr_node_ptr alloc_lowered_eh_construct_node(
                                             a_lowered_eh_construct_kind kind);
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void set_for_each_loop_kind(a_for_each_loop_ptr     felp,
                                   a_for_each_pattern_kind kind);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_switch_case_entry_ptr alloc_switch_case_entry(void);

#if !ABI_CHANGES_FOR_RTTI
extern an_accessible_base_class_ptr alloc_accessible_base_class(
                                                         a_base_class_ptr bcp);
#endif /* !ABI_CHANGES_FOR_RTTI */

extern a_handler_ptr alloc_handler(void);

extern void set_statement_kind(a_statement_ptr  sp,
                               a_statement_kind kind);

extern a_statement_ptr alloc_statement(a_statement_kind stmt_kind);

extern a_constructor_init_ptr alloc_ctor_init(a_constructor_init_kind  kind);

#if GNU_EXTENSIONS_ALLOWED
void clear_gcc_pragma_descr(a_gcc_pragma_descr  *gpd);
#endif /* GNU_EXTENSIONS_ALLOWED */

extern a_pragma_ptr alloc_pragma(a_pragma_kind           kind,
                                 a_source_correspondence *scp);

extern an_object_lifetime_ptr alloc_object_lifetime(
                                               an_object_lifetime_kind  kind);

extern void set_scope_kind(a_scope_ptr    sp,
                           a_scope_kind   kind,
                           a_routine_ptr  assoc_routine);

extern void  clear_namespace(a_namespace_ptr nsp,
                             a_boolean       is_alias);

extern a_namespace_ptr alloc_namespace(a_boolean  is_alias);

extern a_using_decl_ptr alloc_using_decl(void);

extern a_scope_ptr alloc_scope(a_scope_kind   kind,
                               a_scope_number number,
                               a_routine_ptr  assoc_routine);

extern a_local_scope_ref_ptr alloc_local_scope_ref(void);

#if GENERATE_SOURCE_SEQUENCE_LISTS

extern a_source_sequence_entry_ptr alloc_source_sequence_entry(void);

extern a_src_seq_secondary_decl_ptr alloc_src_seq_secondary_decl(void);

extern a_src_seq_end_of_construct_ptr alloc_src_seq_end_of_construct(void);

extern a_src_seq_sublist_ptr alloc_src_seq_sublist(void);

extern an_instantiation_directive_ptr alloc_instantiation_directive(void);

extern a_static_assertion_ptr alloc_static_assertion(void);

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

#if RECORD_HIDDEN_NAMES_IN_IL
extern a_hidden_name_ptr alloc_hidden_name(void);
#endif /* RECORD_HIDDEN_NAMES_IN_IL */

extern a_template_parameter_ptr alloc_template_parameter(void);

extern a_template_decl_ptr alloc_template_decl(void);

extern a_template_ptr alloc_template(void);

#if RECORD_MACROS_IN_IL
extern a_macro_ptr alloc_macro(void);
#endif /* RECORD_MACROS_IN_IL */

#if RECORD_MACRO_INVOCATIONS
extern a_macro_invocation_record_block_ptr alloc_macro_invocation_record_block(
                                                                         void);
#endif /* RECORD_MACRO_INVOCATIONS */

#if EXTRA_SOURCE_POSITIONS_IN_IL
extern void clear_decl_position_supplement(a_decl_position_supplement *dpsp);

extern a_decl_position_supplement_ptr alloc_decl_position_supplement
                                                  (a_boolean  at_file_scope);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

extern a_name_qualifier_ptr alloc_name_qualifier(void);
extern a_name_reference_ptr alloc_name_reference(void);
extern void clear_name_reference(a_name_reference_ptr	nrp);

#if !STANDALONE_UTILITY_PROGRAM

extern char *copy_string_to_region(a_memory_region_number region,
                                   char                   *string);

extern char *copy_string_of_length_to_region(
				      a_memory_region_number region,
				      char                   *string,
				      sizeof_t		     length);

extern char *alloc_text_of_string_literal(sizeof_t size);

#endif /* !STANDALONE_UTILITY_PROGRAM */

extern an_il_entity_list_entry_ptr alloc_il_entity_list_entry(void);

extern an_attribute_ptr alloc_attribute(void);

extern an_attribute_arg_ptr alloc_attribute_arg(void);

extern an_attribute_group_ptr alloc_attribute_group(void);

#if DEBUG
unsigned long show_il_alloc_space_used(unsigned long grand_total);
#endif /* DEBUG */

extern void il_alloc_one_time_init(void);

extern void compute_il_prefix_size(void);

extern void il_alloc_trans_unit_init(void);

extern void il_alloc_init(void);

#ifdef TRACE_ALLOC
void trace_alloc_check(void *ptr);
#endif /* TRACE_ALLOC */

/*
Macro that allocates an entry for the specified type in the file scope
memory region.
*/
#define alloc_il_of_type(type) (type*)alloc_il(sizeof(type))

/*
Macro that allocates an entry for the specified type in the current
memory region.
*/
#define alloc_cil_of_type(type) (type*)alloc_cil(sizeof(type))

#endif /* ifndef IL_ALLOC_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2011 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
