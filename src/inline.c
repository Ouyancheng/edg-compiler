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

inline.c -- Minimal inlining for IL lowering.

Does inlining of calls to inline functions, doing the transformations
on code in IL tree form as part of IL lowering.  Intended to be "minimal"
and mostly for use with the C-generating back end.  Intended to do
inlining at about the same level as cfront.  Since it runs as part of
IL lowering, does not do inlining of C code.
*/

#include "basic_hdrs.h"
#if DO_IL_LOWERING
/* Header files common to all files. */
#include "fe_common.h"
/* Header files used by files involved in IL lowering. */
#include "lower_hdrs.h"
#endif /* DO_IL_LOWERING */

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Only include this code if it is needed: */
#if DO_IL_LOWERING
#if MINIMAL_INLINING

#if !DO_FULL_PORTABLE_EH_LOWERING
 #error -- Inlining requires full portable lowering of excection handling.
#endif /* !DO_FULL_PORTABLE_EH_LOWERING */


static a_scope_ptr
		routine_scope_being_inlined;
			/* When non-NULL, a call of the routine associated
			   with this scope is being expanded as an inline. */


static a_variable_remapping_for_inlining_ptr
                      alloc_variable_remapping_for_inlining(a_variable_ptr var)
/*
Allocate and initialize an entry used to record a variable remapping in effect
during inlining of a function call.  var is the variable that will be remapped.
The entry is placed on the variable_remappings_for_inlining global list.
*/
{
  a_variable_remapping_for_inlining_ptr vrip;

  if (avail_variable_remappings_for_inlining != NULL) {
    /* Reuse an entry previously allocated and freed. */
    vrip = avail_variable_remappings_for_inlining;
    avail_variable_remappings_for_inlining = vrip->next;
  } else {
    /* Allocate a new entry. */
    vrip = (a_variable_remapping_for_inlining_ptr)
                           alloc_fe(sizeof(a_variable_remapping_for_inlining));
#if DEBUG
    num_variable_remappings_for_inlining++;
#endif /* DEBUG */
  }  /* if */
  vrip->next = variable_remappings_for_inlining;
  variable_remappings_for_inlining = vrip;
  vrip->orig_variable = var;
  vrip->is_constant = FALSE;
  vrip->variant.variable = NULL;
  return vrip;
}  /* alloc_variable_remapping_for_inlining */


static void free_variable_remapping_for_inlining(
                                    a_variable_remapping_for_inlining_ptr vrip)
/*
Free a variable remapping entry by placing it on the available list.
*/
{
  vrip->next = avail_variable_remappings_for_inlining;
  avail_variable_remappings_for_inlining = vrip;
}  /* free_variable_remapping_for_inlining */


static void set_up_variable_remapping_for_inlining(
                                           a_scope_ptr        scope,
                                           an_expr_node_ptr   arg_expr_list,
                                           an_insert_location *insert_location)
/*
We are beginning an attempt to inline a call of the routine whose scope
is "scope" with the (already lowered) arguments arg_expr_list.  Generate
temporary variables for parameters and local variables and establish a
remapping list to be used when expanding the body of the function.
The code is inserted at *insert_location, and *insert_location is updated.
*/
{
  a_variable_ptr   param_var, var, temp_var;
  an_expr_node_ptr arg;
  a_variable_remapping_for_inlining_ptr
                   vrip;
#if DEBUG
  a_boolean        first = TRUE;
#endif /* DEBUG */

  /* Process the parameters. */
  for (param_var = scope->variant.routine.parameters, arg = arg_expr_list;
       param_var != NULL;
       param_var = param_var->next, arg = arg->next) {
    check_assertion_str(arg != NULL,
                        "set_up_variable_remapping_...: too few args");
    if (!param_var->source_corresp.referenced) {
      /* We don't need the parameter if it's not referenced.  However, if
         the argument has side effects, we need to evaluate it. */
      if (node_has_side_effects(arg, (a_boolean *)NULL)) {
        insert_expr_statement(arg, insert_location);
      }  /* if */
    } else {
      /* The parameter is referenced, so it must be remapped. */
      vrip = alloc_variable_remapping_for_inlining(param_var);
      if (!param_var->param_value_has_been_changed &&
          !param_var->address_taken &&
          is_constant_node(arg)) {
        /* The argument is constant, and the parameter doesn't get changed
           or have its address taken.  The constant can be used directly. */
        vrip->is_constant = TRUE;
        vrip->variant.constant = arg->variant.constant;
      } else {
        /* A temporary is needed for the parameter.  It is not put into a
           scope yet because we might fail sometime later on the inlining. */
        temp_var = make_temporary_in_scope(param_var->type,
                                           (a_scope_ptr)NULL,
                                           /*force_static=*/FALSE);
        temp_var->address_taken = param_var->address_taken;
        vrip->variant.variable = temp_var;
        /* Initialize the variable to the argument value. */
        (void)insert_var_assignment_statement(temp_var,
                                              (an_expr_operator_kind)eok_last,
                                              arg, insert_location);
      }  /* if */
#if DEBUG
      if (debug_level >= 4) {
        if (first) {
          fprintf(f_debug, "Parameter remappings established:\n");
          first = FALSE;
        }  /* if */
        db_variable(vrip->orig_variable);
        fprintf(f_debug, " --> ");
        if (vrip->is_constant) {
          db_constant(vrip->variant.constant);
        } else {
          db_name(&vrip->variant.variable->source_corresp);
        }  /* if */
        fprintf(f_debug, "\n");
      }  /* if */
#endif /* DEBUG */
    }  /* if */
  }  /* for */
  /* Process the local variables. */
#if DEBUG
  first = TRUE;
#endif /* DEBUG */
  for (var = scope->nonstatic_variables;
       var != NULL;
       var = var->next) {
    /* We don't need the variable if it's not referenced. */
    if (var->source_corresp.referenced) {
      vrip = alloc_variable_remapping_for_inlining(var);
      temp_var = make_temporary_in_scope(var->type,
                                         (a_scope_ptr)NULL,
                                         /*force_static=*/FALSE);
      vrip->variant.variable = temp_var;
      temp_var->address_taken = var->address_taken;
#if DEBUG
      if (debug_level >= 4) {
        if (first) {
          fprintf(f_debug, "Variable remappings established:\n");
          first = FALSE;
        }  /* if */
        db_variable(vrip->orig_variable);
        fprintf(f_debug, " --> ");
        db_name(&vrip->variant.variable->source_corresp);
        fprintf(f_debug, "\n");
      }  /* if */
#endif /* DEBUG */
    }  /* if */        
  }  /* for */
}  /* set_up_variable_remapping_for_inlining */


static void finish_variable_remapping_for_inlining(void)
/*
We have gotten to the end of the inlining of a function call, and
successfully.  Put the temporary variables previously created into the
calling context scope.
*/
{
  a_variable_remapping_for_inlining_ptr vrip, vrip_next;

  /* Look at each remapping established. */
  for (vrip = variable_remappings_for_inlining;
       vrip != NULL;
       vrip = vrip_next) {
    vrip_next = vrip->next;
    if (!vrip->is_constant) {
      /* A temporary.  Add it to the current scope. */
      add_temporary_to_scope(vrip->variant.variable, curr_context->scope);
    }  /* if */
    /* Free the entry. */
    free_variable_remapping_for_inlining(vrip);
  }  /* for */
  variable_remappings_for_inlining = NULL;
}  /* finish_variable_remapping_for_inlining */


a_boolean get_var_remapping_for_inlining(a_variable_ptr var,
                                         a_boolean      *is_constant,
                                         a_constant_ptr *con,
                                         a_variable_ptr *new_var)
/*
See if the variable var is remapped in the current inlining operation.
If so, return TRUE and set *is_constant == FALSE and *new_var to the
new variable if the variable is remapped to a variable, or
*is_constant == TRUE and *con to the constant if the variable is
remapped to a constant.  If the variable is not remapped, return FALSE
and set *new_var to the original variable.
*/
{
  a_boolean                             is_remapped = FALSE;
  a_variable_remapping_for_inlining_ptr vrip;

  *is_constant = FALSE;
  *con = NULL;
  *new_var = NULL;
  /* Look at each variable remapping on the list. */
  for (vrip = variable_remappings_for_inlining;
       vrip != NULL;
       vrip = vrip->next) {
    if (vrip->orig_variable == var) {
      /* Found the remapping. */
      is_remapped = TRUE;
      *is_constant = vrip->is_constant;
      if (*is_constant) {
        *con = vrip->variant.constant;
      } else {
        *new_var = vrip->variant.variable;
      }  /* if */
      break;
    }  /* if */
  }  /* for */
  if (!is_remapped) *new_var = var;
  return is_remapped;
}  /* get_var_remapping_for_inlining */


a_variable_ptr remap_var_for_inlining(a_variable_ptr var)
/*
See if the indicated variable is remapped in the current inlining operation.
Return the new variable if there is a remapping, or the original variable
if not.  An internal error is generated if the remapping is to a constant.
*/
{
  a_boolean      is_constant;
  a_constant_ptr con;
  a_variable_ptr new_var;

  if (get_var_remapping_for_inlining(var, &is_constant, &con, &new_var)) {
    check_assertion_str(!is_constant, "remap_var_for_inlining: is_constant");
  }  /* if */
  return new_var;
}  /* remap_var_for_inlining */


static a_statement_ptr copy_inlined_statement(
                                           a_statement_ptr    statement,
                                           an_insert_location *insert_location)
/*
Make a copy of the indicated statement, insert it at *insert_location, and
return a pointer to the copy.
*/
{
  a_statement_ptr new_statement = alloc_statement(statement->kind);

  copy_statement(statement, new_statement);
  insert_statement(new_statement, insert_location);
  return new_statement;
}  /* copy_inlined_statement */


static void expand_statement_inline(a_statement_ptr    statement,
                                    an_insert_location *insert_location,
                                    a_boolean          *inlinable,
                                    a_boolean          *failed)
/*
Generate a copy of the indicated statement as part of the expansion of an
inline function call.  Insert the copy at *insert_location and update
*insert_location accordingly.  If the expansion cannot be done for some
reason, set *failed TRUE.  If it is noted that the expansion can never
be done, in any context, also set *inlinable FALSE.  If statement is NULL,
do nothing.  Note that the insert location may be an expression insert
location; in that case, only expressions can be inserted, so other kinds
of statements can be inserted only if they can be turned into expressions.
If not, *failed is set.
*/
{
  a_source_position  saved_error_position, saved_code_pos;
  an_expr_node_ptr   stmt_expr, expr;
  a_statement_ptr    new_statement, stmt, prev_stmt;
  a_label_ptr        label;
  an_insert_location sub_insert_location;

  if (statement == NULL) {
    /* No statement to copy. */
  } else if (statement->has_associated_pragma) {
    /* Can't inline a statement with an associated pragma. */
  } else {
    /* Track the source position. */
    saved_code_pos = code_pos_for_lowering;
    set_position_from_stmt_source_position(code_pos_for_lowering,
                                           statement->position);
    saved_error_position = error_position;
    error_position = code_pos_for_lowering;
    stmt_expr = statement->expr;
    if (stmt_expr != NULL) stmt_expr = copy_expr_tree(stmt_expr);
    switch (statement->kind) {
      case stmk_expr:
        set_expr_result_not_used(stmt_expr);
        if (is_expr_insert_location(insert_location)) {
          /* An expression statement is copied as an expression. */
          insert_expr(stmt_expr, insert_location);
        } else {
          /* An expression statement is copied as an expression statement. */
          new_statement = copy_inlined_statement(statement, insert_location);
          new_statement->expr = stmt_expr;
        }  /* if */
        break;
      case stmk_goto:
        /* Gotos in general cannot be inlined, but look for the case
             { ... goto L; } L:
           at the top level, which comes up in destructor epilogues. */
        label = statement->variant.label.ptr;
        stmt = routine_scope_being_inlined->assoc_block->
                                                      variant.block.statements;
        /* See if the label is a top-level label. */
        prev_stmt = NULL;
        for (; stmt != NULL && stmt != label->variant.exec_stmt;
             prev_stmt = stmt, stmt = stmt->next) {}
        if (stmt != NULL && prev_stmt != NULL) {
          /* The label is a top-level label.  See if the previous statement
             is the goto we're considering, or a block whose last statement is
             the goto we're considering. */
          if (prev_stmt->kind == (a_statement_kind)stmk_block) {
            prev_stmt = last_statement_in_block(prev_stmt);
          }  /* if */
          if (prev_stmt == statement) {
            /* Yes.  This goto can just be deleted. */
            break;
          }  /* if */
        }  /* if */
        goto cannot_inline_ever;
      case stmk_label:
        /* Labels are just removed.  The processing on the gotos, if any,
           will decide whether inlining can be done. */
        break;
      case stmk_return:
        /* Returns are allowed only as the last thing in the routine. */
        /* Find the last statement in the top block. */
        stmt = routine_scope_being_inlined->assoc_block;
        stmt = last_statement_in_block(stmt);
        if (stmt == statement) {
          /* Yes, this return is the last in the top block, so it can be
             inlined. */
          if (is_expr_insert_location(insert_location)) {
            /* Expression insert location. */
            if (stmt_expr != NULL) {
              insert_expr(stmt_expr, insert_location);
            } else {
              if (!is_void_type(f_skip_typerefs(
                             routine_scope_being_inlined->variant.routine.ptr->
                                         type)->variant.routine.return_type)) {
                /* The return statement returns nothing and the function
                   expects a return value, so this function cannot be
                   inlined. */
                goto cannot_inline_ever;
              }  /* if */
            }  /* if */
          } else {
            /* Statement insert location.  A return without an expression
               is just thrown away.  A return with an expression is just
               the expression (for side effects). */
            if (stmt_expr != NULL &&
                node_has_side_effects(stmt_expr, (a_boolean *)NULL)) {
              stmt_expr = add_cast(stmt_expr, void_type());
              (void)insert_expr_statement(stmt_expr, insert_location);
            }  /* if */
          }  /* if */
          break;
        }  /* if */
        goto cannot_inline_ever;
      case stmk_if:
        if (is_expr_insert_location(insert_location)) {
          an_expr_node_ptr expr, then_expr, else_expr;
          /* Expression insert location.  Turn an "if" into a "?" operator. */
          /* Copy the "then" statement. */
          set_expr_creation_insert_location(&sub_insert_location);
          expand_statement_inline(statement->variant.if_stmt.then_statement,
                                  &sub_insert_location, inlinable, failed);
          if (!*failed) {
            then_expr = sub_insert_location.variant.expr;
            then_expr = add_cast_if_necessary(then_expr, void_type());
            if (statement->variant.if_stmt.else_statement != NULL) {
              /* Copy the "else" statement. */
              set_expr_creation_insert_location(&sub_insert_location);
              expand_statement_inline(
                                     statement->variant.if_stmt.else_statement,
                                     &sub_insert_location, inlinable, failed);
              if (!*failed) {
                else_expr = sub_insert_location.variant.expr;
                else_expr = add_cast_if_necessary(else_expr, void_type());
              }  /* if */
            } else {
              /* No "else" statement; use (void)0. */
              else_expr = add_cast(
                            node_for_integer_constant(0L,
                                                      (an_integer_kind)ik_int),
                                                      void_type());
            }  /* if */
          }  /* if */
          if (!*failed) {
            stmt_expr->next = then_expr;
            then_expr->next = else_expr;
            /* Make the "?" operator. */
            expr = make_operator_node((an_expr_operator_kind)eok_question,
                                      void_type(),
                                      stmt_expr);
            set_expr_result_not_used(expr);
            insert_expr(expr, insert_location);
          }  /* if */
        } else {
          /* Statement insert location.  Copy and insert the "if" statement. */
          new_statement = copy_inlined_statement(statement, insert_location);
          new_statement->expr = stmt_expr;
          /* Copy the "then" statement. */
          set_statement_creation_insert_location(&sub_insert_location);
          expand_statement_inline(statement->variant.if_stmt.then_statement,
                                  &sub_insert_location, inlinable, failed);
          new_statement->variant.if_stmt.then_statement =
                                              sub_insert_location.variant.stmt;
          /* Copy the "else" statement. */
          set_statement_creation_insert_location(&sub_insert_location);
          expand_statement_inline(statement->variant.if_stmt.else_statement,
                                  &sub_insert_location, inlinable, failed);
          new_statement->variant.if_stmt.else_statement =
                                              sub_insert_location.variant.stmt;
        }  /* if */
        break;
      case stmk_block:
        if (is_expr_insert_location(insert_location)) {
          /* An expression will be created for the statements in the block.
             It will be unattached as it is built, then attached below. */
          set_expr_creation_insert_location(&sub_insert_location);
        } else {
          /* Make and insert a new block statement.  This doesn't use
             copy_statement because that doesn't clone the block
             supplement. */
          new_statement = alloc_statement((a_statement_kind)stmk_block);
          new_statement->position = statement->position;
          insert_statement(new_statement, insert_location);
          /* Copies of the statements in the block will be inserted under the
             copy of the block statement. */
          set_block_start_insert_location(new_statement, &sub_insert_location);
        }  /* if */
        /* Copy the statements inside the block. */
        for (stmt = statement->variant.block.statements;
             stmt != NULL;
             stmt = stmt->next) {
          expand_statement_inline(stmt, &sub_insert_location,
                                  inlinable, failed);
          if (*failed) break;
        }  /* for */
        if (!*failed && is_expr_insert_location(insert_location)) {
          /* Insert the expression for the whole block at the proper
             location. */
          expr = sub_insert_location.variant.expr;
          if (expr == NULL) {
            /* The block contained no statements, so use a (void)0 for the
               while block. */
            expr = add_cast(node_for_integer_constant(0L,
                                                      (an_integer_kind)ik_int),
                            void_type());
          }  /* if */
          insert_expr(expr, insert_location);
        }  /* if */
        break;
      case stmk_init:
        { a_dynamic_init_ptr dip = statement->variant.dynamic_init;
          an_expr_node_ptr   var_expr, init_expr;
          a_variable_ptr     var = remap_var_for_inlining(dip->variable);
          /* A dynamic initialization is rendered as an assignment.  This is
             done even in statement insert mode, to avoid the complexity
             of copying and inserting the stmk_init.  The only negative to
             that is that it doesn't allow inlining aggregate
             initializations in statement insert mode. */
          if (dip->kind == (a_dynamic_init_kind)dik_constant) {
            /* Aggregates can't be handled. */
            if (dip->variant.constant->kind ==
                                          (a_constant_repr_kind)ck_aggregate) {
              goto cannot_inline_ever;
            }  /* if */
            /* Non-aggregate constant initial value. */
            /* This uses copy_unshared constant because that routine does
               variable remapping if necessary. */
            init_expr = alloc_node_for_constant(
                                copy_unshared_constant(dip->variant.constant));
          } else if (dip->kind == (a_dynamic_init_kind)dik_expression) {
            init_expr = copy_expr_tree(dip->variant.expression);
          } else {
            /* Other cases cannot be handled (they probably can't happen here,
               but for the sake of safety...). */
            goto cannot_inline_ever;
          }  /* if */
          /* Assign the initial value to the variable. */
          var_expr = var_lvalue_expr(var);
          var_expr->next = init_expr;
          expr = make_operator_node(lowered_assignment_operator(var->type),
                                    f_skip_typerefs(var->type),
                                    var_expr);
          set_expr_result_not_used(expr);
          (void)insert_expr_statement(expr, insert_location);
        }
        break;
#if GENERATE_SOURCE_SEQUENCE_LISTS
      case stmk_decl:
        /* Statement that marks the location of declarations.  Ignored here. */
        break;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      case stmk_while:
      case stmk_end_test_while:
      case stmk_for:
      case stmk_asm:
      case stmk_switch:
      case stmk_try_block:
      default:
cannot_inline_ever:
        /* This statement cannot be inlined in any context. */
        *failed = TRUE;
        *inlinable = FALSE;
        break;
    }  /* switch */
    error_position = saved_error_position;
    code_pos_for_lowering = saved_code_pos;
  }  /* if */
}  /* expand_statement_inline */


void do_inlining_of_call(an_expr_node_ptr expr,
                         a_statement_ptr  statement)
/*
expr is a lowered eok_call expression.  If the routine called is an inline
function, do inlining on the expression.  If statement is non-NULL, the
call is the top node of the indicated statement (which is an expression
statement).
*/
{
  an_expr_node_ptr arg;
  a_routine_ptr    routine = NULL;
  a_statement_ptr  block_stmt;

  db_enter(4, "do_inlining_of_call");
  check_assertion(is_operation_node(expr) &&
                  expr->variant.operation.kind ==
                                              (an_expr_operator_kind)eok_call);
  arg = expr->variant.operation.operands;
  if (is_routine_address_node(arg)) {
    /* We know which routine is being called. */
    routine = arg->variant.routine;
    if (routine->is_inline) {
      if (!routine->inlinable) {
        /* The routine cannot be inlined, so leave this call alone.  An
           out-of-line copy of the routine will be required.  Note that this
           comes up in particular for calls of inline functions prior
           to their definitions. */
        routine->need_out_of_line_copy = TRUE;
      } else {
        a_boolean          failed = FALSE, inlinable = TRUE;
        an_insert_location insert_location;
        a_scope_ptr        scope;
        /* The routine can be inlined.  Turn off the inlinable flag for the
           duration of the inlining to avoid problems with recursion. */
        routine->inlinable = FALSE;
#if DEBUG
        if (debug_level >= 4) {
          fprintf(f_debug, "Beginning inlining of call to ");
          db_name(&routine->source_corresp);
          fprintf(f_debug, ":\n");
        }  /* if */
#endif /* DEBUG */
        scope = il_header.region_scope_entry[routine->assoc_scope];
        routine_scope_being_inlined = scope;
        /* Set the insert location.  Use a location unattached to the IL
           tree, because we may discover we can't inline the function.
           If the inlining works, we can insert the statement or expression
           into the tree.  If it doesn't, we discard the statement or
           expression and mark the routine so we will not attempt inlining
           in the future. */
        if (statement != NULL) {
          /* Build the code inside an extra block (even though the top
             statement of the function will be a block) because assignments
             to initialize parameter temporaries may be inserted. */
          block_stmt = alloc_statement((a_statement_kind)stmk_block);
          set_block_start_insert_location(block_stmt, &insert_location);
        } else {
          set_expr_creation_insert_location(&insert_location);
        }  /* if */
        check_assertion_str(variable_remappings_for_inlining == NULL,
                           "do_inlining_of_call: remappings list is non-NULL");
        /* Create new variables for parameters and local variables. */
        arg = arg->next;  /* Advance to first argument. */
        set_up_variable_remapping_for_inlining(scope, arg, &insert_location);
        /* Copy the code of the function, replacing references to the
           parameters and variables. */
        expand_statement_inline(scope->assoc_block, &insert_location,
                                &inlinable, &failed);
        if (failed) {
          /* Inlining failed, so we will need an out-of-line copy of the
             routine.  The statement or expression created above is just
             discarded. */
          routine->need_out_of_line_copy = TRUE;
        } else {
          /* Inlining was successful. */
          /* Now that inlining is known to have succeeded, add the temporary
             variables to the current scope. */
          finish_variable_remapping_for_inlining();
          if (statement != NULL) {
            /* Replace the original call statement by overwriting it with
               the block statement containing the inlined code. */
            copy_statement(block_stmt, statement);
          } else {
            /* Replace the original call node by overwriting it with the
               expression for the inlined call. */
            check_assertion(insert_location.variant.expr != NULL);
            overwrite_node(expr, insert_location.variant.expr);
          }  /* if */
        }  /* if */
        /* Put the inlinable flag back on, unless we've discovered that this
           function can never be inlined. */
        routine->inlinable = inlinable;
        routine_scope_being_inlined = NULL;
#if DEBUG
        if (debug_level >= 4) {
          fprintf(f_debug, "End of inlining of call to ");
          db_name(&routine->source_corresp);
          fprintf(f_debug, "%s\n", failed ? " (failed)" : "");
        }  /* if */
#endif /* DEBUG */
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
}  /* do_inlining_of_call */


void set_up_routine_for_inlining(a_scope_ptr scope)
/*
The routine associated with the indicated scope is an inline routine and
inlining is enabled.  The body of the routine has been lowered.  Set up
the routine so it can be inlined on calls from here on.
*/
{
  a_routine_ptr routine = scope->variant.routine.ptr;
  a_type_ptr    routine_type = skip_typerefs(routine->type);
  a_routine_type_supplement_ptr
                rtsp = routine_type->variant.routine.extra_info;

  /* Rule out certain cases up front. */
  if (scope->variables != NULL) {
    /* Function has local static variables. */
  } else if (scope->constants != NULL) {
    /* Function has local constants. */
  } else if (scope->types != NULL) {
    /* Function has local types. */
  } else if (scope->scopes != NULL) {
    /* Function has block scopes. */
  } else if (scope->pragmas != NULL) {
    /* Function has pragmas. */
  } else if (rtsp->has_ellipsis) {
    /* Function has a variable number of arguments. */
#if ASM_FUNCTION_ALLOWED
  } else if (routine->storage_class == (a_storage_class)sc_asm) {
    /* The function is an asm function. */
#endif /* ASM_FUNCTION_ALLOWED */
  } else {
    /* The routine looks like it can be inlined. */
    routine->inlinable = TRUE;
  }  /* if */
}  /* set_up_routine_for_inlining */


void mark_inlined_routines_as_unreferenced(void)
/*
For any inline routines for which all calls were expanded inline, mark the
routines as being unreferenced.  This avoids lots of copies of the static
versions of those routines.
*/
{
  a_routine_ptr routine;

  for (routine = il_header.primary_scope->routines;
       routine != NULL;
       routine = routine->next) {
    if (routine->is_inline && routine->inlinable &&
        !routine->need_out_of_line_copy) {
      routine->source_corresp.referenced = FALSE;
    }  /* if */
  }  /* for */
}  /* mark_inlined_routines_as_unreferenced */


void inline_one_time_init(void)
/*
Do one-time initialization of static variables declared in inline.c.
(Variables that need to be reinitialized with each new translation unit
are handled in inline_init.)
*/
{
  /* Save variables from inline.h and inline.c that are needed for
     precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(avail_variable_remappings_for_inlining),
#if DEBUG
      pch_saved_var_array_elem(num_variable_remappings_for_inlining),
#endif /* DEBUG */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
}  /* inline_one_time_init */


void inline_init(void)
/*
Initialize static variables related to this file.  This is done as a
subroutine (rather than relying on static initialization) so that it
can be redone to compile more than one source file in a single invocation
of the front end.
*/
{
  /* Variables in inline.h: */
  avail_variable_remappings_for_inlining = NULL;
  variable_remappings_for_inlining = NULL;
#if DEBUG
  num_variable_remappings_for_inlining = 0;
#endif /* DEBUG */
  /* Static variables in inline.c: */
  routine_scope_being_inlined = NULL;
}  /* inline_init */

#endif /* MINIMAL_INLINING */
#endif /* DO_IL_LOWERING */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1995 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
