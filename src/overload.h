/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1993 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

overload.h -- Declarations related to expression overload resolution.

*/

/* Avoid including these declarations more than once: */
#ifndef OVERLOAD_H
#define OVERLOAD_H 1

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */
#ifndef EXPRUTIL_H
#include "exprutil.h"
#endif /* ifndef EXPRUTIL_H */


/*
Description of a complete conversion (user-defined part plus standard
conversion part).  Really, the interesting information about that
conversion which must be kept around, which is not really a full
description.  Can describe no conversion.  Also used to describe a bitwise
copy of a class in C or C++, which is not really a conversion.
If this entry is used in describing a conversion that is part of a
reference initialization, the conversion is the one done on the initial
value to bring it to the underlying type of the reference, not a
conversion to the reference type itself.
*/
typedef struct a_conv_descr *a_conv_descr_ptr;
typedef struct a_conv_descr {
  a_routine_ptr	routine;
			/* The conversion routine entry.  NULL if
			   class_identity_or_bitwise_copy is TRUE or if there
			   is no user-defined part of the conversion. */
  a_byte_boolean
		class_identity_or_bitwise_copy;
			/* If TRUE, the "conversion" for a class is either
			   identity (the class already has the right type) or
			   is a bitwise copy (substituting for either a
			   copy constructor or an assignment operator).
			   Note that the source type may be a derived class
			   of the destination type. */
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
  a_std_conv_descr
		std;	/* The standard conversion part of the conversion. */
} a_conv_descr;

/*
Macro that tests for a conversion description with a null user-defined
part.
*/
#define is_null_user_conv_descr(user_conversion)                      \
  ((user_conversion)->routine == NULL &&                              \
   !(user_conversion)->class_identity_or_bitwise_copy)

/*
Macro that returns TRUE if a pointer to a conversion is usable (the
pointer is non-NULL, and the conversion is not ambiguous).
*/
#define conv_usable(conversion)                             \
  ((conversion) != NULL && !(conversion)->ambiguous)



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
		const_anachronism;
			/* In cfront compatibility mode, TRUE to indicate
			   that the match was possible only because of the
			   anachronism that allows a non-const function to
			   be called for a const object. */
  a_byte_boolean
		is_match_for_this_param;
			/* TRUE if this entry describes the match for the
			   "this" parameter. */
  a_type_ptr	param_type;
			/* The type of the parameter.  Used in looking
			   for conversion subsequences involving addition
			   of type qualifiers at the end of a conversion.
			   NULL if not applicable (e.g., for an ellipsis). */
  a_conv_descr	conversion;
			/* Description of the conversion to be done (really,
			   information we wanted to remember about the
			   conversion to be done).  Contains a user-defined
			   conversion part and a standard conversion part.
			   If match_level is aml_user_conversion, this
			   describes a user-defined conversion.
			   Note that in one case involving cfront
			   compatibility, no user-defined conversion is
			   indicated even through match_level is
			   aml_user_conversion.  Also, when passing a
			   class that requires a copy constructor by value,
			   the user-defined conversion will indicate the
			   copy constructor even though the match level
			   is aml_std_conversion or aml_exact. */
} an_arg_match_summary;


/*
Entry describing a function that is a candidate instance of an overloaded
function.  This entry is used in resolving overloaded function calls.
*/
typedef struct a_candidate_function *a_candidate_function_ptr;
typedef struct a_candidate_function {
  a_candidate_function_ptr
		next;	/* Next entry on the list of candidates, or NULL
			   if this is the last entry. */
  a_symbol_ptr	function_symbol;
			/* Pointer to the symbol for the function.  NULL if
			   the "function" is a built-in operator. */
  a_byte_boolean
		is_function_template;
			/* TRUE if function_symbol is a function template. */
  a_template_arg_ptr
		template_arg_list;
			/* If is_function_template is TRUE, and if the
			   proper set of template arguments has been worked
			   out, this is it.  Otherwise, NULL. */
  char		*operand_type_pattern;
			/* For a built-in operator, the operand type pattern
			   string (see operand_type_pattern_for_operator).
			   Specifically, the appropriate one- or two-character
			   segment of the operand pattern string.  NULL
			   if not a built-in operator. */
  a_byte_boolean
		is_user_conversion;
			/* TRUE if this function is a user-defined conversion
			   being examined to resolve an implicit conversion.
			   user_conversion is meaningful in that case.
			   This will have the same setting in all candidate
			   function entries being considered as a set. */
  a_conv_descr	conversion;
			/* If is_user_conversion is TRUE. description of the
			   conversion being done, including the user-defined
			   part. */
  a_type_ptr	pointer_type;
			/* For a built-in operator with an operand pattern
			   including pointers, this indicates the pointer
			   type. */
  an_arg_match_summary_ptr
		arg_matches;
			/* List of entries describing how well each actual
			   argument matches this function's corresponding
			   formal parameter.  If there was a selector object
			   (for the "this" parameter), it appears first. */
  an_arg_operand_ptr
		arg_operand_list;
			/* If is_function_template is TRUE, the list of
			   argument operands for the call.  NULL otherwise.
			   Note that when present this list is shared with
			   all the other candidate function entries. */
  /* Fields used by select_best_candidate_functions: */
  an_arg_match_summary_ptr
		current_arg_match;
			/* The argument match entry currently being considered
			   by select_best_candidate_functions. */
  an_arg_match_summary_ptr
		prev_func_arg_match_with_same_match_level;
			/* If non-NULL, points to an argument match summary
			   for the same argument on a previous candidate
			   function that has the same match level.  This is
			   used to keep track of the set of best-matching
			   arguments: they are the set that has this field
			   pointing to the argument match that was chosen as
			   best (i.e., all the argument matches that tied
			   for "best"). */
  a_byte_boolean
		in_best_match_set;
			/* TRUE if the function is in the set of best-matching
			   functions. */
  a_byte_boolean
		in_best_match_set_for_some_argument;
			/* TRUE if the function is in the set of best-matching
			   functions for some argument. */
} a_candidate_function;


EXTERN a_candidate_function_ptr
		avail_candidate_functions;
			/* List of candidate function entries that have been
			   freed and are available for reuse. */
EXTERN an_arg_match_summary_ptr
		avail_arg_match_summaries;
			/* List of argument match summary entries that have
			   been freed and are available for reuse. */

#if DEBUG
/*
Counts of entries allocated, for debugging purposes.
*/
EXTERN unsigned long
		num_candidate_functions_allocated;
#endif /* DEBUG */


extern void free_arg_match_summary_list(an_arg_match_summary_ptr amsp);

extern void issue_warning_from_arg_match_summary(
                                            an_arg_match_summary_ptr amsp,
                                            a_source_position        *err_pos);

extern a_symbol_ptr find_addr_of_overloaded_function_match(
                                               a_symbol_ptr       ovl_sym,
                                               a_type_ptr         dest_type,
                                               a_source_position  *source_pos,
                                               an_arg_match_level *match_level,
                                               a_std_conv_descr   *std_conv,
                                               a_boolean          *ambiguous);

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

extern void make_constructor_dynamic_init(a_routine_ptr     ctor_routine,
                                          an_expr_node_ptr  arg_expr_list,
                                          a_boolean         result_is_addr,
                                          a_source_position *position,
                                          an_operand        *result);

extern void temp_init_from_operand(an_operand *operand);

extern void overloaded_function_catch_up(
                                  a_symbol_ptr      function_symbol,
                                  a_symbol_ptr      overloaded_function_symbol,
                                  a_boolean         is_qualified_name,
                                  a_source_position *call_position,
                                  a_boolean         elided_reference,
                                  a_boolean         address_taken,
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

a_boolean conversion_from_class_possible(
                               an_operand               *source_operand,
                               a_type_ptr               dest_type,
                               a_builtin_type_kind_set  builtin_types_allowed,
                               a_boolean                need_lvalue_result,
                               a_conv_descr             *conversion,
                               a_boolean                *ambiguous,
                               a_candidate_function_ptr *ambiguity_list);

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

extern a_boolean user_defined_conversion_possible(
                                            an_operand   *source_operand,
                                            a_type_ptr   dest_type,
                                            a_boolean    is_initialization,
                                            a_boolean    need_lvalue_result,
                                            a_boolean    is_explicit_cast,
                                            a_conv_descr *conversion,
                                            a_conv_descr *ctor_arg_conversion,
                                            a_boolean    *failed);

extern void user_convert_operand(an_operand   *operand,
                                 a_type_ptr   dest_type,
                                 a_conv_descr *conversion,
                                 a_conv_descr *ctor_arg_conversion);

extern void prep_elision_initializer_operand(
                                            an_operand         *source_operand,
                                            a_type_ptr         dest_type,
                                            a_dynamic_init_ptr *dip);

extern void prep_initializer_operand(
                                  an_operand    *source_operand,
                                  a_type_ptr    dest_type,
                                  a_conv_descr  *conversion,
                                  a_boolean     initializing_return_value,
                                  a_boolean     static_lifetime,
                                  a_boolean     try_user_conversions,
                                  an_error_code incompatible_err);

extern void prep_argument_operand(an_operand       *source_operand,
                                  a_param_type_ptr formal_param,
                                  a_conv_descr     *conversion,
                                  an_error_code    err_code);

extern void prep_return_by_cctor_operand(an_operand         *source_operand,
                                         a_type_ptr         required_type,
                                         an_error_code      err_code,
                                         a_dynamic_init_ptr *dip);

extern void prep_assignment_operand(an_operand        *source_operand,
                                    a_type_ptr        dest_type,
                                    an_error_code     incompatible_err,
                                    a_source_position *err_pos);

extern void overload_init(void);

#endif /* ifndef OVERLOAD_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1993 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
