/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
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

extern a_boolean node_has_side_effects(an_expr_node_ptr node);

extern a_boolean scan_ctor_arguments(a_symbol_ptr     constructor_sym,
                                     a_routine_ptr    *constr_rout,
                                     an_expr_node_ptr *arg_expr_list);

extern an_expr_node_ptr scan_expression(a_boolean *err);

extern an_expr_node_ptr scan_void_expression(a_boolean *err);

extern void scan_pp_expression(a_constant *constant,
			       a_boolean  *err);

extern void scan_integral_constant_expression(a_constant *constant,
			                      a_boolean  *err);

extern void scan_constant_initializer_expression
	      (a_boolean  convert_array_to_pointer,
	       a_constant *constant,
	       a_boolean  *err);

extern void scan_initializer_expression
	      (a_boolean        convert_array_to_pointer,
               a_boolean        *is_constant,
	       an_expr_node_ptr *expression,
	       a_constant       *constant,
	       a_boolean        *err);

extern an_expr_node_ptr scan_argument_expression(void);

extern a_boolean select_constructor(a_symbol_ptr      constructor_sym,
                                    a_routine_ptr     *constr_rout,
                                    an_expr_node_ptr  *arg_expr_list,
                                    a_source_position *err_pos);

extern an_expr_node_ptr scan_boolean_controlling_expression(a_boolean *err);

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
Macro that is TRUE if the node is a field node.
*/
#define is_field_node(node)						\
	((node)->kind == (an_expr_node_kind)enk_field)

#endif /* ifndef EXPR_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/

