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

folding.h -- Declarations relating to folding operations.

*/

/* Avoid including these declarations more than once: */
#ifndef FOLDING_H
#define FOLDING_H 1

extern void implicit_cast(a_constant_ptr cp,
                          a_type_ptr     new_type);

extern void unary_operation(an_expr_operator_kind op,
		            a_constant            *constant,
		            a_type_ptr            result_type,
			    a_constant            *result,
                            a_boolean             constant_context,
                            a_boolean             *did_not_fold,
			    an_error_code         *err_code,
			    an_error_severity     *err_severity);

extern void binary_operation(an_expr_operator_kind op,
	     	             a_constant            *constant_1,
		             a_constant            *constant_2,
		             a_type_ptr            result_type,
		             a_constant            *result,
                             a_boolean             constant_context,
		             a_boolean             *did_not_fold,
		             an_error_code         *err_code,
		             an_error_severity     *err_severity);

extern void check_shift_count(a_constant    *shift_count_constant,
                              a_type_ptr    operand_type,
                              an_error_code *err_code);

extern void type_change_constant(a_constant        *constant,
			         a_type_ptr        new_type,
				 a_boolean         issue_type_chg_warning,
                                 a_boolean         constant_context,
                                 a_boolean         *did_not_fold,
			         an_error_code     *err_code,
				 an_error_severity *err_severity);

extern a_boolean is_zero_constant(a_constant *constant);

extern a_boolean is_null_pointer_constant(a_constant *constant);

extern void fold_field_selection(a_constant            *constant_1,
                                 a_field_ptr           field,
                                 a_type_ptr            result_type,
                                 a_constant            *result,
                                 a_boolean             *did_not_fold);


extern void fold_base_class_cast(a_constant   *constant_1,
                                 a_base_class *base_class,
                                 a_constant   *result,
                                 a_boolean    *did_not_fold);

extern a_boolean valid_address_constant(a_constant *constant,
                                        a_boolean  *just_past_end);


#endif /* ifndef FOLDING_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
