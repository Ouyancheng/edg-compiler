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

exprutil.h -- Declarations related to expression parsing.

*/

/* Avoid including these declarations more than once: */
#ifndef EXPRUTIL_H
#define EXPRUTIL_H 1

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef EXPR_H
#include "expr.h"
#endif /* ifndef EXPR_H */
#ifndef ERROR_H
#include "error.h"
#endif /* ifndef ERROR_H */

/*
Information used when creating cross-reference information.  This is
done only when f_xref_info != NULL.
*/
EXTERN an_xref_entry_ptr
		avail_xref_entries;
			/* List of cross-reference entries that have been freed
			   and are available for reuse. */
EXTERN an_xref_entry_ptr
		curr_expr_xref_entries;
			/* List of all the cross-reference entries for
			   the current expression.  They are written out at
			   the end of the expression.  Before that, the kind
			   of reference each indicates might be adjusted. */


extern void flush_xref_entries_list(void);

extern an_xref_entry_ptr xref_entry(a_symbol_ptr            sym_ptr,
                                    a_source_position       *source_position,
                                    an_expression_kind      expression_kind);

extern void change_xref_kinds(an_xref_entry_ptr       xref_list,
                              a_symbol_reference_kind kind);

extern void using_lvalue(an_operand *operand);

extern void modifying_lvalue(an_operand         *operand,
                             an_expression_kind expression_kind);

extern a_boolean is_bit_field_operand(an_operand *operand);

extern void take_address_of_lvalue(an_operand         *operand,
                                   an_expression_kind expression_kind);

extern void conv_lvalue_to_rvalue(an_operand         *operand,
                                  an_expression_kind expresion_kind);

extern a_type_ptr determine_arithmetic_conversions(an_operand *operand_1,
					           an_operand *operand_2);

extern void change_binary_operand_types(a_type_ptr         type,
				        an_operand         *operand_1,
				        an_operand         *operand_2,
                                        an_expression_kind expression_kind);

extern void conv_function_designator_to_ptr_to_function(
                                           an_operand         *operand,
                                           an_expression_kind expression_kind);

extern a_type_ptr get_logical_result_type(an_expression_kind expression_kind,
				          an_operand         *operand_1,
				          an_operand         *operand_2);

extern a_boolean op_is_null_pointer_constant(an_operand *operand);

extern a_boolean op_is_zero_constant(an_operand *operand);

extern void make_data_member_operand(a_field_ptr       member,
                                     a_variable_ptr    this_param_variable,
                                     an_operand        *result,
                                     an_xref_entry_ptr xep);

extern void make_lvalue_variable_operand(a_variable_ptr    variable,
                                         an_operand        *result,
                                         an_xref_entry_ptr xep);

extern a_boolean check_object_pointer_operand(an_operand    *operand,
                                              an_error_code err_code);

extern a_boolean check_arithmetic_operand(an_operand *operand);

extern void make_integer_constant_operand(an_operand *operand,
				          long       value);

extern void promote_operand(an_operand         *operand,
                            an_expression_kind expression_kind);

extern void make_constant_operand(a_constant *constant,
			          an_operand *operand);

extern void clear_operand(an_operand_kind kind,
		          an_operand      *operand);

extern a_boolean check_lvalue_operand(an_operand *operand);

extern a_boolean check_scalar_operand(an_operand *operand);

extern void make_field_operand(a_field_ptr field,
			       an_operand  *result);

extern a_boolean check_pointer_operand(an_operand    *operand,
				       an_error_code err_code);

extern void make_expression_operand(an_expr_node_ptr node,
                                    a_type_ptr       type,
			            an_operand       *operand);

extern an_expr_node_ptr make_node_from_operand(an_operand *operand);

extern void cast_operand(a_type_ptr         new_type,
		         an_operand         *operand,
                         an_expression_kind expression_kind,
		         a_boolean          issue_type_chg_warning);

extern void make_error_operand(an_operand *operand);

extern void conv_to_error_operand(an_operand *operand);

extern a_boolean check_function_pointer_operand(an_operand *operand);

extern void make_function_designator_operand(a_routine_ptr     routine,
				             an_operand        *result,
                                             an_xref_entry_ptr xep);

extern void build_unary_result_operand(an_operand            *operand,
                                       an_expr_operator_kind kind,
                                       a_type_ptr            type,
                                       an_operand            *result);

extern void build_binary_result_operand(an_operand            *operand_1,
	       		 	        an_operand            *operand_2,
				        an_expr_operator_kind kind,
				        a_type_ptr            type,
	       			        an_operand            *result);

extern a_boolean check_integral_operand(an_operand *operand);

extern void conv_array_operand_to_pointer_operand(
                                           an_operand *operand,
                                           an_expression_kind expression_kind);

extern void error_and_make_error_operand(an_error_code error_code,
				         an_operand    *operand);

extern void do_binary_operation(an_expr_operator_kind op,
			        an_operand            *operand_1,
			        an_operand            *operand_2,
			        a_type_ptr            type_for_result,
			        an_operand            *result,
			        a_source_position     *operator_position,
                                an_expression_kind    expression_kind);

extern a_boolean check_boolean_controlling_expr(an_operand *operand);

extern a_boolean still_an_lvalue(a_type_ptr type_before_cast,
			         a_type_ptr type_cast_to);

extern an_expr_operator_kind which_binary_operator(a_token_kind token,
						   a_type_ptr   type);

extern void cast_node(an_expr_node_ptr *node,
		      a_type_ptr       type,
		      a_boolean        issue_type_chg_warning);

extern void integral_promote_node(an_expr_node_ptr *node);

extern a_boolean ptr_to_int_cast_okay(a_type_ptr ptr_type,
                                      a_type_ptr int_type);

extern void node_prepare_assignment(an_expr_node_ptr  *right_side_node,
				    a_type_ptr        left_side_type,
                                    an_error_code     incompatible_err,
				    a_boolean         *err);

extern a_type_ptr prepare_assignment_operand(
                                      an_operand         *right_side_operand,
                                      a_type_ptr         left_side_type,
                                      an_expression_kind expression_kind,
                                      an_error_code      incompatible_err,
                                      a_source_position  *err_pos,
                                      a_boolean          *err);

extern void constant_prepare_assignment(a_constant    *constant,
                                        a_type_ptr    left_side_type,
                                        an_error_code incompatible_err,
                                        a_boolean     *err);

extern void error_or_warning(an_error_code     err_code,
			     an_error_severity err_severity,
			     a_boolean         do_pos,
			     a_source_position *position);

extern void error_in_operand(an_error_code error_code,
		             an_operand    *operand);

#endif /* ifndef EXPRUTIL_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/

