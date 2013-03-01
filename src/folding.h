/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2012 Edison Design Group Inc.                   [_]          *
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
                            an_error_code         *error_detected,
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
                             an_error_code         *error_detected,
                             a_source_position     *err_pos);

extern void check_shift_count(a_constant    *shift_count_constant,
                              a_type_ptr    operand_type,
                              an_error_code *err_code);

extern void conv_integer_to_integer(a_constant        *old_constant,
                                    a_constant        *new_constant,
                                    a_boolean         is_implicit_cast,
                                    an_error_code     *err_code,
                                    an_error_severity *err_severity);

extern void conv_float_to_integer(a_constant        *old_constant,
                                  a_constant        *new_constant,
                                  an_error_code     *err_code,
                                  an_error_severity *err_severity,
                                  a_boolean         *depends_on_fp_mode,
                                  a_boolean         constant_context);
extern
void type_change_constant_full(a_constant        *constant,
                               a_type_ptr        new_type,
                               a_boolean         is_implicit_cast,
                               a_boolean         constant_context,
                               a_boolean         evaluated_context,
                               a_boolean         fold_constant_addr_exprs,
                               a_boolean         check_cast_access,
                               a_boolean         check_ambiguity,
                               a_boolean         is_reinterpret_cast,
                               a_boolean         maintain_expression,
                               a_boolean         *did_not_fold,
                               an_error_code     *error_detected,
                               a_source_position *err_pos);

extern void type_change_constant(a_constant        *constant,
                                 a_type_ptr        new_type,
                                 a_boolean         is_implicit_cast,
                                 a_boolean         maintain_expression,
                                 a_boolean         *did_not_fold,
                                 a_source_position *err_pos);

extern a_boolean is_false_constant(a_constant *constant);

extern a_boolean is_null_pointer_constant(a_constant *constant);

extern a_boolean is_or_might_be_null_pointer_constant(a_constant *constant);

extern void fold_base_class_cast(a_constant        *constant_1,
                                 a_base_class      *bcp,
                                 a_type_ptr        qualifiers_model,
                                 a_constant        *result,
                                 a_boolean         check_cast_access,
                                 a_boolean         check_ambiguity,
                                 a_boolean         is_implicit_cast,
                                 a_boolean         is_object_pointer,
                                 a_boolean         *did_not_fold,
                                 a_source_position *err_pos,
                                 an_error_code     *error_detected);

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

/*
Entry used to record a remapping from a parameter variable to an argument 
value for the constexpr evaluation process.
*/
typedef struct a_constexpr_remap *a_constexpr_remap_ptr;
typedef struct a_constexpr_remap {
  a_constexpr_remap_ptr
		next;
			/* Next entry on the list, or NULL if this is the
			   last. */
  a_variable_ptr
		param_var;
			/* A parameter variable to be remapped. */
  an_expr_node_ptr
		arg_expr;
			/* The corresponding argument expression. */
  a_byte_boolean
		is_constant;
			/* TRUE if the argument is constant and the
			   constant value is stored in constant_value below. */
  a_constant	constant_value;
			/* The constant value of the argument, if is_constant
			   is TRUE. */
} a_constexpr_remap;

/*
Context information to be carried around within a constexpr evaluation.
*/
typedef struct a_constexpr_evaluation_block {
  a_constexpr_remap_ptr
		remap_list;
			/* List of remappings of parameter variables to
			   argument values for a constexpr call. */
  a_source_position
		source_position;
			/* Default source position for errors if we have
			   nothing more specific. */
  a_byte_boolean
		do_not_call_back;
			/* Set for calls from fold_expr/fold_lvalue_expr to
			   constant_lvalue_address_full/
			   constant_rvalue_pointer_full and vice-versa, to
			   prevent a call back (and infinite recursion) on
			   the current expression node (but not its
			   subtree). */
  a_byte_boolean
		is_partially_initialized;
			/* TRUE if the aggregate constant generated for
			   a constexpr call or construction does not
			   explicitly initialize some member (probably
			   not at the top level). */
  unsigned long
		call_depth;
			/* Depth of constexpr calls, used to check for
			   recursion overflow. */
  unsigned long
		call_count;
			/* Count of constexpr calls, used to check for
			   recursion overflow. */
  an_error_code
		failure_warning;
			/* If not ec_no_error, gives the reason for a folding
			   failure.  The error code must have no fill-ins. */
} a_constexpr_evaluation_block;

/*
Options for constant_lvalue_address and constant_rvalue_pointer.
*/
typedef int a_constant_address_option_set;
#define CAO_NONE ((a_constant_address_option_set)0x0)
#define CAO_TREAT_LOCAL_VAR_ADDR_AS_CONSTANT \
                            ((a_constant_address_option_set)0x1)
			/* Pretend that local auto variables have
			   constant addresses.  This is used for a gcc
			   folding trick.  Note that address constants created
			   with this option might be invalid and therefore
			   one should be careful not to preserve them in the
			   final IL. */

extern a_boolean constant_lvalue_address(an_expr_node_ptr expr,
                                         a_constant       *con,
                                         a_boolean        address_escapes);

extern a_boolean constant_rvalue_pointer_full(
                             an_expr_node_ptr              expr,
                             a_constexpr_evaluation_block  *ceblock,
                             a_constant                    *con,
                             a_boolean                     address_escapes,
                             a_constant_address_option_set options,
                             a_boolean                     *template_constant);

extern a_boolean constant_rvalue_pointer(an_expr_node_ptr expr,
                                         a_constant       *con,
                                         a_boolean        address_escapes);

extern a_boolean constant_is_pointer_to_string_literal(a_constant *con,
                                                       a_constant **scon);

extern a_boolean expr_is_pointer_to_string_literal(an_expr_node_ptr expr,
                                                   a_constant       **scon);

extern a_constant_ptr constant_value_at_address(a_constant_ptr addr_con,
                                                a_constant_ptr target_con);

extern a_constant_ptr constant_value_addressed_by_node(an_expr_node_ptr  expr,
                                                       a_source_position *pos);

extern void fold_builtin_operation_if_possible(
                                        an_expr_node_ptr   expr,
                                        a_constant_ptr     constant,
                                        a_boolean          maintain_expression,
                                        a_source_position  *pos,
                                        a_boolean          *not_a_constant);

#if GNU_EXTENSIONS_ALLOWED
extern a_boolean fold_bit_count_operation_if_possible(
                                               a_routine_ptr     rp,
                                               an_expr_node_ptr  arg,
                                               a_constant        *result_con);

extern a_boolean fold_fptest_if_possible(a_routine_ptr     rp,
                                         an_expr_node_ptr  arg,
                                         a_constant        *result_con);

extern a_boolean fold_pow_if_possible(a_constant_ptr  base,
                                      a_constant_ptr  exp,
                                      a_constant_ptr  result,
                                      a_type_ptr      result_type);
#endif /* GNU_EXTENSIONS_ALLOWED */

#if GNU_BUILTIN_SYNC_FUNCTIONS_ALLOWED
extern a_boolean fold_lock_free_query_if_possible(
                                         a_builtin_function_kind  bfk,
                                         an_expr_node_ptr         size_arg,
                                         an_expr_node_ptr         ptr_arg,
                                         a_constant_ptr           result,
                                         a_type_ptr               result_type);
#endif /* GNU_BUILTIN_SYNC_FUNCTIONS_ALLOWED */

extern a_boolean fold_constexpr_expr(an_expr_node_ptr  expr,
                                     a_source_position *pos,
                                     a_constant        *result_con);

extern a_boolean fold_constexpr_dynamic_init(a_dynamic_init_ptr dip,
                                             a_type_ptr         dest_type,
                                             a_source_position  *pos,
                                             a_constant         *result_con);

extern a_boolean fold_constexpr_call(an_expr_node_ptr  call_expr,
                                     a_boolean         record_backing_expr,
                                     a_source_position *pos,
                                     a_constant        *result_con,
                                     a_boolean         *returns_reference,
                                     an_error_code     *failure_warning);

extern void add_temp_init_backing_expression(a_constant         *con,
                                             a_dynamic_init_ptr dip);
extern
a_boolean fold_constexpr_ctor(a_dynamic_init_ptr ctor_dip,
                              a_boolean          record_backing_expr,
                              a_source_position  *pos,
                              a_constant         *result_con,
                              a_boolean          *partially_initialized);

extern
a_boolean fold_constexpr_member_selection(an_expr_node_ptr  expr,
                                          a_constant        *result_con,
                                          a_source_position *pos);

#if DEBUG
extern unsigned long db_show_folding_fe_space_used(unsigned long grand_total);
#endif /* DEBUG */

extern void folding_one_time_init(void);

extern void folding_init(void);

#endif /* ifndef FOLDING_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2012 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
