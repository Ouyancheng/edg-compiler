/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1995 Edison Design Group Inc.                   [_]          *
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

#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
a_scope_orphaned_list_header_ptr alloc_scope_orphaned_list_header(
                                                 a_routine_ptr   assoc_routine,
                                                 a_scope_number  scope_number);
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */

extern a_source_file_ptr alloc_source_file(void);

extern void set_constant_kind(a_constant           *cp,
                              a_constant_repr_kind kind);

extern void clear_constant(a_constant           *cp,
                           a_constant_repr_kind kind);

extern a_constant_ptr alloc_constant(a_constant_repr_kind kind);

extern a_constant_ptr fs_constant(a_constant_repr_kind kind);

extern a_param_type_ptr alloc_param_type(a_type_ptr type);

extern a_derivation_step_ptr alloc_derivation_step(void);

extern a_base_class_derivation_ptr alloc_base_class_derivation(void);

extern an_overriding_virtual_function_ptr
                                       alloc_overriding_virtual_function(void);

extern a_template_arg_ptr alloc_template_arg(a_boolean is_type_arg);

extern void free_template_arg_list(a_template_arg_ptr  tap);

extern a_template_param_type_descr_ptr alloc_template_param_type_descr(void);

extern a_base_class_ptr alloc_base_class(void);

extern an_access_adjustment_ptr alloc_access_adjustment(an_il_entry_kind kind);

extern a_class_list_entry_ptr alloc_list_entry_for_class(void);

extern a_routine_list_entry_ptr alloc_list_entry_for_routine(void);

extern a_based_type_list_member_ptr
                        alloc_based_type_list_member(a_based_type_kind  kind);

extern void set_type_kind(a_type_ptr  pte,
                          a_type_kind kind);

extern void clear_type(a_type_ptr  pte,
                       a_type_kind kind);

extern a_type_ptr alloc_type(a_type_kind kind);

extern void set_dynamic_init_kind(a_dynamic_init_ptr  dip,
                                  a_dynamic_init_kind kind);

extern void clear_dynamic_init(a_dynamic_init_ptr  dip,
                               a_dynamic_init_kind kind);

extern a_dynamic_init_ptr alloc_dynamic_init(a_dynamic_init_kind kind);

extern a_local_static_variable_init_ptr alloc_local_static_variable_init(void);

extern a_variable_ptr alloc_variable(a_storage_class  storage_class);

extern a_field_ptr alloc_field(void);

extern an_exception_specification_ptr alloc_exception_specification(void);

extern an_exception_specification_type_ptr
                                  alloc_exception_specification_type(void);

extern a_routine_ptr alloc_routine(void);

extern an_asm_entry_ptr alloc_asm_entry(void);

#if ASM_FUNCTION_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
extern char *alloc_asm_function_body(sizeof_t  len);
#endif /* ASM_FUNCTION_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */

extern a_label_ptr alloc_label(void);

extern void set_expr_node_kind(an_expr_node_ptr  node,
                               an_expr_node_kind kind);

extern void clear_expr_node(an_expr_node_ptr  node,
                            an_expr_node_kind kind);

extern an_expr_node_ptr alloc_expr_node(an_expr_node_kind node_kind);

#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
extern an_expr_node_ptr alloc_lowered_eh_construct_node(
                                             a_lowered_eh_construct_kind kind);
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */

extern a_switch_clause_ptr alloc_switch_clause(void);

extern an_accessible_base_class_ptr alloc_accessible_base_class(
                                                         a_base_class_ptr bcp);

extern a_handler_ptr alloc_handler(void);

extern void set_statement_kind(a_statement_ptr  sp,
                               a_statement_kind kind);

extern a_statement_ptr alloc_statement(a_statement_kind stmt_kind);

extern a_constructor_init_ptr alloc_ctor_init(a_constructor_init_kind  kind);

extern a_pragma_ptr alloc_pragma(a_pragma_kind  kind);

extern an_object_lifetime_ptr alloc_object_lifetime(
                                               an_object_lifetime_kind  kind);

extern a_scope_ptr alloc_scope(a_scope_kind   kind,
                               a_scope_number number,
                               a_routine_ptr  assoc_routine);

#if GENERATE_SOURCE_SEQUENCE_LISTS

extern a_source_sequence_entry_ptr alloc_source_sequence_entry(void);

extern a_src_seq_secondary_decl_ptr alloc_src_seq_secondary_decl(void);

extern a_src_seq_end_of_construct_ptr alloc_src_seq_end_of_construct(void);

extern a_src_seq_sublist_ptr alloc_src_seq_sublist(void);

#if COMMENTS_IN_SOURCE_SEQUENCE_LISTS
extern a_comment_ptr alloc_comment(void);
#endif /* COMMENTS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

#if RECORD_HIDDEN_NAMES_IN_IL
extern a_hidden_name_ptr alloc_hidden_name(void);
#endif /* RECORD_HIDDEN_NAMES_IN_IL */

#if RECORD_TEMPLATES_IN_IL
extern a_template_ptr alloc_template(void);
#endif /* RECORD_TEMPLATES_IN_IL */

#if RECORD_MACROS_IN_IL
extern a_macro_ptr alloc_macro(void);
#endif /* RECORD_MACROS_IN_IL */

#if !STANDALONE_UTILITY_PROGRAM

extern char *alloc_text_of_string_literal(sizeof_t size);

#endif /* !STANDALONE_UTILITY_PROGRAM */

extern void il_alloc_one_time_init(void);

extern void il_one_time_init(void);

#endif /* ifndef IL_ALLOC_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1995 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
