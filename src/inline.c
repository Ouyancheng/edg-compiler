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

#include "folding.h"

#if !DO_FULL_PORTABLE_EH_LOWERING
 #error -- Inlining requires full portable lowering of exception handling.
#endif /* !DO_FULL_PORTABLE_EH_LOWERING */


static a_variable_remapping_for_inlining_ptr
		variable_remappings_for_inlining;
			/* List of remappings of variables to be done while
			   copying the body of a function being inlined. */


static a_scope_ptr
		routine_scope_being_inlined;
			/* When non-NULL, a call of the routine associated
			   with this scope is being expanded as an inline. */


/*
Interface macro to copy_expr_tree.
*/
#define copy_expr_tree_for_inlining(expr) \
  copy_expr_tree((expr), CE_DOING_INLINING_OF_FUNCTION_CALL)


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
  vrip->kind = vrk_none;
  vrip->arg_expr = NULL;
  vrip->arg_expr_next = NULL;
  vrip->orig_temporary = NULL;
  vrip->temporary_used = FALSE;
  vrip->remapping_used = FALSE;
  vrip->local_temporary_okay = FALSE;
  vrip->local_temporary_reused = FALSE;
  return vrip;
}  /* alloc_variable_remapping_for_inlining */


static void free_variable_remappings_for_inlining(void)
/*
Free the variable remapping entries on the global list by returning them to
the available list.
*/
{
  a_variable_remapping_for_inlining_ptr vrip, vrip_next;

  for (vrip = variable_remappings_for_inlining;
       vrip != NULL;
       vrip = vrip_next) {
    vrip_next = vrip->next;
    vrip->next = avail_variable_remappings_for_inlining;
    avail_variable_remappings_for_inlining = vrip;
  }  /* for */
  variable_remappings_for_inlining = NULL;
}  /* free_variable_remappings_for_inlining */

#if DEBUG

static void db_variable_remapping(a_variable_remapping_for_inlining_ptr vrip)
/*
Display the indicated variable remapping for debugging purposes.
*/
{
  db_variable(vrip->orig_variable);
  if (vrip->kind == vrk_none) {
    fprintf(f_debug, " (no remapping)");
  } else {
    fprintf(f_debug, " --> ");
    if (vrip->kind == vrk_temporary) {
      db_name(&vrip->variant.variable->source_corresp);
    } else if (vrip->kind == vrk_constant_expr) {
      an_expr_node_ptr expr = vrip->variant.expr;
      if (is_constant_node(expr)) {
        db_constant(vrip->variant.expr->variant.constant);
      } else if (is_variable_address_node(expr)) {
        fprintf(f_debug, "&");
        db_name(&expr->variant.variable->source_corresp);
      } else {
        db_expression(expr);
      }  /* if */
    } else {
      fprintf(f_debug, " <bad remapping>");
    }  /* if */
  }  /* if */
  fprintf(f_debug, "\n");
}  /* db_variable_remapping */

#endif /* DEBUG */

static a_variable_ptr make_remapping_temporary(
                          a_variable_remapping_for_inlining_ptr
                                    vrip,
                          a_boolean expr_temp,
                          a_boolean is_temp_for_constructor_this_inlined_param,
                          a_boolean is_temp_for_unmodified_inlined_param)
/*
Allocate a temporary variable as the remapping for the variable indicated
in the given remapping entry.  expr_temp is true if the temporary has the
normal temporary lifetime, i.e., it will be used within one full expression.
The is_temp_... flags give values for the like-named flags in the temporary
variable.
*/
{
  a_variable_ptr             orig_var = vrip->orig_variable;
  a_variable_ptr             temp_var = NULL;
  a_temporary_list_entry_ptr tlep = NULL;

  /* Try to reuse an existing local temporary.  Don't do that, though,
     if the variable being remapped has special attributes. */
  if (expr_temp && !orig_var->address_taken) {
    vrip->local_temporary_okay = TRUE;
    do {
      temp_var = find_reusable_temporary(orig_var->type, &tlep);
      if (temp_var == NULL) break;
      /* Reject the temporary and look for another if it doesn't have the
         right set of flags. */
    } while (is_temp_for_constructor_this_inlined_param !=
             temp_var->is_temp_for_constructor_this_inlined_param ||
             is_temp_for_unmodified_inlined_param !=
             temp_var->is_temp_for_unmodified_inlined_param);
  }  /* if */
  if (temp_var != NULL) {
    tlep->in_use = TRUE;
    vrip->local_temporary_reused = TRUE;
  } else {
    /* Allocate a new variable for the temporary. */
    temp_var = make_temporary(orig_var->type, /*force_static=*/FALSE);
  }  /* if */
  vrip->kind = vrk_temporary;
  vrip->variant.variable = temp_var;
  if (orig_var->address_taken) temp_var->address_taken = TRUE;
  if (orig_var->initialization_rewritten_as_assignment) {
    temp_var->initialization_rewritten_as_assignment = TRUE;
  }  /* if */
  temp_var->is_temp_for_constructor_this_inlined_param =
                                    is_temp_for_constructor_this_inlined_param;
  temp_var->is_temp_for_unmodified_inlined_param =
                                    is_temp_for_unmodified_inlined_param;
  return temp_var;
}  /* make_remapping_temporary */


static a_boolean is_ptr_to_member_function_constant_expr(an_expr_node_ptr expr)
/*
Return TRUE if the indicated expression is the lowered version of a
pointer to member function constant.
*/
{
  a_boolean is_pmf_con = FALSE;

  if (is_variable_node(expr)) {
    a_variable_ptr var = expr->variant.variable;
    if (!has_name(var) &&
        /* Variables for constants are initialized.  Temporaries are not. */
        var->init_kind == (an_init_kind)initk_static &&
        is_or_was_ptr_to_member_function_type(var->type)) {
      is_pmf_con = TRUE;
    }  /* if */
  }  /* if */
  return is_pmf_con;
}  /* is_ptr_to_member_function_constant_expr */


static a_boolean is_constant_valued_expression(
                                            an_expr_node_ptr expr,
                                            a_boolean        local_vars_change,
                                            a_boolean        other_vars_change,
                                            a_boolean        *is_non_null)
/*
Return TRUE if the indicated expression has an invariant value over the
duration of an inlined call.  That includes things like addresses of
automatic variables.  If the expression is constant valued and the value
is known to be non-null, return *is_non_null TRUE.  If it cannot be determined
whether the constant is non-NULL, the safe value is FALSE.
local_vars_change is TRUE if the values of unaliased local variables
of the caller might change (e.g., if the argument expressions have
side effects).  other_vars_change is TRUE if the values of other variables
might change (e.g., if the argument expressions or the called function
body have side effects).
*/
{
  a_boolean is_constant_valued = FALSE;

  *is_non_null = FALSE;
  if (is_constant_node(expr)) {
    a_constant_ptr con = expr->variant.constant;
    is_constant_valued = TRUE;
    /* Don't treat string literals as constant, because if we generate C code
       and refer to the constant several times, the address of the string
       literal will be different on each reference. */
    if (con->kind == (a_constant_repr_kind)ck_address &&
        con->variant.address.kind == (an_address_base_kind)abk_constant &&
        con->variant.address.variant.constant->kind ==
                                             (a_constant_repr_kind)ck_string) {
      is_constant_valued = FALSE;
    }  /* if */
    *is_non_null = constant_bool_value_known_at_compile_time(con) &&
                   !is_false_constant(con);
  } else if (is_ptr_to_member_function_constant_expr(expr)) {
    /* A pointer to member function constant is constant. */
    is_constant_valued = TRUE;
  } else if (is_variable_address_node(expr)) {
    is_constant_valued = TRUE;
    /* We assume that variables other than extern variables have non-null
       addresses.  extern variables might have zero addresses because of
       linker magic like weak externals. */
    *is_non_null = (expr->variant.variable->storage_class !=
                    (a_storage_class)sc_extern);
  } else if (is_variable_node(expr)) {
    a_variable_ptr var = expr->variant.variable;
    if ((var->is_parameter && !var->param_value_has_been_changed) ||
        var->is_this_parameter) {
      /* Unassigned parameters are constant-valued within a function.
         The "this" parameter can be considered constant even when
         it is assigned in the allocation section of a constructor.
         It can't be considered constant if there is an assignment to
         "this", but routines with such an assignment are considered
         to be not inlinable. */
      is_constant_valued = TRUE;
    } else if (!local_vars_change &&
               var->source_corresp.is_local_to_function &&
               !var->address_taken) {
      /* This is a local variable, and local variables are invariant
         over the lifetime of the call. */
      is_constant_valued = TRUE;
    } else if (!other_vars_change) {
      /* The values of all variables are invariant over the lifetime
         of the call. */
      is_constant_valued = TRUE;
    }  /* if */
    if (is_constant_valued && var->is_this_parameter) *is_non_null = TRUE;
  } else if (is_routine_address_node(expr)) {
    is_constant_valued = TRUE;
    *is_non_null = TRUE;
  } else if (is_operation_node(expr)) {
    an_expr_operator_kind op = expr->variant.operation.kind;
    if (op == (an_expr_operator_kind)eok_field) {
      /* A field selection address is constant-valued if the address upon
         which it is based is constant-valued.  This is important for
         base-class field selections. */
      is_constant_valued =
                is_constant_valued_expression(expr->variant.operation.operands,
                                              local_vars_change,
                                              other_vars_change,
                                              is_non_null);
    } else if (op == (an_expr_operator_kind)eok_cast &&
               is_pointer_type(expr->type)) {
      /* A cast of a constant address is constant-valued.  This is useful on a
         cast of the address of a local variable to adjust its cv-qualification
         when it is passed as the "this" parameter to a constructor or
         destructor. */
      is_constant_valued =
                is_constant_valued_expression(expr->variant.operation.operands,
                                              local_vars_change,
                                              other_vars_change,
                                              is_non_null);
    }  /* if */
  }  /* if */
  return is_constant_valued;
}  /* is_constant_valued_expression */


static a_boolean func_body_has_side_effects(a_routine_ptr routine)
/*
Return TRUE if the indicated routine's body has side effects.  The
safe answer is TRUE.  More specifically, this returns TRUE if the function
body has any side effects that can affect the values of argument expressions.
*/
{
  a_boolean       has_side_effects = TRUE;
  a_scope_ptr     scope = il_header.region_scope_entry[routine->assoc_scope];
  a_statement_ptr stmt = scope->assoc_block;

  /* Only consider functions where the top block contains only a return
     statement. */
  if (stmt->kind == (a_statement_kind)stmk_block) {
    stmt = stmt->variant.block.statements;
    if (stmt != NULL &&
        stmt->kind == (a_statement_kind)stmk_return &&
        stmt->next == NULL) {
      check_assertion(stmt->variant.return_dynamic_init == NULL);
      if (stmt->expr != NULL) {
        an_expr_node_ptr expr = stmt->expr;
        /* If the final act of the function call is to do an assignment
           whose destination expression has no side effects, ignore that
           with respect to the side effects of the function; it happens
           after everything else, and therefore cannot affect the values
           of argument expressions. */
        if (is_operation_node(expr)) {
          an_expr_operator_kind op = expr->variant.operation.kind;
          if (op == (an_expr_operator_kind)eok_iassign ||
              op == (an_expr_operator_kind)eok_passign ||
              op == (an_expr_operator_kind)eok_fassign) {
            an_expr_node_ptr op1 = expr->variant.operation.operands;
            if (!node_has_side_effects(op1, (a_boolean *)NULL)) {
              expr = op1->next;
              /* if */
            }  /* if */
          }  /* if */
        }  /* if */
        has_side_effects = node_has_side_effects(expr,
                                                 (a_boolean *)NULL);
      } else {
        has_side_effects = FALSE;
      }  /* if */
    }  /* if */
  }  /* if */
  return has_side_effects;
}  /* func_body_has_side_effects */


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
  an_expr_node_ptr arg, arg_next;
  a_variable_remapping_for_inlining_ptr
                   vrip;
  a_routine_ptr    routine = scope->variant.routine.ptr;
  a_statement_ptr  stmt;
  a_boolean        expr_insert = is_expr_insert_location(insert_location);
#if DEBUG
  a_boolean        first = TRUE;
#endif /* DEBUG */
  a_boolean        call_has_side_effects, arg_list_has_side_effects;

  /* Determine whether the call has side effects, either because the
     function body has side effects or because the argument expressions
     have side effects. */
  call_has_side_effects = func_body_has_side_effects(routine);
  arg_list_has_side_effects = FALSE;
  for (arg = arg_expr_list; arg != NULL; arg = arg->next) {
    if (node_has_side_effects(arg, (a_boolean *)NULL)) {
      call_has_side_effects = TRUE;
      arg_list_has_side_effects = TRUE;
      break;
    }  /* if */
  }  /* for */
  /* Process the parameters. */
  for (param_var = scope->variant.routine.parameters, arg = arg_expr_list;
       param_var != NULL;
       param_var = param_var->next, arg = arg_next) {
    check_assertion_str(arg != NULL,
                        "set_up_variable_remapping_...: too few args");
    /* Detach the argument expression from the rest of the list so it can
       be used by itself.  Put a pointer to the argument expression, and
       the original "next" value, in the remap entry, so that the "next"
       pointer can be restored if the inlining cannot be done.  Note that
       if no remapping is required on the parameter, the remap entry is
       still needed to preserve the information needed for the relinking
       on failure. */
    arg_next = arg->next;
    arg->next = NULL;
    vrip = alloc_variable_remapping_for_inlining(param_var);
    vrip->arg_expr = arg;
    vrip->arg_expr_next = arg_next;
    if (!param_var->source_corresp.referenced) {
      /* We don't need the parameter if it's not referenced.  However, if
         the argument has side effects, we need to evaluate it. */
      if (node_has_side_effects(arg, (a_boolean *)NULL)) {
        stmt = insert_expr_statement(arg, insert_location);
        set_stmt_pos_to_code_pos_for_lowering(stmt);
      }  /* if */
    } else {
      /* The parameter is referenced, so it has to be remapped. */
      /* See if the argument value is constant (meaning, in this case,
         invariant over the lifetime of the call). */
      a_boolean is_non_null;
      a_boolean arg_is_constant = is_constant_valued_expression(
                                                     arg,
                                                     arg_list_has_side_effects,
                                                     call_has_side_effects,
                                                     &is_non_null);
      /* See if the parameter is modified. */
      a_boolean param_is_unmodified = FALSE;
      a_boolean param_is_constructor_this = FALSE;
      if (!param_var->param_value_has_been_changed) {
        /* The parameter doesn't get changed or have its address taken. */
        param_is_unmodified = TRUE;
      } else if (param_var->is_this_parameter &&
                 routine->special_kind ==
                                       (a_special_function_kind)sfk_constructor
#if ASSIGNMENT_TO_THIS_ALLOWED
                 && !routine->assignment_to_this_done
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
                                                     ) {
        /* For the "this" parameter of a constructor, take advantage of
           the fact that we know how it works, so we can eliminate the
           allocation code at the top of the routine even though there's
           an assignment to "this" in that code when it does the allocation.
           We can only do that if the value being assigned to "this" is
           non-null. */
        param_is_constructor_this = TRUE;
        if (is_non_null) param_is_unmodified = TRUE;
      }  /* if */
      if (!arg_is_constant && !call_has_side_effects) {
        /* The call has no side effects (and therefore this argument
           expression has no side effects), so any expression is invariant
           over the call and can be passed without a temporary, at the
           expense of extra code.  Do it if the parameter is used only once. */
        if (!param_var->param_used_more_than_once &&
            /* The param_used_more_than_once flag is not maintained for
               parameters added by IL lowering. */
            has_name(param_var)) {
          arg_is_constant = TRUE;
        }  /* if */
      }  /* if */
      if (param_is_unmodified && arg_is_constant &&
          /* We don't have the mechanism to handle class-valued
             variables, because their addresses can get taken implicitly
             when field selections are done.  We would have to support
             remapping an enk_variable_address to some expression, which
             we don't do currently.  Avoid that case. */
          (!is_class_struct_union_type(param_var->type) ||
           is_ptr_to_member_function_constant_expr(arg))) {
        /* The argument is constant-valued and the parameter is unmodified.
           The parameter gets remapped to a constant-valued expression. */
        vrip->kind = vrk_constant_expr;
        vrip->variant.expr = arg;
      } else {
        /* A temporary is needed for the parameter. */
        temp_var = make_remapping_temporary(vrip, expr_insert,
                                            param_is_constructor_this,
                                            param_is_unmodified);
        /* Initialize the variable to the argument value. */
        stmt = insert_var_assignment_statement(temp_var,
                                               (an_expr_operator_kind)eok_last,
                                               arg, insert_location);
        set_stmt_pos_to_code_pos_for_lowering(stmt);
        temp_var->initialization_rewritten_as_assignment = TRUE;
      }  /* if */
#if DEBUG
      if (debug_level >= 4) {
        if (first) {
          fprintf(f_debug, "Parameter remappings established:\n");
          first = FALSE;
        }  /* if */
        db_variable_remapping(vrip);
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
      /* Remap the local variable to a temporary. */
      vrip = alloc_variable_remapping_for_inlining(var);
      temp_var = make_remapping_temporary(
                    vrip, expr_insert,
                    (a_boolean)var->is_temp_for_constructor_this_inlined_param,
                    (a_boolean)var->is_temp_for_unmodified_inlined_param);
#if DEBUG
      if (debug_level >= 4) {
        if (first) {
          fprintf(f_debug, "Variable remappings established:\n");
          first = FALSE;
        }  /* if */
        db_variable_remapping(vrip);
      }  /* if */
#endif /* DEBUG */
    }  /* if */        
  }  /* for */
}  /* set_up_variable_remapping_for_inlining */


static void restore_mapping_to_temporary(
                                    a_variable_remapping_for_inlining_ptr vrip)
/*
If vrip is a remapping that was originally a vrk_temporary and that has been
temporarily remapped to something else, restore the original mapping.
*/
{
  if (vrip->kind != vrk_temporary && vrip->orig_temporary != NULL) {
    vrip->kind = vrk_temporary;
    vrip->variant.variable = vrip->orig_temporary;
    vrip->orig_temporary = NULL;
  }  /* if */
}  /* restore_mapping_to_temporary */


static void finish_variable_remapping_for_inlining(void)
/*
We have gotten to the end of the inlining of a function call, and
successfully.  Put the temporary variables previously created into the
calling context scope.
*/
{
  a_variable_remapping_for_inlining_ptr vrip;

  /* Look at each remapping established. */
  for (vrip = variable_remappings_for_inlining;
       vrip != NULL;
       vrip = vrip->next) {
    if (vrip->temporary_used) {
      /* If a vrk_temporary remapping was temporarily switched to some
         other remapping, but it was used at some point when it was
         a vrk_temporary remapping, switch it back. */
      restore_mapping_to_temporary(vrip);
    }  /* if */
    if (vrip->kind == vrk_temporary) {
      /* A temporary. */
      a_variable_ptr temp_var = vrip->variant.variable;
      if (vrip->local_temporary_reused) {
        /* This variable is a previously-allocated temporary.  It's already
           on the scope list and on the reusable temporaries list. */
      } else {
        /* Add the variable to the current scope. */
        add_temporary_to_scope(temp_var, (a_scope_ptr)NULL);
        if (vrip->local_temporary_okay) {
          /* This variable is used like a local temporary.  Put it on the list
             of such temporaries so that it can be reused after the end of
             the current full expression. */
          add_to_reusable_temporaries_list(temp_var);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
}  /* finish_variable_remapping_for_inlining */


static void relink_argument_expressions_on_failure(void)
/*
Inlining of a call has failed for some reason.  Relink the argument expressions
of the original call into a list again.  (They were broken apart and used
separately in assignments to parameter temporaries.)
*/
{
  a_variable_remapping_for_inlining_ptr vrip;

  /* Look at each remapping established. */
  for (vrip = variable_remappings_for_inlining;
       vrip != NULL;
       vrip = vrip->next) {
    if (vrip->arg_expr != NULL) {
      vrip->arg_expr->next = vrip->arg_expr_next;
      /* Clear the result_is_not_used flag, which may have been set if
         the argument expression was evaluated as a statement only to get
         its side effects, if the parameter is not referenced within the
         called function. */
      vrip->arg_expr->result_is_not_used = FALSE;
    }  /* if */
  }  /* for */
}  /* relink_argument_expressions_on_failure */


static a_variable_remapping_for_inlining_ptr
                             get_var_remapping_for_inlining(a_variable_ptr var)
/*
See if the variable var is remapped in the current inlining operation.
If so, return a pointer to the remapping entry.  If not, return NULL.
*/
{
  a_variable_remapping_for_inlining_ptr vrip;

  /* Look at each variable remapping on the list. */
  for (vrip = variable_remappings_for_inlining;
       vrip != NULL;
       vrip = vrip->next) {
    if (vrip->kind != vrk_none && vrip->orig_variable == var) {
      /* Found the remapping. */
      break;
    }  /* if */
  }  /* for */
  return vrip;
}  /* get_var_remapping_for_inlining */


a_variable_ptr remap_var_for_inlining(a_variable_ptr var)
/*
See if the indicated variable is remapped in the current inlining operation.
Return the new variable if there is a remapping, or the original variable
if not.  An internal error is generated if the remapping is to something
other than a temporary variable.
*/
{
  a_variable_ptr                        new_var;
  a_variable_remapping_for_inlining_ptr vrip;

  vrip = get_var_remapping_for_inlining(var);
  if (vrip != NULL) {
    /* There is a remapping. */
    check_assertion_str(vrip->kind == vrk_temporary,
                        "remap_var_for_inlining: wrong kind of remap");
    new_var = vrip->variant.variable;
    vrip->remapping_used = TRUE;
    vrip->temporary_used = TRUE;
  } else {
    /* There is no remapping, so return the original variable. */
    new_var = var;
  }  /* if */
  return new_var;
}  /* remap_var_for_inlining */


void adjust_copied_expression_for_inlining(an_expr_node_ptr expr)
/*
The indicated expression has just been created as a copy of an expression
during inlining.  See whether it should be adjusted, e.g., because of remapped
variables.
*/
{
  an_expr_node_kind                     kind = expr->kind;
  a_type_ptr                            expr_type = expr->type;
  a_variable_remapping_for_inlining_ptr vrip;
  a_constant_ptr                        con;
  an_expr_node_ptr                      constant_expr;

  if (kind == (an_expr_node_kind)enk_variable) {
    /* Value of a variable.  See if the variable is remapped. */
    vrip = get_var_remapping_for_inlining(expr->variant.variable);
    if (vrip != NULL) {
      /* Yes, there is some kind of remapping. */
      switch (vrip->kind) {
        case vrk_temporary:
          /* The variable is remapped to a temporary variable. */
          expr->variant.variable = vrip->variant.variable;
          vrip->temporary_used = TRUE;
          break;
        case vrk_constant_expr:
          /* The variable is remapped to a constant-valued expression.
             Look for some special cases. */
          constant_expr = vrip->variant.expr;
          if (is_constant_node(constant_expr)) {
            /* The variable is remapped to a constant.  Use an enk_constant
               instead. */
            set_expr_node_kind(expr, (an_expr_node_kind)enk_constant);
            expr->variant.constant = constant_expr->variant.constant;
          } else if (is_variable_address_node(constant_expr)) {
            /* The variable is remapped to the address of a variable. */
            set_expr_node_kind(expr, (an_expr_node_kind)enk_variable_address);
            expr->variant.variable = constant_expr->variant.variable;
          } else {
            /* Other, more complicated, cases.  Just copy the expression. */
            overwrite_node(expr, copy_expr_tree_for_inlining(constant_expr));
            /* Restore the original type, which might be slightly different
               for pointer-to-member cases. */
            expr->type = expr_type;
          }  /* if */
          break;
        default:
          unexpected_condition_str(
                      "adjust_copied_expression_for_inlining: bad remap kind");
      }  /* switch */
      vrip->remapping_used = TRUE;
    }  /* if */
  } else if (kind == (an_expr_node_kind)enk_variable_address) {
    /* Address of a variable.  See if the variable is remapped. */
    expr->variant.variable = remap_var_for_inlining(expr->variant.variable);
  } else if (kind == (an_expr_node_kind)enk_constant) {
    /* Value of a constant. */
    /* When doing inlining, we may have a constant here that is in
       a function scope memory region other than the one we are currently
       working in.  If so, we need to make a copy of the constant so we
       aren't pointing over to another function scope memory region.
       The constant we are copying is probably from an initializer, and
       therefore unshared, but this reference from an expression node
       can use a shareable constant. */
    con = expr->variant.constant;
    if (!in_file_scope(con)) {
      expr->variant.constant = alloc_shareable_constant(con);
    }  /* if */
  } else if (kind == (an_expr_node_kind)enk_operation) {
    /* Look for operations that now have constant operands because of
       parameter variables remapped to constants. */
    an_expr_operator_kind op = expr->variant.operation.kind;
    an_expr_node_ptr      operand = expr->variant.operation.operands;
    an_expr_node_ptr      operand2 = operand->next;
    a_constant_ptr        con2;
    a_constant            constant;
    a_boolean             has_constant_value = FALSE;
    if (is_constant_node(operand) &&
        (operand2 == NULL || is_constant_node(operand2))) {
      /* The operands are constant. */
      a_boolean did_not_fold = TRUE, template_constant = FALSE;
      con = operand->variant.constant;
      if (operand2 != NULL) con2 = operand2->variant.constant;
      /* Fold certain constant operations.  Some, like floating-point
         operations, are not folded because cfront does not do so,
         and because if it were done people might get different
         results with inlining. */
      /* See copy_and_simplify_short_circuited_operation for the
         short-circuited operations. */
      switch (op) {
        case eok_ieq: case eok_ine: case eok_igt:
        case eok_ilt: case eok_ige: case eok_ile:
        case eok_peq: case eok_pne:
        case eok_iadd:
        case eok_isubtract:
        case eok_imultiply:
        case eok_idivide:
        case eok_remainder:
        case eok_and:
        case eok_or:
        case eok_xor:
        case eok_shiftl:
        case eok_shiftr:
          /* Avoid folding operations on pointers to data members that haven't
             been lowered into integers yet (because the constant is in the
             file scope).  Test is done for integer/pointer to be conservative
             in case other kinds of constants in the future are changed by
             lowering. */
          if ((con->kind != (a_constant_repr_kind)ck_integer &&
               con->kind != (a_constant_repr_kind)ck_address) ||
              (con2->kind != (a_constant_repr_kind)ck_integer &&
               con2->kind != (a_constant_repr_kind)ck_address)) {
            /* Do not fold. */
          } else {
            /* Setting evaluated_context to FALSE suppresses warnings on
               errors like division by zero.  Instead, did_not_fold is
               returned TRUE. */
            binary_operation(op, con, con2, expr_type, &constant,
                             /*constant_context=*/FALSE,
                             /*evaluated_context=*/FALSE,
                             &did_not_fold,
                             &template_constant,
                             &code_pos_for_lowering);
          }  /* if */
          break;
        case eok_inegate:
        case eok_complement:
        case eok_not:
          /* See comment above; avoid pointers to data members.  This is
             probably unnecessary. */
          if (con->kind != (a_constant_repr_kind)ck_integer &&
              con->kind != (a_constant_repr_kind)ck_address) {
            /* Do not fold. */
          } else {
            /* Setting evaluated_context to FALSE suppresses warnings on
               errors like division by zero.  Instead, did_not_fold is
               returned TRUE. */
            unary_operation(op, con, expr_type, &constant,
                            /*constant_context=*/FALSE,
                            /*evaluated_context=*/FALSE,
                            &did_not_fold,
                            &template_constant,
                            &code_pos_for_lowering);
          }  /* if */
          break;
        default:
          /* Others cannot be folded. */
          break;
      }  /* switch */
      check_assertion(!template_constant);
      if (!did_not_fold) {
        /* The expression can be folded. */
        has_constant_value = TRUE;
      }  /* if */
    } else if (op == (an_expr_operator_kind)eok_pne ||
               op == (an_expr_operator_kind)eok_peq) {
      /* A special case where we can do folding even with a nonconstant
         operand: &variable != 0 is always 1.  The "== 0" case is
         always 0. */
      a_boolean is_non_null;
      if (is_constant_valued_expression(operand,
                                        /*local_vars_change=*/TRUE,
                                        /*other_vars_change=*/TRUE,
                                        &is_non_null) &&
          is_non_null) {
        operand = operand->next;
        if (is_constant_node(operand) &&
            constant_bool_value_known_at_compile_time(
                                                  operand->variant.constant) &&
            is_false_constant(operand->variant.constant)) {
          /* Yes, this is &auto_variable != NULL, which is always 1,
             or the "== 0" case, which is always 0. */
          a_host_large_integer	temp_value;
          has_constant_value = TRUE;
          temp_value = (a_host_large_integer)
                             ((op == (an_expr_operator_kind)eok_pne)? 1L : 0L);
          set_integer_constant(&constant, temp_value,
                               (an_integer_kind)ik_int);
        }  /* if */
      }  /* if */
    }  /* if */
    if (has_constant_value) {
      /* Replace the expression by a constant value. */
      constant.type = expr_type;
      set_expr_node_kind(expr, (an_expr_node_kind)enk_constant);
      expr->variant.constant = alloc_shareable_constant(&constant);
    }  /* if */
  }  /* if */
}  /* adjust_copied_expression_for_inlining */


a_boolean copy_and_simplify_short_circuited_operation(an_expr_node_ptr expr)
/*
expr is a copy of an enk_operation node being made while copying an
expression for inlining.  The node "expr" is a copy, but its subtree has
not been copied yet.  If expr is a short-circuitable operation, do
the rest of the copy (simplifying in the process) and return TRUE;
otherwise, do no copying and return FALSE.
*/
{
  a_boolean             processed = FALSE;
  an_expr_operator_kind op = expr->variant.operation.kind;
  an_expr_node_ptr      operand, operand2, operand3;

  if (op == (an_expr_operator_kind)eok_question ||
      op == (an_expr_operator_kind)eok_lor ||
      op == (an_expr_operator_kind)eok_land) {
    /* This is a short-circuited operation. */
    processed = TRUE;
    operand = expr->variant.operation.operands;
    operand2 = operand->next;
    operand3 = operand2->next;
    /* Copy the first operand.  In the process, simplify to a constant if
       possible by substituting for parameter variables. */
    operand = copy_expr_tree_for_inlining(operand);
    if (!is_constant_node(operand) ||
        !constant_bool_value_known_at_compile_time(operand->variant.constant)){
      /* The first operand is not constant, so this operation cannot be
         simplified.  Just copy the rest of the operands. */
      operand2 = copy_expr_tree_for_inlining(operand2);
      if (operand3 != NULL) operand3 = copy_expr_tree_for_inlining(operand3);
      /* Link the copied operands together. */
      expr->variant.operation.operands = operand;
      operand->next = operand2;
      operand2->next = operand3;
    } else {
      /* The first operand is constant, so the operation can be simplified. */
      a_constant_ptr con = operand->variant.constant;
      if (op == (an_expr_operator_kind)eok_question) {
        /* "?" operation.  Keep the second or third operand on the basis
           of the value of the first operand. */
        if (is_false_constant(con)) {
          /* The constant is false, so keep the third operand. */
          operand3 = copy_expr_tree_for_inlining(operand3);
          overwrite_node(expr, operand3);
        } else {
          /* The constant is true, so keep the second operand. */
          operand2 = copy_expr_tree_for_inlining(operand2);
          overwrite_node(expr, operand2);
        }  /* if */
      } else if (op == (an_expr_operator_kind)eok_lor) {
        /* "||" operation. */
        if (is_false_constant(con)) {
          /* The first operand is false, so the second operand is the value
             of the expression. */
          operand2 = copy_expr_tree_for_inlining(operand2);
          overwrite_node(expr, operand2);
        } else {
          /* The first operand is true, so the overall operation has the
             value true. */
          overwrite_node(expr, operand);
        }  /* if */
      } else if (op == (an_expr_operator_kind)eok_land) {
        /* "&&" operation. */
        if (is_false_constant(con)) {
          /* The first operand is false, so the overall operation has the
             value false. */
          overwrite_node(expr, operand);
        } else {
          /* The first operand is true, so the second operand is the value
             of the expression. */
          operand2 = copy_expr_tree_for_inlining(operand2);
          overwrite_node(expr, operand2);
        }  /* if */
      } else {
        unexpected_condition();
      }  /* if */
    }  /* if */
  } else if (op == (an_expr_operator_kind)eok_iassign ||
             op == (an_expr_operator_kind)eok_passign) {
    /* If this is an assignment to a temporary with special properties
       created previously by inlining, we may be able to do something
       special. */
    operand = expr->variant.operation.operands;
    operand2 = operand->next;
    if (is_variable_address_node(operand)) {
      a_variable_ptr var = operand->variant.variable;
      if (var->is_temp_for_constructor_this_inlined_param ||
          var->is_temp_for_unmodified_inlined_param) {
        a_variable_remapping_for_inlining_ptr vrip;
        a_boolean                             temp_elim_possible = FALSE;
        /* This is a temporary with special properties.  If the value
           being assigned to the temporary is constant (and non-null,
           for the constructor "this" case), the temporary can be remapped
           to the constant and eliminated.  Note that we are grabbing the
           operation here before the subtree under it has been remapped,
           so we can look at the original variable being assigned to. */
        a_boolean is_non_null;
        vrip = get_var_remapping_for_inlining(var);
        check_assertion(vrip != NULL);
        /* If a vrk_temporary remapping was temporarily switched to some
           other remapping, switch it back. */
        restore_mapping_to_temporary(vrip);
        check_assertion(vrip->kind == vrk_temporary);
        /* Copy the source operand with substitution and constant folding
           so we can see if we have a constant. */
        operand2 = copy_expr_tree_for_inlining(operand2);
        processed = TRUE;
        if (is_constant_valued_expression(operand2,
                                          /*local_vars_change=*/TRUE,
                                          /*other_vars_change=*/TRUE,
                                          &is_non_null) &&
            (!var->is_temp_for_constructor_this_inlined_param ||
             is_non_null)) {
          /* The temporary elimination cannot be done if the temporary has
             already been referenced. */
          if (!vrip->remapping_used) temp_elim_possible = TRUE;
        }  /* if */
        if (temp_elim_possible) {
          a_constant constant;
          /* Change the remapping of the temporary.  Note that changing the
             remapping means that the temporary variable will not be
             added to the scope at the end of the current inline expansion,
             so the temporary disappears completely. */
          /* Save the original temporary pointer so it can be restored.
             This comes up if the temporary is reused; it might be used
             also for another parameter, and might or might not be
             optimized away in that case. */
          vrip->orig_temporary = vrip->variant.variable;
          vrip->kind = vrk_constant_expr;
          vrip->variant.expr = operand2;
          /* Eliminate the assignment node by replacing it with a zero
             of the right type. */
          make_zero_of_proper_type(expr->type, &constant);
          set_expr_node_kind(expr, (an_expr_node_kind)enk_constant);
          expr->variant.constant = alloc_shareable_constant(&constant);
        } else {
          /* The operation cannot be eliminated, so finish the rewriting,
             leaving an updated assignment in place. */
          operand = copy_expr_tree_for_inlining(operand);
          operand->next = operand2;
          expr->variant.operation.operands = operand;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return processed;
}  /* copy_and_simplify_short_circuited_operation */


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
  set_stmt_pos_to_code_pos_for_lowering(new_statement);
  new_statement->has_associated_pragma = FALSE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  new_statement->source_sequence_entry = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
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
  an_expr_node_ptr   stmt_expr, expr;
  a_statement_ptr    new_statement, stmt, prev_stmt;
  a_label_ptr        label;
  an_insert_location sub_insert_location;
  a_boolean          result_is_then, result_is_else;

  if (statement == NULL) {
    /* No statement to copy. */
  } else if (statement->has_associated_pragma) {
    /* Can't inline a statement with an associated pragma. */
    goto cannot_inline_ever;
  } else {
    stmt_expr = statement->expr;
    if (stmt_expr != NULL) stmt_expr = copy_expr_tree_for_inlining(stmt_expr);
    switch (statement->kind) {
#if REPRESENT_EMPTY_STATEMENTS_IN_IL
      case stmk_empty:
        /* An empty statement has no side effects: copy it over as is if we
           are inserting a statement.  If we are inserting an expression, just
           ignore this. */
        if (!is_expr_insert_location_kind(insert_location->kind)) {
          (void)copy_inlined_statement(statement, insert_location);
        }  /* if */
        break;
#endif /* REPRESENT_EMPTY_STATEMENTS_IN_IL */
      case stmk_expr:
        if (is_expr_insert_location(insert_location)) {
          /* An expression statement is copied as an expression. */
          insert_expr(stmt_expr, insert_location);
        } else {
          /* An expression statement is copied as an expression statement. */
          new_statement = copy_inlined_statement(statement, insert_location);
          new_statement->expr = stmt_expr;
          set_expr_result_not_used(stmt_expr);
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
        /* Find the last statement in the top block.  Do that repeatedly
           so we can find a return that is the last statement in a block
           that is the last statement in the block ... that is the last
           statement in the top level block of the function. */
        stmt = routine_scope_being_inlined->assoc_block;
        do {
          stmt = last_statement_in_block(stmt);
        } while (stmt->kind == (a_statement_kind)stmk_block);
        if (stmt == statement) {
          /* Yes, this return is the last in the top block, so it can be
             inlined. */
          if (is_expr_insert_location(insert_location)) {
            /* Expression insert location. */
            if (stmt_expr != NULL) {
              insert_expr(stmt_expr, insert_location);
            } else {
              /* No expression is being returned. */
              a_type_ptr routine_type = f_skip_typerefs(
                                                  routine_scope_being_inlined->
                                                   variant.routine.ptr->type);
              if (routine_type->variant.routine.extra_info->
                                                     value_returned_by_cctor) {
                /* The routine returns its value via a copy constructor, so
                   don't check for the return type matching the type of the
                   expression -- the return value is passed via an added
                   parameter. */
              } else {
                a_type_ptr routine_return_type = routine_type->
                                                   variant.routine.return_type;
                if (!is_void_type(routine_return_type)) {
                  /* The return statement returns nothing and the function
                     expects a return value, so this function cannot be
                     inlined. */
                  goto cannot_inline_ever;
                }  /* if */
              }  /* if */
            }  /* if */
          } else {
            /* Statement insert location.  A return without an expression
               is just thrown away.  A return with an expression is just
               the expression (for side effects). */
            if (stmt_expr != NULL &&
                node_has_side_effects(stmt_expr, (a_boolean *)NULL)) {
              stmt_expr = add_cast(stmt_expr, void_type());
              stmt = insert_expr_statement(stmt_expr, insert_location);
              set_stmt_pos_to_code_pos_for_lowering(stmt);
            }  /* if */
          }  /* if */
          break;
        }  /* if */
        goto cannot_inline_ever;
      case stmk_if:
        /* "if" statement. */
        /* See if the tested expression is known.  If so, the "if" can be
           reduced to the "then" or "else" statement. */
        result_is_then = result_is_else = FALSE;
        if (is_constant_node(stmt_expr) &&
            constant_bool_value_known_at_compile_time(
                                                stmt_expr->variant.constant)) {
          if (is_false_constant(stmt_expr->variant.constant)) {
            result_is_else = TRUE;
          } else {
            result_is_then = TRUE;
          }  /* if */
        }  /* if */
        if (is_expr_insert_location(insert_location)) {
          an_expr_node_ptr then_expr, else_expr;
          /* Expression insert location.  Turn an "if" into a "?" operator. */
          if (!result_is_else) {
            /* Copy the "then" statement. */
            set_expr_creation_insert_location(&sub_insert_location);
            expand_statement_inline(statement->variant.if_stmt.then_statement,
                                    &sub_insert_location, inlinable, failed);
            if (!*failed) {
              then_expr = sub_insert_location.variant.expr;
              then_expr = add_cast_if_necessary(then_expr, void_type());
            }  /* if */
          }  /* if */
          if (!result_is_then) {
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
            if (result_is_then) {
              /* The result is the "then" expression. */
              expr = then_expr;
            } else if (result_is_else) {
              /* The result is the "else" expression. */
              expr = else_expr;
            } else {
              /* Make the "?" operator. */
              stmt_expr->next = then_expr;
              then_expr->next = else_expr;
              expr = make_operator_node((an_expr_operator_kind)eok_question,
                                        void_type(),
                                        stmt_expr);
              expr->is_initialization_guard=statement->is_initialization_guard;
            }  /* if */
            insert_expr(expr, insert_location);
          }  /* if */
        } else {
          a_statement_ptr then_stmt, else_stmt;
          /* Statement insert location. */
          if (!result_is_else) {
            /* Copy the "then" statement. */
            set_statement_creation_insert_location(&sub_insert_location);
            expand_statement_inline(statement->variant.if_stmt.then_statement,
                                    &sub_insert_location, inlinable, failed);
            then_stmt = sub_insert_location.variant.stmt;
          }  /* if */
          if (!result_is_then) {
            if (statement->variant.if_stmt.else_statement != NULL) {
              /* Copy the "else" statement. */
              set_statement_creation_insert_location(&sub_insert_location);
              expand_statement_inline(
                                     statement->variant.if_stmt.else_statement,
                                     &sub_insert_location, inlinable, failed);
              else_stmt = sub_insert_location.variant.stmt;
            } else {
              else_stmt = NULL;
            }  /* if */
          }  /* if */
          if (result_is_then) {
            /* The result is the "then" statement. */
            insert_statement(then_stmt, insert_location);
          } else if (result_is_else) {
            /* The result is the "else" statement. */
            if (else_stmt != NULL) {
              insert_statement(else_stmt, insert_location);
            }  /* if */
          } else {
            /* Insert an "if" statement. */
            new_statement = copy_inlined_statement(statement, insert_location);
            new_statement->expr = stmt_expr;
            new_statement->variant.if_stmt.then_statement = then_stmt;
            new_statement->variant.if_stmt.else_statement = else_stmt;
          }  /* if */
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
          set_stmt_pos_to_code_pos_for_lowering(new_statement);
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
          if (is_array_type(var->type)) {
            /* Aggregates can't be handled. */
            goto cannot_inline_ever;
          } else if (dip->kind == (a_dynamic_init_kind)dik_constant) {
            /* Aggregates can't be handled. */
            if (dip->variant.constant->kind ==
                                          (a_constant_repr_kind)ck_aggregate) {
              goto cannot_inline_ever;
            }  /* if */
            /* Non-aggregate constant initial value. */
            /* This uses copy_unshared_constant_full because that routine does
               variable remapping if necessary. */
            init_expr = alloc_node_for_constant(
                             copy_unshared_constant_full(dip->variant.constant,
                                          CE_DOING_INLINING_OF_FUNCTION_CALL));
          } else if (dip->kind == (a_dynamic_init_kind)dik_expression) {
            init_expr = copy_expr_tree_for_inlining(dip->variant.expression);
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
          stmt = insert_expr_statement(expr, insert_location);
          set_stmt_pos_to_code_pos_for_lowering(stmt);
          var->initialization_rewritten_as_assignment = TRUE;
        }
        break;
      case stmk_while:
      case stmk_end_test_while:
        if (is_expr_insert_location(insert_location)) {
          /* Cannot inline this case in an expression context. */
          goto cannot_inline;
        }  /* if */
        /* Copy the dependent statement. */
        set_statement_creation_insert_location(&sub_insert_location);
        expand_statement_inline(statement->variant.loop_statement,
                                &sub_insert_location, inlinable, failed);
        stmt = sub_insert_location.variant.stmt;
        /* Copy the "while" statement. */
        new_statement = copy_inlined_statement(statement, insert_location);
        new_statement->expr = stmt_expr;
        new_statement->variant.loop_statement = stmt;
        break;
      case stmk_for:
        { a_statement_ptr  init_stmt;
          an_expr_node_ptr increment_expr;
          if (is_expr_insert_location(insert_location)) {
            /* Cannot inline this case in an expression context. */
            goto cannot_inline;
          }  /* if */
          /* Copy the initialization statement. */
          set_statement_creation_insert_location(&sub_insert_location);
          expand_statement_inline(statement->variant.for_loop.extra_info->
                                                                initialization,
                                  &sub_insert_location, inlinable, failed);
          init_stmt = sub_insert_location.variant.stmt;
          /* Copy the dependent statement. */
          set_statement_creation_insert_location(&sub_insert_location);
          expand_statement_inline(statement->variant.for_loop.statement,
                                  &sub_insert_location, inlinable, failed);
          stmt = sub_insert_location.variant.stmt;
          /* Copy the increment expression. */
          increment_expr = statement->variant.for_loop.extra_info->increment;
          if (increment_expr != NULL) {
            increment_expr = copy_expr_tree_for_inlining(increment_expr);
            set_expr_result_not_used(increment_expr);
          }  /* if */
          /* Copy the "for" statement.  This does not use
             copy_inlined_statement because it needs to copy the for loop
             supplement. */
          new_statement = alloc_statement((a_statement_kind)stmk_for);
          set_stmt_pos_to_code_pos_for_lowering(new_statement);
          insert_statement(new_statement, insert_location);
          new_statement->expr = stmt_expr;
          new_statement->variant.for_loop.statement = stmt;
          new_statement->variant.for_loop.extra_info->initialization=init_stmt;
          new_statement->variant.for_loop.extra_info->increment=increment_expr;
        }
        break;
#if GENERATE_SOURCE_SEQUENCE_LISTS
      case stmk_decl:
        /* Statement that marks the location of declarations.  Ignored here. */
        break;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      case stmk_asm:
      case stmk_switch:
      case stmk_try_block:
#if MICROSOFT_EXTENSIONS_ALLOWED
      case stmk_microsoft_try:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      case stmk_set_vla_size:
      case stmk_vla_decl:
      case stmk_vla_dealloc:
      default:
cannot_inline_ever:
        /* This statement cannot be inlined in any context. */
        *inlinable = FALSE;
cannot_inline:
        /* This statement cannot be inlined in this case. */
        *failed = TRUE;
        break;
    }  /* switch */
  }  /* if */
}  /* expand_statement_inline */


static void issue_inlining_failure_diagnostic(a_routine_ptr routine)
/*
Issue a diagnostic about a failure to inline the indicated routine.
*/
{
  a_symbol_ptr sym = (a_symbol_ptr)routine->source_corresp.assoc_info;

  if (sym != NULL) {
    if (!routine->inlinable) {
      /* The routine cannot ever be inlined. */
      pos_sy_remark(ec_cannot_inline, &sym->decl_position, sym);
    } else {
      /* The routine cannot be inlined in this case. */
      sym_remark(ec_cannot_inline_call, sym);
    }  /* if */
  }  /* if */
}  /* issue_inlining_failure_diagnostic */


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
    if (!routine->is_inline) {
      /* Make sure that if the routine gets marked as inline later an
         out-of-line copy is generated to satisfy this call. */
      routine->need_out_of_line_copy = TRUE;
    } else {
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
          /* Relink the argument expressions of the call by their "next"
             pointers. */
          relink_argument_expressions_on_failure();
        } else {
          /* Inlining was successful. */
          /* Now that inlining is known to have succeeded, add the temporary
             variables to the current scope. */
          finish_variable_remapping_for_inlining();
          if (statement != NULL) {
            /* Replace the original call statement by overwriting it with
               the block statement containing the inlined code. */
            /* Eliminate any extra unnecessary blocks that are present.
               This happens if no parameter assignments were generated. */
            a_statement_ptr inner_stmt;
            for (;;) {
              inner_stmt = block_stmt->variant.block.statements;
              if (inner_stmt != NULL &&
                  inner_stmt->kind == (a_statement_kind)stmk_block &&
                  inner_stmt->next == NULL &&
                  block_stmt->variant.block.extra_info->assoc_scope == NULL) {
                block_stmt = inner_stmt;
              } else {
                break;
              }  /* if */
            }  /* for */
            copy_statement(block_stmt, statement);
          } else {
            an_expr_node_ptr inlined_call_expr = insert_location.variant.expr;
            check_assertion(inlined_call_expr != NULL);
            /* Make sure the expression has the type expected for the call. */
            if (is_void_type(expr->type)) {
              /* For void functions, make sure the type of the expression is
                 void (it's currently whatever type the last
                 statement/expression added has.) */
              inlined_call_expr = add_cast_if_necessary(inlined_call_expr,
                                                        expr->type);
            } else {
              /* For non-void functions, the type of the expression can be
                 wrong if the end of the function is unreachable (e.g.,
                 because it ends with a throw).  In that case, add a zero
                 cast to the right type. */
              if (!il_identical_types(expr->type, inlined_call_expr->type)) {
                a_constant       zero_constant;
                a_type_ptr       needed_type = expr->type;
                an_expr_node_ptr zero_node;
                a_boolean        class_case =
                                       is_class_struct_union_type(needed_type);
                if (class_case) {
                  /* For a class case, make a null pointer to the type and
                     indirect through it. */
                  needed_type = make_pointer_type(needed_type);
                }  /* if */
                make_zero_of_proper_type(needed_type, &zero_constant);
                zero_node = alloc_node_for_constant(&zero_constant);
                if (class_case) zero_node = add_indirection_to_node(zero_node);
                insert_expr(zero_node, &insert_location);
                inlined_call_expr = insert_location.variant.expr;
              }  /* if */
            }  /* if */
            /* Replace the original call node by overwriting it with the
               expression for the inlined call. */
            overwrite_node(expr, inlined_call_expr);
          }  /* if */
        }  /* if */
        /* Free the remapping entries. */
        free_variable_remappings_for_inlining();
        /* Put the inlinable flag back on, unless we've discovered that this
           function can never be inlined. */
        routine->inlinable = inlinable;
        if (failed) issue_inlining_failure_diagnostic(routine);
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
#if ASSIGNMENT_TO_THIS_ALLOWED
  } else if (routine->assignment_to_this_done) {
    /* An assignment to "this" was done.  This is ruled out because
       we want to be able to count on "this" being constant across the
       whole invocation. */
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
  } else {
    /* The routine looks like it can be inlined. */
    routine->inlinable = TRUE;
  }  /* if */
  if (!routine->inlinable) issue_inlining_failure_diagnostic(routine);
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
    if (routine->is_inline && routine->inlinable) {
      /* If the routine's address was taken, an out of line copy is needed. */
      if (routine->address_taken) routine->need_out_of_line_copy = TRUE;
#if INSTANTIATE_EXTERN_INLINE
      if (!routine->suppress_inline_body) {
        /* When instantiating extern inline functions we need to put out
           an out-of-line copy when told to do so via the suppress_inline_body
           flag. */
        routine->need_out_of_line_copy = TRUE;
      }  /* if */
#endif /* INSTANTIATE_EXTERN_INLINE */
      if (!routine->need_out_of_line_copy) {
        /* We don't need an out-of-line copy, so mark the routine as
           unreferenced because it's no longer needed. */
        routine->source_corresp.referenced = FALSE;
      }  /* if */
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
#if DEBUG
  num_variable_remappings_for_inlining = 0;
#endif /* DEBUG */
  /* Static variables in inline.c: */
  variable_remappings_for_inlining = NULL;
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
