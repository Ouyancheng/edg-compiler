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
			   class_identity_or_bitwise_copy or
			   unknown_dependent_conversion is TRUE or if there
			   is no user-defined part of the conversion. */
  a_symbol_ptr	routine_symbol;
			/* Non-NULL only for conversion functions, in which
			   case it is the symbol for the routine, possibly
			   a projection symbol.  Needed to check access on
			   conversion function calls. */
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
			   an rvalue (either originally or after an
			   lvalue-->rvalue conversion).  Note that this
			   is meaningful even when the entry indicates no
			   conversion. */
  a_byte_boolean
		unusable;
			/* If TRUE the conversion is unusable, e.g., it is
			   ambiguous. */
  a_byte_boolean
		class_object_adjustment_required;
			/* If TRUE, the result type of the conversion routine
			   is a class type.  It differs from the desired
			   type in being a derived class or in having different
			   cv-qualifiers, but such adjustment is not a standard
			   conversion; it's part of reference binding.
			   std.cast_base_class indicates the derived --> base
			   part of the adjustment. */
  a_byte_boolean
		conversion_for_direct_reference_binding;
			/* If TRUE, the conversion indicated is one that
			   converts an initializer using a conversion function
			   that returns a reference in order to produce an
			   lvalue that a reference can be directly bound to. */
  a_byte_boolean
		copy_initialization_done_as_direct;
			/* If TRUE, the conversion is a copy initialization
			   that, following the rules in [dcl.init] of the
			   C++ standard, is done as if it were a direct
			   initialization. */
  a_byte_boolean
		user_conversion_for_class_copy_must_be_determined;
			/* If TRUE, this is a class value being passed to
			   a parameter of the same type, or a base class type.
			   This is seen as a standard conversion of sorts
			   in overload resolution, but the constructor
			   to be called must be determined once it's known
			   that this conversion will be used. */
  a_byte_boolean
		unknown_dependent_conversion;
			/* If TRUE, the conversion is from or to a template
			   dependent type in a prototype instantiation, and
			   therefore we cannot know the right constructor
			   or conversion function to call. */
  a_std_conv_descr
		std;	/* The standard conversion part of the conversion. */
} a_conv_descr;

/*
Macro that tests for a conversion description with a null user-defined
part.
*/
#define is_null_user_conv_descr(user_conversion)                      \
  ((user_conversion)->routine == NULL &&                              \
   !(user_conversion)->class_identity_or_bitwise_copy &&              \
   !(user_conversion)->unknown_dependent_conversion)

/*
Macro that returns TRUE if a pointer to a conversion is usable (the
pointer is non-NULL, and the conversion is not unusable).
*/
#define conv_usable(conversion)                             \
  ((conversion) != NULL && !(conversion)->unusable && \
   !(conversion)->user_conversion_for_class_copy_must_be_determined)



/*
Bit flags used to indicate the kinds of built-in types allowed
for try_to_convert_class_operand_to_builtin_type and
conversion_from_class_possible.
*/
#define BTK_INTEGRAL 0x1
			/* Any integral type (includes enum in C but not in
			   C++). */
#define BTK_FLOATING 0x2
			/* Any floating type. */
#define BTK_POINTER 0x4
			/* Any pointer. */
#define BTK_OBJECT_POINTER 0x8
			/* Any pointer to (complete) object type. */
#define BTK_FUNCTION_POINTER 0x10
			/* Any pointer to function. */
#define BTK_PTR_TO_MEMBER 0x20
			/* Any pointer to member. */
#define BTK_BOOL 0x40
			/* bool (C++). */
#define BTK_ENUM 0x80
			/* Enumeration types in C++ (in C, they're
			   integral). */
#define BTK_PTRDIFF_T 0x100
			/* ptrdiff_t */
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
		anachronism_used;
			/* TRUE if the match was possible only because an
			   anachronism was used.  This is a tie-breaker on
			   otherwise equal matches.  Note that this is not
			   set for all anachronisms, just those that act
			   as tie-breakers in cfront mode. */
  a_byte_boolean
		const_anachronism;
			/* In cfront compatibility mode, TRUE to indicate
			   that the match was possible only because of the
			   anachronism that allows a non-const function to
			   be called for a const object.  If this is set, the
			   anachronism_used flag will also be set. */
  a_byte_boolean
		is_match_for_this_param;
			/* TRUE if this entry describes the match for the
			   "this" parameter. */
  a_byte_boolean
		arg_is_constant;
			/* TRUE if the corresponding argument is a constant.
			   This is used for a Microsoft-mode test. */
  a_type_ptr	param_type;
			/* The type of the parameter.  Used in looking
			   for conversion subsequences involving addition
			   of type qualifiers at the end of a conversion.
			   NULL if not applicable (e.g., for an ellipsis). */
  a_type_ptr	guide_type;
			/* For operands of builtin operators, the type
			   passed as a guiding type when trying to do the
			   conversion from a class type.  This generally
			   comes from the type of the other operand, and
			   controls template conversion operator
			   applicability. */
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
			   the "function" is a built-in operator or for a
			   surrogate function case.  Can be a projection
			   symbol. */
  a_byte_boolean
		is_function_template;
			/* TRUE if function_symbol is a function template. */
  a_byte_boolean
		expl_template_arg_list_used;
			/* TRUE if an explicit template argument list was
			   used (e.g., f<int>). */
  a_template_arg_ptr
		template_arg_list;
			/* If is_function_template is TRUE, this points to
			   the list of template arguments.  The list is
			   complete, through some combination of explicit
			   specification and deduction. */
  char		*operand_type_pattern;
			/* For a built-in operator, the operand type pattern
			   string (see operand_type_pattern_for_operator).
			   Specifically, the appropriate one- or two-character
			   segment of the operand pattern string.  NULL
			   if not a built-in operator. */
  a_symbol_ptr	surrogate_function_conv_sym;
			/* Non-NULL for a surrogate function case
			   (function_symbol will be NULL).  Points to the
			   symbol for the conversion function that will
			   yield a pointer to the surrogate function to be
			   called. */
  a_byte_boolean
		is_user_conversion;
			/* TRUE if this function is a user-defined conversion
			   being examined to resolve an implicit conversion.
			   The field "conversion" is meaningful in that case.
			   This will have the same setting in all candidate
			   function entries being considered as a set. */
  a_conv_descr	conversion;
			/* If is_user_conversion is TRUE. description of the
			   conversion being done, including the user-defined
			   part. */
  a_type_ptr	specific_type;
			/* For a built-in operator with an operand pattern
			   including corresponding types (e.g., pointer types),
			   this indicates the specific type. */
  an_arg_match_summary_ptr
		arg_matches;
			/* List of entries describing how well each actual
			   argument matches this function's corresponding
			   formal parameter.  If there was a selector object
			   (for the "this" parameter), it appears first. */
  /* Fields used by select_best_candidate_functions: */
  an_arg_match_summary_ptr
		current_arg_match;
			/* The argument match entry currently being considered
			   by select_best_candidate_functions. */
  a_candidate_function_ptr
		next_in_arg_best_match_set;
			/* If non-NULL, points to the next candidate function
			   that's in the set of best matches for the argument
			   currently being examined. */
  a_byte_boolean
		in_best_match_set;
			/* TRUE if the function is in the set of best-matching
			   functions. */
  a_byte_boolean
		in_best_match_set_for_some_argument;
			/* TRUE if the function is in the set of best-matching
			   functions for some argument. */
  a_byte_boolean
		in_best_match_set_for_curr_argument;
			/* TRUE if the function is in the set of best-matching
			   functions for the current argument. */
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


/*
Enumeration indicating the state on return from a scan of a printf format
string.
*/
typedef enum /*a_printf_scan_state*/ {
  pss_new_specifier,	/* Look for new specifier next time. */
  pss_after_field_width,/* Start after field width next time. */
  pss_after_precision	/* Start after precision next time. */
} a_printf_scan_state;


/*
Block of information used to check correspondence of a sequence of call
arguments against a sequence of function parameters.
*/
typedef struct an_arg_check_block {
  a_routine_ptr	routine;
			/* The routine being called, if known.  NULL otherwise,
			   e.g., for a call through a function pointer. */
  a_boolean	unknown_dependent_function;
			/* TRUE if we don't know the routine type because
			   we're in a prototype instantiation and the
			   function to be called is given by a
			   template-dependent expression. */
  a_boolean	have_param_info;
			/* TRUE if we have information on the remaining
			   parameters.  Can be FALSE because
			     (a) The called function has a bad type;
			     (b) The called function has an old-style
			         parameter list and no body (hence no
			         parameter declarations);
			     (c) We're in the ellipsis section of a prototyped
			         function call (including a printf/scanf-type
			         routine);
			     (d) We're in the varargs section of an old-style
			         function call;
			     (e) We're scanning extra arguments after issuing
			         an error about there being too many arguments;
			         or
			     (f) We're scanning the arguments for an
			         overloaded function call or a
			         template-dependent call. */
  a_param_type_ptr
		curr_param_type;
			/* The current parameter type entry, if there is one;
			   NULL otherwise. */
  a_boolean	prototyped;
			/* TRUE if the function is prototyped. */
  a_boolean	has_ellipsis;
			/* TRUE if the function has an ellipsis. */
  a_pragma_kind	arg_list_kind;
			/* The kind of any pragma that applies to the
			   parameter list. */
  int		varargs_count;
			/* The argument count for the varargs lint comment. */
  int		arg_ctr;
			/* The current argument number. */
  an_expr_node_ptr
		argument_head;
			/* The head of the list of argument expressions
			   collected so far. */
  an_expr_node_ptr
		argument_tail;
			/* The tail of the list of argument expressions
			   collected so far. */
#if GNU_EXTENSIONS_ALLOWED
  int		fmt_arg;
			/* If arg_list_kind is pk_printf_args or
			   pk_scanf_args, the argument number
			   containing the format string, or zero if
			   the format string is the last argument
			   before the ellipsis. */
#endif /* GNU_EXTENSIONS_ALLOWED */
  char		*fmt_string;
			/* When checking a printf- or scanf-like function,
			   points to the format string.  NULL otherwise. */
  a_printf_scan_state
		pss;
			/* Current state for printf/scanf argument checking. */
  a_source_position
		closing_paren_position;
			/* Source position of the closing parenthesis of
			   the call. */
} an_arg_check_block;

extern void display_object_type(a_type_ptr object_type);

extern void free_arg_match_summary_list(an_arg_match_summary_ptr amsp);

extern a_type_ptr operand_complete_object_type(an_operand *operand,
                                               a_boolean  call_case);

extern void issue_warning_from_arg_match_summary(
                                            an_arg_match_summary_ptr amsp,
                                            a_source_position        *err_pos);

extern a_symbol_ptr find_addr_of_overloaded_function_match(
                                a_symbol_ptr       ovl_sym,
                                a_boolean          is_template_id,
                                a_template_arg_ptr template_arg_list,
                                a_type_ptr         dest_type,
                                a_boolean          is_cast,
                                an_arg_match_level *match_level,
                                a_std_conv_descr   *std_conv,
                                a_boolean          *unknown_dependent_function,
                                a_boolean          *ambiguous);

extern void choose_function_and_make_address_constant(
                                             a_symbol_ptr   sym,
                                             a_boolean      is_template_id,
                                             a_template_arg *template_arg_list,
                                             a_type_ptr     guide_type,
                                             a_constant_ptr constant,
                                             a_boolean      *err);

extern void selector_match_with_this_param(
                               an_operand           *bound_function_selector,
                               a_boolean            selector_is_object_pointer,
                               a_routine_ptr        rout,
                               a_type_ptr           this_param_type,
                               an_arg_match_summary *arg_summary);

extern a_symbol_ptr select_overloaded_function(
                         a_symbol_ptr             overloaded_function_symbol,
                         a_boolean                is_template_id,
                         a_template_arg_ptr       template_arg_list,
                         a_boolean                have_selector,
                         an_operand               *bound_function_selector,
                         an_arg_operand_ptr       arg_operand_list,
                         a_boolean                do_arg_dep_lookup,
                         an_error_code            err_none_applies,
                         an_error_code            err_ambiguous,
                         a_source_position        *call_position,
                         a_token_sequence_number  paren_tok_seq_number,
                         a_boolean                *single_function,
                         a_boolean                *unknown_dependent_function,
                         a_symbol_ptr             *surrogate_function_conv_sym,
                         an_arg_match_summary_ptr *arg_match_list);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_boolean overloaded_function_match_possible(
                               a_symbol_ptr       overloaded_function_symbol,
                               a_boolean          is_template_id,
                               a_template_arg_ptr template_arg_list,
                               an_arg_operand_ptr arg_operand_list,
                               a_boolean          have_selector,
                               an_operand         *bound_function_selector,
                               a_boolean          selector_is_object_pointer);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void make_constructor_dynamic_init(a_routine_ptr     ctor_routine,
                                          an_expr_node_ptr  arg_expr_list,
                                          a_type_ptr        temp_type,
                                          a_boolean         result_is_addr,
                                          a_boolean         is_explicit_cast,
                                          a_boolean         is_value_init,
                                          a_source_position *position,
                                          an_operand        *result);

extern void temp_init_from_operand(an_operand *operand);

extern void overloaded_function_catch_up(
                                  a_symbol_ptr      function_symbol,
                                  a_symbol_ptr      overloaded_function_symbol,
                                  a_boolean         is_qualified_name,
                                  a_source_position *function_position,
                                  a_source_position *id_position,
                                  a_boolean         elided_reference,
                                  a_boolean         address_taken,
                                  an_operand        *operand,
                                  a_boolean         *access_error_reported);

extern void combine_unneeded_selector_with_operand(
                                           an_operand *bound_function_selector,
                                           a_boolean  is_arrow_operator,
                                           an_operand *operand);

extern void cast_pointer_for_field_selection(
                               an_operand        *operand_1,
                               a_boolean         *is_arrow_operator,
                               a_symbol_ptr      member_sym,
                               a_symbol_ptr      projection_member_sym,
                               a_boolean         access_control_error_reported,
                               a_boolean         do_protected_member_check,
                               a_source_position *member_pos);

extern a_boolean variable_this_exists(a_variable_ptr *this_var);

extern void make_this_variable_operand(a_variable_ptr this_var,
                                       a_boolean      is_implicit,
                                       an_operand     *result);

extern a_boolean make_this_pointer_operand(
                               a_symbol_ptr      member_sym,
                               a_symbol_ptr      projection_member_sym,
                               a_source_position *member_pos,
                               a_boolean         access_control_error_reported,
                               an_operand        *result);

extern void start_call_argument_processing(a_type_ptr         function_type,
                                           a_routine_ptr      routine,
                                           an_arg_check_block *arg_block);

extern void process_call_argument(an_operand         *argument_operand,
                                  an_arg_check_block *arg_block);

extern void process_end_of_call_arguments(an_arg_check_block *arg_block);

extern void change_refs_on_selector_if_const_function(
                                          a_type_ptr routine_type,
                                          an_operand *bound_function_selector);

extern void adjust_overloaded_function_call_arguments(
                           a_symbol_ptr             function_symbol,
                           a_boolean                unknown_dependent_function,
                           a_type_ptr               routine_type,
                           a_boolean                have_selector,
                           an_operand               *bound_function_selector,
                           an_arg_operand_ptr       arg_operand_list,
                           an_arg_match_summary_ptr arg_match_list,
                           an_expr_node_ptr         *arg_expr_list);

extern a_type_ptr select_and_prepare_to_call_overloaded_function(
                           a_symbol_ptr            overloaded_function_symbol,
                           a_boolean               is_template_id,
                           a_template_arg_ptr      template_arg_list,
                           a_boolean               have_selector,
                           an_operand              *bound_function_selector,
                           an_arg_operand_ptr      arg_operand_list,
                           a_boolean               do_arg_dep_lookup,
                           a_boolean               try_surrogate_functions,
                           a_boolean               is_qualified_name,
                           an_error_code           err_none_applies,
                           an_error_code           err_ambiguous,
                           a_source_position       *call_position,
                           a_token_sequence_number paren_tok_seq_number,
                           a_source_position       *function_position,
                           a_source_position       *id_position,
                           a_source_position       *closing_paren_position,
                           a_boolean               *unknown_dependent_function,
                           an_operand              *function_operand,
                           an_expr_node_ptr        *arg_expr_list);

a_boolean conversion_from_class_possible(
                            an_operand               *source_operand,
                            a_type_ptr               dest_type,
                            a_builtin_type_kind_set  builtin_types_allowed,
                            a_boolean                need_lvalue_result,
                            a_boolean                is_copy_initialization,
                            a_boolean                is_reference_binding,
                            a_conv_descr             *conversion,
                            a_boolean                *ambiguous,
                            a_candidate_function_ptr *ambiguity_list);

extern void try_to_convert_class_operand_to_builtin_type(
                                 an_operand              *operand,
                                 a_builtin_type_kind_set builtin_types_allowed,
                                 a_boolean               *processed);

extern void check_for_operator_overloading(
                               an_opname_kind          kind,
                               a_boolean               unary_operator,
                               a_boolean               must_be_member_function,
                               a_boolean               try_conversions,
                               a_boolean               has_predef_meaning,
                               an_operand              *operand_1,
                               an_operand              *operand_2,
                               a_source_position       *operator_position,
                               a_token_sequence_number operator_tok_seq_number,
                               an_operand              *result,
                               a_boolean               *processed);

extern a_boolean conversion_to_class_possible(
                            an_operand               *source_operand,
                            a_type_ptr               dest_type,
                            a_boolean                try_bitwise_copy,
                            a_boolean                is_copy_initialization,
                            a_boolean                is_reference_binding,
                            a_conv_descr             *conversion,
                            a_conv_descr             *ctor_arg_conversion,
                            a_boolean                *ambiguous,
                            a_candidate_function_ptr *ambiguity_list);

extern void bind_member_function_operand_to_selector(
                                      an_operand *function_operand,
                                      an_operand *bound_function_selector);

extern a_boolean user_defined_conversion_possible(
                                        an_operand   *source_operand,
                                        a_type_ptr   dest_type,
                                        a_boolean    need_lvalue_result,
                                        a_boolean    is_copy_initialization,
                                        a_boolean    is_reference_binding,
                                        a_conv_descr *conversion,
                                        a_conv_descr *ctor_arg_conversion,
                                        a_boolean    *failed);

extern void user_convert_operand(
                           an_operand   *operand,
                           a_type_ptr   dest_type,
                           a_conv_descr *conversion,
                           a_conv_descr *ctor_arg_conversion,
                           a_boolean    force_temp_for_class_bitwise_copy,
                           a_boolean    is_explicit_cast);

extern void prep_elision_initializer_operand(
                                            an_operand         *source_operand,
                                            a_type_ptr         dest_type,
                                            a_boolean          fill_in_dtor,
                                            an_error_code      err_code,
                                            a_dynamic_init_ptr *dip);


extern a_boolean direct_reference_binding_possible(
                                       an_operand   *source_operand,
                                       a_type_ptr   source_type,
                                       a_type_ptr   dest_type,
                                       a_boolean    *ref_to_const,
                                       a_boolean    *ref_to_const_volatile,
                                       a_boolean    *binding_to_rvalue_allowed,
                                       a_boolean    *dropping_qualifiers,
                                       a_symbol_ptr *function_symbol);

extern void prep_initializer_operand(an_operand    *source_operand,
                                     a_type_ptr    dest_type,
                                     a_boolean     *is_transparent,
                                     a_conv_descr  *conversion,
                                     a_boolean     initializing_return_value,
                                     a_boolean     initializing_variable,
                                     a_boolean     static_lifetime,
                                     a_boolean     is_copy_initialization,
                                     a_boolean     nontype_template_arg,
                                     an_error_code incompatible_err);

extern void prep_arg_passed_via_copy_constructor(an_operand    *source_operand,
                                                 a_type_ptr    param_type,
                                                 a_conv_descr  *conversion,
                                                 an_error_code err_code);

extern void prep_argument_operand(an_operand       *source_operand,
                                  a_param_type_ptr formal_param,
                                  a_conv_descr     *conversion,
                                  an_error_code    err_code);

extern void prep_assignment_operand(an_operand        *source_operand,
                                    a_type_ptr        dest_type,
                                    an_error_code     incompatible_err,
                                    a_source_position *err_pos);

extern a_boolean nontype_template_arg_conversion_possible(
                                                        an_operand *operand,
                                                        a_type_ptr param_type);

extern a_boolean conditional_operator_conversion_possible(
                                                   an_operand   *op1,
                                                   an_operand   *op2,
                                                   a_conv_descr *conv,
                                                   a_boolean    *ambiguous);

extern a_symbol_ptr select_overloaded_copy_constructor
                                  (a_type_ptr            class_type,
                                   a_type_qualifier_set  required_qualifiers,
                                   a_boolean             source_is_rvalue,
                                   a_source_position     *pos,
                                   a_boolean             *ambiguous,
                                   a_boolean             *uncallable,
                                   a_boolean             *class_bitwise_copy);

extern void overload_init(void);

#endif /* ifndef OVERLOAD_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
