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
typedef struct a_reachability_summary {
  a_boolean	reachable;
			/* Code is reachable, as determined by the front
			   end. */
  a_boolean	reachable_considering_hints;
			/* Code is reachable, as determined by the front
			   end and modified by user hints in the code. */
  a_boolean	suppress_unreachable_warning;
			/* In an unreachable code section, suppress the
			   warning about unreachable code (because it has
			   already been issued, or because of a lint-style
			   comment). */
} a_reachability_summary;

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
  ssk_for,		/* for (...; ...; ...) {} statement. */
  ssk_try_block		/* try compound-stmt handler-seq statement. */
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
		extra_block;
			/* If non-NULL, points to an stmk_block statement
			   added under the primary statement for this
			   structured statement in order to allow attaching
			   more than one statement under a statement that
			   allows only one. */
  a_statement_ptr
		last_dep_statement;
			/* Points to the last dependent statement under
			   the structured statement (or under extra_block,
			   if that is non-NULL).  NULL if there are
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
  unsigned int  for_init:1;
			/* TRUE if the structured statement is a for loop and
			   the statement currently being processed is a
			   for-init statement; FALSE otherwise. */
  a_reachability_summary
		start_reachable;
			/* Indicates whether or not the start of the structured
			   statement is reachable. */
  a_reachability_summary
		end_reachable;
			/* Indicates whether or not the end of the structured
			   statement is reachable. */
  unsigned long
		init_count;
			/* If kind == ssk_compound, a count of initializing
			   declarations that have appeared within the
			   block. */
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
                                          a_boolean explicit_return_type,
                                          a_boolean is_catch_clause);
extern a_boolean curr_code_reachable(void);

/* Structure for saving the current state of the structured statement stack
   so that it can be reinitialized to handle a nested function and then
   restored to continue processing the current function. */
typedef struct a_struct_stmt_stack_state {
  a_ptrdiff	container_pos;
			/* Saved position of the current struct_stmt_stack
			   within struct_statement_stack_container (a static
			   variable of statements.c). */
  int		depth_stmt_stack;
			/* Saved value of global variable depth_stmt_stack. */
  a_reachability_summary
		code_reachability;
			/* Saved value of code_reachability (a static variable
			   of statements.c). */
} a_struct_stmt_stack_state;

extern void new_struct_stmt_stack(a_struct_stmt_stack_state *saved_state);
extern void restore_struct_stmt_stack(a_struct_stmt_stack_state *saved_state);

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
