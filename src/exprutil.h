/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
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

/*
The operators and their precedences are:

Operators			Precedence	Associativity
[] () . -> ++ --		17		L       [] subscripting
							() function call
							++ -- postfix
++ -- & * + - ~ ! sizeof	16		R	Prefix operators
cast				15		R
.* ->* (C++ only)		14		L
* / %				13		L
+ -				12		L
<< >>				11		L
< > <= >=			10		L
== !=				9		L
&				8		L
^				7		L
|				6		L
&&				5		L
||				4		L
?				3		R
= += -= *= /= %=
&= ^= |= <<= >>=		2		R	Assignment operators
,				1		L

Higher-valued precedence means an operator binds more tightly.
Precedence level 0 is used to bracket a complete expression.
*/
#define LEFT_ASSOC  TRUE
#define RIGHT_ASSOC FALSE
#define PREC_POSTFIX    17
#define PREC_PREFIX     16
#define PREC_CAST       15
#define PREC_PTR_TO_MEMBER 14
#define PREC_MULT_DIV   13
#define PREC_PLUS_MINUS 12
#define PREC_SHIFT      11
#define PREC_RELATIONAL 10
#define PREC_EQ_NE       9
#define PREC_AND         8
#define PREC_EXCL_OR     7
#define PREC_OR          6
#define PREC_AND_AND     5
#define PREC_OR_OR       4
#define PREC_QUEST_MARK  3
#define PREC_ASSIGNMENT  2
#define PREC_COMMA       1
#define PREC_LOWEST      0


/*
Interfaces to add_stop_token and remove_stop_token to be used for matching
closing tokens, like ")" for "(".  These mark the beginning and end of
a nested construct.
*/
#define add_matching_stop_token(token)                                \
{ add_stop_token(token);                                              \
  expr_stack->nested_construct_depth++;                               \
}  /* add_matching_stop_token */
#define remove_matching_stop_token(token)                             \
{ remove_stop_token(token);                                           \
  expr_stack->nested_construct_depth--;                               \
}  /* remove_matching_closing_token */


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
  ek_sizeof		/* The operand of sizeof.  This is almost the same
			   as a normal expression. */
};
/* Define as "a_byte" to explicitly control storage size. */
typedef a_byte an_expression_kind;

#if GNU_EXTENSIONS_ALLOWED

/*
These values are returned by __builtin_classify_type.  Order matters;
the values of these enumeration constants must match those used by
GCC.
*/
typedef enum a_type_class_kind {
  tck_none = -1,
  tck_void,             /* void */
  tck_integer,          /* short, int, long, long long */
  tck_char,             /* char */
  tck_enum,             /* enumeration types (currently unused) */
                        /*lint -esym(769,a_type_class_kind::tck_enum)*/
  tck_bool,             /* bool */
  tck_pointer,          /* pointers */
  tck_reference,        /* references */
  tck_offset,           /* Unused in C or C++, but the value must be
			   here to ensure that the values of
			   subsequent enumeration constants are
			   correct. */
			/*lint -esym(769,a_type_class_kind::tck_offset)*/
  tck_float,            /* float, double, long double */
  tck_complex,		/* float complex, double complex, long double
			   complex */
  tck_routine,          /* functions */
  tck_method,           /* Unused in C or C++. */
			/*lint -esym(769,a_type_class_kind::tck_method)*/
  tck_struct,           /* structs or classes */
  tck_union,            /* unions */
  tck_array,            /* arrays -- but not strings */
  tck_string            /* strings */
/* Unused type classes:
  tck_set
  tck_file
  tck_lang
*/
} a_type_class_kind;

#endif /* GNU_EXTENSIONS_ALLOWED */

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
		kind;
			/* Kind of reference (modification, address taken,
			   etc.). */
  a_byte_boolean
		already_recorded;
			/* TRUE if the reference has already been recorded.
			   Used when modifications are recorded at potential
			   sequence points.  The modification is recorded, but
			   the symbol pointer has to be kept around in case
			   the address of the entity is taken for a reference
			   parameter to an overloaded operator function. */
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
  ok_sym_for_member,	/* A symbol for a nonstatic data member or nonstatic
			   member function.  Represented in this way because
			   it's not clear yet how the member is being used --
			   its address might be taken (e.g., "&A::f"), it
			   might get bound to an object and called (e.g.,
			   "p->f(1)"), etc.  Not used for overloaded
			   functions (ok_indefinite_function is used
			   instead).  Used only in C++. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  ok_property_ref,	/* A reference to a field declared with the Microsoft
			   C++ extension __declspec(property(...)). */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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
  a_bit_field	bound_function:1;
			/* TRUE if the operand is a bound function, i.e.,
			   another operand is required to give the object
			   relative to which this function is selected. */
  a_bit_field	virtual_function:1;
			/* TRUE if the operand represents a virtual
			   function. */
  a_bit_field	is_qualified_name:1;
			/* TRUE if the operand was generated from a qualified
			   name. */
  a_bit_field	access_control_error_reported:1;
			/* TRUE if an access control error was reported
			   on the base identifier for this operand.  This
			   remains meaningful only for operands that are
			   essentially still just a representation for
			   an identifier, e.g., ok_indefinite_function and
			   ok_sym_for_member. */
  a_bit_field	is_operand_of_address_of:1;
			/* TRUE if this operand is the immediate operand
			   of an "&" address-of operator.  This is
			   significant in that it discriminates between
			   the standard and nonstandard ways of taking
			   a pointer-to-member address of a member function. */
  a_bit_field	is_template_id:1;
			/* TRUE if an explicit template argument list
			   applies to variant.symbol.  template_arg_list
			   gives the argument list. */
  a_bit_field	is_simple_string_literal:1;
			/* TRUE if this operand is a simple string literal
			   or wide string literal, or the result of the
			   decay of such a literal to a pointer.  FALSE
			   for a string literal that has been subjected to
			   a cast or other operation. */
  a_bit_field	is_cfront_null_pointer_constant:1;
			/* TRUE if this operand is a simple 0 which is
			   suitable as a null pointer constant in
			   overload resolution in cfront mode. */
  a_bit_field	is_using_decl_name:1;
			/* TRUE if the operand was generated from a name
			   that was declared in a using-declaration. */
#if RECORD_FORM_OF_NAME_REFERENCE
  a_bit_field	name_reference_set:1;
			/* TRUE if name_reference has been set. */
  a_name_reference
		name_reference;
			/* Records the form of reference to a name, for
			   some operands that represent named entities
			   (e.g., a function name).  Valid when
			   name_reference_set is TRUE. */
#endif /* RECORD_FORM_OF_NAME_REFERENCE */
  a_source_position
		position;
			/* The source position for the operand. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position
		end_position;
			/* The source position of the end of the operand. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_ref_entry_ptr
		ref_entries_list;
			/* A list of reference entries that are related
			   to the operand.  This list includes only entries
			   for functions and for entities involved in lvalue
			   address computations, whose reference kinds might
			   be changed once the full context surrounding the
			   operand is known. */
  a_template_arg_ptr
		template_arg_list;
			/* When is_template_id is TRUE, a template argument
			   list to be applied to variant.symbol. */
  a_source_position
  		id_position;
			/* Extra source position for an identifier, used
			   for ok_indefinite_function and ok_undefined_symbol.
			   If the name is "X::f", this gives the position of
			   "f", where the field position above gives the
			   position of the "X". */
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
			/* Pointer to the symbol.  May be a projection
			   symbol for ok_indefinite_function or
			   ok_sym_for_member. */
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* When kind == ok_property_ref: */
    /* This is used for a reference to a field declared with the Microsoft
       C++ extension __declspec(property(...)). */
    struct {
      an_expr_node_ptr
		object;	/* Expression for the class object pointer. */
      a_field_ptr
		field;	/* The field referenced. */
      an_arg_operand_ptr
		subscripts;
			/* Optional list of subscript expressions, for cases
			   like p->x[y][z]. */
    } property_ref;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } variant;
} an_operand;


/*
Entry describing an actual argument to a function call.  This is basically
an_operand that can be dynamically allocated and linked into a list.
*/
/* The typedef an_arg_operand_ptr is defined in il_def.h. */
typedef struct an_arg_operand {
  an_arg_operand_ptr
		next;	/* Pointer to the next argument, or NULL if this is the
			   last argument. */
  an_operand	operand;
			/* The argument value. */
} an_arg_operand;


/*
Copy the source position from an expression operand into an expression node.
*/
#if EXTRA_SOURCE_POSITIONS_IN_IL
#define copy_operand_position_to_expr(operand, node) \
  {(node)->expr_range.start = (operand)->position; \
   (node)->expr_range.end   = (operand)->end_position;}
#else /* !EXTRA_SOURCE_POSITIONS_IN_IL */
#define copy_operand_position_to_expr(operand, node) /* Nothing */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */


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
#define TOPT_ADDR_OF_CTOR_ALLOWED 0x10
			/* Taking the address of a constructor is allowed. */
#define TOPT_SUPPRESS_MEMBER_FUNC_TO_PM_CONVERSION 0x20
			/* Member functions should not be converted implicitly
			   to pointer-to-member. */
#define TOPT_SUPPRESS_RVALUE_PROPERTY_REWRITE 0x40
			/* References to fields declared with
			   __declspec(property(...)) (a Microsoft extension)
			   should not be rewritten as calls of the appropriate
			   "get" function. */
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
			   if it's the operand of a sizeof.  Always TRUE in
			   a constant expression, even one inside a not-
			   evaluated expression. */
  a_byte_boolean
		is_default_arg_expression;
			/* TRUE if the expression is or is inside of a
			   C++ default argument expression in a parameter
			   list. */
  a_byte_boolean
		is_template_arg_expression;
			/* TRUE if the expression is an argument of a C++
			   template reference.  This is TRUE only for the
			   top level major expression for a template
			   argument, e.g., it's not TRUE inside a sizeof
			   inside a template argument. */
  a_byte_boolean
		is_vla_dimension_expression;
			/* TRUE if the expression is the dimension of a
			   VLA (variable-length array).  This is TRUE only
			   for the top level major expression for a VLA
			   dimension, e.g., it's not TRUE inside a sizeof
			   inside a VLA dimension. */
  a_byte_boolean
		in_cctor_elision_initializer;
			/* TRUE if the expression is or is inside of an
			   initializer expression that is subject to
			   the copy constructor elision optimization,
			   for example in a return expression in a
			   routine that returns its value to the caller by
			   calling a copy constructor.  This has an effect
			   on destructor calls noted in dynamic initialization
			   entries for temporaries.  See
			   fix_up_dynamic_init_dtors. */
  a_byte_boolean
		fold_constant_addr_exprs;
			/* TRUE if constant addressing expressions should be
			   folded to constants.  Always TRUE if the expression
			   is a constant expression; sometimes TRUE for
			   nonconstant expressions (e.g., initializer
			   expressions, where it helps in discerning static
			   initialization cases from others). */
  a_byte_boolean
		inside_conditional_expression;
			/* TRUE if inside a conditional operand of an
			   operator like "?". */
  a_dynamic_init_dtor_fixup_ptr
		dynamic_init_dtor_fixup_list;
			/* List of dynamic init entries for which destructor
			   processing was delayed.  Only non-NULL when
			   in_cctor_elision_initializer is TRUE. */
  unsigned long	nested_construct_depth;
			/* Number of nested constructs like parentheses
			   begun within this major expression level. */
  an_object_lifetime_ptr
		lifetime;
			/* If non-NULL, points to an object lifetime that
			   exactly covers this expression, i.e., the lifetime
			   for temporaries created in the expression. */
  a_dynamic_init_ptr
		destructions_preceding_expr;
			/* Points to the dynamic initialization that was the
			   first on the list of the current object lifetime
			   (i.e., the most recently constructed) when this
			   expression was begun.  Needed when no lifetime
			   is pushed for the current expression, to find
			   the sequence of destructions associated with the
			   expression.  NULL if the "lifetime" field is
			   non-NULL. */
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

#if RECORD_CONSTANT_EXPRESSIONS_IN_IL

/*
Macro that returns TRUE if the current expression kind is one in which
expressions are recorded for constants.  They are not recorded for
preprocessing expressions (because the constants are never saved) or
for template argument expressions (because a template can be specified
many times with a different argument expression each time).
*/
#define curr_expr_kind_is_one_in_which_const_exprs_are_recorded() \
  (!curr_expr_kind_is(ek_pp) && !curr_expr_kind_is(ek_template_arg))

#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
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


/* Copy an operand.  Note that this does not copy the subtree of the
   operand.  This macro is used to move an operand from one place to
   another, with the idea that the old copy will no longer be used.
   If you want to make a full copy, see clone_operand. */
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
Macro that is TRUE if the operand is a ck_template_param constant operand.
*/
#define is_template_param_constant_operand(operand)			\
	(is_constant_operand(operand) &&                                \
	 (operand)->variant.constant.kind ==                            \
                                (a_constant_repr_kind)ck_template_param)

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

#if MICROSOFT_EXTENSIONS_ALLOWED
/*
Macro that is TRUE if the operand is one that references a field declared
with the Microsoft C++ extension __declspec(property(...)).
*/
#define is_property_ref_operand(operand)				\
	((operand)->kind == (an_operand_kind)ok_property_ref)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

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
constant expressions.  Such a variable has const integral or enum type.
In a prototype instantiation, it could instead have a template parameter type.
The other half of this check, that the variable has an initializer,
is done in var_constant_value.
*/
#define is_const_variable(var)                                          \
  ((is_integral_or_enum_type((var)->type) &&                            \
    is_const_qualified_type((var)->type)) ||                            \
   is_template_param_type((var)->type))


extern a_ref_entry_ptr copy_ref_entry_list(a_ref_entry_ptr ref_list);

extern void flush_ref_entries_except(a_ref_entry_ptr keep_list1,
                                     a_ref_entry_ptr keep_list2);

extern a_ref_entry_ptr ref_entry(a_symbol_ptr      sym_ptr,
                                 a_source_position *source_position);

extern void change_ref_kinds(a_ref_entry_ptr         ref_list,
                             a_symbol_reference_kind new_kind);

extern void change_refs_to_error(a_ref_entry_ptr ref_list);

extern void change_operand_refs_to_error(an_operand *operand);

extern void change_arg_operand_list_refs_to_error(
                                          an_arg_operand_ptr arg_operand_list);

extern void change_some_ref_kinds(a_ref_entry_ptr         ref_list,
                                  a_symbol_reference_kind old_kind,
                                  a_symbol_reference_kind new_kind);

extern void record_operand_modification_refs(an_operand *operand);

extern an_arg_operand_ptr alloc_arg_operand(void);

extern void free_arg_operand_list(an_arg_operand_ptr aop);

extern void free_dynamic_init_dtor_fixup(a_dynamic_init_dtor_fixup_ptr didfp);

extern void if_evaluating_mark_routine_referenced(a_routine_ptr  routine);

extern void expr_reference_to_implicitly_invoked_function
                                            (a_symbol_ptr      sym,
                                             a_source_position *pos,
                                             a_type_ptr        class_of_object,
                                             a_boolean         honor_virtual);

extern an_expr_node_ptr expr_copy_default_arg_expr_list(a_routine_ptr    rout,
                                                        a_param_type_ptr ptp);

extern void push_expr_stack(an_expression_kind      expression_kind,
                            an_expr_stack_entry_ptr new_entry,
                            a_boolean               force_object_lifetime,
                            a_boolean               suppress_object_lifetime);

extern void pop_expr_stack(void);

extern an_expr_node_ptr wrap_up_full_expression(an_expr_node_ptr expr);

extern void discard_curr_expr_object_lifetime(void);

extern void wrap_up_dynamic_init_full_expression(a_dynamic_init_ptr dip);

extern a_constant_ptr var_constant_value(a_variable_ptr var);

extern void using_lvalue(an_operand *operand);

extern void modifying_lvalue(an_operand *operand,
                             a_boolean  value_used);

extern a_boolean is_bit_field_operand(an_operand *operand);

extern a_boolean is_bit_field_whose_address_can_be_taken(
                                                        a_field_ptr field,
                                                        a_type_ptr  *ptr_type);

extern void take_address_of_lvalue(an_operand *operand);

extern void conv_object_pointer_to_lvalue(an_operand *operand);

extern void conv_class_operand_to_object_pointer(an_operand *operand);

extern a_constant_ptr value_of_constant_var_lvalue_operand(
                                                          an_operand *operand);

extern void conv_lvalue_to_rvalue(an_operand *operand);

extern a_type_ptr determine_arithmetic_conversions(an_operand *operand_1,
					           an_operand *operand_2);

extern a_type_ptr usual_arithmetic_conversions(a_type_ptr operand_1_type,
                                               a_type_ptr operand_2_type);

#if TARG_HAS_IEEE_FLOATING_POINT
extern void make_nan_operand(an_operand  *result);

extern void make_infinity_operand(an_operand  *result);
#endif /* TARG_HAS_IEEE_FLOATING_POINT */

#if C99_IL_EXTENSIONS_SUPPORTED
extern void make_imaginary_unit_operand(an_operand  *result);

extern a_boolean determine_imaginary_operation_type
                                        (a_token_kind          op_token,
                                         an_operand            *operand_1,
                                         an_operand            *operand_2,
                                         a_type_ptr            *result_type,
                                         an_expr_operator_kind *op);
#endif /* C99_IL_EXTENSIONS_SUPPORTED */

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void adjust_constant_operand_info_for_microsoft_null_pointer_test(
                                               an_operand *operand,
                                               a_boolean  *operand_is_constant,
                                               a_constant **operand_constant);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

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

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void rewrite_property_field_reference(an_operand *operand,
                                             an_operand *put_operand);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void error_if_indefinite_function(an_operand *operand);

extern void do_operand_transformations(an_operand                   *operand,
                                       a_transformation_options_set options);

extern a_type_ptr boolean_result_type(void);

extern an_expr_node_ptr normalize_boolean_controlling_expr(
                                                       an_expr_node_ptr expr); 

extern a_boolean op_is_zero_constant(an_operand *operand);

extern a_boolean op_is_false_constant(an_operand *operand);

extern void add_reference_indirection(an_operand *result);

extern void make_lvalue_variable_operand(a_variable_ptr  variable,
                                         an_operand      *result,
                                         a_ref_entry_ptr rep,
                                         a_boolean       record_expr);

extern void make_lvalue_operand_from_compound_constant(
                                                     a_constant_ptr  constant,
                                                     an_operand      *operand);

extern void make_ptr_to_member_constant_operand(
                                    a_symbol_ptr      member_sym,
                                    a_symbol_ptr      member_proj_sym,
                                    a_source_position *position,
                                    a_boolean         check_protected_access,
                                    a_boolean         is_qualified_name,
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

extern a_boolean check_arithmetic_or_enum_operand(an_operand *operand);

extern void make_integer_constant_operand(an_operand		*operand,
				          a_host_large_integer	value);

extern void promote_operand(an_operand *operand);

extern void arg_default_promote_operand(an_operand *argument_operand);

extern void make_constant_operand(a_constant *constant,
			          an_operand *operand);

extern void make_constant_variable_operand(a_constant *constant,
                                           a_variable *var,
                                           an_operand *operand);

extern void make_sym_constant_operand(a_symbol_ptr sym,
                                      an_operand   *operand);

extern void make_string_constant_operand(a_constant *constant,
                                         an_operand *operand);

#if DEBUG
extern void db_operand(an_operand *operand);
#endif /* DEBUG */

extern void clear_operand(an_operand_kind kind,
		          an_operand      *operand);

#if EXTRA_SOURCE_POSITIONS_IN_IL
extern void set_operand_expr_position_if_expr(an_operand        *operand,
                                              a_source_position *operator_pos);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

extern void set_operand_kind(an_operand      *operand,
                             an_operand_kind kind);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void clone_operand(an_operand *operand,
                          an_operand *operand_clone);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void error_in_operand(an_error_code error_code,
		             an_operand    *operand);

extern void type2_error_in_operand(an_error_code error_code,
                                   an_operand    *operand,
                                   a_type_ptr    type1,
                                   a_type_ptr    type2);

extern void change_nonreal_member_constant_operand_to_lvalue(
                                                          an_operand *operand);

extern void revert_gcc_rvalue_to_lvalue_if_possible(an_operand *operand,
                                                    a_boolean  ignore_casts);

extern a_boolean check_modifiable_lvalue_operand(an_operand *operand);

extern a_boolean check_scalar_operand(an_operand *operand);

extern void make_field_operand(a_field_ptr field,
			       an_operand  *result);

extern a_dynamic_init_ptr alloc_expr_dynamic_init(a_dynamic_init_kind kind);

extern a_dynamic_init_ptr alloc_dtor_dynamic_init(
                                           a_dynamic_init_kind kind,
                                           a_type_ptr          type,
                                           a_source_position   *position);

extern void set_temp_init_dynamic_init_lifetime(
                                              an_expr_node_ptr temp_init_node);

extern an_expr_node_ptr alloc_temp_init_node(
                                      a_type_ptr         temp_type,
                                      a_dynamic_init_ptr dip,
                                      a_boolean          result_is_addr,
                                      a_boolean          is_explicit_cast);

extern an_expr_node_ptr create_expr_temporary(
                                    a_type_ptr          temp_type,
                                    a_boolean           result_is_addr,
                                    a_boolean           is_explicit_cast,
                                    a_boolean           suppress_abstract_test,
                                    a_dynamic_init_kind init_kind,
                                    a_source_position   *position,
                                    a_dynamic_init_ptr  *dip);

extern a_routine_ptr routine_from_function_operand(an_operand *operand);

extern void make_function_call(an_expr_node_ptr  function_node,
                               a_type_ptr        function_type,
                               a_boolean         is_virtual,
                               a_boolean         virtual_suppressed,
                               a_boolean         compiler_generated,
                               a_boolean         is_conversion,
                               a_source_position *call_pos,
                               an_operand        *result);

extern void assemble_function_call(an_operand        *function_operand,
                                   an_operand        *bound_function_selector,
                                   an_expr_node_ptr  argument_list,
                                   a_boolean         compiler_generated,
                                   a_boolean         is_conversion,
                                   a_source_position *call_position,
                                   an_operand        *result);

extern a_statement_ptr make_call_assignment_statement(
                                            a_routine_ptr     rout,
                                            a_boolean         suppress_virtual,
                                            an_expr_node_ptr  dest,
                                            an_expr_node_ptr  source,
                                            a_source_position *err_pos);

extern a_boolean check_pointer_operand(an_operand    *operand,
				       an_error_code err_code);

extern void make_expression_operand(an_expr_node_ptr node,
                                    a_type_ptr       type,
			            an_operand       *operand);

extern void make_indefinite_function_operand(a_symbol_ptr routine_sym,
                                             a_boolean    curr_id,
                                             an_operand   *operand);

extern void make_sym_for_member_operand(a_symbol_ptr    member_sym,
                                        a_boolean       is_qualified_name,
                                        a_ref_entry_ptr rep,
                                        an_operand      *operand);

extern void make_template_param_expr_constant_operand(
                                                    an_expr_node_ptr node,
                                                    an_operand        *result);

extern an_expr_node_ptr make_node_from_operand(an_operand *operand);

extern an_expr_node_ptr make_node_from_operand_preserving_name_reference(
                                                          an_operand *operand);

#if RECORD_FORM_OF_NAME_REFERENCE
extern void set_operand_name_reference_from_locator_for_curr_id(
                                                          an_operand *operand);
#else /* !RECORD_FORM_OF_NAME_REFERENCE */
#define set_operand_name_reference_from_locator_for_curr_id(x) /* Nothing */
#endif /* RECORD_FORM_OF_NAME_REFERENCE */

extern void extract_constant_from_operand(an_operand     *operand,
                                          a_constant_ptr constant);

extern void discard_operand(an_operand *operand);

extern void prep_generic_nontype_template_argument(an_operand *operand);

extern void prep_generic_template_argument_list(
                                         a_template_arg_ptr template_arg_list);

extern void make_unknown_dependent_function_operand(
                                          a_symbol_ptr       sym,
                                          a_boolean          is_template_id,
                                          a_template_arg_ptr template_arg_list,
                                          a_boolean          is_qualified_name,
                                          an_operand         *operand);

extern void cast_operand(a_type_ptr new_type,
		         an_operand *operand,
                         a_boolean  check_cast_access,
		         a_boolean  is_implicit_cast,
                         a_boolean  is_reinterpret_cast,
                         a_boolean  reinterpret_semantics);

extern void conv_selector_to_object_pointer(an_operand *operand,
                                            a_boolean  *is_arrow_operator);

extern void base_class_cast_operand(an_operand       *operand_1,
                                    a_base_class_ptr bcp,
                                    a_boolean        *is_arrow_operator,
                                    a_boolean        check_cast_access,
                                    a_boolean        is_implicit_cast,
                                    a_boolean        implicit_in_naming,
                                    a_boolean        is_object_pointer);

extern a_boolean is_a_cplusplus_lvalue(an_operand *operand);

extern void make_error_operand(an_operand *operand);

extern void operand_will_not_be_used_because_of_error(an_operand *operand);

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

extern a_boolean check_integral_or_enum_operand(an_operand *operand);

extern a_type_ptr type_after_array_to_pointer_transformation(a_type_ptr type);

extern void conv_array_operand_to_pointer_operand(an_operand *operand);

extern a_type_ptr type_after_function_to_pointer_transformation(
                                                      a_type_ptr arg_type,
                                                      an_operand *arg_operand);

extern void conv_sym_for_member_operand_to_ptr_to_member(an_operand *operand);

extern void conv_function_designator_to_ptr_to_function(an_operand *operand,
                                                        a_boolean  allow_ctor);

extern a_type_ptr do_implicit_type_transformations(a_type_ptr type,
                                                   an_operand *operand);

extern void error_and_make_error_operand(an_error_code error_code,
				         an_operand    *operand);

extern void do_binary_operation(an_expr_operator_kind op,
			        an_operand            *operand_1,
			        an_operand            *operand_2,
			        a_type_ptr            type_for_result,
			        an_operand            *result,
			        a_source_position     *operator_position);

extern void prep_generic_operand(an_operand *operand,
                                 a_boolean  lvalue_expected);

extern void generic_cast_operand(an_operand            *operand,
                                 a_type_ptr            dest_type,
                                 an_expr_operator_kind op,
                                 a_boolean             is_implicit_cast,
                                 a_boolean             is_reference_cast);

extern an_expr_node_ptr prep_generic_argument_list(
                                             an_arg_operand *arg_operand_list);

extern void template_binary_operation(
                                     an_expr_operator_kind op,
                                     an_operand            *operand_1,
                                     an_operand            *operand_2,
                                     an_operand            *result,
                                     a_source_position     *operator_position);

extern void do_unary_operation(an_expr_operator_kind op,
                               an_operand            *operand,
                               a_type_ptr            result_type,
                               an_operand            *result,
                               a_source_position     *start_position);

extern void template_unary_operation(an_expr_operator_kind op,
                                     an_operand            *operand,
                                     an_operand            *result,
                                     a_source_position     *start_position);

extern void do_question_operation(an_operand *operand_1,
                                  an_operand *operand_2,
                                  an_operand *operand_3,
                                  a_type_ptr result_type,
                                  a_boolean  result_is_an_lvalue,
                                  an_operand *result);

extern void template_question_operation(an_operand *operand_1,
                                        an_operand *operand_2,
                                        an_operand *operand_3,
                                        an_operand *result);

extern a_boolean validate_boolean_controlling_expr(an_operand *operand,
                                                   a_boolean  validate_only);

extern a_boolean check_boolean_controlling_expr(an_operand *operand);

extern a_boolean still_an_lvalue(a_type_ptr type_before_cast,
			         a_type_ptr type_cast_to);

extern an_expr_operator_kind which_binary_operator(a_token_kind token,
						   a_type_ptr   type);

extern an_expr_operator_kind generic_operator_for_opname_kind(
                                                an_opname_kind kind,
                                                a_boolean      unary_operator);

extern a_boolean operator_takes_lvalue_operand(an_expr_operator_kind op);

extern void add_base_class_casts(a_base_class_ptr  bcp,
                                 a_type_ptr        qualifiers_model,
                                 a_boolean         check_cast_access,
                                 a_boolean         is_implicit_cast,
                                 a_boolean         implicit_in_naming,
                                 an_expr_node_ptr  *p_node,
                                 a_source_position *err_pos);

extern void cast_node(an_expr_node_ptr  *p_node,
		      a_type_ptr        type,
                      a_boolean         check_cast_access,
		      a_boolean         is_implicit_cast,
                      a_boolean         is_reinterpret_cast,
                      a_boolean         reinterpret_semantics,
                      a_source_position *err_pos);

extern a_type_ptr operand_type_after_integral_promotion(an_operand *operand);

#if UPC_EXTENSIONS_ALLOWED
extern void make_upc_thread_operand(an_operand            *operand,
                                    a_constant_repr_kind  kind);
#endif /* UPC_EXTENSIONS_ALLOWED */

#if DEBUG
extern unsigned long show_expr_space_used(void);
#endif /* DEBUG */

extern void expr_one_time_init(void);

extern void expr_trans_unit_init(void);

extern void expr_init(void);

#endif /* ifndef EXPRUTIL_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
