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

statements.h -- Declarations relating to statements.c (having to do with
                scanning of statements).

*/

/* Avoid including these declarations more than once: */
#ifndef STATEMENTS_H
#define STATEMENTS_H 1

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */

/*
Indication of whether or not code is reachable from the code immediately
preceding.
*/
typedef enum /*a_reachability_code*/ {
  rc_reachable,		/* Code is reachable. */
  rc_unreachable,	/* Code is unreachable. */
  rc_unreachable_error_given
			/* Code is unreachable, and a warning to that effect
			   has already been issued, or the warning has been
			   suppressed by a lint-style "notreached" comment.
			   Such code is considered reachable, either from
			   the preceding dead code, or because a "notreached"
			   comment is wrong (we cannot, after all, allow a
			   comment to affect the semantics of the program). */
} a_reachability_code;

/*
Stack indicating nesting of structured statements.  There is an entry
on this stack for each current structured statement.  A structured statement
is one that can contain other statements.
*/
typedef enum /*a_struct_stmt_kind*/ {
  /* Types of structured statements. */
  ssk_compound,		/* Compound statement, i.e., { ... }. */
  ssk_if,		/* if statement. */
  ssk_switch,		/* switch statement. */
  ssk_while,		/* while (...) {} statement. */
  ssk_do,		/* do {} while (...); statement. */
  ssk_for		/* for (...; ...; ...) {} statement. */
} a_struct_stmt_kind;

typedef struct a_struct_stmt_stack_entry *a_struct_stmt_stack_entry_ptr;
typedef struct a_struct_stmt_stack_entry {
  /* An entry on the structured statement stack, describing one
     current structured statement. */
  a_struct_stmt_kind
		kind;	/* Kind of structured statement. */
  a_boolean	in_else_of_if;
			/* TRUE when kind == ssk_if and we are in the
			   "else" clause. */
  a_statement_ptr
		statement;
			/* The associated IL statement.  Indirectly,
			   also gives the pointer to the first dependent
			   statement of the structured statement. */
  a_switch_clause_ptr
		curr_switch_clause;
			/* When kind == stmk_switch, this points to the
			   current switch clause, or is NULL if there
			   is no current switch clause. */
  a_statement_ptr
		last_dep_statement;
			/* Points to the last dependent statement under
			   the structured statement.  NULL if there are
			   no dependent statements. */
  a_label_ptr	break_label,
		continue_label;
			/* Labels to be branched to for a break or
			   continue out of this statement.  NULL until
			   needed. */
  a_type_ptr	switch_selector_type;
			/* The type of the switch selector expression
			   (int or long). */
  unsigned int	switch_has_default_clause:1;
			/* TRUE if the structured statement is a switch and
			   it has a default clause. */
  unsigned int	rout_type_explicitly_specified:1;
			/* TRUE if the current routine was declared with an
			   explicit return type.  This flag is set in the
			   top level statement stack entry only. */
  unsigned int	any_exec_statement_seen:1;
			/* Within compound statements (blocks), TRUE if any
			   executable statement (not declaration) has been
			   seen. */
  a_reachability_code
		start_reachable;
			/* Indicates whether or not the start of the structured
			   statement is reachable. */
  a_reachability_code
		end_reachable;
			/* Indicates whether or not the end of the structured
			   statement is reachable.  The end is reachable
			   if the end of any clause is reachable or if it's
			   possible to execute none of the clauses. */
} a_struct_stmt_stack_entry;
EXTERN a_struct_stmt_stack_entry_ptr
		struct_stmt_stack
#if VAR_INITIALIZERS
                                  = NULL
#endif /* VAR_INITIALIZERS */
                                        ;
			/* The currently active structured statement stack
			   itself.  The current entry is [depth_stmt_stack].
			   Entry [0] is for the main block of the current
			   function, if we are currently inside a function.
			   Note that in C++ there can be more than one such
			   stack, though only one is active at a time.  The
			   struct_stmt_stack array is actually a subarray of
			   struct_stmt_stack_container. */

EXTERN int	depth_stmt_stack
#if VAR_INITIALIZERS
                                 = -1
#endif /* VAR_INITIALIZERS */
                                     ;
			/* Index of the current entry in struct_stmt_stack.
			   -1 if the stack is empty. */

extern a_statement_ptr add_statement(a_statement_kind kind);
extern a_statement_ptr compound_statement(a_boolean at_function_level,
                                          a_boolean explicit_return_type);
extern a_boolean curr_code_reachable(void);
extern void new_struct_stmt_stack(int  *saved_container_pos,
                                  int  *saved_depth_stmt_stack,
                                  int  *saved_code_reachable);
extern void restore_struct_stmt_stack(int  saved_container_pos,
                                      int  saved_depth_stmt_stack,
                                      int  saved_code_reachable);

#endif /* ifndef STATEMENTS_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
