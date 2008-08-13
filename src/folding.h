/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2007 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

folding.h -- Declarations relating to folding operations.

*/

/* Avoid including these declarations more than once: */
#ifndef FOLDING_H
#define FOLDING_H 1

#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */

extern a_boolean variable_has_non_null_address(a_variable_ptr vp);

extern a_boolean routine_has_non_null_address(a_routine_ptr rp);

extern a_boolean constant_bool_value_known_at_compile_time(a_constant_ptr con);

extern void make_template_param_expr_constant(an_expr_node_ptr node,
                                              a_constant       *con);

extern void make_template_param_cast_constant(a_constant  *old_constant,
                                              a_constant  *new_constant,
                                              a_type_ptr  new_type,
                                              a_boolean   is_explicit);

extern void implicit_cast(a_constant_ptr cp,
                          a_type_ptr     new_type);

extern void unary_operation(an_expr_operator_kind op,
                            a_constant            *constant,
                            a_type_ptr            result_type,
                            a_constant            *result,
                            a_boolean             constant_context,
                            a_boolean             evaluated_context,
                            a_boolean             *did_not_fold,
                            a_boolean             *template_constant,
                            a_source_position     *err_pos);

extern void binary_operation(an_expr_operator_kind op,
                             a_constant            *constant_1,
                             a_constant            *constant_2,
                             a_type_ptr            result_type,
                             a_constant            *result,
                             a_boolean             constant_context,
                             a_boolean             evaluated_context,
                             a_boolean             *did_not_fold,
                             a_boolean             *template_constant,
                             a_source_position     *err_pos);

extern void check_shift_count(a_constant    *shift_count_constant,
                              a_type_ptr    operand_type,
                              an_error_code *err_code);

extern void conv_integer_to_integer(a_constant        *old_constant,
                                    a_constant        *new_constant,
                                    a_boolean         is_implicit_cast,
                                    an_error_code     *err_code,
                                    an_error_severity *err_severity);

extern void type_change_constant(a_constant        *constant,
                                 a_type_ptr        new_type,
                                 a_boolean         is_implicit_cast,
                                 a_boolean         constant_context,
                                 a_boolean         evaluated_context,
                                 a_boolean         fold_constant_addr_exprs,
                                 a_boolean         is_reinterpret_cast,
                                 a_boolean         maintain_expression,
                                 a_boolean         *did_not_fold,
                                 a_source_position *err_pos);

extern a_boolean is_false_constant(a_constant *constant);

extern a_boolean is_null_pointer_constant(a_constant *constant);

extern a_boolean is_or_might_be_null_pointer_constant(a_constant *constant);

extern void fold_base_class_cast(a_constant        *constant_1,
                                 a_base_class      *bcp,
                                 a_constant        *result,
                                 a_boolean         check_cast_access,
                                 a_boolean         is_implicit_cast,
                                 a_boolean         is_object_pointer,
                                 a_boolean         *did_not_fold,
                                 a_source_position *err_pos);

extern void get_integer_attributes(a_constant      *cp,
                                   an_integer_kind *ikind,
                                   a_boolean       *is_signed,
                                   int             *bit_size);

extern void trunc_and_set_integer(an_integer_value  *result_value,
                                  a_constant        *result,
                                  a_boolean         check_overflow,
				  a_boolean	    saturate_on_overflow,
                                  an_error_code     *err_code,
                                  an_error_severity *err_severity);

extern a_boolean constant_lvalue_address(an_expr_node_ptr expr,
                                         a_constant       *con,
                                         a_boolean        address_escapes,
                                         a_boolean        *template_constant);

extern a_boolean constant_rvalue_pointer(an_expr_node_ptr expr,
                                         a_constant       *con,
                                         a_boolean        address_escapes,
                                         a_boolean        *template_constant);

extern a_boolean constant_is_pointer_to_string_literal(a_constant *con,
                                                       a_constant **scon);

extern a_boolean expr_is_pointer_to_string_literal(an_expr_node_ptr expr,
                                                   a_constant       **scon);

extern void fold_builtin_operation_if_possible(
                                        an_expr_node_ptr   expr,
                                        a_constant_ptr     constant,
                                        a_boolean          maintain_expression,
                                        a_source_position  *pos);

#endif /* ifndef FOLDING_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2007 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
