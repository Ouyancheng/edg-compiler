/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1991 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

statements.h -- Declarations relating to statements.c (having to do with
                scanning of statements).

*/

/* Avoid including these declarations more than once: */
#ifndef STATEMENTS_H
#define STATEMENTS_H 1

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */

EXTERN int	depth_stmt_stack;
			/* Index of the current entry in struct_stmt_stack.
			   -1 if the stack is empty. */

extern a_statement_ptr add_statement(a_statement_kind kind);
extern a_statement_ptr compound_statement(a_boolean at_function_level,
                                          a_boolean explicit_return_type);
extern a_boolean curr_code_reachable(void);
extern void new_struct_stmt_stack(sizeof_t  *saved_container_pos,
                                  sizeof_t  *saved_depth_stmt_stack,
                                  int       *saved_code_reachable);
extern void restore_struct_stmt_stack(sizeof_t  saved_container_pos,
                                      sizeof_t  saved_depth_stmt_stack,
                                      int       saved_code_reachable);

#endif /* ifndef STATEMENTS_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1991 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
