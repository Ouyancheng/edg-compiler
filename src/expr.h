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

/* Flag bits used to indicate scanning options that apply to one level
   of expression scanning.  These are localized options that indicate
   special handling for an expression because of the context. */
#define EOPT_DISALLOW_COMMA_OPERATOR 0x1
			/* The comma operator should not be allowed at the top
			   level.  Certain contexts suppress the comma because
			   it has another meaning there (e.g., argument
			   lists). */
#define EOPT_OPERAND_OF_CAST 0x2
			/* This expression is the immediate operand of a cast.
			   Floating constants are allowed in integral constant
			   expressions when they are the immediate operand
			   of a cast. */
#define EOPT_OPERAND_OF_ADDRESS_OF 0x4
			/* This expression is the immediate operand of a
			   unary "&" operator.  This is significant when the
			   operand is a qualified name in C++ -- it indicates
			   a pointer-to-member. */
#define EOPT_TRAPPED_LEFT_PAREN 0x8
			/* The caller of scan_expr scanned over a left
			   parenthesis which it turned out should have begun
			   an expression.  scan_expr pretends that there is
			   a left parenthesis preceding the current token. */
#define EOPT_ALLOW_BOUND_FUNCTION 0x10
			/* A C++ bound function may be returned. */
#define EOPT_NO_OPTIONS 0
typedef int a_local_expr_options_set;


extern a_boolean node_has_side_effects(an_expr_node_ptr node,
                                       a_boolean        *suppress_warning);

extern void check_closing_paren_after_expr_list(void);

a_boolean new_or_delete_type_requires_array_handling(a_type_ptr type);

/* scan_expr and scan_expr_full are only intended to be called from within
   the expression-scanning routines, including new expression-handling
   files added by customers.  Note the use of "struct an_operand"
   here to avoid exposing the definition of an_operand to the front end
   at large. */
extern void scan_expr_full(struct an_operand        *result,
                           struct an_operand        *bound_function_selector,
                           int                      prec_level,
                           a_local_expr_options_set local_options);
/* Interface to scan_expr_full for the simple case where a bound function
   cannot be returned. */
#define scan_expr(result, prec_level, local_options)                  \
  scan_expr_full((result), (an_operand *)NULL, (prec_level),          \
                 (local_options))

extern an_expr_node_ptr scan_switch_expression(void);

extern an_expr_node_ptr scan_void_expression(a_boolean repeated_in_loop);

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
                                        a_boolean        static_lifetime,
                                        a_boolean        force_object_lifetime,
                                        a_boolean        *is_constant,
                                        an_expr_node_ptr *expression,
                                        a_constant       *constant);

extern an_expr_node_ptr prep_rvalue_arg_expr(an_expr_node_ptr  expr,
                                             a_param_type_ptr  param,
                                             a_source_position *err_pos);

extern a_boolean scan_class_initializer_expression(
                                              a_type_ptr         required_type,
                                              a_dynamic_init_ptr *dip);

extern void scan_class_parenthesized_initializer(
                                      a_type_ptr         class_type,
                                      a_type_ptr         object_class_type,
                                      a_boolean          force_object_lifetime,
                                      a_source_position  *source_pos,
                                      a_boolean          fill_in_dtor,
                                      a_dynamic_init_ptr *dip);

extern void scan_template_argument_constant_expression(a_type_ptr param_type,
                                                       a_constant *constant);

extern void scan_constant_initializer_expression(a_type_ptr required_type,
                                                 a_constant *constant);

extern an_expr_node_ptr scan_boolean_controlling_expression(
                                                   a_boolean is_condition_expr,
                                                   a_boolean repeated_in_loop);

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
