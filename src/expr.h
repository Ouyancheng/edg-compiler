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

expr.h -- Declarations related to expression parsing.

*/

/* Avoid including these declarations more than once: */
#ifndef EXPR_H
#define EXPR_H 1

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */


/*
Entry describing a function that is a candidate instance of an overloaded
function.  This entry is used is resolving overloaded function calls.
*/
typedef struct a_candidate_function *a_candidate_function_ptr;
typedef struct a_candidate_function {
  a_candidate_function_ptr
		next;	/* Next entry on the list of candidates, or NULL
			   if this is the last entry. */
  a_symbol_ptr	function_symbol;
			/* Pointer to the symbol for the function. */
} a_candidate_function;
/*
Entry describing how well a given actual argument matches the corresponding
formal parameter, used in resolving overloaded function calls.
*/
typedef enum /*an_argument_match_level*/ {
  /* Match levels -- See ARM 13.2. */
  aml_none,		/* No match. */
  aml_exact,		/* Exact match or trivial conversions. */
  aml_promotions,	/* Match with promotions. */
  aml_standard_conv,	/* Match with standard conversions. */
  aml_user_conv,	/* Match with user-defined conversions. */
  aml_ellipsis,		/* Match with ellipsis. */
  aml_error		/* Match with error type (not in ARM). */
} an_argument_match_level;
typedef struct an_argument_match_summary *an_argument_match_summary_ptr;
typedef struct an_argument_match_summary {
  an_argument_match_summary_ptr
		next;	/* Pointer to entry for following argument, or NULL
			   if this is the last argument.  Also used to link
			   entries on the avail_argument_match_summries
			   list. */
  an_argument_match_level
		match_level;
			/* Match level -- see ARM 13.2.  Primary key. */
  unsigned long	downward_cast_levels;
			/* If the compatibility involves a downward cast,
			   this is the count of levels; for a cast to void *,
			   ULONG_MAX.  Zero otherwise.  Secondary key. */
  a_byte_boolean
		qualifiers_added;
			/* Type qualifiers (const/volatile) were added; this
			   is the tertiary key, used as a tie-breaker. */
} an_argument_match_summary;


extern a_boolean node_has_side_effects(an_expr_node_ptr node);

extern a_boolean scan_ctor_arguments(a_symbol_ptr     constructor_sym,
                                     a_routine_ptr    *constr_rout,
                                     an_expr_node_ptr *arg_expr_list);

extern an_expr_node_ptr scan_expression(a_boolean *err);

extern an_expr_node_ptr scan_void_expression(a_boolean *err);

extern void scan_pp_expression(a_constant *constant,
			       a_boolean  *err);

extern void scan_integral_constant_expression(a_constant *constant,
			                      a_boolean  *err);

extern void scan_constant_initializer_expression
	      (a_boolean  convert_array_to_pointer,
	       a_constant *constant,
	       a_boolean  *err);

extern void scan_initializer_expression
	      (a_boolean        convert_array_to_pointer,
               a_boolean        *is_constant,
	       an_expr_node_ptr *expression,
	       a_constant       *constant,
	       a_boolean        *err);

extern an_expr_node_ptr scan_argument_expression(void);

extern a_boolean select_constructor(a_symbol_ptr      constructor_sym,
                                    a_routine_ptr     *constr_rout,
                                    an_expr_node_ptr  *arg_expr_list,
                                    a_source_position *err_pos);

extern an_expr_node_ptr scan_boolean_controlling_expression(a_boolean *err);

/*
Macro that is TRUE if the node is an operation node.
*/
#define is_operation_node(node)						\
	((node)->kind == (an_expr_node_kind)enk_operation)

/*
Macro that is TRUE if the node is a constant node.
*/
#define is_constant_node(node)						\
	((node)->kind == (an_expr_node_kind)enk_constant)

/*
Macro that is TRUE if the node is a variable node.
*/
#define is_variable_node(node)						\
	((node)->kind == (an_expr_node_kind)enk_variable)

/*
Macro that is TRUE if the node is a variable address node.
*/
#define is_variable_address_node(node)					\
	((node)->kind == (an_expr_node_kind)enk_variable_address)

/*
Macro that is TRUE if the node is a field node.
*/
#define is_field_node(node)						\
	((node)->kind == (an_expr_node_kind)enk_field)

#endif /* ifndef EXPR_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/

