/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2013 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

lower_init.h -- Declarations related to lower_init.c (initializations and
                new/delete processing in IL lowering).

*/

/* Avoid including these declarations more than once: */
#ifndef LOWER_INIT_H
#define LOWER_INIT_H 1

/* Only include this code if it is needed: */
#if DO_IL_LOWERING

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_DEF_H */
#ifndef LOWER_IL_H
#include "lower_il.h"
#endif /* ifndef LOWER_IL_H */


typedef struct an_implied_copy_source {
  /* The description of the source for an implied copy operation. */
  a_constructor_init_ptr
                ctor_init;
                        /* When non-NULL, the source of the implied copy
                           is a ctor-initializer. */
  a_lambda_capture_ptr
                capture;
                        /* When non-NULL, the source of the implied copy
                           is a local variable on a lambda capture list.
                           capture points to the current lambda capture on the
                           capture list and is advanced during lowering of
                           the aggregate dynamic initializer that is used to
                           initialize the lambda closure object. */
  a_boolean
                runtime_throw;
                        /* When TRUE indicates that the source of the implied
                           copy is a thrown exception (to be copied at runtime
                           into the parameter of a catch clause). */
} an_implied_copy_source;

extern void clear_implied_copy_source(an_implied_copy_source *source_desc);

EXTERN a_boolean
		processing_file_scope_init_routine;
			/* TRUE while generating the file-scope initialization
			   routine. */

EXTERN a_statement_ptr
		pending_stmk_init_statements;
			/* A list of stmk_init statements that are generated
			   during lowering of various expressions (e.g.,
			   compound literals).  These statements are inserted
			   by calling insert_pending_stmk_init_statements
			   (before the statement whose lowering has generated
			   them) when control returns to the statement
			   level. */

extern void do_ptr_to_data_member_arg_promotion_on_node(an_expr_node_ptr expr);

extern void do_default_arg_promotions_on_node(an_expr_node_ptr expr);

extern a_type_ptr lowered_return_type_of(a_type_ptr routine_type);

#if !IA64_ABI
extern a_routine_ptr make_subobject_destruction_routine(
                                                       a_dynamic_init_ptr dip);

extern a_routine_ptr make_delegation_destruction_routine(
                                                       a_dynamic_init_ptr dip);
#endif /* !IA64_ABI */

extern an_expr_node_ptr make_call_node(a_routine_ptr      routine,
                                       an_expr_node_ptr   arg_list);

extern a_routine_ptr make_runtime_routine(a_const_char  *name,
                                          a_routine_ptr *routine,
                                          a_type_ptr    return_type);

extern a_routine_ptr make_prototyped_runtime_routine(
                                               a_const_char     *name,
                                               a_routine_ptr    *routine,
                                               a_type_ptr       return_type,
                                               a_type_ptr       param1_type,
                                               a_type_ptr       param2_type,
                                               a_type_ptr       param3_type);

extern void make_call_statement(a_routine_ptr      routine,
                                an_expr_node_ptr   arg_list,
                                an_expr_node_ptr   return_value,
                                an_insert_location *insert_location);

extern an_expr_node_ptr make_runtime_rout_call(a_const_char     *name,
                                               a_routine_ptr    *routine,
                                               a_type_ptr       return_type,
                                               an_expr_node_ptr arg_expr_list);

extern void turn_statement_into_noop(a_statement_ptr statement);

extern an_expr_node_ptr zero_cast_to_void(void);

extern void set_var_init_pos_descr(a_variable_ptr        var,
                                   an_init_pos_descr_ptr ipdp);

extern void set_var_indirect_init_pos_descr(a_variable_ptr        var,
                                            an_init_pos_descr_ptr ipdp);
#if !DO_FULL_PORTABLE_EH_LOWERING
extern void set_thrown_object_init_pos_descr(a_type_ptr            throw_type,
                                             an_init_pos_descr_ptr ipdp);
#endif /* !DO_FULL_PORTABLE_EH_LOWERING */

extern a_type_ptr type_from_init_pos_descr(an_init_pos_descr_ptr ipdp);

extern a_destructible_entity_descr_ptr alloc_destructible_entity_descr(void);

extern void free_destructible_entity_descr(
                                         a_destructible_entity_descr_ptr dedp);

extern an_expr_node_ptr make_address_of_init_entity_node(
                                       an_init_pos_descr_ptr ipdp,
                                       a_boolean             using_as_dest);

extern void init_conditional_flag_var(
                             a_destructible_entity_descr_ptr dedp,
                             an_insert_location              *insert_location);

#if ABI_COMPATIBILITY_VERSION > 310 && GENERATE_EH_TABLES
extern
void add_runtime_exception_object_cleanup(an_insert_location *insert_location);
#endif /* ABI_COMPATIBILITY_VERSION > 310 && GENERATE_EH_TABLES */

extern void rewrite_class_assignment_if_necessary(an_expr_node_ptr expr);

extern void lower_constant_init_of_promoted_static(a_variable_ptr variable,
                                                   a_constant_ptr constant);

#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
extern void build_construction_vtbls_pointer(
                            a_destructible_entity_descr_ptr dedp,
                            an_init_pos_descr               *ipdp,
                            an_insert_location_ptr          insert_location,
                            an_expr_node_ptr                *implied_arg_node);

extern an_expr_node_ptr vtbl_addr_from_construction_vtbls_array(
                        a_variable_ptr                  construction_vtbls_var,
                        a_boolean                       var_is_array,
                        a_construction_vtbl_array_index index);
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */

/*
Options for calls of lower_dynamic_init:
*/
typedef int a_lower_dynamic_init_options_set;
#define LDIO_NONE 0		/* No options. */
#define LDIO_FULL_EXPR 0x1
				/* The initialization being lowered is
				   a full expression. */
#define LDIO_THROW 0x2
				/* The initialization being lowered is
				   the top-level initialization for a throw. */

extern void lower_dynamic_init(a_dynamic_init_ptr     dip,
                               an_init_pos_descr_ptr  ipdp,
                               an_implied_copy_source *source_desc,
                               a_variable_ptr         construction_vtbls_var,
                               a_lower_dynamic_init_options_set
                                                      options,
                               a_boolean              others_follow_in_aggr,
                               an_insert_location_ptr insert_location,
                               a_boolean              *keep_dynamic_init,
                               a_constant_ptr         *constant_to_keep);

#if ABI_CHANGES_FOR_PLACEMENT_DELETE

extern void treat_as_placement_new_if_has_default_args(
                                             a_new_delete_supplement_ptr ndsp);

#endif /* ABI_CHANGES_FOR_PLACEMENT_DELETE */

extern void lower_new_delete(an_expr_node_ptr expr);

extern void zero_automatic_temporary(a_variable_ptr   temp_var,
                                     an_expr_node_ptr expr);

extern void lower_temp_init(an_expr_node_ptr expr);

extern void make_ctor_implied_arg_list(a_routine_ptr    ctor_routine,
                                       a_boolean        a_target_ctor,
                                       an_expr_node_ptr *implied_arg_list,
                                       an_expr_node_ptr *end_implied_arg_list);

extern void make_dtor_implied_arg_list(a_routine_ptr    dtor_routine,
                                       a_boolean        have_complete_object,
                                       an_expr_node_ptr *implied_arg_list,
                                       an_expr_node_ptr *end_implied_arg_list);

extern a_boolean need_zeroing_for_value_initialization(a_dynamic_init_ptr dip);

extern void gen_one_destruction(a_dynamic_init_ptr     dip,
                                an_insert_location_ptr insert_location);

#if VLA_DEALLOCATION_REQUIRED
void gen_vla_deallocation(a_dynamic_init_ptr dip,
                          an_insert_location *insert_location);
#endif /* VLA_DEALLOCATION_REQUIRED */

extern void define_default_version_of_routine(
                                            a_routine_ptr    routine,
                                            a_routine_ptr    new_routine,
                                            an_expr_node_ptr default_arg_list);

#if IA64_ABI
extern void define_construction_vtbls_array(a_type_ptr              class_type,
                                            a_variable_ptr          var,
                                            a_construction_vtbl_ptr elements);

extern void set_primary_ctor_or_dtor_kind(a_routine_ptr routine);

extern a_routine_ptr alternate_entry_point(a_routine_ptr       routine,
                                           a_ctor_or_dtor_kind ctor_dtor_kind,
                                           a_boolean           define_now);

extern void create_alternate_entry_points(a_routine_ptr routine,
                                          a_boolean     define_now);
#endif /* IA64_ABI */

extern void add_constructor_wrapper_code(a_scope_ptr        scope,
                                         an_insert_location *insert_location);

extern void lower_constructor_code(a_scope_ptr scope);

extern
void add_function_try_wrapper_code(a_statement_ptr                 statement,
                                   a_destructor_wrapper_info_block *dtor_info);

extern a_label_ptr insert_temp_label(an_insert_location *insert_location);

extern void insert_dtor_member_and_base_destructions(
                              an_insert_location              *insert_location,
                              a_statement_ptr                 insert_block,
                              a_destructor_wrapper_info_block *dtor_info);

extern void lower_destructor_code(a_scope_ptr scope);

extern void lower_stmk_init(a_statement_ptr statement);

extern void insert_pending_stmk_init_statements(a_statement_ptr  statement);

extern void add_to_end_of_pending_stmk_init_statements_list(
                                                        a_statement_ptr  stmt);

extern void add_stmk_init_for_temp_init(a_variable_ptr      var,
                                        a_dynamic_init_ptr  dip);

#if MICROSOFT_EXTENSIONS_ALLOWED
#if LOWER_MICROSOFT_NONCONSTANT_AGGREGATE
extern void lower_microsoft_C_mode_nonconstant_aggregate_init(
                                                    a_variable_ptr  vp,
                                                    a_statement_ptr init_stmt);
#endif /* LOWER_MICROSOFT_NONCONSTANT_AGGREGATE */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if LOWER_DESIGNATED_INITIALIZERS
extern void lower_designated_initializers(a_constant_ptr init_con,
                                          a_dynamic_init *dip,
                                          a_type_ptr     aggr_type);

extern void lower_dynamic_init_designated_initializers(
                                                 a_dynamic_init_ptr dip,
                                                 a_type_ptr         aggr_type);
#endif /* LOWER_DESIGNATED_INITIALIZERS */

extern void lower_file_scope_dynamic_inits(void);

#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
extern void add_body_for_wrapper_routine(a_routine_ptr routine);
#endif /* ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void lower_uuidof(a_constant *con);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void lower_lambda(an_expr_node_ptr expr);

extern void copy_non_static_data_member_initializers_if_necessary(
                                                            a_scope_ptr scope);

extern void remove_unneeded_constructions_and_destructions(a_scope_ptr scope);

extern a_boolean ctor_or_dtor_body_has_no_effect(a_scope_ptr scope);

extern void make_vtbl_address_constant(a_variable_ptr   var,
                                       a_type_ptr       class_type,
                                       a_base_class_ptr bcp,
                                       a_constant       *addr_constant);

extern void init_lower_one_time_init(void);

extern void init_lower_trans_unit_init(void);

extern void init_lower_init(void);

#if USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES
extern a_routine_ptr thread_local_wrapper_for_variable(a_variable_ptr var);
#endif /* USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES */

#endif /* DO_IL_LOWERING */
#endif /* ifndef LOWER_INIT_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2013 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
