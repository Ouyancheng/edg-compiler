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
#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */

/* Include of overload.h comes later. */


/* Define the categories of expressions that are allowed. */
enum an_expression_kind_tag {
  /* Note that the constant expression kinds must appear at the beginning
     of the list so that one can determine if an expression is constant
     by a "<=" comparison instead of several "==" comparisons.  See
     curr_expr_kind_is_const below.  ek_init_constant must be the last constant
     expression kind. */
  ek_pp,		/* Preprocessing expression (see 3.8.1). */
  ek_integral_constant,	/* Integral constant expression (see 3.4). */
  ek_template_arg,	/* Nontype template argument (C++). */
  ek_init_constant,	/* Constant expression allowed in initializers (see
			   3.4).  Limited use in C++. */
  /* Non-constant expression kinds: */
  ek_normal,		/* Normal expression, no restrictions. */
  ek_sizeof		/* The operand of sizeof. */
};
/* Define as "a_byte" to explicitly control storage size. */
typedef a_byte an_expression_kind;


/*
Information on a single reference to a symbol.  Used in cases where the kind
of reference can be affected by context and therefore cannot be recorded
immediately.  This entry holds the information that must be retained to be
able to record the reference once the kind of reference is known.
*/
typedef struct a_ref_entry *a_ref_entry_ptr;
typedef struct a_ref_entry {
  /* Describes one reference to a symbol. */
  a_symbol_reference_kind
		kind;	/* Kind of reference (modification, address taken,
			   etc.). */
  a_symbol_ptr	symbol;	/* Pointer to the referenced symbol. */
  a_source_position
		position;
			/* Source location of the reference. */
  a_ref_entry_ptr
		next;
			/* Next entry on the list of reference entries
			   for the current expression, NULL if last. */
  a_ref_entry_ptr
		next_operand_ref;
			/* Next entry on the list of reference entries
			   that apply to one operand, NULL if last. */
} a_ref_entry;

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
  a_ref_entry_ptr
		ref_entries_list;
			/* A list of reference entries that are related
			   to the operand.  This list includes only entries
			   for functions and for entities involved in lvalue
			   address computations, whose reference kinds might
			   be changed once the full context surrounding the
			   operand is known. */
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

/* Include overload.h after an_operand has been defined to avoid circular
   reference problems. */
#ifndef OVERLOAD_H
#include "overload.h"
#endif /* ifndef OVERLOAD_H */

/*
Entry used to record information about a dynamic init entry whose
destructor processing could not be completed when the dynamic init entry
was created because the initialization occurs within the return expression
in a routine that returns its value via a copy constructor.  The destructor
call on the topmost initialization is optimized away, but there's no way to
know that when it is generated, so all such initializations are placed on
a list and checked at the end of the expression.
*/
typedef struct a_dynamic_init_dtor_fixup *a_dynamic_init_dtor_fixup_ptr;
typedef struct a_dynamic_init_dtor_fixup {
  a_dynamic_init_dtor_fixup_ptr
		next;	/* Next entry on the list. */
  a_dynamic_init_ptr
		dynamic_init;
			/* The dynamic initialization entry to be checked. */
  a_source_position
		position;
			/* The source position for errors. */
} a_dynamic_init_dtor_fixup;


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
  a_ref_entry_ptr
		old_ref_entries_list;
			/* Saved copy of the global reference entries list
			   at the time of the push of this entry. */
  a_byte_boolean
		evaluated;
			/* Expression is evaluated, e.g., FALSE if it's the
			   operand of a sizeof or in a "dead" part of a
			   short-circuiting operation. */
  a_byte_boolean
		potentially_evaluated;
			/* Expression is potentially evaluated, e.g., FALSE
			   if it's the operand of a sizeof. */
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
  a_byte_boolean
		in_return_by_cctor_expression;
			/* TRUE if the expression is in a return statement in
			   a routine that returns its value to the caller by
			   calling a copy constructor.  This has an effect
			   on destructor calls noted in dynamic initialization
			   entries for temporaries. */
  a_byte_boolean
		fold_constant_addr_exprs;
			/* TRUE if constant addressing expressions should be
			   folded to constants.  Always TRUE if the expression
			   is a constant expression; sometimes TRUE for
			   nonconstant expressions (e.g., initializer
			   expressions, where it helps in discerning static
			   initialization cases from others). */
  a_dynamic_init_dtor_fixup_ptr
		dynamic_init_dtor_fixup_list;
			/* List of dynamic init entries for which destructor
			   processing was delayed.  Only non-NULL when
			   in_return_by_cctor_expression is TRUE. */
  unsigned long	nested_construct_depth;
			/* Number of nested constructs like parentheses
			   begun within this major expression level. */
} an_expr_stack_entry;

EXTERN an_expr_stack_entry_ptr
		expr_stack;
			/* Pointer to the top of the expression stack. */

EXTERN a_ref_entry_ptr
		curr_expr_ref_entries;
			/* List of all the reference entries for the current
			   expression.  They are recorded at the end of the
			   expression.  Before that, the kind of reference
			   each indicates might be adjusted. */

#if DEBUG
/*
Counts of entries allocated, for debugging purposes.
*/
EXTERN unsigned long
		num_arg_match_summaries_allocated;
#endif /* DEBUG */

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
Macro that returns TRUE if the current expression is evaluated, i.e.,
it's not inside a sizeof or alignof, and it's not in a "dead" piece of
a short-circuiting operation like "?".
*/
#define curr_expr_is_evaluated() ((a_boolean)expr_stack->evaluated)

/*
Macro that returns TRUE if the current expression is potentially evaluated,
i.e., it's not inside a sizeof or alignof.
*/
#define curr_expr_is_potentially_evaluated()                          \
  ((a_boolean)expr_stack->potentially_evaluated)


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


extern void flush_ref_entries_except(a_ref_entry_ptr keep_list1,
                                     a_ref_entry_ptr keep_list2,
                                     a_ref_entry_ptr saved_list);

extern void flush_ref_entries_list(void);

extern a_ref_entry_ptr ref_entry(a_symbol_ptr      sym_ptr,
                                 a_source_position *source_position);

extern void change_ref_kinds(a_ref_entry_ptr         ref_list,
                             a_symbol_reference_kind new_kind);

extern void change_refs_to_error(a_ref_entry_ptr ref_list);

extern void change_operand_refs_to_error(an_operand *operand);

extern void change_some_ref_kinds(a_ref_entry_ptr         ref_list,
                                  a_symbol_reference_kind old_kind,
                                  a_symbol_reference_kind new_kind);

extern an_arg_operand_ptr alloc_arg_operand(void);

extern void free_arg_operand_list(an_arg_operand_ptr aop);

extern a_dynamic_init_dtor_fixup_ptr alloc_dynamic_init_dtor_fixup(
                                               a_dynamic_init_ptr dynamic_init,
                                               a_source_position  *position);

extern void free_dynamic_init_dtor_fixup(a_dynamic_init_dtor_fixup_ptr didfp);

extern void if_evaluating_mark_routine_referenced(a_routine_ptr  routine);

extern void push_expr_stack(an_expression_kind      expression_kind,
                            an_expr_stack_entry_ptr new_entry);

extern void pop_expr_stack(void);

extern a_constant_ptr var_constant_value(a_variable_ptr var);

extern void using_lvalue(an_operand *operand);

extern void modifying_lvalue(an_operand *operand,
                             a_boolean  value_used);

extern a_boolean is_bit_field_operand(an_operand *operand);

#if ADDR_OF_BIT_FIELD_ALLOWED
extern a_boolean is_bit_field_whose_address_can_be_taken(
                                                        a_field_ptr field,
                                                        a_type_ptr  *ptr_type);
#endif /* ADDR_OF_BIT_FIELD_ALLOWED */

extern void take_address_of_lvalue(an_operand *operand);

extern void conv_object_pointer_to_lvalue(an_operand *operand);

extern void conv_class_operand_to_object_pointer(an_operand *operand);

extern a_constant_ptr value_of_constant_var_lvalue_expr(an_expr_node_ptr node);

extern void conv_lvalue_to_rvalue(an_operand *operand);

extern void make_function_call(an_expr_node_ptr  function_node,
                               a_type_ptr        function_type,
                               a_boolean         is_virtual,
                               a_source_position *call_pos,
                               an_operand        *result);

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

extern void make_lvalue_variable_operand(a_variable_ptr  variable,
                                         an_operand      *result,
                                         a_ref_entry_ptr rep);

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

extern void make_sym_for_member_operand(a_symbol_ptr    member_sym,
                                        a_ref_entry_ptr rep,
                                        an_operand      *operand);

extern void make_template_param_expr_constant_operand(
                                              an_operand            *operand_1,
                                              an_operand            *operand_2,
                                              an_expr_operator_kind op,
                                              a_type_ptr            type,
                                              an_operand            *result);

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

extern void restore_operand_details(an_operand *operand,
                                    an_operand *orig_operand);

extern a_boolean check_function_pointer_operand(an_operand *operand);

extern void make_function_designator_operand(
                                      a_symbol_ptr      routine_sym,
                                      a_boolean         is_qualified_name,
                                      a_source_position *position,
                                      a_ref_entry_ptr   rep,
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
