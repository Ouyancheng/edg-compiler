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


EXTERN a_boolean
		processing_file_scope_init_routine;
			/* TRUE while generating the file-scope initialization
			   routine. */

EXTERN a_statement_ptr
		temp_init_statements;
			/* A list of statements that initialize temporaries for
			   lowered compound literals.  These statements are
			   inserted by calling insert_temp_init_statements. */

extern void do_ptr_to_data_member_arg_promotion_on_node(an_expr_node_ptr expr);

extern void do_default_arg_promotions_on_node(an_expr_node_ptr expr);

extern an_expr_node_ptr make_call_node(a_routine_ptr      routine,
                                       an_expr_node_ptr   arg_list,
                                       a_boolean          honor_virtual,
                                       an_insert_location *insert_location);

extern a_routine_ptr make_runtime_routine(char          *name,
                                          a_routine_ptr *routine,
                                          a_type_ptr    return_type);

extern void make_call_statement(a_routine_ptr      routine,
                                an_expr_node_ptr   arg_list,
                                an_insert_location *insert_location);

extern an_expr_node_ptr make_runtime_rout_call(char             *name,
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

extern an_expr_node_ptr make_init_entity_node(
                                       an_init_pos_descr_ptr ipdp,
                                       a_boolean             using_as_address,
                                       a_boolean             using_as_dest);

extern void init_conditional_flag_var(
                             a_destructible_entity_descr_ptr dedp,
                             an_insert_location              *insert_location);

#if LOWER_EXTERN_INLINE
extern void lower_constant_init_of_static_in_extern_inline(
                                                    a_variable_ptr variable,
                                                    a_scope_ptr    scope);
#endif /* LOWER_EXTERN_INLINE */

/*
Options for calls of lower_dynamic_init:
*/
typedef int a_lower_dynamic_init_options_set;
#define LDIO_NONE 0		/* No options. */
#define LDIO_FULL_EXPR 0x1	/* The initialization being lowered is
				   a full expression. */
#define LDIO_THROW 0x2		/* The initialization being lowered is
				   the top-level initialization for a throw. */

extern void lower_dynamic_init(a_dynamic_init_ptr     dip,
                               an_init_pos_descr_ptr  ipdp,
                               an_expr_node_ptr       implied_arg_list,
                               an_expr_node_ptr       end_implied_arg_list,
                               a_constructor_init_ptr ctor_init,
                               a_lower_dynamic_init_options_set
                                                      options,
                               a_boolean              others_follow_in_aggr,
                               an_insert_location_ptr insert_location,
                               a_boolean              *keep_dynamic_init,
                               a_constant_ptr         *constant_to_keep);

extern void lower_new_delete(an_expr_node_ptr expr);

extern void lower_temp_init(an_expr_node_ptr expr);

extern void make_ctor_implied_arg_list(a_routine_ptr    ctor_routine,
                                       an_expr_node_ptr *implied_arg_list,
                                       an_expr_node_ptr *end_implied_arg_list);

extern void make_dtor_implied_arg_list(a_routine_ptr    dtor_routine,
                                       a_boolean        have_complete_object,
                                       an_expr_node_ptr *implied_arg_list,
                                       an_expr_node_ptr *end_implied_arg_list);

extern void gen_one_destruction(a_dynamic_init_ptr     dip,
                                an_insert_location_ptr insert_location);

#if IA64_ABI
extern void define_construction_vtbls_array(a_type_ptr              class_type,
                                            a_variable_ptr          var,
                                            a_construction_vtbl_ptr elements);

extern a_routine_ptr alternate_entry_point(a_routine_ptr       routine,
                                           a_ctor_or_dtor_kind ctor_dtor_kind,
                                           a_boolean           define_now);
#endif /* IA64_ABI */

extern void add_constructor_wrapper_code(a_scope_ptr        scope,
                                         an_insert_location *insert_location);

extern void lower_constructor_code(a_scope_ptr scope);

extern void set_cleanup_state_before_destructor_user_code(
                     an_insert_location              *insert_location,
                     a_destructor_wrapper_info_block *dtor_info);

extern a_label_ptr insert_temp_label(an_insert_location *insert_location);

extern void insert_dtor_member_and_base_destructions(
                              a_statement_ptr                 destruction_code,
                              an_insert_location              *insert_location,
                              a_statement_ptr                 insert_block,
                              a_destructor_wrapper_info_block *dtor_info);

extern void lower_destructor_code(a_scope_ptr scope);

extern void lower_stmk_init(a_statement_ptr statement);

extern void insert_temp_init_statements(a_statement_ptr  statement);

extern void add_to_end_of_temp_init_statements_list(a_statement_ptr  stmt);

extern void add_stmk_init_for_compound_literal(a_variable_ptr      var,
                                               a_dynamic_init_ptr  dip);

#if MICROSOFT_EXTENSIONS_ALLOWED
#if LOWER_MICROSOFT_NONCONSTANT_AGGREGATE
extern void lower_microsoft_C_mode_nonconstant_aggregate_init(
                                                    a_variable_ptr  vp,
                                                    a_statement_ptr init_stmt);
#endif /* LOWER_MICROSOFT_NONCONSTANT_AGGREGATE */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if LOWER_DESIGNATED_INITIALIZERS
extern void lower_designated_initializers(a_constant_ptr init_con);
#endif /* LOWER_DESIGNATED_INITIALIZERS */

extern void lower_file_scope_dynamic_inits(void);

#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
extern void add_body_for_covariant_return_type_entry_routine(
                                                        a_routine_ptr routine);
#endif /* ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void lower_uuidof(a_constant *con);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void init_lower_one_time_init(void);

extern void init_lower_trans_unit_init(void);

extern void init_lower_init(void);

#endif /* DO_IL_LOWERING */
#endif /* ifndef LOWER_INIT_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
