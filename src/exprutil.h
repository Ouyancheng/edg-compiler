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
#ifndef ERROR_H
#include "error.h"
#endif /* ifndef ERROR_H */

/* Define the categories of expressions that are allowed. */
enum an_expression_kind_tag {
  /* Note that the constant expression kinds must appear at the beginning
     of the list so that one can determine if an expression is constant
     by a "<=" comparison instead of several "==" comparisons.  See
     is_const_expr_kind below.  ek_init_constant must be the last constant
     expression kind. */
  ek_pp,		/* Preprocessing expression (see 3.8.1). */
  ek_integral_constant,	/* Integral constant expression (see 3.4). */
  ek_init_constant,	/* Constant expression allowed in initializers (see
			   3.4). */
  /* Non-constant expression kinds: */
  ek_normal,		/* Normal expression, no restrictions. */
  ek_not_evaluated	/* Not-evaluated expression, as for example the
			   operand of sizeof. */
};
/* Define as "a_byte" to explicitly control storage size. */
typedef a_byte an_expression_kind;

/*
Macro that returns TRUE if an expression kind is for some variety of
constant expression.  Note that the "<=" is possible because the constant
kinds are at the beginning of the list.
*/
#define is_const_expr_kind(kind)                                      \
  ((kind) <= (an_expression_kind)ek_init_constant)


/* Flag byte used to indicate scanning options that apply to one level
   of expression scanning.  These are localized options that indicate
   special handling for an expression because of the context.
   Each bit indicates an option. */
typedef a_byte a_local_expr_options_set;
#define EOS_NO_LOCAL_OPTIONS 0x0
#define EOS_DISALLOW_COMMA_OPERATOR 0x1
			/* The comma operator should not be allowed at the top
			   level.  Certain contexts suppress the comma because
			   it has another meaning there (e.g., argument
			   lists). */
#define EOS_SUPPRESS_ROUTINE_TO_POINTER_CONVERSION 0x2
			/* Functions should not be converted implicitly to
			   pointer-to-function. */
#define EOS_SUPPRESS_ARRAY_TO_POINTER_CONVERSION 0x4
			/* Arrays should not be converted implicitly to
			   pointer-to-first-element-of-the-array. */
#define EOS_OPERAND_OF_CAST 0x8
			/* This expression is the immediate operand of a cast.
			   Floating constants are allowed in integral constant
			   expressions when they are the immediate operand
			   of a cast. */
#define EOS_TRAPPED_LEFT_PAREN 0x10
			/* The caller of scan_expr scanned over a left
			   parenthesis which it turned out should have begun
			   an expression.  scan_expr pretends that there is
			   a left parenthesis preceding the current token. */

/* Define the values of left and right associativity. */
#define LEFT_ASSOC  TRUE
#define RIGHT_ASSOC FALSE

/*
Information used when creating cross-reference information.  This is
done only when f_xref_info != NULL.
*/
typedef struct an_xref_entry *an_xref_entry_ptr;
typedef struct an_xref_entry {
  /* Describes one reference to a symbol. */
  a_symbol_reference_kind
		kind;	/* Kind of reference (modification, address taken,
			   etc.). */
  an_expression_kind
		expression_kind;
			/* Kind of expression.  Not-evaluated expressions
			   require special handling. */
  a_symbol_ptr	symbol;	/* Pointer to the referenced symbol. */
  a_source_position
		position;
			/* Source location of the reference. */
  an_xref_entry_ptr
		next;
			/* Next entry on the list of cross-reference entries
			   for the current expression, NULL if last. */
  an_xref_entry_ptr
		next_operand_ref;
			/* Next entry on the list of cross-reference entries
			   that apply to one operand, NULL if last. */
} an_xref_entry;

/* Define the kinds of operands there are. */
enum an_operand_kind_tag {
  ok_error,		/* Error. */
  ok_expression,	/* An expression tree. */
  ok_constant,		/* A constant value. */
  ok_undefined_symbol	/* This is a temporary type of operand.  It exists only
			   long enough to enter a default function symbol into
			   the symbol table or detect an error. */
};
/* Define as "a_byte" to explicitly control storage size. */
typedef a_byte an_operand_kind;

/* Define the addressing states the operand can be in. */
enum an_operand_state_tag {
  os_none,
  os_lvalue,
  os_rvalue,
  os_function_designator
};
/* Define as "a_byte" to explicitly control storage size. */
typedef a_byte an_operand_state;

/* Define a local container for passing around expressions or constants. */
typedef struct an_operand {
  a_type_ptr    type;
			/* Type of this operand. */
  an_operand_kind
		kind;
			/* The kind of operand. */
  an_operand_state
		state;
			/* Whether the operand is an lvalue, rvalue, or a
			   function designator. */
  a_source_position
		position;
			/* The source position for a given operand. */
  an_xref_entry_ptr
		xref_entries_list;
			/* A list of cross-reference entries that are
			   related to the operand.  This list includes
			   only entries for entities involved in lvalue
			   address computations, whose reference kinds might
			   be changed once the full context surrounding
			   the operand is known. */
  union {
    /* When kind == ok_error, no variant fields. */
    /* When kind == ok_expression: */
    an_expr_node_ptr
		expression;
    /* When kind == ok_constant: */
    a_constant	constant;
    /* When kind == ok_undefined_symbol: */
    a_symbol_ptr
		symbol;
  } variant;
} an_operand;

#define copy_operand(from, to) (*(to) = *(from))

/*
Macro that is TRUE if the operand is an error operand.
*/
#define is_error_operand(operand)					\
	(((operand)->kind == (an_operand_kind)ok_error) ||		\
	 (is_error_type((operand)->type)))

/*
Macro that is TRUE if the operand is an expression operand.
*/
#define is_expression_operand(operand)					\
	((operand)->kind == (an_operand_kind)ok_expression)

/*
Macro that is TRUE if the operand is a constant operand.
*/
#define is_constant_operand(operand)					\
	((operand)->kind == (an_operand_kind)ok_constant)

/*
Macro that is TRUE if the operand is an undefined symbol operand.
*/
#define is_undefined_symbol_operand(operand)				\
	((operand)->kind == (an_operand_kind)ok_undefined_symbol)

/*
Macro that is TRUE if the operand is an lvalue.  Note that this isn't
precisely the standard definition of an lvalue, as an lvalue is not
allowed to have type void, and this macro returns TRUE for that case.
The difference is compensated for in the cases where it matters.
*/
#define is_an_lvalue(operand)						\
	((operand)->state == (an_operand_state)os_lvalue)

/*
Macro that is TRUE if the operand is an rvalue.
*/
#define is_an_rvalue(operand)						\
	((operand)->state == (an_operand_state)os_rvalue)

/*
Macro that is TRUE if the operand is a function designator.
*/
#define is_a_function_designator(operand)				\
	((operand)->state == (an_operand_state)os_function_designator)

/*
Return TRUE if a variable is a constant identifier usable in
constant expressions.  See ARM 7.1.6.
*/
#define is_const_variable(var)                                          \
	(is_const_qualified_type((var)->type) && is_integral_type((var)->type))

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

extern a_constant_ptr var_constant_value(a_variable_ptr var);

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

