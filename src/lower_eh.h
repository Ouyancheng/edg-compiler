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

lower_eh.h -- Declarations related to lower_eh.c (having to do with IL
              lowering for exception handling constructs).

*/

/* Avoid including these declarations more than once: */
#ifndef LOWER_EH_H
#define LOWER_EH_H 1

/* Only include this code if it is needed: */
#if DO_IL_LOWERING

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef LOWER_IL_H
#include "lower_il.h"
#endif /* ifndef LOWER_IL_H */


extern void define_scope_class_typeinfo_vars(a_scope_ptr scope);

extern void type_is_used_in_exception(a_type_ptr type);

extern void lower_throw(an_expr_node_ptr expr);

extern a_variable_ptr make_caught_object_address_var(void);

extern void make_region_table_entry(a_cleanup_action_ptr cap,
                                    an_insert_location   *insert_location);

extern void set_eh_curr_region(a_context_ptr      context,
                               an_insert_location *insert_location);

extern void add_eh_function_prologue(a_scope_ptr scope);

extern void initialize_catch_parameter(a_handler_ptr handler);

extern void lower_try_block(a_statement_ptr statement);

extern void eh_function_lower_init(void);

extern void eh_lower_init(void);

#endif /* DO_IL_LOWERING */
#endif /* ifndef LOWER_EH_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1993 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
