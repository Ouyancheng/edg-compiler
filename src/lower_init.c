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

#include "basics.h"
#include "host_envir.h"

/* Only include this code if it is needed: */
#if DO_IL_LOWERING

#include "lower_il.h"
#include "lower_init.h"
#include "il.h"
#include "types.h"
#include "mem_manage.h"
#include "debug.h"
#include "folding.h"
#include "cmd_line.h"
#include "exprutil.h"
#include "const_ints.h"

static a_routine_ptr
		file_scope_init_routine,
		file_scope_term_routine;
			/* Pointers to the file-scope initialization and
			   termination routines once created.  NULL until
			   then. */
static a_memory_region_number
		file_scope_init_routine_il_region,
		file_scope_term_routine_il_region;
			/* IL memory region numbers for the above routines,
			   once they are created. */
static an_insert_location
		file_scope_init_routine_insert_location,
		file_scope_term_routine_insert_location;
			/* Insert locations in file_scope_init_routine and
			   file_scope_term_routine.  Only defined if the
			   corresponding routine pointers are non-NULL. */
static a_required_destructor_call_ptr
		destructor_calls_for_local_static_variables,
		end_destructor_calls_for_local_static_variables;
			/* List of required destructor calls for local static
			   variables.  These are saved and output at the
			   file scope. */


#if TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE
/* Needed because of forward reference: */
static void add_static_data_member_init_guard_test(
                                           a_variable_ptr     variable,
                                           an_insert_location *insert_location,
                                           a_variable_ptr     *guard_var);
#endif /* TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE */


/*
If variable != NULL, transfer the position from it into stmt.
*/
#define transfer_pos_from_var_to_statement(variable, stmt)            \
{ if ((variable) != NULL && (stmt) != NULL) {                         \
    set_stmt_source_position((stmt)->position,                        \
                             (variable)->source_corresp.decl_position); \
  }  /* if */                                                         \
}  /* transfer_pos_from_var_to_statement */


static a_type_ptr void_star_type(void)
/*
Make and return a "void *" type.
*/
{
  return make_pointer_type(void_type());
}  /* void_star_type */


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

  if (insert_location->expr_insert) {
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


static a_statement_ptr insert_assignment_statement(
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
  assign_node = make_operator_node(op, type_pointed_to(lvalue_expr->type),
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
  set_variable_address_constant(var, &addr_constant);
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
  rout_node = function_addr_expr(routine);
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


static an_expr_node_ptr make_runtime_rout_call(char             *name,
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

  if (insert_location->expr_insert) {
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
saved so that a destructor may be called later.
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


static void set_var_init_pos_descr(a_variable_ptr        var,
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


static an_expr_node_ptr modify_init_entity_node(
                                          an_expr_node_ptr         entity_node,
                                          an_init_pos_modifier_ptr modifiers)
/*
Add the address modifiers from the list given by "modifiers" to the
entity address "entity_node" and return a pointer to the modified expression
tree.
*/
{
  an_expr_node_ptr elem_num_node;

  /* If there are no modifiers, return the original node. */
  if (modifiers != NULL) {
    /* Process the modifiers preceding the final modifier, then add the final
       qualifier (recall that the modifiers are in order from the innermost
       to the outermost). */
    entity_node = modify_init_entity_node(entity_node, modifiers->next);
    /* Add the final modifier. */
    if (modifiers->curr_field != NULL) {
      /* Add a field selection.  ("au_" for possibly from anonymous union.) */
      entity_node = au_field_lvalue_selection_expr(entity_node,
                                                   modifiers->curr_field);
    } else if (modifiers->curr_base != NULL) {
      /* Add a base class selection. */
      entity_node = make_base_class_lvalue(entity_node, modifiers->curr_base,
                                           /*complete_object=*/FALSE);
    } else {
      /* Add an array element selection. */
      /* Do the pointer decay from array to pointer to element. */
      a_type_ptr ptr_elem_type = make_pointer_type(
                                   array_element_type(
                                     type_pointed_to(entity_node->type)));
      entity_node = add_cast(entity_node, ptr_elem_type);
      if (modifiers->curr_elem != 0) {
        /* Add the subscript if it's non-zero. */
        elem_num_node = node_for_integer_constant(
                                        (long)modifiers->curr_elem,
                                        (an_integer_kind)TARG_SIZE_T_INT_KIND);
        entity_node->next = elem_num_node;
        entity_node = make_operator_node((an_expr_operator_kind)eok_padd_subsc,
                                         make_pointer_type(modifiers->type),
                                         entity_node);
      }  /* if */
    }  /* if */
  }  /* if */
  return entity_node;
}  /* modify_init_entity_node */


an_expr_node_ptr make_init_entity_node(an_init_pos_descr_ptr ipdp)
/*
Make an expression for the entity described by ipdp, as an lvalue, and
return a pointer to it.
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
  }  /* if */
  /* Add the modifiers to the base address. */
  entity_node = modify_init_entity_node(entity_node, ipdp->modifiers);
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
  /* If the initialization is for a whole variable, the position is available
     from the variable. */
  transfer_pos_from_var_to_statement(dip->variable, assign_stmt);
}  /* add_init_assignment */


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
constructor call.  implied_arg_list is a list of implied extra virtual
base class pointer arguments for the constructor, or NULL if this routine
should generate them if required.  Insert the statement at *insert_location
and update *insert_location.  The additional-arguments list given by
dip->variant.constructor.args has already been lowered.
*/
{
  a_routine_ptr    constr_routine = dip->variant.constructor.ptr;
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
    make_ctor_implied_arg_list(constr_routine, &implied_arg_list,
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
  call_stmt = make_call_statement(constr_routine, entity_node);
  /* If the initialization is for a whole variable, the position is available
     from the variable. */
  transfer_pos_from_var_to_statement(dip->variable, call_stmt);
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
		vec_cctor_routine,
		vec_delete_routine;


static an_expr_node_ptr num_elem_node_from_count(long array_element_count)
/*
Build an expression for a constant that represents the number of elements
in an array for an array new/delete call.  -1 indicates a variable-length
array.
*/
{
  a_boolean        did_not_fold;
  an_expr_node_ptr num_elem_node;
  a_constant       num_elem_constant;

  /* Create the constant as long and then change it to int to get any
     truncation error. */
  set_integer_constant(&num_elem_constant, array_element_count,
                       (an_integer_kind)ik_long);
  type_change_constant(&num_elem_constant,
                       integer_type((an_integer_kind)ik_int),
                       /*is_implicit_cast=*/TRUE,
                       /*constant_context=*/TRUE, &did_not_fold,
                       &error_position);
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
                                        (an_integer_kind)TARG_SIZE_T_INT_KIND);
  return size_elem_node;
}  /* size_elem_node_from_pointer_type */


static an_expr_node_ptr make_vec_new_call(an_expr_node_ptr entity_node,
                                          an_expr_node_ptr num_elem_node,
                                          a_routine_ptr    ctor_routine)
/*
Make a call to a runtime routine (__vec_new) that will allocate an array
and call a constructor for each element of the array.  entity_node gives
the address of the array (for cases where the array is already
allocated).  num_elem_node gives (as an expression) the number of
elements in the array.  ctor_routine is the constructor routine to be
called, or NULL if no constructor is to be called.  A pointer to the
expression created is returned.
*/
{
  an_expr_node_ptr call_node, arg_expr_list, size_elem_node;
  an_expr_node_ptr func_addr_node;
  a_constant       null_constant;

  /* Build a constant node for the size of the array elements. */
  size_elem_node = size_elem_node_from_pointer_type(entity_node->type);
  if (ctor_routine != NULL) {
    func_addr_node = function_addr_expr(ctor_routine);
  } else {
    /* No constructor routine to call; use 0 cast to the right function
       pointer type. */
    make_zero_of_proper_type(make_vptp_type(), &null_constant);
    func_addr_node = alloc_node_for_constant(&null_constant);
  }  /* if */
  /* The call looks like
       __vec_new(entity_node, num_elems, size_elem, ctor_routine)
  */
  arg_expr_list = entity_node;
  entity_node->next = num_elem_node;
  num_elem_node->next = size_elem_node;
  size_elem_node->next = func_addr_node;
  call_node = make_runtime_rout_call("__vec_new", &vec_new_routine,
                                     void_star_type(), arg_expr_list);
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

  /* Build a constant node for the number of array elements. */
  num_elem_node = num_elem_node_from_count(array_element_count);
  /* Build a constant node for the size of the array elements. */
  size_elem_node = size_elem_node_from_pointer_type(entity_node->type);
  /* Build the "free_storage" argument: 1 to free storage, 0 otherwise. */
  free_storage_node = node_for_integer_constant(free_storage ? 1L : 0L,
                                                (an_integer_kind)ik_int);
  if (dtor_routine != NULL) {
    func_addr_node = function_addr_expr(dtor_routine);
  } else {
    /* No destructor routine to call; use 0 cast to the right function
       pointer type. */
    make_zero_of_proper_type(make_vptp_type(), &null_constant);
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
  func_addr_node = function_addr_expr(cctor_routine);
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
by default_arg_list (the expressions are already lowered).  Implicitly-
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
  an_expr_node_ptr call_node, temp_arg;
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
  a_boolean        already_lowered;

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
    /* Make a parameter variable for the "this" parameter (again, in lowered
       form as a normal parameter). */
    new_routine_scope->variant.routine.parameters = this_param_var =
                                  make_lowered_param_variable(this_param_type);
    this_param_var->assoc_param_type = new_rtsp->param_type_list;
    /* Make any additional parameter types and parameter vars beyond the
       "this" parameter (this comes up, for instance, on the copy
       constructor case). */
    src_param_type = rtsp->param_type_list;
    already_lowered = src_param_type != NULL && visited_yet(src_param_type);
    if (already_lowered) {
      /* The routine type has already been lowered (i.e., the "this"
         parameter type is already on the explicit parameter type list).
         Skip the first parameter type. */
      src_param_type = src_param_type->next;
      /* Also skip a parameter for each implicit parameter added (pointers
         to virtual base classes). */
      for (temp_arg = implied_arg_list;
           temp_arg != NULL;
           temp_arg = temp_arg->next) {
        src_param_type = src_param_type->next;
      }  /* if */
    }  /* if */
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
      if (!already_lowered && src_param_type->passed_via_copy_constructor) {
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
    set_block_start_insert_location(new_routine_scope->assoc_block,
                                    &insert_location);
    /* Make a call statement that calls the original routine with all
       the implicit arguments, i.e., that passes all the extra arguments
       to the original routine. */
    /* Copy the default argument expressions into the function memory
       region. */
    default_arg_list = copy_list_of_expr_trees(default_arg_list);
    if (implied_arg_list != NULL) {
      /* Add the implicit arguments to the front of the default argument
         list. */
      end_implied_arg_list->next = default_arg_list;
      default_arg_list = implied_arg_list;
    }  /* if */
    /* Add the "this" parameter at the front of the argument list. */
    this_arg = var_rvalue_expr(this_param_var);
    this_arg->next = default_arg_list;
    call_node = make_call_node(routine, this_arg, /*honor_virtual=*/FALSE);
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
dip->variant.constructor.args has already been lowered.
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
    call_node = make_vec_new_call(entity_node, num_elem_node, ctor_routine);
  }  /* if */
  /* Make a statement containing the call. */
  call_stmt = alloc_expr_statement(call_node);
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
  a_routine_ptr    destr_routine = dip->destructor;
  a_statement_ptr  call_stmt;
  an_expr_node_ptr implied_arg_node;

#if CHECKING
  if (destr_routine == NULL) {
    internal_error("add_destructor_call: destructor == NULL");
  }  /* if */
#endif /* CHECKING */
  /* If the destructor is for a class that has virtual base classes, add
     the implicit complete-object argument. */
  make_dtor_implied_arg_list(destr_routine, have_complete_object,
                             &implied_arg_node);
  entity_node->next = implied_arg_node;
  /* Make an expression statement containing the call expression. */
  call_stmt = make_call_statement(destr_routine, entity_node);
  /* If the destruction is for a whole variable, the position is available
     from the variable. */
  transfer_pos_from_var_to_statement(dip->variable, call_stmt);
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
  /* Insert the statement at the right location. */
  insert_statement(call_stmt, insert_location);
}  /* add_array_destructor_call */


static void lower_ck_dynamic_init(a_constant_ptr         con_ptr,
                                  an_init_pos_descr_ptr  ipdp,
                                  a_variable_ptr         first_time_test_var,
                                  a_boolean              dtor_case,
                                  a_constructor_init_ptr ctor_init,
                                  an_insert_location_ptr insert_location)
/*
Generate executable code to handle a ck_dynamic_init constant (pointed
to by con_ptr).  The entity to be initialized is described by ipdp.
The necessary statements are inserted at *insert_location and
*insert_location is updated.  If ipdp->whole_array is TRUE, this
call is handling all the elements of an array.  If first_time_test_var
is non-NULL, it points to a variable entry for the first-time-test variable
that controls access to this initialization.  If dtor_case is TRUE, do
the destruction indicated in the dynamic init immediately and ignore the
initialization.  If the dynamic initialization is part of a constructor
initializer, ctor_init points to the constructor-init entry.
*/
{
  a_constant_ptr next_con;
  a_boolean      keep_dynamic_init;
  a_type_ptr     desired_type;

  if (dtor_case) {
    /* In a destructor case, so the "initialization" is really
       destruction. */
    lower_destructor_dynamic_init(con_ptr->variant.dynamic_init, ipdp,
                                  insert_location);
  } else {
    /* Normal initialization. */
    lower_dynamic_init(con_ptr->variant.dynamic_init, ipdp,
                       first_time_test_var, /*is_expr_temporary=*/FALSE,
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
       away.   Therefore we just leave the ck_dynamic_init constant
       as it is. */
#if CHECKING
    con_ptr->kind = (a_constant_repr_kind)ck_error;
#endif /* CHECKING */
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
                                   a_variable_ptr         first_time_test_var,
                                   a_boolean              dtor_case,
                                   a_constructor_init_ptr ctor_init,
                                   an_insert_location_ptr insert_location,
                                   a_boolean              *keep_constant)
/*
aggr_const points to a ck_aggregate constant that contains one or more
ck_dynamic_init dynamic initializations.  The ck_aggregate constant is
the initial value for the entity described by ipdp.  If first_time_test_var
is non-NULL, it points to a variable entry for the first-time-test variable
that controls access to this initialization.  If dtor_case is TRUE, this
"initialization" is a destruction to be done immediately.  If the dynamic
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

  /* Mark the constant as visited.  This is necessary if the aggregate
     constant ends up being kept because something constant remains after
     the non-constant parts have been rewritten. */
  mark_as_visited(aggr_const);
  /* Determine the type of the aggregate being initialized. */
  if (ipdp->modifiers != NULL) {
    aggr_type = ipdp->modifiers->type;
  } else {
    aggr_type = ipdp->base_type;
  }  /* if */
  aggr_type = skip_typerefs(aggr_type);
  /* Start a new level in the init_pos_modifier chain. */
  ipd = *ipdp;
  ipmp = &ipm;
  add_init_pos_modifier(ipmp, &ipd);
  /* Determine the type of the first element of the aggregate being
     initialized. */
  if (aggr_type->kind == (a_type_kind)tk_array) {
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
    ipmp->curr_field = aggr_type->variant.class_struct_union.field_list;
#if CHECKING
    if (ipmp->curr_field == NULL) {
      internal_error("lower_dynamic_init_aggregate_constant: no fields");
    }  /* if */
#endif /* CHECKING */
    ipmp->type = ipmp->curr_field->type;
  }  /* if */
  /* Look for dynamic init constants on the list of constants. */
  con_ptr = aggr_const->variant.aggregate.first_constant;
  for (;;) {
    if (con_ptr->kind == (a_constant_repr_kind)ck_dynamic_init) {
      /* Dynamic initialization. */
      lower_ck_dynamic_init(con_ptr, &ipd, first_time_test_var,
                            dtor_case, ctor_init, insert_location);
    } else if (con_ptr->kind == (a_constant_repr_kind)ck_init_repeat) {
      /* Repeated constant.  Must be initializing members of an array. */
#if CHECKING
      if (aggr_type->kind != (a_type_kind)tk_array) {
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
      lower_ck_dynamic_init(repeated_con, &ipd, first_time_test_var,
                            dtor_case, ctor_init, insert_location);
    } else if (con_ptr->kind == (a_constant_repr_kind)ck_aggregate) {
      /* Aggregate constant initializing a member of an aggregate. */
      lower_dynamic_init_aggregate_constant(con_ptr, &ipd, first_time_test_var,
                                            dtor_case, ctor_init,
                                            insert_location, keep_constant);
    } else {
      /* Normal constant. */
      lower_constant(con_ptr);
      *keep_constant = TRUE;
    }  /* if */
    /* Go on to the next constant if there is one. */
    con_ptr = con_ptr->next;
    if (con_ptr == NULL) break;
    /* Find the next element type in the aggregate. */
    if (aggr_type->kind == (a_type_kind)tk_array) {
      /* Array -- go on to next element; element type does not change. */
      ipmp->curr_elem++;
    } else {
#if CHECKING
      if (!is_immediate_class_type(aggr_type)) {
        internal_error(
                   "lower_dynamic_init_aggregate_constant: bad aggr kind (2)");
      }  /* if */
#endif /* CHECKING */
      /* Class, struct, or union -- go on to next field (nonstatic data
         member). */
      ipmp->curr_field = ipmp->curr_field->next;
#if CHECKING
      if (ipmp->curr_field == NULL) {
        internal_error(
                   "lower_dynamic_init_aggregate_constant: not enough fields");
      }  /* if */
#endif /* CHECKING */
      ipmp->type = ipmp->curr_field->type;
    }  /* if */
  }  /* for */
}  /* lower_dynamic_init_aggregate_constant */


void make_code_to_invoke_file_scope_init_and_term_routines(void)
/*
Make the code that will ensure that the file-scope initialization and
termination routines (if any) are invoked at program startup and
termination.
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

  /* Only generate the code if there is a startup or termination routine. */
  if (file_scope_init_routine != NULL || file_scope_term_routine != NULL) {
    /* Create a __link variable pointing to a struct that points to the
       initialization/termination routine, using the same form as cfront:
         void __sti__module_id() {...}
         void __std__module_id() {...}
         struct __linkl {
           struct __linkl *next;
           void           (*ctor)();
           void           (*dtor)();
         };
         static struct __linkl __link = {NULL, __sti__module_id,
                                               __std__module_id};
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
         {NULL, __sti__module_id, __std__module_id}
       If either routine does not exist, use a NULL instead. */
    aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
    aggr_con->type = struct_type;
    link_var->init_kind = (an_init_kind)initk_static;
    link_var->initializer.constant = aggr_con;
    /* Zero for "next" field. */
    init_con1 = alloc_constant((a_constant_repr_kind)ck_address);
    make_zero_of_proper_type(ptr_struct_type, init_con1);
    /* Address of __sti__module_id for "ctor" field. */
    init_con2 = alloc_constant((a_constant_repr_kind)ck_address);
    if (file_scope_init_routine != NULL) {
      set_routine_address_constant(file_scope_init_routine, init_con2);
      init_con2->type = ptr_func_type;
    } else {
      /* No init routine.  Use NULL. */
      make_zero_of_proper_type(ptr_func_type, init_con2);
    }  /* if */
    /* Address of __std__module_id for "dtor" field. */
    init_con3 = alloc_constant((a_constant_repr_kind)ck_address);
    if (file_scope_term_routine != NULL) {
      set_routine_address_constant(file_scope_term_routine, init_con3);
      init_con3->type = ptr_func_type;
    } else {
      /* No init routine.  Use NULL. */
      make_zero_of_proper_type(ptr_func_type, init_con3);
    }  /* if */
    /* Link the constants together under the ck_aggregate constant. */
    aggr_con->variant.aggregate.first_constant = init_con1;
    init_con1->next = init_con2;
    init_con2->next = init_con3;
    aggr_con->variant.aggregate.last_constant  = init_con3;
    switch_back_to_original_region(region_to_switch_back_to);
  }  /* if */
}  /* make_code_to_invoke_file_scope_init_and_term_routines */


/*
String made from the primary source file name and the current date/time,
used to generate unique names for the initialization and termination routines.
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
Set module_id to the string.  Do not make the string again it it has already
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
the prefix for the name of the routine.  Set *insert_location for insertion at
the start of the block statement that is the body of the routine, set
*init_rout_scope to point to the scope entry for the routine, set
*il_region to the IL memory region number for the routine, and return a
pointer to the routine.
*/
{
  a_routine_ptr init_rout;
  char          *name;
  sizeof_t      prefix_len = strlen(prefix), alloc_length;

  /* Combine the prefix and an identifier for the current module to make
     a name that is likely to be unique. */
  make_module_id();
  alloc_length = prefix_len + strlen(module_id) + 1;
  name = alloc_lowered_name_string(alloc_length);
  (void)memcpy(name, prefix, size_t_arg(prefix_len));
  (void)strcpy(name+prefix_len, module_id);
  /* Make a type and routine entry for the routine. */
  init_rout = make_rout_entry(name, (a_storage_class)sc_unspecified,
                              void_type(),
                              (a_type_ptr)NULL);
  /* Make a memory region, scope, and block for the init routine definition. */
  *init_rout_scope = make_routine_definition(init_rout, /*make_return=*/TRUE,
                                             il_region);
  set_block_start_insert_location((*init_rout_scope)->assoc_block,
                                  insert_location);
  return init_rout;
}  /* make_file_scope_init_or_term_routine */


static a_scope_ptr file_scope_init_insert_location(
                                        an_insert_location_ptr insert_location)
/*
Determine the insert location for a file-scope initialization statement.
*/
{
  a_scope_ptr scope;

  if (file_scope_init_routine == NULL) {
    file_scope_init_routine = make_file_scope_init_or_term_routine(
                                      IL_LOWERING_INIT_ROUTINE_PREFIX,
                                      &file_scope_init_routine_insert_location,
                                      &scope,
                                      &file_scope_init_routine_il_region);
  }  /* if */
  *insert_location = file_scope_init_routine_insert_location;
  return scope;
}  /* file_scope_init_insert_location */


static a_scope_ptr file_scope_term_insert_location(
                                        an_insert_location_ptr insert_location)
/*
Determine the insert location for a file-scope termination statement.
*/
{
  a_scope_ptr scope;

  if (file_scope_term_routine == NULL) {
    file_scope_term_routine = make_file_scope_init_or_term_routine(
                                      IL_LOWERING_TERM_ROUTINE_PREFIX,
                                      &file_scope_term_routine_insert_location,
                                      &scope,
                                      &file_scope_term_routine_il_region);
  }  /* if */
  *insert_location = file_scope_term_routine_insert_location;
  return scope;
}  /* file_scope_term_insert_location */


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


static void add_conditional_destruction_temp(
                               a_required_destructor_call_ptr rdcp,
                               an_insert_location             *insert_location)
/*
rdcp points to a required destructor call being generated.  We are currently
inside a conditional operand of a "?", "&&", or "||" operation.  Since
the construction is conditional, we add a temporary variable, initialize
it to zero at the beginning of the current block, and insert an assignment
to set the temporary to 1 before insert_location.  The destruction generated
later will be made conditional on the temporary.
*/
{
  a_variable_ptr      temp;
  a_dynamic_init_ptr  dip;
  a_constant          zero_constant;
  a_statement_ptr     stmk_init_stmt, block, label_statement;
  a_switch_clause_ptr scp;
  an_insert_location  insert_before_location;

  rdcp->first_time_test_var = temp =
                 make_lowered_temporary(integer_type((an_integer_kind)ik_int));
  /* The temporary must be initialized to zero.  If it is static, that
     is done implicitly.  Otherwise, it must be done dynamically. */
  if (temp->storage_class != (a_storage_class)sc_static) {
    if (curr_context->assoc_expr != NULL) {
      /* The current context is a region that is a single top-level
         expression.  The initialization must be inserted on top of the
         expression, but it would be dangerous to modify the expression that
         we're currently working on.  Therefore, that's left to be done
         when we get back to the top of the expression.  See
         gen_expr_conditional_destruction_var_initializations. */
      curr_context->any_conditional_destruction_var_initializations_deferred =
                                                                          TRUE;
    } else {
      /* Use a dynamic init entry to do the initialization.  Note that the
         dynamic init entry does not need to be put on a list of dynamic init
         entries.  Such a list is used only at the file scope, and any
         temporary allocated there would be static. */
      dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constant);
      dip->variable = temp;
      /* The dynamic init entry is pointed to by the variable. */
      temp->init_kind = (an_init_kind)initk_dynamic;
      temp->initializer.dynamic = dip;
      set_integer_constant(&zero_constant, 0L, (an_integer_kind)ik_int);
      dip->variant.constant = alloc_unshared_constant(&zero_constant);
      /* The dynamic init entry is pointed to by an stmk_init statement. */
      stmk_init_stmt = alloc_statement((a_statement_kind)stmk_init);
      stmk_init_stmt->variant.dynamic_init = dip;
      scp = curr_context->assoc_switch_clause;
      /* The stmk_init statement must be inserted at the right place.  For
         most cases, the right place is the beginning of the current block.
         For switch clauses, it's the beginning of the clause.  When labels
         appear, the initialization goes after the latest label. */
      label_statement = curr_context->latest_label_statement_processed;
      if (label_statement != NULL) {
        /* Insert the stmk_init after the most recent label. */
        /* The dynamic init is not at the start of the scope. */
        dip->follows_an_exec_statement = TRUE;
        /* Add the stmk_init statement after the label. */
        stmk_init_stmt->next = label_statement->next;
        label_statement->next = stmk_init_stmt;
      } else if (scp != NULL) {
        /* Switch clause. */
        /* The dynamic init is not at the start of the scope. */
        dip->follows_an_exec_statement = TRUE;
        /* Add the stmk_init statement at the beginning of the clause. */
        stmk_init_stmt->next = scp->statements;
        scp->statements = stmk_init_stmt;
      } else {
        /* Normal case. */
        block = curr_context->scope->assoc_block;
#if CHECKING
        if (block == NULL) {
          internal_error("add_conditional_destruction_temp: missing block");
        }  /* if */
#endif /* CHECKING */
        /* Add the stmk_init statement at the beginning of the block. */
        stmk_init_stmt->next = block->variant.block.statements;
        block->variant.block.statements = stmk_init_stmt;
      }  /* if */
    }  /* if */
  }  /* if */
  /* Make and insert an assignment statement to set the temporary to 1.
     The insertion is done before the indicated location, which is presumably
     the expression already generated to do the initialization. */
  /* Note that lower_temp_init recognizes this assignment so it can
     optimize around it. */
  insert_before_location = *insert_location;
#if CHECKING
  if (!insert_before_location.expr_insert ||
      insert_before_location.variant.expr.insert_before) {
    internal_error("add_conditional_destruction_temp: bad insert loc");
  }  /* if */
#endif /* CHECKING */
  insert_before_location.variant.expr.insert_before = TRUE;
  (void)insert_var_assignment_statement(temp,
                                        (an_expr_operator_kind)eok_iassign,
                                        node_for_integer_constant(1L,
                                                      (an_integer_kind)ik_int),
                                        &insert_before_location);
}  /* add_conditional_destruction_temp */


void lower_dynamic_init(a_dynamic_init_ptr     dip,
                        an_init_pos_descr_ptr  ipdp,
                        a_variable_ptr         first_time_test_var,
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

If first_time_test_var is non-NULL, it points to a variable entry for
the first-time-test variable that controls access to this initialization
of a local static variable.

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
  a_context_ptr     destructor_context;
  a_source_position saved_error_position;
  a_statement_ptr   expr_stmt;
#if TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE
  a_boolean         is_template_static_data_member_init = FALSE;
  a_variable_ptr    template_static_data_member_init_guard_var = NULL;
#endif /* TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE */

  *keep_dynamic_init = FALSE;
  saved_error_position = error_position;
  variable = dip->variable;
  if (variable != NULL) {
    /* Whole-variable initialization. */
    /* Track the source position for internal errors. */
    error_position = variable->source_corresp.decl_position;
#if CHECKING
    if (variable != ipdp->variable) {
      internal_error("lower_dynamic_init: variable mismatch");
    }  /* if */
#endif /* CHECKING */
#if TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE
    if (variable->is_template_static_data_member) {
      /* This is the initialization of a static data member in a template. */
      is_template_static_data_member_init = TRUE;
      check_assertion(processing_file_scope_init_routine);
    }  /* if */
#endif /* TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE */
  }  /* if */
  switch (dip->kind) {
    case dik_none:
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
      if (processing_file_scope_init_routine ||
          first_time_test_var != NULL) {
        /* When generating the file-scope initialization routine we have
           an expression from the file scope that must be used in the function
           scope of the initialization routine, so copy it.  Otherwise
           we have a difficult job keeping track of the nodes that are in
           the file scope and those that are in the function scope.
           Similar reasoning applies to local static variables. */
        dip->variant.expression = copy_expr_tree(dip->variant.expression);
      }  /* if */
do_assignment:;
#if CHECKING
      if (ipdp->whole_array) {
        internal_error("lower_dynamic_init: array for const or expr init");
      }  /* if */
#endif /* CHECKING */
#if TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE
      /* If this is a static data member in a template, add guard code around
         the initialization. */
      if (is_template_static_data_member_init) {
        add_static_data_member_init_guard_test(variable, insert_location,
                                  &template_static_data_member_init_guard_var);
      }  /* if */
#endif /* TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE */
      /* Make a node for the entity to be initialized. */
      entity_node = make_init_entity_node(ipdp);
      add_init_assignment(dip, entity_node, insert_location);
      break;
    case dik_call_returning_class_via_cctor:
      /* Initialize the entry by calling a routine that returns its result
         via a copy constructor. */
      /* The address of the temporary being initialized is added as an
         implicit argument of the call. */
      lower_call(dip->variant.expression, ipdp);
      if (processing_file_scope_init_routine ||
          first_time_test_var != NULL) {
        /* When generating the file-scope initialization routine we have
           an expression from the file scope that must be used in the function
           scope of the initialization routine, so copy it.  Otherwise
           we have a difficult job keeping track of the nodes that are in
           the file scope and those that are in the function scope.
           Similar reasoning applies to local static variables. */
        dip->variant.expression = copy_expr_tree(dip->variant.expression);
      }  /* if */
#if TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE
      /* If this is a static data member in a template, add guard code around
         the initialization. */
      if (is_template_static_data_member_init) {
        add_static_data_member_init_guard_test(variable, insert_location,
                                  &template_static_data_member_init_guard_var);
      }  /* if */
#endif /* TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE */
      expr_stmt = insert_expr_statement(dip->variant.expression,
                                        insert_location);
      transfer_pos_from_var_to_statement(variable, expr_stmt);
      break;
    case dik_constructor:
      /* Initialize the entity by calling a constructor. */
      /* The routine does not need to be lowered from here. */
      lower_arg_expr_list(dip->variant.constructor.args,
                          dip->variant.constructor.ptr->type);
      if (processing_file_scope_init_routine ||
          first_time_test_var != NULL) {
        /* When generating the file-scope initialization routine we have
           expressions from the file scope that must be used in the function
           scope of the initialization routine, so copy them.  Otherwise
           we have a difficult job keeping track of the nodes that are in
           the file scope and those that are in the function scope.
           Similar reasoning applies to local static variables. */
        dip->variant.constructor.args =
                        copy_list_of_expr_trees(dip->variant.constructor.args);
      }  /* if */
#if TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE
      /* If this is a static data member in a template, add guard code around
         the initialization. */
      if (is_template_static_data_member_init) {
        add_static_data_member_init_guard_test(variable, insert_location,
                                  &template_static_data_member_init_guard_var);
      }  /* if */
#endif /* TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE */
      /* Make a node for the entity to be initialized. */
      entity_node = make_init_entity_node(ipdp);
      source_node = NULL;
      if (dip->variant.constructor.is_copy_constructor_for_subobject) {
        an_init_pos_descr    cctor_source_ipd;
        an_init_pos_modifier cctor_source_ipm;
        /* The constructor being called is a copy constructor.  The argument
           for the source object is also implicit and needs to be generated.
           It's the first real parameter of the copy constructor routine
           being expanded, modified like the entity_node. */
        set_var_indirect_init_pos_descr(var_for_copy_constructor_source(),
                                        &cctor_source_ipd);
        modify_ctor_init_pos_descr(ctor_init, &cctor_source_ipd,
                                   &cctor_source_ipm);
        source_node = make_init_entity_node(&cctor_source_ipd);
      }  /* if */
      if (ipdp->whole_array) {
        /* Construct an array. */
#if CHECKING
        if (implied_arg_list != NULL) {
          internal_error("lower_dynamic_init: implied arg list for array");
        }  /* if */
#endif /* CHECKING */
        add_array_constructor_call(dip, entity_node, source_node,
                                   ipdp->array_element_count,
                                   insert_location);
      } else {
        /* Construct a simple entity (not an array). */
        add_constructor_call(dip, entity_node, source_node,
                             implied_arg_list, end_implied_arg_list,
                             insert_location);
      }  /* if */
      break;
    case dik_nonconstant_aggregate:
      /* Initialization with a nonconstant aggregate constant.  This is usually
         a whole-variable initialization, but can be used in a ctor-initializer
         to iterate over an array initialization, etc. */
#if TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE
      /* If this is a static data member in a template, add guard code around
         the initialization. */
      if (is_template_static_data_member_init) {
        add_static_data_member_init_guard_test(variable, insert_location,
                                  &template_static_data_member_init_guard_var);
      }  /* if */
#endif /* TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE */
      keep_constant = FALSE;
      lower_dynamic_init_aggregate_constant(dip->variant.constant,
                                            ipdp, first_time_test_var, 
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
#if CHECKING
    default:
      internal_error("lower_dynamic_init: bad kind");
#endif /* CHECKING */
  }  /* switch */
  /* If the dynamic init entry indicates a destructor call, put it on a
     list of destructor calls to be processed at the end of the scope.
     Note that the list gets built in the right order (i.e., the reverse of
     construction order) because each entry is added to the front of the
     list. */
  if (dip->destructor != NULL) {
    a_required_destructor_call_ptr rdcp;
    rdcp = alloc_required_destructor_call();
    /* Copy the entire dynamic init entry because in the case of a local
       static variable the dynamic init entry will be gone by the time the
       destructor call is put out (it's in the function scope memory
       region). */
    rdcp->dynamic_init = *dip;
    /* Clear the destructor field in the dynamic init entry to make it legal
       C IL. */
    dip->destructor = NULL;
    rdcp->init_pos_descr = *ipdp;
    rdcp->is_expr_temporary = is_expr_temporary;
    /* If this is an initialization within an aggregate, we must save the
       init_pos_modifier list.  However, the list runs through the stack,
       so we must make an allocated copy. */
    if (ipdp->modifiers != NULL) {
      rdcp->init_pos_descr.modifiers =
                                  copy_init_pos_modifier_list(ipdp->modifiers);
    }  /* if */
    if (first_time_test_var != NULL) {
      /* Destruction of local static variables must happen at the end of
         the file scope if the initialization has been done (i.e., if the
         first-time-test variable has been set to non-zero. */
      rdcp->first_time_test_var = first_time_test_var;
      /* Put the entry on the front of a special list. */
      rdcp->next = destructor_calls_for_local_static_variables;
      destructor_calls_for_local_static_variables = rdcp;
      if (end_destructor_calls_for_local_static_variables == NULL) {
        end_destructor_calls_for_local_static_variables = rdcp;
      }  /* if */
      /* Indicate to the back end that there will be a non-local reference to
         the variable (from the termination routine). */
      ipdp->variable->referenced_non_locally = TRUE;
    } else {
      /* Destruction of other variables must happen at the end of the current
         scope. */
      destructor_context = curr_context;
      /* For processing of file-scope dynamic inits, put the entry on the
         file-scope list. */
      if (processing_file_scope_init_routine) {
        destructor_context = file_scope_context;
      }  /* if */
      /* Put the new entry on the front of the existing list. */
      rdcp->next = destructor_context->required_destructor_calls;
      destructor_context->required_destructor_calls = rdcp;
      if (num_conditional_exprs_inside_of != 0) {
        /* Inside a conditional operand of a "?", "&&", or "||" operation.
           Since the construction is conditional, we add a temporary
           variable, initialize it to zero at the beginning of the current
           block, set the temporary to 1 here, and test the temporary
           variable later to decide whether or not to do the destruction. */
        add_conditional_destruction_temp(rdcp, insert_location);
      }  /* if */
#if TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE
      if (template_static_data_member_init_guard_var != NULL) {
        /* This is a static data member in a template and it has guard code
           around the initialization, which means it also needs guard code
           around the destruction. */
        rdcp->template_static_data_member_init_guard_var =
                                    template_static_data_member_init_guard_var;
      }  /* if */
#endif /* TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE */
    }  /* if */
  }  /* if */
  /* In the whole-variable cases, adjust the initialization specified in
     the variable (it points to the dynamic init entry). */
  if (variable != NULL) {
    if (simple_constant_init) {
      /* Initialization to a simple constant. */
      if (has_static_storage_duration(variable->storage_class)) {
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
    } else {
      /* The initialization is handled entirely by the generated code.
         It would seem that the variable should no longer be marked as
         initialized, but in fact we want to preserve the distinction between
         static variables that are initialized and those that are tentative
         definitions.  That is important when the initialization is in a
         library; the linker has to see it as a definition in order for it
         to bring in the variable (and hence the initialization code) from
         a library.  So we change the initialization to static initialization
         to zero. */
      if (has_static_storage_duration(variable->storage_class)) {
        variable->init_kind = (an_init_kind)initk_zero;
      } else {
        variable->init_kind = (an_init_kind)initk_none;
      }  /* if */
    }  /* if */
  }  /* if */
  error_position = saved_error_position;
}  /* lower_dynamic_init */


void lower_destructor_dynamic_init(a_dynamic_init_ptr     dip,
                                   an_init_pos_descr_ptr  ipdp,
                                   an_insert_location_ptr insert_location)
/*
Do IL lowering on a dynamic initialization entry that represents a destructor
call in a destructor's init list.  dip points to the dynamic initialization,
and ipdp identifies the entity to be destroyed.  The statements are inserted at
*insert_location and *insert_location is updated.
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
  /* Make an expression for the object to be destroyed. */
  entity_node = make_init_entity_node(ipdp);
  /* Generate code for the destructor call. */
  if (ipdp->whole_array) {
    /* Destruction of whole array. */
    add_array_destructor_call(dip, entity_node, ipdp->array_element_count,
                              insert_location);
  } else {
    /* Destruction of simple entity (non-array). */
    add_destructor_call(dip, entity_node, /*have_complete_object=*/TRUE,
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
  a_routine_ptr               ctor_routine;

  /* Get the array element type. */
  array_type = skip_typerefs(ndsp->type);
  elem_type = new_delete_base_type_from_operation_type(ndsp->type);
  ptr_elem_type = make_pointer_type(elem_type);
  /* Build the node for the address of the array (entity_node). */
#if NEW_CAN_BE_FOLDED_INTO_CTOR
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
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
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
    lower_arg_expr_list(ndsp->arg, ndsp->routine->type);
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
#if NEW_CAN_BE_FOLDED_INTO_CTOR
  }  /* if */
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
  /* Here, we have entity_node pointing to an expression for the address
     of the entity. */
  /* Make a node for the number of elements in the array. */
  if (array_type->size != 0) {
    /* The easy and usual case -- the array has a constant number of
       elements.  Do a division to get the right answer for the
       multi-dimensional array case. */
    set_unsigned_integer_constant(&num_elem_constant,
                                  array_type->size / elem_type->size,
                                  (an_integer_kind)TARG_SIZE_T_INT_KIND);
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
    lower_arg_expr_list(elem_dip->variant.constructor.args,
                        ctor_routine->type);
    ctor_routine = default_version_of_routine(ctor_routine,
                                           elem_dip->variant.constructor.args);
  } else {
    /* There is no dynamic init entry; the storage is not initialized after
       allocation. */
    ctor_routine = NULL;
  }  /* if */
  /* Construct the call of __vec_new. */
  vec_new_node = make_vec_new_call(entity_node, num_elem_node, ctor_routine);
#if NEW_CAN_BE_FOLDED_INTO_CTOR
  if (ndsp->routine != NULL) {
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
    /* Build a comma node that encloses the allocation call and the
       __vec_new call.  See comment above. */
    assign_node->next = vec_new_node;
    vec_new_node = make_operator_node((an_expr_operator_kind)eok_comma,
                                      vec_new_node->type, assign_node);
#if NEW_CAN_BE_FOLDED_INTO_CTOR
  }  /* if */
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
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


static a_boolean new_or_delete_type_requires_array_handling(a_type_ptr type)
/*
type is the base type underlying an array type involved in a new or delete.
Return TRUE if the new or delete operation requires special handling.
Special handling means the __vec_new and __vec_delete routines must be
called, so that constructors and destructors will be called, and so 
that the size of the array is recorded for use at the time of the delete
of the array pointer.
*/
{
  a_boolean                     special = FALSE;
  a_class_symbol_supplement_ptr cssp;

  /* Only types with a constructor or destructor require special handling. */
  if (is_class_struct_union_type(type)) {
    cssp = symbol_supplement_for_class(type);
    if (cssp->constructor != NULL || cssp->destructor != NULL) {
      special = TRUE;
    }  /* if */
  }  /* if */
  return special;
}  /* new_or_delete_type_requires_array_handling */


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
                          ctor_routine->type);
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
    lower_arg_expr_list(ndsp->arg, ndsp->routine->type);
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
      /* Build a description of the entity to be initialized.  Adjust the
         type so that it is an array if necessary. */
      set_var_indirect_init_pos_descr(temp_var, &ipd);
      ipd.base_type = ndsp->type;
      /* Generate code for the initialization. */
      lower_dynamic_init(dip, &ipd,
                         /*first_time_test_var=*/(a_variable_ptr)NULL,
                         /*is_expr_temporary=*/FALSE,
                         (an_expr_node_ptr)NULL, (an_expr_node_ptr)NULL,
                         (a_constructor_init_ptr)NULL,
                         &insert_location, &keep_dynamic_init);
      check_assertion(!keep_dynamic_init);
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
??=error -- DELETE_CAN_BE_FOLDED_INTO_DTOR set wrong.
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
    lower_arg_expr_list(ptr_node, ndsp->routine->type);
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
  /* Determine the type of the temporary. */
  temp_type = expr->type;
  result_is_not_used = expr->result_is_not_used;
  result_is_addr = expr->variant.init.result_is_addr;
  if (result_is_addr) {
    /* The value of the enk_temp_init node is the address of the temporary,
       so drop the pointer-to to get the temporary type. */
    temp_type = type_pointed_to(temp_type);
  }  /* if */
  /* Create a temporary variable. */
  dip->variable = make_temporary_possibly_at_file_scope(
                                           temp_type,
                                           processing_file_scope_init_routine);
  /* Change the enk_temp_init to a reference to the value or address
     of the temporary. */
  if (result_is_addr) {
    set_expr_node_kind(expr, (an_expr_node_kind)enk_variable_address);
  } else {
    set_expr_node_kind(expr, (an_expr_node_kind)enk_variable);
  }  /* if */
  expr->variant.variable = dip->variable;
  /* Generate code for the dynamic init. */
  set_var_init_pos_descr(dip->variable, &ipd);
  /* Any code generated for the dynamic initialization will be
     inserted before the original expression. */
  set_expr_insert_location(expr, &insert_location);
  lower_dynamic_init(dip, &ipd,
                     /*first_time_test_var=*/(a_variable_ptr)NULL,
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
    an_expr_node_ptr comma_expr = expr, first_operand;
    check_assertion(is_operation_node(comma_expr) &&
                    comma_expr->variant.operation.kind ==
                                             (an_expr_operator_kind)eok_comma);
    first_operand = comma_expr->variant.operation.operands;
    check_assertion(result_is_addr ? 
                      is_variable_address_node(first_operand->next) :
                      is_variable_node(first_operand->next));
    overwrite_node(comma_expr, first_operand);
  }  /* if */
}  /* lower_temp_init */


static void add_first_time_test(an_insert_location_ptr insert_location,
                                a_variable_ptr         *first_time_test_var)
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
for further insertion after the assignment statement.  A pointer to the
first-time-test variable is returned in *first_time_test_var.
*/
{
  a_variable_ptr     test_var;
  an_expr_node_ptr   test_var_node, compare_node;
  an_insert_location insert_location2;
  a_type_ptr         int_type;

  /* Make the static first-time-test variable at the file scope.  It's at the
     file scope so it can be reached in file-scope destructor code.
     (Local static variables are initialized when reached in their local
     blocks, but destroyed on exit from the whole program, and only if they
     were initialized). */
  int_type = integer_type((an_integer_kind)ik_int);
  *first_time_test_var = test_var =
                            make_lowered_variable((char *)NULL,
                                                  /*already_il_name=*/TRUE,
                                                  int_type,
                                                  (a_storage_class)sc_static);
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

static void add_static_data_member_init_guard_test(
                                           a_variable_ptr     variable,
                                           an_insert_location *insert_location,
                                           a_variable_ptr     *guard_var)
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
for further insertion after the assignment statement.  A pointer to the
guard variable is returned in *guard_var, or NULL if no guard code is
needed.
*/
{
  a_variable_ptr         test_var;
  an_expr_node_ptr       test_var_node, compare_node;
  an_insert_location     insert_location2;
  a_constant             minus_one_constant;
  a_memory_region_number region_to_switch_back_to;

  /* Make the guard variable at the file scope. */
  test_var = make_instantiation_var("__SDG__", (an_integer_kind)ik_int,
                                    &variable->source_corresp);
  if (variable->specific_def) {
    /* This variable is a specialization of a template entity, so its
       initialization should take precedence over any initialization code
       for other instances.  Initialize the guard variable to -1 to lock out
       all other initialization code.  No test of the guard variable is
       needed here.  Neither is any test needed at the time of destruction,
       so return *guard_var == NULL. */
    *guard_var = NULL;
    test_var->init_kind = (an_init_kind)initk_static;
    set_integer_constant(&minus_one_constant, -1L, (an_integer_kind)ik_int);
    switch_to_file_scope_region(&region_to_switch_back_to);
    test_var->initializer.constant =
                                  alloc_unshared_constant(&minus_one_constant);
    switch_back_to_original_region(region_to_switch_back_to);
  } else {
    /* This is not a specialization, so the guard variable must be tested here
       and at the time of destruction. */
    *guard_var = test_var;
    /* Make "test_var == 0". */
    test_var_node = var_rvalue_expr(test_var);
    test_var_node->next = node_for_integer_constant(0L,
                                                    (an_integer_kind)ik_int);
    compare_node = make_operator_node((an_expr_operator_kind)eok_ieq,
                                      integer_type((an_integer_kind)ik_int),
                                      test_var_node);
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
  }  /* if */
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

#if TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE

void add_static_data_member_destruction_guard_test(
                        a_variable_ptr         guard_var,
                        an_insert_location_ptr insert_location,
                        an_insert_location_ptr insert_location2)
/*
Add a sequence of code that tests the guard variable that protects
initialization and destruction of a static data member of a template.
The sequence is

    if (guard_var > 0) {
      guard_var = 0;
      ... destruction of static data member
    }

(The test is "> 0" instead of "!= 0" because a value of -1 is used when a
specialization is present.)

The sequence is inserted at *insert_location.  *insert_location is updated
for further insertion following the "if".  *insert_location2 is set for
insertion within the "if".
*/
{
  an_expr_node_ptr guard_var_node, compare_node;

  /* Make "guard_var > 0". */
  guard_var_node = var_rvalue_expr(guard_var);
  guard_var_node->next = node_for_integer_constant(0L,
                                                   (an_integer_kind)ik_int);
  compare_node = make_operator_node((an_expr_operator_kind)eok_igt,
                                    integer_type((an_integer_kind)ik_int),
                                    guard_var_node);
  /* Make an "if" statement and insert it into the program. */
  insert_if_statement(compare_node, insert_location, insert_location2);
  /* Add "guard_var = 0;" inside the "if". */
  (void)insert_var_assignment_statement(guard_var,
                                        (an_expr_operator_kind)eok_iassign,
                                        node_for_integer_constant(0L,
                                                      (an_integer_kind)ik_int),
                                        insert_location2);
}  /* add_static_data_member_destruction_guard_test */

#endif /* TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE */

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
    a_boolean          keep_dynamic_init;
    an_init_pos_descr  ipd;
    a_variable_ptr     first_time_test_var = NULL;
    a_context          context;

    set_insert_location(statement, &insert_location);
    set_var_init_pos_descr(dip->variable, &ipd);
    /* If the variable is a local static, add a first-time flag and a
       test. */
    if (dip->variable->storage_class == (a_storage_class)sc_static) {
      add_first_time_test(&insert_location, &first_time_test_var);
      /* Put a dependent-statement context around the lowering of
         the initialization so that any required destructor calls for
         code within the initialization will be emitted within the "if". */
      push_context(&context, curr_context->scope, /*subscope_region=*/TRUE);
    }  /* if */
    lower_dynamic_init(dip, &ipd, first_time_test_var,
                       /*is_expr_temporary=*/FALSE,
                       (an_expr_node_ptr)NULL, (an_expr_node_ptr)NULL,
                       (a_constructor_init_ptr)NULL,
                       &insert_location, &keep_dynamic_init);
    if (!keep_dynamic_init) {
      /* Delete the stmk_init statement. */
      turn_statement_into_noop(statement);
    }  /* if */
    if (first_time_test_var != NULL) {
      /* Generate any required destructor calls for temporaries built within
         a first-time test conditional section. */
      gen_required_destructor_calls(curr_context, &insert_location);
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


static void add_member_copy(an_init_pos_descr_ptr  dest,
                            a_constructor_init_ptr ctor_init,
                            an_insert_location_ptr insert_location)
/*
Make a block move to implement a copy in a copy constructor.  dest
describes the destination of the move.  ctor_init is the constructor
initialization entry, which gives (along with var_for_copy_constructor_source)
the information needed to build the source expression.  Insert the
statement at *insert_location and update *insert_location.
*/
{
  an_expr_node_ptr      source_node, dest_node;
  an_init_pos_descr     ipd;
  an_init_pos_modifier  ipm;

  /* Make an expression for the address of the destination entity. */
  dest_node = make_init_entity_node(dest);
  /* Make an expression for the address of the source entity.  This is
     done by getting the source parameter of the copy constructor and
     modifying it to select the same subobject as in the destination. */
  set_var_indirect_init_pos_descr(var_for_copy_constructor_source(), &ipd);
  modify_ctor_init_pos_descr(ctor_init, &ipd, &ipm);
  source_node = make_init_entity_node(&ipd);
  /* Make an assignment statement. */
  (void)insert_assignment_statement(dest_node,
                                    (an_expr_operator_kind)eok_bassign,
                                    source_node,
                                    insert_location);
}  /* add_member_copy */


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
  if (dip->kind == (a_dynamic_init_kind)dik_member_copy ||
      dip->kind == (a_dynamic_init_kind)dik_base_class_copy) {
    /* Special case -- copying a member or base class in a copy constructor. */
    add_member_copy(&ipd, ctor_init, insert_location);
  } else {
    lower_dynamic_init(dip, &ipd, /*first_time_test_var=*/(a_variable_ptr)NULL,
                       /*is_expr_temporary=*/FALSE,
                       implied_arg_list, end_implied_arg_list, ctor_init,
                       insert_location, &keep_dynamic_init);
#if CHECKING
    if (keep_dynamic_init) {
      internal_error("lower_ctor_init: keep_dynamic_init unexpected");
    }  /* if */
#endif /* CHECKING */
  }  /* if */
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
  a_context              context;

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
  /* Push a subscope region context around the generation of the wrapper
     code so that required destruction for any temporaries created
     within the wrapper will be generated at the end of the wrapper
     code. */
  push_context(&context, curr_context->scope, /*subscope_region=*/TRUE);
  /* The constructor_inits list contains a list of initializations.  Each
     initialization either appeared explicitly in the source or is a default
     initialization supplied by the front end.  Every base class and member
     that requires a constructor appears, in the order (1) virtual base
     classes, (2) normal base classes, (3) data members.  The order within
     each section is source declaration order. */
  ctor_init = scope->variant.routine.constructor_inits;
  scope->variant.routine.constructor_inits = NULL;
  /* Get a pointer to the "this" parameter variable. */
  this_param_var = scope->variant.routine.parameters;
  class_type =
            scope->variant.routine.ptr->source_corresp.class_of_which_a_member;
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
             type-as_subobject for the base class type to a pointer to the
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
    primary_vtbl_var->address_taken = TRUE;
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
    /* Set the pointer if it exists and is not shared with the current class
       pointer already set above. */
    vtbl_var = bcp->virtual_function_table_var;
    if (vtbl_var != NULL && vtbl_var != primary_vtbl_var) {
      /* The base class's virtual function table pointer must be set to
         reflect the fact that it exists as a subobject inside the current
         class. */
      vtbl_addr_node = make_vtbl_address_node(vtbl_var);
      vtbl_var->address_taken = TRUE;
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
  /* Generate any required destructor calls for temporaries built within
     the wrapper code. */
  gen_required_destructor_calls(curr_context, insert_location);
  pop_context();
}  /* add_constructor_wrapper_code */


void lower_constructor_code(a_scope_ptr scope)
/*
Insert constructor wrapper code around the user code in the indicated
constructor scope.
*/
{
  an_insert_location insert_location;
#if NEW_CAN_BE_FOLDED_INTO_CTOR
  a_routine_ptr      ctor_routine = scope->variant.routine.ptr;

  /* Add the wrapper code at the start of the routine. */
#if ASSIGNMENT_TO_THIS_ALLOWED
  /* If there is an assignment to "this" in the body of the constructor,
     do not issue the wrapper code here; it will be issued after each
     assignment to "this". */
  if (!ctor_routine->assignment_to_this_done) {
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
    /* Start off with code to allocate storage if "this" is NULL:
         if (this != NULL || (this = new-rout(size)) != NULL)
       The entire rest of the routine (both wrapper code and user code)
       are placed in the dependent statement of the "if". */
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

    /* Make "new-rout(size)". */
    class_type = ctor_routine->source_corresp.class_of_which_a_member;
    ctsp = class_type->variant.class_struct_union.extra_info;
    new_routine = ctsp->assoc_operator_new_routine;
    /* If there is no default new routine for the class, do not put out
       the code.  This happens if the class has a class-specific new but
       not one that takes a single argument. */
    if (new_routine != NULL) {
      size_node = node_for_integer_constant((long)class_type->size,
                                        (an_integer_kind)TARG_SIZE_T_INT_KIND);
      call_node = make_call_node(new_routine, size_node,
                                 /*honor_virtual=*/FALSE);
      /* Make "this = new_rout(size)". */
      call_node = add_cast_if_necessary(call_node, this_param_var->type);
      this_param_node = var_lvalue_expr(this_param_var);
      this_param_node->next = call_node;
      assign_node = make_operator_node((an_expr_operator_kind)eok_passign,
                                       call_node->type, this_param_node);
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
      set_block_start_insert_location(block_stmt, &insert_location);
    } else {
      /* No default operator new. */
      set_block_start_insert_location(scope->assoc_block, &insert_location);
    }  /* if */
#else /* !NEW_CAN_BE_FOLDED_INTO_CTOR */
    set_block_start_insert_location(scope->assoc_block, &insert_location);
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
    /* Add the wrapper code. */
    add_constructor_wrapper_code(scope, &insert_location);
#if NEW_CAN_BE_FOLDED_INTO_CTOR
#if ASSIGNMENT_TO_THIS_ALLOWED
  }  /* if */
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
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
initialized is a complete object.  The statements created are inserted
at *insert_location, and *insert_location is updated.
*/
{
  an_init_pos_descr    ipd;
  an_init_pos_modifier ipm;
  a_dynamic_init_ptr   dip;
  an_expr_node_ptr     subentity_node;
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
    lower_dynamic_init_aggregate_constant(dip->variant.constant,
                                          &ipd, (a_variable_ptr)NULL,
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
    subentity_node = make_init_entity_node(&ipd);
    add_destructor_call(dip, subentity_node, have_complete_object,
                        insert_location);
  }  /* if */
}  /* lower_dtor_init */


void lower_destructor_code(a_scope_ptr scope)
/*
Insert destructor wrapper code around the user code in the indicated
destructor scope.
*/
{
  a_base_class_ptr       bcp;
  a_variable_ptr         this_param_var, complete_obj_param_var;
  a_type_ptr             class_type, int_type;
  a_class_type_supplement_ptr
                         ctsp;
  a_constructor_init_ptr ctor_init;
  an_insert_location     insert_location, insert_location2;
  a_statement_ptr        top_level_stmt, label_stmt;
  a_statement_ptr        return_stmt;
  an_expr_node_ptr       zero_constant_node, complete_obj_param_node;
  an_expr_node_ptr       compare_node;
  an_expr_node_ptr       vtbl_addr_node, vptr_node;
  a_variable_ptr         primary_vtbl_var, vtbl_var;
  a_routine_ptr          dtor_routine = scope->variant.routine.ptr;

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
  int_type = integer_type((an_integer_kind)ik_int);
  set_block_start_insert_location(scope->assoc_block, &insert_location);
  /* The constructor_inits list contains a list of destructions.  Each
     destruction is a default call supplied by the front end.  Every
     base class and member that requires a destructor appears, in the
     order (1) data members, (2) normal base classes, (3) virtual base
     classes.  The order within each section is source declaration order. */
  ctor_init = scope->variant.routine.constructor_inits;
  scope->variant.routine.constructor_inits = NULL;
  /* Get a pointer to the "this" parameter variable. */
  this_param_var = scope->variant.routine.parameters;
  complete_obj_param_var = this_param_var->next;
  class_type = dtor_routine->source_corresp.class_of_which_a_member;
  ctsp = class_type->variant.class_struct_union.extra_info;
  /* If the current class has any virtual functions, generate code to
     set the virtual function table pointer in the current class. */
  primary_vtbl_var = ctsp->virtual_function_table_var;
  if (primary_vtbl_var != NULL) {
    /* Assign the primary virtual table address to the virtual table pointer
       in the current class. */
    vtbl_addr_node = make_vtbl_address_node(primary_vtbl_var);
    primary_vtbl_var->address_taken = TRUE;
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
    if (vtbl_var == NULL && !cfront_compatibility_mode) {
      a_class_type_supplement_ptr base_class_ctsp =
                              bcp->type->variant.class_struct_union.extra_info;
      /* Use the virtual function table for the base class as a complete
         object, if there is one. */
      vtbl_var = base_class_ctsp->virtual_function_table_var;
    }  /* if */
    if (vtbl_var != NULL && vtbl_var != primary_vtbl_var) {
      /* The base class virtual function table pointer must be set
         to reflect the fact that it exists as a subobject inside the
         current class. */
      vtbl_addr_node = make_vtbl_address_node(vtbl_var);
      vtbl_var->address_taken = TRUE;
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
  /* The user code in the destructor follows this point, so find the end of the
     top-level statement sequence and add the rest of the code there.  Return
     statements in the body of the destructor have already been turned into
     gotos to destructor_epilogue_label.
  */
  /* Note that there must be at least one statement in the top-level block. */
  for (top_level_stmt = scope->assoc_block->variant.block.statements;
       top_level_stmt->next != NULL;
       top_level_stmt = top_level_stmt->next) {}
  if (top_level_stmt->kind == (a_statement_kind)stmk_goto &&
      top_level_stmt->variant.label == destructor_epilogue_label) {
    /* The last top-level statement is a goto to the epilogue, so it can
       be turned into a no-op. */
    turn_statement_into_noop(top_level_stmt);
    count_of_refs_to_destructor_epilogue_label--;
  }  /* if */
  set_insert_location(top_level_stmt, &insert_location);
  destructor_epilogue_label->source_corresp.referenced =
                             (count_of_refs_to_destructor_epilogue_label != 0);
  if (destructor_epilogue_label->source_corresp.referenced) {
    /* Define the epilogue label. */
    label_stmt = alloc_statement((a_statement_kind)stmk_label);
    label_stmt->variant.label = destructor_epilogue_label;
    destructor_epilogue_label->variant.exec_stmt = label_stmt;
    destructor_epilogue_label->parent_block = scope->assoc_block;
    add_to_labels_list(destructor_epilogue_label);
    insert_statement(label_stmt, &insert_location);
  }  /* if */
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
    lower_dtor_init(ctor_init, this_param_var, /*have_complete_object=*/FALSE,
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
  /* Add code to free the storage if the "free" bit (0x1) is on in the
     added parameter:
       if ((param & 0x1) != 0) delete-routine((void *)this);
  */
  { an_expr_node_ptr this_param_node;
    an_expr_node_ptr and_node, two_constant_node, if_node;
    a_statement_ptr  call_stmt;
    a_routine_ptr    delete_routine;
    a_routine_type_supplement_ptr
                     delete_routine_rtsp;
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
    delete_routine_rtsp = f_skip_typerefs(delete_routine->type)->
                                                    variant.routine.extra_info;
    param1 = delete_routine_rtsp->param_type_list;
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
                                        (an_integer_kind)TARG_SIZE_T_INT_KIND);
    }  /* if */
    delete_routine->source_corresp.referenced = TRUE;
    call_stmt = make_call_statement(delete_routine, this_param_node);
    insert_statement(call_stmt, &insert_location2);
  }
  /* Add a return statement at the end of the routine. */
  return_stmt = alloc_statement((a_statement_kind)stmk_return);
  insert_statement(return_stmt, &insert_location);
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

  dip = file_scope->dynamic_inits;
  if (dip != NULL) {
    /* There are some file-scope dynamic initializations.  Generate a routine
       containing them. */
    scope = file_scope_init_insert_location(&insert_location);
    push_context(&context, scope, /*subscope_region=*/FALSE);
    switch_il_region(file_scope_init_routine_il_region);
    processing_file_scope_init_routine = TRUE;
    for (; dip != NULL; dip = dip->next) {
      set_var_init_pos_descr(dip->variable, &ipd);
      lower_dynamic_init(dip, &ipd,
                         /*first_time_test_var=*/(a_variable_ptr)NULL,
                         /*is_expr_temporary=*/FALSE,
                         (an_expr_node_ptr)NULL, (an_expr_node_ptr)NULL,
                         (a_constructor_init_ptr)NULL,
                         &insert_location, &keep_dynamic_init);
#if CHECKING
      if (keep_dynamic_init) {
        internal_error(
               "lower_file_scope_dynamic_inits: keep_dynamic_init unexpected");
      }  /* if */
#endif /* CHECKING */
    }  /* for */
    processing_file_scope_init_routine = FALSE;
    pop_context();
    done_with_memory_region(file_scope_init_routine_il_region);
    switch_il_region(FILE_SCOPE_REGION_NUMBER);
    file_scope->dynamic_inits = NULL;
  }  /* if */
  /* Put destructor calls for local static variables on the front of
     the file-scope list. */
  if (destructor_calls_for_local_static_variables != NULL) {
    end_destructor_calls_for_local_static_variables->next =
                                       curr_context->required_destructor_calls;
    curr_context->required_destructor_calls =
                                   destructor_calls_for_local_static_variables;
  }  /* if */
  /* Generate any destructor calls associated with the file scope. */
  if (curr_context->required_destructor_calls != NULL) {
    /* There are some file-scope required destructor calls.  Generate a
       routine containing them. */
    scope = file_scope_term_insert_location(&insert_location);
    push_context(&context, scope, /*subscope_region=*/FALSE);
    switch_il_region(file_scope_term_routine_il_region);
    gen_required_destructor_calls(file_scope_context, &insert_location);
    pop_context();
    done_with_memory_region(file_scope_term_routine_il_region);
    switch_il_region(FILE_SCOPE_REGION_NUMBER);
  }  /* if */
}  /* lower_file_scope_dynamic_inits */


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
  vec_new_routine = vec_cctor_routine = vec_delete_routine = NULL;
  file_scope_init_routine = NULL;
  file_scope_term_routine = NULL;
  destructor_calls_for_local_static_variables = NULL;
  end_destructor_calls_for_local_static_variables = NULL;
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
