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

def_arg.h -- Declarations related to def_arg.c (having to do with
	     default argument processing).

*/

/* Avoid including these declarations more than once: */
#ifndef DEF_ARG_H
#define DEF_ARG_H 1

#ifndef LANG_FEAT_H
#include "lang_feat.h"
#endif /* ifndef LANG_FEAT_H */
#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */

/*
Structure for keeping track of token cache representing a default argument
expression, prescanned during a member function declaration within a class
definition and actually processed once the class definition is complete.
The declaration for a_def_arg_expr_fixup_ptr is in symbol_tbl.h.
*/
typedef struct a_def_arg_expr_fixup {
  a_def_arg_expr_fixup_ptr
		next;
			/* Next in a linked list of entries representing
			   default argument expressions for the parameters
			   of a given function. */
  a_token_cache token_cache;
			/* A pointer to the token cache that describes the
			   default argument expression. */
  a_param_type_ptr
		param_type;
			/* A pointer to the param type entry in which the
			   expression node is to be stored once its tokens
			   have been scanned. */
} a_def_arg_expr_fixup;

extern void prescan_default_arg_expr(a_token_cache	*token_cache,
				     a_boolean          is_template_param);

extern void prescan_default_function_arg_expr(a_param_type_ptr 	      ptp,
				              a_def_arg_expr_fixup_ptr *list);

extern void delayed_scan_of_default_arg_expr
				(a_param_type_ptr param_type_entry,
                                 a_boolean        check_for_errors);

extern void delayed_scan_of_template_default_arg_expr(a_type_ptr     type,
					              a_constant_ptr constant);

extern void free_def_arg_expr_fixup(a_def_arg_expr_fixup_ptr  daefp);

extern void def_arg_init(void);

#if DEBUG
extern unsigned long db_show_def_arg_expr_fixups_used(
                                                   unsigned long  grand_total);
#endif /* DEBUG */

#endif /* DEF_ARG_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
