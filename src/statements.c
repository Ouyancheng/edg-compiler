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
#include "const_ints.h"
#include "trans_lims.h"
#include "symbol_tbl.h"
#include "mem_manage.h"


static a_struct_stmt_stack_entry_ptr
		struct_stmt_stack_container = NULL;
			/* A dynamically allocated array of structured
			   statement stack entries that is can accommodate
			   the coexistence of more than one stack.  When a
                           stack is currently active and a new stack is
                           required (for member function definitions of
			   local classes, for instance) an unused segment of
			   the container is employed; later the inactive
			   stack can be reactivated.  Because the container
			   is dynamically allocated, it can be expanded if
			   necessary.   size_struct_stmt_stack_container
			   gives the number of elements currently allocated.
			   Allocation is not per-file.*/
static sizeof_t	size_struct_stmt_stack_container = 0;
			/* Size of struct_stmt_stack_container, in terms of
			   the number of elements. */
#define STRUCT_STMT_STACK_INCREMENTAL_ALLOCATION 30
			/* The number of elements added to struct_stmt_stack
			   each time it is reallocated; also the initial
			   allocation. */

static a_reachability_summary
		curr_reachability;
			/* Indicates whether or not the current location in
			   the code (following the last statement of the top
			   structured statement on the statement stack) is
			   reachable by flowing into it from the previous
			   statement. */

/*
Set var to indicate that the associated code is reachable.
*/
#define set_reachable(var)                                            \
{ (var).reachable = TRUE;                                             \
  (var).reachable_considering_hints = TRUE;                           \
  (var).suppress_unreachable_warning = FALSE;                         \
}  /* set_reachable */

/*
Set var to indicate that the associated code is unreachable.
*/
#define set_unreachable(var)                                          \
{ (var).reachable = FALSE;                                            \
  (var).reachable_considering_hints = FALSE;                          \
  (var).suppress_unreachable_warning = FALSE;                         \
}  /* set_unreachable */


/*
Declarations needed because of mutual recursion:
*/
static a_boolean statement(void);


/*
Macro to check the lint-style "notreached" flag and record information on
it in the curr_reachability flag so as to suppress warnings that might
otherwise be issued later.
*/
#define check_lint_notreached_flag()                                  \
{ if (lint_notreached_flag) {                                         \
    curr_reachability.reachable_considering_hints = FALSE;            \
    curr_reachability.suppress_unreachable_warning = TRUE;            \
    lint_notreached_flag = FALSE;                                     \
  }  /* if */                                                         \
}  /* check_lint_notreached_flag */


static void merge_reachability(a_reachability_summary *reachability,
                               a_reachability_summary *merged_reachability)
/*
Merge the reachability information from "reachability" into
"merged_reachability".
*/
{
  merged_reachability->reachable |= reachability->reachable;
  merged_reachability->reachable_considering_hints |= 
                                    reachability->reachable_considering_hints;
  merged_reachability->suppress_unreachable_warning |=
                                    reachability->suppress_unreachable_warning;
}  /* merge_reachability */


static a_struct_stmt_stack_entry_ptr find_enclosing_block_struct_stmt(void)
/*
Return a pointer to the structure statement stack entry corresponding to the
nearest enclosing compound statement.
*/
{
  a_struct_stmt_stack_entry_ptr sssep;

  for (sssep = &struct_stmt_stack[depth_stmt_stack]; ; sssep--) {
    if (sssep->kind == ssk_compound) {
      /* The structured statement is a compound statement. */
      break;
    }  /* if */
    check_assertion(sssep != &struct_stmt_stack[0]);
  }  /* for */
  return sssep;
}  /* find_enclosing_block_struct_stmt */

/*
Return a pointer to the nearest enclosing compound statement.
*/
#define nearest_enclosing_compound_statement()                        \
    find_enclosing_block_struct_stmt()->statement;


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
  statement_list_allowed = FALSE;
  if (sssep->extra_block != NULL) {
    /* An extra block statement has already been added under the primary
       statement.  The instruction should be added under this extra block. */
    head_ptr = &sssep->extra_block->variant.block.statements;
    statement_list_allowed = TRUE;
  } else {
    ssp = sssep->statement;
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
      case stmk_for:
        if (sssep->for_init) {
          head_ptr = &ssp->variant.for_loop.extra_info->initialization;
        } else {
          head_ptr = &ssp->variant.for_loop.statement;
        }  /* if */
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
      case stmk_try_block:
        head_ptr = &ssp->variant.try_block.statement;
        break;
#if CHECKING
      default:
        internal_error("add_statement: bad kind of stmt in struc. stmt stack");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */

  /* Maintain the code reachable flag.  Labels are always reachable. */
  if (kind == (a_statement_kind)stmk_label) set_reachable(curr_reachability);

  /* Allocate the statement entry. */
  sp = alloc_statement(kind);
  /* Set the position from pos_curr_token. */
  set_stmt_source_position(sp->position, pos_curr_token);

  /* See if the statement can be attached under the existing statement. */
  if (*head_ptr != NULL && !statement_list_allowed) {
    /* The structured statement already has a statement attached to it,
       and it is not a statement to which a list of statements may
       be attached.  This happens in rare cases like

         if (a) b: c = 1;

       i.e., the dependent statement of the "if" is labeled, and therefore
       two dependent statements are required under the if, which only allows
       one.  It also happens for "continue" labels.  For cases like this,
       we create an additional block to contain the list of statements. 
       If the dependent statement is a block (because the source dependent
       statement is a block), that block is used. */
    if ((*head_ptr)->kind == (a_statement_kind)stmk_block &&
        (*head_ptr)->variant.block.extra_info->assoc_scope == NULL &&
        !(*head_ptr)->dependent_statement) {
      /* There is an existing block from a source construct.  Find the 
         end of its statement list, and add there.  Note that blocks that
         contain declarations are ruled out: we don't want to add a
         statement inside such a block.  (That's especially true in
         C++, where the end of the block may kick off destructor calls
         which must be done before the statement being added is executed.)
         Also note that the top compound statement of a switch never has
         an associated scope at this point (the scope gets added at the
         closing brace), so it's acceptable, which is what we want. */
      extra_block = *head_ptr;
      temp_stmt = extra_block->variant.block.statements;
      if (temp_stmt != NULL) {
        while (temp_stmt->next != NULL) temp_stmt = temp_stmt->next;
      }  /* if */
      sssep->last_dep_statement = temp_stmt;
    } else {
      /* Create a new block to allow additional statements. */
      extra_block = alloc_statement((a_statement_kind)stmk_block);
      extra_block->variant.block.statements = *head_ptr;
      *head_ptr = extra_block;
    }  /* if */
    head_ptr = &extra_block->variant.block.statements;
    sssep->extra_block = extra_block;
  } /* if */
  /* Add the new statement to the end of the statement list for the
     current level of the structured statement stack.  Even unreachable
     code is kept. */
  if (*head_ptr == NULL) {
    /* Add the statement as the first statement on the list. */
    *head_ptr = sp;
  } else {
    if (sssep->last_dep_statement == NULL) {
      /* If the last pointer is NULL, find the last statement in the list
         and set the pointer to it.  This is needed when switching back to
         a statement list that already has some statements in it (e.g.,
         after a break statement in a switch). */
      temp_stmt = *head_ptr;
      while (temp_stmt->next != NULL) temp_stmt = temp_stmt->next;
      sssep->last_dep_statement = temp_stmt;
    }  /* if */
    sssep->last_dep_statement->next = sp;
  }  /* if */
  sssep->last_dep_statement = sp;

  /* Turn off curr_reachability if the current statement is an
     unconditional branch. */
  if (kind == (a_statement_kind)stmk_goto   ||
      kind == (a_statement_kind)stmk_return) {
    set_unreachable(curr_reachability);
  }  /* if */
  /* If the statement was an executable statement, set a flag indicating
     that an executable statement has been seen in the current block. */
  if (kind != (a_statement_kind)stmk_init) {
    struct_stmt_stack[depth_stmt_stack].any_exec_statement_seen = TRUE;
  } else {
    ++(find_enclosing_block_struct_stmt()->init_count);
  }  /* if */

  db_exit();
  return(sp);
}  /* add_statement */


static void check_for_unreachable_code(void)
/*
Generate a warning if the current location in the code is unreachable.
*/
{
  if (!curr_reachability.reachable) {
    if (!curr_reachability.suppress_unreachable_warning) {
      warning(ec_code_is_unreachable);
      /* Suppress the warning once it has been issued. */
      curr_reachability.suppress_unreachable_warning = TRUE;
    }  /* if */
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
  if (!curr_reachability.reachable) {
    if (!curr_reachability.suppress_unreachable_warning) {
      warning(ec_loop_not_reachable);
      /* Suppress the warning once it has been issued. */
      curr_reachability.suppress_unreachable_warning = TRUE;
    }  /* if */
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
    label->parent_block = nearest_enclosing_compound_statement();
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


static void expand_struct_stmt_stack(void)
/*
Reallocate the structured statement stack container, copying the contents
of the present one into the new one.  Also reset static variables defining
the size and state of the stack:  size_struct_stmt_stack_container,
struct_stmt_stack_container, and struct_stmt_stack.
*/
{
  sizeof_t  struct_stmt_stack_offset, new_size;

  /* Note that struct_stmt_stack is a pointer into the container.  This
     allows several stacks to coexist, though only that pointed to by
     struct_stmt_stack is currently active.  The offset computed is the
     element count from the start of the container to the start of the
     currently active stack. */
  struct_stmt_stack_offset = struct_stmt_stack - struct_stmt_stack_container;
  /* Recompute the size of the container. */
  new_size = size_struct_stmt_stack_container +
                                      STRUCT_STMT_STACK_INCREMENTAL_ALLOCATION;
  /* Reallocate the container, copying the old to the new. */
  struct_stmt_stack_container =
                       (a_struct_stmt_stack_entry_ptr)realloc_general(
                       (char *)struct_stmt_stack_container,
                       (sizeof_t)(size_struct_stmt_stack_container*
                                            sizeof(a_struct_stmt_stack_entry)),
                       (sizeof_t)(new_size*sizeof(a_struct_stmt_stack_entry)));
  /* Record the size of the container. */
  size_struct_stmt_stack_container = new_size;
  /* Recompute the address of the struct_stmt_stack.  The offset remains the
     the same, but the address of the container has changed. */
  struct_stmt_stack = struct_stmt_stack_container + struct_stmt_stack_offset;
}  /* expand_struct_stmt_stack */


/* Macro to check whether the structured statement stack is large enough to
   accept one more entry and if it is not to reallocate it to a larger size. */
#define ensure_struct_stmt_stack_space()                            	\
  if ((sizeof_t)(struct_stmt_stack -					\
		 struct_stmt_stack_container +                          \
                 depth_stmt_stack + 1) ==                               \
                                          size_struct_stmt_stack_container) { \
    expand_struct_stmt_stack();                                   \
  }  /* if */


void new_struct_stmt_stack(a_ptrdiff               *saved_container_pos,
                           int                     *saved_depth_stmt_stack,
                           a_reachability_summary  *saved_code_reachability)
/*
Save the state of the current structured statement stack, returning it to
the caller, and create a new structured statement stack.  This is used to
support function definitions nested within function definitions -- a
possibility in C++ with member functions of local classes.  There is no
algorithmic limit on the number of levels of nesting supported.
*/
{
  /* Expand the structured statement stack if necessary. */
  ensure_struct_stmt_stack_space();
  *saved_container_pos = struct_stmt_stack - struct_stmt_stack_container;
  *saved_depth_stmt_stack = depth_stmt_stack;
  struct_stmt_stack = &struct_stmt_stack[depth_stmt_stack+1];
  depth_stmt_stack = -1;
  *saved_code_reachability = curr_reachability;
}  /* new_struct_stmt_stack */


void restore_struct_stmt_stack(a_ptrdiff              saved_container_pos,
                               int                    saved_depth_stmt_stack,
                               a_reachability_summary *saved_code_reachability)
/*
Using state values returned from new_struct_stmt_stack, restore the original
statement stack.
*/
{
#if CHECKING
  if (saved_container_pos < 0 ||
      saved_container_pos > (a_ptrdiff)size_struct_stmt_stack_container) {
    internal_error(
              "restore_struct_stmt_stack: saved_container_pos out of range");
  } else if (saved_container_pos + saved_depth_stmt_stack >
                                      (int)size_struct_stmt_stack_container) {
    internal_error(
          "restore_struct_stmt_stack: saved_depth_stmt_stack out of range");
  }  /* if */
#endif /* CHECKING */  
  struct_stmt_stack = &struct_stmt_stack_container[saved_container_pos];
  depth_stmt_stack = saved_depth_stmt_stack;
  curr_reachability = *saved_code_reachability;
}  /* restore_struct_stmt_stack */


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
  /* Expand the structured statement stack if necessary. */
  ensure_struct_stmt_stack_space();
  /* Push the stack and initialize the new entry. */
  sssep = &struct_stmt_stack[++depth_stmt_stack];
  sssep->kind                 = kind;
  sssep->in_else_of_if        = FALSE;
  sssep->statement            = sp;
  sssep->curr_switch_clause   = NULL;
  sssep->extra_block          = NULL;
  sssep->last_dep_statement   = NULL;
  sssep->break_label          = NULL;
  sssep->continue_label       = NULL;
  sssep->switch_selector_type = NULL;
  sssep->switch_has_default_clause
                              = FALSE;
  sssep->rout_type_explicitly_specified
                              = FALSE;
  sssep->any_exec_statement_seen
                              = FALSE;
  sssep->for_init             = FALSE;
  sssep->init_count           = 0;
  if (kind != ssk_compound || sp->dependent_statement) {
    /* For statements other than blocks, copy down the any_exec_statement_seen
       flag.  It's really being maintained for the block containing this
       non-block statement, and it gets copied back up at the end of the
       statement. */
    sssep->any_exec_statement_seen = sssep[-1].any_exec_statement_seen;
  }  /* if */
  sssep->start_reachable      = curr_reachability;
  set_unreachable(sssep->end_reachable);  /* So far. */
  if (kind == ssk_while || kind == ssk_do || kind == ssk_for) {
    /* The bodies of loops are reachable in that the bottom can branch to
       the top. */
    set_reachable(curr_reachability);
  } else if (kind == ssk_switch) {
    /* The body of a switch is not reachable until a case or default label
       appears. */
    set_unreachable(curr_reachability);
  }  /* if */
  db_exit();
}  /* push_stmt_stack */


static void end_stmt_sequence(a_struct_stmt_stack_entry_ptr sssep)
/*
End a statement sequence under a structured statement, i.e., clear
the flags used by add_statement to add statements at the end of a
sequence.
*/
{
  sssep->extra_block        = NULL;
  sssep->last_dep_statement = NULL;
}  /* end_stmt_sequence */


static void start_stmt_clause(a_struct_stmt_stack_entry_ptr sssep)
/*
Start a new clause of a structured statement.  sssep points to the
struct_stmt_stack entry for the structured statement (since it need not
be the topmost one).
*/
{
  /* The start of a clause is reachable if the start of the structured
     statement is reachable. */
  curr_reachability = sssep->start_reachable;
  end_stmt_sequence(sssep);
}  /* start_stmt_clause */


static void term_stmt_clause(a_struct_stmt_stack_entry_ptr sssep)
/*
end the current clause of a structured statement.  sssep points to the
struct_stmt_stack entry for the structured statement.
*/
{
  /* If the end of the clause is reachable, then the end of the whole
     structured statement is reachable. */
  merge_reachability(&curr_reachability, &sssep->end_reachable);
  end_stmt_sequence(sssep);
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
      stmt->kind == (a_statement_kind)stmk_end_test_while ||
      stmt->kind == (a_statement_kind)stmk_for) {
    expr = stmt->expr;
    /* In the "for" loop, the expression can be NULL and that implies an
       infinite loop. */
    if (expr == NULL) {
      is_inf_loop = TRUE;
    } else if (expr->kind == (an_expr_node_kind)enk_constant) {
      if (!is_false_constant(expr->variant.constant)) {
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
  a_statement_ptr                        sp;
  
  db_enter(4, "pop_stmt_stack");
  sssep = &struct_stmt_stack[depth_stmt_stack];
  kind = sssep->kind;
  sp = sssep->statement;
  /* Close the final clause of the statement, if any. */
  if ((kind != ssk_switch || sssep->curr_switch_clause != NULL) &&
      kind != ssk_try_block) {
    term_stmt_clause(sssep);
  }  /* if */
  /* Determine whether or not the code following the statement is reachable,
     and set curr_reachability appropriately. */
  if (kind == ssk_while || kind == ssk_for || kind == ssk_do) {
    /* A loop. */
    if (is_infinite_loop(sp)) {
      /* An infinite loop.  The code after the loop is not reachable. */
      set_unreachable(curr_reachability);
    } else if (kind == ssk_while || kind == ssk_for) {
      /* A top-test loop.  The code after the loop is reachable if the current
         location is reachable or if the start of the loop is reachable. */
      merge_reachability(&sssep->start_reachable, &curr_reachability);
    } else {
      /* A bottom-test loop.  The code after the loop is reachable if the
         current location is reachable. */
    }  /* if */
  } else {
    /* Non-loop statement. */
    if ((kind == ssk_switch && !sssep->switch_has_default_clause) ||
        (kind == ssk_if && sp->variant.if_stmt.else_statement == NULL)) {
      /* Switch statement without a default, or if without an else.  If the
         initial statement can be reached, the end can be reached. */
      merge_reachability(&sssep->start_reachable, &sssep->end_reachable);
    }  /* if */
    /* The code after the statement can be reached if the end of the statement
       can be reached. */
    curr_reachability = sssep->end_reachable;
  }  /* if */
  /* If the statement just exited is a non-block, propagate the
     any_exec_statement_seen flag upwards. */
  if (kind != ssk_compound || sp->dependent_statement) {
    sssep[-1].any_exec_statement_seen = sssep->any_exec_statement_seen;
  }  /* if */
  /* Pop the stack. */
  depth_stmt_stack--;
  /* If the break label for this statement was referenced, generate 
     its definition now.  This must be done after depth_stmt_stack is
     decremented so that the label will appear outside the structured
     statement.  It must also be done after curr_reachability has been
     adjusted. */
  define_label(sssep->break_label);
  db_exit();
}  /* pop_stmt_stack */


static void asm_statement(void)
/*
Scan an asm statement.  This is a non-ANSI construct but it is defined in
C++.  Its form is

asm ( "string" ) ;

*/
{
  a_statement_ptr sp;

  db_enter(3, "asm_statement");

  check_for_unreachable_code();
  /* Allocate the statement. */
  sp = add_statement((a_statement_kind)stmk_asm);
  sp->variant.asm_entry = asm_declaration(/*asm_decl_allowed=*/TRUE);

  db_exit();
}  /* asm_statement */


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


static void start_block_statement(a_statement_ptr *block,
                                  a_boolean       dependent_statement)
/*
Do processing to begin a block or compound statement.  Return a pointer
to the block statement in *block.  dependent_statement is TRUE if the
block is being created to surround a dependent statement in C++.
*/
{
  a_boolean cfront_dependent_statement = 
                              cfront_compatibility_mode && dependent_statement;

  *block = add_statement((a_statement_kind)stmk_block);
  if (cfront_dependent_statement) {
    /* This is a dependent statement in cfront mode, which is special in
       that no scope is created for it.  Mark the block for special
       processing in IL lowering or a back end: anything constructed
       within the block must also be destroyed therein.  This flag must
       be set before push_stmt_stack is called. */
    (*block)->dependent_statement = TRUE;
  }  /* if */
  /* Make the parent pointer in the block point to the nearest enclosing
     compound statement. */
  (*block)->variant.block.extra_info->parent_block =
                                        nearest_enclosing_compound_statement();
  /* Push an entry on the structured statement stack. */
  push_stmt_stack(ssk_compound, *block);
  /* Push an associated scope.  This does not allocate the IL scope yet.
     Do not do this in cfront compatibility mode (the old rule was that no
     scope is created). */
  if (!cfront_dependent_statement) {
    (void)push_scope((a_scope_kind)sck_block, NO_SCOPE_NUMBER,
                     (a_type_ptr)NULL, (a_routine_ptr)NULL,
                     (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                     (a_template_arg_ptr)NULL);
  }  /* if */
}  /* start_block_statement */


static void finish_block_statement(a_statement_ptr block)
/*
Do processing to finish a block or compound statement.  block points to the
block statement.
*/
{
  a_scope_ptr scope_ptr;

  /* Remember whether or not the end of the block is reachable.  This
     is helpful in IL lowering. */
  block->variant.block.extra_info->end_of_block_reachable = 
                                                   curr_reachability.reachable;
  if (!block->dependent_statement) {
    /* Store the IL scope pointer in the block.  This is NULL except for
       blocks with declarations. */
    scope_ptr = scope_stack[decl_scope_level].il_scope;
    if (scope_ptr != NULL) {
      block->variant.block.extra_info->assoc_scope = scope_ptr;
      scope_ptr->assoc_block = block;
    }  /* if */
    /* Pop the name scope. */
    pop_scope();
  }  /* if */
  /* Pop the statement stack. */
  pop_stmt_stack();
}  /* finish_block_statement */


static void dependent_statement(void)
/*
In C++ the dependent statement of a loop-statement or a selection-statement
implicitly defines a local scope.  Push a new scope on the scope stack and then
call statement().  In C mode or when the dependent statement is a compound
statement no new scope is required.
*/
{
  a_boolean         block_added, is_executable;
  a_statement_ptr   block;
  a_source_position start_position;

  db_enter(3, "dependent_statement");
  start_position = pos_curr_token;
  /* In C++, add a block (and potential scope).  Do not do so, however,
     if a block will be created anyway. */
  if (C_dialect != C_dialect_cplusplus || curr_token == tok_lbrace) {
    block_added = FALSE;
  } else {
    /* Normal case (in C++): add a block and potential scope.
       In cfront mode, the block is added but not the scope. */
    start_block_statement(&block, /*dependent_statement=*/TRUE);
    block_added = TRUE;
  }  /* if */
  is_executable = statement();
  if (cfront_compatibility_mode && !is_executable) {
    /* In cfront mode, the dependent statement is not allowed to be a
       declaration. */
    pos_error(ec_dependent_stmt_is_declaration, &start_position);
  }  /* if */
  if (block_added) finish_block_statement(block);
  db_exit();
}  /* dependent_statement */


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
  a_statement_ptr               sp;
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
  sp->expr = scan_boolean_controlling_expression();
  /* Check for and skip the closing parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_rparen);
  /* Scan the "then" statement. */
  add_stop_token(tok_else);
  dependent_statement();
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
    dependent_statement();
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
  a_statement_ptr sp;

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
  /* Scan the controlling expression and check to see that it is integral. */
  sp->expr = scan_switch_expression();
  if (!is_error_node(sp->expr)) {
    /* The expression is integral.  Promote it (to int) if necessary. */
    if (C_dialect != C_dialect_pcc) {
      /* ANSI: the normal integral promotions are done. */
      integral_promote_node(&sp->expr);
    } else {
      /* pcc treats all switch expressions as int.  This differs from
         ANSI in that even long is cast to int. */
      cast_node(&sp->expr, integer_type((an_integer_kind)ik_int),
                /*is_implicit_cast=*/TRUE, &error_position);
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
  dependent_statement();
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
  a_statement_ptr sp;

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
  sp->expr = scan_boolean_controlling_expression();
  /* Check for and skip the closing parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_rparen);
  /* Scan the dependent statement. */
  dependent_statement();
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
  a_statement_ptr sp;

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
  dependent_statement();
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
  sp->expr = scan_boolean_controlling_expression();
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


static void try_block_statement(void)
/*
Scan a C++ try-block statement.  Its form is:

  "try" compound-statement handler-seq

*/
{
  a_statement_ptr                sp;

  db_enter(3, "try_block_statement");
  check_for_unreachable_code();
  /* Allocate the statement. */
  sp = add_statement((a_statement_kind)stmk_try_block);
  /* Push an entry on the structured statement stack. */
  push_stmt_stack(ssk_try_block, sp);
#if CHECKING
  if (curr_token != tok_try) {
    internal_error("try_block_statement: expected try");
  }  /* if */
#endif /* CHECKING */
  if (exceptions_disabled) {
    /* Support for exceptions is suppressed for this compilation. */
    pos_error(ec_no_exception_support, &pos_curr_token);
  }  /* if */
  /* Bypass "try". */
  (void)get_token();
  /* Scan the compound statement, and save a pointer to it in the try-block
     statement. */
  sp->variant.try_block.statement = compound_statement(
                                               /*at_function_level=*/FALSE,
                                               /*explicit_return_type=*/FALSE,
                                               /*is_catch_clause=*/FALSE);
  term_stmt_clause(&struct_stmt_stack[depth_stmt_stack]);
  /* The next token should be a "catch" introducing the first handler. */
  if (required_token(tok_catch, ec_missing_handler)) {
    /* Loop through the (1 or more) handler declarations, adding each to
       the linked list of handlers pointed to by sp. */
    do {
      start_stmt_clause(&struct_stmt_stack[depth_stmt_stack]);
      handler_declaration(sp);
      term_stmt_clause(&struct_stmt_stack[depth_stmt_stack]);
    } while (loop_token(tok_catch));
  }  /* if */
  /* Pop the structured statement stack. */
  pop_stmt_stack();

  db_exit();
}  /* try_block_statement */


static void expression_statement(void)
/*
Scan an expression statement.
*/
{
  a_statement_ptr   sp;
  an_expr_node_ptr  expr;
  a_source_position start_position;

  start_position = pos_curr_token;

  expr = scan_void_expression();
  /* Add the expression if is is not void. */
  if (expr != NULL) {
    sp = add_statement((a_statement_kind)stmk_expr);
    set_stmt_source_position(sp->position, start_position);
    sp->expr = expr;
  }  /* if */
}  /* expression_statement */


static void for_init_statement(void)
/*
Scan the initializing expression or, in C++, declaration of a for statement.
*/
{
  a_struct_stmt_stack_entry_ptr sssep;

  sssep = &struct_stmt_stack[depth_stmt_stack];
  /* Let add_statement know this is a for_init so that the statement is
     attached in the right place. */
  sssep->for_init = TRUE;
  if (C_dialect == C_dialect_cplusplus &&
      is_decl_not_expr(/*abstract_declarator_allowed=*/FALSE,
                       /*real_declarator_allowed=*/TRUE)) {
    /* Scan a declaration (C++ only). */
    local_declaration();
  } else {
    /* Scan an expression.  It may be omitted. */
    if (curr_token != tok_semicolon) expression_statement();
    (void)required_token(tok_semicolon, ec_exp_semicolon);
  }  /* if */
  /* Restore the for_init flag to its default value. */
  sssep->for_init = FALSE;
  /* Clear the fields that will have been updated if the for-init required
     more than one stmk_init statement, e.g.:
       for (int i = 0, j = 10; j > i; --j, ++i) { }    */
  end_stmt_sequence(sssep);
}  /* for_init_statement */


static void for_statement(void)
/*
Scan a "for" statement and add it to the current statement sequence.
The syntax is:

3.6.5  iteration-statement:
		for ( expression    ; expression    ; expression    ) statement
                                opt             opt             opt

See also 3.6.5.3.

In C++ the first expression is replaced by for-init-statement, which is
either an expression statement or a declaration statement.
*/
{
  a_statement_ptr   sp;

  db_enter(3, "for_statement");

  check_loop_unreachable_code();
  /* Allocate the for statement. */
  sp = add_statement((a_statement_kind)stmk_for);
  /* Push an entry on the structured statement stack. */
  push_stmt_stack(ssk_for, sp);
  /* Ignore the initial "for". */
#if CHECKING
  if (curr_token != tok_for) internal_error("for_statement: expected for");
#endif /* CHECKING */
  (void)get_token();
  /* Check for and skip the opening parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_stop_token(tok_rparen);
  add_stop_token(tok_semicolon);
  /* Scan the initializing expression or declaration if it is present.  It
     will be added to the correct place in the stmk_for entry. */
  for_init_statement();
  /* Scan the controlling expression if it is present, and check to see
     that it is scalar. */
  if (curr_token != tok_semicolon) {
    sp->expr = scan_boolean_controlling_expression();
  }  /* if */
  (void)required_token(tok_semicolon, ec_exp_semicolon);
  remove_stop_token(tok_semicolon);
  /* Scan the incrementing expression if it is present. */
  if (curr_token != tok_rparen) {
    sp->variant.for_loop.extra_info->increment = scan_void_expression();
  }  /* if */
  /* Check for and skip the closing parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_rparen);
  /* Scan the dependent statement. */
  dependent_statement();
  /* Define the "continue" label, if it is needed. */
  define_continue_label();
  /* Pop the structured statement stack. */
  pop_stmt_stack();

  db_exit();
}  /* for_statement */


static a_statement_ptr find_parent_statement_for_block(a_statement_ptr  block)
/*
*/
{
  a_statement_ptr  sp, parent_stmt = NULL;

  for (sp = block->variant.block.extra_info->
                            parent_block->variant.block.statements;
       parent_stmt == NULL ;
       sp = sp->next) {
    check_assertion (sp != NULL);
    switch (sp->kind) {
      case stmk_block:
        if (sp == block) parent_stmt = sp;
        break;
      case stmk_if: 
        if (sp->variant.if_stmt.then_statement == block ||
            sp->variant.if_stmt.else_statement == block) parent_stmt = sp;
        break;
      case stmk_while:
      case stmk_end_test_while:
        if (sp->variant.loop_statement == block) parent_stmt = sp;
        break;
      case stmk_for:
        if (sp->variant.for_loop.statement == block) parent_stmt = sp;
        break;
      case stmk_switch:
        if (sp->variant.switch_stmt.body_statement == block) {
          parent_stmt = sp;
        } else {
          a_switch_clause_ptr  scp = sp->variant.switch_stmt.clause_list;
          a_statement_ptr      clause_sp;

          for (; parent_stmt == NULL && scp != NULL; scp = scp->next) {
            clause_sp = scp->statements;
            for (; clause_sp != NULL; clause_sp = clause_sp->next) {
              if (clause_sp == block) {
#if 0
/* When should the parent statement be the switch statement itself and when
   should it be the statement within the clause?  Does other info have to be
   returned to the caller if the stmt in the clause is returned? */
#else
                parent_stmt = sp;
#endif /* if 0 */
                break;
              }  /* if */
            }  /* for */
          }  /* for */
        }  /* if */
        break;
      case stmk_try_block:
        if (sp->variant.try_block.statement == block) {
          parent_stmt = sp;
        } else {
          a_handler_ptr  hp = sp->variant.try_block.handlers;
          for (; hp != NULL; hp = hp->next) {
            if (hp->statement == block) {
              parent_stmt = sp;
              break;
            }  /* if */
          }  /* for */
        }  /* if */
        break;
      default:;
    }  /* switch */
  }  /* for */
  return parent_stmt;
}  /* find_parent_statement */


void check_for_stmk_init_in_statement_list(a_statement_ptr    start_stmt,
                                           a_statement_ptr    start_block,
                                           a_statement_ptr    curr_block,
                                           a_label_ptr        label,
                                           a_statement_ptr    end_block,
                                           a_source_position  *error_pos,
                                           a_boolean          *stmk_init_seen)
/*
*/
{
  a_statement_ptr  sp, last_stmt_to_check_in_curr_block = NULL;
  a_statement_ptr  first_stmt_to_check_in_curr_block, curr_block_parent;
  a_variable_ptr   vp;

  if (start_block != curr_block) {
    curr_block_parent = curr_block->variant.block.extra_info->parent_block;
    check_for_stmk_init_in_statement_list(start_stmt, start_block,
                                          curr_block_parent, (a_label_ptr)NULL,
                                          curr_block, error_pos,
                                          stmk_init_seen);
    first_stmt_to_check_in_curr_block = curr_block->variant.block.statements;
  } else if (start_stmt == NULL) {
    first_stmt_to_check_in_curr_block = curr_block->variant.block.statements;
  } else {
    first_stmt_to_check_in_curr_block = start_stmt->next;
  }  /* if */
  if (label == NULL) {
    last_stmt_to_check_in_curr_block =
                            find_parent_statement_for_block(end_block);
  }  /* if */
  for (sp = first_stmt_to_check_in_curr_block; sp != NULL; sp = sp->next) {
    if (sp->kind == (a_statement_kind)stmk_label &&
        sp->variant.label == label) {
      break;
    }  /* if */
    vp = NULL;
    if (sp->kind == (a_statement_kind)stmk_init) {
      vp = sp->variant.dynamic_init->variable;
      check_assertion(vp != NULL);
    } else if (sp->kind == (a_statement_kind)stmk_for) {
      a_statement_ptr  init = sp->variant.for_loop.extra_info->initialization;
      if (init != NULL && init->kind == (a_statement_kind)stmk_init) {
        vp = init->variant.dynamic_init->variable;
        check_assertion(vp != NULL);
      }  /* if */
    }  /* if */
    if (vp != NULL) {
      if (!*stmk_init_seen) {
        *stmk_init_seen = TRUE;
        pos_start_error(ec_jumping_over_init, error_pos);
      }  /* if */
      sym_add_diag_info(ec_name_at_decl_position,
                        (a_symbol_ptr)vp->source_corresp.assoc_info);
    }  /* if */
    if (sp == last_stmt_to_check_in_curr_block) break;
  }  /* for */
}  /* check_for_stmk_init_in_statement_list */


static a_boolean block_is_on_parent_list(a_statement_ptr block,
                                         a_statement_ptr block2)
/*
Return TRUE if the block statement "block" is on the list of parent blocks
of block2. */
{
  a_statement_ptr  parent;
  a_boolean        on_list = FALSE;

  for (parent = block2->variant.block.extra_info->parent_block;
       parent != NULL;
       parent = parent->variant.block.extra_info->parent_block) {
    if (block == parent) {
      on_list = TRUE;
      break;
    }  /* if */
  }  /* for */
  return on_list;
}  /* block_is_on_parent_list */


static void check_forwards_goto(a_statement_ptr   label_statement,
                                a_goto_entry_ptr  gep)
/*
*/
{
  a_label_ptr      label;
  a_statement_ptr  goto_block, label_block, sp;
  a_statement_ptr  start_statement, start_block;
  unsigned long    goto_block_init_count, label_block_init_count;
  a_boolean        stmk_init_seen = FALSE;
  a_boolean        do_check = TRUE;

  db_enter(4, "check_forwards_goto");
  goto_block = gep->assoc_block;
  goto_block_init_count = gep->block_init_count;
  label = label_statement->variant.label;
  label_block = label->parent_block;
  check_assertion(label->parent_block ==
                     find_enclosing_block_struct_stmt()->statement);
  label_block_init_count = find_enclosing_block_struct_stmt()->init_count;
  if (label_block == goto_block) {
    /* goto forwards within same block. */
    if (label_block_init_count > goto_block_init_count) {
      /* Error. */
      start_statement = gep->goto_statement;
      start_block = goto_block;
    } else {
      do_check = FALSE;
    }  /* if */
  } else if (block_is_on_parent_list(goto_block, label_block)) {
    /* A forward goto from an outer block to a label in an inner block, the
       latter enclosed within the former. */
    start_statement = gep->goto_statement;
    start_block = goto_block;
  } else if (block_is_on_parent_list(label_block, goto_block)) {
    /* A forward goto from an inner block to a label in an enclosing block. */
    sp = goto_block;
    while (sp->variant.block.extra_info->parent_block != label_block) {
      sp = sp->variant.block.extra_info->parent_block;
      check_assertion(sp != NULL);
    }  /* while */
    start_statement = find_parent_statement_for_block(sp);
    start_block = label_block;
  } else {
    /* A forward goto from a block to another block, neither block contained
       within the other. */
    a_statement_ptr  common_parent;

    common_parent = label_block->variant.block.extra_info->parent_block;
    while (!block_is_on_parent_list(common_parent, goto_block)) {
      common_parent = common_parent->variant.block.extra_info->parent_block;
      check_assertion(common_parent != NULL);
    }  /* while */
    sp = goto_block;
    while (sp->variant.block.extra_info->parent_block != common_parent) {
      sp = sp->variant.block.extra_info->parent_block;
      check_assertion(sp != NULL);
    }  /* while */
    start_statement = find_parent_statement_for_block(sp);
    start_block = common_parent;
  }  /* if */
  if (do_check) {
    check_for_stmk_init_in_statement_list(start_statement, start_block,
                                          label_block, label,
                                          label_block, &gep->source_position,
                                          &stmk_init_seen);
    if (stmk_init_seen) end_error();
  }  /* if */
  db_exit();
}  /* check_forwards_goto */


static void check_backwards_goto(a_statement_ptr   goto_statement)
/*
*/
{
  a_label_ptr                    label;
  a_statement_ptr                goto_block, label_block, sp;
  a_statement_ptr                start_statement, start_block;
#if 0
  a_symbol_ptr                   label_sym;
  unsigned long                  goto_block_init_count, label_block_init_count;
#endif /* if 0 */
  a_struct_stmt_stack_entry_ptr  sssep;
  a_boolean                      stmk_init_seen = FALSE;
  a_boolean                      do_check = TRUE;

  db_enter(4, "check_backwards_goto");
  sssep = find_enclosing_block_struct_stmt();
  goto_block = sssep->statement;
  label = goto_statement->variant.label;
  label_block = label->parent_block;
#if 0
  goto_block_init_count = sssep->init_count;
  label_sym = (a_symbol_ptr)label->source_corresp.assoc_info;
  label_block_init_count =
               label_sym->variant.label.variant.curr_block_init_count;
#endif /* if 0 */
  if (label_block == goto_block) {
    /* goto backwards within same block -- no error if this a the function
       scope. */
    if (label_block->variant.block.extra_info->parent_block == NULL) {
      /* No error */
      do_check = FALSE;
    } else {
      start_statement = NULL;
      start_block = goto_block;
    }  /* if */
  } else if (block_is_on_parent_list(label_block, goto_block)) {
    /* goto backwards to a containing block -- no error. */
    do_check = FALSE;
  } else if (block_is_on_parent_list(goto_block, label_block)) {
    /* goto backward from an outer block to an inner block. */
    sp = label_block;
    while (sp->variant.block.extra_info->parent_block != goto_block) {
      sp = sp->variant.block.extra_info->parent_block;
      check_assertion(sp != NULL);
    }  /* while */
    start_statement = NULL;
    start_block = sp;
  } else {
    /* goto backward from one block to another, with common parent. */
    a_statement_ptr  common_parent;

    common_parent = label_block->variant.block.extra_info->parent_block;
    while (!block_is_on_parent_list(common_parent, goto_block)) {
      common_parent = common_parent->variant.block.extra_info->parent_block;
      check_assertion(common_parent != NULL);
    }  /* while */
    sp = label_block;
    while (sp->variant.block.extra_info->parent_block != common_parent) {
      sp = sp->variant.block.extra_info->parent_block;
      check_assertion(sp != NULL);
    }  /* while */
    start_statement = NULL;
    start_block = sp;
  }  /* if */
  if (do_check) {
    check_for_stmk_init_in_statement_list(start_statement, start_block,
                                          label_block, label,
                                          label_block, &error_position,
                                          &stmk_init_seen);
    if (stmk_init_seen) end_error();
  }  /* if */
  db_exit();
}  /* check_backwards_goto */


static void check_jump_over_initialization(a_statement_ptr  sp)
/*
*/
{
  a_label_ptr                    label;
  a_symbol_ptr                   label_sym;
  a_goto_entry_ptr               gep, end_of_list;
  a_struct_stmt_stack_entry_ptr  sssep;


  check_assertion (sp->kind == (a_statement_kind)stmk_label ||
                   sp->kind == (a_statement_kind)stmk_goto);
  label = sp->variant.label;
  label_sym = (a_symbol_ptr)label->source_corresp.assoc_info;
  if (sp->kind == (a_statement_kind)stmk_label) {
    /* This is the definition of the label. */
    gep = label_sym->variant.label.variant.goto_list;
    if (gep != NULL) {
      /* There was at least one forward goto referencing this label.  For
         each check whether it jumped over any initializing declarations. */
      do {
        check_forwards_goto(sp, gep);
        gep = gep->next;
      } while (gep != NULL);
      /* Free the list of goto entries for reuse. */
      free_goto_entry_list(&label_sym->variant.label.variant.goto_list);
    }  /* if */
    /* Record the number initializing declarations seen so far in the current
       block. */
    label_sym->variant.label.variant.curr_block_init_count =
                       find_enclosing_block_struct_stmt()->init_count;
  } else {
    if (label_sym->defined) {
      /* This is a backwards goto -- i.e., it references a label that has
         already been defined.  Check whether it jumps over any initializing
         declarations. */
      check_backwards_goto(sp);
    } else {
      /* This is a forwards goto -- i.e., it references a label that has not
         yet been defined.  Record information about it so that, when the
         label definition is reached, a check can made whether it involves
         jumping over any initializing declarations. */
      /* Allocate and fill in a goto entry. */
      gep = alloc_goto_entry();
      gep->goto_statement = sp;
      sssep = find_enclosing_block_struct_stmt();
      gep->assoc_block = sssep->statement;
      gep->source_position = error_position;
      gep->block_init_count = sssep->init_count;
      /* Add it to the end of the goto-entry list of the label symbol. */
      if (label_sym->variant.label.variant.goto_list == NULL) {
        label_sym->variant.label.variant.goto_list = gep;
      } else {
        end_of_list = label_sym->variant.label.variant.goto_list;
        while (end_of_list->next != NULL) end_of_list = end_of_list->next;
        end_of_list->next = gep;
      }  /* if */
    }  /* if */
  }  /* if */
}  /* check_jump_over_initialization */


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
  if (C_dialect == C_dialect_cplusplus) {
    /* If this is a forward reference to a label, record information about
       the goto to allow diagnosis of jump-over-initialization errors.  If
       it is backward reference, do the checking immediately. */
    check_jump_over_initialization(sp);
  }  /* if */
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
  } else {
    if (sssep->kind == ssk_switch &&
        sssep->curr_switch_clause != NULL) {
      /* A break that exits a switch clause. */
      if (depth_stmt_stack != 0 &&
          &struct_stmt_stack[depth_stmt_stack-1] == sssep) {
        /* This break statement exits a switch clause in a way that can
           be represented implicitly as the default action at the end of
           the clause.  No goto is required.  However, the current switch
           clause must be ended.  Note that this special trick can be done
           only when the break is at the top level in the case clause. */
        set_stmt_source_position(sssep->curr_switch_clause->break_position,
                                 pos_curr_token);
        sssep->curr_switch_clause = NULL;
        term_stmt_clause(sssep);
        set_unreachable(curr_reachability);
        goto break_handled;
      }  /* if */
    }  /* if */
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
break_handled:;
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
  a_routine_ptr   rout;
  a_type_ptr      tp;
  a_symbol_ptr    function_name_symbol;
  a_boolean       issue_no_value_returned_diag;
  an_error_severity
		  no_returned_value_severity;


  /* Get a pointer to the current routine entry, and get its return
     type. */
  rout = current_routine_entry();
  tp = rout->type->variant.routine.return_type;
  issue_no_value_returned_diag = FALSE;
  if (!is_void_type(tp) && !is_error_type(tp)) {
    if (C_dialect != C_dialect_cplusplus) {
      /* If a return with no expression appears in a function with a
         non-void type, issue a diagnostic.  Do not issue the diagnostic for
         the main program, or if the declaration of the function did not
         have an explicit type specifier (omitting the specifier implies
         "int", but may have been intended to mean "void" in old-style C). */
      if (!struct_stmt_stack->rout_type_explicitly_specified) {
        /* No diagnostic if the routine's type was not explicitly specified. */
      } else if (rout == il_header.main_routine) {
        /* No diagnostic for "main". */
      } else {
        issue_no_value_returned_diag = TRUE;
        no_returned_value_severity = es_warning;
      }  /* if */
    } else {
      /* C++:  Issue a diagnostic unless we are returning from a constructor.
	 The diagnostic is either a warning or a strict ANSI diagnostic.
	 While the ARM (6.6.3) does not appear to special case "main"
	 it seems inappropriate to issue an error for a program that may
	 exit from "main" using the "exit" function;  So only a warning
	 is given for main.  There is no special case for cases in which
	 the return type is not explicit. */
      if (rout->special_kind == (a_special_function_kind)sfk_constructor) {
        /* Constructors will not have a return expression since at the source
           level they have no return type; however, in the IL they are
           represented as returning the "this" parameter. */
      } else {
        issue_no_value_returned_diag = TRUE;
	if (strict_ansi_mode && rout != il_header.main_routine) {
          no_returned_value_severity = strict_ansi_error_severity;
        } else {
          no_returned_value_severity = es_warning;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */

  /* Output diagnostic about no value returned from non-void function
     if necessary. */
  if (issue_no_value_returned_diag) {
    /* Get pointer to the symbol for the function name. */
    function_name_symbol = (a_symbol_ptr)rout->source_corresp.assoc_info;
#if CHECKING
    if (function_name_symbol == NULL) {
        internal_error("check_void_return_okay: unexpected NULL assoc_info");
    }  /* if */
#endif /* CHECKING */
    sym_diagnostic(no_returned_value_severity,
                   ec_no_value_returned_in_non_void_function,
                   function_name_symbol);
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
  a_statement_ptr    sp;
  an_expr_node_ptr   return_expr = NULL;
  a_dynamic_init_ptr dip = NULL;
  a_routine_ptr      rout;
  a_type_ptr         return_type, routine_type;
  a_boolean          void_return_used = FALSE;
  a_source_position  return_pos, expr_pos;

  db_enter(3, "return_statement");
  check_for_unreachable_code();
  /* Ignore the initial "return". */
#if CHECKING
  if (curr_token != tok_return) {
    internal_error("return_statement: expected return");
  }  /* if */
#endif /* CHECKING */
  /* Save the position of the beginning of the return statement. */
  return_pos = pos_curr_token;
  (void)get_token();
  add_stop_token(tok_semicolon);
  /* Get a pointer to the current routine entry. */
  rout = current_routine_entry();
  /* See if the optional expression is present. */
  if (curr_token == tok_semicolon) {
    /* The expression is missing. */
    check_void_return_okay();
    if (rout->special_kind == (a_special_function_kind)sfk_constructor) {
      /* In a constructor the user may not specify a return value.  However,
         the IL contains code to return the "this" variable. */
      return_expr = this_param_value_expr();
    }  /* if */
  } else {
    /* The expression is present. */
    /* Get the return type of the current routine entry. */
    routine_type = skip_typerefs(rout->type);
    return_type = routine_type->variant.routine.return_type;
    if (rout->special_kind == (a_special_function_kind)sfk_constructor ||
        rout->special_kind == (a_special_function_kind)sfk_destructor) {
      /* Constructors and destructors may not return a value (ARM 6.6.3). */
      error(ec_value_returned_in_constructor);
      return_type = error_type();
    } else if (is_void_type(return_type)) {
      /* A void function may not return a value.  Accept with a warning
         in cfront compatibility mode. */
      if (cfront_compatibility_mode) {
        warning(ec_value_returned_in_void_function);
        void_return_used = TRUE;
      } else {
        error(ec_value_returned_in_void_function);
        return_type = error_type();
      }  /* if */
    }  /* if */
    /* Save the position of the start of the expression.  This is used
       if we need to create a new statement for the expression on
       a return in a void function in cfront mode. */
    expr_pos = pos_curr_token;
    /* Scan the return expression and convert it to the function type. */
    return_expr = scan_return_expression(return_type,
                                         ec_bad_return_value_type,
                                         &dip);
  }  /* if */
  /* If a return expression was found in a void function (which is allowed
     in cfront mode) generate an expression statement that is output
     before the return statement.  This is done to prevent generating
     a return statement in the IL that has a void type and yet contains
     a return expression. */
  if (void_return_used && return_expr != NULL) {
    sp = add_statement((a_statement_kind)stmk_expr);
    sp->expr = return_expr;
    set_stmt_source_position(sp->position, expr_pos);
    set_expr_result_not_used(return_expr);
    return_expr = NULL;
  }  /* if */
  /* Allocate the return statement. */
  sp = add_statement((a_statement_kind)stmk_return);
  sp->expr = return_expr;
  sp->variant.dynamic_init = dip;
  set_stmt_source_position(sp->position, return_pos);
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
  a_reachability_summary
                      prev_reachability, save_reachability;

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
              cmp_integer_constants(cp, constant_ptr) == 0) {
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
      (can_add_to_curr_clause && scp->constant_list == NULL)) {
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
         cp != NULL && cmp_integer_constants(cp, constant_ptr) < 0;
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
      if (curr_reachability.reachable) {
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
    if (label != NULL) {
      /* Save reachability information on the flow-in. */
      prev_reachability = curr_reachability;
    }  /* if */
    /* Activate the new switch clause so code will be added here (if the
       switch is the outermost structured statement). */
    sssep->curr_switch_clause = scp;
    /* Start a new clause. */
    start_stmt_clause(sssep);
    if (label != NULL) {
      /* Define the label for one of the gotos above, if necessary.
         Since we generated this label, we can do a better job of maintaining
         the reachability than is done by the low-level routines. */
      save_reachability = curr_reachability;
      define_label(label);
      curr_reachability = save_reachability;
      merge_reachability(&prev_reachability, &curr_reachability);
    }  /* if */
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
  a_boolean                     did_not_fold;
  a_constant                    constant;
  a_constant_ptr                constant_ptr = NULL;

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
  scan_integral_constant_expression(&constant);
  if (is_error_constant(&constant)) {
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
                           /*is_implicit_cast=*/TRUE,
                           /*constant_context=*/TRUE, &did_not_fold,
                           &error_position);
    }  /* if */
    /* Allocate a copy of the case constant. */
    constant_ptr = alloc_unshared_constant(&constant);
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
    set_reachable(curr_reachability);
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
    set_reachable(curr_reachability);
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


static a_boolean statement(void)
/*
Scan a statement.  Add it to the current statement sequence.  Return
TRUE if the statement is executable, FALSE if it is a declaration (C++ mode
only).
*/
{
  a_label_ptr      label;
  a_boolean        prev_was_label = FALSE, is_declaration = FALSE;
  a_boolean        get_another_statement;

  db_enter(3, "statement");

rescan_statement:
  get_another_statement = FALSE;
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
      (void)compound_statement(/*at_function_level=*/FALSE,
                               /*explicit_return_type=*/FALSE,
                               /*is_catch_clause=*/FALSE);
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
    case tok_asm:
      /* Asm "declaration" (ARM 7.3). */
      asm_statement();
      break;
    case tok_try:
      /* C++ try block. */
      try_block_statement();
      break;
    case tok_case:
      /* Case label (3.6.1). */
      case_label();
      prev_was_label = TRUE;
      get_another_statement = TRUE;
      break;
    case tok_default:
      /* Default label (3.6.1). */
      default_label();
      prev_was_label = TRUE;
      get_another_statement = TRUE;
      break;
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
          sym_error(ec_already_defined,
                    (a_symbol_ptr)label->source_corresp.assoc_info);
          set_reachable(curr_reachability);
        } else {
          /* The label has not previously been declared, so put out the
             definition. */
          define_label(label);
          if (C_dialect == C_dialect_cplusplus) {
            /* If there have been forward gotos referencing this label, check
               whether any have jumped over initializing declarations. */
            check_jump_over_initialization(label->variant.exec_stmt);
          }  /* if */
        }  /* if */
#if CHECKING
        if (curr_token != tok_colon) {
          internal_error("statement: expected colon");
        }  /* if */
#endif /* CHECKING */
        (void)get_token();
        prev_was_label = TRUE;
        get_another_statement = TRUE;
        break;
      }  /* if */
      /* Other cases are expression statements. */
      goto expr_statement;
    default:
expr_statement:
      /* First look for things that can't be expression statements, and
         produce a specific "Expected a statement" message for those cases. */
      if (curr_token == tok_rbrace || curr_token == tok_else) {
        if (prev_was_label && curr_token == tok_rbrace) {
          /* When a label definition precedes a "}", let it by as an
             extension, with a warning (at least) in all modes. */
          if (strict_ansi_mode) {
            diagnostic(strict_ansi_error_severity, ec_exp_statement);
          } else {
            warning(ec_exp_statement);
          }  /* if */
        } else {
          add_stop_token(tok_semicolon);
          syntax_error(ec_exp_statement);
          remove_stop_token(tok_semicolon);
        }  /* if */
      } else if (C_dialect == C_dialect_cplusplus &&
                 is_decl_not_expr(/*abstract_declarator_allowed=*/FALSE,
                                  /*real_declarator_allowed=*/TRUE)) {
        /* Scan a declaration (C++ only). */
        is_declaration = TRUE;
        local_declaration();
      } else {
        /* expression-statement (3.6.3). */
        add_stop_token(tok_semicolon);
        check_for_unreachable_code();
        expression_statement();
        (void)required_token(tok_semicolon, ec_exp_semicolon);
        remove_stop_token(tok_semicolon);
      }  /* if */
      break;
  }  /* switch */
  /* Loop if we just got a label and not an actual statement. */
  if (get_another_statement) goto rescan_statement;

  db_exit();
  return !is_declaration;
}  /* statement */


a_statement_ptr compound_statement(a_boolean  at_function_level,
                                   a_boolean  explicit_return_type,
                                   a_boolean  is_catch_clause)
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
  a_boolean       any_statements = FALSE;
  unsigned char   old_else_stop_token_value;

  db_enter (3, "compound_statement");

  /* Allocate the statement block. */
  if (at_function_level) {
    /* Block for a function. */
    set_reachable(curr_reachability);
    lint_notreached_flag = FALSE;
    block = alloc_statement((a_statement_kind)stmk_block);
    set_stmt_source_position(block->position, pos_curr_token);
    /* Clear statement stack just to be careful. */
    depth_stmt_stack = -1;
    /* Push an entry on the structured statement stack. */
    push_stmt_stack(ssk_compound, block);
    /* Record in the statement stack entry whether the routine was declared
       with an explicit return type. */
    if (explicit_return_type) {
      struct_stmt_stack->rout_type_explicitly_specified = TRUE;
    }  /* if */
  } else if (is_catch_clause) {
    block = alloc_statement((a_statement_kind)stmk_block);
    /* Push an entry on the structured statement stack. */
    push_stmt_stack(ssk_compound, block);
  } else {
    /* Block nested within a function.  Link it onto the current statement
       sequence.  Check for unreachable code. */
    if (struct_stmt_stack[depth_stmt_stack].kind == ssk_switch) {
      /* The body statement of a switch is not considered dead code. */
    } else {
      check_for_unreachable_code();
    }  /* if */
    start_block_statement(&block, /*dependent_statement=*/FALSE);
    /* Clear the entry for "else" in the stop tokens set.  Without this,
       an else encountered where a statement is expected could cause an
       error recovery loop. */
    old_else_stop_token_value = stop_token_array[(int)tok_else];
    stop_token_array[(int)tok_else] = 0;
  }  /* if */
  /* Skip over the opening brace.  Note that this is NOT an internal error
     check; when a compound statement is the body of a function, it's
     required. */
  add_stop_token(tok_rbrace);
  (void)required_token(tok_lbrace, ec_exp_lbrace);

  /* Scan the sequence of statements. */
  while (curr_token != tok_rbrace && curr_token != tok_end_of_source) {
    if (C_dialect == C_dialect_cplusplus) {
      /* In C++ mode, where declarations can be interspersed with executable
         statements, statement() handles declarations, too. */
      (void)statement();
    } else {
      /* In C mode the declarations are expected to appear first. */
      if (is_decl_start(/*expr_context=*/TRUE,
                        /*real_declarator_allowed=*/TRUE)) {
        /* Scan any declarations.  In C, these must all be at the beginning
           of the block. */
        if (any_statements) {
          error(ec_declaration_after_statements);
          /* Special error-recovery trick: this tries to deal with mismatched
             braces, in the case where a "}" is missing and thus there appears
             to be an extra "{".  If we are at function level, and the next
             thing appears to be a declaration rather than a statement,
             and it's not indented, assume a "}" and exit the compound
             statement. */
          if (at_function_level && pos_curr_token.column == 1) break;
        }  /* if */
        local_declaration();
      } else {
        /* Scan a statement. */
        any_statements = TRUE;
        (void)statement();
      }  /* if */
    }  /* if */
  }  /* while */

  /* If a lint-style "notreached" comment was detected, suppress the
     warning on unreachable code. */
  check_lint_notreached_flag();
  if (at_function_level) {
    /* Function. */
    /* If the code at the end of a function runs off the end, a default
       return must be added.  See 3.6.6.4. */
    /* Unless we're already in dead code, check that a void return
       (one returning no value) is compatible with the current function
       (i.e., the current function should also have type void), and add
       a return with no expression. */
    if (at_function_level && curr_reachability.reachable) {
      a_statement_ptr sp;

      /* Suppress the warning if the user told us this code is not
         reachable. */
      if (curr_reachability.reachable_considering_hints) {
        check_void_return_okay();
      }  /* if */
      sp = add_statement((a_statement_kind)stmk_return);
      if (current_routine_entry()->special_kind ==
                             (a_special_function_kind)sfk_constructor) {
        /* By default constructors return the "this" variable. */
        sp->expr = this_param_value_expr();
      }  /* if */
    }  /* if */
    /* Pop the statement stack. */
    pop_stmt_stack();
    /* Clear statement stack just to be careful. */
    depth_stmt_stack = -1;
  } else if (is_catch_clause) {
    pop_stmt_stack();
  } else {
    /* Block/compound statement rather than function. */
    finish_block_statement(block);
    /* Restore the entry for "else" in the stop tokens set (see comment
       above). */
    stop_token_array[(int)tok_else] = old_else_stop_token_value;
  }  /* if */

  /* Remember the sequence number of the current token, which is expected
     to be the closing brace. */
  set_stmt_source_position(block->variant.block.extra_info->final_position,
                           pos_curr_token);
  /* Check for the closing "}".  Note that for a function, the "}" is left
     for the caller (function_definition) to handle. */
  if (!at_function_level) (void)required_token(tok_rbrace, ec_exp_rbrace);
  remove_stop_token(tok_rbrace);

  db_exit();
  return block;
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
  return curr_reachability.reachable;
}  /* curr_code_reachable */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
