/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1995 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

inline.h -- Declarations related to inline.c (minimal inlining for IL
            lowering).

*/

/* Avoid including these declarations more than once: */
#ifndef INLINE_H
#define INLINE_H 1

/* Only include this code if it is needed: */
#if DO_IL_LOWERING
#if MINIMAL_INLINING

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_DEF_H */


EXTERN a_boolean
		currently_doing_inlining_of_function_call;
			/* TRUE if currently doing the expansion of an
			   inline function call. */
/*
Entry used to record information about the remapping to be done on a variable
while expanding an inline function call.
*/
typedef enum /*a_variable_remapping_kind*/ {
  vrk_none,		/* No remapping; used in entries that exist only to
			   record the arg_expr and arg_expr_next fields. */
  vrk_temporary,	/* Variable is remapped to a temporary variable. */
  vrk_constant,		/* Variable is remapped to a constant. */
  vrk_addr_variable,	/* Variable is remapped to the address of a
			   variable. */
} a_variable_remapping_kind;
typedef struct a_variable_remapping_for_inlining
                                        *a_variable_remapping_for_inlining_ptr;
typedef struct a_variable_remapping_for_inlining {
  a_variable_remapping_for_inlining_ptr
		next;	/* Used to link entries on a list. */
  a_variable_ptr
		orig_variable;
			/* The original variable, i.e. the one being
			   rewritten. */
  a_variable_remapping_kind
		kind;
			/* Kind of remapping. */
  union {
    /* When kind == vrk_none, no variant fields. */
    /* When kind == vrk_temporary or vrk_addr_variable: */
    a_variable_ptr
		variable;
    /* When kind == vrk_constant: */
    a_constant_ptr
		constant;
  } variant;
  /* Information used if this remapping came from an argument.  It is needed
     to restore the "next" pointer between argument expressions if the
     inlining fails. */
  an_expr_node_ptr
		arg_expr,
		arg_expr_next;
			/* Argument expression and the original "next" pointer
			   thereof.  arg_expr is NULL if this information is
			   not applicable. */
} a_variable_remapping_for_inlining;


EXTERN a_variable_remapping_for_inlining_ptr
		avail_variable_remappings_for_inlining;
			/* List of remapping entries that have been freed
			    and are available for reuse. */

#if DEBUG
/*
Count of entries allocated, for debugging purposes.
*/
EXTERN unsigned long
		num_variable_remappings_for_inlining;
#endif /* DEBUG */


extern a_variable_ptr remap_var_for_inlining(a_variable_ptr var);

extern void adjust_copied_expression_for_inlining(an_expr_node_ptr expr);

extern a_boolean copy_and_simplify_short_circuited_operation(
                                                        an_expr_node_ptr expr);

extern void do_inlining_of_call(an_expr_node_ptr expr,
                                a_statement_ptr  statement);

extern void set_up_routine_for_inlining(a_scope_ptr scope);

extern void mark_inlined_routines_as_unreferenced(void);

extern void inline_one_time_init(void);

extern void inline_init(void);

#endif /* MINIMAL_INLINING */
#endif /* DO_IL_LOWERING */
#endif /* ifndef INLINE_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1995 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
