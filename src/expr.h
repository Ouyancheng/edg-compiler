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


extern a_boolean node_has_side_effects(an_expr_node_ptr node);

extern void scan_ctor_arguments(a_symbol_ptr     constructor_sym,
                                an_expr_node_ptr *arg_expr_list,
                                a_routine_ptr    *conversion_routine);

extern an_expr_node_ptr scan_expression(void);

extern an_expr_node_ptr scan_void_expression(void);

extern an_expr_node_ptr scan_required_type_expression(
                                           a_type_ptr    required_type,
                                           a_boolean     allow_top_level_comma,
                                           an_error_code err_code);

extern an_expr_node_ptr scan_return_expression(a_type_ptr    required_type,
                                               an_error_code err_code);

extern void scan_pp_expression(a_constant *constant);

extern void scan_integral_constant_expression(a_constant *constant);

extern void scan_new_array_dimension_expression(a_boolean        *is_constant,
                                                an_expr_node_ptr *expression,
                                                a_constant       *constant);

extern void scan_initializer_expression(a_type_ptr       required_type,
                                        a_boolean        *is_constant,
                                        an_expr_node_ptr *expression,
                                        a_constant       *constant);

extern a_routine_ptr select_default_constructor(a_type_ptr        class_type,
                                                a_source_position *err_pos);

extern a_routine_ptr select_destructor(a_type_ptr class_type);

extern a_symbol_ptr find_copy_constructor(a_type_ptr class_type,
                                          a_boolean  const_object_required,
                                          a_boolean  volatile_object_required,
                                          a_boolean  *ambiguous);

extern a_routine_ptr select_copy_constructor(
                                    a_type_ptr        class_type,
                                    a_boolean         const_object_required,
                                    a_boolean         volatile_object_required,
                                    a_source_position *err_pos);

extern an_expr_node_ptr scan_class_initializer_expression(
                                            a_type_ptr    required_type,
                                            a_routine_ptr *conversion_routine);

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
