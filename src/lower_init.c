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

lower_init.c -- IL lowering: initializations and new/delete.

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

/* Additional header files. */
#include "expr.h"
#include "exprutil.h"

static a_routine_ptr
		file_scope_init_routine;
			/* Pointer to the file-scope initialization routine
			   once created.  NULL until then. */
static an_insert_location
		dtor_wrapper_prologue_insert_location;
			/* Insert location in the wrapper prologue for
			   a destructor, used to insert exception handling
			   setup code. */


/*
Put the current code_pos_for_lowering into a statement, if the statement
pointer is non-NULL.
*/
#define set_stmt_pos_to_code_pos_for_lowering(stmt)                   \
{ if ((stmt) != NULL) {                                               \
    set_stmt_source_position((stmt)->position, code_pos_for_lowering);\
  }  /* if */                                                         \
}  /* set_stmt_pos_to_code_pos_for_lowering */


static a_type_ptr make_function_type(a_type_ptr return_type,
                                     a_type_ptr param_1_type)
/*
Make a function type for a prototyped function prototyped as
having a parameter with type param_1_type and returning return_type,
and return a pointer to it.  If no arguments are desired, param_1_type
should be specified as NULL.
*/
{
  a_type_ptr       rout_type;
  a_param_type_ptr ptp;

  rout_type = alloc_type((a_type_kind)tk_routine);
  rout_type->variant.routine.return_type = return_type;
  rout_type->variant.routine.extra_info->prototyped =
                                              !MAKE_ALL_FUNCTIONS_UNPROTOTYPED;
  if (param_1_type != NULL) {
    ptp = alloc_param_type(param_1_type);
    /* It is not necessary to clear il_lowering_flag; the entry does not need
       to be lowered. */
    rout_type->variant.routine.extra_info->param_type_list = ptp;
  }  /* if */
  return rout_type;
}  /* make_function_type */


static a_routine_ptr make_rout_entry(char            *name,
                                     a_storage_class rout_storage_class,
                                     a_type_ptr      return_type,
                                     a_type_ptr      param_1_type)
/*
Make a routine entry for a function with the given name, prototyped as
having a parameter with type param_1_type and returning return_type,
and having storage class rout_storage_class.  Return a pointer to the
routine entry created.  The routine entry and its type are allocated
in the file scope.  If no arguments are desired, param_1_type should
be specified as NULL.  The name may be NULL.
*/
{
  a_routine_ptr rout;
  a_type_ptr    rout_type;
  sizeof_t      alloc_length;

  rout_type = make_function_type(return_type, param_1_type);
  rout = alloc_routine();
  if (name != NULL) {
    alloc_length = strlen(name)+1;
    rout->source_corresp.name = strcpy(alloc_lowered_name_string(alloc_length),
                                       name);
  }  /* if */
  rout->storage_class = rout_storage_class;
  rout->source_corresp.name_linkage =
        (rout_storage_class == (a_storage_class)sc_unspecified ||
         rout_storage_class == (a_storage_class)sc_extern) ?
                                            (a_name_linkage_kind)nlk_external :
        (rout_storage_class == (a_storage_class)sc_static) ?
                                            (a_name_linkage_kind)nlk_internal :
        /* Otherwise: */
                                            (a_name_linkage_kind)nlk_none;
  rout->type = rout_type;
  /* Add the routine to the file scope list. */
  add_to_routines_list(rout, /*at_file_scope=*/TRUE);
  return rout;
}  /* make_rout_entry */


a_routine_ptr make_runtime_routine(char          *name,
                                   a_routine_ptr *routine,
                                   a_type_ptr    return_type)
/*
Make a routine entry for the runtime routine named "name" and return a
pointer to it.  Also save the pointer in *routine.  If *routine is non-NULL
on entry, use that pointer.  The routine has unprototyped arguments and
its return type is return_type.
*/
{
  if (*routine == NULL) {
    *routine = make_rout_entry(name, (a_storage_class)sc_extern, return_type,
                               (a_type_ptr)NULL);
    (*routine)->type->variant.routine.extra_info->prototyped = FALSE;
  }  /* if */
  return *routine;
}  /* make_runtime_routine */


static a_statement_ptr insert_expr_statement(
                                        an_expr_node_ptr       node,
                                        an_insert_location_ptr insert_location)
/*
Make a statement from expression expr.  Insert the statement at
*insert_location and update *insert_location.  Return a pointer to the
statement, or NULL if no statement was created (in an expression insert
context).
*/
{
  a_statement_ptr stmt;

  if (is_expr_insert_location_kind(insert_location->kind)) {
    /* Insert within an expression.  The statement need not be created. */
    insert_expr(node, insert_location);
    stmt = NULL;
  } else {
    /* Make the expression statement. */
    stmt = alloc_expr_statement(node);
    /* Insert the statement at the right location. */
    insert_statement(stmt, insert_location);
  }  /* if */
  return stmt;
}  /* insert_expr_statement */


a_statement_ptr insert_assignment_statement(
                                        an_expr_node_ptr       lvalue_expr,
                                        an_expr_operator_kind  op,
                                        an_expr_node_ptr       rvalue_expr,
                                        an_insert_location_ptr insert_location)
/*
Make a statement that assigns rvalue_expr to lvalue_expr using assignment
operator op.  Insert the statement at *insert_location and update
*insert_location.  Return a pointer to the statement, or NULL if no
statement was created (in an expression insert context).
*/
{
  a_statement_ptr  assign_stmt;
  an_expr_node_ptr assign_node;

  lvalue_expr->next = rvalue_expr;
  /* Make the assignment operation. */
  assign_node = make_operator_node(op,
                                   f_skip_typerefs(
                                           type_pointed_to(lvalue_expr->type)),
                                   lvalue_expr);
  /* Make the expression statement. */
  assign_stmt = insert_expr_statement(assign_node, insert_location);
  return assign_stmt;
}  /* insert_assignment_statement */


a_statement_ptr insert_var_assignment_statement(
                                        a_variable_ptr         lvalue_var,
                                        an_expr_operator_kind  op,
                                        an_expr_node_ptr       rvalue_expr,
                                        an_insert_location_ptr insert_location)
/*
Make a statement that assigns rvalue_expr to lvalue_var using assignment
operator op.  Insert the statement at *insert_location and update
*insert_location.  Return a pointer to the statement, or NULL if no
statement was created (in an expression insert context).
*/
{
  a_statement_ptr  assign_stmt;
  an_expr_node_ptr lvalue_expr;

  /* Make an expression for the lvalue address. */
  lvalue_expr = var_lvalue_expr(lvalue_var);
  /* Make and insert the assignment. */
  assign_stmt = insert_assignment_statement(lvalue_expr, op, rvalue_expr,
                                            insert_location);
  return assign_stmt;
}  /* insert_var_assignment_statement */


static an_expr_node_ptr make_vtbl_address_node(a_variable_ptr var)
/*
Make an expression for the address of a virtual function table variable and
return a pointer to it.  The variable has an array type.  The pointer
has type pointer to element.
*/
{
  an_expr_node_ptr var_node;
  a_constant       addr_constant;
  a_type_ptr       ptr_element_type;

  ptr_element_type = make_pointer_type(array_element_type(var->type));
  /* Make a constant for the address of the array, implicitly cast it to
     pointer-to-element-type, and make an expression whose value is the
     address constant.  This gives an address with the right type. */
  set_variable_address_constant(var, &addr_constant,
                                /*set_address_taken_flag=*/FALSE);
  implicit_cast(&addr_constant, ptr_element_type);
  var_node = alloc_node_for_constant(&addr_constant);
  return var_node;
}  /* make_vtbl_address_node */


void do_ptr_to_data_member_arg_promotion_on_node(an_expr_node_ptr expr)
/*
expr is an unprototyped argument of a function call, whose value is a
pointer to data member.  Do widening on it, needed because pointers
to data members are lowered into a small integer type.
*/
{
  a_type_ptr ptr_to_data_member_type = integer_type(TARG_DELTA_INT_KIND);
  a_type_ptr promoted_type =
                           default_argument_promotion(ptr_to_data_member_type);

  if (ptr_to_data_member_type != promoted_type) {
    /* Some widening is needed. */
    if (is_constant_node(expr)) {
      /* A constant.  Do the type change on a copy of the constant. */
      /* This must be done on a copy because the constant is typically shared
         and in the file scope, and therefore unlowerable at this point. */
      a_constant con;
      con = *expr->variant.constant;
      lower_ptr_to_member_constant(&con);
      /* Widen the constant by changing its type. */
#if CHECKING
      if (con.kind != (a_constant_repr_kind)ck_integer) {
        internal_error(
                "do_ptr_to_data_member_arg_promotion_on_node: pm not int con");
      }  /* if */
#endif /* CHECKING */
      con.type = promoted_type;
      /* Allocate a copy of the constant, and point the expression to it. */
      expr->variant.constant = alloc_shareable_constant(&con);
      expr->type = promoted_type;
    } else {
      /* Add a cast, but reuse the original node as the cast to preserve the
         expression address. */
      change_to_cast(expr, copy_node(expr), promoted_type);
    }  /* if */
  }  /* if */
}  /* do_ptr_to_data_member_arg_promotion_on_node */

#if MAKE_ALL_FUNCTIONS_UNPROTOTYPED

void do_default_arg_promotions_on_node(an_expr_node_ptr expr)
/*
expr is an argument to a call.  If necessary, add a cast to it to
do any default argument promotions needed to pass it as an argument to
an unprototyped function (or to an ellipsis position on a prototyped
function).
*/
{
  a_type_ptr arg_type = expr->type, promoted_type;

  /* Note that we drop type qualifiers so we won't add a cast to drop
     type qualifiers. */
  arg_type = skip_typerefs(arg_type);
  if (is_integral_type(arg_type)) {
    /* Note that default_argument_promotion is not used for integral
       expressions because special handling is required for bit fields. */
    promoted_type = node_type_after_integral_promotion(expr);
  } else if (is_floating_type(arg_type)) {
    promoted_type = default_argument_promotion(arg_type);
  } else if (is_or_was_ptr_to_data_member_type(arg_type)) {
    /* Widen pointers-to-data-members (which have been or will be turned into
       integers). */
    do_ptr_to_data_member_arg_promotion_on_node(expr);
    /* The subroutine does all the processing. */
    goto done;
  } else {
    promoted_type = arg_type;
  }  /* if */
  if (promoted_type != arg_type) {
    /* Put in the promotion cast. */
    an_expr_node_ptr expr_cast = expr, expr_next = expr->next;
    an_expr_node     node_copy;

    cast_node(&expr_cast, promoted_type, /*is_implicit_cast=*/TRUE,
              &error_position);
    expr_cast->next = expr_next;
    if (expr_cast != expr) {
      /* A cast was added, so swap the cast and the original node so that the
         cast node ends up at the original address. */
      node_copy = *expr;
      *expr = *expr_cast;
      *expr_cast = node_copy;
#if CHECKING
      if (!is_operation_node(expr) ||
          expr->variant.operation.kind != (an_expr_operator_kind)eok_cast ||
          expr->variant.operation.operands != expr) {
        internal_error("do_default_arg_promotions_on_node: bad cast node");
      }  /* if */
#endif /* CHECKING */
      expr->variant.operation.operands = expr_cast;
    }  /* if */
  }  /* if */
done:;
}  /* do_default_arg_promotions_on_node */

#endif /* MAKE_ALL_FUNCTIONS_UNPROTOTYPED */

static an_expr_node_ptr make_call_node(a_routine_ptr    routine,
                                       an_expr_node_ptr arg_list,
                                       a_boolean        honor_virtual)
/*
Make an expression that calls routine "routine" with arguments "arg_list",
and return a pointer to it.  A virtual call is generated if the routine
is virtual and honor_virtual is TRUE.  The virtual call is *not* lowered.
*/
{
  an_expr_node_ptr      call_node, rout_node;
  a_type_ptr            rout_type, rout_return_type;
  an_expr_operator_kind op;

#if MAKE_ALL_FUNCTIONS_UNPROTOTYPED
  /* If transforming all functions to old-style unprototyped form (for
     cfront compatibility), do default argument promotions on the arguments.
     It might seem wasteful to do this on every argument list, since
     not many of the arguments will require promotion.  However, doing it
     here guarantees that all calls created by IL lowering will have
     properly-promoted arguments without special-case checks all over the
     place. */
  { an_expr_node_ptr arg_node;
    for (arg_node = arg_list; arg_node != NULL; arg_node = arg_node->next) {
      do_default_arg_promotions_on_node(arg_node);
    }  /* for */
  }
#endif /* MAKE_ALL_FUNCTIONS_UNPROTOTYPED */
  /* Make a node for the address of the routine. */
  rout_node = function_addr_expr(routine, /*set_address_taken_flag=*/FALSE);
  routine->called = TRUE;
  rout_node->next = arg_list;
  /* Lower the function type (or record it as an orphan).  This is important
     when calling routines mentioned in dynamic initialization entries that
     are declared now and get defined later in this compilation.  The type
     pointer in the routine entry will be changed to a new (equivalent) type
     at the point of definition, which means the type here will not be
     attached to any list and will not get lowered unless we do it here. */
  lower_os_type(routine->type);
  /* Choose the right operation (virtual call or non-virtual call). */
  if (routine->is_virtual && honor_virtual) {
    op = (an_expr_operator_kind)eok_virtual_call;
  } else {
    op = (an_expr_operator_kind)eok_call;
    routine->source_corresp.referenced = TRUE;
  }  /* if */
  /* Make the call node. */
  rout_type = skip_typerefs(routine->type);
  rout_return_type = rout_type->variant.routine.return_type;
  call_node = make_operator_node(op, rout_return_type, rout_node);
  return call_node;
}  /* make_call_node */


a_statement_ptr make_call_statement(a_routine_ptr    routine,
                                    an_expr_node_ptr arg_list)
/*
Make a statement that calls routine "routine" with arguments "arg_list",
and return pointer to it.
*/
{
  an_expr_node_ptr call_node;
  a_statement_ptr  call_stmt;

  /* Make the call node. */
  call_node = make_call_node(routine, arg_list, /*honor_virtual=*/FALSE);
  /* Allocate an expression statement and put the call into it. */
  call_stmt = alloc_expr_statement(call_node);
  return call_stmt;
}  /* make_call_statement */


an_expr_node_ptr make_runtime_rout_call(char             *name,
                                        a_routine_ptr    *routine,
                                        a_type_ptr       return_type,
                                        an_expr_node_ptr arg_expr_list)
/*
Make an expression node that calls the runtime routine "name" with the
arguments given by arg_expr_list.  *routine is set to point to the runtime
routine entry; if it is non-NULL on entry, it is used.  The routine has
unprototyped arguments and its return type is return_type.
*/
{
  an_expr_node_ptr node;

  /* Make the routine entry if it does not exist already. */
  (void)make_runtime_routine(name, routine, return_type);
  /* Make the call node. */
  node = make_call_node(*routine, arg_expr_list, /*honor_virtual=*/FALSE);
  return node;
}  /* make_runtime_rout_call */


static void turn_statement_into_noop(a_statement_ptr statement)
/*
Convert the indicated statement into a no-op.
*/
{
  /* The general technique is to turn the statement into a block statement
     containing no statements.  If the statement was already a block statement,
     this loses the storage for the old block entry.  However, since it works,
     and since there's no reason to expect this routine to be called for
     block statements, we won't worry about that case. */
  set_statement_kind(statement, (a_statement_kind)stmk_block);
}  /* turn_statement_into_noop */


static void insert_if_statement(an_expr_node_ptr       test_expr,
                                an_insert_location_ptr insert_location,
                                an_insert_location_ptr insert_location2)
/*
Create an "if" statement that tests test_expr, and insert it at
insert_location.  Set insert_location2 to allow insertion of the
dependent statements of the "if".  This routine also handles the
case of inserting an if-equivalent into the middle of an expression.
*/
{
  a_statement_ptr  if_stmt, block_stmt;
  an_expr_node_ptr question_node, op2_node, op3_node, zero_node;
  a_type_ptr       void_type_ptr;

  if (is_expr_insert_location_kind(insert_location->kind)) {
    /* Insert within an expression. */
    /* Insert "test_expr ? (void)0 : (void)0" at the right place. */
    void_type_ptr = void_type();
    /* The second and third operands are each "(void)0". */
    zero_node = node_for_integer_constant(0L, (an_integer_kind)ik_int);
    op2_node = make_operator_node((an_expr_operator_kind)eok_cast,
                                  void_type_ptr, zero_node);
    zero_node = node_for_integer_constant(0L, (an_integer_kind)ik_int);
    op3_node = make_operator_node((an_expr_operator_kind)eok_cast,
                                  void_type_ptr, zero_node);
    test_expr->next = op2_node;
    op2_node->next = op3_node;
    question_node = make_operator_node((an_expr_operator_kind)eok_question,
                                       void_type_ptr, test_expr);
    insert_expr(question_node, insert_location);
    /* The insert location is before the "(void)0" of the second operand. */
    set_expr_insert_location(op2_node, insert_location2);
  } else {
    /* Insert within a statement sequence.  Allocate an "if" statement with
       a block statement under it. */
    if_stmt = alloc_statement((a_statement_kind)stmk_if);
    if_stmt->expr = test_expr;
    if_stmt->variant.if_stmt.then_statement = block_stmt =
                                 alloc_statement((a_statement_kind)stmk_block);
    insert_statement(if_stmt, insert_location);
    set_block_start_insert_location(block_stmt, insert_location2);
  }  /* if */
}  /* insert_if_statement */


static void enclose_routine_in_if(a_scope_ptr      scope,
                                  an_expr_node_ptr if_node,
                                  a_statement_ptr  *p_block_stmt,
                                  a_variable_ptr   return_var)
/*
Add an "if" statement around the entire body of the routine whose scope is
pointed to by scope.  if_node is the expression to be tested in the "if".
*block_stmt is set to point to the block statement that is made as the
dependent statement of the "if".  return_var is the variable to be returned
if a "return" statement must be generated, or NULL if no value needs
to be returned.
*/
{
  a_statement_ptr if_stmt, block_stmt, stmt, prev_stmt;

  if_stmt = alloc_statement((a_statement_kind)stmk_if);
  if_stmt->expr = if_node;
  if_stmt->variant.if_stmt.then_statement = *p_block_stmt = block_stmt =
                                 alloc_statement((a_statement_kind)stmk_block);
  /* Make the "if" the top-level statement in the routine, and put the
     original code under the "if". */
  block_stmt->variant.block.statements = stmt =
                                  scope->assoc_block->variant.block.statements;
  block_stmt->variant.block.extra_info->end_of_block_reachable = FALSE;
  scope->assoc_block->variant.block.statements = if_stmt;
  /* See if there is a return statement at the end of the original list of
     statements.  If so, move it outside the "if". */
  if (stmt != NULL) {
    for (prev_stmt = NULL;
         stmt->next != NULL;
         prev_stmt = stmt, stmt = stmt->next) {}
    if (stmt->kind == (a_statement_kind)stmk_return) {
      /* The last statement is a return.  Move it. */
      block_stmt->variant.block.extra_info->end_of_block_reachable = TRUE;
      if (prev_stmt == NULL) {
        block_stmt->variant.block.statements = NULL;
      } else {
        prev_stmt->next = NULL;
      }  /* if */
      if_stmt->next = stmt;
    }  /* if */
  }  /* if */
  /* If there is no return statement at the end of the routine (because the
     end of the original routine was not reachable), add one (because the
     end of the new routine is reachable if the "if" is not taken). */
  if (if_stmt->next == NULL) {
    a_statement_ptr return_stmt =
                                alloc_statement((a_statement_kind)stmk_return);
    if_stmt->next = return_stmt;
    if (return_var != NULL) return_stmt->expr = var_rvalue_expr(return_var);
  }  /* if */
}  /* enclose_routine_in_if */


static a_scope_ptr make_routine_definition(a_routine_ptr          rout_ptr,
                                           a_boolean              make_return,
                                           a_memory_region_number *il_region)
/*
Make a definition for the given routine, i.e., create a new memory region,
scope, and top-level block.  Return the address of the scope created,
and return the memory region number of the IL memory region in *il_region.
If make_return is TRUE, a return statement will be put into the top-level
block.
*/
{
  a_scope_ptr            scope;
  a_memory_region_number region_to_switch_back_to = curr_il_region_number;
  a_statement_ptr        block_stmt;

  /* Make a new memory region and scope. */
  scope = new_il_region((a_scope_kind)sck_function, next_scope_number++,
                        rout_ptr);
  *il_region = curr_il_region_number;
  /* Link the routine to the scope.  new_il_region did the link in the
     other direction. */
  rout_ptr->type->variant.routine.extra_info->assoc_routine = rout_ptr;
  rout_ptr->assoc_scope = curr_il_region_number;
  /* Make the top-level block statement. */
  scope->assoc_block = block_stmt =
                                 alloc_statement((a_statement_kind)stmk_block);
  block_stmt->variant.block.extra_info->end_of_block_reachable = FALSE;
  if (make_return) {
    /* Make a return statement as the end of the block. */
    block_stmt->variant.block.statements =
                                alloc_statement((a_statement_kind)stmk_return);
  }  /* if */
  switch_il_region(region_to_switch_back_to);
  return scope;
}  /* make_routine_definition */


static void set_variable_address_taken(a_variable_ptr variable)
/*
Set the address_taken flag in the indicated variable.
*/
{
  variable->address_taken = TRUE;
  /* If the storage class is "register", change it to "auto", because
     C doesn't allow taking the address of a register variable (C++ does). */
  if (variable->storage_class == (a_storage_class)sc_register) {
    variable->storage_class = (a_storage_class)sc_auto;
  }  /* if */
}  /* set_variable_address_taken */


static void clear_init_pos_modifier(an_init_pos_modifier_ptr ipmp)
/*
Set the fields of the indicated initialization position modifier entry to
default values.
*/
{
  ipmp->next       = NULL;
  ipmp->type       = NULL;
  ipmp->curr_elem  = 0;
  ipmp->curr_field = NULL;
  ipmp->curr_base  = NULL;
}  /* clear_init_pos_modifier */


static void add_init_pos_modifier(an_init_pos_modifier_ptr ipmp,
                                  an_init_pos_descr_ptr    ipdp)
/*
Set the fields of the indicated initialization position modifier entry to
default values and link it on the front of the list of modifiers of *ipdp.
*/
{
  clear_init_pos_modifier(ipmp);
  ipmp->next = ipdp->modifiers;
  ipdp->modifiers = ipmp;
}  /* add_init_pos_modifier */


static an_init_pos_modifier_ptr alloc_init_pos_modifier(void)
/*
Allocate an initialization position modifier entry, set its fields
to default values, and return a pointer to it.  Note that such entries are
usually allocated on the stack, so this routine is not called much.
*/
{
  an_init_pos_modifier_ptr ipmp;

  if (avail_init_pos_modifiers != NULL) {
    /* Reuse a freed entry. */
    ipmp = avail_init_pos_modifiers;
    avail_init_pos_modifiers = ipmp->next;
  } else {
    /* Allocate a new entry. */
    ipmp = (an_init_pos_modifier_ptr)alloc_fe(sizeof(an_init_pos_modifier));
#if DEBUG
    num_init_pos_modifiers_allocated++;
#endif /* DEBUG */
  }  /* if */
  clear_init_pos_modifier(ipmp);
  return ipmp;
}  /* alloc_init_pos_modifier */


void free_init_pos_modifier_list(an_init_pos_modifier_ptr ipmp)
/*
Free a list of initialization position modifier entries by putting them on
the available list.
*/
{
  an_init_pos_modifier_ptr ipmp_next;

  for (; ipmp != NULL; ipmp = ipmp_next) {
    ipmp_next = ipmp->next;
    ipmp->next = avail_init_pos_modifiers;
    avail_init_pos_modifiers = ipmp;
  }  /* for */
}  /* free_init_pos_modifier_list */


static an_init_pos_modifier_ptr copy_init_pos_modifier_list(
                                                 an_init_pos_modifier_ptr ipmp)
/*
Make a copy of an initialization position modifier list and return a pointer
to the copy.  This is used when a list made up of stack entries must be
saved so that a cleanup action may be generated later.
*/
{
  an_init_pos_modifier_ptr copy_ipmp;

  copy_ipmp = alloc_init_pos_modifier();
  *copy_ipmp = *ipmp;
  if (ipmp->next != NULL) {
    copy_ipmp->next = copy_init_pos_modifier_list(ipmp->next);
  }  /* if */
  return copy_ipmp;
}  /* copy_init_pos_modifier_list */


void clear_init_pos_descr(an_init_pos_descr_ptr ipdp)
/*
Clear an initialization position description entry to default values.
*/
{
  ipdp->variable                  = NULL;
  ipdp->indirect_through_variable = FALSE;
  ipdp->base_type                 = NULL;
  ipdp->modifiers                 = NULL;
  ipdp->whole_array               = FALSE;
  ipdp->array_element_count       = 0;
}  /* clear_init_pos_descr */


void set_var_init_pos_descr(a_variable_ptr        var,
                            an_init_pos_descr_ptr ipdp)
/*
Make an initialization position description entry for the variable var.
*/
{
  clear_init_pos_descr(ipdp);
  ipdp->variable = var;
  ipdp->base_type = var->type;
}  /* set_var_init_pos_descr */


void set_var_indirect_init_pos_descr(a_variable_ptr        var,
                                     an_init_pos_descr_ptr ipdp)
/*
Make an initialization position description entry for the object pointed
to by variable var.
*/
{
  clear_init_pos_descr(ipdp);
  ipdp->variable = var;
  ipdp->indirect_through_variable = TRUE;
  ipdp->base_type = type_pointed_to(var->type);
}  /* set_var_indirect_init_pos_descr */


a_type_ptr type_from_init_pos_descr(an_init_pos_descr_ptr ipdp)
/*
Return the type of the object indicated by ipdp.
*/
{
  a_type_ptr type;

  /* If the description has modifiers, then the type is that after the last
     modifier (the first on the list).  Otherwise, the type is the base
     type. */
  if (ipdp->modifiers != NULL) {
    type = ipdp->modifiers->type;
  } else {
    type = ipdp->base_type;
  }  /* if */
  return type;
}  /* type_from_init_pos_descr */


static a_boolean init_pos_is_static(an_init_pos_descr_ptr ipdp)
/*
Return TRUE if the indicated initialization position is for a static
variable (or a part of one).
*/
{
  a_boolean is_for_static_var = !ipdp->indirect_through_variable &&
                    has_static_storage_duration(ipdp->variable->storage_class);
  return is_for_static_var;
}  /* init_pos_is_static */


static void modify_ctor_init_pos_descr(a_constructor_init_ptr   ctor_init,
                                       an_init_pos_descr_ptr    ipdp,
                                       an_init_pos_modifier_ptr ipmp)
/*
Modify the initialization position description in *ipdp to describe the
entity being initialized by the constructor init entry ctor_init.  *ipdp is
already set to describe the base address of the entity, and this routine
adds the modifiers needed to get to the proper subobject of the entity.
ipmp points to a local variable in the caller that can be used for an
init position modifier.
*/
{
  switch (ctor_init->kind) {
    case cik_virtual_base_class:
    case cik_direct_base_class:
      /* Add a modifier that selects the base class relative to the "this"
         parameter. */
      add_init_pos_modifier(ipmp, ipdp);
      ipmp->curr_base = ctor_init->variant.base_class;
      ipmp->type = ctor_init->variant.base_class->type;
      break;
    case cik_field:
      /* Add a modifier that selects the field relative to the "this"
         parameter. */
      add_init_pos_modifier(ipmp, ipdp);
      ipmp->curr_field = ctor_init->variant.field;
      ipmp->type = ctor_init->variant.field->type;
      break;
#if CHECKING
    default:
      internal_error("modify_ctor_init_pos_descr: bad kind");
#endif /* CHECKING */
  }  /* switch */
}  /* modify_ctor_init_pos_descr */


static void develop_ctor_init_pos_descr(
                                       a_constructor_init_ptr   ctor_init,
                                       a_variable_ptr           this_param_var,
                                       an_init_pos_descr_ptr    ipdp,
                                       an_init_pos_modifier_ptr ipmp)
/*
Develop an initialization position description in *ipdp to describe the
entity being initialized by the constructor init entry ctor_init.
this_param_var points to the "this" parameter variable for the constructor.
ipmp points to a local variable in the caller that can be used for an init
position modifier.
*/
{
  set_var_indirect_init_pos_descr(this_param_var, ipdp);
  modify_ctor_init_pos_descr(ctor_init, ipdp, ipmp);
}  /* develop_ctor_init_pos_descr */


static an_expr_node_ptr drop_const_on_init_entity_node(
                                             an_expr_node_ptr      entity_node,
                                             an_init_pos_descr_ptr ipdp)
/*
The entity whose address is given by the expression entity_node is to
be initialized by executable code.  If it is "const", drop the const by
casting so the entity can be written to.  ipdp is the init position
description for the complete entity being initialized.  entity_node
cannot be a bitfield selection.
*/
{
  a_type_ptr entity_type = type_pointed_to(entity_node->type);

  if (is_const_qualified_type(entity_type)) {
    entity_type = make_unqualified_type(entity_type);
    entity_node = add_cast(entity_node, make_pointer_type(entity_type));
    /* Because of the cast, we're using the object's address as a real
       address, not just as an lvalue address, so set the address taken
       flag if appropriate.  Note that the interpretation of the
       address_taken flag has changed a few times, so the processing
       here is conservative -- it sets the flag in all cases, which
       guarantees it will work. */
    if (!ipdp->indirect_through_variable) {
      set_variable_address_taken(ipdp->variable);
    }  /* if */
  }  /* if */
  return entity_node;
}  /* drop_const_on_init_entity_node */


static an_expr_node_ptr modify_init_entity_node(
                                        an_expr_node_ptr         entity_node,
                                        an_init_pos_descr_ptr    ipdp,
                                        an_init_pos_modifier_ptr modifiers,
                                        a_boolean                using_as_dest)
/*
Add the address modifiers from the list given by "modifiers" (from the
init position description ipdp) to the entity address expression "entity_node"
and return a pointer to the modified expression tree.  If using_as_dest is
TRUE, the entity is the destination of an initialization operation.
*/
{
  an_expr_node_ptr elem_num_node;

  /* If there are no modifiers, return the original node. */
  if (modifiers != NULL) {
    /* Process the modifiers preceding the final modifier, then add the final
       qualifier (recall that the modifiers are in order from the innermost
       to the outermost). */
    entity_node = modify_init_entity_node(entity_node, ipdp, modifiers->next,
                                          using_as_dest);
    /* Add the final modifier. */
    if (modifiers->curr_field != NULL) {
      /* Add a field selection.  ("au_" for possibly from anonymous union.) */
      a_field_ptr field = modifiers->curr_field;
      entity_node = au_field_lvalue_selection_expr(entity_node, field);
      /* We don't drop const from the type here.  Most back ends won't care
         if we assign to a const member, the C-generating back end drops
         const on member declarations, and there's no way to rewrite
         bitfield cases anyway (because you can't take their addresses). */
    } else if (modifiers->curr_base != NULL) {
      /* Add a base class selection. */
      entity_node = make_base_class_lvalue(entity_node, modifiers->curr_base,
                                           /*complete_object=*/FALSE);
    } else {
      /* Add an array element selection. */
      /* Do the pointer decay from array to pointer to element. */
      a_type_ptr elem_type = array_element_type(type_pointed_to(
                                                           entity_node->type));
      if (using_as_dest) {
        /* The entity will be used as the destination of an initialization, so
           drop "const" (if present) from the type to make it modifiable. */
        elem_type = make_unqualified_type(elem_type);
        /* The address_taken flag on the underlying variable is already
           set appropriately. */
      }  /* if */
      entity_node = add_cast(entity_node, make_pointer_type(elem_type));
      if (modifiers->curr_elem != 0) {
        /* Add the subscript if it's non-zero. */
        elem_num_node = node_for_integer_constant((long)modifiers->curr_elem,
                                                  targ_size_t_int_kind);
        entity_node->next = elem_num_node;
        entity_node = make_operator_node((an_expr_operator_kind)eok_padd_subsc,
                                         entity_node->type, entity_node);
      }  /* if */
    }  /* if */
  }  /* if */
  return entity_node;
}  /* modify_init_entity_node */


an_expr_node_ptr make_init_entity_node(an_init_pos_descr_ptr ipdp,
                                       a_boolean             using_as_address,
                                       a_boolean             using_as_dest)
/*
Make an expression for the entity described by ipdp, as an lvalue, and
return a pointer to it.  If using_as_address is TRUE, the expression will
be used as an address (and that means really as an address that escapes,
not simply as an address because it's an lvalue).  If using_as_dest is
TRUE, the entity is the destination of an initialization operation.
*/
{
  an_expr_node_ptr entity_node;

  /* Make a node for the base address. */
  if (ipdp->indirect_through_variable) {
    /* Indirect through the variable. */
    entity_node = var_rvalue_expr(ipdp->variable);
  } else {
    /* Normal case, a simple variable. */
    entity_node = var_lvalue_expr(ipdp->variable);
    /* If we will be using this expression as an address, set the address-taken
       flag in the variable. */
    if (using_as_address) set_variable_address_taken(ipdp->variable);
  }  /* if */
  if (using_as_dest) {
    /* The entity will be used as the destination of an initialization, so
       drop "const" (if present) from the type to make it modifiable. */
    entity_node = drop_const_on_init_entity_node(entity_node, ipdp);
  }  /* if */
  /* Add the modifiers to the base address. */
  entity_node = modify_init_entity_node(entity_node, ipdp, ipdp->modifiers,
                                        using_as_dest);
  return entity_node;
}  /* make_init_entity_node */


static void add_init_assignment(a_dynamic_init_ptr     dip,
                                an_expr_node_ptr       entity_node,
                                an_insert_location_ptr insert_location)
/*
Make an assignment statement to implement the dynamic initialization
described by dip.  entity_node is an expression that gives the address
of the entity to be initialized.  Insert the statement at *insert_location
and update *insert_location.  The constant or expression initial value
pointed to by dip is lowered.
*/
{
  an_expr_node_ptr      init_val_node;
  a_statement_ptr       assign_stmt;
  an_expr_operator_kind op;

  switch (dip->kind) {
    case dik_constant:
      /* Assign a constant to the entity to be initialized. */
      /* The constant has already been lowered. */
      init_val_node = make_node_for_il_constant(dip->variant.constant);
      break;
    case dik_expression:
      /* Assign an expression to the entity to be initialized. */
      /* The expression has already been lowered. */
      init_val_node = dip->variant.expression;
      break;
#if CHECKING
    default:
      internal_error("add_init_assignment: bad kind");
#endif /* CHECKING */
  }  /* switch */
  /* Make an assignment statement.  Note that we know that no constructor
     (copy or other) is involved because we have this kind of dynamic
     initialization.  We also know the thing being initialized is not an
     array. */
  op = lowered_assignment_operator(init_val_node->type);
  assign_stmt = insert_assignment_statement(entity_node, op, init_val_node,
                                            insert_location);
  set_stmt_pos_to_code_pos_for_lowering(assign_stmt);
}  /* add_init_assignment */


static a_variable_ptr var_for_copy_constructor_source(void)
/*
We are currently expanding the body of a copy constructor.  Return a pointer
for the source parameter of the copy constructor.
*/
{
  a_routine_ptr    curr_routine;
  a_variable_ptr   source_param_var;
  a_type_ptr       class_type;
  a_base_class_ptr bcp;

  curr_routine = nearest_function_scope->variant.routine.ptr;
#if CHECKING
  if (curr_routine->special_kind != (a_special_function_kind)sfk_constructor) {
    internal_error(
             "var_for_copy_constructor_source: curr routine not constructor");
  }  /* if */
#endif /* CHECKING */
  source_param_var = nearest_function_scope->variant.routine.parameters->next;
#if CHECKING
  if (source_param_var == NULL) {
    internal_error("var_for_copy_constructor_source: source param missing");
  }  /* if */
#endif /* CHECKING */
  /* Skip over any parameters added for virtual base class pointers.
     See add_constructor_params. */
  class_type = curr_routine->source_corresp.class_of_which_a_member;
  if (class_type->variant.class_struct_union.any_virtual_base_classes) {
    for (bcp = class_type->variant.class_struct_union.extra_info->base_classes;
         bcp != NULL;
         bcp = bcp->next) {
      if (bcp->is_virtual) {
        source_param_var = source_param_var->next;
#if CHECKING
        if (source_param_var == NULL) {
          internal_error(
                  "var_for_copy_constructor_source: source param missing (2)");
        }  /* if */
#endif /* CHECKING */
      }  /* if */
    }  /* for */
  }  /* if */
  return source_param_var;
}  /* var_for_copy_constructor_source */


static an_expr_node_ptr implied_source_of_copy(
                                       a_constructor_init_ptr ctor_init,
                                       an_init_pos_descr_ptr  dest,
                                       a_boolean              using_as_address)
/*
We're processing a dynamic initialization entry that represents a copy of
something from an implied source location to the thing being initialized.
If ctor_init is non-NULL, it points to a constructor-initializer entry
that indicates a copy of a member of a class; if ctor_init is NULL, the
copy is of the object thrown by an exception handling "throw" into the
parameter of the catch clause.  In either case, create an expression to
describe the address of the implied source and return a pointer to it.
dest describes the entity being initialized.  If using_as_address is TRUE,
the expression will be used as an address (and that means really as an
address that escapes, not simply as an address because it's an lvalue).
*/
{
  an_expr_node_ptr     source_node;
  an_init_pos_descr    cctor_source_ipd;
  an_init_pos_modifier cctor_source_ipm;
  a_variable_ptr       catch_parameter, caught_object_addr;
  a_type_ptr           param_type;

  if (ctor_init != NULL) {
    /* The implied source is the member being copied by the
       ctor-initializer. */
    set_var_indirect_init_pos_descr(var_for_copy_constructor_source(),
                                    &cctor_source_ipd);
    modify_ctor_init_pos_descr(ctor_init, &cctor_source_ipd,
                               &cctor_source_ipm);
    source_node = make_init_entity_node(&cctor_source_ipd, using_as_address,
                                        /*using_as_dest=*/FALSE);
  } else {
    /* The implied source is the address in __caught_object_address. */
    caught_object_addr = make_caught_object_address_var();
    /* We expect a simple catch parameter as the destination. */
    check_assertion(dest->modifiers == NULL &&
                    !dest->indirect_through_variable);
    catch_parameter = dest->variable;
    param_type = catch_parameter->type;
    if (is_reference_type(param_type)) {
      /* Initializing a reference parameter, so copy the pointer into
         the parameter, instead of copying the object pointed to. */
      source_node = var_lvalue_expr(caught_object_addr);
      /* Set the address_taken flag if the variable address escapes.  This
         is for completeness; it would be strange for this routine to be
         called with using_as_address TRUE for this case. */
      if (using_as_address) set_variable_address_taken(caught_object_addr);
     } else {
      /* Normal case (not a reference). */
      source_node = var_rvalue_expr(caught_object_addr);
    }  /* if */
    /* Cast the source node a pointer to the type of thing to be copied. */
    source_node = add_cast_if_necessary(source_node,
                                        make_pointer_type(param_type));
  }  /* if */
  return source_node;
}  /* implied_source_of_copy */


static void add_bitwise_copy(an_init_pos_descr_ptr  dest,
                             a_constructor_init_ptr ctor_init,
                             an_insert_location_ptr insert_location)
/*
Generate code to implement an initialization by bitwise copy.  dest
describes the destination of the move.  ctor_init is the constructor
initialization entry, or is NULL if this is the initialization of
a catch clause parameter.  Insert the statement at *insert_location
and update *insert_location.
*/
{
  an_expr_node_ptr      source_node, dest_node;
  a_type_ptr            type;
  an_expr_operator_kind op;

  /* Make an expression for the address of the destination entity. */
  /* Note that using_as_address is FALSE even for the block copy case,
     because the address doesn't escape. */
  dest_node = make_init_entity_node(dest, /*using_as_address=*/FALSE,
                                    /*using_as_dest=*/TRUE);
  /* Make an expression for the address of the source entity. */
  source_node = implied_source_of_copy(ctor_init, dest,
                                       /*using_as_address=*/FALSE);
  /* Make an assignment statement. */
  /* Choose the operation.  For simple types use the built-in operator.
     For other types use a block copy. */
  type = type_pointed_to(source_node->type);
  if (is_reference_type(type)) {
    /* Replace a reference type by a pointer type. */
    type = make_pointer_type(type_pointed_to(type));
  }  /* if */
  if (is_integral_type(type) ||
      is_floating_type(type) ||
      is_pointer_type(type) ||
      is_class_struct_union_type(type)) {
    op = lowered_assignment_operator(type);
    /* The normal assignment operators take an rvalue as the source, so
       change the node to an rvalue. */
    source_node = add_indirection_to_node(source_node);
  } else {
    /* For other kinds, use a block move. */
    op = (an_expr_operator_kind)eok_bassign;
  }  /* if */
  (void)insert_assignment_statement(dest_node, op, source_node,
                                    insert_location);
}  /* add_bitwise_copy */


void make_ctor_implied_arg_list(a_routine_ptr    ctor_routine,
                                an_expr_node_ptr *implied_arg_list,
                                an_expr_node_ptr *end_implied_arg_list)
/*
Build and return a list of the implied arguments to be added to a call of
the constructor ctor_routine.  There is one implied argument for each
virtual base class of the associated base class, and they are used to
ensure that each virtual base class is constructed only once.  The
beginning and end of the list are returned in *implied_arg_list and
*end_implied_arg_list.  For an empty list, both will be set to NULL.
*/
{
  an_expr_node_ptr implied_arg_node;
  a_type_ptr       class_type, subobject_type;
  a_base_class_ptr bcp;
  a_constant       null_constant;

  *implied_arg_list = *end_implied_arg_list = NULL;
  /* Get the class type. */
  class_type = ctor_routine->source_corresp.class_of_which_a_member;
  prelower_class_type(class_type);
  if (class_type->variant.class_struct_union.any_virtual_base_classes) {
    /* The class has at least one virtual base class. */
    for (bcp = class_type->variant.class_struct_union.extra_info->base_classes;
         bcp != NULL;
         bcp = bcp->next) {
      if (bcp->is_virtual) {
        /* Allocate an expression that is a NULL pointer to the virtual
           base class.  Use the type of the base class when used as a
           subobject. */
        subobject_type = bcp->type->variant.class_struct_union.extra_info->
                                                             type_as_subobject;
        make_zero_of_proper_type(make_pointer_type(subobject_type),
                                 &null_constant);
        implied_arg_node = alloc_node_for_constant(&null_constant);
        /* Add the node to the list. */
        if (*implied_arg_list == NULL) {
          *implied_arg_list = implied_arg_node;
        } else {
          (*end_implied_arg_list)->next = implied_arg_node;
        }  /* if */
        *end_implied_arg_list = implied_arg_node;
      }  /* if */
    }  /* for */
  }  /* if */
}  /* make_ctor_implied_arg_list */


void make_dtor_implied_arg_list(a_routine_ptr    dtor_routine,
                                a_boolean        have_complete_object,
                                an_expr_node_ptr *implied_arg_node)
/*
Build and return the implied argument to be added to a call of the destructor
dtor_routine.  A pointer to the argument is returned in *implied_arg_node,
or NULL if no implied argument is needed.  If have_complete_object is TRUE,
we know we are calling the destructor for a complete object.
*/
{
  a_type_ptr class_type;

  *implied_arg_node = NULL;
  /* Get the class type. */
  class_type = dtor_routine->source_corresp.class_of_which_a_member;
  prelower_class_type(class_type);
  /* 0x2 bit means "have complete object".  0x1 bit means "free storage"
     which does not apply here. */
  *implied_arg_node = node_for_integer_constant(have_complete_object ? 2L : 0L,
                                                (an_integer_kind)ik_int);
}  /* make_dtor_implied_arg_list */


static void add_constructor_call(a_dynamic_init_ptr     dip,
                                 an_expr_node_ptr       entity_node,
                                 an_expr_node_ptr       source_node,
                                 an_expr_node_ptr       implied_arg_list,
                                 an_expr_node_ptr       end_implied_arg_list,
                                 an_insert_location_ptr insert_location)
/*
Make a call statement that invokes a constructor as required in the dynamic
initialization entry pointed to by dip.  entity_node is an expression
that gives the address of the entity to be initialized.  If source_node
is non-NULL, it points to an expression that is the source for a copy
constructor call.  Both entity_node and source_node have already been
cast to the proper type for the corresponding parameter to eliminate
qualifier and type-as-subobject differences.  implied_arg_list is a
list of implied extra virtual base class pointer arguments for the
constructor, or NULL if this routine should generate them if required.
Insert the statement at *insert_location and update *insert_location.
The additional-arguments list given by dip->variant.constructor.args has
already been lowered.
*/
{
  a_routine_ptr    ctor_routine = dip->variant.constructor.ptr;
  an_expr_node_ptr last_node;
  a_statement_ptr  call_stmt;

#if CHECKING
  if (dip->kind != (a_dynamic_init_kind)dik_constructor) {
    internal_error("add_constructor_call: bad kind");
  }  /* if */
#endif /* CHECKING */
  /* If no implied_arg_list is supplied and the constructor needs one
     (because it initializes a class that has virtual base classes), make
     the implied_arg_list (all entries are NULL pointer values). */
  if (implied_arg_list == NULL) {
    make_ctor_implied_arg_list(ctor_routine, &implied_arg_list,
                               &end_implied_arg_list);
  }  /* if */
  /* Link the entity node, the implied arguments if any, the source node if
     any, and the other arguments together. */
  last_node = entity_node;
  if (implied_arg_list != NULL) {
    last_node->next = implied_arg_list;
    last_node = end_implied_arg_list;
  }  /* if */
  if (source_node != NULL) {
    last_node->next = source_node;
    last_node = source_node;
  }  /* if */
  last_node->next = dip->variant.constructor.args;
  /* Make an expression statement containing the call expression. */
  call_stmt = make_call_statement(ctor_routine, entity_node);
  set_stmt_pos_to_code_pos_for_lowering(call_stmt);
  /* Insert the statement at the right place. */
  insert_statement(call_stmt, insert_location);
}  /* add_constructor_call */


/*
Pointers to routine entries for runtime routines for call of a constructor,
copy constructor, or destructor for each element of an array, once created.
NULL until then.
*/
static a_routine_ptr
		vec_new_routine,
		vec_new_eh_routine,
		vec_cctor_routine,
		vec_delete_routine;


static an_expr_node_ptr num_elem_node_from_count(long array_element_count)
/*
Build an expression for a constant that represents the number of elements
in an array for an array new/delete call.  -1 indicates a variable-length
array.
*/
{
  an_expr_node_ptr num_elem_node;
  a_constant       num_elem_constant;

  set_integer_constant_with_overflow_check(&num_elem_constant,
                                           array_element_count,
                                           (an_integer_kind)ik_int);
  /* Allocate an expression node for the constant. */
  num_elem_node = alloc_node_for_constant(&num_elem_constant);
  return num_elem_node;
}  /* num_elem_node_from_count */


static a_type_ptr new_delete_base_type_from_operation_type(a_type_ptr type)
/*
type is the type operated on in a new or delete operation.
Extract and return the underlying entity type.
*/
{
  a_type_ptr base_type;

  base_type = type;
  /* For multi-dimensional array cases, drop down to the underlying class
     type. */
  while (is_array_type(base_type)) {
    base_type = array_element_type(base_type);
  }  /* while */
  base_type = skip_typerefs(base_type);
  return base_type;
}  /* new_delete_base_type_from_operation_type */


static an_expr_node_ptr size_elem_node_from_pointer_type(a_type_ptr ptr_type)
/*
Build an expression node for the constant that is the size of the element
type of the array pointed to by ptr_type, and return a pointer to it.
*/
{
  a_type_ptr       elem_type;
  an_expr_node_ptr size_elem_node;

  elem_type = new_delete_base_type_from_operation_type(
                                                    type_pointed_to(ptr_type));
  size_elem_node = node_for_integer_constant((long)elem_type->size,
                                             targ_size_t_int_kind);
  return size_elem_node;
}  /* size_elem_node_from_pointer_type */


static an_expr_node_ptr make_vec_new_call(an_expr_node_ptr entity_node,
                                          an_expr_node_ptr num_elem_node,
                                          a_routine_ptr    ctor_routine,
                                          a_routine_ptr    dtor_routine)
/*
Make a call to a runtime routine (__vec_new) that will allocate an array
and call a constructor for each element of the array.  entity_node gives
the address of the array (for cases where the array is already
allocated).  num_elem_node gives (as an expression) the number of
elements in the array.  ctor_routine is the constructor routine to be
called, or NULL if no constructor is to be called.  dtor_routine
is the destructor routine to be called -- this is non-NULL only if
there is a destructor and is used only if exceptions are enabled
(in that case, it may be necessary to destroy array elements that
were created if a throw occurs halfway through the initialization of
the array); the runtime routine __vec_new_eh is called in that case.
A pointer to the expression created is returned.
*/
{
  an_expr_node_ptr call_node, arg_expr_list, size_elem_node;
  an_expr_node_ptr func_addr_node;
  a_constant       null_constant;
  a_type_ptr       gen_func_ptr_type;

  /* Build a constant node for the size of the array elements. */
  size_elem_node = size_elem_node_from_pointer_type(entity_node->type);
  gen_func_ptr_type = make_vptp_type();
  if (ctor_routine != NULL) {
    func_addr_node = function_addr_expr(ctor_routine,
                                        /*set_address_taken_flag=*/TRUE);
    /* Cast the function pointer to the generic function type. */
    func_addr_node = add_cast_if_necessary(func_addr_node, gen_func_ptr_type);
  } else {
    /* No constructor routine to call; use 0 cast to the right function
       pointer type. */
    make_zero_of_proper_type(gen_func_ptr_type, &null_constant);
    func_addr_node = alloc_node_for_constant(&null_constant);
  }  /* if */
  /* The call looks like
       __vec_new   (entity_node, num_elems, size_elem, ctor_routine)
       __vec_new_eh(entity_node, num_elems, size_elem, ctor_routine,
                                                       dtor_routine)
  */
  arg_expr_list = entity_node;
  entity_node->next = num_elem_node;
  num_elem_node->next = size_elem_node;
  size_elem_node->next = func_addr_node;
  if (exceptions_enabled && dtor_routine != NULL) {
    /* __vec_new_eh call, with destructor. */
    an_expr_node_ptr dtor_addr_node = function_addr_expr(dtor_routine,
                                              /*set_address_taken_flag=*/TRUE);
    dtor_addr_node = add_cast_if_necessary(dtor_addr_node, gen_func_ptr_type);
    func_addr_node->next = dtor_addr_node;
    call_node = make_runtime_rout_call("__vec_new_eh", &vec_new_eh_routine,
                                       void_star_type(), arg_expr_list);
  } else {
    /* __vec_new call, without destructor. */
    call_node = make_runtime_rout_call("__vec_new", &vec_new_routine,
                                       void_star_type(), arg_expr_list);
  }  /* if */
  return call_node;
}  /* make_vec_new_call */


static an_expr_node_ptr make_vec_delete_call(
                                          an_expr_node_ptr entity_node,
                                          long             array_element_count,
                                          a_routine_ptr    dtor_routine,
                                          a_boolean        free_storage)
/*
Make a call to a runtime routine (__vec_delete) that will call a
destructor for each element of an array and then deallocate the array.
entity_node gives the address of the array.  array_element_count is the
number of elements in the array, or -1 for a variable-length array.
dtor_routine is the destructor routine to be called, or NULL if no
destructor is to be called.  free_storage is TRUE if the storage for the
array is to be freed.  A pointer to the expression created is returned.
*/
{
  an_expr_node_ptr call_node, arg_expr_list, num_elem_node, size_elem_node;
  an_expr_node_ptr func_addr_node, free_storage_node;
  a_constant       null_constant;
  a_type_ptr       gen_func_ptr_type;

  /* Build a constant node for the number of array elements. */
  num_elem_node = num_elem_node_from_count(array_element_count);
  /* Build a constant node for the size of the array elements. */
  size_elem_node = size_elem_node_from_pointer_type(entity_node->type);
  /* Build the "free_storage" argument: 1 to free storage, 0 otherwise. */
  free_storage_node = node_for_integer_constant(free_storage ? 1L : 0L,
                                                (an_integer_kind)ik_int);
  gen_func_ptr_type = make_vptp_type();
  if (dtor_routine != NULL) {
    func_addr_node = function_addr_expr(dtor_routine,
                                        /*set_address_taken_flag=*/TRUE);
    /* Cast the function pointer to the generic function type. */
    func_addr_node = add_cast_if_necessary(func_addr_node, gen_func_ptr_type);
  } else {
    /* No destructor routine to call; use 0 cast to the right function
       pointer type. */
    make_zero_of_proper_type(gen_func_ptr_type, &null_constant);
    func_addr_node = alloc_node_for_constant(&null_constant);
  }  /* if */
  /* The call looks like
       __vec_delete(entity_node, num_elems, size_elem, dtor_routine,
                    free_storage, 0)
     The final argument is never used.  It's there for cfront compatibility.
  */
  arg_expr_list = entity_node;
  entity_node->next = num_elem_node;
  num_elem_node->next = size_elem_node;
  size_elem_node->next = func_addr_node;
  func_addr_node->next = free_storage_node;
  free_storage_node->next = node_for_integer_constant(0L,
                                                      (an_integer_kind)ik_int);
  call_node = make_runtime_rout_call("__vec_delete", &vec_delete_routine,
                                     void_type(), arg_expr_list);
  return call_node;
}  /* make_vec_delete_call */


static an_expr_node_ptr make_vec_cctor_call(
                                          an_expr_node_ptr entity_node,
                                          an_expr_node_ptr source_node,
                                          long             array_element_count,
                                          a_routine_ptr    cctor_routine)
/*
Make a call to a runtime routine (__vec_cctor) that will call a copy
constructor for each element of an array.  entity_node gives the address
of the array.  source_node gives the source for the copy.
array_element_count is the number of elements in the array.
cctor_routine is the copy constructor routine to be called.
A pointer to the expression created is returned.
*/
{
  an_expr_node_ptr call_node, arg_expr_list, num_elem_node, size_elem_node;
  an_expr_node_ptr func_addr_node;

  /* Build a constant node for the number of array elements. */
  num_elem_node = num_elem_node_from_count(array_element_count);
  /* Build a constant node for the size of the array elements. */
  size_elem_node = size_elem_node_from_pointer_type(entity_node->type);
  /* Build a node for the address of the copy constructor. */
  func_addr_node = function_addr_expr(cctor_routine,
                                      /*set_address_taken_flag=*/TRUE);
  /* Cast the function pointer to the generic function type. */
  func_addr_node = add_cast_if_necessary(func_addr_node, make_vptp_type());
  /* The call looks like
       __vec_cctor(entity_node, num_elems, size_elem, cctor_routine,
                   source_node)
  */
  arg_expr_list = entity_node;
  entity_node->next = num_elem_node;
  num_elem_node->next = size_elem_node;
  size_elem_node->next = func_addr_node;
  func_addr_node->next = source_node;
  call_node = make_runtime_rout_call("__vec_cctor", &vec_cctor_routine,
                                     void_type(), arg_expr_list);
  return call_node;
}  /* make_vec_cctor_call */


static a_routine_ptr default_version_of_routine(
                                             a_routine_ptr    routine,
                                             an_expr_node_ptr default_arg_list)
/*
Return a pointer to a routine that does the same thing as "routine" but
in which the parameters that have default argument expressions have been
removed.  The values to be used for those default arguments are given
by default_arg_list (the expressions are NOT already lowered; this is
important, since they have to be copied, and you can't successfully copy
a lowered expression, since it might have temporaries in it).  Implicitly-
generated parameters of constructors and destructors are also removed.
This is used to generate a version of a constructor or destructor that
can be called with just a "this" parameter, or of a copy constructor
that can be called with just a "this" parameter and a source pointer.
The routine must have a "this" parameter.
*/
{
  a_memory_region_number
                   region_to_switch_back_to = curr_il_region_number;
  an_expr_node_ptr implied_arg_list = NULL, end_implied_arg_list = NULL;
  an_expr_node_ptr call_node;
  a_routine_ptr    new_routine;
  a_type_ptr       routine_type = skip_typerefs(routine->type);
  a_type_ptr       this_param_type, pass_through_param_type;
  a_param_type_ptr src_param_type, param_type, last_param_type;
  a_routine_type_supplement_ptr
                   rtsp, new_rtsp;
  a_scope_ptr      new_routine_scope;
  an_insert_location
                   insert_location;
  a_memory_region_number
                   new_routine_il_region;
  a_variable_ptr   this_param_var, param_var, last_param_var;
  an_expr_node_ptr this_arg, pass_through_arg;
  a_statement_ptr  call_stmt, return_stmt;
  a_context        context;

  /* Determine any implicit arguments required for a constructor or
     destructor. */
  if (routine->special_kind == (a_special_function_kind)sfk_constructor) {
    make_ctor_implied_arg_list(routine, &implied_arg_list,
                               &end_implied_arg_list);
  } else if (routine->special_kind ==
                                     (a_special_function_kind)sfk_destructor) {
    make_dtor_implied_arg_list(routine, /*have_complete_object=*/TRUE,
                               &implied_arg_list);
    end_implied_arg_list = implied_arg_list;
  }  /* if */
  if (default_arg_list != NULL || implied_arg_list != NULL) {
    /* There are some implicit or default arguments, so a wrapper routine
       must be created and used in place of the original routine. */
    /* Make a type and routine entry for the routine. */
    /* Note that the routine has no name. */
    /* The "this" parameter is generated in its lowered form (i.e., as a
       normal parameter). */
    rtsp = routine_type->variant.routine.extra_info;
    this_param_type = rtsp->implicit_this_param_type;
#if CHECKING
    /* The routine must have a "this" parameter. */
    if (this_param_type == NULL) {
      internal_error("default_version_of_routine: missing this param");
    }  /* if */
#endif /* CHECKING */
    /* Additional parameter types, if any, are added below. */
    new_routine = make_rout_entry((char *)NULL, (a_storage_class)sc_static,
                                  routine_type->variant.routine.return_type,
                                  this_param_type);
    new_rtsp = new_routine->type->variant.routine.extra_info;
#if CHECKING
    /* The routine is not allowed to be one that returns its value via
       a pointer provided by the caller (the extra code for that case
       is not implemented). */
    if (rtsp->value_returned_by_cctor) {
      internal_error("default_version_of_routine: return value ptr");
    }  /* if */
#endif /* CHECKING */
    /* Make a memory region, scope, and block for the routine definition. */
    new_routine_scope = make_routine_definition(new_routine,
                                                /*make_return=*/FALSE,
                                                &new_routine_il_region);
    switch_il_region(new_routine_il_region);
    push_context(&context, new_routine_scope, /*subscope_region=*/FALSE);
    /* Make a parameter variable for the "this" parameter (again, in lowered
       form as a normal parameter). */
    new_routine_scope->variant.routine.parameters = this_param_var =
                                  make_lowered_param_variable(this_param_type);
    this_param_var->assoc_param_type = new_rtsp->param_type_list;
    this_param_var->is_this_parameter = TRUE;
    /* Make any additional parameter types and parameter vars beyond the
       "this" parameter (this comes up, for instance, on the copy
       constructor case). */
    src_param_type = unlowered_param_type_list(routine_type);
    last_param_type = new_rtsp->param_type_list;
    last_param_var = this_param_var;
    /* Do not process parameters with default argument values, since they
       are removed from the routine's interface. */
    for (; src_param_type != NULL && !src_param_type->has_default_arg;
         src_param_type = src_param_type->next) {
      pass_through_param_type = src_param_type->type;
      /* If the parameter is passed via a copy constructor and it has
         not been lowered, replace it by a pointer to the object.
         Note that a second copy constructor call (i.e., one within
         the generated routine) is not necessary. */
      if (src_param_type->passed_via_copy_constructor &&
          !visited_yet(src_param_type)) {
        pass_through_param_type = make_pointer_type(pass_through_param_type);
      }  /* if */
      param_type = alloc_param_type(pass_through_param_type);
      /* It is not necessary to clear il_lowering_flag; the entry does not need
         to be lowered.  Also note that the parameter types will be lowered
         when the original function is lowered, and do not need to be
         lowered here. */
      last_param_type->next = param_type;
      param_var = make_lowered_param_variable(pass_through_param_type);
      param_var->assoc_param_type = param_type;
      last_param_var->next = param_var;
      /* Add a reference to the parameter to the argument list to be used
         to call the original function.  This passes the parameter through
         unchanged.  Note that parameters of this type follow the
         implicit arguments, if any. */
      pass_through_arg = var_rvalue_expr(param_var);
      if (implied_arg_list == NULL) {
        implied_arg_list = pass_through_arg;
      } else {
        end_implied_arg_list->next = pass_through_arg;
      }  /* if */
      end_implied_arg_list = pass_through_arg;
      last_param_type = param_type;
      last_param_var = param_var;
    }  /* for */
    if (default_arg_list != NULL) {
      /* Copy the default argument expressions into the function memory
         region. */
      default_arg_list = copy_list_of_expr_trees(default_arg_list);
      /* Lower the default argument expressions.  Note that this must be done
         after the copy because you can't copy an expression once it has been
         lowered -- temporaries might have been added. */
      lower_arg_expr_list(default_arg_list, routine_type, src_param_type);
    }  /* if */
    if (implied_arg_list != NULL) {
      /* Add the implicit arguments to the front of the default argument
         list. */
      end_implied_arg_list->next = default_arg_list;
      default_arg_list = implied_arg_list;
    }  /* if */
    /* Add the "this" parameter at the front of the argument list. */
    this_arg = var_rvalue_expr(this_param_var);
    this_arg->next = default_arg_list;
    /* Make a call statement that calls the original routine with all
       the implicit arguments, i.e., that passes all the extra arguments
       to the original routine. */
    call_node = make_call_node(routine, this_arg, /*honor_virtual=*/FALSE);
    set_block_start_insert_location(new_routine_scope->assoc_block,
                                    &insert_location);
    /* If the routine has a void type, insert a statement for the call
       followed by a return statement.  Otherwise, attach the call directly
       to the return. */
    if (is_void_type(call_node->type)) {
      /* Insert the statement at the right place. */
      call_stmt = alloc_expr_statement(call_node);
      insert_statement(call_stmt, &insert_location);
      call_node = NULL;
    }  /* if */
    return_stmt = alloc_statement((a_statement_kind)stmk_return);
    return_stmt->expr = call_node;
    insert_statement(return_stmt, &insert_location);
    pop_context();
    done_with_memory_region(new_routine_il_region);
    switch_il_region(region_to_switch_back_to);
    routine = new_routine;
  }  /* if */
  return routine;
}  /* default_version_of_routine */


static void add_array_constructor_call(
                                   a_dynamic_init_ptr     dip,
                                   an_expr_node_ptr       entity_node,
                                   an_expr_node_ptr       source_node,
                                   long                   array_element_count,
                                   an_insert_location_ptr insert_location)
/*
Generate code that calls a constructor for each element of an array.
dip indicates the initialization to be performed; entity_node gives the
address of the array; source_node (if non-NULL) gives the address of
the source for a copy constructor call; and array_element_count gives the
number of elements in the array.  Insert the statements at *insert_location
and update *insert_location.  The additional-arguments list given by
dip->variant.constructor.args must NOT already be lowered (see comment
in default_version_of_routine).
*/
{
  a_routine_ptr    ctor_routine;
  an_expr_node_ptr call_node, num_elem_node;
  a_statement_ptr  call_stmt;

#if CHECKING
  if (dip->kind != (a_dynamic_init_kind)dik_constructor) {
    internal_error("add_array_constructor_call: not dik_constructor");
  }  /* if */
#endif /* CHECKING */
  ctor_routine = dip->variant.constructor.ptr;
  ctor_routine = default_version_of_routine(ctor_routine,
                                            dip->variant.constructor.args);
  if (source_node != NULL) {
    /* Copy constructor case. */
    call_node = make_vec_cctor_call(entity_node, source_node,
                                    array_element_count, ctor_routine);
  } else {
    /* Normal constructor case. */
    /* Build a constant node for the number of array elements. */
    num_elem_node = num_elem_node_from_count(array_element_count);
    call_node = make_vec_new_call(entity_node, num_elem_node, ctor_routine,
                                  dip->destructor);
  }  /* if */
  /* Make a statement containing the call. */
  call_stmt = alloc_expr_statement(call_node);
  set_stmt_pos_to_code_pos_for_lowering(call_stmt);
  /* Insert the statement at the right location. */
  insert_statement(call_stmt, insert_location);
}  /* add_array_constructor_call */


static void add_destructor_call(a_dynamic_init_ptr     dip,
                                an_expr_node_ptr       entity_node,
                                a_boolean              have_complete_object,
                                an_insert_location_ptr insert_location)
/*
Make a call statement that invokes a destructor as required in the dynamic
initialization entry pointed to by dip.  entity_node is an expression
that gives the address of the entity to be destroyed.  have_complete_object
is TRUE if the entity is a complete object.  Insert the statement at
*insert_location and update *insert_location.  The call generated is
not a virtual call even if the destructor is virtual.
*/
{
  a_routine_ptr    dtor_routine = dip->destructor;
  a_statement_ptr  call_stmt;
  an_expr_node_ptr implied_arg_node;
  a_type_ptr       this_param_type;

#if CHECKING
  if (dtor_routine == NULL) {
    internal_error("add_destructor_call: destructor == NULL");
  }  /* if */
#endif /* CHECKING */
  /* Cast the entity node pointer to the right type.  It might be a pointer
     to the class type-as-subobject. */
  this_param_type = implicit_this_param_type_of(dtor_routine->type);
  entity_node = add_cast_if_necessary(entity_node,
                                      f_skip_typerefs(this_param_type));
  /* If the destructor is for a class that has virtual base classes, add
     the implicit complete-object argument. */
  make_dtor_implied_arg_list(dtor_routine, have_complete_object,
                             &implied_arg_node);
  entity_node->next = implied_arg_node;
  /* Make an expression statement containing the call expression. */
  call_stmt = make_call_statement(dtor_routine, entity_node);
  set_stmt_pos_to_code_pos_for_lowering(call_stmt);
  /* Insert the statement at the right place. */
  insert_statement(call_stmt, insert_location);
}  /* add_destructor_call */


static void add_array_destructor_call(
                                   a_dynamic_init_ptr     dip,
                                   an_expr_node_ptr       entity_node,
                                   long                   array_element_count,
                                   an_insert_location_ptr insert_location)
/*
Generate code that calls a destructor for each element of an array.
dip indicates the destruction to be performed; entity_node gives the
address of the array; and array_element_count gives the number of elements
in the array.  Insert the statements at *insert_location and update
*insert_location.
*/
{
  a_routine_ptr    dtor_routine;
  an_expr_node_ptr call_node;
  a_statement_ptr  call_stmt;

#if CHECKING
  if (dip->destructor == NULL) {
    internal_error("add_array_destructor_call: no destructor");
  }  /* if */
#endif /* CHECKING */
  dtor_routine = dip->destructor;
  /* default_version_of_routine is not called on purpose; __vec_delete
     knows about the implicit argument for destructors and generates
     it automatically. */
  /* Generate the __vec_delete call. */
  call_node = make_vec_delete_call(entity_node, array_element_count,
                                   dtor_routine, /*free_storage=*/FALSE);
  /* Make a statement containing the call. */
  call_stmt = alloc_expr_statement(call_node);
  set_stmt_pos_to_code_pos_for_lowering(call_stmt);
  /* Insert the statement at the right location. */
  insert_statement(call_stmt, insert_location);
}  /* add_array_destructor_call */


static void lower_ck_dynamic_init(a_constant_ptr         con_ptr,
                                  an_init_pos_descr_ptr  ipdp,
                                  a_boolean              dtor_case,
                                  a_constructor_init_ptr ctor_init,
                                  an_insert_location_ptr insert_location)
/*
Generate executable code to handle a ck_dynamic_init constant (pointed
to by con_ptr).  The entity to be initialized is described by ipdp.
The necessary statements are inserted at *insert_location and
*insert_location is updated.  If ipdp->whole_array is TRUE, this
call is handling all the elements of an array.  If dtor_case is TRUE, we
are generating a destructor wrapper; do the destruction indicated in
the dynamic init but ignore any initialization.  If the dynamic
initialization is part of a constructor initializer, ctor_init points
to the constructor-init entry.
*/
{
  a_constant_ptr next_con;
  a_boolean      keep_dynamic_init;
  a_type_ptr     desired_type;

  if (dtor_case) {
    /* In a destructor case, so the "initialization" is really
       destruction. */
    lower_destructor_dynamic_init(con_ptr->variant.dynamic_init, ipdp,
                                  (a_cleanup_action_ptr)NULL,
                                  /*have_complete_object=*/TRUE,
                                  insert_location);
  } else {
    /* Normal initialization. */
    lower_dynamic_init(con_ptr->variant.dynamic_init, ipdp,
                       /*is_expr_temporary=*/FALSE,
                       (an_expr_node_ptr)NULL, (an_expr_node_ptr)NULL,
                       ctor_init, insert_location, &keep_dynamic_init);
#if CHECKING
    if (keep_dynamic_init) {
      internal_error("lower_ck_dynamic_init: keep_dynamic_init unexpected");
    }  /* if */
#endif /* CHECKING */
  }  /* if */
  /* Overwrite the constant with a harmless constant of the right kind.
     It's just a place-holder that gets overwritten by the dynamic
     initialization. */
  desired_type = ipdp->modifiers->type;
  if (is_aggregate_or_union_type(desired_type)) {
    /* An aggregate is initialized with a ck_dynamic_init.  This can
       come up in something like
         complex v[6] = {1, complex(1,2), complex(), 4};
       (From the ARM, 12.6.1).  Fortunately, if there's one of these
       cases in a ck_aggregate, there can be no "normal" constants
       in the aggregate, and the whole aggregate will be thrown
       away.   For such a case, we could just leave the ck_dynamic_init
       constant as it is.  There is another case, however: a pointer-to-
       member-function is lowered into an aggregate, and that case
       will come here too.  To handle that, we change the
       ck_dynamic_init into an empty aggregate constant.  Note that
       if we wanted a fully general solution for the earlier case
       we would have to build a multi-level empty aggregate constant
       with a structure that matches the aggregate, but since the constant
       here is only used in the pointer-to-member-function case, we
       need do no more than the simplest change. */
    set_constant_kind(con_ptr, (a_constant_repr_kind)ck_aggregate);
  } else {
    /* Not an aggregate: a zero of the right type will be fine. */
    next_con = con_ptr->next;
    make_zero_of_proper_type(desired_type, con_ptr);
    con_ptr->next = next_con;
  }  /* if */
}  /* lower_ck_dynamic_init */


static void lower_dynamic_init_aggregate_constant(
                                   a_constant_ptr         aggr_const,
                                   an_init_pos_descr_ptr  ipdp,
                                   a_boolean              dtor_case,
                                   a_constructor_init_ptr ctor_init,
                                   an_insert_location_ptr insert_location,
                                   a_boolean              *keep_constant)
/*
aggr_const points to a ck_aggregate constant that contains one or more
ck_dynamic_init dynamic initializations.  The ck_aggregate constant is
the initial value for the entity described by ipdp.  If dtor_case is TRUE,
we are generating a destructor wrapper; do the destruction indicated in
the aggregate init but ignore any initialization.  If the dynamic
initialization is part of a constructor initializer, ctor_init points to
the constructor-init entry.  Insert statements to implement the
initialization at *insert_location and update *insert_location.  If there
are any (genuine) constants in the aggregate, set *keep_constant to TRUE.
*/
{
  an_init_pos_descr    ipd;
  an_init_pos_modifier ipm, *ipmp;
  a_type_ptr           aggr_type;
  a_constant_ptr       con_ptr, repeated_con;
  a_boolean            array_aggr;

  /* Mark the constant as visited.  This is necessary if the aggregate
     constant ends up being kept because something constant remains after
     the non-constant parts have been rewritten. */
  mark_as_visited(aggr_const);
  /* Determine the type of the aggregate being initialized. */
  aggr_type = type_from_init_pos_descr(ipdp);
  aggr_type = skip_typerefs(aggr_type);
  /* Start a new level in the init_pos_modifier chain. */
  ipd = *ipdp;
  ipmp = &ipm;
  add_init_pos_modifier(ipmp, &ipd);
  con_ptr = aggr_const->variant.aggregate.first_constant;
  /* Determine the type of the first element of the aggregate being
     initialized. */
  array_aggr = (aggr_type->kind == (a_type_kind)tk_array);
  if (array_aggr) {
    /* Array -- get the element type. */
    ipmp->curr_elem = 0;
    ipmp->type = aggr_type->variant.array.element_type;
  } else {
#if CHECKING
    if (!is_immediate_class_type(aggr_type)) {
      internal_error("lower_dynamic_init_aggregate_constant: bad aggr kind");
    }  /* if */
#endif /* CHECKING */
    /* Class, struct, or union -- get first field (nonstatic data member). */
    ipmp->curr_field = next_initializable_field(
                             aggr_type->variant.class_struct_union.field_list);
  }  /* if */
  /* Work through the list of constants, pairing each one with a member of
     the aggregate. */
  for (; con_ptr != NULL; con_ptr = con_ptr->next) {
    if (!array_aggr) {
      check_assertion_str(ipmp->curr_field != NULL,
             "lower_dynamic_init_aggregate_constant: have constant, no field");
      /* For class and struct initialization, get the type of the member
         next up to be initialized. */
      ipmp->type = ipmp->curr_field->type;
    }  /* if */
    /* Initialize one member of the aggregate (of type ipmp->type) with
       one constant (con_ptr). */
    if (con_ptr->kind == (a_constant_repr_kind)ck_dynamic_init) {
      /* Dynamic initialization. */
      lower_ck_dynamic_init(con_ptr, &ipd, dtor_case, ctor_init,
                            insert_location);
    } else if (con_ptr->kind == (a_constant_repr_kind)ck_init_repeat) {
      /* Repeated constant.  Must be initializing members of an array. */
#if CHECKING
      if (!array_aggr) {
        internal_error(
                 "lower_dynamic_init_aggregate_constant: repeat on non-array");
      }  /* if */
#endif /* CHECKING */
      repeated_con = con_ptr->variant.init_repeat.constant;
      /* Repeat the constant the right number of times.  It must be a
         ck_dynamic_init constant. */
#if CHECKING
      if (repeated_con->kind != (a_constant_repr_kind)ck_dynamic_init) {
        internal_error(
    "lower_dynamic_init_aggregate_constant: repeated con not ck_dynamic_init");
      }  /* if */
#endif /* CHECKING */
      ipd.whole_array = TRUE;
      ipd.array_element_count = con_ptr->variant.init_repeat.count;
      lower_ck_dynamic_init(repeated_con, &ipd, dtor_case, ctor_init,
                            insert_location);
    } else if (con_ptr->kind == (a_constant_repr_kind)ck_aggregate) {
      /* Aggregate constant initializing a member of an aggregate. */
      lower_dynamic_init_aggregate_constant(con_ptr, &ipd,
                                            dtor_case, ctor_init,
                                            insert_location, keep_constant);
    } else {
      /* Normal constant. */
      lower_constant(con_ptr);
      *keep_constant = TRUE;
    }  /* if */
    /* Find the next member in the aggregate. */
    if (array_aggr) {
      /* Array -- go on to next element. */
      ipmp->curr_elem++;
    } else {
      /* Class or struct -- go on to next field (nonstatic data member). */
      ipmp->curr_field = next_initializable_field(ipmp->curr_field->next);
    }  /* if */
    /* Loop while there are more constants. */
  }  /* for */
}  /* lower_dynamic_init_aggregate_constant */


void make_code_to_invoke_file_scope_init_routine(void)
/*
Make the code that will ensure that the file-scope initialization routine
(if any) is invoked at program startup.
*/
{
  a_type_ptr       func_type, struct_type, ptr_struct_type;
  a_type_ptr       ptr_func_type;
  a_targ_size_t    byte_offset;
  a_field_ptr      last_field;
  a_variable_ptr   link_var;
  a_constant_ptr   aggr_con, init_con1, init_con2, init_con3;
  a_memory_region_number
                   region_to_switch_back_to;

  /* Only generate the code if there is a startup routine. */
  if (file_scope_init_routine != NULL) {
    /* Create a __link variable pointing to a struct that points to the
       initialization routine, using the same form as cfront:
         void __sti__module_id() {...}
         void __std__module_id() {...}
         struct __linkl {
           struct __linkl *next;
           void           (*ctor)();
           void           (*dtor)();
         };
         static struct __linkl __link = {NULL, __sti__module_id, NULL};
       Note that the mechanism provides for a termination routine as well
       as a startup routine, but we don't make use of that part of it;
       the destructions for variables are put on a list of destructions
       to be done at program termination, by calling a runtime routine.
       The AT&T patch step will find the __link static variable
       and link it with other initialization code to be invoked by _main.
       Alternatively, the munch step will find the routines with names
       beginning "__sti__" and "__std__".
       Note that the AT&T approach uses "char" for "void" in all the
       above.
    */
    switch_to_file_scope_region(&region_to_switch_back_to);
    /* Make the __linkl struct type.  It doesn't actually have a name. */
    struct_type = alloc_type((a_type_kind)tk_struct);
    byte_offset = 0;
    last_field = NULL;
    /* field: struct __linkl *next; */
    ptr_struct_type = make_pointer_type(struct_type);
    make_lowered_field("next", ptr_struct_type, &byte_offset, struct_type,
                       &last_field);
    /* field: void (*ctor)(); */
    func_type = make_function_type(void_type(), (a_type_ptr)NULL);
    ptr_func_type = make_pointer_type(func_type);
    make_lowered_field("ctor", ptr_func_type, &byte_offset, struct_type,
                       &last_field);
    /* field: void (*dtor)(); */
    make_lowered_field("dtor", ptr_func_type, &byte_offset, struct_type,
                       &last_field);
    finish_class_type(struct_type, &byte_offset);
    add_to_front_of_file_scope_types_list(struct_type);
    /* Make the __link variable. */
    link_var = make_lowered_variable("__link", /*already_il_name=*/FALSE,
                                     struct_type, (a_storage_class)sc_static);
    /* Give the __link variable the initial value
         {NULL, __sti__module_id, NULL}
       If either routine does not exist, use a NULL instead. */
    aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
    link_var->init_kind = (an_init_kind)initk_static;
    link_var->initializer.constant = aggr_con;
    /* NULL for "next" field. */
    init_con1 = alloc_constant((a_constant_repr_kind)ck_address);
    make_zero_of_proper_type(ptr_struct_type, init_con1);
    /* Address of __sti__module_id for "ctor" field. */
    init_con2 = alloc_constant((a_constant_repr_kind)ck_address);
    if (file_scope_init_routine != NULL) {
      set_routine_address_constant(file_scope_init_routine, init_con2,
                                   /*set_address_taken_flag=*/TRUE);
      implicit_cast(init_con2, ptr_func_type);
    } else {
      /* No init routine.  Use NULL. */
      make_zero_of_proper_type(ptr_func_type, init_con2);
    }  /* if */
    /* NULL for "dtor" field. */
    init_con3 = alloc_constant((a_constant_repr_kind)ck_address);
    make_zero_of_proper_type(ptr_func_type, init_con3);
    /* Link the constants together under the ck_aggregate constant. */
    aggr_con->variant.aggregate.first_constant = init_con1;
    init_con1->next = init_con2;
    init_con2->next = init_con3;
    aggr_con->variant.aggregate.last_constant  = init_con3;
    switch_back_to_original_region(region_to_switch_back_to);
  }  /* if */
}  /* make_code_to_invoke_file_scope_init_routine */


/*
String made from the primary source file name and the current date/time,
used to generate a unique name for the initialization routine.
NULL until set by make_module_id.
*/
static char	*module_id;


static void change_non_id_characters(char *str)
/*
Change any non-identifier characters in the indicated string to underscores.
*/
{
  for (; *str != '\0'; str++) if (!isalnum((unsigned char)*str)) *str = '_';
}  /* change_non_id_characters */


static void make_module_id(void)
/*
Make a string that is based on the name of the current module and is used to
qualify static names that are put out as external names, to make them unique.
Set module_id to the string.  Do not make the string again if it has already
been made.
*/
{
  char     *file_name = il_header.primary_source_file->file_name;
  char     *date_time = il_header.time_of_compilation;
  sizeof_t file_name_len = strlen(file_name);

  if (module_id == NULL) {
    /* The identifier is made of the primary source file name plus the
       current date and time, with non-identifier characters changed to
       underscores. */
    module_id = alloc_general(file_name_len + 1 + strlen(date_time) + 1);
    (void)strcpy(module_id, file_name);
    module_id[file_name_len] = '_';
    (void)strcpy(module_id+file_name_len+1, date_time);
    /* Change non-identifier characters to "_". */
    change_non_id_characters(module_id);
  }  /* if */
}  /* make_module_id */


static a_routine_ptr make_file_scope_init_or_term_routine(
                                       char                   *prefix,
                                       an_insert_location_ptr insert_location,
                                       a_scope_ptr            *init_rout_scope,
                                       a_memory_region_number *il_region)
/*
Make a routine to do file-scope initialization or termination.  prefix is
the prefix for the name of the routine, or is NULL if the routine should
be unnamed.  Set *insert_location for insertion at
the start of the block statement that is the body of the routine, set
*init_rout_scope to point to the scope entry for the routine, set
*il_region to the IL memory region number for the routine, and return a
pointer to the routine.  The routine is external if named, and static
if unnamed.
*/
{
  a_routine_ptr   init_rout;
  char            *name;
  sizeof_t        prefix_len, alloc_length;
  a_statement_ptr return_stmt;

  if (prefix == NULL) {
    /* Make an unnamed routine. */
    name = NULL;
  } else {
    /* Combine the prefix and an identifier for the current module to make
       a name that is likely to be unique. */
    make_module_id();
    prefix_len = strlen(prefix);
    alloc_length = prefix_len + strlen(module_id) + 1;
    name = alloc_lowered_name_string(alloc_length);
    (void)memcpy(name, prefix, size_t_arg(prefix_len));
    (void)strcpy(name+prefix_len, module_id);
  }  /* if */
  /* Make a type and routine entry for the routine. */
  init_rout = make_rout_entry(name,
                              name != NULL ? (a_storage_class)sc_unspecified :
                                             (a_storage_class)sc_static,
                              void_type(),
                              (a_type_ptr)NULL);
  /* Make a memory region, scope, and block for the routine definition. */
  *init_rout_scope = make_routine_definition(init_rout, /*make_return=*/TRUE,
                                             il_region);
  /* Add the return statement at the end of the routine to the return memo
     list. */
  return_memo_list = NULL;
  return_stmt = (*init_rout_scope)->assoc_block->variant.block.statements;
  add_to_return_memo_list(return_stmt);
  /* Set the insert location to the start of the top-level block. */
  set_block_start_insert_location((*init_rout_scope)->assoc_block,
                                  insert_location);
  return init_rout;
}  /* make_file_scope_init_or_term_routine */


static a_scope_ptr file_scope_init_insert_location(
                                        an_insert_location_ptr insert_location,
                                        a_memory_region_number *region_number)
/*
Create the file-scope initialization routine.  Set *insert location so it
can be used to insert code in that routine, and set *region_number to
the memory region number for the routine.  Return the scope for the routine.
*/
{
  a_scope_ptr scope;

  file_scope_init_routine = make_file_scope_init_or_term_routine(
                                      IL_LOWERING_INIT_ROUTINE_PREFIX,
                                      insert_location,
                                      &scope,
                                      region_number);
  return scope;
}  /* file_scope_init_insert_location */


static a_scope_ptr file_scope_term_insert_location(
                                        an_insert_location_ptr insert_location,
                                        a_memory_region_number *region_number)
/*
Create a file-scope termination routine.  Set *insert_location so it
can be used to insert code in that routine, and set *region_number to
the memory region number for the routine.  Return the scope for the routine.
Such routines are used for code that destroys a single variable (not, as
in cfront, for the code for all the file-scope destructions), so there
may be many different such routines generated (all unnamed).
*/
{
  a_scope_ptr scope;

  (void)make_file_scope_init_or_term_routine((char *)NULL,  /* Unnamed. */
                                             insert_location,
                                             &scope,
                                             region_number);
  return scope;
}  /* file_scope_term_insert_location */


/* Declaration needed because of mutual recursion: */
static a_boolean examine_expr_for_unsequenced_temp_inits(
                                           an_expr_node_ptr node,
                                           a_boolean        *p_any_temp_inits);


static a_boolean examine_expr_list_for_unsequenced_temp_inits(
                                        an_expr_node_ptr node_list,
                                        a_boolean        seq_point_after_first,
                                        a_boolean        *p_any_temp_inits)
/*
Examine the list of expressions headed by node_list, and their subtrees,
looking for unsequenced enk_temp_init initializations.  Return TRUE if
any are found.  If seq_point_after_operand is TRUE, there is a sequence
point after the first expression on the list.  Return *p_any_temp_inits
TRUE if there are any enk_temp_inits in the tree.
*/
{
  a_boolean             any_unsequenced = FALSE, any_temp_inits;
  a_boolean             any_prev_temp_inits = FALSE;
  an_expr_node_ptr      node;

  *p_any_temp_inits = FALSE;
  /* Go through the expressions and see where the enk_temp_inits fall. */
  for (node = node_list; node != NULL; node = node->next) {
    any_unsequenced = examine_expr_for_unsequenced_temp_inits(node,
                                                              &any_temp_inits);
    if (any_temp_inits) {
      /* Some enk_temp_inits in the expression.  If there have been any in
         any previous expressions (since the sequence point, if there was
         one), the enk_temp_inits are unsequenced. */
      if (any_prev_temp_inits) any_unsequenced = TRUE;
      any_prev_temp_inits = *p_any_temp_inits = TRUE;
    }  /* if */
    if (any_unsequenced) break;
    /* See if there is a sequence point after the first expression. */
    if (seq_point_after_first) {
      any_prev_temp_inits = FALSE;
      seq_point_after_first = FALSE;
    }  /* if */
  }  /* for */
  return any_unsequenced;
}  /* examine_expr_list_for_unsequenced_temp_inits */


static a_boolean examine_dynamic_init_for_unsequenced_temp_inits(
                                        a_dynamic_init_ptr dip,
                                        a_boolean          *p_any_temp_inits)
/*
Examine the dynamic initialization entry pointed to by dip, and its subtree,
looking for unsequenced enk_temp_init initializations.  Return TRUE if any
are found.  Return *p_any_temp_inits TRUE if there are any enk_temp_inits
in the tree.  Do nothing if dip == NULL.
*/
{
  a_boolean any_unsequenced = FALSE;

  *p_any_temp_inits = FALSE;
  if (dip != NULL) {
    switch (dip->kind) {
      case dik_none:
      case dik_zero:
      case dik_constant:
        break;
      case dik_expression:
      case dik_call_returning_class_via_cctor:
        any_unsequenced = examine_expr_for_unsequenced_temp_inits(
                                                       dip->variant.expression,
                                                       p_any_temp_inits);
        break;
      case dik_constructor:
        any_unsequenced = examine_expr_list_for_unsequenced_temp_inits(
                                               dip->variant.constructor.args,
                                               /*seq_point_after_first=*/FALSE,
                                               p_any_temp_inits);
        break;
      case dik_nonconstant_aggregate:
      case dik_bitwise_copy:
        /* These are not expected under expressions. */
      default:
        unexpected_condition_str(
     "examine_dynamic_init_for_unsequenced_temp_inits: bad dynamic init kind");
    }  /* switch */
  }  /* if */
  return any_unsequenced;
}  /* examine_dynamic_init_for_unsequenced_temp_inits */


static a_boolean examine_expr_for_unsequenced_temp_inits(
                                            an_expr_node_ptr node,
                                            a_boolean        *p_any_temp_inits)
/*
Examine node and its subtree looking for unsequenced enk_temp_init
initializations.  Return TRUE if any are found.  Return *p_any_temp_inits
TRUE if there are any enk_temp_inits in the tree.
*/
{
  a_boolean             any_unsequenced = FALSE;
  a_boolean             any_temp_inits, seq_point_after_first;
  an_expr_operator_kind op;

  *p_any_temp_inits = FALSE;
  switch (node->kind) {
    case enk_error:
    case enk_constant:
    case enk_variable:
    case enk_variable_address:
    case enk_routine_address:
    case enk_field:
      /* No temp inits. */
      break;
    case enk_operation:
      seq_point_after_first = FALSE;
      op = node->variant.operation.kind;
      if (op == (an_expr_operator_kind)eok_land ||
          op == (an_expr_operator_kind)eok_lor ||
          op == (an_expr_operator_kind)eok_comma ||
          op == (an_expr_operator_kind)eok_question) {
        /* Operators with a sequence point after the first operand. */
        seq_point_after_first = TRUE;
      }  /* if */
      any_unsequenced = examine_expr_list_for_unsequenced_temp_inits(
                                        node->variant.operation.operands,
                                        seq_point_after_first,
                                        p_any_temp_inits);
      break;
    case enk_temp_init:
      any_unsequenced = examine_dynamic_init_for_unsequenced_temp_inits(
                                        node->variant.init.dynamic_init,
                                        p_any_temp_inits);
      /* An enk_temp_init only counts if it includes a destruction. */
      if (node->variant.init.dynamic_init->destructor != NULL) {
        *p_any_temp_inits = TRUE;
      }  /* if */
      break;
    case enk_new_delete:
      any_unsequenced = examine_expr_list_for_unsequenced_temp_inits(
                                        node->variant.new_delete->arg,
                                        /*seq_point_after_first=*/FALSE,
                                        &any_temp_inits);
      *p_any_temp_inits |= any_temp_inits;
      if (any_unsequenced) break;
      any_unsequenced = examine_dynamic_init_for_unsequenced_temp_inits(
                                        node->variant.new_delete->dynamic_init,
                                        &any_temp_inits);
      *p_any_temp_inits |= any_temp_inits;
      break;
    case enk_throw:
      any_unsequenced = examine_dynamic_init_for_unsequenced_temp_inits(
                                        node->variant.throw_info->dynamic_init,
                                        p_any_temp_inits);
      break;
    default:
      unexpected_condition_str(
                     "examine_expr_for_unsequenced_temp_inits: bad expr kind");
  }  /* switch */
  return any_unsequenced;
}  /* examine_expr_for_unsequenced_temp_inits */


static void examine_curr_full_expression_for_unsequenced_temp_inits(void)
/*
Look at curr_full_expression to see if it contains any enk_temp_init
initializations that are unsequenced relative to one another.  Set
curr_full_expression_has_unsequenced_temp_inits accordingly.  Set
curr_full_expression_examined_for_unsequenced_temp_inits to TRUE
to indicate that the search has been done, and do not do the search
again on subsequent calls.
*/
{
  a_boolean any_temp_inits;

  if (!curr_full_expression_examined_for_unsequenced_temp_inits) {
    curr_full_expression_has_unsequenced_temp_inits =
                 examine_expr_for_unsequenced_temp_inits(curr_full_expression,
                                                         &any_temp_inits);
    curr_full_expression_examined_for_unsequenced_temp_inits = TRUE;
  } /* if */
}  /* examine_curr_full_expression_for_unsequenced_temp_inits */


static void add_conditional_flag(a_cleanup_action_ptr cap,
                                 an_insert_location   *insert_location)
/*
cap points to a cak_destruction cleanup action being generated.  Add
a conditional flag to the cleanup.  This is needed, for example,
inside a conditional operand of a "?", "&&", or "||" operation, to make
the corresponding destruction dependent on whether the construction was
done.  We add a temporary variable and insert an assignment to set the
temporary to 1 at insert_location.  init_conditional_flag_var must be
called later to initialize the temporary to zero at the beginning of
the current block (that can't be done yet because we don't know the
object table address assigned to the conditional flag for exception
cleanup).
*/
{
  a_variable_ptr temp;

  check_assertion(cap->kind == cak_destruction);
  temp = make_lowered_temporary(integer_type((an_integer_kind)ik_int));
  cap->variant.object.conditional_flag_var = temp;
  /* Make and insert an assignment statement to set the temporary to 1. */
  (void)insert_var_assignment_statement(temp,
                                        (an_expr_operator_kind)eok_iassign,
                                        node_for_integer_constant(1L,
                                                      (an_integer_kind)ik_int),
                                        insert_location);
}  /* add_conditional_flag */


static void init_conditional_flag_object_addr_table_entry(
                                         a_cleanup_action_ptr cap,
                                         an_insert_location   *insert_location)
/*
Generate any code required to put the address of the conditional flag
associated with the cleanup action cap into the object address table.
*/
{
  an_init_pos_descr ipd;
  a_variable_ptr    cond_var = cap->variant.object.conditional_flag_var;

  check_assertion(is_object_cleanup_action(cap));
  set_var_init_pos_descr(cond_var, &ipd);
  /* The region number for the conditional flag is one greater than
     the base region number. */
  init_object_addr_table_entry(&ipd, cap->region_number+1, insert_location);
}  /* init_conditional_flag_object_addr_table_entry */


void init_conditional_flag_var(a_cleanup_action_ptr cap,
                               an_insert_location   *insert_location)
/*
cap is a cleanup action that has an associated conditional flag.
Generate code to set the conditional flag variable to 0.  If 
insert_location is non-NULL, it indicates the point at which the code should
be inserted.  Otherwise, the initialization is done with an stmk_init
statement inserted at the right place in the current scope.  This routine
must called late, after the object address table entry for the conditional
flag has been created.
*/
{
  a_variable_ptr      cond_var;
  a_dynamic_init_ptr  dip;
  a_constant          zero_constant;
  a_statement_ptr     stmk_init_stmt, block, label_statement;
  a_switch_clause_ptr scp;
  an_insert_location  local_insert_location, *eff_insert_location;
  a_boolean           var_is_static;
  a_boolean           follows_an_exec_statement = FALSE;

  check_assertion(is_object_cleanup_action(cap));
  cond_var = cap->variant.object.conditional_flag_var;
  var_is_static = (cond_var->storage_class == (a_storage_class)sc_static);
  eff_insert_location = insert_location;
  if (eff_insert_location == NULL && (!var_is_static || exceptions_enabled)) {
    /* Some code will be generated, so we need an insert location, but we do
       not have one.  Generate one from context.  Also set
       follows_an_exec_statement to indicate whether the insertion point
       follows any executable statements in its block (this is needed for
       the dynamic initialization entry). */
    /* For most cases, the right place is the beginning of the current block.
       For switch clauses, it's the beginning of the clause.  When labels
       appear, the initialization goes after the latest label. */
    scp = curr_context->assoc_switch_clause;
    label_statement = curr_context->latest_label_statement_processed;
    if (label_statement != NULL) {
      /* Insert after the most recent label. */
      follows_an_exec_statement = TRUE;
      set_insert_location(label_statement, &local_insert_location);
    } else if (scp != NULL) {
      /* Insert at the start of the current switch clause. */
      follows_an_exec_statement = TRUE;
      set_switch_clause_start_insert_location(scp, &local_insert_location);
    } else {
      /* Normal case.  Insert at the start of the current block. */
      block = curr_context->scope->assoc_block;
#if CHECKING
      if (block == NULL) {
        internal_error("init_conditional_flag_var: missing block");
      }  /* if */
#endif /* CHECKING */
     set_block_start_insert_location(block, &local_insert_location);
    }  /* if */
    eff_insert_location = &local_insert_location;
  }  /* if */
  /* Initialize the conditional flag variable. */
  if (var_is_static) {
    /* The conditional flag is static and therefore is implicitly initialized
       to zero. */
  } else if (insert_location != NULL) {
    /* The flag requires initialization, and the insert location was
       provided by the caller.  Insert an assignment at that point.  Note
       that this is an assignment rather than an stmk_init because this
       case is used for initialization within expressions. */
    (void)insert_var_assignment_statement(cond_var,
                                          (an_expr_operator_kind)eok_iassign,
                                          node_for_integer_constant(0L,
                                                      (an_integer_kind)ik_int),
                                          insert_location);
  } else {
    /* The flag requires initialization, and the insert location was
       generated in this routine.  Use a dynamic initialization entry to
       do the initialization. Note that the dynamic init entry does not
       need to be put on a list of dynamic init entries.  Such a list is
       used only at the file scope, and any temporary allocated there
       would be static. */
    dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constant);
    dip->variable = cond_var;
    dip->follows_an_exec_statement = follows_an_exec_statement;
    /* The dynamic init entry is pointed to by the variable. */
    cond_var->init_kind = (an_init_kind)initk_dynamic;
    cond_var->initializer.dynamic = dip;
    set_integer_constant(&zero_constant, 0L, (an_integer_kind)ik_int);
    dip->variant.constant = alloc_unshared_constant(&zero_constant);
    /* The dynamic init entry is pointed to by an stmk_init statement. */
    stmk_init_stmt = alloc_statement((a_statement_kind)stmk_init);
    stmk_init_stmt->variant.dynamic_init = dip;
    insert_statement(stmk_init_stmt, eff_insert_location);
  }  /* if */
  if (exceptions_enabled) {
    /* Initialize the object address table entry for the conditional flag. */
    init_conditional_flag_object_addr_table_entry(cap, eff_insert_location);
  }  /* if */
}  /* init_conditional_flag_var */


static a_cleanup_action_ptr alloc_destruction_cleanup_action(
                            a_dynamic_init_ptr    dip,
                            an_init_pos_descr_ptr ipdp,
                            a_boolean             applies_on_block_exit,
                            a_boolean             applies_on_exception_cleanup)
/*
Allocate a cak_destruction cleanup action entry and return a pointer to it.
The dynamic initialization for which the destruction must be done is
given by dip, and the object to be destroyed is described by ipdp.
applies_on_block_exit and applies_on_exception_cleanup are the values for
the like-named flags in the cleanup entry.
*/
{
  a_cleanup_action_ptr cap;

  cap = alloc_cleanup_action(cak_destruction,
                             applies_on_block_exit,
                             applies_on_exception_cleanup);
  /* Copy the entire dynamic init entry in case the original one gets
     modified. */
  cap->variant.object.dynamic_init = *dip;
  cap->variant.object.init_pos_descr = *ipdp;
  /* If this is an initialization within an aggregate, we must save the
     init_pos_modifier list.  However, the list runs through the stack,
     so we must make an allocated copy. */
  if (ipdp->modifiers != NULL) {
    cap->variant.object.init_pos_descr.modifiers =
                                  copy_init_pos_modifier_list(ipdp->modifiers);
  }  /* if */
  return cap;
}  /* alloc_destruction_cleanup_action */


/*
Pointer to the struct type used to provide information to the runtime about
a needed destruction for a file-scope or local static variable.
NULL until created.
*/
static a_type_ptr
		needed_destruction_type;
static a_field_ptr
		needed_destruction_object_field;


static a_type_ptr make_needed_destruction_type(void)
/*
Make the struct type used to provide information to the runtime about a needed
destruction for a file-scope or local static variable, if it is not made
already, and return a pointer to it.  Its definition is

       struct a_needed_destruction {
         a_needed_destruction *next;
         void                 *object;
         __vptp               dtor;
       };

See the runtime files dtor_list.h and dtor_list.c.
*/
{
  a_targ_size_t byte_offset;
  a_field_ptr   last_field;

  if (needed_destruction_type == NULL) {
    /* Make the struct type.  It doesn't actually have a name. */
    needed_destruction_type = alloc_type((a_type_kind)tk_struct);
    add_to_front_of_file_scope_types_list(needed_destruction_type);
    byte_offset = 0;
    last_field = NULL;
    /* field: a_needed_destruction_ptr next */
    make_lowered_field("next", make_pointer_type(needed_destruction_type),
                       &byte_offset, needed_destruction_type, &last_field);
    /* field: void *object */
    make_lowered_field("object", void_star_type(),
                       &byte_offset, needed_destruction_type, &last_field);
    needed_destruction_object_field = last_field;
    /* field: __vptp dtor */
    make_lowered_field("dtor", make_vptp_type(),
                       &byte_offset, needed_destruction_type, &last_field);
    finish_class_type(needed_destruction_type, &byte_offset);
  }  /* if */
  return needed_destruction_type;
}  /* make_needed_destruction_type */


static a_routine_ptr make_destruction_routine(a_cleanup_action_ptr cap)
/*
Make a routine that contains the code necessary to do the cleanup indicated
in *cap (a destruction).
*/
{
  a_scope_ptr            scope;
  an_insert_location     insert_location;
  a_memory_region_number region_number;
  a_memory_region_number region_to_switch_back_to = curr_il_region_number;
  a_context              context;
  a_routine_ptr          routine;
  /* The return_memo_list is saved and restored because we may be inside
     a routine. */
  a_return_memo_ptr      saved_return_memo_list = return_memo_list;

  /* Create a routine. */
  scope = file_scope_term_insert_location(&insert_location, &region_number);
  routine = scope->variant.routine.ptr;
  switch_il_region(region_number);
  push_context(&context, scope, /*subscope_region=*/FALSE);
  /* Generate the code for the destruction. */
  gen_one_cleanup_action(cap, &insert_location);
  /* Mark the variable as referenced from another function. */
  cap->variant.object.init_pos_descr.variable->referenced_non_locally = TRUE;
  free_return_memo_list(return_memo_list);
  return_memo_list = saved_return_memo_list;
  pop_context();
  done_with_memory_region(region_number);
  switch_il_region(region_to_switch_back_to);
  return routine;
}  /* make_destruction_routine */


/*
Pointer to the routine entry for the runtime routine used to record a
needed call of a destructor, once created.  NULL until then.
*/
static a_routine_ptr
		record_needed_destruction_routine;


static void record_needed_destruction(a_cleanup_action_ptr   cap,
                                      an_insert_location_ptr insert_location)
/*
cap points to a cleanup action (not on any list) that describes a destruction
required for a local static variable or an object initialized in the
file-scope initialization routine.  Generate a runtime call that records
the need for the destruction at the time of program termination.
Free the cleanup action entry.  Insert any generated code at *insert_location
and update *insert_location accordingly.
*/
{
  a_variable_ptr         var;
  a_constant_ptr         aggr_con, next_con, object_con, dtor_con;
  a_boolean              complex_cleanup, complex_address;
  a_routine_ptr          dtor_routine;
  a_memory_region_number region_to_switch_back_to = NULL_region_number;
  an_init_pos_descr_ptr  ipdp;
  an_expr_node_ptr       call_node;
  a_statement_ptr        call_stmt;

  check_assertion(cap->applies_on_block_exit &&
                  cap->kind == cak_destruction);
  ipdp = &cap->variant.object.init_pos_descr;
  /* Record the required destruction by generating a call of the runtime
     routine __record_needed_destruction.  A data structure passed to
     that routine describes the destruction to be done:

       struct a_needed_destruction {
         a_needed_destruction *next;
         void                 *object;
         __vptp               dtor;
       };

     For a simple cleanup -- just a destructor call -- object points
     to the object and dtor points to the destructor.  For anything
     more complex (e.g., an array), object is NULL and dtor points
     to a routine generated specifically for this case and containing
     the necessary destruction code.  next is always initialized to
     NULL; the runtime routine sets it. */
  complex_cleanup = requires_nontrivial_cleanup(cap);
  /* Compute the object address (instead of doing static initialization to
     the address) if it is more than a simple variable. */
  complex_address = ipdp->indirect_through_variable ||
                    ipdp->modifiers != NULL;
  switch_to_file_scope_region(&region_to_switch_back_to);
  /* Make an unnamed static variable for the descriptive structure. */
  var = make_unnamed_local_static_variable(make_needed_destruction_type(),
                                           /*in_function_scope=*/FALSE);
  /* Make the top-level aggregate constant that will be its initial value. */
  aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
  var->init_kind = (an_init_kind)initk_static;
  var->initializer.constant = aggr_con;
  /* Make the constants under the aggregate constant. */
  next_con = alloc_constant((a_constant_repr_kind)ck_address);
  make_zero_of_proper_type(make_pointer_type(var->type), next_con);
  object_con = alloc_constant((a_constant_repr_kind)ck_address);
  dtor_con = alloc_constant((a_constant_repr_kind)ck_address);
  if (complex_cleanup) {
    /* Complex cleanup -- the object field is NULL and the dtor field points
       to a fabricated routine containing the destruction code. */
    make_zero_of_proper_type(void_star_type(), object_con);
    dtor_routine = make_destruction_routine(cap);
  } else {
    /* Simple cleanup -- the object field points to the object variable and
       the dtor field points to the destructor. */
    if (complex_address) {
      /* For a complex object address, initialize the field to NULL and
         set the object address via code (below). */
      make_zero_of_proper_type(void_star_type(), object_con);
    } else {
      set_variable_address_constant(ipdp->variable, object_con,
                                    /*set_address_taken_flag=*/TRUE);
      implicit_cast(object_con, void_star_type());
    }  /* if */
    dtor_routine = cap->variant.object.dynamic_init.destructor;
  }  /* if */
  set_routine_address_constant(dtor_routine, dtor_con,
                               /*set_address_taken_flag=*/TRUE);
  implicit_cast(dtor_con, make_vptp_type());
  /* Link the aggregate constant together. */
  aggr_con->variant.aggregate.first_constant = next_con;
  next_con->next = object_con;
  object_con->next = dtor_con;
  aggr_con->variant.aggregate.last_constant = dtor_con;
  /* Free the cleanup action now that it's no longer needed. */
  free_cleanup_action(cap);
  switch_back_to_original_region(region_to_switch_back_to);
  if (!complex_cleanup && complex_address) {
    /* For simple cleanup with a complex address, compute the object address
       in code and store it in the object field of the struct. */
    an_expr_node_ptr object_node, field_node;
    a_statement_ptr  assign_stmt;
    object_node = make_init_entity_node(ipdp, /*using_as_address=*/TRUE,
                                        /*using_as_dest=*/FALSE);
    field_node = field_lvalue_selection_expr(var_lvalue_expr(var),
                                             needed_destruction_object_field);
    assign_stmt = insert_assignment_statement(field_node,
                                            (an_expr_operator_kind)eok_passign,
                                              object_node,
                                              insert_location);
    set_stmt_pos_to_code_pos_for_lowering(assign_stmt);
  }  /* if */
  /* Make a call of __record_needed_destruction.  Its argument is the
     address of the structure variable created above. */
  call_node = make_runtime_rout_call("__record_needed_destruction",
                                     &record_needed_destruction_routine,
                                     void_type(), var_lvalue_expr(var));
  /* Make a statement containing the call. */
  call_stmt = alloc_expr_statement(call_node);
  set_stmt_pos_to_code_pos_for_lowering(call_stmt);
  /* Insert the statement at the right location. */
  insert_statement(call_stmt, insert_location);
}  /* record_needed_destruction */


void lower_dynamic_init(a_dynamic_init_ptr     dip,
                        an_init_pos_descr_ptr  ipdp,
                        a_boolean              is_expr_temporary,
                        an_expr_node_ptr       implied_arg_list,
                        an_expr_node_ptr       end_implied_arg_list,
                        a_constructor_init_ptr ctor_init,
                        an_insert_location_ptr insert_location,
                        a_boolean              *keep_dynamic_init)
/*
Do IL lowering of the indicated dynamic initialization and everything under
it.  ipdp indicates the entity to be initialized.  Ordinarily, that is the
entire variable indicated in the dynamic initialization entry (that happens
when the entry is pointed to by an stmk_init statement or when it appears
on a file-scope dynamic_inits list).  ipdp can, however, indicate a part of
an aggregate.

If is_expr_temporary is TRUE, this dynamic initialization is pointed to by
an enk_temp_init expression node, i.e., it initializes a temporary in
an expression.

If implied_arg_list and end_implied_arg_list are non-NULL, they point to
the beginning and end of a list of implied arguments for a constructor
call (for implicit virtual base class arguments).

If the dynamic initialization is part of a constructor initializer,
ctor_init points to the constructor-init entry.

This routine is only called for non-C cases, and therefore it will always
generate some executable code.  (Well, almost always: a dynamic initialization
that contains a destructor but that could otherwise be rendered as a static
initialization will be turned into the static initialization, which means
no code will be generated.)  The code will be inserted at *insert_location.
*insert_location will be updated to indicate a location after the inserted
code.

On return, *keep_dynamic_init is TRUE if the dynamic init entry is to
be kept, FALSE if it should be deleted.
*/
{
  an_expr_node_ptr  entity_node, source_node;
  a_variable_ptr    variable;
  a_boolean         simple_constant_init = FALSE, keep_constant;
  a_constant_ptr    simple_constant;
  a_source_position saved_error_position, saved_code_pos;
  a_statement_ptr   expr_stmt;
  a_type_ptr        ctor_routine_type;
  a_type_ptr        this_param_type;
  a_param_type_ptr  param;
  a_boolean         static_var_init, expr_copy_needed;

  *keep_dynamic_init = FALSE;
  saved_code_pos = code_pos_for_lowering;
  saved_error_position = error_position;
  variable = dip->variable;
  if (variable != NULL) {
    /* Whole-variable initialization. */
    /* Track the source position. */
    /* Don't change the position for enk_temp_init temporaries; keep the
       position of the surrounding expression. */
    if (!is_expr_temporary) {
      code_pos_for_lowering = error_position =
                                        variable->source_corresp.decl_position;
    }  /* if */
#if CHECKING
    if (variable != ipdp->variable) {
      internal_error("lower_dynamic_init: variable mismatch");
    }  /* if */
#endif /* CHECKING */
    /* Let the back end know that some initialization code was
       rewritten as executable code. */
    variable->initialization_rewritten_as_assignment = TRUE;
  }  /* if */
  /* Initializations of static variables (whether global or function-local)
     require some special processing. */
  static_var_init = init_pos_is_static(ipdp);
  /* When generating the file-scope initialization routine we have
     an expression from the file scope that must be used in the function
     scope of the initialization routine, so it must be copied.  Otherwise
     we have a difficult job keeping track of the nodes that are in
     the file scope and those that are in the function scope.
     Similar reasoning applies to local static variables. */
  expr_copy_needed = (static_var_init || processing_file_scope_init_routine);
  switch (dip->kind) {
    case dik_none:
      break;
    case dik_zero:
      /* Initialize a variable to zero. */
      check_assertion_str(variable != NULL,
                          "lower_dynamic_init: dik_zero variable missing");
      /* Do nothing here.  Processing is below. */
      break;
    case dik_constant:
      /* Assign a constant to the entity to be initialized. */
      lower_constant(dip->variant.constant);
      /* If there is a whole variable, this dynamic initialization can be
         rendered in C IL without generating an assignment.  It's probably
         here as a dynamic initialization because it has a destructor call
         too. */
      if (variable != NULL) {
        simple_constant_init = TRUE;
        simple_constant = dip->variant.constant;
        break;
      }  /* if */
      /* For the normal cases, go on and generate an assignment. */
      goto do_assignment;
    case dik_expression:
      /* Assign an expression to the entity to be initialized. */
      lower_normal_expr(dip->variant.expression);
      if (expr_copy_needed) {
        /* Copy a file-scope expression into the current (function scope)
           memory region. */
        dip->variant.expression = copy_expr_tree(dip->variant.expression);
      }  /* if */
do_assignment:;
#if CHECKING
      if (ipdp->whole_array) {
        internal_error("lower_dynamic_init: array for const or expr init");
      }  /* if */
#endif /* CHECKING */
      /* Make a node for the entity to be initialized. */
      entity_node = make_init_entity_node(ipdp, /*using_as_address=*/FALSE,
                                          /*using_as_dest=*/TRUE);
      add_init_assignment(dip, entity_node, insert_location);
      break;
    case dik_call_returning_class_via_cctor:
      /* Initialize the entry by calling a routine that returns its result
         via a copy constructor. */
      if (expr_copy_needed) {
        /* Copy a file-scope expression into the current (function scope)
           memory region. */
        dip->variant.expression = copy_expr_tree(dip->variant.expression);
      }  /* if */
      /* The address of the temporary being initialized is added as an
         implicit argument of the call. */
      lower_call(dip->variant.expression, ipdp);
      expr_stmt = insert_expr_statement(dip->variant.expression,
                                        insert_location);
      set_stmt_pos_to_code_pos_for_lowering(expr_stmt);
      break;
    case dik_constructor:
      /* Initialize the entity by calling a constructor. */
      /* The routine does not need to be lowered from here. */
      if (expr_copy_needed) {
        /* Copy a file-scope expression into the current (function scope)
           memory region. */
        dip->variant.constructor.args =
                        copy_list_of_expr_trees(dip->variant.constructor.args);
      }  /* if */
      /* Make a node for the entity to be initialized. */
      entity_node = make_init_entity_node(ipdp, /*using_as_address=*/TRUE,
                                          /*using_as_dest=*/TRUE);
      /* Cast the entity node pointer to the right type to eliminate
         qualifier and type-as-subobject differences. */
      ctor_routine_type = dip->variant.constructor.ptr->type;
      this_param_type = implicit_this_param_type_of(ctor_routine_type);
      entity_node = add_cast_if_necessary(entity_node,
                                          f_skip_typerefs(this_param_type));
      source_node = NULL;
      param = NULL;
      if (dip->variant.constructor.is_copy_constructor_with_implied_source) {
        /* The constructor is a copy constructor, and the source of the
           copy is implied.  Determine the source location. */
        source_node = implied_source_of_copy(ctor_init, ipdp,
                                             /*using_as_address=*/TRUE);
        /* Cast the expression to the right type to eliminate qualifier and
           type-as-subobject differences.  Use the pointer version of
           the parameter reference type. */
        param = unlowered_param_type_list(ctor_routine_type);
        source_node = add_cast_if_necessary(source_node,
                                            make_pointer_type(
                                                type_pointed_to(param->type)));
        /* Leave the parameter pointer set for lowering any additional
           arguments below. */
        param = param->next;
      }  /* if */
      if (ipdp->whole_array) {
        /* Construct an array. */
#if CHECKING
        if (implied_arg_list != NULL) {
          internal_error("lower_dynamic_init: implied arg list for array");
        }  /* if */
#endif /* CHECKING */
        /* Note that dip->variant.constructor.args has not been lowered,
           which is what the subroutine requires. */
        add_array_constructor_call(dip, entity_node, source_node,
                                   ipdp->array_element_count,
                                   insert_location);
      } else {
        /* Construct a simple entity (not an array). */
        /* Lower any added arguments. */
        lower_arg_expr_list(dip->variant.constructor.args, ctor_routine_type,
                            param);
        /* Generate the constructor call. */
        add_constructor_call(dip, entity_node, source_node,
                             implied_arg_list, end_implied_arg_list,
                             insert_location);
      }  /* if */
      break;
    case dik_nonconstant_aggregate:
      /* Initialization with a nonconstant aggregate constant.  This is usually
         a whole-variable initialization, but can be used in a ctor-initializer
         to iterate over an array initialization, etc. */
      keep_constant = FALSE;
      lower_dynamic_init_aggregate_constant(dip->variant.constant, ipdp,
                                            /*dtor_case=*/FALSE, ctor_init,
                                            insert_location,
                                            &keep_constant);
      if (keep_constant) {
        /* Keep a (now-)constant aggregate value as the static initial value
           of the variable.  The nonconstant parts have been put out as
           code and replaced with placeholder constants. */
        simple_constant_init = TRUE;
        simple_constant = dip->variant.constant;
      }  /* if */
      break;
    case dik_bitwise_copy:
      /* Bitwise copy of a value.  The source location is implied.
         This is used for copying members of classes in ctor-initializers
         of copy constructors, and for the parameter of catch clauses.
         ctor_init is non-NULL for the first of those cases. */
      add_bitwise_copy(ipdp, ctor_init, insert_location);
      break;
#if CHECKING
    default:
      internal_error("lower_dynamic_init: bad kind");
#endif /* CHECKING */
  }  /* switch */
  /* If the dynamic init entry indicates a destructor call, put it on a
     list of cleanup actions to be processed at the end of the scope.
     Note that the list gets built in the right order (i.e., the reverse of
     construction order) because each entry is added to the front of the
     list. */
  if (dip->destructor != NULL) {
    a_cleanup_action_ptr cap;
    a_boolean            applies_on_block_exit = TRUE;
    /* The action applies on block exit except when we are processing the
       wrapper of a constructor (ctor_init != NULL).  In that case the
       destructor part of the initialization is only there for exception
       cleanup.  For the file-scope initialization routine "on block exit"
       gets interpreted as "in the the file-scope termination routine." */
    if (ctor_init != NULL) {
      applies_on_block_exit = FALSE;
    } else if (nearest_function_scope != NULL &&
               nearest_function_scope->variant.routine.
                                               return_value_variable != NULL &&
               ipdp->variable == return_value_pointer_variable) {
      /* This is the initialization of the parameter substituted for the
         return value optimization variable.  The destruction doesn't get
         done on exit from the routine; the caller does it. */
      applies_on_block_exit = FALSE;
    }  /* if */
    cap = alloc_destruction_cleanup_action(dip, ipdp,
                                 applies_on_block_exit,
                                 /*applies_on_exception_cleanup=*/TRUE);
    /* Clear the destructor field in the dynamic init entry to make it legal
       C IL. */
    dip->destructor = NULL;
    /* For local static variables and all initializations inside the
       file-scope initialization routine, generate code to record at runtime
       the need for a destruction later, and don't put the cleanup action
       entry on a list.  Note that conditional flag variables are not
       needed even for constructions in conditional parts of expressions,
       since the destruction is only put on the list if the construction
       was done. */
    if (static_var_init) {
      record_needed_destruction(cap, insert_location);
    } else {
      /* Initializations of nonstatic variables. */
      /* Remember whether this initialization is for an enk_temp_init. */
      if (is_expr_temporary) {
        cap->variant.object.is_expr_temporary = TRUE;
        /* Remember the associated full expression. */
        cap->variant.object.full_expression = curr_full_expression;
      }  /* if */
      /* Remember whether this initialization is in a constructor wrapper. */
      if (ctor_init != NULL) cap->constructor_wrapper_cleanup = TRUE;
      if (num_conditional_exprs_inside_of != 0) {
        /* Inside a conditional operand of a "?", "&&", or "||" operation.
           Since the construction is conditional, we add a temporary
           variable, set it to 1 here, and test it later to decide whether
           to do the destruction.  init_conditional_flag_var is called
           later to initialize the temporary to zero at the beginning
           of the current scope. */
        add_conditional_flag(cap, insert_location);
      } else if (is_expr_temporary &&
                 curr_full_expression_has_unsequenced_temp_inits) {
        /* Also add the conditional flag when there are unsequenced
           enk_temp_init operations. */
        cap->variant.object.conditional_flag_added_for_unsequenced_case = TRUE;
        add_conditional_flag(cap, insert_location);
      }  /* if */
      /* Put the new entry on the front of the existing cleanup list for
         the current context. */
      add_cleanup_action_to_context_list(cap, curr_context, insert_location);
      if (cap->variant.object.conditional_flag_var != NULL) {
        /* This operation has a conditional flag.  Initialize the flag to zero.
           This must be done after the cleanup action has been added to the
           context list (and therefore make_region_table_entry has been called)
           so that the exception cleanup region entry for the conditional flag
           has been created. */
        if (curr_context->assoc_expr != NULL) {
          /* The current context is a region that is a single top-level
             expression.  The initialization must be inserted on top of the
             expression, but it would be dangerous to modify the expression
             that we're currently working on.  Therefore, that's left to be
             done when we get back to the top of the expression.  See
             gen_expr_conditional_flag_var_initializations. */
          curr_context->any_conditional_flag_var_initializations_deferred=TRUE;
        } else {
          /* Set the variable to zero initially. */
          init_conditional_flag_var(cap, (an_insert_location *)NULL);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  /* In the whole-variable cases, adjust the initialization specified in
     the variable (it points to the dynamic init entry). */
  if (variable != NULL) {
    if (simple_constant_init) {
      /* Initialization to a simple constant. */
      if (static_var_init) {
        /* Initialization of a static variable to a constant.  Can be
           done as a static initialization. */
        variable->init_kind = (an_init_kind)initk_static;
        variable->initializer.constant = simple_constant;
      } else {
        /* Initialization of an automatic variable to a constant.  Can be done
           by keeping the dynamic init entry. */
        *keep_dynamic_init = TRUE;
        set_dynamic_init_kind(dip, (a_dynamic_init_kind)dik_constant);
        dip->variant.constant = simple_constant;
      }  /* if */
    } else if (dip->kind == (a_dynamic_init_kind)dik_zero) {
      /* Initialization to zero. */
      variable->init_kind = (an_init_kind)initk_zero;
    } else {
      /* The initialization is handled entirely by the generated code.
         It would seem that the variable should no longer be marked as
         initialized, but in fact we want to preserve the distinction between
         static variables that are initialized and those that are tentative
         definitions.  That is important when the initialization is in a
         library; the linker has to see it as a definition in order for it
         to bring in the variable (and hence the initialization code) from
         a library.  There is also an issue with automatic variables that
         are aggregates: if the initialization was partial, we have to be
         sure the rest of the aggregate is initialized to zero.
         So we change the initialization kind to initialization to zero. */
      if (static_var_init || variable->is_partially_initialized) {
        variable->init_kind = (an_init_kind)initk_zero;
      } else {
        variable->init_kind = (an_init_kind)initk_none;
      }  /* if */
    }  /* if */
  }  /* if */
  error_position = saved_error_position;
  code_pos_for_lowering = saved_code_pos;
}  /* lower_dynamic_init */


void lower_destructor_dynamic_init(a_dynamic_init_ptr     dip,
                                   an_init_pos_descr_ptr  ipdp,
                                   a_cleanup_action_ptr   cap,
                                   a_boolean              have_complete_object,
                                   an_insert_location_ptr insert_location)
/*
Do IL lowering of the destruction part of a dynamic initialization entry
(ignore any initialization that is indicated).  dip points to the dynamic
initialization, and ipdp identifies the entity to be destroyed.  cap
points to the cleanup action entry for the destruction, or is NULL if
this destruction is part of a destructor wrapper.  If have_complete_object 
is TRUE, the entity being destroyed is a complete object.  The statements
are inserted at *insert_location and *insert_location is updated.
*/
{
  an_expr_node_ptr  entity_node;
  a_variable_ptr    variable;
  a_source_position saved_error_position;

  saved_error_position = error_position;
  variable = dip->variable;
  if (variable != NULL) {
    /* Track the source position for internal errors. */
    error_position = variable->source_corresp.decl_position;
#if CHECKING
    if (variable != ipdp->variable) {
      internal_error("lower_destructor_dynamic_init: variable mismatch");
    }  /* if */
#endif /* CHECKING */
  }  /* if */
  /* Suppress exception handling on static variables because they are cleaned
     up via a list of all initialized static variables. */
  if (exceptions_enabled && !init_pos_is_static(ipdp)) {
    if (cap == NULL) {
      /* Generate an exception cleanup action for a destruction in a destructor
         wrapper. */
      cap = alloc_destruction_cleanup_action(dip, ipdp,
                                        /*applies_on_block_exit=*/FALSE,
                                        /*applies_on_exception_cleanup=*/TRUE);
      cap->destructor_wrapper_cleanup = TRUE;
      add_cleanup_action_to_context_list(cap, curr_context,
                                       &dtor_wrapper_prologue_insert_location);
      /* Insert an assignment to set the current exception handling region
         to this new region.  That gets set before the *previous* cleanup
         action. */
      set_region_on_prev_destructor_wrapper_cleanup(cap, insert_location);
    } else {
      /* Normal case (not a destructor wrapper).  Set the region number
         to what it should be after the destruction, because as soon as
         we start the destruction it's the destructor's job to deal with
         partial destruction. */
      a_cleanup_region_number region_number =
                            cleanup_region_number(cap->next_exception_cleanup);
      assign_region_number_to_eh_curr_region(region_number, insert_location);
    }  /* if */
  }  /* if */
  /* Make an expression for the object to be destroyed. */
  entity_node = make_init_entity_node(ipdp, /*using_as_address=*/TRUE,
                                      /*using_as_dest=*/FALSE);
  /* Generate code for the destructor call. */
  if (ipdp->whole_array) {
    /* Destruction of whole array. */
    add_array_destructor_call(dip, entity_node, ipdp->array_element_count,
                              insert_location);
  } else {
    /* Destruction of simple entity (non-array). */
    add_destructor_call(dip, entity_node, have_complete_object,
                        insert_location);
  }  /* if */
  error_position = saved_error_position;
}  /* lower_destructor_dynamic_init */


static a_dynamic_init_ptr elem_dynamic_init(a_dynamic_init_ptr dip)
/*
dip points to a dynamic init entry that initializes a whole array.  Find
the dynamic init entry that applies to each element and return a pointer to it.
*/
{
  a_constant_ptr     con;
  a_dynamic_init_ptr elem_dip;

#if CHECKING
  if (dip->kind != (a_dynamic_init_kind)dik_nonconstant_aggregate) {
    internal_error("elem_dynamic_init: not nonconst aggregate");
  }  /* if */
#endif /* CHECKING */
  /* The nonconstant aggregate case has ck_aggregate constant ->
     ck_init_repeat constant -> ck_dynamic_init constant ->
     dynamic init entry. */
  con = dip->variant.constant;
  con = con->variant.aggregate.first_constant;
#if CHECKING
  if (con->kind != (a_constant_repr_kind)ck_init_repeat) {
    internal_error("elem_dynamic_init: not ck_init_repeat");
  }  /* if */
#endif /* CHECKING */
  con = con->variant.init_repeat.constant;
#if CHECKING
  if (con->kind != (a_constant_repr_kind)ck_dynamic_init) {
    internal_error("elem_dynamic_init: not ck_dynamic_init");
  }  /* if */
#endif /* CHECKING */
  elem_dip = con->variant.dynamic_init;
  return elem_dip;
}  /* elem_dynamic_init */


static void lower_array_new(an_expr_node_ptr expr)
/*
Do lowering of an array new operation.  expr points to the enk_new_delete
expression.  The subtrees of the original expressions have not been lowered
yet.  This routine is used for arrays that require special handling, i.e.,
arrays with class elements.
*/
{
  a_new_delete_supplement_ptr ndsp = expr->variant.new_delete;
  a_dynamic_init_ptr          dip = ndsp->dynamic_init, elem_dip;
  a_type_ptr                  array_type, elem_type, ptr_elem_type;
  an_expr_node_ptr            entity_node, new_node, temp_var_node;
  an_expr_node_ptr            assign_node, num_elem_node, vec_new_node;
  a_variable_ptr              temp_var;
  a_constant                  num_elem_constant;
  a_boolean                   preserve_size_node;
  a_targ_size_t               elem_size;
  an_expr_node_ptr            size_node, constant_node, nonconstant_node;
  a_constant                  size_constant, null_constant;
  a_targ_size_t               con_for_size;
  a_boolean                   ovflo;
  a_routine_ptr               ctor_routine, dtor_routine;

  /* Get the array element type. */
  array_type = skip_typerefs(ndsp->type);
  elem_type = new_delete_base_type_from_operation_type(ndsp->type);
  ptr_elem_type = make_pointer_type(elem_type);
  /* Build the node for the address of the array (entity_node). */
#if !NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE
 #error -- NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE wrong
#endif /* !NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE */
  if (ndsp->routine == NULL) {
    /* The __vec_new routine should do the allocation of the array (the normal
       case).  The entity_node is therefore a NULL pointer. */
    make_zero_of_proper_type(ptr_elem_type, &null_constant);
    entity_node = alloc_node_for_constant(&null_constant);
    /* Lower "arg" even though it is usually ignored.  It is used when the
       array size is nonconstant.  Note that it is not necessary to lower
       this as an argument list because it will not be used directly as
       such (pieces might be put into an argument list). */
    lower_expr_list(ndsp->arg, 0, FALSE);
    preserve_size_node = FALSE;
  } else {
    /* The allocation is not standard and must be done before calling
       the __vec_new routine.  This happens for something like
         A *p = new (x, y, z) A[3];
       The "new" call is assigned to a temporary, and entity_node uses
       the temporary, as in
         ((temp = (type *)new-call(...)), (type *)__vec_new(temp, ...))
       The comma expression is necessary because we can't count on the
       order of evaluation of arguments of the __vec_new call and the new-call
       may contain the assignment to a temporary needed to make a reusable
       copy of the size expression.  */
    /* Make the "new" call. */
    lower_arg_expr_list(ndsp->arg, ndsp->routine->type,
                        (a_param_type_ptr)NULL);
    new_node = make_call_node(ndsp->routine, ndsp->arg,
                              /*honor_virtual=*/FALSE);
    /* Make "temp = (type *)new-call(...)". */
    temp_var = make_lowered_temporary(ptr_elem_type);
    temp_var_node = var_lvalue_expr(temp_var);
    temp_var_node->next = add_cast_if_necessary(new_node, ptr_elem_type);
    assign_node = make_operator_node((an_expr_operator_kind)eok_passign,
                                     ptr_elem_type, temp_var_node);
    /* The comma node is built at the end of this routine. */
    entity_node = var_rvalue_expr(temp_var);
    /* The size node is used in the "new" call, so it cannot be destroyed. */
    preserve_size_node = TRUE;
  }  /* if */
  /* Here, we have entity_node pointing to an expression for the address
     of the entity. */
  /* Make a node for the number of elements in the array. */
  if (array_type->size != 0) {
    /* The easy and usual case -- the array has a constant number of
       elements.  Do a division to get the right answer for the
       multi-dimensional array case. */
    set_unsigned_integer_constant(&num_elem_constant,
                                  array_type->size / elem_type->size,
                                  targ_size_t_int_kind);
    num_elem_node = alloc_node_for_constant(&num_elem_constant);
  } else {
    /* Nonconstant number of elements in the array.  The number of elements
       must be extracted from the size expression.  If preserve_size_node
       is TRUE, the size expression is used in the "new" call, and therefore
       a reusable copy must be made of whatever part is reused here. */
    /* Get the node that gives the size of the allocation. */
    size_node = ndsp->arg;
    /* Get the size of each element, in bytes. */
    elem_size = elem_type->size;
    if (elem_size == 1) {
      /* The element size is 1, so the number of elements is equal to the
         total size. */
      if (preserve_size_node) {
        /* size_node must be preserved, so make a copy of it. */
        num_elem_node = make_reusable_copy(size_node,
                                           /*vars_can_change=*/TRUE);
      } else {
        num_elem_node = size_node;
      }  /* if */
    } else {
      /* A division by the element size is required.  The size expression
         should look like "expr*n" where "n" is the element size,
         i.e., a multiplication added while scanning the "new" to convert
         the number of elements to the total size.  The usual strategy
         is to remove the "*n".  Note however that for a multi-dimensional
         array case "n" is the product of the element size and the dimension
         bounds after the first; for that case we create a new constant
         that is "n" divided by the element size. */
      /* Drop any cast on the top of the expression, such as one added by
         lower_arg_expr_list to promote the expression for calling an old-style
         function. */
      while (is_operation_node(size_node) &&
             size_node->variant.operation.kind ==
                                             (an_expr_operator_kind)eok_cast) {
        size_node = size_node->variant.operation.operands;
      }  /* if */
      check_assertion(is_operation_node(size_node) &&
                      size_node->variant.operation.kind ==
                                         (an_expr_operator_kind)eok_imultiply);
      nonconstant_node = size_node->variant.operation.operands;
      constant_node = nonconstant_node->next;
      check_assertion(is_constant_node(constant_node));
      if (preserve_size_node) {
        /* We need to preserve size_node, and therefore we need a copy of the
           nonconstant node. */
        nonconstant_node = make_reusable_copy(nonconstant_node,
                                              /*vars_can_change=*/TRUE);
      } else {
        /* We can use the expression directly.  Break the connection
           between the first operand and second operand of the "*"
           operation. */
        nonconstant_node->next = NULL;
      }  /* if */
      /* Divide the constant by the element size. */
      size_constant = *constant_node->variant.constant;
      check_assertion(size_constant.kind == (a_constant_repr_kind)ck_integer);
      con_for_size = unsigned_value_of_integer_constant(&size_constant,
                                                        &ovflo);
      check_assertion(!ovflo);
      /* Note that we know the type is not incomplete, so the element size
         is not zero. */
      con_for_size /= elem_size;
      if (con_for_size == 1) {
        /* No multiplication is needed. */
        num_elem_node = nonconstant_node;
      } else {
        /* The multiplication is still needed.  This must be a multi-
           dimensional array case. */
        set_unsigned_integer_value(&size_constant.variant.integer_value,
                                   (unsigned long)con_for_size);
        constant_node = alloc_node_for_constant(&size_constant);
        nonconstant_node->next = constant_node;
        num_elem_node = make_operator_node(
                                          (an_expr_operator_kind)eok_imultiply,
                                          nonconstant_node->type,
                                          nonconstant_node);
      }  /* if */
    }  /* if */
  }  /* if */
  /* Here, num_elem_node is an expression for the number of elements in
     the array. */
  /* Determine the constructor routine (if any) to be called. */
  if (dip != NULL) {
    /* There is a dynamic init entry to initialize the storage after it is
       allocated. */
    /* Get a pointer to the dynamic init entry that applies to the array
       elements instead of the whole array. */
    elem_dip = elem_dynamic_init(dip);
    check_assertion(elem_dip->kind == (a_dynamic_init_kind)dik_constructor) ;
    /* Get the constructor routine to call. */
    ctor_routine = elem_dip->variant.constructor.ptr;
    /* If the constructor has default arguments, make a routine that
       calls the constructor with the necessary default arguments. */
    /* Note that elem_dip->variant.constructor.args must not be lowered
       before passing it to default_version_of_routine. */
    ctor_routine = default_version_of_routine(ctor_routine,
                                           elem_dip->variant.constructor.args);
    /* If exceptions are enabled, a destructor will be specified if
       appropriate. */
    dtor_routine = dip->destructor;
  } else {
    /* There is no dynamic init entry; the storage is not initialized after
       allocation. */
    ctor_routine = NULL;
    dtor_routine = NULL;
  }  /* if */
  /* Construct the call of __vec_new. */
  vec_new_node = make_vec_new_call(entity_node, num_elem_node, ctor_routine,
                                   dtor_routine);
  if (ndsp->routine != NULL) {
    /* Build a comma node that encloses the allocation call and the
       __vec_new call.  See comment above. */
    assign_node->next = vec_new_node;
    vec_new_node = make_operator_node((an_expr_operator_kind)eok_comma,
                                      vec_new_node->type, assign_node);
  }  /* if */
  /* Overwrite expr with a cast of the result of __vec_new (of type void *)
     to the right pointer type. */
  change_to_cast(expr, vec_new_node, expr->type);
}  /* lower_array_new */


static void lower_array_delete(an_expr_node_ptr expr)
/*
Do lowering of an array delete operation.  expr points to the enk_new_delete
for the operation.  The subtrees of the expression have not been lowered
yet.  This routine is used for arrays that require special handling,
i.e., arrays with class elements.
*/
{
  a_new_delete_supplement_ptr ndsp = expr->variant.new_delete;
  a_dynamic_init_ptr          dip = ndsp->dynamic_init, elem_dip;
  a_routine_ptr               dtor_routine;
  an_expr_node_ptr            vec_delete_node;

  /* Lower "arg"; do it as a list in case the delete routine is the
     two-argument version.  Drop the second argument if present. */
  lower_expr_list(ndsp->arg, 0, FALSE);
  ndsp->arg->next = NULL;
  if (dip != NULL) {
    /* A destructor must be called. */
    /* Get a pointer to the dynamic init entry that applies to the array
       elements instead of the whole array. */
    elem_dip = elem_dynamic_init(dip);
    /* Get the destructor to call. */
    dtor_routine = elem_dip->destructor;
    check_assertion(dtor_routine != NULL);
  } else {
    /* There is no dynamic init entry, and therefore no destruction need be
       done along with the deallocation. */
    dtor_routine = NULL;
  }  /* if */
  vec_delete_node = make_vec_delete_call(ndsp->arg,
                                         /*array_element_count=*/-1L,
                                         dtor_routine,
                                         /*free_storage=*/TRUE);
  /* Overwrite the original node with the __vec_delete call. */
  overwrite_node(expr, vec_delete_node);
}  /* lower_array_delete */


static void lower_new(an_expr_node_ptr expr)
/*
Do IL lowering of an enk_new_delete expression node for a "new".
The subtree of the node has not yet been lowered.
*/
{
  a_new_delete_supplement_ptr ndsp = expr->variant.new_delete;
  a_dynamic_init_ptr          dip = ndsp->dynamic_init;
  a_type_ptr                  base_type, ptr_base_type;
  a_variable_ptr              temp_var;
  an_expr_node_ptr            temp_var_node, assign_node, compare_node;
  an_expr_node_ptr            init_node, call_node, null_node;
  a_constant                  null_constant;
  an_insert_location          insert_location;
  an_init_pos_descr           ipd;
  a_boolean                   keep_dynamic_init;
  a_cleanup_action_ptr        new_allocation_cap;
  
  base_type = new_delete_base_type_from_operation_type(ndsp->type);
  if (is_array_type(ndsp->type) &&
      new_or_delete_type_requires_array_handling(base_type)) {
    /* An array "new". */
    lower_array_new(expr);
#if NEW_CAN_BE_FOLDED_INTO_CTOR
  } else if (ndsp->routine == NULL) {
    /* The "new" call has been folded into the constructor call. */
    a_routine_ptr    ctor_routine = dip->variant.constructor.ptr;
    an_expr_node_ptr implied_arg_list, end_implied_arg_list;
    /* ndsp->arg is not lowered because it is thrown away. */
    /* Pass a NULL for the "this" parameter to tell the constructor to
       do the allocation. */
    make_zero_of_proper_type(make_pointer_type(base_type), &null_constant);
    null_node = alloc_node_for_constant(&null_constant);
    /* Add any implicit arguments for the constructor. */
    make_ctor_implied_arg_list(ctor_routine, &implied_arg_list,
                               &end_implied_arg_list);
    if (implied_arg_list != NULL) {
      null_node->next = implied_arg_list;
    } else {
      end_implied_arg_list = null_node;
    }  /* if */
    /* Preserve any additional parameters from the constructor call. */
    if (dip->variant.constructor.args != NULL) {
      lower_arg_expr_list(dip->variant.constructor.args,
                          ctor_routine->type, (a_param_type_ptr)NULL);
      end_implied_arg_list->next = dip->variant.constructor.args;
    }  /* if */
    /* Make the constructor call. */
    call_node = make_call_node(ctor_routine, null_node,
                               /*honor_virtual=*/FALSE);
    /* The constructor call returns a pointer to the object initialized.
       Cast the pointer to the right type if necessary. */
    call_node = add_cast_if_necessary(call_node, expr->type);
    /* Overwrite the enk_new_delete node with the call/cast. */
    overwrite_node(expr, call_node);
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
  } else {
    /* Non-array case, or array case that does not require special handling. */
    /* Create a call of the "new" routine. */
    lower_arg_expr_list(ndsp->arg, ndsp->routine->type,
                        (a_param_type_ptr)NULL);
    call_node = make_call_node(ndsp->routine, ndsp->arg,
                               /*honor_virtual=*/FALSE);
    /* Note that the type of the "new" call might be unrelated to the type
       we are allocating, e.g., it might be "void *"; a cast is done later. */
    if (dip != NULL) {
      /* Initialization is required.  It must be done only if the allocation
         succeeds, so build an expression like
           ((temp = (type *)new-call(...)) != NULL) ?
                                     (initialization, temp) : NULL
      */
      /* Allocate the temporary. */
      ptr_base_type = make_pointer_type(base_type);
      temp_var = make_lowered_temporary(ptr_base_type);
      temp_var_node = var_lvalue_expr(temp_var);
      /* Assign the entity address expression to the temporary. */
      temp_var_node->next = add_cast_if_necessary(call_node, ptr_base_type);
      assign_node = make_operator_node((an_expr_operator_kind)eok_passign,
                                       ptr_base_type, temp_var_node);
      /* Compare the assignment node to a NULL constant of the right type. */
      make_zero_of_proper_type(ptr_base_type, &null_constant);
      null_node = alloc_node_for_constant(&null_constant);
      assign_node->next = null_node;
      compare_node = make_operator_node((an_expr_operator_kind)eok_pne,
                                        integer_type((an_integer_kind)ik_int),
                                        assign_node);
      /* Start the initialization code as just the value of the temporary.
         Additional code will be inserted by putting comma operators on
         top of this node. */
      init_node = var_rvalue_expr(temp_var);
      set_expr_insert_location(init_node, &insert_location);
      if (ndsp->delete_routine != NULL) {
        /* Exceptions are enabled, so make a cak_new_allocation cleanup action
           entry to get the storage freed if a throw occurs. */
        new_allocation_cap =
                   alloc_cleanup_action(cak_new_allocation,
                                        /*applies_on_block_exit=*/FALSE,
                                        /*applies_on_exception_cleanup=*/TRUE);
        set_var_indirect_init_pos_descr(temp_var,
                           &new_allocation_cap->variant.object.init_pos_descr);
        /* Add information on the delete routine. */
        new_allocation_cap->variant.object.delete_routine =
                                                          ndsp->delete_routine;
        /* Remember the current full expression. */
        new_allocation_cap->variant.object.full_expression =
                                                          curr_full_expression;
        add_cleanup_action_to_context_list(new_allocation_cap, curr_context,
                                           &insert_location);
      }  /* if */
      /* Build a description of the entity to be initialized.  Adjust the
         type so that it is an array if necessary. */
      set_var_indirect_init_pos_descr(temp_var, &ipd);
      ipd.base_type = ndsp->type;
      /* Generate code for the initialization. */
      lower_dynamic_init(dip, &ipd,
                         /*is_expr_temporary=*/FALSE,
                         (an_expr_node_ptr)NULL, (an_expr_node_ptr)NULL,
                         (a_constructor_init_ptr)NULL,
                         &insert_location, &keep_dynamic_init);
      check_assertion(!keep_dynamic_init);
      if (ndsp->delete_routine != NULL) {
        /* Now that the region entry and the code to set the region number have
           been emitted, remove the cleanup action. */
        remove_cleanup_action(new_allocation_cap);
        set_eh_curr_region(curr_context, &insert_location);
      }  /* if */
      /* Build the ?: operation.  Its first argument is the comparison of
         the temp pointer against NULL; its second is the initialization code;
         and its third is another NULL constant of the right type. */
      make_zero_of_proper_type(ptr_base_type, &null_constant);
      null_node = alloc_node_for_constant(&null_constant);
      compare_node->next = init_node;
      init_node->next = null_node;
      call_node = make_operator_node((an_expr_operator_kind)eok_question,
                                     ptr_base_type, compare_node);
    }  /* if */
    /* Turn the original enk_new_delete node into a cast to the right
       pointer type. */
    change_to_cast(expr, call_node, expr->type);
  }  /* if */
}  /* lower_new */


static an_expr_node_ptr make_dtor_call_for_delete(
                                                 a_dynamic_init_ptr dip,
                                                 an_expr_node_ptr   ptr_node,
                                                 a_boolean          deallocate)
/*
Generate a call of a destructor as part of a delete operation.  dip points
to a dynamic initialization entry that indicates the destructor.  ptr_node
points to the object to be destroyed.  deallocate is TRUE if the destructor
should be asked to deallocate the storage.  If the destructor is virtual,
it is called as a virtual function, which involves some special tricks.
*/
{
  an_expr_node_ptr ptr_node_copy, call_node;
  an_expr_node_ptr operand_node, compare_node;
  a_constant       null_constant;
  a_routine_ptr    dtor_routine = dip->destructor;
  long             bit_mask;

  check_assertion(dtor_routine != NULL);
  /* Add an implicit parameter to the destructor call with bits
     0x2 (whole object) + 0x1 (free storage, if deallocate is TRUE). */
  bit_mask = 2L;
  if (deallocate) bit_mask |= 1L;
  ptr_node->next = node_for_integer_constant(bit_mask,
                                             (an_integer_kind)ik_int);
  /* Make a call of the destructor. */
  call_node = make_call_node(dtor_routine, ptr_node, /*honor_virtual=*/TRUE);
  if (dtor_routine->is_virtual) {
    /* The destructor is virtual, so rewrite the call. */
    /* Make a copy of the "this" argument so it can be used twice. */
    ptr_node_copy = make_reusable_copy(ptr_node, /*vars_can_change=*/FALSE);
    /* Put the copy under the original destructor call; the original gets
       tested for NULL. */
    operand_node = call_node->variant.operation.operands;
    ptr_node_copy->next = ptr_node->next;
    operand_node->next = ptr_node_copy;
    /* Rewrite the virtual call as a normal call. */
    lower_virtual_function_call(call_node);
    /* Also ... if the pointer to be deleted is NULL, one can't use it to
       look up a virtual function, and therefore the destructor cannot
       be the one to do the NULL pointer test.  We must add the test here
       above the destructor call.  The test inside the destructor is still
       needed for those cases where the routine is called non-virtually. */
    /* Make "ptr_node != NULL". */
    make_zero_of_proper_type(ptr_node->type, &null_constant);
    ptr_node->next = alloc_node_for_constant(&null_constant);
    compare_node = make_operator_node((an_expr_operator_kind)eok_pne,
                                      integer_type((an_integer_kind)ik_int),
                                      ptr_node);
    /* Make "(ptr_node != NULL) ? dtor(...) : (void)0". */
    compare_node->next = call_node;
    compare_node->next->next =
               add_cast(node_for_integer_constant(0L, (an_integer_kind)ik_int),
                        void_type());
    call_node = make_operator_node((an_expr_operator_kind)eok_question,
                                   call_node->type, compare_node);
  }  /* if */
  return call_node;
}  /* make_dtor_call_for_delete */


static void lower_delete(an_expr_node_ptr expr)
/*
Do IL lowering of an enk_new_delete expression node for a "delete".
The subtree of the node has not yet been lowered.
*/
{
  a_new_delete_supplement_ptr ndsp = expr->variant.new_delete;
  a_dynamic_init_ptr          dip = ndsp->dynamic_init;
  a_type_ptr                  base_type;
  an_expr_node_ptr            ptr_node = ndsp->arg, call_node, dtor_call_node;
  an_expr_node_ptr            second_arg_node;

  base_type = new_delete_base_type_from_operation_type(ndsp->type);
  if (ndsp->array_delete &&
      new_or_delete_type_requires_array_handling(base_type)) {
    /* An array "delete". */
    lower_array_delete(expr);
#if !DELETE_CAN_BE_FOLDED_INTO_DTOR
/* IL lowering requires that it be possible to fold the delete call into
   a destructor.  Without that, it has no way of getting the right size
   on a delete of a pointer to a class with a virtual destructor. */
 #error -- DELETE_CAN_BE_FOLDED_INTO_DTOR set wrong.
#endif /* !DELETE_CAN_BE_FOLDED_INTO_DTOR */
  } else if (ndsp->routine == NULL) {
    /* The "delete" call has been folded into the destructor call.
       Generate the destructor call with an implicit parameter to indicate
       deallocation. */
    /* Lower "arg"; do it as a list in case the delete routine is the
       two-argument version.  Drop the second argument if present. */
    lower_expr_list(ptr_node, 0, FALSE);
    ptr_node->next = NULL;
    dtor_call_node = make_dtor_call_for_delete(dip, ptr_node,
                                               /*deallocate=*/TRUE);
    /* Overwrite the enk_new_delete node with the call. */
    overwrite_node(expr, dtor_call_node);
  } else {
    /* Non-array case, or array case that does not require special handling. */
    /* Lower "arg"; do it as a list in case the delete routine is the
       two-argument version. */
    lower_arg_expr_list(ptr_node, ndsp->routine->type, (a_param_type_ptr)NULL);
    /* Break off the second argument if there is one; it will be reattached
       later after ptr_node has been messed with. */
    second_arg_node = ptr_node->next;
    ptr_node->next = NULL;
    if (dip != NULL) {
      /* Case like
           struct A { ~A(); } *p;
           ::delete p;
         This cannot be folded into the destructor call because the delete
         routine is not the standard one.  Make something like
           (dtor(p), delete(p))
      */
      an_expr_node_ptr orig_ptr_node = ptr_node;
      /* Make a reusable copy of the pointer node for use in the
         delete call. */
      ptr_node = make_reusable_copy(orig_ptr_node, /*vars_can_change=*/FALSE);
      dtor_call_node = make_dtor_call_for_delete(dip, orig_ptr_node,
                                                 /*deallocate=*/FALSE);
      /* The comma node is built later in this routine. */
    }  /* if */
    /* Make the "delete" call.  It is not necessary to test for non-NULL;
       the delete routine does that.  Cast the argument to "void *", which
       is what the delete routine expects. */
    ptr_node = add_cast_if_necessary(ptr_node, void_star_type());
    /* Reattach the second operand to delete is there is one. */
    ptr_node->next = second_arg_node;
    call_node = make_call_node(ndsp->routine, ptr_node,
                               /*honor_virtual=*/FALSE);
    if (dip != NULL) {
      /* Finish the destructor case by building the comma node. */
      dtor_call_node->next = call_node;
      call_node = make_operator_node((an_expr_operator_kind)eok_comma,
                                     call_node->type, dtor_call_node);
    }  /* if */
    /* Overwrite the enk_new_delete node with the call. */
    overwrite_node(expr, call_node);
  }  /* if */
}  /* lower_delete */


void lower_new_delete(an_expr_node_ptr expr)
/*
Do IL lowering of an enk_new_delete expression node, used for a "new" or
"delete".  The subtree of the node has not yet been lowered.
*/
{
  if (expr->variant.new_delete->is_new) {
    /* "new" case. */
    lower_new(expr);
  } else {
    /* "delete" case. */
    lower_delete(expr);
  }  /* if */
}  /* lower_new_delete */


void lower_temp_init(an_expr_node_ptr expr)
/*
Do IL lowering of an enk_temp_init expression node.
*/
{
  a_dynamic_init_ptr dip;
  a_type_ptr         temp_type;
  an_init_pos_descr  ipd;
  a_boolean          keep_dynamic_init, result_is_addr, result_is_not_used;
  an_insert_location insert_location;

  dip = expr->variant.init.dynamic_init;
  if (exceptions_enabled && dip->destructor != NULL) {
    /* If there are multiple enk_temp_init nodes in one expression and
       they are unsequenced with respect to one another, we have to add
       conditional flags on the initializations so that exception
       cleanup can tell which ones have been done at any given point.
       Look at the current full expression and see if there are
       unsequenced enk_temp_inits therein. */
    examine_curr_full_expression_for_unsequenced_temp_inits();
  }  /* if */
  /* Determine the type of the temporary. */
  temp_type = expr->type;
  result_is_not_used = expr->result_is_not_used;
  result_is_addr = expr->variant.init.result_is_addr;
  if (result_is_addr) {
    /* The value of the enk_temp_init node is the address of the temporary,
       so drop the pointer-to to get the temporary type. */
    temp_type = type_pointed_to(temp_type);
  }  /* if */
  /* Create a temporary variable.  If we're inside the file-scope
     initialization routine, make the temporary in the file scope because
     it may have to survive to the end of the program. */
  dip->variable = make_temporary_possibly_at_file_scope(
                                           temp_type,
                                           processing_file_scope_init_routine);
  /* Change the enk_temp_init to a reference to the value or address
     of the temporary. */
  if (result_is_addr) {
    set_expr_node_kind(expr, (an_expr_node_kind)enk_variable_address);
    /* The address of the temporary escapes (or might escape) into the
       surrounding context, so set its address_taken flag. */
    set_variable_address_taken(dip->variable);
  } else {
    set_expr_node_kind(expr, (an_expr_node_kind)enk_variable);
  }  /* if */
  expr->variant.variable = dip->variable;
  /* Generate code for the dynamic init. */
  set_var_init_pos_descr(dip->variable, &ipd);
  /* Any code generated for the dynamic initialization will be
     inserted before the (modified) original expression. */
  set_expr_insert_location(expr, &insert_location);
  lower_dynamic_init(dip, &ipd,
                     /*is_expr_temporary=*/TRUE,
                     (an_expr_node_ptr)NULL, (an_expr_node_ptr)NULL,
                     (a_constructor_init_ptr)NULL,
                     &insert_location, &keep_dynamic_init);
  check_assertion(!keep_dynamic_init);
  /* Optimization -- if the initialization is done by a constructor,
     and the enk_temp_init returns the address of the temporary,
     use the pointer returned from the constructor as the value of
     the expression.  Likewise, if the result of the expression is not
     used, the node for the temporary value or address is not needed. */
  if ((result_is_addr &&
      dip->kind == (a_dynamic_init_kind)dik_constructor) ||
      result_is_not_used) {
    /* Check for the form (ctor-call(args),  temp)
                       or (ctor-call(args), &temp) as appropriate.
       Note that we do not do the optimization if some other terms have
       been inserted (e.g., setting a conditional destruction flag). */
    if (is_operation_node(expr) &&
        expr->variant.operation.kind == (an_expr_operator_kind)eok_comma) {
      an_expr_node_ptr first_operand = expr->variant.operation.operands;
      an_expr_node_ptr second_operand = first_operand->next;
      if (is_operation_node(first_operand) &&
          first_operand->variant.operation.kind ==
                                             (an_expr_operator_kind)eok_call &&
          (result_is_addr ? is_variable_address_node(second_operand) :
                            is_variable_node(second_operand))) {
        /* The optimization is possible. */
        overwrite_node(expr, first_operand);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* lower_temp_init */


static void add_first_time_test(an_insert_location_ptr insert_location)
/*
Add a first-time test sequence that will surround the initialization of a
local static variable.  In effect:

  static int test_var;  // Global test var, implicitly init to 0
  {
    if (test_var == 0) {
      test_var = 1;
      ... real initialization of variable being initialized
    }
  }

The sequence is inserted at *insert_location.  *insert_location is updated
for further insertion after the assignment statement.
*/
{
  a_variable_ptr     test_var;
  an_expr_node_ptr   test_var_node, compare_node;
  an_insert_location insert_location2;
  a_type_ptr         int_type;

  /* Make the static first-time-test variable in the current scope. */
  int_type = integer_type((an_integer_kind)ik_int);
  test_var = make_unnamed_local_static_variable(int_type,
                                                /*in_function_scope=*/FALSE);
  /* Make "test_var == 0". */
  test_var_node = var_rvalue_expr(test_var);
  test_var_node->next = node_for_integer_constant(0L, (an_integer_kind)ik_int);
  compare_node = make_operator_node((an_expr_operator_kind)eok_ieq,
                                    int_type, test_var_node);
  /* Make an "if" statement and insert it into the program. */
  insert_if_statement(compare_node, insert_location, &insert_location2);
  /* Further inserts are done at the start of the block. */
  *insert_location = insert_location2;
  /* Make "test_var = 1" and insert it inside the "if" statement. */
  (void)insert_var_assignment_statement(test_var,
                                        (an_expr_operator_kind)eok_iassign,
                                        node_for_integer_constant(1L,
                                                      (an_integer_kind)ik_int),
                                        insert_location);
}  /* add_first_time_test */

#if TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE

static a_boolean add_static_data_member_init_guard_test(
                                          a_variable_ptr     variable,
                                          an_insert_location *insert_location,
                                          an_insert_location *insert_location2)
/*
variable is a static data member of a template.  If its initialization
requires guard code, insert the code as follows:

  int guard_var;  // Global test var, implicitly init to 0
  {
    if (guard_var == 0) {
      guard_var = 1;
      ... real initialization of static data member being initialized
    }
  }

The sequence is inserted at *insert_location.  *insert_location is updated
for insertion after the "if"; *insert_location2 is set for insertion after
the assignment statement inside the "if".
In an environment that instantiates everything and lets the linker eliminate
duplicates, the initialization code for a static data member of a template
causes some problems, because the initialization is placed in a file-scope
initialization routine.  There is no way for the linker to remove just
that code from an initialization routine, so guard code is used instead
to ensure that the initialization is done only once.  If there is a
specialization of the initialization of the static data member, that takes
precedence over the other initializations.  It does so by initializing the
guard variable to non-zero, thus locking out the other initializations.
This routine returns TRUE if guard code was emitted.
*/
{
  a_variable_ptr         test_var;
  an_expr_node_ptr       test_var_node, compare_node;
  a_constant             minus_one_constant;
  a_memory_region_number region_to_switch_back_to;
  a_boolean              guard_code_emitted = FALSE;

  /* Make the guard variable at the file scope. */
  test_var = make_instantiation_var("__SDG__", (an_integer_kind)ik_int,
                                    &variable->source_corresp);
  if (variable->specific_def) {
    /* This variable is a specialization of a template entity, so its
       initialization should take precedence over any initialization code
       for other instances.  Initialize the guard variable to -1 to lock out
       all other initialization code.  No test of the guard variable is
       needed here. */
    test_var->init_kind = (an_init_kind)initk_static;
    set_integer_constant(&minus_one_constant, -1L, (an_integer_kind)ik_int);
    switch_to_file_scope_region(&region_to_switch_back_to);
    test_var->initializer.constant =
                                  alloc_unshared_constant(&minus_one_constant);
    switch_back_to_original_region(region_to_switch_back_to);
  } else {
    /* This is not a specialization, so the guard variable must be tested
       here. */
    guard_code_emitted = TRUE;
    /* Make "test_var == 0". */
    test_var_node = var_rvalue_expr(test_var);
    test_var_node->next = node_for_integer_constant(0L,
                                                    (an_integer_kind)ik_int);
    compare_node = make_operator_node((an_expr_operator_kind)eok_ieq,
                                      integer_type((an_integer_kind)ik_int),
                                      test_var_node);
    /* Make an "if" statement and insert it into the program. */
    insert_if_statement(compare_node, insert_location, insert_location2);
    /* Make "test_var = 1" and insert it inside the "if" statement. */
    (void)insert_var_assignment_statement(test_var,
                                          (an_expr_operator_kind)eok_iassign,
                                          node_for_integer_constant(1L,
                                                      (an_integer_kind)ik_int),
                                          insert_location2);
  }  /* if */
  return guard_code_emitted;
}  /* add_static_data_member_init_guard_test */

#endif /* TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE */

void add_last_time_test(a_variable_ptr         test_var,
                        an_insert_location_ptr insert_location,
                        an_insert_location_ptr insert_location2)
/*
Add a sequence of code that tests the variable set on first-time initialization
of a variable.  This is used in deciding whether or not to call a
destructor on the variable.  The sequence is

    if (test_var != 0) {
      ... destruction of variable being destroyed
    }

The sequence is inserted at *insert_location.  *insert_location is updated
for further insertion following the "if".  *insert_location2 is set for
insertion within the "if".
*/
{
  an_expr_node_ptr test_var_node, compare_node;

  /* Make "test_var != 0". */
  test_var_node = var_rvalue_expr(test_var);
  test_var_node->next = node_for_integer_constant(0L, (an_integer_kind)ik_int);
  compare_node = make_operator_node((an_expr_operator_kind)eok_ine,
                                    integer_type((an_integer_kind)ik_int),
                                    test_var_node);
  /* Make an "if" statement and insert it into the program. */
  insert_if_statement(compare_node, insert_location, insert_location2);
}  /* add_last_time_test */


void lower_stmk_init(a_statement_ptr statement)
/*
Generate code for a stmk_init (dynamic initialization) statement.
*/
{
  a_dynamic_init_ptr dip = statement->variant.dynamic_init;
  a_boolean          non_C_case;

  /* Only lower the cases that do not come up in C: */
  non_C_case = FALSE;
  if (dip->destructor != NULL) {
    /* Initialization with a later destructor. */
    non_C_case = TRUE;
  }  /* if */
  switch (dip->kind) {
    case dik_none:
      break;
    case dik_zero:
      non_C_case = TRUE;
      break;
    case dik_constant:
      break;
    case dik_expression:
      if (dip->variable->storage_class == (a_storage_class)sc_static) {
        /* Initialization of a local static variable to an expression, as in
               int f() {static int i = j+1;}
        */
        non_C_case = TRUE;
      }  /* if */
      break;
    case dik_call_returning_class_via_cctor:
      /* Initialization from class returned via copy constructor. */
      non_C_case = TRUE;
      break;
    case dik_constructor:
      /* Initialization using a constructor. */
      non_C_case = TRUE;
      break;
    case dik_nonconstant_aggregate:
      /* Initialization to a nonconstant aggregate, as in
                  auto int a[3] = {1, i+1, 3};
      */
      non_C_case = TRUE;
      break;
#if CHECKING
    default:
      internal_error("lower_stmk_init: bad dynamic init kind");
#endif /* CHECKING */
  }  /* switch */
  if (non_C_case) {
    /* Rewrite a non-C case. */
    an_insert_location insert_location;
    a_boolean          keep_dynamic_init, is_local_static = FALSE;
    an_init_pos_descr  ipd;
    a_context          context;
    a_variable_ptr     var = dip->variable;

    set_insert_location(statement, &insert_location);
    if (var_is_return_value_variable(var)) {
      /* The variable being initialized is the return value optimization
         variable for the function.  Initialize the space provided by the
         caller instead; its address is given by an implicit parameter. */
      set_var_indirect_init_pos_descr(return_value_pointer_variable, &ipd);
      dip->variable = NULL;
    } else {
      /* Normal case (not the return value optimization variable). */
      set_var_init_pos_descr(var, &ipd);
      /* If the variable is a local static, add a first-time flag and a
         test. */
      if (var->storage_class == (a_storage_class)sc_static) {
        is_local_static = TRUE;
        add_first_time_test(&insert_location);
        /* Put a dependent-statement context around the lowering of
           the initialization so that any cleanup actions for code within
           the initialization will be emitted within the "if". */
        push_context(&context, curr_context->scope, /*subscope_region=*/TRUE);
      }  /* if */
    }  /* if */
    lower_dynamic_init(dip, &ipd, /*is_expr_temporary=*/FALSE,
                       (an_expr_node_ptr)NULL, (an_expr_node_ptr)NULL,
                       (a_constructor_init_ptr)NULL,
                       &insert_location, &keep_dynamic_init);
    if (!keep_dynamic_init) {
      /* Delete the stmk_init statement. */
      turn_statement_into_noop(statement);
    }  /* if */
    if (is_local_static) {
      /* Generate any cleanup actions for temporaries built within a
         first-time test conditional section. */
      gen_cleanup_actions(curr_context, &insert_location);
      pop_context();
    }  /* if */
  } else {
    /* Normal C case.  Lower the subtree if any. */
    switch (dip->kind) {
      case dik_constant:
        lower_constant(dip->variant.constant);
        break;
      case dik_expression:
        lower_normal_expr(dip->variant.expression);
        break;
#if CHECKING
      default:
        internal_error("lower_stmk_init: bad dynamic init kind (2)");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */
}  /* lower_stmk_init */


static a_variable_ptr implicit_virtual_base_parameter(
                                                a_type_ptr     class_type,
                                                a_type_ptr     base_class_type,
                                                a_variable_ptr this_param_var)
/*
Find the implicit virtual base class parameter under class_type that is
for the virtual base class base_class_type and return a pointer to it.
this_param_var points to the "this" parameter variable for class_type;
the implicit parameters follow it.
*/
{
  a_variable_ptr   vbase_param_var;
  a_base_class_ptr bcp;

  /* Find the base class entry under the main class that is for this
     same virtual base class.  While doing so, step through the added
     parameter entries so that at the end of the loop vbase_param_var
     is the added parameter for the base class indicated by base_class_type. */
  vbase_param_var = this_param_var;
  for (bcp = class_type->variant.class_struct_union.extra_info->base_classes;
       ;
       bcp = bcp->next) {
#if CHECKING
    if (bcp == NULL) {
      /* Virtual base class of a base class must be a virtual base class
         of the main class, by definition. */
      internal_error(
              "implicit_virtual_base_parameter: virtual base class not found");
    }  /* if */
#endif /* CHECKING */
    if (bcp->is_virtual) {
      vbase_param_var = vbase_param_var->next;
      /* Exit the inner loop when we've found the base class entry in
         the main class that is for the virtual base class of interest. */
      if (bcp->type == base_class_type) break;
    }  /* if */
  }  /* for */
#if CHECKING
  { a_type_ptr param_base_type =
                       f_skip_typerefs(type_pointed_to(vbase_param_var->type));
    if (param_base_type != base_class_type &&
        param_base_type != base_class_type->variant.class_struct_union.
                                               extra_info->type_as_subobject) {
      internal_error(
                    "implicit_virtual_base_parameter: param type not correct");
    }  /* if */
  }
#endif /* CHECKING */
  return vbase_param_var;
}  /* implicit_virtual_base_parameter */


static void lower_ctor_init(a_constructor_init_ptr ctor_init,
                            a_variable_ptr         this_param_var,
                            a_boolean              use_implicit_param,
                            a_type_ptr             class_type,
                            an_insert_location_ptr insert_location)
/*
Generate code to implement the constructor_init entry pointed to by ctor_init.
this_param_var is the "this" parameter variable for the overall object
being initialized, whose class is class_type.  Implicit parameters for
virtual base classes, if any, follow the this_param_var.  If use_implicit_param
is TRUE, the entity being initialized is a virtual base class of class_type
and its address is available in an implicit parameter.  The statement(s)
created are inserted at *insert_location, and *insert_location is updated.
*/
{
  a_type_ptr           base_class_type;
  a_variable_ptr       vbase_param_var;
  a_base_class_ptr     bcp;
  an_expr_node_ptr     implied_arg_node;
  an_expr_node_ptr     implied_arg_list = NULL, end_implied_arg_list = NULL;
  a_dynamic_init_ptr   dip;
  an_init_pos_descr    ipd;
  an_init_pos_modifier ipm;
  a_boolean            keep_dynamic_init;

  dip = ctor_init->initializer;
  if (ctor_init->kind == (a_constructor_init_kind)cik_virtual_base_class ||
      ctor_init->kind == (a_constructor_init_kind)cik_direct_base_class) {
    /* Initializing a base class. */
    base_class_type = ctor_init->variant.base_class->type;
    /* Develop a position description for the entity to initialize. */
    if (use_implicit_param) {
      /* The sub-entity is a virtual base class and there is a parameter
         pointing to it. */
      vbase_param_var = implicit_virtual_base_parameter(class_type,
                                                        base_class_type,
                                                        this_param_var);
      set_var_indirect_init_pos_descr(vbase_param_var, &ipd);
    } else {
      /* Simple case; develop the entity position description. */
      develop_ctor_init_pos_descr(ctor_init, this_param_var, &ipd, &ipm);
    }  /* if */
    /* For a base class initialized by a constructor call, build a list
       of implicit virtual base class pointer arguments.  The required
       entries are expressions providing the value of the associated virtual
       base class pointer parameter for each virtual base class of the base
       class. */
    if (dip->kind == (a_dynamic_init_kind)dik_constructor) {
      for (bcp = base_class_type->variant.class_struct_union.extra_info->
                                                                  base_classes;
           bcp != NULL;
           bcp = bcp->next) {
        if (bcp->is_virtual) {
          /* Find the implicit virtual base parameter under the main class
             that is for this same virtual base class. */
          vbase_param_var = implicit_virtual_base_parameter(class_type,
                                                            bcp->type,
                                                            this_param_var);
          /* Build an expression specifying the value of the appropriate
             virtual base class parameter, and add it to the list. */
          implied_arg_node = var_rvalue_expr(vbase_param_var);
          if (implied_arg_list == NULL) {
            implied_arg_list = implied_arg_node;
          } else {
            end_implied_arg_list->next = implied_arg_node;
          }  /* if */
          end_implied_arg_list = implied_arg_node;
        }  /* if */
      }  /* for */
    }  /* if */
  } else {
    /* Initializing something other than a base class. */
    /* Develop a position description for the entity to initialize. */
    develop_ctor_init_pos_descr(ctor_init, this_param_var, &ipd, &ipm);
  }  /* if */
  /* Generate the code to do the initialization. */
  lower_dynamic_init(dip, &ipd, /*is_expr_temporary=*/FALSE,
                     implied_arg_list, end_implied_arg_list, ctor_init,
                     insert_location, &keep_dynamic_init);
#if CHECKING
  if (keep_dynamic_init) {
    internal_error("lower_ctor_init: keep_dynamic_init unexpected");
  }  /* if */
#endif /* CHECKING */
}  /* lower_ctor_init */


void add_constructor_wrapper_code(a_scope_ptr        scope,
                                  an_insert_location *insert_location)
/*
Insert constructor wrapper code at the indicated location in the indicated
constructor scope.  The insert location is usually at the beginning of the
constructor, but may instead be after an assignment to "this".
*/
{
  a_base_class_ptr       bcp;
  a_variable_ptr         this_param_var, vbase_param_var;
  a_type_ptr             class_type;
  a_class_type_supplement_ptr
                         ctsp;
  a_constructor_init_ptr ctor_init;
  a_constant             null_constant;
  an_insert_location     insert_location2;
  an_expr_node_ptr       null_constant_node, vbase_param_node, compare_node;
  an_expr_node_ptr       vaddr_node, vbptr_node, vtbl_addr_node, vptr_node;
  a_variable_ptr         primary_vtbl_var, vtbl_var;
  a_source_position      saved_error_position, saved_code_pos;

  /* The following pseudo-code shows both the processing in this routine
     and the code added to the constructor routine.  Lines enclosed in [...]
     are tests and loops done in the processing in this routine; other
     lines are the code added to the constructor routine.

     [If current class has any virtual base classes:]
       If the first added parameter == NULL (indicating a complete object is
           being initialized and virtual base classes must be constructed):
         [For each virtual base class of the current class:]
           Set the parameter to the address of the virtual base class.
         [endfor]
         [For each virtual base class on the ctor-initializer list:]
           Call the constructor for the base class (arguments as indicated by
               the ctor-initializer list, plus any virtual base class pointer
               arguments, using the added parameters for those).
         [endfor]
       endif
       [For each virtual base class of the current class:]
         If the virtual base class pointer for the base class is allocated
             in the current class, initialize it to point to the base class.
             (The pointers allocated in base classes are set by the constructor
             calls for those base classes.)
       [endfor]
     [endif]
     [For each initialized direct nonvirtual base class (entries for these
         appear as the middle of the ctor-initializer list):]
       Call the constructor for the base class (arguments as indicated by
         the ctor-initializer list, plus any virtual base class pointer
         arguments, using the added parameters for those).
     [endfor]
     [If the current class has any virtual functions:]
       Set the virtual function table pointer in the current class.
     [endif]
     [For each base class of the current class:]
       [If the base class needs a virtual function table instance distinct
           from the derived class instance:]
         Set the virtual function table pointer in the base class.  Virtual
             base classes must be accessed using the virtual base class
             pointer parameters.
       [endif]
     [endfor]
     [For each initialized data member (entries for these are the rest
         of the ctor-initializer list):]
       Do the initialization (a constructor call or some other dynamic
           initialization).
     [endfor]
  */
  saved_code_pos = code_pos_for_lowering;
  saved_error_position = error_position;
  code_pos_for_lowering = error_position = 
                      scope->variant.routine.ptr->source_corresp.decl_position;
  /* The constructor_inits list contains a list of initializations.  Each
     initialization either appeared explicitly in the source or is a default
     initialization supplied by the front end.  Every base class and member
     that requires a constructor appears, in the order (1) virtual base
     classes, (2) normal base classes, (3) data members.  The order within
     each section is source declaration order. */
  ctor_init = scope->variant.routine.constructor_inits;
  /* The list is not cleared here, because it may be used again if there
     is more than one assignment to "this" in a constructor. */
  /* Get a pointer to the "this" parameter variable. */
  this_param_var = scope->variant.routine.parameters;
  class_type =
            scope->variant.routine.ptr->source_corresp.class_of_which_a_member;
  /* Mark the class as referenced because, at the very least, the
     "this" parameter uses it. */
  class_type->source_corresp.referenced = TRUE;
  ctsp = class_type->variant.class_struct_union.extra_info;
  if (class_type->variant.class_struct_union.any_virtual_base_classes) {
    /* Put out code that tests whether or not the virtual base classes need
       to be initialized.  This is done by testing whether or not the
       first added parameter is NULL. */
    vbase_param_var = this_param_var->next;
    /* Make a NULL pointer constant of the right type. */
    make_zero_of_proper_type(vbase_param_var->type, &null_constant);
    /* Make an expression node pointing to the NULL constant. */
    null_constant_node = alloc_node_for_constant(&null_constant);
    /* Make an expression node for the parameter. */
    vbase_param_node = var_rvalue_expr(vbase_param_var);
    /* Make a node comparing the parameter against NULL. */
    vbase_param_node->next = null_constant_node;
    compare_node = make_operator_node((an_expr_operator_kind)eok_peq,
                                      integer_type((an_integer_kind)ik_int),
                                      vbase_param_node);
    /* Make an "if" statement with a block statement under it:
         if (param == NULL) {}
                             ^--- additional statements will be inserted.
    */
    insert_if_statement(compare_node, insert_location, &insert_location2);
    /* Set the added parameters to the addresses of the virtual base
       classes. */
    for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
      if (bcp->is_virtual) {
        /* Make an expression whose value is the address of the virtual
           base class. */
        vaddr_node = make_vbase_class_lvalue_from_var(this_param_var,
                                                      bcp,
                                                     /*complete_object=*/TRUE);
        /* Add a cast if necessary to convert from a pointer to the base
           class type to a pointer to the type-as-subobject for the base
           class type. */
        vaddr_node = add_cast_if_necessary(vaddr_node, vbase_param_var->type);
        /* Make an assignment to set the virtual base class parameter. */
        (void)insert_var_assignment_statement(vbase_param_var,
                                            (an_expr_operator_kind)eok_passign,
                                              vaddr_node,
                                              &insert_location2);
        /* Move on to the next added parameter for the next iteration of
           the loop. */
        vbase_param_var = vbase_param_var->next;
      }  /* if */
    }  /* for */
    /* Initialize any virtual base classes on the ctor_init list. */
    for (; ctor_init != NULL &&
            ctor_init->kind == (a_constructor_init_kind)cik_virtual_base_class;
         ctor_init = ctor_init->next) {
      lower_ctor_init(ctor_init, this_param_var, /*use_implicit_param=*/TRUE,
                      class_type, &insert_location2);
    }  /* for */
    /* Note that the "if" created above effectively ends here.  The code
       created below is executed even when we do not have a complete object. */
    /* For each virtual base class of the current class, set the
       virtual base class pointer in the current class to point to the value
       of the associated virtual base class parameter, i.e., the address
       of the virtual base class. */
    vbase_param_var = this_param_var;
    for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
      if (bcp->is_virtual) {
        vbase_param_var = vbase_param_var->next;
        /* Do not set the pointer if it is shared with a base class --
           the base class constructor has already or will set it. */
        if (bcp->pointer_base_class == NULL) {
          /* Make an expression for the value of the implicit parameter. */
          vbase_param_node = var_rvalue_expr(vbase_param_var);
          /* Make an expression node for the address of the virtual base
             class pointer. */
          vbptr_node = make_vbptr_field_lvalue_from_var(this_param_var, bcp);
          /* Add a cast if necessary to convert from a pointer to the
             type-as-subobject for the base class type to a pointer to the
             base class type. */
          vbase_param_node = add_cast_if_necessary(vbase_param_node,
                                            type_pointed_to(vbptr_node->type));
          /* Make an assignment statement that copies the implicit parameter
             value (set earlier in the constructor code) into the virtual
             base class pointer. */
          (void)insert_assignment_statement(vbptr_node,
                                            (an_expr_operator_kind)eok_passign,
                                            vbase_param_node,
                                            insert_location);
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  /* Generate initialization for each non-virtual base class that appears on
     the ctor_init list. */
  for (; ctor_init != NULL &&
          ctor_init->kind == (a_constructor_init_kind)cik_direct_base_class;
       ctor_init = ctor_init->next) {
    lower_ctor_init(ctor_init, this_param_var, /*use_implicit_param=*/FALSE,
                    class_type, insert_location);
  }  /* for */
  /* If the current class has any virtual functions, generate code to
     set the virtual function table pointer in the current class. */
  primary_vtbl_var = ctsp->virtual_function_table_var;
  if (primary_vtbl_var != NULL) {
    /* Assign the primary virtual table address to the virtual table pointer
       in the current class. */
    vtbl_addr_node = make_vtbl_address_node(primary_vtbl_var);
    set_variable_address_taken(primary_vtbl_var);
    primary_vtbl_var->source_corresp.referenced = TRUE;
    vptr_node = make_vptr_field_lvalue_from_var(this_param_var);
    (void)insert_assignment_statement(vptr_node,
                                      (an_expr_operator_kind)eok_passign,
                                      vtbl_addr_node,
                                      insert_location);
  }  /* if */
  /* Set the virtual function table pointer in any base classes for which
     that is required. */
  /* Loop through the base classes of the current class. */
  for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
    /* Set the pointer if there is one. */
    vtbl_var = bcp->virtual_function_table_var;
    if (vtbl_var != NULL) {
      /* The base class's virtual function table pointer must be set to
         reflect the fact that it exists as a subobject inside the current
         class. */
      vtbl_addr_node = make_vtbl_address_node(vtbl_var);
      set_variable_address_taken(vtbl_var);
      vtbl_var->source_corresp.referenced = TRUE;
      if (!bcp->is_virtual) {
        /* For non-virtual base classes, use the usual code.  Note that if
           the base class here is non-virtual itself but is inside a virtual
           base class, the code will use a pointer to get to the virtual base
           class and then field selection(s) to get to the non-virtual base
           class within that.  It would be possible to use the implicit
           parameter for the virtual base class to do better, but this code
           works (the virtual base class pointers are all set by this
           point). */
        vptr_node = make_base_class_lvalue_from_var(this_param_var, bcp,
                                                    /*complete_object=*/FALSE);
      } else {
        /* For virtual base classes, access the class by using the implicit
           parameter.  That works even when the current class is not a
           complete object, and is a little better than the general code. */
        vbase_param_var = implicit_virtual_base_parameter(class_type,
                                                          bcp->type,
                                                          this_param_var);
        vptr_node = var_rvalue_expr(vbase_param_var);
      }  /* if */
      vptr_node = make_vptr_field_lvalue(vptr_node);
      /* Make and insert the assignment statement. */
      (void)insert_assignment_statement(vptr_node,
                                        (an_expr_operator_kind)eok_passign,
                                        vtbl_addr_node,
                                        insert_location);
    }  /* if */
  }  /* for */
  /* Generate initialization for each data member that appears on the
     ctor_init list. */
  for (; ctor_init != NULL; ctor_init = ctor_init->next) {
    lower_ctor_init(ctor_init, this_param_var, /*use_implicit_param=*/FALSE,
                    class_type, insert_location);
  }  /* for */
  error_position = saved_error_position;
  code_pos_for_lowering = saved_code_pos;
}  /* add_constructor_wrapper_code */


void lower_constructor_code(a_scope_ptr scope)
/*
Insert constructor wrapper code around the user code in the indicated
constructor scope, and also lower the user code.
*/
{
  a_statement_ptr    user_code_stmts =
                                  scope->assoc_block->variant.block.statements;
  a_statement_ptr    last_statement;
  an_insert_location insert_location;
  a_source_position  saved_error_position, saved_code_pos;
  a_routine_ptr      ctor_routine = scope->variant.routine.ptr;

  saved_code_pos = code_pos_for_lowering;
  saved_error_position = error_position;
  code_pos_for_lowering = error_position = 
                                    ctor_routine->source_corresp.decl_position;
#if ASSIGNMENT_TO_THIS_ALLOWED
  /* Assignment to "this" is allowed. */
  /* If there is an assignment to "this" in the body of the constructor,
     do not issue the wrapper code here; it is issued after each
     assignment to "this". */
  if (!ctor_routine->assignment_to_this_done) {
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
    /* Add the wrapper code at the start of the routine. */
    set_block_start_insert_location(scope->assoc_block, &insert_location);
    add_constructor_wrapper_code(scope, &insert_location);
#if ASSIGNMENT_TO_THIS_ALLOWED
  }  /* if */
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */

  /* Lower the user code in the constructor. */
  lower_statement_list(user_code_stmts, &last_statement);

#if NEW_CAN_BE_FOLDED_INTO_CTOR
  /* Add code to allocate storage if "this" is NULL:
       if (this != NULL || (this = new-rout(size)) != NULL)
     The entire rest of the routine (both wrapper code and user code)
     is placed in the dependent statement of the "if". */
#if ASSIGNMENT_TO_THIS_ALLOWED
  /* The allocation code is not added if there is an assignment to "this"
     in the constructor. */
  if (!ctor_routine->assignment_to_this_done)
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
  /* Do not add code here -- this is the dependent statement of the "if"
     above. */
  {
    a_variable_ptr     this_param_var = scope->variant.routine.parameters;
    a_type_ptr         class_type, int_type;
    an_expr_node_ptr   size_node, call_node, assign_node;
    an_expr_node_ptr   new_compare_node, if_node, this_param_node;
    an_expr_node_ptr   null_constant_node, this_compare_node;
    a_statement_ptr    block_stmt;
    a_constant         null_constant;
    a_class_type_supplement_ptr
                       ctsp;
    a_routine_ptr      new_routine;
    a_cleanup_action_ptr
                       new_allocation_cap;

    /* Make "new-rout(size)". */
    class_type = ctor_routine->source_corresp.class_of_which_a_member;
    ctsp = class_type->variant.class_struct_union.extra_info;
    new_routine = ctsp->assoc_operator_new_routine;
    /* If there is no default new routine for the class, do not put out
       the code.  This happens if the class has a class-specific new but
       not one that takes a single argument. */
    if (new_routine != NULL) {
      size_node = node_for_integer_constant((long)class_type->size,
                                            targ_size_t_int_kind);
      call_node = make_call_node(new_routine, size_node,
                                 /*honor_virtual=*/FALSE);
      /* Make "this = new_rout(size)". */
      call_node = add_cast_if_necessary(call_node,
                                        f_skip_typerefs(this_param_var->type));
      this_param_node = var_lvalue_expr(this_param_var);
      this_param_node->next = call_node;
      assign_node = make_operator_node((an_expr_operator_kind)eok_passign,
                                       call_node->type, this_param_node);
      if (exceptions_enabled) {
        a_variable_ptr cond_var;
        /* Exceptions are enabled.  Record the allocation so it can
           be freed if a throw occurs while this routine is running. */
        /* "this = new_rout(size)" is turned into
             (this = new_rout(size), (exception_code, this))
        */
        this_param_node = var_rvalue_expr(this_param_var);
        assign_node->next = this_param_node;
        assign_node = make_operator_node((an_expr_operator_kind)eok_comma,
                                         this_param_node->type, assign_node);
        set_expr_insert_location(this_param_node, &insert_location);
        /* Make an indicator variable that is set to nonzero if the allocation
           is done.  The variable is initialized to zero by
           calling init_conditional_flag_var later. */
        cond_var = 
                 make_lowered_temporary(integer_type((an_integer_kind)ik_int));
        /* Set the indicator variable to nonzero. */
        (void)insert_var_assignment_statement(cond_var,
                                            (an_expr_operator_kind)eok_iassign,
                                            node_for_integer_constant(1L,
                                                      (an_integer_kind)ik_int),
                                            &insert_location);
        /* Add the cleanup action. */
        new_allocation_cap = alloc_cleanup_action(
                                        cak_new_allocation,
                                        /*applies_on_block_exit=*/FALSE,
                                        /*applies_on_exception_cleanup=*/TRUE);
        new_allocation_cap->constructor_wrapper_cleanup = TRUE;
        set_var_indirect_init_pos_descr(this_param_var,
                           &new_allocation_cap->variant.object.init_pos_descr);
        /* The deletion is only done if the indicator variable is set. */
        new_allocation_cap->variant.object.conditional_flag_var = cond_var;
        /* Add information on the delete routine. */
        new_allocation_cap->variant.object.delete_routine =
                                           ctsp->assoc_operator_delete_routine;
        add_cleanup_action_to_context_list(new_allocation_cap, curr_context,
                                           &insert_location);
      }  /* if */
      /* Make "(this = new_rout(size)) != NULL". */
      make_zero_of_proper_type(this_param_var->type, &null_constant);
      null_constant_node = alloc_node_for_constant(&null_constant);
      assign_node->next = null_constant_node;
      int_type = integer_type((an_integer_kind)ik_int);
      new_compare_node = make_operator_node((an_expr_operator_kind)eok_pne,
                                            int_type, assign_node);
      /* Make "this != NULL || (this = new-rout(size)) != NULL". */
      this_param_node = var_rvalue_expr(this_param_var);
      make_zero_of_proper_type(this_param_var->type, &null_constant);
      null_constant_node = alloc_node_for_constant(&null_constant);
      this_param_node->next = null_constant_node;
      this_compare_node = make_operator_node((an_expr_operator_kind)eok_pne,
                                             int_type, this_param_node);
      this_compare_node->next = new_compare_node;
      if_node = make_operator_node((an_expr_operator_kind)eok_lor,
                                   int_type, this_compare_node);
      /* Make "if (this != NULL || (this = new-rout(size)) != NULL)". */
      enclose_routine_in_if(scope, if_node, &block_stmt, this_param_var);
      if (exceptions_enabled) {
        /* Initialize the conditional flag to zero.  This must be done after
           enclose_routine_in_if is called so that the initialization is
           done at the right place (i.e., outside the "if"). */
        init_conditional_flag_var(new_allocation_cap,
                                  (an_insert_location *)NULL);
      }  /* if */
    }  /* if */
  } 
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
  /* Clear the list of constructor inits. */
  scope->variant.routine.constructor_inits = NULL;
  error_position = saved_error_position;
  code_pos_for_lowering = saved_code_pos;
}  /* lower_constructor_code */


static void lower_dtor_init(a_constructor_init_ptr ctor_init,
                            a_variable_ptr         this_param_var,
                            a_boolean              have_complete_object,
                            an_insert_location_ptr insert_location)
/*
Generate code to implement the constructor_init entry pointed to by ctor_init,
one that appears on the constructor_init list for a destructor.
this_param_var is the "this" parameter variable for the overall object
being destroyed.  If have_complete_object is TRUE, the entity being
destroyed is a complete object.  The statements created are inserted
at *insert_location, and *insert_location is updated.
*/
{
  an_init_pos_descr    ipd;
  an_init_pos_modifier ipm;
  a_dynamic_init_ptr   dip;
  a_boolean            keep_constant;

  /* Develop a position description for the entity to destroy. */
  develop_ctor_init_pos_descr(ctor_init, this_param_var, &ipd, &ipm);
  dip = ctor_init->initializer;
  if (dip->kind == (a_dynamic_init_kind)dik_nonconstant_aggregate) {
    /* Odd case: destructor for an array.  The top level looks like an
       initialization; lower down we will find dynamic init entries for
       the destruction. */
#if CHECKING
    if (ctor_init->kind != (a_constructor_init_kind)cik_field) {
      internal_error("lower_dtor_init: aggr value for non-field");
    }  /* if */
    keep_constant = FALSE;
#endif /* CHECKING */
    lower_dynamic_init_aggregate_constant(dip->variant.constant, &ipd,
                                          /*dtor_case=*/TRUE,
                                          (a_constructor_init_ptr)NULL,
                                          insert_location,
                                          &keep_constant);
#if CHECKING
    if (keep_constant) {
      internal_error("lower_dtor_init: keep_constant unexpected");
    }  /* if */
#endif /* CHECKING */
  } else {
    /* Normal case; generate the code to do the destruction. */
    lower_destructor_dynamic_init(dip, &ipd, (a_cleanup_action_ptr)NULL,
                                  have_complete_object, insert_location);
  }  /* if */
}  /* lower_dtor_init */


void lower_destructor_code(a_scope_ptr scope)
/*
Insert destructor wrapper code around the user code in the indicated
destructor scope, and also lower the user code.
*/
{
  a_base_class_ptr       bcp;
  a_variable_ptr         this_param_var, complete_obj_param_var;
  a_type_ptr             class_type, int_type;
  a_class_type_supplement_ptr
                         ctsp;
  a_constructor_init_ptr ctor_init;
  an_insert_location     insert_location, insert_location2;
  a_statement_ptr        user_code_stmts, epilogue_block;
  a_statement_ptr        top_level_stmt, prev_stmt, label_stmt;
  an_expr_node_ptr       zero_constant_node, complete_obj_param_node;
  an_expr_node_ptr       compare_node;
  an_expr_node_ptr       vtbl_addr_node, vptr_node;
  a_variable_ptr         primary_vtbl_var, vtbl_var;
  a_routine_ptr          dtor_routine = scope->variant.routine.ptr;
  a_return_memo_ptr      rmp, rmp_next;
  a_label_ptr            epilogue_label;
  a_source_position      saved_error_position, saved_code_pos;

  /* The following pseudo-code shows both the processing in this routine
     and the code added to the destructor routine.  Lines enclosed in [...]
     are tests and loops done in the processing in this routine; other
     lines are the code added to the destructor routine.

     [If a delete can be folded into the destructor:]
       If this != NULL test around entire routine.
     [endif]
     [If the current class has any virtual functions:]
       Set the virtual function table pointer in the current class.
     [endif]
     [For each base class of the current class:]
       [If the base class needs a virtual function table instance distinct
           from the derived class instance:]
         Set the virtual function table pointer in the base class.  Virtual
             base classes must be accessed through the virtual base class
             pointer.
       [endif]
     [endfor]
     ... user destructor code goes here ...
         -- returns in the user code are turned into gotos to the following
            code:
     [For each data member on the ctor-initializer list:]
       Call the destructor.  The complete-object implicit argument is TRUE.
     [endfor]
     [For each direct nonvirtual base class on the ctor-initializer list:]
       Call the destructor.  The complete-object implicit argument is FALSE.
     [endfor]
     [If there are any items left on the ctor-initializer list (which
         must be for virtual base classes):]
       If the added parameter != 0 (indicating a complete object is
           being destroyed and virtual base classes must be destroyed):
         [For each virtual base class on the ctor-initializer list:]
           Call the destructor.  The complete-object implicit argument is 0.
         [endfor]
       endif
     [endif]
     If (added parameter & 0x1) != 0:
       delete((void)*this)
     endif
     return;
  */
  saved_code_pos = code_pos_for_lowering;
  saved_error_position = error_position;
  code_pos_for_lowering = error_position = 
                                    dtor_routine->source_corresp.decl_position;
  int_type = integer_type((an_integer_kind)ik_int);
  /* Get a pointer to the "this" parameter variable. */
  this_param_var = scope->variant.routine.parameters;
  complete_obj_param_var = this_param_var->next;
  class_type = dtor_routine->source_corresp.class_of_which_a_member;
  /* Mark the class as referenced because, at the very least, the
     "this" parameter uses it.  For some cases involving generated virtual
     destructors, this is necessary. */
  class_type->source_corresp.referenced = TRUE;
  ctsp = class_type->variant.class_struct_union.extra_info;
  /* Remember where the user code (if any) is. */
  user_code_stmts = scope->assoc_block->variant.block.statements;
  /* Generate prologue code at the start of the routine. */
  set_block_start_insert_location(scope->assoc_block, &insert_location);
  /* If the current class has any virtual functions, generate code to
     set the virtual function table pointer in the current class. */
  primary_vtbl_var = ctsp->virtual_function_table_var;
  if (primary_vtbl_var != NULL) {
    /* Assign the primary virtual table address to the virtual table pointer
       in the current class. */
    vtbl_addr_node = make_vtbl_address_node(primary_vtbl_var);
    set_variable_address_taken(primary_vtbl_var);
    primary_vtbl_var->source_corresp.referenced = TRUE;
    vptr_node = make_vptr_field_lvalue_from_var(this_param_var);
    (void)insert_assignment_statement(vptr_node,
                                      (an_expr_operator_kind)eok_passign,
                                      vtbl_addr_node,
                                      &insert_location);
  }  /* if */
  /* For each base class of this class that needs it, generate code to
     set the virtual function table pointer in the base class.  This gets
     rid of entries in the virtual function table that point to functions
     of classes derived from the current class. */
  for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
    vtbl_var = bcp->virtual_function_table_var;
    /* If class_type has no virtual functions but the base class does,
       it's possible that the virtual function table pointer in the base class
       is currently set for a class derived from class_type.  Consider:
         struct A {
           virtual void f() {}
           A() {}
           ~A() {}
         };
         struct B : public A {
            B() {}
           ~B() {f();}  // Should call A::f according to ARM 12.7
         };
         struct C : public B {
           void f() {}
         } c;
       Without this special case, when destroying a C object C::f would
       be called.  Don't do this in cfront mode.
    */
    if (vtbl_var == NULL && !any_cfront_mode() &&
        !bcp->shares_virtual_function_info) {
      a_class_type_supplement_ptr base_class_ctsp =
                              bcp->type->variant.class_struct_union.extra_info;
      /* Use the virtual function table for the base class as a complete
         object, if there is one. */
      vtbl_var = base_class_ctsp->virtual_function_table_var;
    }  /* if */
    if (vtbl_var != NULL) {
      /* The base class virtual function table pointer must be set
         to reflect the fact that it exists as a subobject inside the
         current class. */
      vtbl_addr_node = make_vtbl_address_node(vtbl_var);
      set_variable_address_taken(vtbl_var);
      vtbl_var->source_corresp.referenced = TRUE;
      /* Build a node to address the virtual table pointer in the base
         class.   The base class may be virtual or may be inside a virtual
         base class.  We cannot optimize virtual base class cases because
         we do not know whether or not we have a complete object (at least,
         we don't know at compile time). */
      vptr_node = make_base_class_lvalue_from_var(this_param_var, bcp,
                                                  /*complete_object=*/FALSE);
      vptr_node = make_vptr_field_lvalue(vptr_node);
      /* Make and insert the assignment statement. */
      (void)insert_assignment_statement(vptr_node,
                                        (an_expr_operator_kind)eok_passign,
                                        vtbl_addr_node,
                                        &insert_location);
    }  /* if */
  }  /* for */
  /* Now generate epilogue wrapper code to destroy members and base classes.
     This is done early, and into a block off to the side, so that the
     proper exception cleanup actions can be put on the cleanup list before
     the user code is lowered.  Later, the epilogue block will be inserted
     into the destructor at the right place. */
  /* The constructor_inits list contains a list of destructions.  Each
     destruction is a default call supplied by the front end.  Every
     base class and member that requires a destructor appears, in the
     order (1) data members, (2) normal base classes, (3) virtual base
     classes.  The order within each section is source declaration order. */
  ctor_init = scope->variant.routine.constructor_inits;
  scope->variant.routine.constructor_inits = NULL;
  if (ctor_init == NULL) {
    /* No constructor_init entries, so not epilogue block is needed. */
    epilogue_block = NULL;
  } else {
    /* Save the prologue insert location as the place to insert exception
       handling initialization code. */
    dtor_wrapper_prologue_insert_location = insert_location;
    epilogue_block = alloc_statement((a_statement_kind)stmk_block);
    set_block_start_insert_location(epilogue_block, &insert_location);
    /* Generate a destructor call for each data member that appears on the
       ctor_init list. */
    for (; ctor_init != NULL &&
                         ctor_init->kind == (a_constructor_init_kind)cik_field;
         ctor_init = ctor_init->next) {
      lower_dtor_init(ctor_init, this_param_var, /*have_complete_object=*/TRUE,
                      &insert_location);
    }  /* for */
    /* Generate a destructor call for each non-virtual direct base class
       that appears on the ctor_init list. */
    for (; ctor_init != NULL &&
             ctor_init->kind == (a_constructor_init_kind)cik_direct_base_class;
         ctor_init = ctor_init->next) {
      lower_dtor_init(ctor_init, this_param_var,
                      /*have_complete_object=*/FALSE,
                      &insert_location);
    }  /* for */
    /* If any items remain on the ctor_init list, they must be for virtual
       base classes. */
    if (ctor_init != NULL) {
#if CHECKING
      if (ctor_init->kind != (a_constructor_init_kind)cik_virtual_base_class) {
        internal_error("lower_destructor_code: bad ctor_init item kind");
      }  /* if */
#endif /* CHECKING */
      /* Put out code that tests whether or not the virtual base classes need
         to be destroyed.  This is done by testing whether or not the
         added parameter indicates we have a whole object. */
      /* Note that we can do a "!= 0" test instead of a bit test because the
         0x1 bit (for "free storage") would only be on for a whole object. */
      /* Make an expression node pointing to the zero constant. */
      zero_constant_node = node_for_integer_constant(0L,
                                                     (an_integer_kind)ik_int);
      /* Make an expression node for the parameter. */
      complete_obj_param_node = var_rvalue_expr(complete_obj_param_var);
      /* Make a node comparing the parameter against zero. */
      complete_obj_param_node->next = zero_constant_node;
      compare_node = make_operator_node((an_expr_operator_kind)eok_ine,
                                        int_type, complete_obj_param_node);
      /* Make an "if" statement with a block statement under it:
           if (param != 0) {}
                            ^--- additional statements will be inserted.
      */
      insert_if_statement(compare_node, &insert_location, &insert_location2);
      /* Destroy any virtual base classes on the ctor_init list. */
      for (; ctor_init != NULL; ctor_init = ctor_init->next) {
        lower_dtor_init(ctor_init, this_param_var,
                        /*have_complete_object=*/FALSE, &insert_location2);
      }  /* for */
      /* Note that the "if" created above effectively ends here. */
    }  /* if */
    if (exceptions_enabled) {
      /* Set the exception cleanup region before the last destructor call
         in the epilogue, if any. */
      set_region_on_prev_destructor_wrapper_cleanup((a_cleanup_action_ptr)NULL,
                                                    &insert_location);
      /* Set the region number at the end of the prologue (i.e., just before
         going into user code) to the first cleanup region for the wrapper
         cleanup. */
      assign_region_number_to_eh_curr_region(
                                       (a_cleanup_region_number)0,
                                       &dtor_wrapper_prologue_insert_location);
    } /* if */
  }  /* if */
  /* Now lower the user code.  Note that the cleanup actions for the
     exception cleanup for the wrapper code are already on the cleanup
     list. */
  lower_statement_list(user_code_stmts, &top_level_stmt);
  /* Now figure out where to attach the epilogue code. */
  /* All returns in the destructor have been put on the return_memo_list.
     See if the first of them (i.e., the last encountered in the routine)
     is a top-level return. */
  for (prev_stmt = NULL,
           top_level_stmt = scope->assoc_block->variant.block.statements;
       top_level_stmt != NULL;
       prev_stmt = top_level_stmt,
           top_level_stmt = top_level_stmt->next) {
    if (return_memo_list != NULL && top_level_stmt == return_memo_list->stmt) {
      /* This is a top-level return statement. */
      break;
    }  /* if */
  }  /* for */
  /* We will be inserting code before the return or at the end of the top-level
     statement list.   If there are no statements preceding the return, insert
     at the start of the block (which is the same thing). */
  if (prev_stmt == NULL) {
    set_block_start_insert_location(scope->assoc_block, &insert_location);
  } else {
    set_insert_location(prev_stmt, &insert_location);
  }  /* if */
  if (top_level_stmt == NULL) {
    /* There was no top-level return, so add one at the end of the top-level
       statement list. */
    an_insert_location saved_insert_location;
    top_level_stmt = alloc_statement((a_statement_kind)stmk_return);
    saved_insert_location = insert_location;
    insert_statement(top_level_stmt, &insert_location);
    insert_location = saved_insert_location;
    /* Add the return to the return memo list. */
    add_to_return_memo_list(top_level_stmt);
  }  /* if */
  /* Now there is a top-level return statement and insert_location is set to
     insert in front of it.  The return statement is pointed to by
     top_level_stmt and by the first entry of the return memo list,
     and prev_stmt points to the statement preceding the return. */
  /* Leave just the entry for this return on the memo list.  The rest are
     processed and freed. */
  rmp = return_memo_list->next;
  return_memo_list->next = NULL;
  if (rmp == NULL) {
    /* There are no other returns. */
  } else {
    /* There are other returns.  Add an epilogue label and change the other
       returns to gotos to that label. */
    epilogue_label = alloc_label();
    label_stmt = alloc_statement((a_statement_kind)stmk_label);
    label_stmt->variant.label = epilogue_label;
    epilogue_label->variant.exec_stmt = label_stmt;
    epilogue_label->parent_block = scope->assoc_block;
    epilogue_label->source_corresp.referenced = TRUE;
    add_to_labels_list(epilogue_label);
    insert_statement(label_stmt, &insert_location);
    /* Change the other returns to gotos. */
    for (; rmp != NULL; rmp = rmp_next) {
      a_statement_ptr stmt = rmp->stmt;
      rmp_next = rmp->next;
      set_statement_kind(stmt, (a_statement_kind)stmk_goto);
      stmt->variant.label = epilogue_label;
      rmp->next = NULL;
      free_return_memo_list(rmp);
    }  /* for */
  }  /* if */
  /* Add the epilogue block created earlier, if there is one. */
  if (epilogue_block != NULL) {
    insert_statement(epilogue_block, &insert_location);
  }  /* if */
  /* Add code to free the storage if the "free" bit (0x1) is on in the
     added parameter:
       if ((param & 0x1) != 0) delete-routine((void *)this);
  */
  { an_expr_node_ptr this_param_node;
    an_expr_node_ptr and_node, two_constant_node, if_node;
    a_statement_ptr  call_stmt;
    a_routine_ptr    delete_routine;
    a_param_type_ptr param1;

    /* Make "param & 0x1". */
    complete_obj_param_node = var_rvalue_expr(complete_obj_param_var);
    two_constant_node = node_for_integer_constant(1L, (an_integer_kind)ik_int);
    complete_obj_param_node->next = two_constant_node;
    and_node = make_operator_node((an_expr_operator_kind)eok_and,
                                  int_type, complete_obj_param_node);
    /* Make "(param & 0x1) != 0". */
    zero_constant_node = node_for_integer_constant(0L,
                                                   (an_integer_kind)ik_int);
    and_node->next = zero_constant_node;
    if_node = make_operator_node((an_expr_operator_kind)eok_ine,
                                 int_type, and_node);
#if ASSIGNMENT_TO_THIS_ALLOWED
    /* If an assignment to "this" was done in the body of the destructor,
       also test "this != NULL". */
    if (dtor_routine->assignment_to_this_done) {
      an_expr_node_ptr null_constant_node, this_compare_node;
      a_constant       null_constant;
      /* Make "this != NULL". */
      this_param_node = var_rvalue_expr(this_param_var);
      make_zero_of_proper_type(this_param_var->type, &null_constant);
      null_constant_node = alloc_node_for_constant(&null_constant);
      this_param_node->next = null_constant_node;
      this_compare_node = make_operator_node((an_expr_operator_kind)eok_pne,
                                             int_type, this_param_node);
      /* Make "this != NULL && (param & 0x1) != 0". */
      this_compare_node->next = if_node;
      if_node = make_operator_node((an_expr_operator_kind)eok_land,
                                   int_type, this_compare_node);
    }  /* if */
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
    /* Make "if ((param & 0x1) != 0)". */
    insert_if_statement(if_node, &insert_location, &insert_location2);
    /* Make "delete-routine((void *)this);" under the "if". */
    this_param_node = var_rvalue_expr(this_param_var);
    this_param_node = add_cast_if_necessary(this_param_node, void_star_type());
    /* If the delete routine takes two arguments, add a second argument
       of type size_t that gives the size of the class. */
    delete_routine = ctsp->assoc_operator_delete_routine;
    check_assertion(delete_routine != NULL);
    param1 = unlowered_param_type_list(delete_routine->type);
#if CHECKING
    if (param1 == NULL) {
      internal_error("lower_destructor_code: bad delete rout 1st param");
    }  /* if */
#endif /* CHECKING */
    if (param1->next != NULL) {
      /* Two-argument form.  Add a second argument of type size_t that
         indicates the (static) size of the object. */
      this_param_node->next =
              node_for_integer_constant((long)(class_type->size),
                                        targ_size_t_int_kind);
    }  /* if */
    delete_routine->source_corresp.referenced = TRUE;
    call_stmt = make_call_statement(delete_routine, this_param_node);
    insert_statement(call_stmt, &insert_location2);
  }
  { a_statement_ptr  block_stmt;
    an_expr_node_ptr this_param_node, null_constant_node, if_node;
    a_constant       null_constant;

    /* Make and add "if (this != NULL)" around the entire routine body.
       This is needed when the delete call can be folded into the
       destructor call, and is handy to avoid a test before the call
       even when that is not allowed. */
    this_param_node = var_rvalue_expr(this_param_var);
    make_zero_of_proper_type(this_param_var->type, &null_constant);
    null_constant_node = alloc_node_for_constant(&null_constant);
    this_param_node->next = null_constant_node;
    if_node = make_operator_node((an_expr_operator_kind)eok_pne,
                                 int_type, this_param_node);
    /* Make the "if" statement. */
    enclose_routine_in_if(scope, if_node, &block_stmt, (a_variable_ptr)NULL);
  }
  error_position = saved_error_position;
  code_pos_for_lowering = saved_code_pos;
}  /* lower_destructor_code */


void lower_file_scope_dynamic_inits(void)
/*
Do lowering on the file-scope dynamic initializations list.
*/
{
  a_dynamic_init_ptr dip;
  an_insert_location insert_location;
  a_boolean          keep_dynamic_init;
  a_scope_ptr        file_scope = il_header.primary_scope, scope;
  an_init_pos_descr  ipd;
  a_context          context;
  a_memory_region_number
                     region_number;

  dip = file_scope->dynamic_inits;
  if (dip != NULL) {
    /* There are some file-scope dynamic initializations.  Generate a routine
       containing them. */
    scope = file_scope_init_insert_location(&insert_location, &region_number);
    switch_il_region(region_number);
    push_context(&context, scope, /*subscope_region=*/FALSE);
    if (exceptions_enabled) {
      /* Initialize for exception handling lowering. */
      eh_function_lower_init();
    }  /* if */
    processing_file_scope_init_routine = TRUE;
    /* Put a null statement at the beginning of the block.  This changes the
       insert_location from block-start to after-statement, which is necessary
       in case some lower-level code tries to insert initialization code
       (e.g., to set the object address table for a conditional flag) at
       the beginning of the block -- we don't want that code to come out
       after the first initialization. */
    insert_statement(alloc_statement((a_statement_kind)stmk_block),
                     &insert_location);
    /* Generate the initializations. */
    for (; dip != NULL; dip = dip->next) {
      an_insert_location_ptr eff_insert_location = &insert_location;
#if TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE
      an_insert_location     insert_location2;
      if (dip->variable->is_template_static_data_member) {
        /* This is the initialization of a static data member in a template.
           Add guard code around the initialization if necessary. */
        if (add_static_data_member_init_guard_test(dip->variable,
                                                   &insert_location,
                                                   &insert_location2)) {
          /* Guard code was emitted.  The actual initialization code is
             inserted inside the guard "if". */
          eff_insert_location = &insert_location2;
        }  /* if */
      }  /* if */
#endif /* TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE */
      set_var_init_pos_descr(dip->variable, &ipd);
      lower_dynamic_init(dip, &ipd, /*is_expr_temporary=*/FALSE,
                         (an_expr_node_ptr)NULL, (an_expr_node_ptr)NULL,
                         (a_constructor_init_ptr)NULL,
                         eff_insert_location, &keep_dynamic_init);
#if CHECKING
      if (keep_dynamic_init) {
        internal_error(
               "lower_file_scope_dynamic_inits: keep_dynamic_init unexpected");
      }  /* if */
#endif /* CHECKING */
    }  /* for */
    if (exceptions_enabled) {
      /* Add prologue/epilogue code for exceptions if needed. */
      add_eh_function_prologue(scope);
    }  /* if */
    /* Free any return memos that were not used. */
    free_return_memo_list(return_memo_list);
    processing_file_scope_init_routine = FALSE;
    pop_context();
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
    /* Make orphan lists for any local types or static variables in the
       routine or any of its blocks. */
    add_scope_orphaned_il_lists(scope);
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
    done_with_memory_region(region_number);
    switch_il_region(FILE_SCOPE_REGION_NUMBER);
    file_scope->dynamic_inits = NULL;
  }  /* if */
}  /* lower_file_scope_dynamic_inits */


void init_lower_one_time_init(void)
/*
Do one-time initialization of static variables declared in lower_init.c.
(Variables that need to be reinitialized with each new translation unit
are handled in il_lower_init.)
*/
{
  /* Save variables from lower_init.c that are needed for precompiled
     headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(dtor_wrapper_prologue_insert_location),
      pch_saved_var_array_elem(file_scope_init_routine),
      pch_saved_var_array_elem(vec_cctor_routine),
      pch_saved_var_array_elem(record_needed_destruction_routine),
      pch_saved_var_array_elem(vec_delete_routine),
      pch_saved_var_array_elem(vec_new_eh_routine),
      pch_saved_var_array_elem(vec_new_routine),
      pch_saved_var_array_elem(needed_destruction_type),
      pch_saved_var_array_elem(needed_destruction_object_field),
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
}  /* one_time_init_lower_init */


void init_lower_init(void)
/*
Initialize static variables related to this file.  This is done as a
subroutine (rather than relying on static initialization) so that it
can be redone to compile more than one source file in a single invocation
of the front end.
*/
{
  /* Variable in lower_init.h: */
  processing_file_scope_init_routine = FALSE;
  /* Static variables in lower_init.c: */
  module_id = NULL;
  vec_new_routine = vec_new_eh_routine = vec_cctor_routine =
                                                     vec_delete_routine = NULL;
  record_needed_destruction_routine = NULL;
  needed_destruction_type = NULL;
  file_scope_init_routine = NULL;
}  /* init_lower_init */

#endif /* DO_IL_LOWERING */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1993 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
