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


typedef struct a_control_flow_descr *a_control_flow_descr_ptr;
/*
a_control_flow_desr is an entry used in tracking gotos, labels, and
initializing declarations in order to diagnose errors in transferring
control past an initialization.
*/
enum a_control_flow_descr_kind_tag {
  cfdk_block,		/* Start of a block. */
  cfdk_init,		/* Refers to an stmk_init, stmk_set_vla_size, or
                           stmk_vla_decl statement. */
  cfdk_goto,		/* Refers to an stmk_goto statement. */
  cfdk_label,		/* Refers to an stmk_label statement. */
  cfdk_case_label,	/* Case label in switch statement. */
  cfdk_end_of_block	/* End of a block. */
};
/* Define as "a_byte" to explicitly control storage size. */
typedef a_byte a_control_flow_descr_kind;


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
#if UPC_EXTENSIONS_ALLOWED
  a_statement_ptr
		enclosing_forall;
			/* Used to track and match up enclosing forall
			   statements for gotos and labels.  Gotos to labels in
			   different forall statements, or into or out of
			   forall statements, are not allowed.  */
#endif /* UPC_EXTENSIONS_ALLOWED */
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
      an_object_lifetime_ptr
		object_lifetime;
			/* Pointer to the object lifetime, if any, pushed for
			   the current block. */
      unsigned long
		goto_count;
			/* Number of goto statements in the current block and
			   blocks contained within the current block.  This
			   counter is decremented as goto entries are
			   removed from the list. */
      a_bit_field
		any_labels:1;
			/* TRUE if the current block contains any label
			   statements or any blocks with label statements. */
      a_bit_field
		any_vla_variables:1;
			/* TRUE if the current block contains any vla-decl
			   statements that represent the declaration of a
			   VLA variable (i.e., not a typedef or variable
			   declaration involving a variably modified
			   type). */
      a_bit_field
		is_switch_block:1;
			/* TRUE if the current block represents the body
			   of a switch statement. */
      a_bit_field
		is_switch_subblock:1;
			/* TRUE if is_switch is TRUE for a block in which the
			   current block is enclosed. */
      a_bit_field
		exposed_init_in_switch:1;
			/* If is_switch is TRUE for this block or for a block
			   in which the current block is enclosed, there
			   has been at least one initializing declaration that
			   is "exposed" -- that is, that may give rise to a
			   jump-over-initialization diagnostic if it is
			   followed by a case label before the current block is
			   closed.  Once the block is closed or a case label
			   appears, the flag is cleared. */
      a_bit_field
		is_catch_block:1;
			/* TRUE if this is the top level block of a catch
			   clause. */
      a_bit_field
		is_try_block:1;
			/* TRUE if this is the top level block of a try
			   statement. */
      a_bit_field
		is_statement_expr:1;
			/* TRUE if this is the top level block of a GNU
			   statement expression, i.e., ({ ... }). */
      a_bit_field
		is_within_goto_protected_block:1;
			/* TRUE if this block is or is contained within
			   a block for which transfers of control into the
			   block are prohibited. */
    } block;
    /* When kind == cfdk_init: */
    struct {
      a_statement_ptr
		statement;
			/* A pointer to an stmk_init, stmk_set_vla_size, or
			   stmk_vla_decl statement.  (In addition, in
			   Microsoft C mode it can be an stmk_block
			   statement, which is what's left over if a dynamic
			   initialization is lowered in place.) */
      a_variable_ptr
		variable;
			/* A pointer to the variable that is dynamically
			   initialized.  NULL when the statement pointer
			   refers to an stmk_set_vla_size statement. */
      a_bit_field
		is_vla_variable;
			/* TRUE if the statement is an stmk_vla_decl
			   statement that represents the declaration of a
			   VLA variable, i.e., a variable that will require
			   allocation at runtime. */
    } init;
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
#if MICROSOFT_EXTENSIONS_ALLOWED
  ssk_microsoft_try,	/* Microsoft try-except or try-finally. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  ssk_try_block		/* try compound-stmt handler-seq statement. */
} a_struct_stmt_kind;

typedef struct a_struct_stmt_stack_entry *a_struct_stmt_stack_entry_ptr;
typedef struct a_struct_stmt_stack_entry {
  /* An entry on the structured statement stack, describing one
     current structured statement. */
  a_struct_stmt_kind
		kind;	/* Kind of structured statement. */
  a_bit_field	in_else_of_if:1;
			/* TRUE when kind == ssk_if and we are in the
			   "else" clause. */
  a_bit_field	for_init:1;
			/* TRUE if the structured statement is a for loop and
			   the statement currently being processed is a
			   for-init statement; FALSE otherwise. */
  a_bit_field	is_catch_clause:1;
			/* TRUE if kind == ssk_compound and this structured
			   statement represents the top level block of a
			   catch clause. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	in_cleanup_statement_of_microsoft_try:1;
			/* TRUE if currently inside the cleanup statement of
			   a Microsoft try-finally or try-except. */
  a_bit_field	in_handler_parameter_declaration:1;
			/* TRUE if currently inside the declaration of the
			   handler parameter. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_bit_field	switch_has_default_clause:1;
			/* TRUE if the structured statement is a switch and
			   it has a default clause. */
  a_bit_field	rout_type_explicitly_specified:1;
			/* TRUE if the current routine was declared with an
			   explicit return type.  This flag is set in the
			   top level statement stack entry only. */
  a_bit_field	any_exec_statement_seen:1;
			/* Within compound statements (blocks), TRUE if any
			   executable statement (not declaration) has been
			   seen. */
  a_bit_field	label_invalidates_curr_block_object_lifetime:1;
			/* TRUE if kind == ssk_compound and the object
			   lifetime pointed to by this entry has been
			   invalidated by a label in an inner block. This
			   flag will be cleared again once the required fixup
			   has been done and the curr_block_object_lifetime
			   pointer has been reset. */
  a_bit_field	is_statement_expr:1;
			/* TRUE if the statement is a GNU statement
			   expression, ({ ... }). */
  a_bit_field	inside_statement_expr:1;
			/* TRUE if the statement is or is inside of a
			   GNU statement expression. */
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
  a_constant_ptr
		discarded_case_label_constants;
			/* When kind == stmk_switch, this points to a list
			   constant entries that were discarded because they
			   belonged to the same switch-clause as a default
			   label; NULL otherwise. */
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
  a_label_ptr	break_label;
			/* Label to be branched to for a break out of this
			   statement.  NULL until needed. */
  a_control_flow_descr_ptr
		break_statements;
			/* Pointer to a linked list of control flow entries
			   identifying the break statements (if any) in this
			   structured statement. */
  a_label_ptr	continue_label;
			/* Label to be branched to for a continue of this loop
			   statement.  NULL until needed. */
  a_control_flow_descr_ptr
		continue_statements;
			/* Pointer to a linked list of control flow entries
			   identifying the continue statements (if any) in this
			   structured statement. */
  a_type_ptr	switch_selector_type;
			/* The type of the switch selector expression
			   (int or long). */
  a_reachability_summary
		start_reachable;
			/* Indicates whether or not the start of the structured
			   statement is reachable. */
  a_reachability_summary
		end_reachable;
			/* Indicates whether or not the end of the structured
			   statement is reachable. */
  an_object_lifetime_ptr
		curr_block_object_lifetime;
			/* If kind == ssk_compound, a pointer to the currently
			   active object lifetime directly associated with
			   this block (if any).  A lifetime is pushed when a
			   block starts, but a label in the midst of the
			   block "invalidates" the lifetime and a new one is
			   pushed to replace it; this pointer then points to
			   the new one. */
  a_scope_depth depth_of_assoc_scope;
			/* If kind == ssk_compound and a scope stack entry
			   was pushed in conjunction with this structured
			   statement stack entry, the depth of the former in
			   the scope stack; NO_SCOPE_DEPTH otherwise. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  unsigned long	num_microsoft_trys_inside_of;
			/* Number of Microsoft try-finally or try-except
			   statements currently on the stack. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_switch_clause_ptr
		last_switch_clause;
			/* If non-NULL, points to the last switch clause for
			   the current switch statement.  This is usually
			   the same as curr_switch_clause, but when the
			   latter is cleared at a "break" last_switch_clause
			   remains set. */
  a_constant_ptr
		last_const_in_last_switch_clause;
			/* Points to the last (i.e., maximum) constant in the
			   switch clause indicated by last_switch_clause.
			   NULL if that switch clause is empty so far. */
  a_constant_ptr
		switch_max_case_value;
			/* If non-NULL, points to the constant with the
			   maximum value so far in a switch statement case
			   label. */
} a_struct_stmt_stack_entry;

EXTERN a_struct_stmt_stack_entry_ptr
		struct_stmt_stack;
			/* The currently active structured statement stack
			   itself.  The current entry is [depth_stmt_stack].
			   Entry [0] is for the main block of the current
			   function, if we are currently inside a function.
			   Note that in C++ there can be more than one such
			   stack, though only one is active at a time.  The
			   struct_stmt_stack array is actually a subarray of
			   struct_stmt_stack_container. */

EXTERN int	depth_stmt_stack;
			/* Index of the current entry in struct_stmt_stack.
			   -1 if the stack is empty. */

extern a_statement_ptr add_statement_at_stmt_pos(a_statement_kind   kind,
                                                 a_source_position  *stmt_pos);

extern void update_init_statement_control_flow(a_statement_ptr  sp);

extern void set_vla_size_statement(a_vla_dimension_ptr  vdp,
                                   a_source_position    *pos);

extern a_statement_ptr compound_statement(a_boolean at_function_level,
                                          a_boolean explicit_return_type,
                                          a_boolean is_catch_clause,
                                          a_boolean is_statement_expr);

extern void start_of_function_try_block(void);

extern a_statement_ptr function_try_block(a_boolean  explicit_return_type);

extern void wrapup_control_flow_processing(a_scope_ptr  scope_ptr);

extern void warn_if_code_is_unreachable(an_error_code      error_code,
                                        a_source_position  *err_pos);

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
  a_control_flow_descr_ptr
		goto_fixup_list;
			/* Saved pointer to fixup list for goto statements. */
} a_struct_stmt_stack_state;

extern void new_struct_stmt_stack(a_struct_stmt_stack_state *saved_state);
extern void restore_struct_stmt_stack(a_struct_stmt_stack_state *saved_state);

extern a_boolean inside_statement_expression(void);

extern void statements_one_time_init(void);

extern void statements_trans_unit_init(void);

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
