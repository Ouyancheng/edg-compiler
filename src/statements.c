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

statements.c -- Scanning of statements.

*/


#include "basics.h"
#include "host_envir.h"
#include "statements.h"
#include "decls.h"
#include "lexical.h"
#include "error.h"
#include "expr.h"
#include "exprutil.h"
#include "types.h"
#include "il.h"
#include "cmd_line.h"
#include "folding.h"
#include "trans_lims.h"
#include "symbol_tbl.h"
#include "mem_manage.h"


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
  a_boolean	switch_has_default_clause;
			/* TRUE if the structured statement is a switch and
			   it has a default clause. */
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

static a_struct_stmt_stack_entry_ptr
		struct_stmt_stack = NULL;
			/* The structured statement stack itself.  Current
			   entry is [depth_stmt_stack].  Entry [0] is for
			   the main block of the current function, if
			   we are currently inside a function.  Dynamically
			   allocated; can be expanded if necessary.
			   size_struct_stmt_stack gives the number of elements
			   currently allocated.  Allocation is not per-file. */
static sizeof_t	size_struct_stmt_stack = 0;
			/* Allocated size in elements of struct_stmt_stack.
			   Not per-file. */
#define STRUCT_STMT_STACK_INCREMENTAL_ALLOCATION 30
			/* The number of elements added to struct_stmt_stack
			   each time it is reallocated; also the initial
			   allocation. */
static a_reachability_code
		code_reachable;
			/* Indicates whether or not the current location in
			   the code (following the last statement of the top
			   structured statement on the statement stack) is
			   reachable by flowing into it from the previous
			   statement. */


/*
Declarations needed because of mutual recursion:
*/
static void statement(void);


/*
Macro to check the lint-style "notreached" flag, and suppress a warning
on unreachable code if a "notreached" comment was found.
*/
#define check_lint_notreached_flag()                                  \
{ if (lint_notreached_flag) {                                         \
    code_reachable = rc_unreachable_error_given;                      \
    lint_notreached_flag = FALSE;                                     \
  }  /* if */                                                         \
}  /* check_lint_notreached_flag */


a_statement_ptr add_statement(a_statement_kind kind)
/*
Allocate a statement of the indicated kind, and link it onto the end of
the current statement sequence.
*/
{
  a_statement_ptr               sp;
  a_struct_stmt_stack_entry_ptr sssep;
  a_statement_ptr               ssp;
  a_statement_ptr               *head_ptr;
  a_boolean                     statement_list_allowed;
  a_statement_ptr               extra_block;
  a_statement_ptr               temp_stmt;

  db_enter(4, "add_statement");

  /* Find the header pointer for the statement list for the current
     structured statement. */
#if CHECKING
  if (depth_stmt_stack < 0) {
    internal_error("add_statement: struct_stmt_stack is empty");
  }  /* if */
#endif /* CHECKING */
  sssep = &struct_stmt_stack[depth_stmt_stack];
  /* A block that is the primary statement of a switch should be ignored;
     statements should be added to the switch itself. */
  if (sssep->kind == ssk_compound &&
      depth_stmt_stack != 0 &&
      struct_stmt_stack[depth_stmt_stack-1].kind == ssk_switch) {
    sssep--;
  }  /* if */
  ssp = sssep->statement;
  statement_list_allowed = FALSE;
  switch(ssp->kind) {
    case stmk_if:
      if (sssep->in_else_of_if) {
        head_ptr = &ssp->variant.if_stmt.else_statement;
      } else {
        head_ptr = &ssp->variant.if_stmt.then_statement;
      }  /* if */
      break;
    case stmk_while:
    case stmk_end_test_while:
      head_ptr = &ssp->variant.loop_statement;
      break;
    case stmk_switch:
      if (sssep->curr_switch_clause == NULL) {
        /* There is no current switch clause, so add statements to the
           body_statement of the switch (this is unusual). */
        head_ptr = &ssp->variant.switch_stmt.body_statement;
      } else {
        head_ptr = &sssep->curr_switch_clause->statements;
        statement_list_allowed = TRUE;
      }  /* if */
      break;
    case stmk_block:
      head_ptr = &ssp->variant.block.statements;
      statement_list_allowed = TRUE;
      break;
#if CHECKING
    default:
      internal_error("add_statement: bad kind of stmt in struc. stmt stack");
#endif /* CHECKING */
  }  /* switch */
  if (*head_ptr == NULL) {
    /* If the statement list is empty, make sure the last pointer is NULL.
       This is important for the starts of "else" and case clauses. */
    sssep->last_dep_statement = NULL;
  } else if (sssep->last_dep_statement == NULL) {
    /* Otherwise, if the last pointer is NULL, find the last statement
       in the list and set the pointer to it.  This is needed when
       switching back to a statement list that already has some statements
       in it.  To do that, we clear last_dep_statement and let it get
       re-established here. */
    temp_stmt = *head_ptr;
    while (temp_stmt->next != NULL) temp_stmt = temp_stmt->next;
    sssep->last_dep_statement = temp_stmt;
  }  /* if */

  /* Maintain the code reachable flag.  Labels are always reachable. */
  if (kind == (a_statement_kind)stmk_label) code_reachable = rc_reachable;

  /* Allocate the statement entry. */
  sp = alloc_statement(kind);

  /* Link it onto the end of the statement list for the current level of
     the structured statement stack.  Even unreachable code is kept
     (note in particular that code following a lint notreached comment
     is probably unreachable, but by ANSI C rules it must be put out,
     since it might in fact be reachable -- the comment might be wrong, and
     the comment cannot affect the semantics). */
  if (*head_ptr != NULL && !statement_list_allowed) {
    /* The structured statement already has a statement attached to it,
       and it is not a statement to which a list of statements may
       be attached.  This happens in rare cases like

         if (a) b: c = 1;

       i.e., the dependent statement of the "if" is labeled, and therefore
       two dependent statements are required under the if, which only allows
       one.  It also happens for "continue" labels.  For cases like this,
       we create an additional block to contain the list of statements. 
       We check to see if the block has already been created, so that on
       subsequent calls we do not create additional blocks.  If the block
       is present because the source dependent statement is a block,
       that block likewise is used. */
    if (*head_ptr != sssep->last_dep_statement) {
      /* The current end-of-list pointer is not the first statement on
         the list, which means that a previous run through this code has
         already created the extra block and set last_dep_statement
         pointing to the last statement in that block.  Therefore, the
         addition can be done by just adding after the current end of
         the list. */
    } else {
      /* It's necessary to use an existing or create a new block. */
#if CHECKING
      if ((*head_ptr)->next != NULL) {
        internal_error("add_statement: stmt list not allowed under stmt");
      }  /* if */
#endif /* CHECKING */
      if ((*head_ptr)->kind == (a_statement_kind)stmk_block) {
        /* There is an existing block from a source construct.  Find the 
           end of its statement list, and add there. */
        extra_block = *head_ptr;
        temp_stmt = extra_block->variant.block.statements;
        if (temp_stmt != NULL) {
          while (temp_stmt->next != NULL) temp_stmt = temp_stmt->next;
        }  /* if */
        sssep->last_dep_statement = temp_stmt;
      } else {
        /* Create a new block to allow additional statements. */
        extra_block = alloc_statement((a_statement_kind)stmk_block);
        /* Set the source sequence number to zero to indicate that the
           block did not come from the source. */
        extra_block->seq_number = 0;
        extra_block->variant.block.statements = *head_ptr;
        *head_ptr = extra_block;
      }  /* if */
      head_ptr = &extra_block->variant.block.statements;
    }  /* if */
  } /* if */
  /* Now add the new statement to the end of the list. */
  if (*head_ptr == NULL) {
    /* Add the statement as the first statement on the list. */
    *head_ptr = sp;
  } else {
    sssep->last_dep_statement->next = sp;
  }  /* if */
  sssep->last_dep_statement = sp;

  /* Turn off the code_reachable flag if the current statement is an
     unconditional branch. */
  if (kind == (a_statement_kind)stmk_goto   ||
      kind == (a_statement_kind)stmk_return) {
    code_reachable = rc_unreachable;
  }  /* if */

  db_exit();
  return(sp);
}  /* add_statement */


static void check_for_unreachable_code(void)
/*
Generate a warning if the current location in the code is unreachable.
*/
{
  if (code_reachable == rc_unreachable) {
    warning(ec_code_is_unreachable);
    /* Note that once the warning has been issued, it will be issued no
       more because of the special value rc_unreachable_error_given. */
    code_reachable = rc_unreachable_error_given;
  }  /* if */
}  /* check_for_unreachable_code */


static void check_loop_unreachable_code(void)
/*
Generate a warning if the current location in the code (the top of a loop)
is unreachable.  This generates a different message than the normal
check_for_unreachable_code, because the bodies of loops can be reached
via branch from the bottom.
*/
{
  if (code_reachable == rc_unreachable) {
    warning(ec_loop_not_reachable);
    /* Note that once the warning has been issued, it will be issued no
       more because of the special value rc_unreachable_error_given. */
    code_reachable = rc_unreachable_error_given;
  }  /* if */
}  /* check_loop_unreachable_code */


static a_label_ptr alloc_temp_label(void)
/*
Allocate and return a pointer to a temporary label entry.
*/
{
  a_label_ptr label;

  label = alloc_label();
  add_to_labels_list(label);

  return (label);
}  /* alloc_temp_label */


static void define_label(a_label_ptr label)
/*
Put out the definition for the indicated label.  If label == NULL, do nothing.
*/
{
  a_statement_ptr sp;

  db_enter(4, "define_label");
  if (label != NULL) {
    sp = add_statement((a_statement_kind)stmk_label);
    label->variant.exec_stmt = sp;
    sp->variant.label = label;
  }  /* if */
  db_exit();
}  /* define_label */


static void define_continue_label(void)
/*
Define the "continue" label for the current structured statement,
if it has been used.
*/
{
  define_label(struct_stmt_stack[depth_stmt_stack].continue_label);
}  /* define_continue_label */


static void push_stmt_stack(a_struct_stmt_kind kind,
                            a_statement_ptr    sp)
/*
Push an entry onto the structured statement stack, to record that we
are within a structured statement of the indicated kind.  sp points to
the associated il statement.
*/
{
  register a_struct_stmt_stack_entry_ptr sssep;

  db_enter(4, "push_stmt_stack");
  if (depth_stmt_stack+1 == size_struct_stmt_stack) {
    /* Stack is full; expand it. */
    sizeof_t new_size = size_struct_stmt_stack +
                                      STRUCT_STMT_STACK_INCREMENTAL_ALLOCATION;
    struct_stmt_stack = (a_struct_stmt_stack_entry_ptr)realloc_general(
                       (char *)struct_stmt_stack,
                       (sizeof_t)(size_struct_stmt_stack*
                                            sizeof(a_struct_stmt_stack_entry)),
                       (sizeof_t)(new_size*sizeof(a_struct_stmt_stack_entry)));
    size_struct_stmt_stack = new_size;
  }  /* if */
  /* Push the stack and initialize the new entry. */
  sssep = &struct_stmt_stack[++depth_stmt_stack];
  sssep->kind                 = kind;
  sssep->in_else_of_if        = FALSE;
  sssep->statement            = sp;
  sssep->curr_switch_clause   = NULL;
  sssep->last_dep_statement   = NULL;
  sssep->break_label          = NULL;
  sssep->continue_label       = NULL;
  sssep->switch_selector_type = NULL;
  sssep->switch_has_default_clause
                              = FALSE;
  sssep->start_reachable      = code_reachable;
  sssep->end_reachable        = rc_unreachable;  /* So far. */
  if (kind == ssk_while || kind == ssk_do || kind == ssk_for) {
    /* The bodies of loops are reachable in that the bottom can branch to
       the top. */
    code_reachable = rc_reachable;
  } else if (kind == ssk_switch) {
    /* The body of a switch is not reachable until a case or default label
       appears. */
    code_reachable = rc_unreachable;
  }  /* if */
  db_exit();
}  /* push_stmt_stack */


static void start_stmt_clause(a_struct_stmt_stack_entry_ptr sssep)
/*
Start a new clause of a structured statement.  sssep points to the
struct_stmt_stack entry for the structured statement (since it need not
be the topmost one).
*/
{
  /* The start of a clause is reachable if the start of the structured
     statement is reachable. */
  code_reachable = sssep->start_reachable;
}  /* start_stmt_clause */


static void term_stmt_clause(a_struct_stmt_stack_entry_ptr sssep)
/*
end the current clause of a structured statement.  sssep points to the
struct_stmt_stack entry for the structured statement (for symmetry with
start_stmt_clause).
*/
{
  /* If the end of the clause is reachable, then the end of the whole
     structured statement is reachable. */
  if (code_reachable == rc_reachable) {
    sssep->end_reachable = code_reachable;
  }  /* if */
}  /* term_stmt_clause */


static a_boolean is_infinite_loop(a_statement_ptr stmt)
/*
Return TRUE if the indicated statement is an infinite loop.  The safe answer,
if the truth cannot be discovered, is FALSE.
*/
{
  a_boolean        is_inf_loop = FALSE;
  an_expr_node_ptr expr;

  if (stmt->kind == (a_statement_kind)stmk_while ||
      stmt->kind == (a_statement_kind)stmk_end_test_while) {
    expr = stmt->expr;
    if (expr->kind == (an_expr_node_kind)enk_constant) {
      if (!is_zero_constant(expr->variant.constant)) {
        /* Loop expression is a non-zero constant: it's an infinite loop. */
        is_inf_loop = TRUE;
      }  /* if */
    }  /* if */
  } /* if */
  return(is_inf_loop);
}  /* is_infinite_loop */


static void pop_stmt_stack(void)
/*
Pop the top entry off the structured statement stack, recording that
a structured statement has ended.
*/
{
  register a_struct_stmt_stack_entry_ptr sssep;
  a_struct_stmt_kind                     kind;
  
  db_enter(4, "pop_stmt_stack");
  sssep = &struct_stmt_stack[depth_stmt_stack];
  kind = sssep->kind;
  /* Close the final clause of the statement, if any. */
  if (kind != ssk_switch || sssep->curr_switch_clause != NULL) {
    term_stmt_clause(sssep);
  }  /* if */
  /* The code after the structured statement is reachable if the end
     of any of the clauses is reachable, or if the opening statement
     is reachable and the statement is of a kind that can transfer
     directly to the end of the statement without executing a clause.
     Such statements are: a zero-trip loop, a "switch" without a default
     clause, and an "if" without an "else" clause. */
  if (sssep->end_reachable == rc_reachable ||
      (sssep->start_reachable == rc_reachable &&
       (kind == ssk_while || kind == ssk_for ||
        (kind == ssk_switch && !sssep->switch_has_default_clause) ||
        (kind == ssk_if &&
         sssep->statement->variant.if_stmt.else_statement == NULL)))) {
    /* The code after the structured statement is reachable.  One further
       test: if a loop is an infinite loop, the end is not reachable
       (remember that a "break", if any, is handled separately via
       the break_label). */
    if (is_infinite_loop(sssep->statement)) {
      if (sssep->start_reachable == rc_unreachable_error_given) {
        code_reachable = rc_unreachable_error_given;
      } else {
        code_reachable = rc_unreachable;
      }  /* if */
    } else {
      code_reachable = rc_reachable;
    }  /* if */
  } else {
    /* The code after the structured statement is not reachable.
       If an unreachable code error has been generated either before 
       the statement or in it, remember that fact. */
    if (sssep->end_reachable   == rc_unreachable_error_given ||
        sssep->start_reachable == rc_unreachable_error_given) {
      code_reachable = rc_unreachable_error_given;
    } else {
      code_reachable = rc_unreachable;
    }  /* if */
  }  /* if */
  /* Pop the stack. */
  depth_stmt_stack--;
  /* If the break label for this statement was referenced, generate 
     its definition now.  This must be done after depth_stmt_stack is
     decremented so that the label will appear outside the structured
     statement.  It must also be done after code_reachable has been
     adjusted. */
  define_label(sssep->break_label);
  db_exit();
}  /* pop_stmt_stack */


#if ASM_STATEMENT_ALLOWED
static void asm_statement(void)
/*
Scan an asm statement.  This is a non-ANSI construct.  Its form is

asm ( "string" ) ;

*/
{
  a_statement_ptr sp;
  a_constant      asm_string;

  db_enter(3, "asm_statement");

  check_for_unreachable_code();
  /* Allocate the statement. */
  sp = add_statement((a_statement_kind)stmk_asm);
  /* Ignore the initial "asm". */
#if CHECKING
  if (curr_token != tok_identifier) {
    internal_error("asm_statement: expected asm");
  }  /* if */
#endif  /* CHECKING */
  (void)get_token();
  /* Check for and skip the opening parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_stop_token(tok_rparen);
  /* Scan the enclosed string. */
  if (curr_token != tok_string_literal) {
    syntax_error(ec_exp_asm_string);
    set_error_constant(&asm_string);
  } else {
    copy_constant(&const_for_curr_token, &asm_string);
    (void)get_token();
  }  /* if */
  sp->variant.asm_string = alloc_unshared_constant(&asm_string);
  /* Check for and skip the closing parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_rparen);

  db_exit();
}  /* asm_statement */
#endif /* ASM_STATEMENT_ALLOWED */


static a_struct_stmt_stack_entry_ptr find_enclosing_struct_stmt(
                                                     a_boolean find_switch,
                                                     a_boolean find_loop)
/*
Search the structured statement stack from the current entry outward,
looking for a switch statement (if find_switch is TRUE) or a loop
statement (while, do, or for; if find_loop is TRUE).  Return a pointer
to the first structured statement stack entry found, or NULL if none 
was found.
*/
{
  a_struct_stmt_stack_entry_ptr sssep;
  a_struct_stmt_kind            kind;

  sssep = &struct_stmt_stack[depth_stmt_stack];
  /* Note that the loop never looks at entry [0], since that is for
     the compound statement that defines the function. */
  while (sssep != &struct_stmt_stack[0]) {
    kind = sssep->kind;
    if (find_switch && kind == ssk_switch)  goto found;
    if (find_loop   &&(kind == ssk_while ||
                       kind == ssk_do    ||
                       kind == ssk_for   )) goto found;
    /* Keeping looking at entries in the structured statement stack. */
    sssep--;
  }  /* while */
  /* No structured statement matching the criteria was found. */
  sssep = NULL;
found:
  return(sssep);
}  /* find_enclosing_struct_stmt */


static void if_statement(void)
/*
Scan an "if" statement (with or without else) and add it to the current
statement sequence.  The syntax is:

3.6.4  selection-statement:
		if ( expression ) statement
		if ( expression ) statement else statement

See also 3.6.4.1.
*/
{
  register a_statement_ptr      sp;
  a_boolean                     err;
  a_struct_stmt_stack_entry_ptr sssep;

  db_enter(3, "if_statement");

  check_for_unreachable_code();
  /* Allocate the statement. */
  sp = add_statement((a_statement_kind)stmk_if);
  /* Push an entry on the structured statement stack. */
  push_stmt_stack(ssk_if, sp);
  /* Ignore the initial "if". */
#if CHECKING
  if (curr_token != tok_if) internal_error("if_statement: expected if");
#endif /* CHECKING */
  (void)get_token();
  /* Check for and skip the opening parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_stop_token(tok_rparen);
  /* Scan the controlling expression, and check to see that it is scalar. */
  sp->expr = scan_boolean_controlling_expression(&err);
  /* Check for and skip the closing parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_rparen);
  /* Scan the "then" statement. */
  add_stop_token(tok_else);
  statement();
  remove_stop_token(tok_else);
  /* Scan "else" and another statement if they appear. */
  if (curr_token == tok_else) {
    (void)get_token();
    /* Getting the address of the struct_stmt_stack entry is done late
       because the stack might be reallocated while scanning the contained
       statement. */
    sssep = &struct_stmt_stack[depth_stmt_stack];
    term_stmt_clause(sssep);
    sssep->in_else_of_if = TRUE;
    start_stmt_clause(sssep);
    statement();
  }  /* if */
  /* Pop the structured statement stack. */
  pop_stmt_stack();

  db_exit();
}  /* if_statement */


static void switch_statement(void)
/*
Scan a "switch" statement and add it to the current statement sequence.
The syntax is:

3.6.4  selection-statement:
		switch ( expression ) statement

See also 3.6.4.2.
*/
{
  register a_statement_ptr sp;
  a_boolean                err;

  db_enter(3, "switch_statement");

  check_for_unreachable_code();
  /* Allocate the statement. */
  sp = add_statement((a_statement_kind)stmk_switch);
  /* Push an entry on the structured statement stack. */
  push_stmt_stack(ssk_switch, sp);
  /* Ignore the initial "switch". */
#if CHECKING
  if (curr_token != tok_switch) {
    internal_error("switch_statement: expected switch");
  }  /* if */
#endif /* CHECKING */
  (void)get_token();
  /* Check for and skip the opening parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_stop_token(tok_rparen);
  /* Scan the controlling expression, and check to see that it is integral. */
  sp->expr = scan_expression(&err);
  if (!is_integral_type(sp->expr->type)) {
    /* Error, the expression is not integral. */
    if (!err && !is_error_type(sp->expr->type)) {
      error(ec_expr_not_integral);
    }  /* if */
    sp->expr = error_node();
  } else {
    /* The expression is indeed integral.  Promote it (to int) if necessary. */
    if (C_dialect != C_dialect_pcc) {
      /* ANSI: the normal integral promotions are done. */
      integral_promote_node(&sp->expr);
    } else {
      /* pcc treats all switch expressions as int.  This differs from
         ANSI in that even long is cast to int. */
      cast_node(&sp->expr, integer_type((an_integer_kind)ik_int),
                /*issue_type_chg_warning=*/TRUE);
    }  /* if */
    /* Issue a remark if the selector is constant. */
    if (is_constant_node(sp->expr)) {
      remark(ec_switch_selector_expr_is_constant);
    }  /* if */
  }  /* if */
  /* Save the selector expression type for checking of the case label
     values. */
  struct_stmt_stack[depth_stmt_stack].switch_selector_type = sp->expr->type;
  /* Check for and skip the closing parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_rparen);
  /* Scan the dependent statement. */
  statement();
  /* Pop the structured statement stack. */
  pop_stmt_stack();

  db_exit();
}  /* switch_statement */


static void while_statement(void)
/*
Scan a "while" statement and add it to the current statement sequence.
The syntax is:

3.6.5  iteration-statement:
		while ( expression ) statement

See also 3.6.5.1.
*/
{
  register a_statement_ptr sp;
  a_boolean                err;

  db_enter(3, "while_statement");

  check_loop_unreachable_code();
  /* Allocate the statement. */
  sp = add_statement((a_statement_kind)stmk_while);
  /* Push an entry on the structured statement stack. */
  push_stmt_stack(ssk_while, sp);
  /* Ignore the initial "while". */
#if CHECKING
  if (curr_token != tok_while) {
    internal_error("while_statement: expected while");
  }  /* if */
#endif /* CHECKING */
  (void)get_token();
  /* Check for and skip the opening parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_stop_token(tok_rparen);
  /* Scan the controlling expression, and check to see that it is scalar. */
  sp->expr = scan_boolean_controlling_expression(&err);
  /* Check for and skip the closing parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_rparen);
  /* Scan the dependent statement. */
  statement();
  /* Define the "continue" label, if it is needed. */
  define_continue_label();
  /* Pop the structured statement stack. */
  pop_stmt_stack();

  db_exit();
}  /* while_statement */


static void do_statement(void)
/*
Scan a "do" statement and add it to the current statement sequence.
The syntax is:

3.6.5  iteration-statement:
		do ( expression ) statement

See also 3.6.5.2.
*/
{
  register a_statement_ptr sp;
  a_boolean                err;

  db_enter(3, "do_statement");

  check_loop_unreachable_code();
  /* Allocate the statement. */
  sp = add_statement((a_statement_kind)stmk_end_test_while);
  /* Push an entry on the structured statement stack. */
  push_stmt_stack(ssk_do, sp);
  /* Ignore the initial "do". */
#if CHECKING
  if (curr_token != tok_do) internal_error("do_statement: expected do");
#endif /* CHECKING */
  (void)get_token();
  /* Scan the dependent statement. */
  add_stop_token(tok_while);
  statement();
  /* Define the "continue" label, if it is needed. */
  define_continue_label();
  /* Check for and skip the keyword "while". */
  (void)required_token(tok_while, ec_exp_while);
  remove_stop_token(tok_while);
  add_stop_token(tok_semicolon);
  /* Check for and skip the opening parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_stop_token(tok_rparen);
  /* Scan the controlling expression, and check to see that it is scalar. */
  sp->expr = scan_boolean_controlling_expression(&err);
  /* Check for and skip the closing parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_rparen);
  /* Check for and skip the semicolon. */
  (void)required_token(tok_semicolon, ec_exp_semicolon);
  remove_stop_token(tok_semicolon);
  /* Pop the structured statement stack. */
  pop_stmt_stack();

  db_exit();
}  /* do_statement */


static void for_statement(void)
/*
Scan a "for" statement and add it to the current statement sequence.
The syntax is:

3.6.5  iteration-statement:
		for ( expression    ; expression    ; expression    ) statement
                                opt             opt             opt

See also 3.6.5.3.
*/
{
  register a_statement_ptr sp;
  a_boolean                err;
  a_statement_ptr          temp_stmt;
  a_constant               constant;
  an_expr_node_ptr         incr_expr, init_expr;
  a_seq_number             start_seq_number, temp_seq_number;

  db_enter(3, "for_statement");

  check_loop_unreachable_code();
  /* Overall, the "for" statement

       for (expr1; expr2; expr3) statement

     is translated as

       expr1;
       while (expr2) {
         statement
         expr3;
       }

     If expr2 is omitted, "1" is used instead. */

  /* Save the source sequence number of the "for" for use later. */
  start_seq_number = pos_curr_token.seq;
  /* Ignore the initial "for". */
#if CHECKING
  if (curr_token != tok_for) internal_error("for_statement: expected for");
#endif /* CHECKING */
  (void)get_token();

  /* Check for and skip the opening parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_stop_token(tok_rparen);
  add_stop_token(tok_semicolon);

  /* Scan the initializing expression if it is present. */
  if (curr_token != tok_semicolon) {
    temp_seq_number = pos_curr_token.seq;
    init_expr = scan_void_expression(&err);
    /* Add the expression if is is not void. */
    if (init_expr != NULL) {
      temp_stmt = add_statement((a_statement_kind)stmk_expr);
      temp_stmt->seq_number = temp_seq_number;
      temp_stmt->expr = init_expr;
    }  /* if */
  }  /* if */
  (void)required_token(tok_semicolon, ec_exp_semicolon);

  /* Allocate the statement.  This is done late so that the initializing
     expression can be evaluated outside the loop. */
  sp = add_statement((a_statement_kind)stmk_while);
  sp->seq_number = start_seq_number;
  /* Push an entry on the structured statement stack. */
  push_stmt_stack(ssk_for, sp);

  /* Scan the controlling expression if it is present, and check to see
     that it is scalar. */
  if (curr_token != tok_semicolon) {
    sp->expr = scan_boolean_controlling_expression(&err);
  } else {
    /* Use a constant "1" for an omitted expression. */
    set_integer_constant(&constant, 1L);
    sp->expr = alloc_node_for_constant(&constant);
  }  /* if */
  (void)required_token(tok_semicolon, ec_exp_semicolon);
  remove_stop_token(tok_semicolon);

  /* Scan the incrementing expression if it is present.  Save the expression
     for later use as a statement within the loop. */
  if (curr_token != tok_rparen) {
    temp_seq_number = pos_curr_token.seq;
    incr_expr = scan_void_expression(&err);
  } else {
    /* Incrementing expression is omitted. */
    incr_expr = NULL;
  }  /* if */

  /* Check for and skip the closing parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_rparen);

  /* Scan the dependent statement. */
  statement();

  /* Define the "continue" label, if it is needed. */
  define_continue_label();

  /* If there was an incrementing expression, add it to the end of the
     loop. */
  if (incr_expr != NULL) {
    temp_stmt = add_statement((a_statement_kind)stmk_expr);
    temp_stmt->seq_number = temp_seq_number;
    temp_stmt->expr = incr_expr;
  }  /* if */

  /* Pop the structured statement stack. */
  pop_stmt_stack();

  db_exit();
}  /* for_statement */


static void goto_statement(void)
/*
Scan a "goto" statement and add it to the current statement sequence.
The syntax is:

3.6.6  jump-statement:
		goto identifier ;

See also 3.6.6.1.
*/
{
  register a_statement_ptr sp;

  db_enter(3, "goto_statement");
  check_for_unreachable_code();
  /* Allocate the statement. */
  sp = add_statement((a_statement_kind)stmk_goto);
  /* Ignore the initial "goto". */
#if CHECKING
  if (curr_token != tok_goto) internal_error("goto_statement: expected goto");
#endif /* CHECKING */
  (void)get_token();
  add_stop_token(tok_semicolon);
  /* Scan the label identifier. */
  sp->variant.label = scan_label(/*is_definition=*/FALSE);
  /* Check for and ignore the final semicolon. */
  (void)required_token(tok_semicolon, ec_exp_semicolon);
  remove_stop_token(tok_semicolon);
  db_exit();
}  /* goto_statement */


static void continue_statement(void)
/*
Scan a "continue" statement and add it to the current statement sequence.
The syntax is:

3.6.6  jump-statement:
		continue ;

See also 3.6.6.2.
*/
{
  register a_statement_ptr      sp;
  a_struct_stmt_stack_entry_ptr sssep;
  a_label_ptr                   dest_label = NULL;

  db_enter(3, "continue_statement");
  check_for_unreachable_code();
  /* See if we are within a loop body (while, do, or for) by looking at the
     entries in the structured statement stack. */
  sssep = find_enclosing_struct_stmt(/*find_switch=*/FALSE,
                                     /*find_loop=*/TRUE);
  if (sssep == NULL) {
    /* No appropriate structured statement was found. */
    error(ec_continue_must_be_in_loop);
    dest_label = alloc_temp_label();
  } else {
    /* Found the loop that this continue statement should exit. */
    dest_label = sssep->continue_label;
    if (dest_label == NULL) {
      /* The continue label has not previously been used, so generate it. */
      dest_label = sssep->continue_label = alloc_temp_label();
    }  /* if */
    /* Allocate the goto statement. */
    sp = add_statement((a_statement_kind)stmk_goto);
    /* Put the destination label into the goto. */
    sp->variant.label = dest_label;
  }  /* if */
  /* Ignore the initial "continue". */
#if CHECKING
  if (curr_token != tok_continue) {
    internal_error("continue_statement: expected continue");
  }  /* if */
#endif /* CHECKING */
  (void)get_token();
  /* Check for and ignore the final semicolon. */
  (void)required_token(tok_semicolon, ec_exp_semicolon);
  db_exit();
}  /* continue_statement */


static void break_statement(void)
/*
Scan a "break" statement and add it to the current statement sequence.
The syntax is:

3.6.6  jump-statement:
		break ;

See also 3.6.6.3.
*/
{
  register a_statement_ptr      sp;
  a_struct_stmt_stack_entry_ptr sssep;
  a_label_ptr                   dest_label = NULL;

  db_enter(3, "break_statement");
  check_for_unreachable_code();
  /* See if we are within a loop body (while, do, or for) or a switch
     by looking at the entries in the structured statement stack. */
  sssep = find_enclosing_struct_stmt(/*find_switch=*/TRUE,
                                     /*find_loop=*/TRUE);
  if (sssep == NULL) {
    /* No appropriate structured statement was found. */
    error(ec_break_must_be_in_loop_or_switch);
    dest_label = alloc_temp_label();
  } else if (sssep->kind == ssk_switch &&
             sssep->curr_switch_clause != NULL &&
             depth_stmt_stack != 0 &&
             &struct_stmt_stack[depth_stmt_stack-1] == sssep) {
    /* This break statement exits a switch clause.  No goto need be generated,
       because the default action at the end of the clause in the IL
       is a "break".  However, the current switch clause must be ended.
       Note that this special trick can be done only when the break is 
       at the top level in the case clause. */
    sssep->curr_switch_clause = NULL;
    sssep->last_dep_statement = NULL;
    term_stmt_clause(sssep);
    code_reachable = rc_unreachable;
  } else {
    /* This break statement exits a loop, or some part of a switch that
       is not inside a switch clause. */
    dest_label = sssep->break_label;
    if (dest_label == NULL) {
      /* The break label has not previously been used, so generate it. */
      dest_label = sssep->break_label = alloc_temp_label();
    }  /* if */
    /* Allocate the goto statement. */
    sp = add_statement((a_statement_kind)stmk_goto);
    /* Put the destination label into the goto. */
    sp->variant.label = dest_label;
  }  /* if */
  /* Ignore the initial "break". */
#if CHECKING
  if (curr_token != tok_break) {
    internal_error("break_statement: expected break");
  }  /* if */
#endif /* CHECKING */
  (void)get_token();
  /* Check for and ignore the final semicolon. */
  (void)required_token(tok_semicolon, ec_exp_semicolon);
  db_exit();
}  /* break_statement */


static void check_void_return_okay(void)
/*
Check that a void return (one with no value) is okay as a way of exiting
the current routine.
*/
{
  a_routine_ptr            rout;
  a_type_ptr      rout_type;

  /* Get a pointer to the current routine entry, and get its return
     type. */
  rout = current_routine_entry();
  rout_type = skip_typerefs(rout->type)->variant.routine.return_type;
  if (!is_void_type(rout_type) && !is_error_type(rout_type)) {
    /* If a return with no expression appears in a function with a
       non-void type, issue a warning.  Do not issue the warning for
       the main program, or if the declaration of the function did not
       have an explicit type specifier (omitting the specifier implies
       "int", but may have been intended to mean "void" in old-style C). */
    if (!curr_rout_type_explicitly_specified) {
      /* No warning if the routine's type was not explicitly specified. */
    } else if (rout == il_header.main_routine) {
      /* No warning for "main". */
    }  else {
      warning(ec_no_value_returned_in_non_void_function);
    }  /* if */
  }  /* if */
}  /* check_void_return_okay */


static void return_statement(void)
/*
Scan a "return" statement and add it to the current statement sequence.
The syntax is:

3.6.6  jump-statement:
		return expression    ;
                                 opt

See also 3.6.6.4.
*/
{
  register a_statement_ptr sp;
  a_routine_ptr            rout;
  a_type_ptr               rout_type;
  a_boolean                err;

  db_enter(3, "return_statement");
  check_for_unreachable_code();
  /* Allocate the statement. */
  sp = add_statement((a_statement_kind)stmk_return);
  /* Ignore the initial "return". */
#if CHECKING
  if (curr_token != tok_return) {
    internal_error("return_statement: expected return");
  }  /* if */
#endif /* CHECKING */
  (void)get_token();
  add_stop_token(tok_semicolon);
  /* See if the optional expression is present. */
  if (curr_token == tok_semicolon) {
    /* The expression is missing. */
    check_void_return_okay();
  } else {
    /* The expression is present. */
    sp->expr = scan_expression(&err);
    /* Get a pointer to the current routine entry, and get its return
       type. */
    rout = current_routine_entry();
    rout_type = skip_typerefs(rout->type)->variant.routine.return_type;
    if (is_void_type(rout_type)) {
      /* A void function may not return a value. */
      error(ec_value_returned_in_void_function);
      sp->expr = NULL;
    } else if (is_no_type(rout_type)) {
      /* Only constructors and destructors have a return type of tk_none.
         Like void functions, they may not return a value (ARM 6.6.3). */
      error(ec_value_returned_in_constructor);
      sp->expr = NULL;
    } else {
      /* Cast the expression to the return type, if necessary, with semantics
         the same as for assignment. */
      node_prepare_assignment(&sp->expr, rout_type, ec_bad_return_value_type,
                              &err);
    }  /* if */
  }  /* if */
  /* Check for and ignore the final semicolon. */
  (void)required_token(tok_semicolon, ec_exp_semicolon);
  remove_stop_token(tok_semicolon);
  db_exit();
}  /* return_statement */


static void add_switch_clause(a_struct_stmt_stack_entry_ptr sssep,
                              a_constant_ptr                constant_ptr)
/*
Begin a clause of the switch statement associated with the structured
statement stack entry pointed to by sssep, for the case value indicated
by *constant_ptr.  constant_ptr is NULL to indicate the default label.
*/
{
  a_switch_clause_ptr scp, prev_scp;
  a_constant_ptr      cp, prev_cp;
  a_boolean           can_add_to_curr_clause;
  a_boolean           label_directly_in_switch;
  a_statement_ptr     clause_stmts;
  a_label_ptr         label;
  a_statement_ptr     goto_stmt;

  db_enter(4, "add_switch_clause");

  /* Check to see if the constant (or default) already appears somewhere
     in the switch clauses.  Also remember where the last entry is for later
     addition of a new entry at the end of the list. */
  for (prev_scp = NULL,
         scp = sssep->statement->variant.switch_stmt.clause_list;
       scp != NULL;
       prev_scp = scp, scp = scp->next) {
    cp = scp->constant_list;
    if (constant_ptr == NULL) {
      if (cp == NULL) {
        /* "default" appears more than once. */
        error(ec_default_label_appears_more_than_once);
        goto routine_exit;
      }  /* if */
    } else {
      /* Check the list of constants in this clause to see if the new
         constant appears thereon. */
      if (constant_ptr->kind == (a_constant_repr_kind)ck_integer) {
        for (; cp != NULL; cp = cp->next) {
          if (cp->kind == (a_constant_repr_kind)ck_integer &&
              cp->variant.integer_value ==
                constant_ptr->variant.integer_value) {
            error(ec_case_label_appears_more_than_once);
            goto routine_exit;
          }  /* if */
        }  /* for */
      } /* if */
    }  /* if */
  }  /* for */
  /* There is a strange case in switches, where case labels appear within
     a structured statement nested within the switch, rather than directly
     within the switch itself, as in

     n = count / 8;
     switch (count % 8) {
       do { 
                 *a++ = *b++;
         case 7: *a++ = *b++;
         case 6: *a++ = *b++;
         case 5: *a++ = *b++;
         case 4: *a++ = *b++;
         case 3: *a++ = *b++;
         case 2: *a++ = *b++;
         case 1: *a++ = *b++;
         case 0: ;
       } while (--n >= 0);
     }

     (This is known as "Duff's device", after Tom Duff.)  For this case,
     the il switch clauses contain gotos to the proper labels within the
     inner loop, rather than containing the code itself directly.  Note
     that it is normal for a compound statement to be the body of the
     switch, and we take care not to consider that case to be unusual.
     See add_statement for special code in adding code to a switch
     statement. */
  label_directly_in_switch =
              &struct_stmt_stack[depth_stmt_stack] == sssep ||
              (struct_stmt_stack[depth_stmt_stack].kind == ssk_compound &&
               depth_stmt_stack != 0 &&
               &struct_stmt_stack[depth_stmt_stack-1] == sssep);
              
  /* The value does not appear already, and therefore it is okay to proceed
     and add it.  First, we try to see if the new value can just be added
     to the existing current clause for this switch, as when case labels
     appear next to one another:

       case 1:
       case 2:
       default:

     The current clause can be used if no code has yet been added to it.
     However, this really means "no meaningful code": labels are ignored,
     and for the "Duff's device" case above, the goto into the inner
     statement is ignored. */
  can_add_to_curr_clause = FALSE;
  if ((scp = sssep->curr_switch_clause) != NULL) {
    /* There is a current switch clause, so perhaps it can be reused. */
     clause_stmts = scp->statements;
    /* If this is a "Duff's device" case, follow the goto. */
    if (!label_directly_in_switch &&
        clause_stmts != NULL &&
        clause_stmts->kind == (a_statement_kind)stmk_goto) {
      clause_stmts = clause_stmts->variant.label->variant.exec_stmt;
    }  /* if */
    /* Ignore any number of labels at this point. */
    while (clause_stmts != NULL &&
           clause_stmts->kind == (a_statement_kind)stmk_label) {
        clause_stmts = clause_stmts->next;
    }  /* while */
    /* If there is no code at the end of this list, then the clause can
       be reused. */
    can_add_to_curr_clause = (clause_stmts == NULL);
  }  /* if */
  if (!can_add_to_curr_clause) {
    /* The new value cannot be added to the current switch clause; a new
       clause must be created, and the new value added to it. */
    scp = alloc_switch_clause();
    if (prev_scp == NULL) {
      sssep->statement->variant.switch_stmt.clause_list = scp;
    } else {
      prev_scp->next = scp;
    }  /* if */
  }  /* if */
  /* Add the new value to the (new?) current switch clause.  For the
     default case, this just means setting the constant_list to NULL;
     for valued cases, it means inserting the value at the right spot
     on the list. */
  if (constant_ptr == NULL ||
      can_add_to_curr_clause && scp->constant_list == NULL) {
    /* The default case is indicated by a NULL pointer.  Note that if
       a clause includes the default case, specifying any other constants
       along with "default" is redundant.  Therefore, we just clear the
       pointer.  This applies whether the new value is "default" or
       the existing clause is "default"; either way, the result is
       simply "default". */
    scp->constant_list = NULL;  /* Indicating default clause. */
  } else {
    /* Add a case value at the right spot on the list of constants. */
    prev_cp = NULL;
    for (cp = scp->constant_list;
         cp != NULL &&
           cp->variant.integer_value < constant_ptr->variant.integer_value;
         prev_cp = cp, cp = cp->next) {};
    if (prev_cp == NULL) {
      scp->constant_list = constant_ptr;
    } else {
      prev_cp->next = constant_ptr;
    }  /* if */
    constant_ptr->next = cp;
  }  /* if */
  if (can_add_to_curr_clause) {
    /* For the case where the value could be added to the current clause, we
       have nothing further to do. */
  } else {
    /* Start a new clause. */
    label = NULL;
    if (label_directly_in_switch) {
      /* Normal case: the clause statements will be attached to the
         switch clause directly.  If there was a previous switch clause that
         flows into this one, generate a goto from there. */
      if (code_reachable == rc_reachable) {
        label = alloc_temp_label();
        goto_stmt = add_statement((a_statement_kind)stmk_goto);
        goto_stmt->variant.label = label;
      }  /* if */
    } else {
      /* When the destination is inside a structured statement nested within
         the switch, we create a goto that is the switch clause and
         transfers control to the proper point in the nested statement. */
      label = alloc_temp_label();
      goto_stmt = alloc_statement((a_statement_kind)stmk_goto);
      goto_stmt->variant.label = label;
      scp->statements = goto_stmt;
    }  /* if */
    /* Note that it is not appropriate to terminate the previous
       switch clause, if any, by calling term_stmt_clause.  Only a
       break really terminates a switch clause; other cases are
       flow-ins. */
    /* Activate the new switch clause so code will be added here (if the
       switch is the outermost strutured statement). */
    sssep->curr_switch_clause = scp;
    /* Define the label for one of the gotos above, if necessary. */
    define_label(label);
    /* Start a new clause. */
    start_stmt_clause(sssep);
    /* If there is flow-in from the previous clause, the code here is
       reachable. */
    if (label != NULL) code_reachable = rc_reachable;
  }  /* if */
routine_exit:
  db_exit();
}  /* add_switch_clause */


static void case_label(void)
/*
Scan a case label definition.  The syntax is:

3.6.1  labeled_statement
		case constant-expression : statement

*/
{
  a_struct_stmt_stack_entry_ptr sssep;
  a_boolean                     err;
  a_constant                    constant;
  a_constant_ptr                constant_ptr = NULL;
  an_error_code                 err_code;
  an_error_severity             err_severity;

  db_enter(4, "case_label");

  add_stop_token(tok_colon);
  /* See if we are within a switch body by looking at the entries in
     the structured statement stack. */
  sssep = find_enclosing_struct_stmt(/*find_switch=*/TRUE,
                                     /*find_loop=*/FALSE);
  if (sssep == NULL) {
    /* We are not inside a switch statement. */
    error(ec_case_label_must_be_in_switch);
  }  /* if */
  /* Ignore the initial "case". */
#if CHECKING
  if (curr_token != tok_case) internal_error("case_label: expected case");
#endif /* CHECKING */
  (void)get_token();
  constant_ptr = NULL;
  /* Scan the constant expression. */
  scan_integral_constant_expression(&constant, &err);
  if (err || constant.kind == (a_constant_repr_kind)ck_error) {
    /* Error; constant_ptr is left NULL. */
  } else {
#if CHECKING
    if (constant.kind != (a_constant_repr_kind)ck_integer) {
      internal_error("case_label: case value not int");
    }  /* if */
#endif /* CHECKING */
    /* Change the constant to the type of the selector expression.  This
       can cause an error if the selector type is "int" and the case
       label value is in the "long" range. */
    if (sssep != NULL) {
      type_change_constant(&constant, sssep->switch_selector_type,
                           /*issue_type_chg_warning=*/TRUE,
                           &err_code, &err_severity);
      if (err_code != ec_no_error) {
        if (err_severity == es_warning) {
          warning(err_code);
        } else {
          error(err_code);
          err = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
    if (!err) {
      /* Allocate a copy of the case constant. */
      constant_ptr = alloc_unshared_constant(&constant);
    }  /* if */
  }  /* if */
  if (sssep != NULL) {
    if (constant_ptr != NULL) {
      /* Add the proper switch clause. */
      add_switch_clause(sssep, constant_ptr);
    } else {
      /* Make code reachable for the error case. */
      start_stmt_clause(sssep);
    }  /* if */
  } else {
    /* Make code reachable for the error case. */
    code_reachable = rc_reachable;
  }  /* if */
  /* Check for and ignore the final colon. */
  (void)required_token(tok_colon, ec_exp_colon);
  remove_stop_token(tok_colon);
  db_exit();
}  /* case_label */


static void default_label(void)
/*
Scan a default case label definition.  The syntax is:

3.6.1  labeled_statement
		default : statement

*/
{
  a_struct_stmt_stack_entry_ptr sssep;

  db_enter(4, "default_label");

  /* See if we are within a switch body by looking at the entries in
     the structured statement stack. */
  sssep = find_enclosing_struct_stmt(/*find_switch=*/TRUE,
                                     /*find_loop=*/FALSE);
  if (sssep != NULL) {
    /* Found the proper enclosing switch statement. */
    sssep->switch_has_default_clause = TRUE;
    add_switch_clause(sssep, (a_constant_ptr)NULL);
  }  else {
    /* We are not inside a switch statement. */
    error(ec_default_label_must_be_in_switch);
    code_reachable = rc_reachable;
  }  /* if */
  /* Ignore the initial "default". */
#if CHECKING
  if (curr_token != tok_default) {
    internal_error("default_label: expected default");
  }  /* if */
#endif /* CHECKING */
  (void)get_token();
  /* Check for and ignore the final colon. */
  (void)required_token(tok_colon, ec_exp_colon);
  db_exit();
}  /* default_label */


static void statement(void)
/*
Scan a statement.  Add it to the current statement sequence.
*/
{
  a_statement_ptr  sp;
  a_boolean        err;
  a_label_ptr      label;
  a_seq_number     temp_seq_number;
  an_expr_node_ptr temp_expr;
  a_boolean        prev_was_label = FALSE;

  db_enter(3, "statement");

rescan_statement:
  /* If a lint-style "notreached" comment was detected, suppress the
     warning on unreachable code. */
  check_lint_notreached_flag();
  switch(curr_token) {
    case tok_semicolon:
      /* Empty statement (part of expression-statement, 3.6.3). */
      (void)get_token();
      break;
    case tok_lbrace:
      /* Compound statement (3.6.2). */
      (void)compound_statement(/*at_function_level=*/FALSE);
      break;
    case tok_if:
      /* If statement (3.6.4). */
      if_statement();
      break;
    case tok_switch:
      /* Switch statement (3.6.4). */
      switch_statement();
      break;
    case tok_while:
      /* While statement (3.6.5). */
      while_statement();
      break;
    case tok_do:
      /* do .. while statement (3.6.5). */
      do_statement();
      break;
    case tok_for:
      /* For statement (3.6.5). */
      for_statement();
      break;
    case tok_goto:
      /* Goto statement (3.6.6). */
      goto_statement();
      break;
    case tok_continue:
      /* Continue statement (3.6.6). */
      continue_statement();
      break;
    case tok_break:
      /* Break statement (3.6.6). */
      break_statement();
      break;
    case tok_return:
      /* Return statement (3.6.6). */
      return_statement();
      break;
    case tok_case:
      /* Case label (3.6.1). */
      case_label();
      prev_was_label = TRUE;
      goto rescan_statement;
    case tok_default:
      /* Default label (3.6.1). */
      default_label();
      prev_was_label = TRUE;
      goto rescan_statement;
    case tok_identifier:
      /* Identifier.  Probably the start of an expression-statement,
         but first we must check to see if it is a label definition
         by looking to see if the next token is a colon. */
      if (next_token() == tok_colon) {
        /* This is a label definition. */
        /* Scan the label identifier, and enter it into the symbol table
           if needed. */
        label = scan_label(/*is_definition=*/TRUE);
        /* See if the label has already been declared. */
        if (label->variant.exec_stmt != NULL) {
          str_error(ec_label_already_defined, label->source_corresp.name);
          code_reachable = rc_reachable;
        } else {
          /* The label has not previously been declared, so put out the
             definition. */
          define_label(label);
        }  /* if */
#if CHECKING
        if (curr_token != tok_colon) {
          internal_error("statement: expected colon");
        }  /* if */
#endif /* CHECKING */
        (void)get_token();
        prev_was_label = TRUE;
        goto rescan_statement;
      }  /* if */
#if ASM_STATEMENT_ALLOWED
      if (!strict_ansi_mode) {
        char         *id_name;
        a_symbol_ptr sym_ptr;

        /* Check for "asm" statement.  "asm" is not a keyword, so we check for
           an identifier "asm" that is not defined in any way that's meaningful
           here. */
        id_name = locator_for_curr_id.symbol_header->identifier;
        if (*id_name == 'a' && strcmp(id_name, "asm") == 0) {
          /* Identifier is "asm" -- check for definition. */
          sym_ptr = normal_id_lookup(&locator_for_curr_id, IDL_NO_OPTIONS);
          if (sym_ptr == NULL) {
            /* The identifier is not defined as a variable, function,
               constant, or type.  Therefore, this is considered to be an
               asm statement. */
            asm_statement();
            break;  /* out of switch */
          }  /* if */
        }  /* if */
      }  /* if */
#endif /* ASM_STATEMENT_ALLOWED */
      /* Other cases are expression statements. */
      goto expr_statement;
    default:
expr_statement:
      /* Look for things that can't be expression statements, and produce
         a specific "Expected a statement" message for those cases. */
      if (curr_token == tok_rbrace || curr_token == tok_else) {
        /* When a label definition precedes a "}", let it by as an extension,
           with a warning in all modes. */
        if (prev_was_label && curr_token == tok_rbrace) {
          warning(ec_exp_statement);
        } else {
          add_stop_token(tok_semicolon);
          syntax_error(ec_exp_statement);
          remove_stop_token(tok_semicolon);
        }  /* if */
        break;
      }  /* if */
      /* expression-statement (3.6.3). */
      add_stop_token(tok_semicolon);
      check_for_unreachable_code();
      temp_seq_number = pos_curr_token.seq;
      temp_expr = scan_void_expression(&err);
      if (temp_expr != NULL) {
        sp = add_statement((a_statement_kind)stmk_expr);
        sp->seq_number = temp_seq_number;
        sp->expr = temp_expr;
      }  /* if */
      (void)required_token(tok_semicolon, ec_exp_semicolon);
      remove_stop_token(tok_semicolon);
      break;
  }  /* switch */

  db_exit();
}  /* statement */


a_statement_ptr compound_statement(a_boolean at_function_level)
/*
Scan a compound-statement.  The syntax is

3.6.2  compound-statement
		{ declaration-list    statement-list   }
                                  opt               opt
3.6.2  statement-list
		statement
		statement-list statement

Return a pointer to the statement created.

at_function_level is TRUE if this compound-statement is the body of a
function (rather than an enclosed block).  In that case, the closing "}"
is not swallowed by this routine.  This is unusual, but desirable in
that it gets any error messages (like those for unresolved labels) to 
come out on the closing "}".
*/
{
  a_statement_ptr block;
  a_scope_ptr     scope_ptr;
  a_boolean       any_statements = FALSE;
  unsigned char   old_else_stop_token_value;

  db_enter (3, "compound_statement");

  /* Allocate the statement block. */
  if (at_function_level) {
    /* Block for a function. */
    code_reachable = rc_reachable;
    lint_notreached_flag = FALSE;
    block = alloc_statement((a_statement_kind)stmk_block);
    /* Clear statement stack just to be careful. */
    depth_stmt_stack = -1;
  } else {
    /* Block nested within a function.  Link it onto the current statement
       sequence.  Check for unreachable code. */
    if (struct_stmt_stack[depth_stmt_stack].kind == ssk_switch) {
      /* The body statement of a switch is not considered dead code. */
    } else {
      check_for_unreachable_code();
    }  /* if */
    block = add_statement((a_statement_kind)stmk_block);
    /* Clear the entry for "else" in the stop stokens set.  Without this,
       an else encountered where a statement is expected could cause an
       error recovery loop. */
    old_else_stop_token_value = stop_token_array[(int)tok_else];
    stop_token_array[(int)tok_else] = 0;
  }  /* if */
  /* Push an entry on the structured statement stack. */
  push_stmt_stack(ssk_compound, block);
  /* Skip over the opening brace.  Note that this is NOT an internal error
     check; when a compound statement is the body of a function, it's
     required. */
  (void)required_token(tok_lbrace, ec_exp_lbrace);
  add_stop_token(tok_rbrace);
  /* Push an associated scope if this block is not for a function.  This
     does not allocate the IL scope yet. */
  if (!at_function_level) {
    (void)push_scope((a_scope_kind)sck_block, NO_SCOPE_NUMBER,
                     (a_type_ptr)NULL, (a_routine_ptr)NULL);
  }  /* if */

  while (curr_token != tok_rbrace && curr_token != tok_end_of_source) {
    if (is_decl_start()) {
      /* Scan any declarations.  In C, these must all be at the beginning
         of the block.  In C++, they may appear anywhere in the block. */
      if (C_dialect != C_dialect_cplusplus && any_statements) {
        error(ec_declaration_after_statements);
        /* Special error-recovery trick: this tries to deal with mismatched
           braces, in the case where a "}" is missing and thus there appears to
           be an extra "{".  If we are at function level, and the next thing
           appears to be a declaration rather than a statement, and it's not
           indented, assume a "}" and exit the compound statement. */
        if (at_function_level && pos_curr_token.column == 1) break;
      }  /* if */
      local_declaration();
    } else {
      /* Scan a statement. */
      any_statements = TRUE;
      statement();
    }  /* if */
  }  /* while */
  /* If a lint-style "notreached" comment was detected, suppress the
     warning on unreachable code. */
  check_lint_notreached_flag();

  if (at_function_level) {
    /* If the code at the end of a function runs off the end, a default
       return must be added.  See 3.6.6.4. */
    /* Unless we're already in dead code, or there is a lint-style
       "notreached" comment, check that a void return (one returning
       no value) is compatible with the current function (i.e., the
       current function should also have type void). */
    if (code_reachable == rc_reachable) check_void_return_okay();
    /* Add a return with no expression.  Do not add it if the current
       code is truly unreachable. */
    if (code_reachable != rc_unreachable) {
      (void)add_statement((a_statement_kind)stmk_return);
    }  /* if */
  }  /* if */

  /* Pop a name scope if one was pushed earlier in this routine (for
     a block). */
  if (!at_function_level) {
    /* Store the IL scope pointer in the block.  This is NULL except for
       blocks with declarations. */
    scope_ptr = scope_stack[decl_scope_level].il_scope;
    if (scope_ptr != NULL) {
      block->variant.block.extra_info->assoc_scope = scope_ptr;
      scope_ptr->assoc_block = block;
    }  /* if */
    pop_scope();
    /* Restore the entry for "else" in the stop tokens set (see comment
       above). */
    stop_token_array[(int)tok_else] = old_else_stop_token_value;
  }  /* if */
  /* Pop the statement stack. */
  pop_stmt_stack();
  if (at_function_level) {
    /* Clear statement stack just to be careful. */
    depth_stmt_stack = -1;
  }  /* if */

  /* Remember the sequence number of the current token, which is expected
     to be the closing brace. */
  block->variant.block.extra_info->final_seq_number = pos_curr_token.seq;
  /* Check for the closing "}".  Note that for a function, the "}" is left
     for the caller (function_definition) to handle. */
  if (!at_function_level) (void)required_token(tok_rbrace, ec_exp_rbrace);
  remove_stop_token(tok_rbrace);

  db_exit();
  return(block);
}  /* compound_statement */


a_boolean curr_code_reachable(void)
/*
Return TRUE if the current code is reachable.
*/
{
#if CHECKING
  if (depth_stmt_stack < 0) {
    internal_error("curr_code_reachable: struct_stmt_stack is empty");
  }  /* if */
#endif /* CHECKING */
  return(code_reachable == rc_reachable);
}  /* curr_code_reachable */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
