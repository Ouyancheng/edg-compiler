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
#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */


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
#define is_const_expr_kind(kind) ((int)(kind) <= (int)ek_init_constant)


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

/* Kinds of operands: */
enum an_operand_kind_tag {
  ok_error,		/* Error. */
  ok_expression,	/* An expression tree. */
  ok_constant,		/* A constant value. */
  ok_indefinite_function,
			/* A function name that's not fully discriminated yet,
			   i.e., a C++ overloaded function.  With state ==
			   os_function_designator, represents the function
			   itself and has very limited lifetime.  With state ==
			   os_rvalue, represents the address of an overloaded
			   function, and survives until it meets a destination
			   type (which selects a specific function) or until
			   used in some other way (which is an error).  Not
			   used in C. */
  ok_sym_for_ptr_to_member,
			/* A symbol for a data member or member function
			   preserved so that its address can be taken as a
			   pointer-to-member.  Has a very limited lifetime.
			   Not used in C. */
  ok_undefined_symbol	/* An undefined symbol encountered while scanning an
			   expression.  Could be an implicit function
			   declaration or a genuine undefined symbol.
			   Has a very limited lifetime. */
};
/* Define as "a_byte" to explicitly control storage size. */
typedef a_byte an_operand_kind;

/* Operand states (lvalue versus rvalue, etc.): */
enum an_operand_state_tag {
  os_none,
  os_lvalue,
  os_rvalue,
  os_function_designator
};
/* Define as "a_byte" to explicitly control storage size. */
typedef a_byte an_operand_state;

/* Data structure used to represent an expression within the expression
   routines: */
typedef struct an_operand {
  a_type_ptr    type;
			/* Type of this operand.  A tk_unknown type if not
			   applicable (ok_indefinite_function,
			   ok_sym_for_ptr_to_member, ok_undefined_symbol). */
  an_operand_kind
		kind;
			/* The kind of operand. */
  an_operand_state
		state;
			/* Whether the operand is an lvalue, rvalue, or a
			   function designator. */
  a_byte_boolean
		bound_function;
			/* TRUE if the operand is a bound function, i.e.,
			   another operand is required to give the object
			   relative to which this function is selected. */
  a_byte_boolean
		virtual_function;
			/* TRUE if the operand represents a virtual
			   function. */
  a_byte_boolean
		is_qualified_name;
			/* TRUE if the operand was generated from a qualified
			   name.  Used only when kind ==
			   ok_indefinite_function. */
  a_source_position
		position;
			/* The source position for the operand. */
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
    /* When kind == ok_indefinite_function, ok_sym_for_ptr_to_member, or
       ok_undefined_symbol: */
    a_symbol_ptr
		symbol;
			/* Pointer to the symbol. */
  } variant;
} an_operand;


/*
Entry describing an actual argument to a function call.  This is basically
an_operand that can be dynamically allocated and linked into a list.
*/
typedef struct an_arg_operand *an_arg_operand_ptr;
typedef struct an_arg_operand {
  an_arg_operand_ptr
		next;	/* Pointer to the next argument, or NULL if this is the
			   last argument. */
  an_operand	operand;
			/* The argument value. */
} an_arg_operand;

/*
Argument match levels for overloaded function call resolution; See ARM 13.2.
*/
typedef enum /*an_arg_match_level*/ {
  aml_exact,		/* Exact match or trivial conversions. */
  aml_exact_qualified,	/* Trivial conversions including removal of a type
			   qualifier from the base type of a reference or
			   pointer. */
  aml_promotion,	/* Match with promotions. */
  aml_std_conversion,	/* Match with standard conversions. */
  aml_user_conversion,	/* Match with user-defined conversions. */
  aml_ellipsis,		/* Match with ellipsis. */
  aml_error,		/* Match with error type (not in ARM). */
  aml_none		/* No match.  Must be last (highest value). */
} an_arg_match_level;

/*
Entry describing how well an actual argument to a function call matches
the corresponding formal parameter.
*/
typedef struct an_arg_match_summary *an_arg_match_summary_ptr;
typedef struct an_arg_match_summary {
  an_arg_match_summary_ptr
		next;	/* Pointer to entry for following argument, or NULL
			   if this is the last argument. */
  an_arg_match_level
		match_level;
			/* Match level -- see ARM 13.2.  Primary key. */
  a_derivation_step_ptr
		downward_cast_derivation;
			/* If match_level == aml_std_conversion and the
			   compatibility involves a downward cast, this is
			   the derivation.  Otherwise, NULL.  Secondary key. */
  a_byte_boolean
		reversed_derivation;
			/* If TRUE, the downward_cast_derivation describes
			   the reverse of the cast performed.  Used for
			   implicit conversions of pointers to members to
			   pointers to members of derived classes. */
  a_byte_boolean
		const_anachronism;
			/* In cfront compatibility mode, TRUE to indicate
			   that the match was possible only because of the
			   anachronism that allows a non-const function to
			   be called for a const object. */
  an_error_code	warning_suggested;
			/* If not ec_no_error, the code for a warning to be
			   issued if this match is chosen. */
} an_arg_match_summary;


/*
Bit flags used in calling do_operand_transformations, to suppress
some of the transformations.
*/
#define TOPT_SUPPRESS_FUNCTION_TO_POINTER_CONVERSION 0x1
			/* Functions should not be converted implicitly to
			   pointer-to-function. */
#define TOPT_SUPPRESS_ARRAY_TO_POINTER_CONVERSION 0x2
			/* Arrays should not be converted implicitly to
			   pointer-to-first-element-of-the-array. */
#define TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION 0x4
			/* Suppress conversion of an lvalue to an rvalue. */
#define TOPT_SUPPRESS_CHECK_FOR_INDEFINITE_FUNCTION 0x8
			/* Suppress the check for indefinite functions. */
#define TOPT_NO_OPTIONS 0
typedef int a_transformation_options_set;


/*
Variable that is TRUE while scanning a default argument expression,
FALSE otherwise.
*/
EXTERN a_boolean
		inside_default_arg_expression;


/* Copy an operand. */
#define copy_operand(from, to) (*(to) = *(from))

/*
Macro that is TRUE if the operand is an error operand.
*/
#define is_error_operand(operand)					\
	((operand)->kind == (an_operand_kind)ok_error ||		\
	 is_error_type((operand)->type))

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
Macro that is TRUE if the operand is an indefinite function operand.
*/
#define is_indefinite_function_operand(operand)				\
	((operand)->kind == (an_operand_kind)ok_indefinite_function)

/*
Macro that is TRUE if the operand is a pointer-to-member symbol operand.
*/
#define is_sym_for_ptr_to_member_operand(operand)			\
	((operand)->kind == (an_operand_kind)ok_sym_for_ptr_to_member)

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


extern void clear_xref_entries_list(an_xref_entry_ptr *old_xref_entries_list);

extern void flush_xref_entries_list(an_xref_entry_ptr old_xref_entries_list);

extern an_xref_entry_ptr xref_entry(a_symbol_ptr            sym_ptr,
                                    a_source_position       *source_position,
                                    an_expression_kind      expression_kind);

extern void change_xref_kinds(an_xref_entry_ptr       xref_list,
                              a_symbol_reference_kind kind);

extern an_arg_operand_ptr alloc_arg_operand(void);

extern void issue_warning_from_arg_match_summary(
                                            an_arg_match_summary_ptr amsp,
                                            a_source_position        *err_pos);

extern void selector_match_with_this_param(
                               an_operand           *bound_function_selector,
                               a_boolean            selector_is_object_pointer,
                               a_boolean            operator_function_case,
                               a_type_ptr           routine_type,
                               an_arg_match_summary *arg_summary);

extern a_symbol_ptr select_overloaded_function(
                           a_symbol_ptr             overloaded_function_symbol,
                           a_boolean                have_selector,
                           an_operand               *bound_function_selector,
                           an_arg_operand_ptr       arg_operand_list,
                           a_boolean                is_qualified_name,
                           an_expression_kind       expression_kind,
                           an_error_code            err_none_applies,
                           an_error_code            err_ambiguous,
                           a_source_position        *call_position,
                           an_operand               *function_operand,
                           an_expr_node_ptr         *arg_expr_list);

extern void try_to_convert_class_operand_to_builtin_type(
                                       an_operand         *operand,
                                       a_boolean          integral_allowed,
                                       a_boolean          floating_allowed,
                                       a_boolean          pointer_allowed,
                                       a_boolean          result_may_be_lvalue,
                                       an_expression_kind expression_kind,
                                       a_boolean          *processed);

extern void check_for_operator_overloading(
                                    an_opname_kind     kind,
                                    a_boolean          unary_operator,
                                    a_boolean          must_be_member_function,
                                    a_boolean          try_conversions,
                                    a_boolean          has_predef_meaning,
                                    an_operand         *operand_1,
                                    an_operand         *operand_2,
                                    an_expression_kind expression_kind,
                                    a_source_position  *operator_position,
                                    an_operand         *result,
                                    a_boolean          *processed);

extern void bind_member_function_operand_to_selector(
                                      an_operand *function_operand,
                                      an_operand *bound_function_selector);

extern a_constant_ptr var_constant_value(a_variable_ptr var);

extern void using_lvalue(an_operand *operand);

extern void modifying_lvalue(an_operand         *operand,
                             an_expression_kind expression_kind);

extern a_boolean is_bit_field_operand(an_operand *operand);

extern void take_address_of_lvalue(an_operand         *operand,
                                   an_expression_kind expression_kind);

extern void conv_object_pointer_to_lvalue(an_operand *operand);

extern void conv_operand_to_object_pointer(an_operand         *operand,
                                           an_expression_kind expression_kind);

extern void conv_lvalue_to_rvalue(an_operand         *operand,
                                  an_expression_kind expresion_kind);

extern a_type_ptr determine_arithmetic_conversions(an_operand *operand_1,
					           an_operand *operand_2);

extern a_boolean check_compatibility_of_pointer_operands(
                   an_operand        *operand_1,
                   an_operand        *operand_2,
                   a_source_position *operator_position,
                   a_boolean         pointer_normalization_standard_in_C,
                   a_boolean         pointers_to_functions_standard_in_C,
                   a_boolean         pointers_to_incomplete_standard_in_C,
                   a_boolean         mixed_object_and_incomplete_standard_in_C,
                   a_type_ptr        *operation_type);

extern a_boolean check_ptr_to_member_operands_for_compatibility(
                                          an_operand        *operand_1,
                                          an_operand        *operand_2,
                                          a_source_position *operator_position,
                                          a_type_ptr        *operation_type);

extern void change_binary_operand_types(a_type_ptr         type,
				        an_operand         *operand_1,
				        an_operand         *operand_2,
                                        an_expression_kind expression_kind);

extern void conv_function_designator_to_ptr_to_function(
                                           an_operand         *operand,
                                           an_expression_kind expression_kind);

extern void do_operand_transformations(
                                an_operand                   *operand,
                                a_transformation_options_set options,
                                an_expression_kind           expression_kind);

extern a_type_ptr get_logical_result_type(an_expression_kind expression_kind,
				          an_operand         *operand_1,
				          an_operand         *operand_2);

extern a_boolean op_is_zero_constant(an_operand *operand);

extern void add_reference_indirection(an_operand *result);

extern void make_lvalue_variable_operand(a_variable_ptr    variable,
                                         an_operand        *result,
                                         an_xref_entry_ptr xep);

extern void make_rvalue_variable_operand(a_variable_ptr variable,
                                         an_operand     *result);

extern void make_ptr_to_member_constant_operand(
                                         a_symbol_ptr      member_proj_sym,
                                         a_source_position *position,
                                         an_operand        *result);

extern a_boolean check_object_pointer_operand(an_operand    *operand,
                                              an_error_code err_code);

extern a_boolean check_arithmetic_operand(an_operand *operand);

extern void make_integer_constant_operand(an_operand *operand,
				          long       value);

extern void promote_operand(an_operand         *operand,
                            an_expression_kind expression_kind);

extern void arg_default_promote_operand(an_operand         *argument_operand,
                                        an_expression_kind expression_kind);

extern void make_constant_operand(a_constant *constant,
			          an_operand *operand);

extern void make_string_constant_operand(a_constant *constant,
                                         an_operand *operand);

extern void clear_operand(an_operand_kind kind,
		          an_operand      *operand);

extern void set_operand_kind(an_operand      *operand,
                             an_operand_kind kind);

extern void error_in_operand(an_error_code error_code,
		             an_operand    *operand);

extern void sym_error_in_operand(an_error_code error_code,
                                 an_operand    *operand,
                                 a_symbol_ptr  sym);

extern void type2_error_in_operand(an_error_code error_code,
                                   an_operand    *operand,
                                   a_type_ptr    type1,
                                   a_type_ptr    type2);

extern a_boolean check_modifiable_lvalue_operand(an_operand *operand);

extern a_boolean check_scalar_operand(an_operand *operand);

extern void make_field_operand(a_field_ptr field,
			       an_operand  *result);

extern void make_function_call(an_expr_node_ptr  function_node,
                               a_type_ptr        function_type,
                               a_boolean         is_virtual,
                               a_boolean         new_or_delete_call_for_array,
                               a_source_position *call_pos,
                               an_operand        *result);

extern void assemble_function_call(an_operand       *function_operand,
                                   an_operand       *bound_function_selector,
                                   an_expr_node_ptr argument_list,
                                   an_operand       *result);

extern a_boolean check_pointer_operand(an_operand    *operand,
				       an_error_code err_code);

extern void make_expression_operand(an_expr_node_ptr node,
                                    a_type_ptr       type,
			            an_operand       *operand);

extern void make_indefinite_function_operand(a_symbol_ptr routine_sym,
                                             a_boolean    is_qualified_name,
                                             an_operand   *operand);

extern void make_sym_for_ptr_to_member_operand(a_symbol_ptr      member_sym,
                                               an_xref_entry_ptr xep,
                                               an_operand        *operand);

extern an_expr_node_ptr make_node_from_operand(an_operand *operand);

extern void extract_constant_from_operand(an_operand     *operand,
                                          a_constant_ptr constant);

extern void discard_operand(an_operand *operand);

extern void cast_operand(a_type_ptr         new_type,
		         an_operand         *operand,
                         an_expression_kind expression_kind,
		         a_boolean          is_implicit_cast);

extern void conv_selector_to_object_pointer(
                                     an_operand         *operand,
                                     a_boolean          *is_arrow_operator,
                                     an_expression_kind expression_kind);

extern void base_class_cast_operand(an_operand         *operand_1,
                                    a_base_class_ptr   bcp,
                                    a_boolean          *is_arrow_operator,
                                    a_boolean          check_cast_access,
                                    an_expression_kind expression_kind);

extern void make_error_operand(an_operand *operand);

extern void conv_to_error_operand(an_operand *operand);

extern a_boolean check_function_pointer_operand(an_operand *operand);

extern void make_function_designator_operand(
                                      a_symbol_ptr      routine_sym,
                                      a_boolean         is_qualified_name,
                                      a_source_position *position,
                                      an_xref_entry_ptr xep,
                                      an_operand        *result);

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

extern void cast_node(an_expr_node_ptr  *node,
		      a_type_ptr        type,
		      a_boolean         is_implicit_cast,
                      a_source_position *err_pos);

extern void integral_promote_node(an_expr_node_ptr *node);

extern void make_constructor_dynamic_init(a_routine_ptr    ctor_routine,
                                          an_expr_node_ptr arg_expr_list,
                                          a_boolean        result_is_addr,
                                          an_operand       *result);

extern a_boolean user_defined_conversion_possible(
                                  an_operand         *source_operand,
                                  a_type_ptr         dest_type,
                                  a_boolean          is_initialization,
                                  a_routine_ptr      *conversion_routine,
                                  a_boolean          *class_bitwise_copy,
                                  a_boolean          *failed);

extern void user_convert_operand(an_operand         *operand,
                                 a_type_ptr         dest_type,
                                 a_boolean          result_may_be_lvalue,
                                 a_routine_ptr      conversion_routine,
                                 a_boolean          class_bitwise_copy,
                                 an_expression_kind expression_kind);

extern void prep_elision_initializer_operand(
                                      an_operand       *source_operand,
                                      a_type_ptr       class_type,
                                      a_routine_ptr    *conversion_routine,
                                      an_expr_node_ptr *arg_expr_list,
                                      a_boolean        *class_bitwise_copy);

extern void prep_initializer_operand(
                                  an_operand         *source_operand,
                                  a_type_ptr         dest_type,
                                  a_boolean          initializing_return_value,
                                  an_expression_kind expression_kind,
                                  an_error_code      incompatible_err);

extern void prep_argument_operand(an_operand         *source_operand,
                                  a_param_type_ptr   formal_param,
                                  an_error_code      err_code,
                                  an_expression_kind expression_kind);

extern void prep_return_operand(an_operand         *source_operand,
                                a_type_ptr         required_type,
                                an_expression_kind expression_kind,
                                an_error_code      err_code);

extern void prep_assignment_operand(an_operand         *source_operand,
                                    a_type_ptr         dest_type,
                                    an_expression_kind expression_kind,
                                    an_error_code      incompatible_err,
                                    a_source_position  *err_pos);

extern void expr_init(void);

#endif /* ifndef EXPRUTIL_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
