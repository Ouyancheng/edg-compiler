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
			   3.4).  Limited use in C++. */
  /* Non-constant expression kinds: */
  ek_normal,		/* Normal expression, no restrictions. */
  ek_sizeof		/* The operand of sizeof. */
};
/* Define as "a_byte" to explicitly control storage size. */
typedef a_byte an_expression_kind;


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
  ok_sym_for_member,	/* A symbol for a data member or member function
			   that was referenced by qualified name (e.g., A::f),
			   preserved because its address might be taken as a
			   pointer-to-member.  That's guaranteed in the data
			   member case; in the member function case, it's
			   possible (likely, even) that the function will be
			   called instead.  Not used in C. */
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
			   ok_undefined_symbol). */
  an_operand_kind
		kind;
			/* The kind of operand. */
  an_operand_state
		state;
			/* Whether the operand is an lvalue, rvalue, or a
			   function designator. */
  unsigned int	bound_function:1;
			/* TRUE if the operand is a bound function, i.e.,
			   another operand is required to give the object
			   relative to which this function is selected. */
  unsigned int	virtual_function:1;
			/* TRUE if the operand represents a virtual
			   function. */
  unsigned int	is_qualified_name:1;
			/* TRUE if the operand was generated from a qualified
			   name. */
  unsigned int	came_from_reference:1;
			/* For an lvalue, TRUE if the lvalue came from a
			   C++ reference. */
  unsigned int	access_control_error_reported:1;
			/* TRUE if an access control error was reported
			   on the base identifier for this operand.  This
			   remains meaningful only for operands that are
			   essentially still just a representation for
			   an identifier, e.g., ok_indefinite_function and
			   ok_sym_for_member. */
  unsigned int	is_operand_of_address_of:1;
			/* TRUE if this operand is the immediate operand
			   of an "&" address-of operator.  This is
			   significant in that it discriminates between
			   the standard and nonstandard ways of taking
			   a pointer-to-member address of a member function. */
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
    /* When kind == ok_indefinite_function, ok_sym_for_member, or
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
Description of a user-defined conversion, i.e., a conversion using a
constructor or conversion function.  Can also be used (with all fields
at default values) as a description of "no user-defined conversion";
see is_null_user_conv_descr below.  Also used to describe a bitwise
copy of a class in C or C++, which is not really a "user-defined conversion".
Perhaps a more precise way of describing the information contained here
is "anything that isn't just a cast".  If this entry is used in
describing a conversion that is part of a reference initialization, the
conversion is the one done on the initial value to bring it to the
underlying type of the reference, not a conversion to the reference
type itself.
*/
typedef struct a_user_conv_descr *a_user_conv_descr_ptr;
typedef struct a_user_conv_descr {
  a_routine_ptr	routine;
			/* The conversion routine entry.  NULL if
			   class_identity_or_bitwise_copy is TRUE. */
  a_byte_boolean
		class_identity_or_bitwise_copy;
			/* If TRUE, the "conversion" for a class is either
			   identity (the class already has the right type) or
			   is a bitwise copy (substituting for either a
			   copy constructor or an assignment operator).
			   Note that the source type may be a derived class
			   of the destination type. */
  a_byte_boolean
		std_conversion_needed;
			/* If TRUE, a standard conversion is required after
			   the user-defined conversion. */
  a_byte_boolean
		result_is_an_lvalue;
			/* If TRUE, the function returns a reference and the
			   reference should be left as an lvalue rather than
			   converted to an rvalue.  If FALSE, the result is
			   always an lvalue (either originally or after
			   an lvalue-->rvalue conversion).  Note that this
			   is meaningful even when the entry indicates no
			   conversion. */
  a_byte_boolean
		ambiguous;
			/* If TRUE the conversion is ambiguous. */
} a_user_conv_descr;

/*
Macro that tests for a user conversion description that is null, i.e.,
one that describes no conversion to be done.
*/
#define is_null_user_conv_descr(user_conversion)                      \
  ((user_conversion)->routine == NULL &&                              \
   !(user_conversion)->class_identity_or_bitwise_copy)

/*
Macro that returns TRUE if a pointer to a user-defined conversion
is usable (the pointer is non-NULL, and the conversion is not ambiguous).
*/
#define user_conv_usable(user_conversion)                             \
  ((user_conversion) != NULL && !(user_conversion)->ambiguous)


/*
Argument match levels for overloaded function call resolution; See ARM 13.2.
*/
typedef enum /*an_arg_match_level*/ {
  aml_exact,		/* Exact match or trivial conversions. */
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
			/* Match level -- see ARM 13.2. */
  a_byte_boolean
		less_desirable_exact_match;
			/* TRUE if the match is one of the exact match cases
			   indicated as "less desirable", i.e., those that
			   add type qualifiers under references or pointers. */
  a_derivation_step_ptr
		downward_cast_derivation;
			/* If the match involves a standard conversion that
			   is a downward cast, this is the derivation.
			   Otherwise, NULL. */
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
  a_type_ptr    base_param_type;
			/* The underlying type of the parameter, shorn of
			   any top-level reference type and similar
			   trivial-conversion baggage.  Used in looking
			   for conversion subsequences involving addition
			   of type qualifiers at the end of a conversion.
			   NULL if not applicable (e.g., for an ellipsis). */
  a_user_conv_descr
		user_conversion;
			/* If match_level is aml_user_conversion, this
			   describes the user-defined conversion.
			   Note that in one case involving cfront
			   compatibility, no conversion is indicated even
			   through match_level is aml_user_conversion.
			   Also, when passing a class that requires a copy
			   constructor by value, this will indicate the
			   copy constructor even though the match level
			   is aml_std_conversion or aml_exact. */
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
Bit flags used to indicate the kinds of built-in types allowed
for try_to_convert_class_operand_to_builtin_type and
conversion_from_class_possible.
*/
#define BTK_INTEGRAL 0x1
			/* Any integral type. */
#define BTK_FLOATING 0x2
			/* Any floating type. */
#define BTK_POINTER 0x4	/* Any pointer. */
#define BTK_PTR_TO_MEMBER 0x8
			/* Any pointer to member. */
#define BTK_NONE 0
typedef int a_builtin_type_kind_set;


/*
Entry in a stack used during expression processing to record transitions
into substantially disjunct pieces of the expression (e.g., inside a
sizeof is quite different from a normal expression).  These entries are
allocated in the stack.
*/
typedef struct an_expr_stack_entry *an_expr_stack_entry_ptr;
typedef struct an_expr_stack_entry {
  an_expr_stack_entry_ptr
		prev;	/* Previous entry on the stack */
  an_expression_kind
		expression_kind;
			/* The kind of expression. */
  an_xref_entry_ptr
		old_xref_entries_list;
			/* Saved copy of the cross-reference entries list
			   at the time of the push of this entry. */
  a_byte_boolean
		evaluated;
			/* Expression is evaluated, e.g., FALSE if it's the
			   operand of a sizeof. */
  a_byte_boolean
		is_default_arg_expression;
			/* TRUE if the expression is or is inside of a
			   C++ default argument expression in a parameter
			   list. */
  a_byte_boolean
		is_template_arg_expression;
			/* TRUE if the expression is an argument of a C++
			   template reference.  This is only TRUE for the
			   top level major expression for a template
			   argument, e.g., it's not TRUE inside a sizeof
			   inside a template argument. */
  unsigned long	nested_construct_depth;
			/* Number of nested constructs like parentheses
			   begun within this major expression level. */
} an_expr_stack_entry;

EXTERN an_expr_stack_entry_ptr
		expr_stack;
			/* Pointer to the top of the expression stack. */

/*
Macro that returns the current expression kind.
*/
#define curr_expr_kind() (expr_stack->expression_kind)

/*
Macro that tests the current expression kind.
*/
#define curr_expr_kind_is(kind)                                       \
  (curr_expr_kind() == (an_expression_kind)(kind))

/*
Macro that returns TRUE if the current expression kind is some variety of
constant expression.  Note that the "<=" is possible because the constant
kinds are at the beginning of the list.
*/
#define curr_expr_kind_is_const()                                     \
  ((int)(curr_expr_kind()) <= (int)ek_init_constant)

/*
Macro that returns TRUE if the current expression is evaluated.
*/
#define curr_expr_is_evaluated() (expr_stack->evaluated)


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
#define is_sym_for_member_operand(operand)			        \
	((operand)->kind == (an_operand_kind)ok_sym_for_member)

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


extern an_xref_entry_ptr xref_entry(a_symbol_ptr      sym_ptr,
                                    a_source_position *source_position);

extern void change_xref_kinds(an_xref_entry_ptr       xref_list,
                              a_symbol_reference_kind kind);

extern void if_evaluating_mark_routine_referenced(a_routine_ptr     routine,
                                                  a_source_position *position);

extern void push_expr_stack(an_expression_kind      expression_kind,
                            an_expr_stack_entry_ptr new_entry);

extern void pop_expr_stack(void);

extern an_arg_operand_ptr alloc_arg_operand(void);

extern void free_arg_operand_list(an_arg_operand_ptr aop);

extern void free_arg_match_summary_list(an_arg_match_summary_ptr amsp);

extern void issue_warning_from_arg_match_summary(
                                            an_arg_match_summary_ptr amsp,
                                            a_source_position        *err_pos);

extern void determine_arg_match_level(
                               an_operand           *arg_operand,
                               a_type_ptr           arg_type,
                               a_type_ptr           param_type,
                               a_boolean            try_user_conversions,
                               an_arg_match_summary *arg_summary);

extern void selector_match_with_this_param(
                               an_operand           *bound_function_selector,
                               a_boolean            selector_is_object_pointer,
                               a_boolean            conversion_function_case,
                               a_routine_ptr        rout,
                               a_type_ptr           routine_type,
                               an_arg_match_summary *arg_summary);

extern a_symbol_ptr select_overloaded_function(
                           a_symbol_ptr             overloaded_function_symbol,
                           a_boolean                have_selector,
                           an_operand               *bound_function_selector,
                           an_arg_operand_ptr       arg_operand_list,
                           an_error_code            err_none_applies,
                           an_error_code            err_ambiguous,
                           a_source_position        *call_position,
                           an_arg_match_summary_ptr *arg_match_list);


extern void overloaded_function_catch_up(
                                  a_symbol_ptr      function_symbol,
                                  a_symbol_ptr      overloaded_function_symbol,
                                  a_boolean         is_qualified_name,
                                  a_source_position *call_position,
                                  a_boolean         elided_reference,
                                  an_operand        *operand,
                                  a_boolean         *access_error_reported);

extern a_boolean variable_this_exists(a_variable_ptr *this_var);

extern void make_this_variable_operand(a_variable_ptr this_var,
                                       an_operand     *result);

extern a_boolean make_this_pointer_operand(a_symbol_ptr      member_sym,
                                           a_source_position *member_pos,
                                           a_boolean         check_cast_access,
                                           an_operand        *result);

extern void adjust_overloaded_function_call_arguments(
                             a_symbol_ptr             function_symbol,
                             a_boolean                have_selector,
                             an_operand               *bound_function_selector,
                             an_arg_operand_ptr       arg_operand_list,
                             an_arg_match_summary_ptr arg_match_list,
                             an_expr_node_ptr         *arg_expr_list);

extern a_symbol_ptr select_and_prepare_to_call_overloaded_function(
                                 a_symbol_ptr       overloaded_function_symbol,
                                 a_boolean          have_selector,
                                 an_operand         *bound_function_selector,
                                 an_arg_operand_ptr arg_operand_list,
                                 a_boolean          is_qualified_name,
                                 an_error_code      err_none_applies,
                                 an_error_code      err_ambiguous,
                                 a_source_position  *call_position,
                                 an_operand         *function_operand,
                                 an_expr_node_ptr   *arg_expr_list);

extern void try_to_convert_class_operand_to_builtin_type(
                                 an_operand              *operand,
                                 a_builtin_type_kind_set builtin_types_allowed,
                                 a_boolean               *processed);

extern void check_for_operator_overloading(
                                     an_opname_kind    kind,
                                     a_boolean         unary_operator,
                                     a_boolean         must_be_member_function,
                                     a_boolean         try_conversions,
                                     a_boolean         has_predef_meaning,
                                     an_operand        *operand_1,
                                     an_operand        *operand_2,
                                     a_source_position *operator_position,
                                     an_operand        *result,
                                     a_boolean         *processed);

extern void bind_member_function_operand_to_selector(
                                      an_operand *function_operand,
                                      an_operand *bound_function_selector);

extern a_constant_ptr var_constant_value(a_variable_ptr var);

extern void using_lvalue(an_operand *operand);

extern void modifying_lvalue(an_operand *operand);

extern a_boolean is_bit_field_operand(an_operand *operand);

extern void take_address_of_lvalue(an_operand *operand);

extern void conv_object_pointer_to_lvalue(an_operand *operand);

extern void conv_class_operand_to_object_pointer(an_operand *operand);

extern void conv_lvalue_to_rvalue(an_operand *operand);

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

extern void change_binary_operand_types(a_type_ptr type,
				        an_operand *operand_1,
				        an_operand *operand_2);

extern void conv_function_designator_to_ptr_to_function(an_operand *operand);

extern void do_operand_transformations(an_operand                   *operand,
                                       a_transformation_options_set options);

extern a_type_ptr get_logical_result_type(an_operand *operand_1,
				          an_operand *operand_2);

extern a_boolean op_is_zero_constant(an_operand *operand);

extern a_boolean op_is_false_constant(an_operand *operand);

extern void add_reference_indirection(an_operand *result);

extern void make_lvalue_variable_operand(a_variable_ptr    variable,
                                         an_operand        *result,
                                         an_xref_entry_ptr xep);

extern void make_ptr_to_member_constant_operand(
                                    a_symbol_ptr      member_sym,
                                    a_symbol_ptr      member_proj_sym,
                                    a_source_position *position,
                                    a_boolean         check_protected_access,
                                    a_boolean         is_operand_of_address_of,
                                    an_operand        *result);

extern a_boolean check_object_pointer_operand(an_operand    *operand,
                                              an_error_code err_code);

#if PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED
extern a_boolean check_object_or_incomp_array_pointer_operand(
                                                       an_operand    *operand,
                                                       an_error_code err_code,
                                                       an_operand    *otherop);
#endif /* PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED */

extern a_boolean check_arithmetic_operand(an_operand *operand);

extern void make_integer_constant_operand(an_operand *operand,
				          long       value);

extern void promote_operand(an_operand *operand);

extern void arg_default_promote_operand(an_operand *argument_operand);

extern void make_constant_operand(a_constant *constant,
			          an_operand *operand);

extern void make_sym_constant_operand(a_symbol_ptr sym,
                                      an_operand   *operand);

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

extern void make_sym_for_member_operand(a_symbol_ptr      member_sym,
                                        an_xref_entry_ptr xep,
                                        an_operand        *operand);

extern an_expr_node_ptr make_node_from_operand(an_operand *operand);

extern void extract_constant_from_operand(an_operand     *operand,
                                          a_constant_ptr constant);

extern void discard_operand(an_operand *operand);

extern void cast_operand(a_type_ptr new_type,
		         an_operand *operand,
		         a_boolean  is_implicit_cast);

extern void conv_selector_to_object_pointer(an_operand *operand,
                                            a_boolean  *is_arrow_operator);

extern void base_class_cast_operand(an_operand       *operand_1,
                                    a_base_class_ptr bcp,
                                    a_boolean        *is_arrow_operator,
                                    a_boolean        check_cast_access);

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

extern void conv_array_operand_to_pointer_operand(an_operand *operand);

extern void error_and_make_error_operand(an_error_code error_code,
				         an_operand    *operand);

extern void do_binary_operation(an_expr_operator_kind op,
			        an_operand            *operand_1,
			        an_operand            *operand_2,
			        a_type_ptr            type_for_result,
			        an_operand            *result,
			        a_source_position     *operator_position);

extern a_boolean check_boolean_controlling_expr(an_operand *operand);

extern a_boolean still_an_lvalue(a_type_ptr type_before_cast,
			         a_type_ptr type_cast_to);

extern an_expr_operator_kind which_binary_operator(a_token_kind token,
						   a_type_ptr   type);

extern void cast_node(an_expr_node_ptr  *node,
		      a_type_ptr        type,
		      a_boolean         is_implicit_cast,
                      a_source_position *err_pos);

extern a_type_ptr node_type_after_integral_promotion(an_expr_node_ptr node);

extern void integral_promote_node(an_expr_node_ptr *node);

extern void make_constructor_dynamic_init(a_routine_ptr    ctor_routine,
                                          an_expr_node_ptr arg_expr_list,
                                          a_boolean        result_is_addr,
                                          an_operand       *result);

extern a_boolean user_defined_conversion_possible(
                                  an_operand        *source_operand,
                                  a_type_ptr        dest_type,
                                  a_boolean         is_initialization,
                                  a_user_conv_descr *user_conversion,
                                  a_boolean         *failed);

extern void user_convert_operand(an_operand         *operand,
                                 a_type_ptr         dest_type,
                                 a_user_conv_descr  *user_conversion);

extern void prep_elision_initializer_operand(
                                      an_operand       *source_operand,
                                      a_type_ptr       class_type,
                                      a_routine_ptr    *conversion_routine,
                                      an_expr_node_ptr *arg_expr_list,
                                      a_boolean        *class_bitwise_copy);

extern void prep_initializer_operand(
                                  an_operand         *source_operand,
                                  a_type_ptr         dest_type,
                                  a_user_conv_descr  *user_conversion,
                                  a_boolean          initializing_return_value,
                                  an_error_code      incompatible_err);

extern void prep_argument_operand(an_operand         *source_operand,
                                  a_param_type_ptr   formal_param,
                                  a_user_conv_descr  *user_conversion,
                                  an_error_code      err_code);

extern void prep_return_operand(an_operand    *source_operand,
                                a_type_ptr    required_type,
                                an_error_code err_code);

extern void prep_assignment_operand(an_operand        *source_operand,
                                    a_type_ptr        dest_type,
                                    an_error_code     incompatible_err,
                                    a_source_position *err_pos);

#if DEBUG
extern unsigned long show_expr_space_used(void);
#endif /* DEBUG */

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
