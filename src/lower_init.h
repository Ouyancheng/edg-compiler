/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1993 Edison Design Group Inc.                   [_]          *
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

extern void do_ptr_to_data_member_arg_promotion_on_node(an_expr_node_ptr expr);

extern void do_default_arg_promotions_on_node(an_expr_node_ptr expr);

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

extern void lower_dynamic_init(a_dynamic_init_ptr       dip,
                               an_init_pos_descr_ptr    ipdp,
                               an_expr_node_ptr         implied_arg_list,
                               an_expr_node_ptr         end_implied_arg_list,
                               a_constructor_init_ptr   ctor_init,
                               an_insert_location_ptr   insert_location,
                               a_boolean                *keep_dynamic_init);

extern void lower_new_delete(an_expr_node_ptr expr);

extern void lower_temp_init(an_expr_node_ptr expr);

extern void make_ctor_implied_arg_list(a_routine_ptr    ctor_routine,
                                       an_expr_node_ptr *implied_arg_list,
                                       an_expr_node_ptr *end_implied_arg_list);

extern void make_dtor_implied_arg_list(a_routine_ptr    dtor_routine,
                                       a_boolean        have_complete_object,
                                       an_expr_node_ptr *implied_arg_node);

extern void gen_one_destruction(a_dynamic_init_ptr     dip,
                                an_insert_location_ptr insert_location);

extern void add_constructor_wrapper_code(a_scope_ptr        scope,
                                         an_insert_location *insert_location);

extern void lower_constructor_code(a_scope_ptr scope);

extern void lower_destructor_code(a_scope_ptr scope);

extern void lower_stmk_init(a_statement_ptr statement);

extern void lower_file_scope_dynamic_inits(void);

extern void make_code_to_invoke_file_scope_init_routine(void);

extern void init_lower_one_time_init(void);

extern void init_lower_init(void);

#endif /* DO_IL_LOWERING */
#endif /* ifndef LOWER_INIT_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1993 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
