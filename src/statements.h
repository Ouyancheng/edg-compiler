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
#ifndef PRAGMA_H
#include "pragma.h"
#endif /* ifndef PRAGMA_H */

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
			   is no current switch clause.  It is set only for
			   simple clauses, those begun by case labels appearing
			   directly within the switch statement or a top-level
			   compound statement.  When kind != stmk_switch,
			   if this statement is nested within a switch and
			   it contains case labels, this points to the case
			   clause for the case label most recently encountered;
			   otherwise, it is NULL. */
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
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_statement_ptr
		curr_decl_statement;
			/* When kind == ssk_compound, pointer to the current
			   stmk_decl statement, if any, governing a series of
			   declarations; when a statement that is not a
			   declaration is reached, this pointer is cleared; it
			   is reset once a new declaration is encountered. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
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
  unsigned int	saved_curr_construct_pragma_list_is_on_stmt_stack:1;
			/* Used to save the value of the scope stack entry
			   flag that indicates where the active
			   curr_construct_pragmas list pointer resides. */
  a_reachability_summary
		start_reachable;
			/* Indicates whether or not the start of the structured
			   statement is reachable. */
  a_reachability_summary
		end_reachable;
			/* Indicates whether or not the end of the structured
			   statement is reachable. */
  a_pending_pragma_ptr
		curr_construct_pragmas;
			/* The current construct pragma list is stored on
			   the top scope stack entry or the top statement
			   stack entry, whichever is most recent.  When
			   this statement stack entry is the most recent
			   of the two, this field contains a pointer to
			   the pragma entries to be bound to the current
			   statement or declaration. */
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

extern a_statement_ptr add_statement_at_stmt_pos(a_statement_kind   kind,
                                                 a_source_position  *stmt_pos);
extern a_statement_ptr compound_statement(a_boolean at_function_level,
                                          a_boolean explicit_return_type,
                                          a_boolean is_catch_clause);

extern void warn_if_code_is_unreachable(an_error_code      error_code,
                                        a_source_position  *err_pos);

/*
a_control_flow_desr is an entry used in tracking gotos, labels, and
initializing declarations in order to diagnose errors in transferring
control past an initialization.
*/
enum a_control_flow_descr_kind_tag {
  cfdk_block,		/* Start of a block. */
  cfdk_init,		/* Refers to an stmk_init statement. */
  cfdk_goto,		/* Refers to an stmk_goto statement. */
  cfdk_label,		/* Refers to an stmk_label statement. */
  cfdk_case_label,	/* Case label in switch statement. */
  cfdk_end_of_block	/* End of a block. */
};
/* Define as "a_byte" to explicitly control storage size. */
typedef a_byte a_control_flow_descr_kind;

typedef struct a_control_flow_descr *a_control_flow_descr_ptr;
typedef struct a_control_flow_descr {
  a_control_flow_descr_ptr
		next;
			/* Pointer to the next entry in a linked list; NULL
			   for the last entry on the list. */
  a_control_flow_descr_ptr
		prev;
			/* Pointer to the preceding entry in a linked list;
			   NULL for the first entry on the list. */
  a_control_flow_descr_ptr
		parent;
			/* Pointer to an entry representing the parent
			   block of the given entry.  NULL only for the
			   cfdk_block and cfdk_end_of_block entries associated
			   the routine scope; all other entries on a list
			   have parents. */
  a_source_position
		source_pos;
			/* Source position of the goto statement. */
  a_control_flow_descr_kind
		kind;
			/* The kind of entry. */
#if DEBUG
  unsigned long id_number;
			/* Unique identifying number for this entry. */
#endif /* DEBUG */
  union {
    /* When kind == cfdk_case_label: no variant fields */
    /* When kind == cfdk_block: */
    struct {
      a_control_flow_descr_ptr
		end_of_block;
			/* An entry representing the start of a block has a
			   pointer to the entry representing the end of the
			   same block. */
      a_control_flow_descr_ptr
		last_case_label;
			/* When is_switch_block or is_switch_subblock is TRUE,
			   a pointer to the last case label in the current
			   block or a subblock of the current block.  NULL
			   when the block is not contained within a switch
			   statement or contains no case labels. */
      unsigned long
		goto_count;
			/* Number of goto statements in the current block and
			   blocks contained within the current block.  This
			   counter is decremented as goto entries are
			   removed from the list. */
      unsigned int
		any_labels:1;
			/* TRUE if the current block contains any label
			   statements or any blocks with label statements. */
      unsigned int
		is_switch_block:1;
			/* TRUE if the current block represents the body
			   of a switch statement. */
      unsigned int
		is_switch_subblock:1;
			/* TRUE if is_switch is TRUE for a block in which the
			   current block is enclosed. */
      unsigned int
		exposed_init_in_switch:1;
			/* If is_switch is TRUE for this block or for a block
			   in which the current block is enclosed, there
			   has been at least one initializing declaration that
			   is "exposed" -- that is, that may give rise to a
			   jump-over-initialization diagnostic if it is
			   followed by a case label before the current block is
			   closed.  Once the block is closed or a case label
			   appears, the flag is cleared. */
      unsigned int
		is_handler_block:1;
			/* TRUE if this is the top level block of a handler
			   (i.e., the body of a catch clause). */
#if CHECKING
      unsigned int
		dummy:2;
			/* Extra field that can be initialized to prevent
			   spurious reference to uninitialized data warnings
			   from CodeCenter. */
#endif /* CHECKING */
    } block;
    /* When kind == cfdk_init: */
    a_statement_ptr
		init_statement;
			/* A pointer to an stmk_init statement. */
    /* When kind == cfdk_goto: */
    struct {
      a_statement_ptr
		ptr;
			/* A pointer to an stmk_goto statement. */
      a_control_flow_descr_ptr
		prev_goto;
			/* A pointer to a cfdk_goto entry that points to a
			   goto statement referring to the same label; NULL
			   for the first goto statement for a given label in
			   the program.  This pointer produces a chain that
			   can be walked to visit all forward gotos referring
			   to a given label. */
    } goto_statement;
    /* When kind == cfdk_label: */
      a_statement_ptr
		label_statement;
			/* A pointer to an stmk_label statement. */
    /* When kind == cfdk_end_of_block: */
    a_control_flow_descr_ptr
		start_of_block;
			/* An entry representing the end of a block has a
			   pointer to the entry representing the start of the
			   same block. */
  } variant;
} a_control_flow_descr;


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
  a_control_flow_descr_ptr
		control_flow_list;
			/* Saved pointer to head of control flow list. */
  a_control_flow_descr_ptr
		end_of_control_flow_list;
			/* Saved pointer to tail of control flow list. */
} a_struct_stmt_stack_state;

extern void new_struct_stmt_stack(a_struct_stmt_stack_state *saved_state);
extern void restore_struct_stmt_stack(a_struct_stmt_stack_state *saved_state);

extern void statements_init(void);

#if DEBUG
/* Show and return the amount of memory used by statements entries. */
extern unsigned long show_statements_space_used(void);
#endif /* DEBUG*/

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
