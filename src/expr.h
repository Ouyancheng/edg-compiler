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

expr.h -- Declarations related to expression parsing.

*/

/* Avoid including these declarations more than once: */
#ifndef EXPR_H
#define EXPR_H 1

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */


extern a_boolean node_has_side_effects(an_expr_node_ptr node,
                                       a_boolean        *suppress_warning);

extern void check_closing_paren_after_expr_list(void);

extern void scan_ctor_arguments(a_symbol_ptr       constructor_sym,
                                an_expr_node_ptr   *arg_expr_list,
                                a_routine_ptr      *conversion_routine,
                                a_source_position  *source_pos,
                                a_type_ptr         object_class_type);

a_boolean new_or_delete_type_requires_array_handling(a_type_ptr type);

extern an_expr_node_ptr scan_switch_expression(void);

extern an_expr_node_ptr scan_void_expression(void);

extern void scan_default_arg_expr(a_param_type_ptr ptp);

extern an_expr_node_ptr scan_return_expression(
                                              a_type_ptr         required_type,
                                              an_error_code      err_code,
                                              a_dynamic_init_ptr *dip);

extern void scan_pp_expression(a_constant *constant);

extern void scan_integral_constant_expression(a_constant *constant);

extern void scan_new_array_dimension_expression(a_boolean        *is_constant,
                                                an_expr_node_ptr *expression,
                                                a_constant       *constant);

extern void scan_initializer_expression(a_type_ptr       required_type,
                                        a_boolean        *is_constant,
                                        an_expr_node_ptr *expression,
                                        a_constant       *constant);

extern an_expr_node_ptr prep_rvalue_arg_expr(an_expr_node_ptr  expr,
                                             a_param_type_ptr  param,
                                             a_source_position *err_pos);

extern a_boolean scan_class_initializer_expression(
                                              a_type_ptr         required_type,
                                              a_dynamic_init_ptr *dip);

extern void scan_template_argument_constant_expression(a_type_ptr param_type,
                                                       a_constant *constant);

extern void scan_constant_initializer_expression(a_type_ptr required_type,
                                                 a_constant *constant);

extern an_expr_node_ptr scan_boolean_controlling_expression(void);

/*
Macro that is TRUE if the node is an operation node.
*/
#define is_operation_node(node)						\
	((node)->kind == (an_expr_node_kind)enk_operation)

/*
Macro that is TRUE if the node is a constant node.
*/
#define is_constant_node(node)						\
	((node)->kind == (an_expr_node_kind)enk_constant)

/*
Macro that is TRUE if the node is a variable node.
*/
#define is_variable_node(node)						\
	((node)->kind == (an_expr_node_kind)enk_variable)

/*
Macro that is TRUE if the node is a variable address node.
*/
#define is_variable_address_node(node)					\
	((node)->kind == (an_expr_node_kind)enk_variable_address)

/*
Macro that is TRUE if the node is a routine address node.
*/
#define is_routine_address_node(node)					\
	((node)->kind == (an_expr_node_kind)enk_routine_address)

/*
Macro that is TRUE if the node is a field node.
*/
#define is_field_node(node)						\
	((node)->kind == (an_expr_node_kind)enk_field)

/*
Macro that is TRUE if the node is a error node.
*/
#define is_error_node(node)						\
	((node)->kind == (an_expr_node_kind)enk_error)

#endif /* ifndef EXPR_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
