/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
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
#include "class_decl.h"
#include "expr.h"
#include "exprutil.h"
#if MAINTAIN_NEEDED_FLAGS
#include "il_walk.h"
#endif /* MAINTAIN_NEEDED_FLAGS */


/* Declarations needed because of forward references: */
static void lower_destructor_dynamic_init(
                                   a_dynamic_init_ptr     dip,
                                   an_init_pos_descr_ptr  ipdp,
                                   a_boolean              have_complete_object,
                                   an_expr_node_ptr       vtt_addr_node,
                                   an_insert_location_ptr insert_location);
static void reset_conditional_flag_var(a_variable_ptr     conditional_flag_var,
                                       an_insert_location *insert_location);
static void insert_call_to_zero_entity(an_expr_node_ptr   entity_node,
                                       an_expr_node_ptr   entity_size_node,
                                       an_insert_location *insert_location);
#if IA64_ABI
static void insert_call_to_helper_routine_to_zero_entity(
                                          a_type_ptr         entity_type,
                                          an_expr_node_ptr   entity_node,
                                          an_expr_node_ptr   num_elements,
                                          an_insert_location *insert_location);
#endif /* IA64_ABI */
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
static a_variable_ptr make_construction_vtbls_array(
                                           a_type_ptr              class_type,
                                           a_construction_vtbl_ptr elements);
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */


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
                                              !make_all_functions_unprototyped;
  if (param_1_type != NULL) {
    ptp = alloc_param_type(param_1_type);
    /* It is not necessary to clear il_lowering_flag; the entry does not need
       to be lowered. */
    rout_type->variant.routine.extra_info->param_type_list = ptp;
  }  /* if */
  return rout_type;
}  /* make_function_type */


static a_routine_ptr make_rout_entry_no_add(char            *name,
                                            a_storage_class rout_storage_class,
                                            a_type_ptr      return_type,
                                            a_type_ptr      param_1_type)
/*
Make a routine entry for a function with the given name, prototyped as
having a parameter with type param_1_type and returning return_type,
and having storage class rout_storage_class.  Return a pointer to the
routine entry created.  The routine entry and its type are allocated
in the file scope.  If no arguments are desired, param_1_type should
be specified as NULL.  The name may be NULL.  The routine entry is
not added to any routines list; see make_rout_entry for that.
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
  rout->compiler_generated = TRUE;
  return rout;
}  /* make_rout_entry_no_add */


static a_routine_ptr make_rout_entry(char            *name,
                                     a_storage_class rout_storage_class,
                                     a_type_ptr      return_type,
                                     a_type_ptr      param_1_type)
/*
Make a routine entry by calling make_rout_entry_no_add, then add
the routine to the file-scope routines list.
*/
{
  a_routine_ptr rout;

  rout = make_rout_entry_no_add(name, rout_storage_class, return_type,
                                param_1_type);
  /* Add the routine to the file scope list. */
  add_to_routines_list(rout, DEPTH_OF_FILE_SCOPE);
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


#if !IA64_ABI
/*ARGSUSED*/ /* <-- class_type and bcp are unused in that case. */
#endif /* !IA64_ABI */
static an_expr_node_ptr make_vtbl_address_node(a_variable_ptr   var,
                                               a_type_ptr       class_type,
                                               a_base_class_ptr bcp)
/*
Make an expression for the address of a virtual function table variable (var)
and return a pointer to it.  class_type is the type whose constructor or
destructor is being generated; bcp is the base whose virtual function table is
being addressed, or NULL if the primary virtual function table is being
addressed.  The variable has an array type.  The pointer has type pointer to
element.
*/
{
  an_expr_node_ptr            var_node;
  a_constant                  addr_constant;
  a_type_ptr                  ptr_element_type;
#if IA64_ABI
  a_class_type_supplement_ptr ctsp;
  a_virtual_table_index       vtbl_index;
#endif /* IA64_ABI */

  ptr_element_type = make_pointer_type(array_element_type(var->type));
  /* Make a constant for the address of the array, implicitly cast it to
     pointer-to-element-type, and make an expression whose value is the
     address constant.  This gives an address with the right type. */
  set_variable_address_constant(var, &addr_constant,
                                /*set_address_taken_flag=*/FALSE);
  implicit_cast(&addr_constant, ptr_element_type);
#if IA64_ABI
  /* Add the offset from the start of the variable to the actual address
     point. */
  if (bcp != NULL) {
    check_assertion(!bcp->shares_virtual_function_info);
    ctsp = bcp->type->variant.class_struct_union.extra_info;
  } else {
    ctsp = class_type->variant.class_struct_union.extra_info;
  }  /* if */
  if (bcp != NULL && emit_vcall_offsets_in_virtual_function_table(bcp)) {
    vtbl_index = -ctsp->next_negative_virtual_table_index - 1;
  } else {
    vtbl_index = -ctsp->first_vcall_offset_index - 1;
  }  /* if */
  if (bcp != NULL) {
    vtbl_index += bcp->virtual_function_table_offset;
  }  /* if */
  addr_constant.variant.address.offset = 
                                     vtbl_index * make_vtbl_entry_type()->size;
#endif /* IA64_ABI */
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
  a_type_ptr ptr_to_data_member_type =
                                integer_type(targ_ptr_to_data_member_int_kind);
  a_type_ptr promoted_type =
                           default_argument_promotion(ptr_to_data_member_type);

  if (!same_entities(ptr_to_data_member_type, promoted_type)) {
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
  if (is_arithmetic_or_enum_type(arg_type)) {
    /* Note that no special handling is done for bit fields because they
       have already been cast to the prototyped parameter type and therefore
       have lost whatever type malleability they might have had. */
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
  if (!same_entities(promoted_type, arg_type)) {
    /* Put in the promotion cast. */
    an_expr_node_ptr expr_cast = expr, expr_next = expr->next;
    an_expr_node     node_copy;

    cast_node(&expr_cast, promoted_type, /*check_cast_access=*/TRUE,
              /*is_implicit_cast=*/TRUE, /*is_reinterpret_cast=*/FALSE,
              /*reinterpret_semantics=*/FALSE, &error_position);
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


an_expr_node_ptr make_call_node(a_routine_ptr      routine,
                                an_expr_node_ptr   arg_list,
                                a_boolean          honor_virtual,
                                an_insert_location *insert_location)
/*
Make an expression that calls routine "routine" with arguments "arg_list",
and return a pointer to it.  arg_list is assumed to be lowered already.
A virtual call is generated if the routine is virtual and honor_virtual
is TRUE.  The virtual call is *not* lowered.  If insert_location is not
NULL, an expression statement containing the created call node is inserted
at *insert_location.
*/
{
  an_expr_node_ptr      call_node, rout_node;
  a_type_ptr            rout_return_type;
  an_expr_operator_kind op;
#if MINIMAL_INLINING
  a_statement_ptr       call_stmt = NULL;
#endif /* MINIMAL_INLINING */

  if (make_all_functions_unprototyped) {
    /* If transforming all functions to old-style unprototyped form (for
       cfront compatibility), do default argument promotions on the arguments.
       It might seem wasteful to do this on every argument list, since
       not many of the arguments will require promotion.  However, doing it
       here guarantees that all calls created by IL lowering will have
       properly-promoted arguments without special-case checks all over the
       place. */
    an_expr_node_ptr arg_node;
    for (arg_node = arg_list; arg_node != NULL; arg_node = arg_node->next) {
      do_default_arg_promotions_on_node(arg_node);
    }  /* for */
  }  /* if */
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
  rout_return_type = il_return_type_of(routine->type);
  call_node = make_operator_node(op, rout_return_type, rout_node);
  if (insert_location != NULL) {
#if MINIMAL_INLINING
    call_stmt = insert_expr_statement_set_pos(call_node, insert_location);
#else /* !MINIMAL_INLINING */
    (void)insert_expr_statement_set_pos(call_node, insert_location);
#endif /* MINIMAL_INLINING */
  }  /* if */
#if MINIMAL_INLINING
  if (inlining_enabled && op == (an_expr_operator_kind)eok_call) {
    do_inlining_of_call(call_node, call_stmt);
  }  /* if */
#endif /* MINIMAL_INLINING */
  return call_node;
}  /* make_call_node */


void make_call_statement(a_routine_ptr      routine,
                         an_expr_node_ptr   arg_list,
                         an_insert_location *insert_location)
/*
Make a statement that calls routine "routine" with arguments "arg_list"
and insert it at *insert_location.  arg_list is assumed to be lowered already.
*/
{
  (void)make_call_node(routine, arg_list, /*honor_virtual=*/FALSE,
                       insert_location);
}  /* make_call_statement */


an_expr_node_ptr make_runtime_rout_call(char             *name,
                                        a_routine_ptr    *routine,
                                        a_type_ptr       return_type,
                                        an_expr_node_ptr arg_expr_list)
/*
Make an expression node that calls the runtime routine "name" with the
arguments given by arg_expr_list.  *routine is set to point to the runtime
routine entry; if it is non-NULL on entry, it is used.  The routine has
unprototyped arguments and its return type is return_type.  arg_expr_list
is assumed to be lowered already.
*/
{
  an_expr_node_ptr node;

  /* Make the routine entry if it does not exist already. */
  (void)make_runtime_routine(name, routine, return_type);
  /* Make the call node. */
  node = make_call_node(*routine, arg_expr_list, /*honor_virtual=*/FALSE,
                        (an_insert_location *)NULL);
  return node;
}  /* make_runtime_rout_call */


void turn_statement_into_noop(a_statement_ptr statement)
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


an_expr_node_ptr zero_cast_to_void(void)
/*
Return an expression for "(void)0", a zero constant cast to void.
*/
{
  an_expr_node_ptr zero_node =
                        node_for_integer_constant(0L, (an_integer_kind)ik_int);
  an_expr_node_ptr expr = add_cast(zero_node, void_type());

  return expr;
}  /* zero_cast_to_void */


static void insert_if_statement(an_expr_node_ptr       test_expr,
                                a_boolean              is_initialization_guard,
                                an_insert_location_ptr insert_location,
                                a_statement_ptr        *p_block_stmt,
                                an_insert_location_ptr then_insert_location,
                                an_insert_location_ptr else_insert_location)
/*
Create an "if" statement that tests test_expr, and insert it at
insert_location.  Set *then_insert_location to allow insertion of the
dependent statements of the "if".  Set *p_block_stmt to point to the
block statement added, unless p_block_stmt is NULL.  This routine also
handles the case of inserting an if-equivalent into the middle of an
expression.  If this test is the guard code around an initialization,
is_initialization_guard is TRUE; that's used to indicate to a back
end that the test-and-set of the guard flag should be done as an
atomic operation.  If else_insert_location is non-NULL, add an "else"
to the "if", and set *else_insert_location to allow insertion in the
"else".
*/
{
  a_statement_ptr  if_stmt, block_stmt = NULL, else_stmt;
  an_expr_node_ptr question_node, op2_node, op3_node;

  if (is_expr_insert_location_kind(insert_location->kind)) {
    /* Insert within an expression. */
    /* Insert "test_expr ? (void)0 : (void)0" at the right place. */
    /* The second and third operands are each "(void)0". */
    op2_node = zero_cast_to_void();
    op3_node = zero_cast_to_void();
    test_expr->next = op2_node;
    op2_node->next = op3_node;
    question_node = make_operator_node((an_expr_operator_kind)eok_question,
                                       op2_node->type, test_expr);
    question_node->is_initialization_guard = is_initialization_guard;
    insert_expr(question_node, insert_location);
    /* The insert location is before the "(void)0" of the second operand. */
    set_expr_insert_location(op2_node, then_insert_location);
    if (else_insert_location != NULL) {
      /* The "else" insert location is before the "(void)0" of the third
         operand. */
      set_expr_insert_location(op3_node, else_insert_location);
    }  /* if */
  } else {
    /* Insert within a statement sequence.  Allocate an "if" statement with
       a block statement under it. */
    if_stmt = alloc_statement((a_statement_kind)stmk_if);
    if_stmt->expr = test_expr;
    if_stmt->is_initialization_guard = is_initialization_guard;
    insert_statement(if_stmt, insert_location);
    if_stmt->variant.if_stmt.then_statement = block_stmt =
                                 alloc_statement((a_statement_kind)stmk_block);
    set_block_start_insert_location(block_stmt, then_insert_location);
    if (else_insert_location != NULL) {
      if_stmt->variant.if_stmt.else_statement = else_stmt =
                                 alloc_statement((a_statement_kind)stmk_block);
      set_block_start_insert_location(else_stmt, else_insert_location);
    }  /* if */
  }  /* if */
  if (p_block_stmt != NULL) *p_block_stmt = block_stmt;
}  /* insert_if_statement */


static a_boolean move_final_return_out_of_block(
                                             a_statement_ptr block_stmt,
                                             a_statement_ptr insert_after_stmt)
/*
If the last statement of the block block_stmt is a return, move it out of
the block and after insert_after_stmt.  If the last statement of the block
is also a block, look recursively inside that block to see whether
its last statement is a return, etc.  Return TRUE if a return was
moved out of the block.
*/
{
  a_boolean       return_moved = FALSE;
  a_statement_ptr stmt, prev_stmt, temp_block_stmt, temp2_block_stmt;

  check_assertion(block_stmt != NULL &&
                  block_stmt->kind == (a_statement_kind)stmk_block);
  for (temp_block_stmt = block_stmt; ; temp_block_stmt = stmt) {
    stmt = temp_block_stmt->variant.block.statements;
    if (stmt == NULL) break;
    /* Find the last statement in the block. */
    for (prev_stmt = NULL;
         stmt->next != NULL;
         prev_stmt = stmt, stmt = stmt->next) {}
    /* If the last statement is itself a block, look inside it. */
    if (stmt->kind != (a_statement_kind)stmk_block) break;
  }  /* for */
  if (stmt != NULL && stmt->kind == (a_statement_kind)stmk_return) {
    /* The last statement is a return.  Move it. */
    if (prev_stmt == NULL) {
      temp_block_stmt->variant.block.statements = NULL;
    } else {
      prev_stmt->next = NULL;
    }  /* if */
    stmt->next = insert_after_stmt->next;
    insert_after_stmt->next = stmt;
    return_moved = TRUE;
    /* Mark the block from which the return was removed (and any
       surrounding it, out to block_stmt) as reachable. */
    for (temp2_block_stmt = block_stmt;
         /* Termination test in loop. */;
         temp2_block_stmt = last_statement_in_block(temp2_block_stmt)) {
      temp2_block_stmt->variant.block.extra_info->
                                                 end_of_block_reachable = TRUE;
      if (temp2_block_stmt == temp_block_stmt) break;
    }  /* for */
  }  /* if */
  return return_moved;
}  /* move_final_return_out_of_block */


static void enclose_routine_in_if(a_scope_ptr      scope,
                                  an_expr_node_ptr if_node,
                                  a_variable_ptr   return_var)
/*
Add an "if" statement around the entire body of the routine whose scope is
pointed to by scope.  if_node is the expression to be tested in the "if".
return_var is the variable to be returned if a "return" statement must be
generated, or NULL if no value needs to be returned.
*/
{
  a_statement_ptr if_stmt, block_stmt;

  if_stmt = alloc_statement((a_statement_kind)stmk_if);
  if_stmt->expr = if_node;
  if_stmt->variant.if_stmt.then_statement = block_stmt =
                                 alloc_statement((a_statement_kind)stmk_block);
  /* Make the "if" the top-level statement in the routine, and put the
     original code under the "if". */
  check_assertion_str(scope->assoc_block->kind == (a_statement_kind)stmk_block,
                      "enclose_routine_in_if: top stmt not block");
  block_stmt->variant.block.statements =
                                  scope->assoc_block->variant.block.statements;
  block_stmt->variant.block.extra_info->end_of_block_reachable = FALSE;
  scope->assoc_block->variant.block.statements = if_stmt;
  /* See if there is a return statement at the end of the original list of
     statements.  If so, move it outside the "if". */
  (void)move_final_return_out_of_block(block_stmt, if_stmt);
  /* If there is no return statement at the end of the routine (because the
     end of the original routine was not reachable), add one (because the
     end of the new routine is reachable if the "if" is not taken). */
  if (if_stmt->next == NULL) {
    a_statement_ptr return_stmt =
                                alloc_statement((a_statement_kind)stmk_return);
    if_stmt->next = return_stmt;
    if (return_var != NULL) return_stmt->expr = var_rvalue_expr(return_var);
    add_to_return_memo_list(return_stmt);
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
block.  push/pop_generated_routine_context should be called on the created
routine later in order to ensure that the "defined" flag is set.
*/
{
  a_scope_ptr            scope;
  a_memory_region_number region_to_switch_back_to = curr_il_region_number;
  a_statement_ptr        block_stmt;
  a_type_ptr             rout_type;

  /* Make a new memory region and scope. */
  scope = new_il_region((a_scope_kind)sck_function, take_next_scope_number(),
                        rout_ptr);
  *il_region = curr_il_region_number;
  /* Link the routine to the scope.  new_il_region did the link in the
     other direction. */
  rout_type = skip_typerefs(rout_ptr->type);
  rout_type->variant.routine.extra_info->assoc_routine = rout_ptr;
  rout_ptr->assoc_scope = curr_il_region_number;
  if (rout_ptr->storage_class == (a_storage_class)sc_extern) {
    rout_ptr->storage_class = (a_storage_class)sc_unspecified;
  }  /* if */
  /* The "defined" flag is set in pop_generated_routine_context. */
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


static void set_lowering_variable_address_taken(a_variable_ptr variable)
/*
Set the address_taken flag in the indicated variable.
*/
{
  set_variable_address_taken(variable);
  /* If the storage class is "register", change it to "auto", because
     C doesn't allow taking the address of a register variable (C++ does). */
  if (variable->storage_class == (a_storage_class)sc_register) {
    variable->storage_class = (a_storage_class)sc_auto;
  }  /* if */
}  /* set_lowering_variable_address_taken */


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
sometimes allocated on the stack.
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


static void free_init_pos_modifier_list(an_init_pos_modifier_ptr ipmp)
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
saved so that a destruction may be generated later.
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


static void clear_init_pos_descr(an_init_pos_descr_ptr ipdp)
/*
Clear an initialization position description entry to default values.
*/
{
  ipdp->variable                  = NULL;
#if !DO_FULL_PORTABLE_EH_LOWERING
  ipdp->thrown_object_address     = FALSE;
#endif /* !DO_FULL_PORTABLE_EH_LOWERING */
  ipdp->indirect_through_variable = FALSE;
  ipdp->array_element_sequence    = FALSE;
  ipdp->base_class_subobject      = FALSE;
  ipdp->base_type                 = NULL;
  ipdp->modifiers                 = NULL;
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

#if !DO_FULL_PORTABLE_EH_LOWERING

void set_thrown_object_init_pos_descr(a_type_ptr            throw_type,
                                      an_init_pos_descr_ptr ipdp)
/*
Make an initialization position description entry for the runtime location
to which a thrown object should be copied.  throw_type is the type of
object being thrown.
*/
{
  clear_init_pos_descr(ipdp);
  ipdp->thrown_object_address = TRUE;
  ipdp->base_type = throw_type;
}  /* set_thrown_object_init_pos_descr */

#endif /* !DO_FULL_PORTABLE_EH_LOWERING */

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


static void copy_init_pos_descr(an_init_pos_descr *source_ipdp,
                                an_init_pos_descr *dest_ipdp)
/*
Copy an initialization position description from source_ipdp to dest_ipdp.
If there are modifiers, copy them too.  This is usually necessary because the
source description and its modifiers are in the stack.
*/
{
  *dest_ipdp = *source_ipdp;
  if (source_ipdp->modifiers != NULL) {
    /* Copy the modifiers. */
    dest_ipdp->modifiers = copy_init_pos_modifier_list(source_ipdp->modifiers);
  }  /* if */
}  /* copy_init_pos_descr */


static a_boolean init_pos_is_static(an_init_pos_descr_ptr ipdp)
/*
Return TRUE if the indicated initialization position is for a static
variable (or a part of one).
*/
{
  a_boolean is_for_static_var = !ipdp->indirect_through_variable &&
#if !DO_FULL_PORTABLE_EH_LOWERING
                                !ipdp->thrown_object_address &&
#endif /* !DO_FULL_PORTABLE_EH_LOWERING */
                    has_static_storage_duration(ipdp->variable->storage_class);
  return is_for_static_var;
}  /* init_pos_is_static */


a_destructible_entity_descr_ptr alloc_destructible_entity_descr(void)
/*
Allocate a destructible entity description, set its fields to default values,
and return a pointer to it.
*/
{
  a_destructible_entity_descr_ptr dedp;

  if (avail_destructible_entity_descrs != NULL) {
    /* Reuse a freed entry. */
    dedp = avail_destructible_entity_descrs;
    avail_destructible_entity_descrs = dedp->next;
  } else {
    /* Allocate a new entry. */
    dedp = (a_destructible_entity_descr_ptr)
                                 alloc_fe(sizeof(a_destructible_entity_descr));
#if DEBUG
    num_destructible_entity_descrs_allocated++;
#endif /* DEBUG */
  }  /* if */
  dedp->next = NULL;
  clear_init_pos_descr(&dedp->init_pos_descr);
  dedp->conditional_flag_var = NULL;
#if DO_FULL_PORTABLE_EH_LOWERING
  dedp->conditional_flag_handle = 0;
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
#if GENERATE_EH_TABLES
  dedp->region_number = null_eh_region_number;
  dedp->cleanup_state_to_set_when_starting_destruction = NULL;
  dedp->region_table_entry = NULL;
  dedp->next_in_region_table = NULL;
#endif /* GENERATE_EH_TABLES */
  dedp->initialization_done = FALSE;
  return dedp;
}  /* alloc_destructible_entity_descr */


void free_destructible_entity_descr(a_destructible_entity_descr_ptr dedp)
/*
Free a destructible entity description by putting it on the available list.
*/
{
  /* Free any attached modifiers. */
  free_init_pos_modifier_list(dedp->init_pos_descr.modifiers);
  /* Put the entry on the available list. */
  dedp->next = avail_destructible_entity_descrs;
  avail_destructible_entity_descrs = dedp;
}  /* free_destructible_entity_descr_list */


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
      ipdp->base_class_subobject = TRUE;
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
description for the complete entity being initialized (or NULL for
an internal adjustment, e.g., for an array element).  entity_node
cannot be a bitfield selection.
*/
{
  a_type_ptr entity_type = type_pointed_to(entity_node->type);

  /* If the entity is an array, don't drop the const at this level.  It
     will be dropped on the address of the array element once that is
     extracted. */
  if (is_const_qualified_type(entity_type) && !is_array_type(entity_type)) {
    a_type_qualifier_set qualifiers = get_type_qualifiers(entity_type);
    qualifiers &= ~(a_type_qualifier_set)TQ_CONST;
    entity_type = make_unqualified_type(entity_type);
    entity_type = make_qualified_type(entity_type, qualifiers);
    entity_node = add_cast(entity_node, make_pointer_type(entity_type));
    /* Because of the cast, we're using the object's address as a real
       address, not just as an lvalue address, so set the address taken
       flag if appropriate.  Note that the interpretation of the
       address_taken flag has changed a few times, so the processing
       here is conservative -- it sets the flag in all cases, which
       guarantees it will work. */
    if (ipdp != NULL &&
        !ipdp->indirect_through_variable && ipdp->variable != NULL) {
      set_lowering_variable_address_taken(ipdp->variable);
    }  /* if */
  }  /* if */
  return entity_node;
}  /* drop_const_on_init_entity_node */


static an_expr_node_ptr modify_init_entity_node(
                                        an_expr_node_ptr         entity_node,
                                        an_init_pos_modifier_ptr modifiers,
                                        a_boolean                using_as_dest)
/*
Add the address modifiers from the list given by "modifiers" (from an
init position description) to the entity address expression "entity_node"
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
    entity_node = modify_init_entity_node(entity_node, modifiers->next,
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
      entity_node = add_cast(entity_node, make_pointer_type(elem_type));
      if (using_as_dest) {
        /* The entity will be used as the destination of an initialization, so
           drop "const" (if present) from the type to make it modifiable. */
        entity_node = drop_const_on_init_entity_node(entity_node,
                                                  (an_init_pos_descr_ptr)NULL);
      }  /* if */
      if (modifiers->curr_elem != 0) {
        /* Add the subscript if it's non-zero. */
        elem_num_node = node_for_host_large_integer(
             (a_host_large_integer)modifiers->curr_elem, targ_size_t_int_kind);
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
#if !DO_FULL_PORTABLE_EH_LOWERING
  if (ipdp->thrown_object_address) {
    /* The address is the address in the runtime to which a thrown object
       should be copied. */
    entity_node = make_thrown_object_address_node();
    entity_node = add_cast_if_necessary(entity_node,
                                        make_pointer_type(ipdp->base_type));
  } else
#endif /* !DO_FULL_PORTABLE_EH_LOWERING */
  /* Do not insert code here; this is the else of the above if. */
  if (ipdp->indirect_through_variable) {
    /* Indirect through the variable. */
    check_assertion(ipdp->variable != NULL);
    entity_node = var_rvalue_expr(ipdp->variable);
  } else {
    /* Normal case, a simple variable. */
    check_assertion(ipdp->variable != NULL);
    entity_node = var_lvalue_expr(ipdp->variable);
    /* If we will be using this expression as an address, set the address-taken
       flag in the variable. */
    if (using_as_address) set_lowering_variable_address_taken(ipdp->variable);
  }  /* if */
  if (using_as_dest) {
    /* The entity will be used as the destination of an initialization, so
       drop "const" (if present) from the type to make it modifiable. */
    entity_node = drop_const_on_init_entity_node(entity_node, ipdp);
  }  /* if */
  /* Add the modifiers to the base address. */
  entity_node = modify_init_entity_node(entity_node, ipdp->modifiers,
                                        using_as_dest);
  return entity_node;
}  /* make_init_entity_node */


static void add_init_assignment(a_dynamic_init_ptr     dip,
                                a_constant_ptr         con,
                                an_expr_node_ptr       entity_node,
                                an_insert_location_ptr insert_location)
/*
Make an assignment statement to implement the dynamic initialization
described by dip.  If dip is NULL, con indicates the constant value of
the initializer.  entity_node is an expression that gives the address
of the entity to be initialized.  Insert the statement at *insert_location
and update *insert_location.  The constant or expression initial value
pointed to by dip or con is already lowered.
*/
{
  an_expr_node_ptr      init_val_node, assign_node;
  a_statement_ptr       assign_stmt;
  an_expr_operator_kind op;
  a_boolean             string_literal_case = FALSE;

  switch ((dip == NULL) ? (a_dynamic_init_kind)dik_constant : dip->kind) {
    case dik_zero:
      /* Set the entity to zero (default initialization). */
      { a_constant     zero_constant;
        a_type_ptr     entity_type = type_pointed_to(entity_node->type);
        make_zero_of_proper_type(entity_type, &zero_constant);
        con = alloc_shareable_constant(&zero_constant);
        /* Lower the zero constant so that (e.g.) pointer to data member
           constants become the right integral constants. */
        mark_as_not_visited(con);
        lower_constant(con);
        init_val_node = make_node_for_il_constant(con);
      }
      break;
    case dik_constant:
      /* Assign a constant to the entity to be initialized. */
      /* The constant has already been lowered. */
      if (dip != NULL) con = dip->variant.constant;
      if (con->kind == (a_constant_repr_kind)ck_string &&
          !con->implicit_cast) {
        /* An character array initialized by a string literal, e.g., in
           a ctor-initializer. */
        a_constant addr_con;
        set_constant_address_constant(con, &addr_con);
        init_val_node = alloc_node_for_constant(&addr_con);
        string_literal_case = TRUE;
      } else {
        /* Normal case, not a string literal. */
        init_val_node = make_node_for_il_constant(con);
      }  /* if */
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
     initialization. */
  if (string_literal_case) {
    op = (an_expr_operator_kind)eok_bassign;
  } else {
    op = lowered_assignment_operator(init_val_node->type);
  }  /* if */
  assign_node = make_assignment_expr(entity_node, op, init_val_node);
  if (op == (an_expr_operator_kind)eok_sassign) {
    /* Eliminate empty base class assignments. */
    eliminate_assignment_if_empty_class(assign_node);
  }  /* if */
  assign_stmt = insert_expr_statement(assign_node, insert_location);
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
#if !IA64_ABI
  a_type_ptr       class_type;
  a_base_class_ptr bcp;
#endif /* IA64_ABI */

  curr_routine = innermost_function_scope->variant.routine.ptr;
#if CHECKING
  if (curr_routine->special_kind != (a_special_function_kind)sfk_constructor) {
    internal_error(
             "var_for_copy_constructor_source: curr routine not constructor");
  }  /* if */
#endif /* CHECKING */
  source_param_var= innermost_function_scope->variant.routine.parameters->next;
  check_assertion_str(source_param_var != NULL,
                      "var_for_copy_constructor_source: source param missing");
#if !IA64_ABI
  /* Skip over any parameters added for virtual base class pointers.
     See add_constructor_params. */
  class_type = curr_routine->source_corresp.parent.class_type;
  if (class_type->variant.class_struct_union.any_virtual_base_classes) {
    for (bcp = class_type->variant.class_struct_union.extra_info->base_classes;
         bcp != NULL;
         bcp = bcp->next) {
      if (bcp->is_virtual) {
        source_param_var = source_param_var->next;
        check_assertion_str(source_param_var != NULL,
                  "var_for_copy_constructor_source: source param missing (2)");
      }  /* if */
    }  /* for */
  }  /* if */
#else /* IA64_ABI */
  /* Skip over the VTT parameter. */
  source_param_var = source_param_var->next;
  check_assertion_str(source_param_var != NULL,
                  "var_for_copy_constructor_source: source param missing (3)");
#endif /* IA64_ABI */
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
    a_variable_ptr catch_parameter;
    a_type_ptr     param_type;

    /* We expect a simple catch parameter as the destination. */
    check_assertion(dest->modifiers == NULL &&
                    !dest->indirect_through_variable);
    catch_parameter = dest->variable;
    param_type = catch_parameter->type;
    /* Make the address of the caught object. */
    source_node = make_caught_object_address_node(param_type);
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
  if (is_arithmetic_or_enum_type(type) ||
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


/*
Return TRUE if the indicated constructor routine needs added implied arguments.
This must match make_ctor_implied_arg_list.
*/
#if !IA64_ABI
#define ctor_needs_implied_arg_list(ctor_routine)                     \
  ((ctor_routine)->source_corresp.parent.class_type->                 \
                 variant.class_struct_union.any_virtual_base_classes)
#else /* IA64_ABI */
#define ctor_needs_implied_arg_list(ctor_routine) TRUE
#endif /* IA64_ABI */

void make_ctor_implied_arg_list(a_routine_ptr    ctor_routine,
                                an_expr_node_ptr *implied_arg_list,
                                an_expr_node_ptr *end_implied_arg_list)
/*
Build and return a list of the implied arguments to be added to a call of the
constructor ctor_routine.  The beginning and end of the list are returned in
*implied_arg_list and *end_implied_arg_list.  For an empty list, both will be
set to NULL.
*/
#if !IA64_ABI
/*
There is one implied argument for each virtual base class of the associated
base class, and they are used to ensure that each virtual base class is
constructed only once.
*/
#else /* IA64_ABI */
/*
There is an implied argument for the VTT.
*/
#endif /* IA64_ABI */
{
  an_expr_node_ptr implied_arg_node;
  a_type_ptr       class_type;
  a_constant       null_constant;
  a_boolean        needs_implied_arg_list;
#if !IA64_ABI
  a_type_ptr       subobject_type;
  a_base_class_ptr bcp;
#endif /* !IA64_ABI */

  *implied_arg_list = *end_implied_arg_list = NULL;
  /* Get the class type. */
  class_type = ctor_routine->source_corresp.parent.class_type;
  prelower_class_type(class_type);
  needs_implied_arg_list = ctor_needs_implied_arg_list(ctor_routine);
  if (needs_implied_arg_list) {
#if !IA64_ABI
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
#else /* IA64_ABI */
    /* Allocate an expression that is a NULL VTT pointer. */
    make_zero_of_proper_type(make_virtual_table_table_pointer_type(),
                             &null_constant);
    implied_arg_node = alloc_node_for_constant(&null_constant);
    *implied_arg_list = *end_implied_arg_list = implied_arg_node;
#endif /* IA64_ABI */
  }  /* if */
}  /* make_ctor_implied_arg_list */


/*
Return TRUE if the indicated destructor routine needs added implied arguments.
This must match make_dtor_implied_arg_list.
*/
#define dtor_needs_implied_arg_list(dtor_routine) TRUE


void make_dtor_implied_arg_list(a_routine_ptr    dtor_routine,
                                a_boolean        have_complete_object,
                                an_expr_node_ptr *implied_arg_list,
                                an_expr_node_ptr *end_implied_arg_list)
/*
Build and return the implied argument to be added to a call of the destructor
dtor_routine.  The beginning and end of the list are returned in
*implied_arg_list and *end_implied_arg_list.  For an empty list, both will be
set to NULL.  If have_complete_object is TRUE, we know we are calling the
destructor for a complete object.
*/
{
  a_type_ptr       class_type;
  an_expr_node_ptr implied_arg_node;
#if IA64_ABI
  a_constant null_constant;
#endif /* IA64_ABI */

  /* If you change this, see also dtor_needs_implied_arg_list, above. */
  *implied_arg_list = *end_implied_arg_list = NULL;
  /* Get the class type. */
  class_type = dtor_routine->source_corresp.parent.class_type;
  prelower_class_type(class_type);
  /* 0x2 bit means "have complete object".  0x1 bit means "free storage"
     which does not apply here. */
  implied_arg_node = node_for_integer_constant(have_complete_object ? 2L : 0L,
                                               (an_integer_kind)ik_int);
  *implied_arg_list = implied_arg_node;
#if IA64_ABI
  /* Add a NULL VTT argument. */
  make_zero_of_proper_type(make_virtual_table_table_pointer_type(),
                           &null_constant);
  implied_arg_node = alloc_node_for_constant(&null_constant);
  (*implied_arg_list)->next = implied_arg_node;
#endif /* IA64_ABI */
  *end_implied_arg_list = implied_arg_node;
}  /* make_dtor_implied_arg_list */


static a_boolean need_zeroing_for_value_initialization(a_dynamic_init_ptr dip)
/*
Return TRUE if the dik_constructor initialization in the indicated
dynamic initialization is value-initialization that requires zeroing
of the storage before the constructor is called.
*/
{
  a_boolean     need_zeroing = FALSE;
  a_routine_ptr ctor_routine = dip->variant.constructor.ptr;

  /* Zeroing is required if the initialization is value-initialization
     for a class that has no user-written constructor, and the class
     has data members that require zero initialization. */
  if (dip->variant.constructor.value_initialization &&
      ctor_routine->compiler_generated &&
      ctor_routine->source_corresp.parent.class_type->
                          variant.class_struct_union.has_zero_init_component) {
    need_zeroing = TRUE;
  }  /* if */
  return need_zeroing;
}  /* need_zeroing_for_value_initialization */


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

#if CHECKING
  if (dip->kind != (a_dynamic_init_kind)dik_constructor) {
    internal_error("add_constructor_call: bad kind");
  }  /* if */
#endif /* CHECKING */
  if (need_zeroing_for_value_initialization(dip)) {
    /* To do value-initialization on a class without a user-written
       constructor, zero the object and then call the default constructor. */
    an_expr_node_ptr entity_node_copy =
                                make_reusable_copy(entity_node,
                                                   /*vars_can_change=*/FALSE);
    a_type_ptr       class_type =
                                ctor_routine->source_corresp.parent.class_type;
#if IA64_ABI
    if (contains_ptr_to_data_member(class_type)) {
      /* Pointers to data members must be initialized to -1. */
      insert_call_to_helper_routine_to_zero_entity(
                  class_type,
                  entity_node,
                  node_for_integer_constant(1L, targ_size_t_int_kind),
                  insert_location);
    } else 
#endif /* IA64_ABI */
    /* Do not insert code here. */
    {
      a_targ_size_t    class_size;
      an_expr_node_ptr entity_size_node;

#if IA64_ABI
      if (ctor_routine->ctor_dtor_kind == (a_ctor_or_dtor_kind)cdk_subobject) {
        class_size = class_type->variant.class_struct_union.extra_info->
                                            size_without_virtual_base_classes;
      } else
#endif /* IA64_ABI */
      /* Do not insert code here. */
      {
        class_size = class_type->size;
      }  /* if */
      entity_size_node = node_for_host_large_integer(
                                              (a_host_large_integer)class_size,
                                              targ_size_t_int_kind);
      insert_call_to_zero_entity(entity_node, entity_size_node,
                                 insert_location);
    }  /* if */
    entity_node = entity_node_copy;
  }  /* if */
#if IA64_ABI
  /* If no entry point has been specified yet, use the complete object 
     entry point. */
  if (ctor_routine->ctor_dtor_kind == (a_ctor_or_dtor_kind)cdk_none) {
    ctor_routine = alternate_entry_point(ctor_routine,
                                         (a_ctor_or_dtor_kind)cdk_complete,
                                         /*define_now=*/FALSE);
  }  /* if */
#else /* !IA64_ABI */
  /* If no implied_arg_list is supplied and the constructor needs one
     (because it initializes a class that has virtual base classes), make
     the implied_arg_list (all entries are NULL pointer values). */
  if (implied_arg_list == NULL) {
    make_ctor_implied_arg_list(ctor_routine, &implied_arg_list,
                               &end_implied_arg_list);
  }  /* if */
#endif /* IA64_ABI */
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
  /* Make and insert an expression statement containing the call expression. */
  make_call_statement(ctor_routine, entity_node, insert_location);
}  /* add_constructor_call */


/*
Pointers to routine entries for runtime routines for call of a constructor,
copy constructor, or destructor for each element of an array, once created.
NULL until then.
*/
static a_routine_ptr
		vec_new_routine,
#if !IA64_ABI
		vec_new_eh_routine,
		vec_new_eh_zero_routine,
		array_new_routine,
		array_new_zero_routine,
		placement_array_new_routine,
		placement_array_new_zero_routine,
#else /* IA64_ABI */
		vec_new2_routine,
		vec_new3_routine,
		vec_ctor_routine,
#endif /* IA64_ABI */
		vec_cctor_routine,
#if !IA64_ABI
		vec_cctor_eh_routine,
#endif /* !IA64_ABI */
		vec_delete_routine,
#if !IA64_ABI
		array_delete_routine
#else /* IA64_ABI */
		vec_delete2_routine,
		vec_delete3_routine,
		vec_dtor_routine
#endif /* IA64_ABI */
		;


static an_expr_node_ptr num_elem_node_from_count(
                                          a_targ_ptrdiff_t array_element_count)
/*
Build an expression for a constant that represents the number of elements
in an array for an array new/delete call.  -1 indicates a variable-length
array.
*/
{
  an_expr_node_ptr num_elem_node;
  a_constant       num_elem_constant;

  set_integer_constant_with_overflow_check(
                 &num_elem_constant, (a_host_large_integer)array_element_count,
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
  size_elem_node = node_for_host_large_integer(
                  (a_host_large_integer)elem_type->size, targ_size_t_int_kind);
  return size_elem_node;
}  /* size_elem_node_from_pointer_type */


static an_expr_node_ptr expr_for_pointer_to_routine(a_routine_ptr routine)
/*
Build and return an expression node for the address of the indicated routine.
If the routine pointer is NULL, build an expression that is a null function
pointer and return that.  In either case, the node is cast to a generic
function pointer type.
*/
{
  an_expr_node_ptr expr;
  a_type_ptr       gen_func_ptr_type = make_vptp_type();
  a_constant       null_constant;

  if (routine != NULL) {
    expr = function_addr_expr(routine, /*set_address_taken_flag=*/TRUE);
    /* Cast the function pointer to the generic function type. */
    expr = add_cast_if_necessary(expr, gen_func_ptr_type);
  } else {
    /* No routine; use 0 cast to the right function pointer type. */
    make_zero_of_proper_type(gen_func_ptr_type, &null_constant);
    expr = alloc_node_for_constant(&null_constant);
  }  /* if */
  return expr;
}  /* expr_for_pointer_to_routine */

#if IA64_ABI

static an_expr_node_ptr get_array_new_padding(a_type_ptr    type,
                                              a_routine_ptr new_routine,
                                              a_boolean     even_if_zero)
/*
Return the amount of extra padding required for a dynamically
allocated array whose elements are of the indicated type.  If no padding
is required, return NULL (instead of an expression for zero) unless
even_if_zero is TRUE.  If new_routine is non-NULL, it is the placement new
routine that is being called to allocate the memory.  This is used
for the IA-64 ABI (see "Array operator new cookies", section 2.7).
*/
{
  a_type_ptr                    size_type;
  a_targ_size_t                 padding_size = 0;
  a_boolean                     need_padding = TRUE;
  an_expr_node_ptr              padding_node = NULL;

  /* Check to see if this type needs padding. */
  if (is_array_type(type)) {
    type = underlying_array_element_type(type);
  }  /* if */
  if (!new_or_delete_type_requires_array_handling(
                                               type,
                                               /*check_constructor=*/FALSE)) {
    need_padding = FALSE;
  } else if (new_routine != NULL) {
    /* No padding is required for a call to "::operator new[](size_t, 
       void *)". */
    a_type_ptr       rout_type;
    a_param_type_ptr param;
    rout_type = skip_typerefs(new_routine->type);
    param = unlowered_param_type_list(rout_type);
    if (!new_routine->source_corresp.is_class_member &&
        new_routine->source_corresp.parent.namespace_ptr == NULL &&
        param->next != NULL && param->next->next == NULL && 
        is_void_star_type(param->next->type)) {
      need_padding = FALSE;
    }  /* if */
  }  /* if */
  if (need_padding) {
    /* The amount of padding is equal to the maximum of the size of size_t and
       the alignment of an element in the array.  */
    padding_size = type->alignment;
    size_type = integer_type((an_integer_kind)targ_size_t_int_kind);
    if (size_type->size > padding_size) {
      padding_size = size_type->size;
    }  /* if */
  } /* if */
  if (padding_size != 0 || even_if_zero) {
    /* Make the expression. */
    padding_node = node_for_integer_constant(padding_size, 
                                             targ_size_t_int_kind);
  }  /* if */
  return padding_node;
}  /* get_array_new_padding */

#endif /* IA64_ABI */

#if IA64_ABI
/*ARGSUSED*/ /* <-- zero_storage is not used in that case. */
#endif /* IA64_ABI */
static an_expr_node_ptr make_vec_new_call(an_expr_node_ptr entity_node,
                                          a_type_ptr       entity_type,
                                          an_expr_node_ptr num_elem_node,
                                          a_routine_ptr    ctor_routine,
                                          a_routine_ptr    dtor_routine,
                                          a_routine_ptr    new_routine,
                                          a_routine_ptr    delete_routine,
                                          a_boolean        zero_storage)
/*
Make a call to a runtime routine (__vec_new or __array_new) that will
allocate an array and call a constructor for each element of the
array.  A pointer to the expression created is returned.  entity_node
gives the address of the array (for cases where the array is already
allocated).  entity_node == NULL if the runtime routine is supposed
to do the allocation.  entity_type gives the type of the pointer to
the entity.  num_elem_node gives (as an expression) the number of
elements in the array.  ctor_routine is the constructor routine to be
called, or NULL if no constructor is to be called.  dtor_routine is
the destructor routine to be called -- this is non-NULL only if there
is a destructor and if exceptions are enabled (in that case, it may
be necessary to destroy array elements that were created if a
throw occurs halfway through the initialization of the array);
the runtime routine __vec_new_eh is called in that case.  If
new_routine is non-NULL, it points to an "operator new[]" routine to
be used to do the allocation; if it is null, the default routine is
used.  If delete_routine is non-NULL, it points to an "operator
delete[]" routine to be used to free the storage if an exception is
thrown before initialization is completed; if it is NULL, the default
routine is used.  zero_storage is TRUE if the storage should be
zeroed before the constructor is called, for value-initialization.
The runtime routine __array_new is called for cases that require a
special new or delete routine.  This routine is not used for
placement new cases.  The routines called are different for the
IA-64 ABI; see comments below.
*/
{
  an_expr_node_ptr call_node, arg_expr_list, size_elem_node;
#if IA64_ABI
  an_expr_node_ptr padding_size_node = NULL;
#endif /* IA64_ABI */
  an_expr_node_ptr ctor_addr_node, dtor_addr_node;
  an_expr_node_ptr new_addr_node, delete_addr_node;
#if !IA64_ABI
  an_expr_node_ptr is_two_arg_node;
  a_constant       null_constant;
#endif /* !IA64_ABI */

  /* Build a constant node for the size of the array elements. */
  size_elem_node = size_elem_node_from_pointer_type(entity_type);
#if IA64_ABI
  if (entity_node == NULL || new_routine != NULL || 
      delete_routine != NULL) {
    /* Build an expression node for the size of the "cookie" that
       precedes the array allocation. */
    padding_size_node = get_array_new_padding(type_pointed_to(entity_type),
                                              new_routine,
                                              /*even_if_zero=*/TRUE);
    size_elem_node->next = padding_size_node;
  }  /* if */
#endif /* IA64_ABI */
  /* Build an expression for the address of the constructor. */
  ctor_addr_node = expr_for_pointer_to_routine(ctor_routine);
  if (new_routine == NULL && delete_routine == NULL) {
    /* Normal case.  The call looks like
         __vec_new   (entity_node, num_elems, size_elem, ctor_routine)
         __vec_new_eh(entity_node, num_elems, size_elem, ctor_routine,
                                                         dtor_routine)
         __vec_new_eh_zero
                     (entity_node, num_elems, size_elem, ctor_routine,
                                                         dtor_routine)
       The "_zero" version zeroes the storage before calling the constructor,
       for value-initialization cases.
    */
    /* For the IA-64 ABI, the call looks like
         __cxa_vec_ctor(entity_node, num_elems, size_elem, ctor_routine,
                                                           dtor_routine);
         __cxa_vec_new (num_elems, size_elem, padding, ctor_routine,
                                                       dtor_routine);
    */
#if !IA64_ABI
    if (entity_node == NULL) {
      /* If the runtime routine is supposed to do the allocation, pass a
         null pointer to the routine. */
      make_zero_of_proper_type(void_star_type(), &null_constant);
      entity_node = alloc_node_for_constant(&null_constant);
    }  /* if */
#else /* IA64_ABI */
    if (entity_node != NULL)
#endif /* IA64_ABI */
    {
      arg_expr_list = entity_node;
      entity_node->next = num_elem_node;
    }
#if IA64_ABI
    else {
      arg_expr_list = num_elem_node;
    }  /* if */
#endif /* IA64_ABI */
    num_elem_node->next = size_elem_node;
#if !IA64_ABI
    size_elem_node->next = ctor_addr_node;
#else /* IA64_ABI */
    if (padding_size_node != NULL) {
      padding_size_node->next = ctor_addr_node;
    } else {
      size_elem_node->next = ctor_addr_node;
    }  /* if */
#endif /* IA64_ABI */
#if !IA64_ABI
    if (zero_storage) {
      /* __vec_new_eh_zero call, which zeroes storage before calling the
         constructor, for value-initialization. */
      dtor_addr_node = expr_for_pointer_to_routine(dtor_routine);
      ctor_addr_node->next = dtor_addr_node;
      call_node = make_runtime_rout_call("__vec_new_eh_zero",
                                         &vec_new_eh_zero_routine,
                                         void_star_type(), arg_expr_list);
    } else if (exceptions_enabled && dtor_routine != NULL) {
      /* __vec_new_eh call, with destructor. */
      dtor_addr_node = expr_for_pointer_to_routine(dtor_routine);
      ctor_addr_node->next = dtor_addr_node;
      call_node = make_runtime_rout_call("__vec_new_eh", &vec_new_eh_routine,
                                         void_star_type(), arg_expr_list);
    } else {
      /* __vec_new call, without destructor. */
      call_node = make_runtime_rout_call("__vec_new", &vec_new_routine,
                                         void_star_type(), arg_expr_list);
    }  /* if */
#else /* IA64_ABI */
    dtor_addr_node = expr_for_pointer_to_routine(dtor_routine);
    ctor_addr_node->next = dtor_addr_node;
    if (entity_node != NULL) {
      call_node = make_runtime_rout_call("__cxa_vec_ctor", &vec_ctor_routine,
                                         void_star_type(), arg_expr_list);
    } else {
      call_node = make_runtime_rout_call("__cxa_vec_new", &vec_new_routine,
                                         void_star_type(), arg_expr_list);
    }  /* if */
#endif /* !IA64_ABI */
  } else {
    a_boolean is_two_arg_delete;
    /* A special new or delete routine must be used.  The call looks like
         __array_new(num_elems, size_elem, ctor_routine,
                     dtor_routine, new_routine, delete_routine, is_two_arg)
         __array_new_zero
                    (num_elems, size_elem, ctor_routine,
                     dtor_routine, new_routine, delete_routine, is_two_arg)
       The dtor_routine and delete_routine are always NULL when exceptions
       are disabled.  is_two_arg is 1 if the delete routine has two arguments
       and 0 otherwise.  The "_zero" version zeroes the storage before
       calling the constructor, for value-initialization cases. */
    /* The IA-64 ABI calls are
         __cxa_vec_new2(num_elems, size_elem, padding, ctor_routine,
                        dtor_routine, new_routine, delete_routine)
         __cxa_vec_new3(num_elems, size_elem, padding, ctor_routine,
                        dtor_routine, new_routine, delete_routine)
       The latter is for the two-argument delete case. */
    check_assertion(entity_node == NULL);
    dtor_addr_node = expr_for_pointer_to_routine(dtor_routine);
    new_addr_node = expr_for_pointer_to_routine(new_routine);
    delete_addr_node = expr_for_pointer_to_routine(delete_routine);
    is_two_arg_delete = (delete_routine != NULL &&
                         is_two_argument_delete(delete_routine));
#if !IA64_ABI
    is_two_arg_node = node_for_integer_constant((long)is_two_arg_delete,
                                                (an_integer_kind)ik_int);
#endif /* !IA64_ABI */
    arg_expr_list = num_elem_node;
    num_elem_node->next = size_elem_node;
#if !IA64_ABI
    size_elem_node->next = ctor_addr_node;
#else /* IA64_ABI */
    check_assertion(padding_size_node != NULL);
    padding_size_node->next = ctor_addr_node;
#endif /* IA64_ABI */
    ctor_addr_node->next = dtor_addr_node;
    dtor_addr_node->next = new_addr_node;
    new_addr_node->next = delete_addr_node;
#if !IA64_ABI
    delete_addr_node->next = is_two_arg_node;
    if (!zero_storage) {
      call_node = make_runtime_rout_call("__array_new", &array_new_routine,
                                         void_star_type(), arg_expr_list);
    } else {
      call_node = make_runtime_rout_call("__array_new_zero",
                                         &array_new_zero_routine,
                                         void_star_type(), arg_expr_list);
    }  /* if */
#else /* IA64_ABI */
    if (is_two_arg_delete) {
      call_node = make_runtime_rout_call("__cxa_vec_new3", &vec_new3_routine,
                                         void_star_type(), arg_expr_list);
    } else {
      call_node = make_runtime_rout_call("__cxa_vec_new2", &vec_new2_routine,
                                         void_star_type(), arg_expr_list);
    }  /* if */
#endif /* IA64_ABI */
  }  /* if */
  return call_node;
}  /* make_vec_new_call */

#if ABI_CHANGES_FOR_PLACEMENT_DELETE

#if !IA64_ABI
/*ARGSUSED*/ /* <-- prefix_size_node is not used in that case. */
#else /* IA64_ABI */
/*ARGSUSED*/ /* <-- zero_storage is not used in that case. */
#endif /* IA64_ABI */
static an_expr_node_ptr make_placement_array_new_call(
                                          an_expr_node_ptr entity_node,
                                          a_type_ptr       entity_type,
                                          an_expr_node_ptr num_elem_node,
                                          an_expr_node_ptr prefix_size_node,
                                          a_routine_ptr    ctor_routine,
                                          a_routine_ptr    dtor_routine,
                                          a_routine_ptr    delete_routine,
                                          an_expr_node_ptr delete_args,
                                          a_boolean        zero_storage)
/*
Make a call to a runtime routine (__placement_array_new) that will record the
size of an array allocated via placement new and call a constructor for each
element of the array.  A pointer to the expression created is returned.
entity_node gives the address of the array.  entity_type gives the type of the
pointer to the entity.  num_elem_node gives (as an expression) the number of
elements in the array.  prefix_size_node gives (as an expression) the size of
the array prefix, or is NULL if there is no array prefix.  ctor_routine is the
constructor routine to be called, or NULL if no constructor is to be called.
dtor_routine is the destructor routine to be called -- this is non-NULL only
if there is a destructor and if exceptions are enabled (in that case, it may
be necessary to destroy array elements that were created if a throw occurs
halfway through the initialization of the array).  If delete_routine is
non-NULL, it points to an "operator delete[]" routine to be used to free the
storage if an exception is thrown before initialization is completed; if it is
NULL, the storage is not freed.  zero_storage is TRUE if the storage should be
zeroed before the constructor is called, for value-initialization.  For the
IA-64 ABI, the routines called are different.
*/
{
  an_expr_node_ptr call_node;
#if !IA64_ABI
  an_expr_node_ptr arg_expr_list, size_elem_node;
  an_expr_node_ptr ctor_addr_node, dtor_addr_node;

  /* The call looks like
         __placement_array_new(entity_node, num_elems, size_elem,
                               ctor_routine, dtor_routine)
  */
  /* Build a constant node for the size of the array elements. */
  size_elem_node = size_elem_node_from_pointer_type(entity_type);
  ctor_addr_node = expr_for_pointer_to_routine(ctor_routine);
  dtor_addr_node = expr_for_pointer_to_routine(dtor_routine);
  arg_expr_list = entity_node;
  entity_node->next = num_elem_node;
  num_elem_node->next = size_elem_node;
  size_elem_node->next = ctor_addr_node;
  ctor_addr_node->next = dtor_addr_node;
  if (!zero_storage) {
    call_node = make_runtime_rout_call("__placement_array_new",
                                       &placement_array_new_routine,
                                       void_star_type(), arg_expr_list);
  } else {
    call_node = make_runtime_rout_call("__placement_array_new_zero",
                                       &placement_array_new_zero_routine,
                                       void_star_type(), arg_expr_list);
  }  /* if */
#else /* IA64_ABI */
  if (prefix_size_node != NULL) {
    an_expr_node_ptr cookie_ptr_node, cookie_value_node, assign_node;
    /* If there was padding, we must set the value indicating how many
       elements there are.  Compute the address of the "cookie". */
    cookie_ptr_node = entity_node;
    entity_node = make_reusable_copy(entity_node, /*vars_can_change=*/FALSE);
    cookie_ptr_node = add_cast_if_necessary(cookie_ptr_node,
                                            make_pointer_type
                                                    (prefix_size_node->type));
    cookie_ptr_node->next = node_for_integer_constant(1L,
                                                      targ_size_t_int_kind);
    cookie_ptr_node = make_operator_node((an_expr_operator_kind)eok_psubtract,
                                         cookie_ptr_node->type,
                                         cookie_ptr_node);
    /* Compute the value. */
    cookie_value_node = num_elem_node;
    num_elem_node = make_reusable_copy(num_elem_node, 
                                       /*vars_can_change=*/FALSE);
    /* Perform the assignment. */
    assign_node = make_assignment_expr(cookie_ptr_node, 
                                       (an_expr_operator_kind)eok_iassign,
                                       cookie_value_node);
    entity_node = make_comma_node(assign_node, entity_node);
  }  /* if */
  call_node = make_vec_new_call(entity_node, entity_type, num_elem_node, 
                                ctor_routine, dtor_routine,
                                (a_routine_ptr)NULL, (a_routine_ptr)NULL,
                                zero_storage);
#endif /* IA64_ABI */
  if (delete_routine != NULL) {
    /* A placement delete routine must be called.  The fact that the
       pointer is non-NULL means exceptions are enabled. */
    /* Wrap the call in an internal "try" block whose "catch" is a call
       of the placement delete routine. */
    an_expr_node_ptr entity_node_copy, delete_call, temp_value;

    /* Assign the call result to a temporary and make an expression to
       use it later. */
    temp_value = assign_expr_to_temp_and_make_expr_for_reuse(call_node);
    /* The first argument for the delete call is a pointer to the array. */
    entity_node_copy = make_reusable_copy(entity_node,
                                          /*vars_can_change=*/TRUE);
    /* Cast the argument to "void *", which is what the delete routine
       expects. */
    entity_node_copy = add_cast_if_necessary(entity_node_copy,
                                             void_star_type());
    entity_node_copy->next = delete_args;
    /* Make a call of the placement delete routine. */
    delete_call = make_call_node(delete_routine, entity_node_copy,
                                 /*honor_virtual=*/FALSE,
                                 (an_insert_location *)NULL);
    /* Wrap the expressions in an internal "try" block. */
    call_node = make_internal_try_expr(call_node, delete_call);
    /* Add a comma expression to get the value returned from the call as
       the value of the overall expression. */
    call_node = make_comma_node(call_node, temp_value);
  }  /* if */
  return call_node;
}  /* make_placement_array_new_call */

#endif /* ABI_CHANGES_FOR_PLACEMENT_DELETE */

static an_expr_node_ptr make_vec_delete_call(
                                          an_expr_node_ptr entity_node,
                                          a_targ_ptrdiff_t array_element_count,
                                          a_routine_ptr    dtor_routine,
                                          a_routine_ptr    delete_routine,
                                          a_boolean        free_storage)
/*
Make a call to a runtime routine (__vec_delete or __array_delete)
that will call a destructor for each element of an array and then
deallocate the array.  entity_node gives the address of the array.
array_element_count is the number of elements in the array, or -1 for
a variable-length array.  dtor_routine is the destructor routine to
be called, or NULL if no destructor is to be called.  delete_routine
is the delete routine to be called, or NULL if the normal delete
routine should be called.  free_storage is TRUE if the storage for
the array is to be freed.  A pointer to the expression created is
returned.  When delete_routine is non-zero, __array_delete is called
instead of __vec_delete.
*/
{
  an_expr_node_ptr call_node, arg_expr_list, num_elem_node, size_elem_node;
  an_expr_node_ptr dtor_addr_node, delete_addr_node;
#if !IA64_ABI
  an_expr_node_ptr is_two_arg_node, free_storage_node;
#else /* IA64_ABI */
  an_expr_node_ptr prefix_size_node;
#endif /* IA64_ABI */

#if IA64_ABI
  if (array_element_count != -1) {
#endif /* IA64_ABI */
    /* Build a constant node for the number of array elements. */
    num_elem_node = num_elem_node_from_count(array_element_count);
#if IA64_ABI
  } else {
    num_elem_node = NULL;
  }  /* if */
#endif /* IA64_ABI */
  /* Build a constant node for the size of the array elements. */
  size_elem_node = size_elem_node_from_pointer_type(entity_node->type);
  /* Build an expression for the address of the destructor. */
  dtor_addr_node = expr_for_pointer_to_routine(dtor_routine);
#if !IA64_ABI
  if (delete_routine == NULL) {
    /* The call looks like
         __vec_delete(entity_node, num_elems, size_elem, dtor_routine,
                      free_storage, 0)
       The final argument is never used.  It's there for cfront compatibility.
    */
    /* Build the "free_storage" argument: 1 to free storage, 0 otherwise. */
    free_storage_node = node_for_integer_constant(free_storage ? 1L : 0L,
                                                  (an_integer_kind)ik_int);
    arg_expr_list = entity_node;
    entity_node->next = num_elem_node;
    num_elem_node->next = size_elem_node;
    size_elem_node->next = dtor_addr_node;
    dtor_addr_node->next = free_storage_node;
    free_storage_node->next = node_for_integer_constant(0L,
                                                      (an_integer_kind)ik_int);
    call_node = make_runtime_rout_call("__vec_delete", &vec_delete_routine,
                                       void_type(), arg_expr_list);
  } else {
    /* There's a special delete routine, so use the call
       __array_delete(entity_node, num_elems, size_elem, dtor_routine,
                      delete_routine, is_two_arg)
       is_two_arg is 1 to indicate that the delete routine is a 2-argument
       routine, 0 otherwise.
    */
    delete_addr_node = expr_for_pointer_to_routine(delete_routine);
    is_two_arg_node = node_for_integer_constant(
                              is_two_argument_delete(delete_routine) ? 1L : 0L,
                              (an_integer_kind)ik_int);
    arg_expr_list = entity_node;
    entity_node->next = num_elem_node;
    num_elem_node->next = size_elem_node;
    size_elem_node->next = dtor_addr_node;
    dtor_addr_node->next = delete_addr_node;
    delete_addr_node->next = is_two_arg_node;
    call_node = make_runtime_rout_call("__array_delete", &array_delete_routine,
                                       void_type(), arg_expr_list);
  }  /* if */
#else /* IA64_ABI */
  arg_expr_list = entity_node;
  entity_node->next = size_elem_node;
  if (array_element_count != -1) {
    size_elem_node->next = dtor_addr_node;
  } else {
    prefix_size_node = get_array_new_padding(
                                           type_pointed_to(entity_node->type),
                                           (a_routine_ptr)NULL,
                                           /*even_if_zero=*/TRUE);
    size_elem_node->next = prefix_size_node;
    prefix_size_node->next = dtor_addr_node;
  }  /* if */
  if (delete_routine == NULL) {
    if (array_element_count != -1) {
      /* The call looks like
           __cxa_vec_dtor(entity_node, num_elems, size_elem, dtor_routine)
      */
      check_assertion(!free_storage);
      /* Splice in the node for the number of elements. */
      entity_node->next = num_elem_node;
      num_elem_node->next = size_elem_node;
      call_node = make_runtime_rout_call("__cxa_vec_dtor", &vec_dtor_routine,
                                         void_type(), arg_expr_list);
    } else {
      /* The call looks like
           __cxa_vec_delete(entity_node, size_elem, padding, dtor_routine)
         The runtime uses a cookie to determine the array size.
      */
      check_assertion(free_storage);
      call_node = make_runtime_rout_call("__cxa_vec_delete", 
                                         &vec_delete_routine, void_type(),
                                         arg_expr_list);
    }  /* if */
  } else {
    delete_addr_node = expr_for_pointer_to_routine(delete_routine);
    dtor_addr_node->next = delete_addr_node;
    check_assertion(array_element_count == -1 && free_storage);
    if (is_two_argument_delete(delete_routine)) {
      /* The call looks like
           __cxa_vec_delete3(entity_node, size_elem, padding, dtor_routine,
                             delete_routine)
         The runtime uses a cookie to determine the array size.  The
         delete routine is a two-argument version.
      */
      call_node = make_runtime_rout_call("__cxa_vec_delete3",
                                         &vec_delete3_routine, void_type(),
                                         arg_expr_list);
    } else {
      /* The call looks like
           __cxa_vec_delete2(entity_node, size_elem, padding, dtor_routine,
                             delete_routine)
         The runtime uses a cookie to determine the array size.
      */
      call_node = make_runtime_rout_call("__cxa_vec_delete2",
                                         &vec_delete2_routine, void_type(),
                                         arg_expr_list);
    }  /* if */
  } /* if */
#endif /* IA64_ABI */
  return call_node;
}  /* make_vec_delete_call */


static an_expr_node_ptr make_vec_cctor_call(
                                          an_expr_node_ptr entity_node,
                                          an_expr_node_ptr source_node,
                                          a_targ_ptrdiff_t array_element_count,
                                          a_routine_ptr    cctor_routine,
                                          a_routine_ptr    dtor_routine)
/*
Make a call to a runtime routine (__vec_cctor) that will call a copy
constructor for each element of an array.  entity_node gives the address
of the array.  source_node gives the source for the copy.
array_element_count is the number of elements in the array.
cctor_routine is the copy constructor routine to be called.
dtor_routine is the destructor to be called if an exception is thrown
during the operation, or NULL if there isn't one.  A pointer to the
expression created is returned.  For the IA-64 ABI the routine
called is different.
*/
{
  an_expr_node_ptr call_node, arg_expr_list, num_elem_node, size_elem_node;
  an_expr_node_ptr func_addr_node, dtor_addr_node;

  /* Build a constant node for the number of array elements. */
  num_elem_node = num_elem_node_from_count(array_element_count);
  /* Build a constant node for the size of the array elements. */
  size_elem_node = size_elem_node_from_pointer_type(entity_node->type);
  /* Build an expression for the address of the copy constructor. */
  func_addr_node = expr_for_pointer_to_routine(cctor_routine);
#if !IA64_ABI
  /* The call looks like
       __vec_cctor   (entity_node, num_elems, size_elem, cctor_routine,
                      source_node)
       __vec_cctor_eh(entity_node, num_elems, size_elem, cctor_routine,
                      source_node, dtor_routine)
  */
  arg_expr_list = entity_node;
  entity_node->next = num_elem_node;
  num_elem_node->next = size_elem_node;
  size_elem_node->next = func_addr_node;
  func_addr_node->next = source_node;
  if (exceptions_enabled && dtor_routine != NULL) {
    /* __vec_cctor_eh call, with destructor. */
    dtor_addr_node = expr_for_pointer_to_routine(dtor_routine);
    source_node->next = dtor_addr_node;
    call_node = make_runtime_rout_call("__vec_cctor_eh", &vec_cctor_eh_routine,
                                       void_type(), arg_expr_list);
  } else {
    /* __vec_cctor call, without destructor. */
    call_node = make_runtime_rout_call("__vec_cctor", &vec_cctor_routine,
                                       void_type(), arg_expr_list);
  }  /* if */
#else /* IA64_ABI */
  /* The call looks like
       __cxa_vec_cctor(entity_node, source_node, num_elems, size_elem,
                       cctor_routine, dtor_routine);
  */
  dtor_addr_node = expr_for_pointer_to_routine(dtor_routine);
  arg_expr_list = entity_node;
  entity_node->next = source_node;
  source_node->next = num_elem_node;
  num_elem_node->next = size_elem_node;
  size_elem_node->next = func_addr_node;
  func_addr_node->next = dtor_addr_node;
  call_node = make_runtime_rout_call("__cxa_vec_cctor", &vec_cctor_routine,
                                     void_type(), arg_expr_list);
#endif /* IA64_ABI */
  return call_node;
}  /* make_vec_cctor_call */


static void add_object_lifetime_to_function_scope(a_scope_ptr scope)
/*
Add an object lifetime to the indicated scope (a function scope) if it
doesn't already have one.
*/
{
  if (scope->lifetime == NULL) {
    an_object_lifetime_ptr saved_curr_object_lifetime = curr_object_lifetime;

    curr_object_lifetime = il_header.primary_scope->lifetime;
    push_object_lifetime(iek_scope, (char *)(scope),
                         (an_object_lifetime_kind)olk_block);
    curr_object_lifetime = saved_curr_object_lifetime;
  }  /* if */
}  /* add_object_lifetime_to_function_scope */


/*
Structure used by push_generated_routine_context/pop_generated_routine_context
to save/restore state information.
*/
typedef struct a_generated_routine_context {
  a_context	context;
  a_memory_region_number
		region_to_switch_back_to;
  a_scope_depth	depth_innermost_function_scope;
  a_scope_ptr	innermost_function_scope;
  a_byte_boolean
		processing_file_scope_init_routine;
  a_return_memo_ptr
		return_memo_list;
  a_local_static_variable_init_ptr
		promoted_local_static_variable_inits;
  an_eh_lowering_context
		ehcontext;
} a_generated_routine_context;


static void push_generated_routine_context(
                                     a_scope_ptr                 scope,
                                     a_memory_region_number      region_number,
                                     a_generated_routine_context *grcontext)
/*
IL lowering is fabricating a routine that didn't exist in the source program.
Push appropriate context for the generation.  scope is the function scope
for the routine; region number is the memory region number for the routine.
grcontext is a local variable used to save state for later restoration.
*/
{
  grcontext->region_to_switch_back_to = curr_il_region_number;
  switch_il_region(region_number);
  /* depth_innermost_function_scope is reset, in particular, so that
     alloc_object_lifetime will not attempt to maintain an available list
     for object lifetimes in this function (there is no scope stack entry). */
  grcontext->depth_innermost_function_scope = depth_innermost_function_scope;
  depth_innermost_function_scope = NO_SCOPE_DEPTH;
  grcontext->innermost_function_scope = innermost_function_scope;
  innermost_function_scope = scope;
  grcontext->processing_file_scope_init_routine =
                                            processing_file_scope_init_routine;
  processing_file_scope_init_routine = FALSE;
  grcontext->return_memo_list = return_memo_list;
  /* return_memo_list is cleared by function_lower_init. */
  grcontext->promoted_local_static_variable_inits = 
                                          promoted_local_static_variable_inits;
  promoted_local_static_variable_inits = NULL;
  save_eh_lowering_context(&grcontext->ehcontext);
  add_object_lifetime_to_function_scope(scope);
  push_context(&grcontext->context, scope, (an_object_lifetime_ptr)NULL);
  /* Initialize for lowering a function. */
  function_lower_init();
}  /* push_generated_routine_context */


static void pop_generated_routine_context(
                                     a_scope_ptr                 scope,
                                     a_memory_region_number      region_number,
                                     a_generated_routine_context *grcontext)
/*
Pop function corresponding to push_generated_routine_context.
*/
{
  a_routine_ptr rout = scope->variant.routine.ptr;

  /* If there is reason to promote the local types and static variables
     to the file scope, do that now and clear the lists.  That makes the
     promoted entities part of the file scope and no longer orphans. */
  promote_local_entities_to_file_scope(scope);
  pop_context();
  /* Restore and pop the lifetime attached to the scope so that it can be
     deleted if it is empty. */
  { an_object_lifetime_ptr saved_curr_object_lifetime = curr_object_lifetime;
    curr_object_lifetime = scope->lifetime;
    (void)pop_object_lifetime();
    curr_object_lifetime = saved_curr_object_lifetime;
  }
  clean_up_all_object_lifetimes(scope);
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  /* Make orphan lists for any local types or static variables in the
     routine. */
  add_scope_orphaned_il_lists(scope);
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
  scope->function_body_processing_finished = TRUE;
#if MAINTAIN_NEEDED_FLAGS
  /* Walk subtrees of local types and variables that have already been
     marked as needed. */
  walk_subtrees_of_local_entities(scope);
  /* If the routine is external (but not extern inline), mark it as needed. */
  if (rout->storage_class == (a_storage_class)sc_unspecified &&
      !rout->is_inline) {
    mark_as_needed((char *)rout, iek_routine);
  }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
  set_routine_defined(rout);
  restore_eh_lowering_context(&grcontext->ehcontext);
  promoted_local_static_variable_inits =
                               grcontext->promoted_local_static_variable_inits;
  free_return_memo_list(return_memo_list);
  return_memo_list = grcontext->return_memo_list;
  processing_file_scope_init_routine =
                                 grcontext->processing_file_scope_init_routine;
  innermost_function_scope = grcontext->innermost_function_scope;
  depth_innermost_function_scope = grcontext->depth_innermost_function_scope;
  check_for_done_with_memory_region(region_number);
  switch_il_region(grcontext->region_to_switch_back_to);
}  /* pop_generated_routine_context */


static void define_default_version_of_routine(
                                            a_routine_ptr    routine,
                                            a_routine_ptr    new_routine,
                                            an_expr_node_ptr default_arg_list)
/*
Define new_routine, which is an alternate entry point for routine, with fewer
arguments.  Replacements for trailing arguments in routine are given by the
default_arg_list.
*/
{
  an_expr_node_ptr implied_arg_list = NULL, end_implied_arg_list = NULL;
  an_expr_node_ptr call_node;
  a_scope_ptr      new_routine_scope;
  a_type_ptr       routine_type = skip_typerefs(routine->type);
  a_type_ptr       this_param_type;
  a_param_type_ptr src_param_type, param_type;
  a_routine_type_supplement_ptr
                   rtsp, new_rtsp;
  an_insert_location
                   insert_location;
  a_memory_region_number
                   new_routine_il_region;
  a_variable_ptr   this_param_var, param_var, last_param_var;
  an_expr_node_ptr this_arg, pass_through_arg;
  a_statement_ptr  return_stmt;
  a_generated_routine_context
                   grcontext;
  a_boolean        insert_as_statement, void_return;
  an_object_lifetime_ptr
                   init_expr_lifetime = NULL;
  a_context        def_arg_context;

  rtsp = routine->type->variant.routine.extra_info;
  new_rtsp = new_routine->type->variant.routine.extra_info;
  this_param_type = new_rtsp->param_type_list->type;
  /* Make a memory region, scope, and block for the routine definition. */
  new_routine_scope = make_routine_definition(new_routine,
                                              /*make_return=*/FALSE,
                                              &new_routine_il_region);
  set_block_start_insert_location(new_routine_scope->assoc_block,
                                  &insert_location);
  push_generated_routine_context(new_routine_scope, new_routine_il_region,
                                 &grcontext);
  /* Make a parameter variable for the "this" parameter (again, in lowered
     form as a normal parameter). */
  new_routine_scope->variant.routine.parameters = this_param_var =
                                make_lowered_param_variable(this_param_type);
  this_param_var->assoc_param_type = new_rtsp->param_type_list;
  this_param_var->is_this_parameter = TRUE;
  last_param_var = this_param_var;
#if IA64_ABI
  if (new_routine->ctor_dtor_kind == (a_ctor_or_dtor_kind)cdk_complete ||
      (new_routine->ctor_dtor_kind == (a_ctor_or_dtor_kind)cdk_subobject &&
       !new_rtsp->this_class->variant.class_struct_union.
                                                  any_virtual_base_classes)) {
#endif /* IA64_ABI */
    /* Make expression lists for constructor or destructor implied
       arguments. */
    if (routine->special_kind == (a_special_function_kind)sfk_constructor) {
      make_ctor_implied_arg_list(routine, &implied_arg_list,
                                 &end_implied_arg_list);
    } else if (routine->special_kind ==
                                  (a_special_function_kind)sfk_destructor) {
      make_dtor_implied_arg_list(routine, /*have_complete_object=*/TRUE,
                                 &implied_arg_list, &end_implied_arg_list);
    }  /* if */
#if IA64_ABI
  } else if (routine->special_kind == 
                                    (a_special_function_kind)sfk_destructor) {
    /* Add the argument that indicates whether virtual bases should be
       destroyed. */
    implied_arg_list = node_for_integer_constant(
                                ((new_routine->ctor_dtor_kind == 
                                 (a_ctor_or_dtor_kind)cdk_deleting) ? 3 : 0),
                                (an_integer_kind)ik_int);
    if (new_routine->ctor_dtor_kind == (a_ctor_or_dtor_kind)cdk_deleting) {
      /* Add a NULL VTT argument. */
      /* Allocate an expression that is a NULL VTT pointer. */
      a_constant null_constant;
      make_zero_of_proper_type(make_virtual_table_table_pointer_type(),
                               &null_constant);
      implied_arg_list->next = alloc_node_for_constant(&null_constant);
      end_implied_arg_list = implied_arg_list->next;
    }  else {
      end_implied_arg_list = implied_arg_list;
    }  /* if */
  }  /* if */
#endif /* IA64_ABI */
  /* Do not process parameters with default argument values, since they
     are removed from the routine's interface. */
  for (param_type = new_rtsp->param_type_list->next; 
       param_type != NULL;
       param_type = param_type->next) {
    param_var = make_lowered_param_variable(param_type->type);
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
    last_param_var = param_var;
  }  /* for */
  if (default_arg_list != NULL) {
    /* There are default arguments for the call, so they have to be
       copied and lowered. */
    /* Create an expression temporary lifetime surrounding the copy of
       the expressions to catch any needed destructions. */
    an_object_lifetime_ptr saved_curr_object_lifetime = curr_object_lifetime;
    src_param_type = rtsp->param_type_list->next;
    /* Find the original parameter type that corresponds to this parameter. */
    while (src_param_type != NULL && !src_param_type->has_default_arg) {
      src_param_type = src_param_type->next;
    }  /* while */
    push_object_lifetime(iek_none, (char *)NULL,
                         (an_object_lifetime_kind)olk_expr_temporary);
    init_expr_lifetime = curr_object_lifetime;
    curr_object_lifetime = saved_curr_object_lifetime;
    /* Push a context for the lifetime.  */
    push_context(&def_arg_context, (a_scope_ptr)NULL, init_expr_lifetime);
    /* Copy the default argument expressions into the function memory
       region. */
    default_arg_list =
                      copy_list_of_expr_trees(default_arg_list,
                                              CE_UNLINK_SOURCE_DESTRUCTIONS);
    if (is_useless_object_lifetime(init_expr_lifetime)) {
      /* There weren't any temporaries in the default argument expressions,
         so the lifetime is not needed. */
      init_expr_lifetime = NULL;
      pop_context();
    } else {
      /* There were some destructible temporaries in the default
         argument expressions, so the lifetime is needed. */
      if (keep_object_lifetime_info_in_lowered_il) {
        /* The object lifetime is to be kept in the IL, so add a block
           statement and bind the lifetime to it. */
        a_statement_ptr block_stmt =
                               alloc_statement((a_statement_kind)stmk_block);
        insert_statement(block_stmt, &insert_location);
        set_block_start_insert_location(block_stmt, &insert_location);
        bind_object_lifetime(init_expr_lifetime,
                             (an_il_entry_kind)iek_block,
                             (char *)block_stmt->variant.block.extra_info);
      }  /* if */
      begin_object_lifetime(init_expr_lifetime, &insert_location);
    }  /* if */
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
  /* Make a call node that calls the original routine with all
     the implicit arguments, i.e., that passes all the extra arguments
     to the original routine. */
  call_node = make_call_node(routine, this_arg, /*honor_virtual=*/FALSE,
                             (an_insert_location *)NULL);
  /* If the routine has a void type, insert a statement for the call
     followed by a return statement.  Otherwise, attach the call directly
     to the return. */
  void_return = is_void_type(routine_type->variant.routine.return_type);
  insert_as_statement = void_return;
  /* If we might have to insert destructor calls, insert the call as
     a statement. */
  if (init_expr_lifetime != NULL) insert_as_statement = TRUE;
  if (insert_as_statement) {
    /* The call will be inserted as a separate statement. */
    a_variable_ptr temp_var;
    /* If the routine has a non-void return, put the value in a temporary
       and then return the temporary later. */
    if (!void_return) {
      temp_var = make_lowered_temporary(call_node->type);
      call_node = make_var_assignment_expr(temp_var,
                                           (an_expr_operator_kind)eok_last,
                                           call_node);
    }  /* if */
    /* Insert the call as a statement. */
    (void)insert_expr_statement(call_node, &insert_location);
    /* Set up the expression to be used in the return statement (the value
       of the temporary). */
    if (void_return) {
      call_node = NULL;
    } else {
      call_node = var_rvalue_expr(temp_var);
    }  /* if */
  }  /* if */
  if (init_expr_lifetime != NULL) {
    /* Generate the destructions. */
    gen_cleanup_actions(init_expr_lifetime, &insert_location);
    pop_context();
  }  /* if */
  /* Add the return statement. */
  return_stmt = alloc_statement((a_statement_kind)stmk_return);
  return_stmt->expr = call_node;
  insert_statement(return_stmt, &insert_location);
  add_to_return_memo_list(return_stmt);
  if (exceptions_enabled) {
    /* Add prologue/epilogue code for exceptions if needed. */
    add_eh_function_prologue(new_routine_scope);
  }  /* if */
  pop_generated_routine_context(new_routine_scope, new_routine_il_region,
                                &grcontext);
#if MINIMAL_INLINING
  if (new_routine->is_inline && inlining_enabled) {
    set_up_routine_for_inlining(new_routine_scope);
  }  /* if */
#endif /* MINIMAL_INLINING */
}  /* define_default_version_of_routine */


static void copy_and_lower_param_type_list(a_type_ptr       routine_type,
                                           a_param_type_ptr last_param_type,
                                           a_boolean        do_default_args,
                                           a_boolean        do_lowering)
/*
Copy the parameter type entries given by the unlowered parameter list of the
routine_type, adding them to the list of which last_param_type is presently
the end.  Add indirections to parameters with copy constructors as the types
are processed.  If do_default_args is FALSE, parameter types corresponding to
default arguments are not copied.  If do_lowering is TRUE, the routine_type is
modified; if not, only the new parameters are modified.
*/
{
  a_type_ptr       pass_through_param_type;
  a_param_type_ptr src_param_type, param_type;

  for (src_param_type = unlowered_param_type_list(routine_type); 
       src_param_type != NULL && 
         (do_default_args || !src_param_type->has_default_arg);
       src_param_type = src_param_type->next) {
    /* If the parameter is passed via a copy constructor and it has
       not been lowered, replace it by a pointer to the object.
       Note that a second copy constructor call (i.e., one within
       the generated routine) is not necessary. */
    if (src_param_type->passed_via_copy_constructor &&
        !visited_yet(src_param_type)) {
      if (do_lowering) {
        add_indirection_to_cctor_param_type(src_param_type);
        pass_through_param_type = src_param_type->type;
      } else {
        pass_through_param_type = 
                 type_of_cctor_param_after_adding_indirection(src_param_type);
      }  /* if */
    } else {
      pass_through_param_type = src_param_type->type;
    }  /* if */
    param_type = alloc_param_type(pass_through_param_type);
    param_type->has_default_arg = src_param_type->has_default_arg;
    /* It is not necessary to clear il_lowering_flag; the entry does not need
       to be lowered.  Also note that the parameter types will be lowered
       when the original function is lowered, and do not need to be
       lowered here. */
    last_param_type->next = param_type;
    last_param_type = param_type;
  }  /* for */
}  /* copy_and_lower_param_type_list */


static a_routine_ptr default_version_of_routine(
                                         a_routine_ptr       routine,
                                         an_expr_node_ptr    default_arg_list)
/*
Return a pointer to a routine that does the same thing as "routine" but in
which the parameters that have default argument expressions have been removed.
The values to be used for those default arguments are given by
default_arg_list (the expressions are NOT already lowered; this is important,
since they have to be copied, and you can't successfully copy a lowered
expression, since it might have temporaries in it).  Implicitly-generated
parameters of constructors and destructors are also removed.  This information
is used to generate a version of a constructor or destructor that can be
called with just a "this" parameter, or of a copy constructor that can be
called with just a "this" parameter and a source pointer.  The routine must
have a "this" parameter.  If the original routine has no default arguments, no
wrapper routine is created; the original routine is returned.
*/
{
  a_routine_ptr    new_routine;
  a_type_ptr       routine_type = skip_typerefs(routine->type);
  a_type_ptr       this_param_type;
#if CHECKING
  a_routine_type_supplement_ptr
                   rtsp;
#endif /* CHECKING */
  a_routine_type_supplement_ptr 
                   new_rtsp;
  a_boolean        any_implied_args;

  /* Determine if any implicit arguments are required for a constructor or
     destructor. */
  any_implied_args = FALSE;
  if (routine->special_kind == (a_special_function_kind)sfk_constructor) {
    any_implied_args = ctor_needs_implied_arg_list(routine);
  } else if (routine->special_kind ==
                                     (a_special_function_kind)sfk_destructor) {
    any_implied_args = dtor_needs_implied_arg_list(routine);
  }  /* if */
  if (default_arg_list != NULL || any_implied_args) {
    /* There are some implicit or default arguments, so a wrapper routine
       must be created and used in place of the original routine. */
    /* Make a type and routine entry for the routine. */
    /* Note that the routine has no name. */
    /* The "this" parameter is generated in its lowered form (i.e., as a
       normal parameter). */
    this_param_type = implicit_this_param_type_of(routine_type);
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
#if CHECKING
    rtsp = routine_type->variant.routine.extra_info;
    /* The routine is not allowed to be one that returns its value via
       a pointer provided by the caller (the extra code for that case
       is not implemented). */
    if (rtsp->value_returned_by_cctor) {
      internal_error("default_version_of_routine: return value ptr");
    }  /* if */
#endif /* CHECKING */
    /* Make any additional parameter types and parameter vars beyond the
       "this" parameter (this comes up, for instance, on the copy
       constructor case).  Do not process parameters with default argument
       values, since they are removed from the routine's interface. */
    new_rtsp = new_routine->type->variant.routine.extra_info;
    copy_and_lower_param_type_list(routine_type, new_rtsp->param_type_list, 
                                   /*do_default_args=*/FALSE,
                                   /*do_lowering=*/TRUE);
    define_default_version_of_routine(routine, new_routine, 
                                      default_arg_list);
    routine = new_routine;
  }  /* if */
  return routine;
}  /* default_version_of_routine */

#if IA64_ABI

a_routine_ptr alternate_entry_point(a_routine_ptr       routine,
                                    a_ctor_or_dtor_kind kind,
                                    a_boolean           define_now)
/*
Return a pointer to the alternate entry point for "routine" indicated by
"kind".  If the alternate entry point does not already exist, it is created.
If define_now is TRUE, the routine is defined if appropriate.  This
is used to create alternate entry points for constructors and
destructors in the IA-64 ABI.
*/
{
  a_routine_ptr    new_routine = NULL;
  a_routine_list_entry_ptr 
                   rlep;

  check_assertion(routine->special_kind ==
                                  (a_special_function_kind)sfk_constructor ||
                  routine->special_kind ==
                                  (a_special_function_kind)sfk_destructor);
  /* Check to see if the routine already exists on the alternate_entry_points
     list. */
  for (rlep = routine->variant.ctor_dtor.alternate_entry_points;
       rlep != NULL; 
       rlep = rlep->next) {
    if (rlep->routine->ctor_dtor_kind == kind) {
      /* The routine already exists. */
      new_routine = rlep->routine;
      break;
    }  /* if */
  }  /* for */
  if (new_routine == NULL) {
    a_type_ptr                    routine_type = skip_typerefs(routine->type);
    a_type_ptr                    this_param_type;
    a_param_type_ptr              param_type, last_param_type;
    a_routine_type_supplement_ptr rtsp, new_rtsp;
    a_storage_class               new_storage_class;
    rtsp = routine->type->variant.routine.extra_info;
    /* Make a type and routine entry for the routine. */
    /* The "this" parameter is generated in its lowered form (i.e., as a
       normal parameter). */
    this_param_type = implicit_this_param_type_of(routine_type);
    /* Additional parameter types, if any, are added below. */
    new_storage_class = routine->storage_class;
    if (new_storage_class == (a_storage_class)sc_unspecified) {
      new_storage_class = (a_storage_class)sc_extern;
    }  /* if */
    /* The routine is not added to the routines list now; see
       promote_routines.  Check that the routine has not already
       been promoted out of its class, to make sure we will get
       to promote_routines later. */
    check_assertion(routine->source_corresp.is_class_member);
    new_routine = make_rout_entry_no_add(
                                  (char *)NULL, new_storage_class,
                                  routine_type->variant.routine.return_type,
                                  this_param_type);
    new_routine->is_inline = routine->is_inline;
#if INSTANTIATE_EXTERN_INLINE
    new_routine->inline_instance_required = routine->inline_instance_required;
#endif /* INSTANTIATE_EXTERN_INLINE */
    new_routine->source_corresp.is_class_member = TRUE;
    new_routine->source_corresp.parent.class_type =
                                     routine->source_corresp.parent.class_type;
    set_routine_special_kind(new_routine, routine->special_kind);
    new_routine->ctor_dtor_kind = kind;
    new_routine->compiler_generated = TRUE;
    new_routine->pure_virtual = routine->pure_virtual;
#if ONE_INSTANTIATION_PER_OBJECT
    new_routine->instantiation_needed_bit_number =
                                      routine->instantiation_needed_bit_number;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
    new_rtsp = new_routine->type->variant.routine.extra_info;
    new_rtsp->this_class = rtsp->this_class;
    mangle_alternate_entry_point_name(new_routine, routine);
    /* Make the new routine virtual if the old one is so that virtual
       destructors work correctly.  The virtual function number for the
       deleting destructor is one greater than for the complete object
       destructor. */
    if (routine->is_virtual && kind != (a_ctor_or_dtor_kind)cdk_subobject) {
      check_assertion(routine->special_kind ==
                                     (a_special_function_kind)sfk_destructor);
      new_routine->is_virtual = TRUE;
      if (kind == (a_ctor_or_dtor_kind)cdk_complete) {
        new_routine->virtual_function_number = 
                                             routine->virtual_function_number;
      } else {
        check_assertion(kind == (a_ctor_or_dtor_kind)cdk_deleting);
        new_routine->virtual_function_number = 
                                         routine->virtual_function_number + 1;
      }  /* if */
    }  /* if */
    /* The new routine has an ellipsis if the old one does.  */
    new_rtsp->has_ellipsis = rtsp->has_ellipsis;
    /* Add new_routine to the list of alternate entry points for 
       routine. */
    rlep = alloc_list_entry_for_routine();
    rlep->routine = new_routine;
    rlep->next = routine->variant.ctor_dtor.alternate_entry_points;
    routine->variant.ctor_dtor.alternate_entry_points = rlep;
    last_param_type = new_rtsp->param_type_list;
    if (kind == (a_ctor_or_dtor_kind)cdk_subobject && 
        rtsp->this_class->
                       variant.class_struct_union.any_virtual_base_classes) {
      /* If this is a subobject constructor or destructor for a class with
         virtual bases, add a VTT parameter. */
      param_type = alloc_param_type(make_virtual_table_table_pointer_type());
      last_param_type->next = param_type;
      last_param_type = param_type;
    }  /* if */
    /* Copy the remainder of the parameters. */
    copy_and_lower_param_type_list(routine_type, last_param_type, 
                                   /*do_default_args=*/TRUE,
                                   /*do_lowering=*/FALSE);
  }  /* if */
  /* Define the routine if appropriate. */
  if (routine->assoc_scope != NULL_region_number &&
      !routine->suppress_inline_body &&
      define_now) {
    if (routine->storage_class == (a_storage_class)sc_extern) {
      routine->storage_class = (a_storage_class)sc_unspecified;
    }  /* if */
    define_default_version_of_routine(routine, new_routine, 
                                      (an_expr_node_ptr)NULL);
#if LOWER_EXTERN_INLINE
    if (routine->use_comdat) {
      put_routine_into_comdat_group(new_routine);
    }  /* if */
#endif /* LOWER_EXTERN_INLINE */
  }  /* if */
  return new_routine;
}  /* alternate_entry_point */


void create_alternate_entry_points(a_routine_ptr routine,
                                   a_boolean     define_now)
/*
Create all the alternate entry points for the indicated constructor or
destructor.  Give them definitions if define_now is TRUE and if the
primary routine has a definition.
*/
{
  check_assertion(routine->special_kind ==
                                    (a_special_function_kind)sfk_constructor ||
                  routine->special_kind ==
                                    (a_special_function_kind)sfk_destructor);
  (void)alternate_entry_point(routine, (a_ctor_or_dtor_kind)cdk_complete,
                              define_now);
  (void)alternate_entry_point(routine, (a_ctor_or_dtor_kind)cdk_subobject,
                              define_now);
  if (routine->special_kind == (a_special_function_kind)sfk_destructor &&
      /* The deleting destructor is used only when the destructor is
         virtual. */
      routine->is_virtual) {
    (void)alternate_entry_point(routine, 
                                (a_ctor_or_dtor_kind)cdk_deleting,
                                define_now);
  }  /* if */
}  /* create_alternate_entry_points */

#endif /* IA64_ABI */

static void add_array_constructor_call(
                                   a_dynamic_init_ptr     dip,
                                   an_expr_node_ptr       entity_node,
                                   an_expr_node_ptr       source_node,
                                   a_targ_ptrdiff_t       array_element_count,
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
  a_routine_ptr    ctor_routine, dtor_routine;
  an_expr_node_ptr call_node, num_elem_node;
  a_boolean        zero_storage;

#if CHECKING
  if (dip->kind != (a_dynamic_init_kind)dik_constructor) {
    internal_error("add_array_constructor_call: not dik_constructor");
  }  /* if */
#endif /* CHECKING */
  zero_storage = need_zeroing_for_value_initialization(dip);
  ctor_routine = dip->variant.constructor.ptr;
#if IA64_ABI
  ctor_routine = alternate_entry_point(ctor_routine,
                                       (a_ctor_or_dtor_kind)cdk_complete,
                                       /*define_now=*/FALSE);
#endif /* IA64_ABI */
  ctor_routine = default_version_of_routine(ctor_routine,
                                            dip->variant.constructor.args);
  dtor_routine = dip->destructor;
#if IA64_ABI
  if (dtor_routine != NULL) {
    dtor_routine = alternate_entry_point(dtor_routine,
                                         (a_ctor_or_dtor_kind)cdk_complete,
                                         /*define_now=*/FALSE);
  }  /* if */
#endif /* IA64_ABI */
  if (dip->init_expr_lifetime != NULL) {
    unbind_object_lifetime(dip->init_expr_lifetime);
  }  /* if */
  if (source_node != NULL) {
    /* Copy constructor case. */
    check_assertion(!dip->variant.constructor.value_initialization);
    call_node = make_vec_cctor_call(entity_node, source_node,
                                    array_element_count, ctor_routine,
                                    dtor_routine);
  } else {
    /* Normal constructor case. */
    /* Build a constant node for the number of array elements. */
    num_elem_node = num_elem_node_from_count(array_element_count);
    call_node = make_vec_new_call(entity_node, entity_node->type,
                                  num_elem_node,
                                  ctor_routine,
                                  exceptions_enabled ? dtor_routine :
                                                     (a_routine *)NULL,
                                  (a_routine *)NULL, (a_routine *)NULL,
                                  zero_storage);
  }  /* if */
  /* Make a statement containing the call and insert it at the right
     location. */
  (void)insert_expr_statement_set_pos(call_node, insert_location);
  if (dip->destructor != NULL) {
    /* Since the destruction is managed by the runtime routine, remove
       it from the cleanup list. */
    check_assertion(dip->destruction_is_for_partially_constructed_aggregate);
    remove_from_destruction_list(dip);
  }  /* if */
}  /* add_array_constructor_call */


#if !IA64_ABI
/*ARGSUSED*/ /* <-- vtt_addr_node is not used in that case. */
#endif /* !IA64_ABI */
static void add_destructor_call(a_routine_ptr          dtor_routine,
                                an_init_pos_descr_ptr  ipdp,
                                a_boolean              have_complete_object,
                                an_expr_node_ptr       vtt_addr_node,
                                an_insert_location_ptr insert_location)
/*
Make a call statement that invokes the destructor dtor_routine for
the entity whose position is given by ipdp.  If the entity is a whole array,
destroy all the elements.  have_complete_object is TRUE if the
entity is a complete object.  vtt_addr_node is an expression for the virtual
table table address that should be passed to the base class destructor, or
NULL if none.  Insert the statement at *insert_location and update
*insert_location.  The call generated is not a virtual call even if the
destructor is virtual.  Note: this routine takes separate dtor_routine and
ipdp parameters instead of a dynamic init pointer because of the
make_destruction_routine case.
*/
{
  an_expr_node_ptr entity_node, call_node;
  an_expr_node_ptr implied_arg_list;
  a_type_ptr       this_param_type;
  a_type_ptr       entity_type = type_from_init_pos_descr(ipdp);
  a_boolean        is_array = FALSE;
  a_targ_ptrdiff_t array_element_count;

  /* Make an expression for the object to be destroyed. */
  entity_node = make_init_entity_node(ipdp, /*using_as_address=*/TRUE,
                                      /*using_as_dest=*/FALSE);
  /* See if the entity is an array or a sequence of elements of an array. */
  if (ipdp->array_element_sequence) {
    /* Destruction of a sequence of array elements. */
    is_array = TRUE;
    array_element_count = ipdp->array_element_count;
  } else if (is_array_type(entity_type)) {
    /* Destruction of whole array. */
    is_array = TRUE;
    array_element_count = num_array_elements(entity_type);
  }  /* if */
#if IA64_ABI
  if (dtor_routine != NULL) {
    dtor_routine = alternate_entry_point(dtor_routine, 
                                         (have_complete_object ? 
                                          (a_ctor_or_dtor_kind)cdk_complete :
                                          (a_ctor_or_dtor_kind)cdk_subobject),
                                         /*define_now=*/FALSE);
  }  /* if */
#endif /* !IA64_ABI */
  /* Generate code for the destructor call. */
  if (is_array) {
#if !IA64_ABI
    /* default_version_of_routine is not called on purpose; __vec_delete
       knows about the implicit argument for destructors and generates
       it automatically. */
#endif /* !IA64_ABI */
    /* Generate the __vec_delete call. */
    call_node = make_vec_delete_call(entity_node, array_element_count,
                                     dtor_routine, (a_routine *)NULL,
                                     /*free_storage=*/FALSE);
    /* Make a statement containing the call and insert it at the right
       location. */
    (void)insert_expr_statement_set_pos(call_node, insert_location);
  } else {
    /* Destruction of simple entity (non-array). */
    /* Cast the entity node pointer to the right type.  It might be a pointer
       to the class type-as-subobject. */
    this_param_type = implicit_this_param_type_of(dtor_routine->type);
    entity_node = add_cast_if_necessary(entity_node,
                                        f_skip_typerefs(this_param_type));
#if IA64_ABI
    if (entity_type->variant.class_struct_union.any_virtual_base_classes &&
        !have_complete_object) {
      if (vtt_addr_node == NULL) {
        /* Add a NULL VTT argument. */
        a_constant null_constant;
        make_zero_of_proper_type(make_virtual_table_table_pointer_type(),
                                 &null_constant);
        implied_arg_list = alloc_node_for_constant(&null_constant);
      } else {
        implied_arg_list = vtt_addr_node;
      }  /* if */
    } else {
      implied_arg_list = NULL;
    }  /* if */
#else /* !IA64_ABI */
    { an_expr_node_ptr end_implied_arg_list;
      /* If the destructor is for a class that has virtual base classes, add
         the implicit complete-object argument. */
      make_dtor_implied_arg_list(dtor_routine, have_complete_object,
                                 &implied_arg_list, &end_implied_arg_list);
    }
#endif /* !IA64_ABI */
    entity_node->next = implied_arg_list;
    /* Make and insert an expression statement containing the call
       expression. */
    make_call_statement(dtor_routine, entity_node, insert_location);
  }  /* if */
}  /* add_destructor_call */


static void add_conditional_flag_test(a_variable_ptr         test_var,
                                      an_insert_location_ptr insert_location,
                                      an_insert_location_ptr insert_location2)
/*
Add a sequence of code that tests conditional flag set on initialization
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
  insert_if_statement(compare_node, /*is_initialization_guard=*/FALSE,
                      insert_location, (a_statement_ptr *)NULL,
                      insert_location2, (an_insert_location *)NULL);
}  /* add_conditional_flag_test */


void gen_one_destruction(a_dynamic_init_ptr     dip,
                         an_insert_location_ptr insert_location)
/*
Generate code for the destruction of the indicated dynamic initialization.
The code is inserted at *insert_location and *insert_location is updated.
This routine is used for automatically-generated destructions at ends
of/exits from lifetimes, which means it is not used for static variables
and not for constructor_init entries in destructors.
*/
{
  a_destructible_entity_descr_ptr dedp = dip->destructible_entity_descr;
  an_insert_location              insert_location2;
  an_insert_location_ptr          effective_insert_loc;

  check_assertion(dedp != NULL);
  /* Set the cleanup state to what it should be after the destruction,
     because as soon as we start the destruction it's the destructor's
     job to deal with partial destruction. */
  curr_context->curr_cleanup_state =
                          dedp->cleanup_state_to_set_when_starting_destruction;
  if (exceptions_enabled) {
    insert_code_to_indicate_cleanup_state(curr_context->curr_cleanup_state,
                                          insert_location,
                                          /*unreachable=*/FALSE);
  }  /* if */
  effective_insert_loc = insert_location;
  /* If the entity is conditionally-created temporary, generate an
     "if" statement to test whether or not the variable was ever
     initialized.  Only do the destruction if it was. */
  if (dip->inside_conditional_expression) {
    add_conditional_flag_test(dedp->conditional_flag_var, 
                              insert_location, &insert_location2);
    effective_insert_loc = &insert_location2;
  }  /* if */
#if DO_UNORDERED_EH_PROCESSING
  if (exceptions_enabled) {
    if (dip->unordered) {
      /* For unordered destructions, clear the associated conditional flag
         to indicate that the destruction has been done.  That's necessary
         because all of the members of the unordered set stay in the
         active cleanup list in the region table until all of them have been
         destroyed, and the conditional flags tell us which ones still
         require destruction.  We don't need to do this on the last
         destruction in an unordered set because the whole set comes out
         of the region table at that point. */
      a_dynamic_init_ptr next_dip = dedp->next_in_region_table;
      if (next_dip != NULL && next_dip->unordered) {
        reset_conditional_flag_var(dedp-> conditional_flag_var,
                                   effective_insert_loc);
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* DO_UNORDERED_EH_PROCESSING */
  add_destructor_call(dip->destructor,
                      &dedp->init_pos_descr,
                      /*have_complete_object=*/TRUE,
                      (an_expr_node_ptr)NULL,
                      effective_insert_loc);
}  /* gen_one_destruction */


static void lower_ck_dynamic_init(a_constant_ptr         con_ptr,
                                  an_init_pos_descr_ptr  ipdp,
                                  a_boolean              dtor_case,
                                  a_constructor_init_ptr ctor_init,
                                  a_boolean              others_follow_in_aggr,
                                  an_insert_location_ptr insert_location,
                                  a_boolean              *keep_constant)
/*
Generate executable code to handle a ck_dynamic_init constant (pointed
to by con_ptr).  The entity to be initialized is described by ipdp.
The necessary statements are inserted at *insert_location and
*insert_location is updated.  If ipdp->array_element_sequence is TRUE,
this call is handling a sequence of elements in an array.  If dtor_case
is TRUE, we are generating a destructor wrapper; do the destruction
indicated in the dynamic init but ignore any initialization.  If the dynamic
initialization is part of a constructor initializer, ctor_init points
to the constructor-init entry.  others_follow_in_aggr is TRUE if this constant
is followed by others in an aggregate initialization (i.e., it's not the
last).  If the initialization is of an aggregate and there some parts
of the initialization that are constant, the ck_dynamic_init constant
will be changed to an aggregate constant for the constant parts and
*keep_constant will be set to TRUE.
*/
{
  a_constant_ptr next_con;
  a_type_ptr     desired_type;
  a_constant_ptr constant_to_keep = NULL;

  if (dtor_case) {
    /* In a destructor case, so the "initialization" is really
       destruction. */
    lower_destructor_dynamic_init(con_ptr->variant.dynamic_init, ipdp,
                                  /*have_complete_object=*/TRUE,
                                  (an_expr_node_ptr)NULL,
                                  insert_location);
  } else {
    /* Normal initialization. */
    lower_dynamic_init(con_ptr->variant.dynamic_init, ipdp,
                       (an_expr_node_ptr)NULL, (an_expr_node_ptr)NULL,
                       ctor_init, LDIO_FULL_EXPR, others_follow_in_aggr,
                       insert_location, (a_boolean *)NULL,
                       &constant_to_keep);
  }  /* if */
  if (constant_to_keep != NULL) {
    /* There's a constant part of the initialization that needs to be
       kept.  Replace the ck_dynamic_init constant with that constant. */
    a_constant_ptr con_ptr_next = con_ptr->next;
    copy_constant(constant_to_keep, con_ptr);
    con_ptr->next = con_ptr_next;
    *keep_constant = TRUE;
  } else {
    /* Overwrite the constant with a harmless constant of the right kind.
       It's just a place-holder that gets overwritten by the dynamic
       initialization. */
    desired_type = ipdp->modifiers->type;
    /* For pointers to members, switch to the implementation type. */
    if (is_or_was_ptr_to_data_member_type(desired_type)) {
      desired_type = integer_type(targ_ptr_to_data_member_int_kind);
    } else if (is_or_was_ptr_to_member_function_type(desired_type)) {
      desired_type = make_mptr_type();
    }  /* if */
    if (is_aggregate_or_union_type(desired_type)
#if DO_C99_IL_LOWERING
        || is_complex_type(desired_type)
#endif /* DO_C99_IL_LOWERING */
                                                ) {
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
      /* Note that C99 complex types are lowered to aggregate types. */
      set_constant_kind(con_ptr, (a_constant_repr_kind)ck_aggregate);
    } else {
      /* Not an aggregate: a zero of the right type will be fine. */
      next_con = con_ptr->next;
#if DO_C99_IL_LOWERING
      if (is_imaginary_type(desired_type)) {
        /* In C99, create a float constant for an imaginary type. */
        desired_type = skip_typerefs(desired_type);
        desired_type = float_type(desired_type->variant.float_kind);
      }  /* if */
#endif /* DO_C99_IL_LOWERING */
      make_zero_of_proper_type(desired_type, con_ptr);
      con_ptr->next = next_con;
    }  /* if */
  }  /* if */
}  /* lower_ck_dynamic_init */


static void lower_dynamic_init_aggregate_constant(
                                 a_constant_ptr         aggr_const,
                                 an_init_pos_descr_ptr  ipdp,
                                 a_boolean              dtor_case,
                                 a_constructor_init_ptr ctor_init,
                                 a_boolean              others_follow_in_aggr,
                                 an_insert_location_ptr insert_location,
                                 a_boolean              *keep_constant)
/*
aggr_const points to a ck_aggregate constant that contains one or more
ck_dynamic_init dynamic initializations.  The ck_aggregate constant is
the initial value for the entity described by ipdp.  If dtor_case is TRUE,
we are generating a destructor wrapper; do the destruction indicated in
the aggregate init but ignore any initialization.  If the dynamic
initialization is part of a constructor initializer, ctor_init points to
the constructor-init entry.  others_follow_in_aggr is TRUE if this constant
is followed by others in an aggregate initialization (i.e., it's not the
last).  Insert statements to implement the initialization at *insert_location
and update *insert_location.  If there are any (genuine) constants in the
aggregate, set *keep_constant to TRUE.
*/
{
  an_init_pos_descr    ipd;
  an_init_pos_modifier ipm, *ipmp;
  a_type_ptr           aggr_type;
  a_constant_ptr       con_ptr, repeated_con, prev_con;
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
  /* If the modifier up one level is a field selection of an anonymous
     parent object (standard or nonstandard), remove the parent object
     from the modifiers list.  This is done so the anonymous union level
     does not appear in the modifiers list.  The anonymous union
     levels are restored when the field selections are generated
     (see au_field_lvalue_selection_expr).  This seems a bit
     roundabout, and it is, but it's needed to get ctor-inits
     of anonymous union fields to work right.  In that case it's
     not convenient to add additional modifiers to represent each
     anonymous union level, because the modifier structures are local
     variables and there's no opportunity to do recursion to
     get the extra modifier entries on the list. */
  ipmp = ipd.modifiers;
  if (ipmp != NULL && ipmp->curr_field != NULL &&
      ipmp->curr_field->is_anonymous_parent_object) {
    ipd.modifiers = ipmp->next;
  }  /* if */
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
  for (prev_con = NULL;
       con_ptr != NULL;
       prev_con = con_ptr, con_ptr = con_ptr->next) {
    a_boolean others_follow;
    if (con_ptr->kind == (a_constant_repr_kind)ck_designator) {
      /* A designator appears (e.g., in a C99 nonconstant aggregate
         initialization).  Update the current position. */
      if (con_ptr->variant.designator.field != NULL) {
        check_assertion(!array_aggr);
        ipmp->curr_field = con_ptr->variant.designator.field;
      } else {
        check_assertion(array_aggr);
        ipmp->curr_elem = con_ptr->variant.designator.array_element;
      }  /* if */
      con_ptr = con_ptr->next;
      check_assertion(con_ptr != NULL &&
                      con_ptr->kind != (a_constant_repr_kind)ck_designator);
    }  /* if */
    others_follow = (others_follow_in_aggr || con_ptr->next != NULL);
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
                            others_follow, insert_location, keep_constant);
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
      if (C_mode()) {
        /* With designated initializers, it is possible to get a repeated
           constant.  Leave it alone, except for lowering the underlying
           constant.  This comes up in C mode when IL lowering is used to
           lower nonconstant initializers.  (However, the repeated constant
           will be actually constant.) */
#if DO_C99_IL_LOWERING
        if (c99_mode || gcc_mode) {
          lower_c99_constant(repeated_con);
        } else
#endif /* DO_C99_IL_LOWERING */
        {
          lower_constant(repeated_con);
        }  /* if */
        *keep_constant = TRUE;
      } else {
#if CHECKING
        if (repeated_con->kind != (a_constant_repr_kind)ck_dynamic_init) {
          internal_error(
    "lower_dynamic_init_aggregate_constant: repeated con not ck_dynamic_init");
        }  /* if */
#endif /* CHECKING */
        ipd.array_element_sequence = TRUE;
        ipd.array_element_count =
                          (a_targ_ptrdiff_t)con_ptr->variant.init_repeat.count;
        lower_ck_dynamic_init(repeated_con, &ipd, dtor_case, ctor_init,
                              others_follow, insert_location, keep_constant);
        /* Remove the ck_init_repeat constant, in case the overall aggregate
           is kept for the constant parts. */
        check_assertion(con_ptr->next == NULL);
        if (prev_con == NULL) {
          aggr_const->variant.aggregate.first_constant = NULL;
        } else {
          prev_con->next = NULL;
        }  /* if */
        aggr_const->variant.aggregate.last_constant = prev_con;
      }  /* if */
    } else if (con_ptr->kind == (a_constant_repr_kind)ck_aggregate) {
      /* Aggregate constant initializing a member of an aggregate. */
      lower_dynamic_init_aggregate_constant(con_ptr, &ipd,
                                            dtor_case, ctor_init,
                                            others_follow, insert_location,
                                            keep_constant);
    } else {
      /* Normal constant. */
      if (C_mode()) {
#if DO_C99_IL_LOWERING
        if (c99_mode || gcc_mode) {
          /* When lowering C99 code, use the C99 lowering routines. */
          lower_c99_constant(con_ptr);
        }  /* if */
#endif /* DO_C99_IL_LOWERING */
      } else {
        /* C++ mode. */
        lower_constant(con_ptr);
      }  /* if */
      if (ipd.indirect_through_variable) {
        /* The entity being initialized is not a simple variable, so we
           don't want to keep any part of the initialization as a constant
           aggregate initialization.  This comes up with return value
           optimization (the variable is initialized with a partially-constant
           aggregate, but the initialization is actually done on the address
           passed in by the caller as the return address, and that can't be
           initialized with an aggregate). */
        an_expr_node_ptr entity_node;
        entity_node = make_init_entity_node(&ipd,
                                            /*using_as_address=*/FALSE,
                                            /*using_as_dest=*/TRUE);
        con_ptr->next = NULL;
        add_init_assignment((a_dynamic_init *)NULL, con_ptr, entity_node,
                            insert_location);
      } else {
        /* Normal case.  Keep this as part of a constant aggregate. */
        *keep_constant = TRUE;
      }  /* if */
    }  /* if */
    /* Find the next member in the aggregate. */
    if (array_aggr) {
      /* Array -- go on to next element. */
      if (con_ptr->kind != (a_constant_repr_kind)ck_init_repeat) {
        ipmp->curr_elem++;
      } else {
        /* For an init-repeat constant, advance the right number of
           elements in the array. */
        ipmp->curr_elem += con_ptr->variant.init_repeat.count;
      }  /* if */
    } else {
      /* Class or struct -- go on to next field (nonstatic data member). */
      ipmp->curr_field = next_initializable_field(ipmp->curr_field->next);
    }  /* if */
    /* Loop while there are more constants. */
  }  /* for */
}  /* lower_dynamic_init_aggregate_constant */

#if !USE_INIT_SECTION_IN_GENERATED_C

/*
Pointer to the struct type for the __linkl structure.  NULL until created.
*/
static a_type_ptr linkl_type;


static a_type_ptr make_linkl_type(void)
/*
Make the __linkl structure used in specifying initialization routines
to be executed at program startup.  It has the following structure:

  struct __linkl {
    struct __linkl *next;
    void           (*ctor)();
    void           (*dtor)();
  };

This is compatible with the structure used by cfront.
Note that the cfront approach uses "char" for "void" in all the above.
*/
{
  a_type_ptr  ptr_func_type, ptr_linkl_type;
  a_field_ptr last_field;

  if (linkl_type == NULL) {
    /* The type made doesn't actually have a name. */
    linkl_type = alloc_type((a_type_kind)tk_struct);
    last_field = NULL;
    /* field: struct __linkl *next; */
    ptr_linkl_type = make_pointer_type(linkl_type);
    make_lowered_field("next", ptr_linkl_type, linkl_type, &last_field);
    /* field: void (*ctor)(); */
    ptr_func_type = make_vptp_type();
    make_lowered_field("ctor", ptr_func_type, linkl_type, &last_field);
    /* field: void (*dtor)(); */
    make_lowered_field("dtor", ptr_func_type, linkl_type, &last_field);
    finish_class_type(linkl_type);
    add_to_front_of_file_scope_types_list(linkl_type);
  }  /* if */
  return linkl_type;
}  /* make_linkl_type */


static void make_code_to_invoke_file_scope_init_routine(
                                         a_routine_ptr file_scope_init_routine)
/*
Make the code that will ensure that the indicated file-scope initialization
routine is invoked at program startup.
*/
{
  a_type_ptr       ptr_func_type;
  a_variable_ptr   link_var;
  a_constant_ptr   aggr_con, init_con1, init_con2, init_con3;
  a_memory_region_number
                   region_to_switch_back_to;

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
  */
  switch_to_file_scope_region(&region_to_switch_back_to);
  /* Make the __linkl struct type. */
  (void)make_linkl_type();
  /* Make the __link variable. */
  link_var = make_lowered_variable("__link", /*already_il_name=*/FALSE,
                                   linkl_type, (a_storage_class)sc_static);
  /* Give the __link variable the initial value
       {NULL, __sti__module_id, NULL}
  */
  aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
  aggr_con->type = link_var->type;
  link_var->init_kind = (an_init_kind)initk_static;
  link_var->initializer.constant = aggr_con;
  /* NULL for "next" field. */
  init_con1 = alloc_constant((a_constant_repr_kind)ck_address);
  make_zero_of_proper_type(make_pointer_type(linkl_type), init_con1);
  /* Address of __sti__module_id for "ctor" field. */
  init_con2 = alloc_constant((a_constant_repr_kind)ck_address);
  set_routine_address_constant(file_scope_init_routine, init_con2,
                               /*set_address_taken_flag=*/TRUE);
  ptr_func_type = make_vptp_type();
  implicit_cast(init_con2, ptr_func_type);
  /* NULL for "dtor" field. */
  init_con3 = alloc_constant((a_constant_repr_kind)ck_address);
  make_zero_of_proper_type(ptr_func_type, init_con3);
  /* Link the constants together under the ck_aggregate constant. */
  aggr_con->variant.aggregate.first_constant = init_con1;
  init_con1->next = init_con2;
  init_con2->next = init_con3;
  aggr_con->variant.aggregate.last_constant  = init_con3;
#if MAINTAIN_NEEDED_FLAGS
  /* This is a funny variable that is "needed" by munch even though it
     is not externally visible. */
#if ONE_INSTANTIATION_PER_OBJECT
  if (one_instantiation_per_object) {
    link_var->instantiation_needed_bit_number =
                      file_scope_init_routine->instantiation_needed_bit_number;
    set_per_instantiation_needed_flag((char *)link_var, iek_variable,
                                    link_var->instantiation_needed_bit_number);
  }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  mark_as_needed((char *)link_var, iek_variable);
#endif /* MAINTAIN_NEEDED_FLAGS */
  switch_back_to_original_region(region_to_switch_back_to);
}  /* make_code_to_invoke_file_scope_init_routine */

#endif /* !USE_INIT_SECTION_IN_GENERATED_C */

#if !ONE_INSTANTIATION_PER_OBJECT
/*ARGSUSED*/ /* <-- needed_bit_number is not used in that case. */
#endif /* !ONE_INSTANTIATION_PER_OBJECT */
static a_scope_ptr make_file_scope_init_or_term_routine(
                                 a_type_ptr                  param1_type,
                                 unsigned long               needed_bit_number,
                                 char                        *prefix,
                                 an_insert_location_ptr      insert_location,
                                 a_memory_region_number      *il_region,
                                 a_generated_routine_context *grcontext)
/*
Make a routine to do file-scope initialization or termination.  param1_type is
the type of the first parameter, or NULL if there are no parameters.  prefix
is the prefix for the name of the routine, or is NULL if the routine should be
unnamed.  Set *insert_location for insertion at the start of the block
statement that is the body of the routine, set *il_region to the IL memory
region number for the routine, and return a pointer to the scope for the
routine.  The routine is external if named, and static if unnamed.  A
generated routine context is pushed, with *grcontext used to save the old
state for later restoration.
*/
#if ONE_INSTANTIATION_PER_OBJECT
/*
needed_bit_number, if non-zero, indicates a per-instantiation "needed"
bit number; each instantiation is being put in a separate file, and this
initialization routine is being generated for the instantiation associated
with the indicated bit number.
*/
#endif /* ONE_INSTANTIATION_PER_OBJECT */
{
  a_routine_ptr   init_rout;
  a_scope_ptr     scope;
  char            *name;
  sizeof_t        prefix_len, alloc_length;
  a_statement_ptr return_stmt;
#if ONE_INSTANTIATION_PER_OBJECT
  char            buffer[50];
#endif /* ONE_INSTANTIATION_PER_OBJECT */

  if (prefix == NULL) {
    /* Make an unnamed routine. */
    name = NULL;
  } else {
    /* Combine the prefix and an identifier for the current module to make
       a name that is likely to be unique. */
    char	*module_id;
    module_id = make_module_id();
    prefix_len = strlen(prefix);
    alloc_length = prefix_len + strlen(module_id) + 1;
#if ONE_INSTANTIATION_PER_OBJECT
    if (needed_bit_number != 0) {
      /* Add a suffix to distinguish initialization routines for
         specific instantiations. */
      (void)sprintf(buffer, "_%lu", needed_bit_number);
      alloc_length += strlen(buffer);
    }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
    name = alloc_lowered_name_string(alloc_length);
    (void)memcpy(name, prefix, size_t_arg(prefix_len));
    (void)strcpy(name+prefix_len, module_id);
#if ONE_INSTANTIATION_PER_OBJECT
    if (needed_bit_number != 0) {
      (void)strcpy(name+prefix_len+strlen(module_id), buffer); /*lint !e645*/
    }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  }  /* if */
  /* Make a type and routine entry for the routine. */
  init_rout = make_rout_entry(name,
                              (a_storage_class)(name != NULL ? sc_unspecified :
                                                               sc_static),
                              void_type(),
                              param1_type);
  /* Make a memory region, scope, and block for the routine definition. */
  scope = make_routine_definition(init_rout, /*make_return=*/TRUE, il_region);
#if IA64_ABI
  if (param1_type != NULL) {
    scope->variant.routine.parameters = 
                                      make_lowered_param_variable(param1_type);
  }  /* if */
#endif /* IA64_ABI */
  /* Save the current state and push a new context for the generated
     routine. */
  push_generated_routine_context(scope, *il_region, grcontext);
  /* Set the insert location to the start of the top-level block. */
  set_block_start_insert_location(scope->assoc_block,
                                  insert_location);
  /* Add the return statement at the end of the routine to the return memo
     list. */
  return_stmt = scope->assoc_block->variant.block.statements;
  check_assertion(return_stmt != NULL &&
                  return_stmt->kind == (a_statement_kind)stmk_return);
  add_to_return_memo_list(return_stmt);
  return scope;
}  /* make_file_scope_init_or_term_routine */


static a_scope_ptr file_scope_init_insert_location(
                                 unsigned long               needed_bit_number,
                                 an_insert_location_ptr      insert_location,
                                 a_memory_region_number      *region_number,
                                 a_generated_routine_context *grcontext)
/*
Create the file-scope initialization routine.  Set *insert location so it
can be used to insert code in that routine, and set *region_number to
the memory region number for the routine.  Return the scope for the routine.
A generated routine context is pushed, with *grcontext used to save the
old state for later restoration.  If needed_bit_number is non-zero, it
is the per-instantiation "needed" bit number associated with an instantiation,
and the routine being generated is the initialization routine for that
instantiation.
*/
{
  a_scope_ptr scope = make_file_scope_init_or_term_routine(
                                       (a_type_ptr)NULL,
                                       needed_bit_number,
                                       IL_LOWERING_INIT_ROUTINE_PREFIX,
                                       insert_location,
                                       region_number,
                                       grcontext);
#if ONE_INSTANTIATION_PER_OBJECT
  scope->variant.routine.ptr->instantiation_needed_bit_number =
                                                             needed_bit_number;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  return scope;
}  /* file_scope_init_insert_location */


static a_scope_ptr file_scope_term_insert_location(
                                   an_insert_location_ptr      insert_location,
                                   a_memory_region_number      *region_number,
                                   a_generated_routine_context *grcontext)
/*
Create a file-scope termination routine.  Set *insert_location so it
can be used to insert code in that routine, and set *region_number to
the memory region number for the routine.  Return the scope for the routine.
Such routines are used for code that destroys a single variable (not, as
in cfront, for the code for all the file-scope destructions), so there
may be many different such routines generated (all unnamed).
A generated routine context is pushed, with *grcontext used to save the
old state for later restoration.
*/
{
  a_scope_ptr scope = make_file_scope_init_or_term_routine(
#if IA64_ABI
                                       void_star_type(),
#else /* !IA64_ABI */
                                       (a_type_ptr)NULL,
#endif /* !IA64_ABI */
                                       (unsigned long)0,
                                       (char *)NULL,  /* Unnamed. */
                                       insert_location,
                                       region_number,
                                       grcontext);
  return scope;
}  /* file_scope_term_insert_location */


void init_conditional_flag_var(
                              a_destructible_entity_descr_ptr dedp,
                              an_insert_location              *insert_location)
/*
Insert code to initialize a conditional flag variable to zero.
dedp points to the destructible entity description for the
entity whose conditional flag should be initialized.  If code needs
to be inserted, it is inserted at *insert_location.
*/
{
  a_variable_ptr cond_var = dedp->conditional_flag_var;

  /* If the conditional flag is static, initialization to zero is
     implicit and requires nothing special in the IL. */
  if (cond_var->storage_class != (a_storage_class)sc_static) {
    /* Otherwise, for an automatic variable, the variable must be explicitly
       initialized to zero. */
    a_constant zero_constant;
    set_integer_constant(&zero_constant, (a_host_large_integer)0,
                         (an_integer_kind)ik_int);
    if (is_expr_insert_location_kind(insert_location->kind)) {
       /* The insert location is inside an expression, so use an stmk_expr. */
      (void)insert_assignment_statement(var_lvalue_expr(cond_var),
                                        (an_expr_operator_kind)eok_iassign,
                                       alloc_node_for_constant(&zero_constant),
                                        insert_location);
    } else {
      /* Normal case: use an stmk_init. */
      a_statement_ptr    stmk_init_stmt;
      a_dynamic_init_ptr init_dip =
                         alloc_dynamic_init((a_dynamic_init_kind)dik_constant);
      init_dip->variable = cond_var;
      init_dip->follows_an_exec_statement = TRUE;
      /* The dynamic init entry is pointed to by the variable. */
      cond_var->init_kind = (an_init_kind)initk_dynamic;
      cond_var->initializer.dynamic = init_dip;
      init_dip->variant.constant = alloc_unshared_constant(&zero_constant);
      /* The dynamic init entry is pointed to by an stmk_init statement. */
      stmk_init_stmt = alloc_statement((a_statement_kind)stmk_init);
      stmk_init_stmt->variant.dynamic_init = init_dip;
      insert_statement(stmk_init_stmt, insert_location);
    }  /* if */
  }  /* if */
#if DO_FULL_PORTABLE_EH_LOWERING
  if (exceptions_enabled) {
    an_init_pos_descr ipd;
    /* Put the address of the variable into the object address table. */
    set_var_init_pos_descr(cond_var, &ipd);
    init_object_addr_table_entry(&ipd, dedp->conditional_flag_handle,
                                 insert_location);
  }  /* if */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
}  /* init_conditional_flag_var */


static void set_conditional_flag_var(a_variable_ptr     conditional_flag_var,
                                     an_insert_location *insert_location)
/*
Insert code at *insert_location to set the indicated conditional flag variable
to a nonzero value.
*/
{
  (void)insert_var_assignment_statement(
                            conditional_flag_var,
                            (an_expr_operator_kind)eok_iassign,
                            node_for_integer_constant(1L,
                                                      (an_integer_kind)ik_int),
                            insert_location);
}  /* set_conditional_flag_var */


static void reset_conditional_flag_var(a_variable_ptr     conditional_flag_var,
                                       an_insert_location *insert_location)
/*
Insert code at *insert_location to reset the indicated conditional flag
variable to a zero value.
*/
{
  (void)insert_var_assignment_statement(
                            conditional_flag_var,
                            (an_expr_operator_kind)eok_iassign,
                            node_for_integer_constant(0L,
                                                      (an_integer_kind)ik_int),
                            insert_location);
}  /* reset_conditional_flag_var */


static void adjust_cleanup_state_for_inner_lifetime_temporaries(
                                                   a_dynamic_init_ptr temp_dip,
                                                   a_dynamic_init_ptr dip)
/*
dip points to a destruction for an entity that is created while
destructible temporaries in an inner object lifetime are still in existence.
temp_dip points to the destruction for one of those temporaries.
Update the region table information for the temporary and those
following it in its object lifetime so that the cleanup list includes
the temporaries and then the outer-lifetime entity.  Note that some
entries on the list may be ones indicating freeing of storage on
exceptions, rather than temporaries in the strict sense.
*/
#if GENERATE_EH_TABLES
/*
This may involve cloning some of the region table entries for the
temporaries, since currently the last temporary points past the
outer-lifetime entity to the next thing to be destroyed after that.
The region table entry for dip has already been created.
*/
#endif /* GENERATE_EH_TABLES */
{
#if GENERATE_EH_TABLES
  a_destructible_entity_descr_ptr dedp = temp_dip->destructible_entity_descr;
  a_dynamic_init_ptr              next_dip = dedp->next_in_region_table;

  if (next_dip == NULL) {
    /* End of the list, beginning of the object lifetime of the temporaries. */
    curr_context->latest_initialization = NULL;
    curr_context->curr_cleanup_state = dip;
  } else {
    /* Use a recursive call to process the rest of the list. */
    adjust_cleanup_state_for_inner_lifetime_temporaries(next_dip, dip);
  }  /* if */
  /* Adjust the pointer to the previous entity, to one after this one on
     the cleanup list. */
  dedp->cleanup_state_to_set_when_starting_destruction =
                                              curr_context->curr_cleanup_state;
  /* Clone the region table entry for this destruction and add it to
     the beginning of a region table cleanup sequence that runs through
     the temporaries and then destroys the outer-lifetime entity.
     Don't clone the region table entry for the first destruction
     in the temporary lifetime, because a cleanup state including
     that destruction will not be needed -- we start with destroying
     that one, and the cleanup state established right away points to
     the second destruction on the list, or the outer-lifetime entity's
     destruction if there is only one temporary destruction on the
     list. */
  if (temp_dip != temp_dip->lifetime->destructions) {
    clone_region_table_entry_list(temp_dip, next_dip);
  }  /* if */
#else /* !GENERATE_EH_TABLES */
  /* Find the last destruction entry for a temporary and reset its
     cleanup_state_to_set_when_starting_destruction to dip. */
  { a_dynamic_init_ptr last_dip;
    a_destructible_entity_descr_ptr last_dedp;
    for (last_dip = temp_dip;
         last_dip->next_in_destruction_list != NULL;
         last_dip = last_dip->next_in_destruction_list) {}
    last_dedp = last_dip->destructible_entity_descr;
    last_dedp->cleanup_state_to_set_when_starting_destruction = dip;
  }    
#endif /* GENERATE_EH_TABLES */
  curr_context->latest_initialization = temp_dip;
  set_curr_cleanup_state_to_latest_initialization();
}  /* adjust_cleanup_state_for_inner_lifetime_temporaries */


static void add_dyn_init_cleanup(a_dynamic_init_ptr     dip,
                                 an_init_pos_descr_ptr  ipdp,
                                 a_boolean              set_cond_flag_if_any,
                                 a_context_ptr          context,
                                 an_insert_location_ptr insert_location)
/*
The code for the initialization at *dip (for the entity whose position
is given by ipdp) has just been put out.  The initialization requires
some kind of later cleanup (e.g., destruction).  Put out anything needed
to put the dynamic init on the cleanup list.  Set any associated
conditional flag if set_cond_flag_if_any is TRUE; otherwise, do not
set it.  context is the effective context for the initialization.
Any code needed is inserted at *insert_location.
*/
{
  a_destructible_entity_descr_ptr dedp = dip->destructible_entity_descr;
#if GENERATE_EH_TABLES
  a_dynamic_init_ptr              prev_initialization =
                                                context->latest_initialization;
#endif /* GENERATE_EH_TABLES */

  check_assertion_str(dedp != NULL,
                    "add_dyn_init_cleanup: missing destructible entity descr");
  if (set_cond_flag_if_any && dedp->conditional_flag_var != NULL) {
    /* This initialization has an associated conditional flag variable,
       e.g., because it is inside a conditional expression.  Set the
       flag to nonzero to indicate the initialization has been done. */
    set_conditional_flag_var(dedp->conditional_flag_var, insert_location);
  }  /* if */
  /* Put a copy of the initialization position description into the
     destruction entity description for use at destruction time. */
  copy_init_pos_descr(ipdp, &dedp->init_pos_descr);
  dedp->cleanup_state_to_set_when_starting_destruction =
                                                   context->curr_cleanup_state;
  /* Set the current cleanup state. */
  context->curr_cleanup_state = dip;
  /* Record this dynamic initialization as the last encountered in the
     context. */
  context->latest_initialization = dip;
  if (exceptions_enabled) {
#if GENERATE_EH_TABLES
    /* Make a region table entry for the entity (and for its conditional
       flag, if it has one). */
    make_dyn_init_region_table_entry(dip,
                                     prev_initialization,
                                     insert_location);
#endif /* GENERATE_EH_TABLES */
    if (dip->overlaps_temps_in_inner_lifetime) {
      /* This entity is initialized during an inner lifetime, and overlaps
         with the lifetime of some temporaries in the inner lifetime.
         Adjust the cleanup information for those so that both the temporaries
         and the present entity are on the cleanup list. */
      check_assertion_str(curr_context != context,
                          "add_dyn_init_cleanup: curr_context == context");
      check_assertion_str(curr_context->latest_initialization != NULL,
                          "add_dyn_init_cleanup: no temps");
      adjust_cleanup_state_for_inner_lifetime_temporaries(
                                     curr_context->latest_initialization, dip);
#if !GENERATE_EH_TABLES
      /* Insert an leck_initialization_completed node that indicates the
         point at which the initialization has been done. */
      { an_expr_node_ptr node = alloc_lowered_eh_construct_node(
                   (a_lowered_eh_construct_kind)leck_initialization_completed);
        node->variant.lowered_eh.variant.dynamic_init = dip;
        (void)insert_expr_statement(node, insert_location);
      }
#endif /* !GENERATE_EH_TABLES */
      /* There's no need to emit code to set the cleanup state here: it's
         not necessary because the cleanup state will be set in a moment
         when the destruction of the last temporary begins.  If we were to
         try to set the cleanup state here, we would be referring to the
         region table for that last temporary, which was not cloned because
         it's not needed. */
    } else {
      insert_code_to_indicate_cleanup_state(context->curr_cleanup_state,
                                            insert_location,
                                            /*unreachable=*/FALSE);
    }  /* if */
  }  /* if */
}  /* add_dyn_init_cleanup */

#if !IA64_ABI

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
  a_field_ptr   last_field;

  if (needed_destruction_type == NULL) {
    /* Make the struct type.  It doesn't actually have a name. */
    needed_destruction_type = alloc_type((a_type_kind)tk_struct);
    add_to_front_of_file_scope_types_list(needed_destruction_type);
    last_field = NULL;
    /* field: a_needed_destruction_ptr next */
    make_lowered_field("next", make_pointer_type(needed_destruction_type),
                       needed_destruction_type, &last_field);
    /* field: void *object */
    make_lowered_field("object", void_star_type(), needed_destruction_type,
                       &last_field);
    needed_destruction_object_field = last_field;
    /* field: __vptp dtor */
    make_lowered_field("dtor", make_vptp_type(), needed_destruction_type,
                       &last_field);
    finish_class_type(needed_destruction_type);
  }  /* if */
  return needed_destruction_type;
}  /* make_needed_destruction_type */

#endif /* !IA64_ABI */

static a_routine_ptr make_destruction_routine(a_dynamic_init_ptr    dip,
                                              an_init_pos_descr_ptr ipdp)
/*
Make a routine that contains the code necessary to do the destruction of
the static variable whose initialization is described by dip and whose position
is described by ipdp.
*/
{
  a_scope_ptr            scope;
  an_insert_location     insert_location;
  a_memory_region_number region_number;
  a_generated_routine_context
                         grcontext;
  a_routine_ptr          routine;

  /* Create a routine. */
  scope = file_scope_term_insert_location(&insert_location, &region_number,
                                          &grcontext);
  /* Save the routine pointer because the scope won't be around at the
     end of this routine. */
  routine = scope->variant.routine.ptr;
  /* Generate the code for the destruction. */
  add_destructor_call(dip->destructor, ipdp, /*have_complete_object=*/TRUE,
                      (an_expr_node_ptr)NULL, &insert_location);
  /* Mark the variable as referenced from another function. */
  ipdp->variable->referenced_non_locally = TRUE;
  pop_generated_routine_context(scope, region_number, &grcontext);
  return routine;
}  /* make_destruction_routine */


/*
Pointer to the routine entry for the runtime routine used to record a
needed call of a destructor, once created.  NULL until then.
*/
static a_routine_ptr
		record_needed_destruction_routine;

#if IA64_ABI

#if IA64_ABI_USE_GUARD_ACQUIRE_RELEASE
static a_routine_ptr
		guard_acquire_routine,
		guard_release_routine;
#endif /* IA64_ABI_USE_GUARD_ACQUIRE_RELEASE */

/*
Pointer to the variable entry for __dso_handle.
*/
static a_variable_ptr
		dso_handle_var;

#endif /* IA64_ABI */

static void record_needed_destruction(a_dynamic_init_ptr     dip,
                                      an_init_pos_descr_ptr  ipdp,
                                      an_insert_location_ptr insert_location)
/*
ipdp describes the position of a static entity, initialized by the dynamic
initialization entry pointed to by dip, that requires a destruction.
Generate a runtime call that records the need for the destruction at the
time of program termination.  Insert any generated code at *insert_location
and update *insert_location accordingly.
*/
{
#if !IA64_ABI
  a_variable_ptr         var;
  a_constant_ptr         aggr_con;
#endif /* !IA64_ABI */
  a_constant_ptr         object_con, dtor_con;
#if !IA64_ABI
  a_constant_ptr         next_con;
#endif /* !IA64_ABI */
  a_boolean              complex_cleanup, complex_address;
  a_routine_ptr          dtor_routine;
  an_expr_node_ptr       object_node, call_node;
  a_type_ptr             entity_type = type_from_init_pos_descr(ipdp);
#if IA64_ABI
  an_expr_node_ptr       dtor_node, dso_handle_node;
#endif /* IA64_ABI */

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
  /* For the IA-64 ABI, the runtime routine is __cxa_atexit. */
  complex_cleanup = ipdp->array_element_sequence ||
                    is_array_type(entity_type);
  /* Compute the object address (instead of doing static initialization to
     the address) if it is more than a simple variable. */
  complex_address = ipdp->indirect_through_variable ||
                    ipdp->modifiers != NULL;
#if !IA64_ABI
  /* Make an unnamed static variable for the descriptive structure. */
  var = make_unnamed_local_static_variable(make_needed_destruction_type(),
                                           /*in_function_scope=*/FALSE);
  /* Make the top-level aggregate constant that will be its initial value. */
  aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
  aggr_con->type = var->type;
  /* Use a local-static-variable-init entry to indicate the initialization. */
  (void)make_local_static_variable_init(var, curr_context->scope,
                                        (an_init_kind)initk_static,
                                        aggr_con, (a_dynamic_init_ptr)NULL);
  /* Make the constants under the aggregate constant. */
  next_con = alloc_constant((a_constant_repr_kind)ck_address);
  make_zero_of_proper_type(make_pointer_type(var->type), next_con);
#endif /* !IA64_ABI */
  object_con = alloc_constant((a_constant_repr_kind)ck_address);
  dtor_con = alloc_constant((a_constant_repr_kind)ck_address);
  if (complex_cleanup) {
    /* Complex cleanup -- the object field is NULL and the dtor field points
       to a fabricated routine containing the destruction code. */
    make_zero_of_proper_type(void_star_type(), object_con);
    dtor_routine = make_destruction_routine(dip, ipdp);
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
    dtor_routine = dip->destructor;
#if IA64_ABI
    dtor_routine = alternate_entry_point(dtor_routine,
                                         (a_ctor_or_dtor_kind)cdk_complete,
                                         /*define_now=*/FALSE);
#endif /* IA64_ABI */
  }  /* if */
  set_routine_address_constant(dtor_routine, dtor_con,
                               /*set_address_taken_flag=*/TRUE);
#if !IA64_ABI
  implicit_cast(dtor_con, make_vptp_type());
  /* Link the aggregate constant together. */
  aggr_con->variant.aggregate.first_constant = next_con;
  next_con->next = object_con;
  object_con->next = dtor_con;
  aggr_con->variant.aggregate.last_constant = dtor_con;
#endif /* !IA64_ABI */
  if (!complex_cleanup && complex_address) {
    /* For simple cleanup with a complex address, compute the object address
       in code and store it in the object field of the struct. */
#if !IA64_ABI
    an_expr_node_ptr field_node;
    a_statement_ptr  assign_stmt;
#endif /* !IA64_ABI */
    object_node = make_init_entity_node(ipdp, /*using_as_address=*/TRUE,
                                        /*using_as_dest=*/FALSE);
#if !IA64_ABI
    object_node = add_cast_if_necessary(object_node,
                                        needed_destruction_object_field->type);
    field_node = field_lvalue_selection_expr(var_lvalue_expr(var),
                                             needed_destruction_object_field);
    assign_stmt = insert_assignment_statement(field_node,
                                            (an_expr_operator_kind)eok_passign,
                                              object_node,
                                              insert_location);
    set_stmt_pos_to_code_pos_for_lowering(assign_stmt);
#else /* IA64_ABI */
  } else {
    object_node = alloc_node_for_constant(object_con);
#endif /* IA64_ABI */
  }  /* if */
#if IA64_ABI
  dtor_node = alloc_node_for_constant(dtor_con);
  if (dso_handle_var == NULL) {
    /* Make the variable that identifies the current DSO, i.e. it
       discriminates between user code and dynamically loaded libraries. */
    dso_handle_var = make_lowered_variable("__dso_handle",
                                           /*already_il_name=*/FALSE,
                                           void_star_type(),
                                           (a_storage_class)sc_extern);
  }  /* if */
  dso_handle_node = var_lvalue_expr(dso_handle_var);
  dtor_node->next = object_node;
  object_node->next = dso_handle_node;
  /* Make a call of __cxa_atexit.  Its arguments are the expressions created
     above. */
  call_node = make_runtime_rout_call("__cxa_atexit", 
                                     &record_needed_destruction_routine,
                                     integer_type((an_integer_kind)ik_int),
                                     dtor_node);
#else /* !IA64_ABI */
  /* Make a call of __record_needed_destruction.  Its argument is the
     address of the structure variable created above. */
  call_node = make_runtime_rout_call("__record_needed_destruction",
                                     &record_needed_destruction_routine,
                                     void_type(), var_lvalue_expr(var));
  /* Make a statement containing the call and insert it at the right
     location. */
#endif /* !IA64_ABI */
  (void)insert_expr_statement_set_pos(call_node, insert_location);
  /* Remove the dynamic initialization from the destruction list, since
     its destruction is now handled by the static cleanup mechanism. */
  remove_from_destruction_list(dip);
}  /* record_needed_destruction */


static an_expr_node_ptr copy_expr_to_function_memory_region(
                                                         an_expr_node_ptr expr)
/*
Copy the indicated expression (in the file scope) to the current IL memory
region (a function scope) and return a pointer to the copy.  This is used
when generating the file-scope initialization routine: initializer
expressions are copied into the function scope so that when they are lowered
there isn't a mixture of function scope and file scope pieces in the
resulting expression.
*/
{
  an_expr_node_ptr expr_copy = copy_expr_tree(expr,
                                              CE_TRANSFER_DESTR_ENTITY_DESCR |
                                              CE_UNLINK_SOURCE_DESTRUCTIONS);
  /* If the expression has an object lifetime node at the top, eliminate
     it, because the source expression will not remain in the IL tree. */
  eliminate_expr_object_lifetime(expr);
  return expr_copy;
}  /* copy_expr_to_function_memory_region */


static void push_init_expr_lifetime(
                                   an_object_lifetime_ptr *init_expr_lifetime,
                                   a_boolean              copy_lifetime,
                                   a_context              *context,
                                   an_insert_location     *insert_location,
                                   an_insert_location     *insert_location2,
                                   an_insert_location_ptr *eff_insert_location)
/*
*init_expr_lifetime points to an object lifetime that is attached to a
dynamic initialization and surrounds the initialization.  Push it onto the
context stack.  If copy_lifetime is TRUE, push a copy instead, update
*init_expr_lifetime to point to the copy, and unbind the original.  context is
the address of a context block to be pushed onto the stack.  *insert_location
is the point at which any generated code should be inserted.  If this
routine needs to insert a block there so it can bind the object lifetime
to it, it will put the insert location for within that block in
*insert_location2 and set *eff_insert_location to point at *insert_location2.
Otherwise *eff_insert_location is set to point at *insert_location.  The
caller then uses *eff_insert_location as the insert point for the
code for the dynamic initialization.
*/
{
  *eff_insert_location = insert_location;
  if (copy_lifetime) {
    /* Copy the lifetime to the current function scope if necessary.
       (For example, when generating the file-scope initialization routine,
       the object lifetime is in the file scope but we need it in the
       function scope.) */
    an_object_lifetime_ptr saved_curr_object_lifetime = curr_object_lifetime;
    curr_object_lifetime = innermost_function_scope->lifetime;
    push_object_lifetime(iek_none, (char *)NULL, (*init_expr_lifetime)->kind);
    /* The original lifetime won't be used, so unbind it. */
    unbind_object_lifetime(*init_expr_lifetime);
    *init_expr_lifetime = curr_object_lifetime;
    curr_object_lifetime = saved_curr_object_lifetime;
  }  /* if */
  /* Push a context for the lifetime.  */
  push_context(context, (a_scope_ptr)NULL, *init_expr_lifetime);
  if (keep_object_lifetime_info_in_lowered_il) {
    a_statement_ptr block_stmt;
    /* The dynamic init entry will be rewritten in lowering, so it probably
       won't end up in the IL tree.  We have to keep the object lifetime,
       but to do so we have to attach it to some other entity (instead of
       the dynamic init).  We add a block statement and attach the lifetime
       to the block statement.  This is only possible if the insert location
       passed in is a statement position rather than an expression
       position.  The only case where there could potentially be a
       problem is on a constructor_init being expanded on an assignment
       to "this", but assignment to "this" is suppressed when exceptions
       are enabled, so it ends up not being a problem. */
    check_assertion_str(!is_expr_insert_location_kind(insert_location->kind),
 "push_init_expr_lifetime: cannot preserve obj lifetime with expr insert loc");
    /* Add a block and update the caller's insert location to follow the
       block.  Then use an insert location inside the block for the rest
       of the lowering of the initialization. */
    block_stmt = alloc_statement((a_statement_kind)stmk_block);
    insert_statement(block_stmt, insert_location);
    set_block_start_insert_location(block_stmt, insert_location2);
    *eff_insert_location = insert_location2;
    /* Rebind the object lifetime to the block. */
    if (!copy_lifetime) {
      unbind_object_lifetime(*init_expr_lifetime);
    }  /* if */
    bind_object_lifetime(*init_expr_lifetime, iek_block,
                         (char *)block_stmt->variant.block.extra_info);
  }  /* if */
}  /* push_init_expr_lifetime */


static void add_first_time_test(a_variable_ptr         guarded_var,
                                an_insert_location_ptr insert_location,
                                an_insert_location_ptr insert_location2,
                                a_statement_ptr        *block_stmt,
                                a_variable_ptr         *test_var)
/*
Add a first-time test sequence that will surround the initialization of the
local static variable guarded_var.  In effect:

  static int test_var;  // Global test var, implicitly init to 0
  {
    if (test_var == 0) {
      test_var = 1;
      ... real initialization of guarded_var
    }
  }

The sequence is inserted at *insert_location.  *insert_location is updated
for further insertion after the "if"; *insert_location2 is set for insertion
after the assignment statement inside the "if".  *block_stmt is set to point
at the block statement inserted, in the statement insert case.  A pointer to
the conditional variable is returned in *test_var.  If insert_location
and insert_location2 point to the same location, the value set in that
location is the insert_location2 value (after the assignment statement).
*/
{
  an_expr_node_ptr   test_var_node, compare_node;
  an_integer_kind    int_kind;
  a_type_ptr         int_type;
#if IA64_ABI
#if IA64_ABI_USE_GUARD_ACQUIRE_RELEASE
  a_statement_ptr    outer_then;
#endif /* IA64_ABI_USE_GUARD_ACQUIRE_RELEASE */
#endif /* IA64_ABI */

  /* Make the static first-time-test variable in the current scope. */
#if !IA64_ABI
  int_kind = (an_integer_kind)ik_int;
#else /* IA64_ABI */
  /* The ABI specifies that we use a 64-bit integer type.  Try that, and
     then fall back to "int". */
  int_kind = int_kind_for_bit_size(64, /*is_signed=*/FALSE);
  if (int_kind == (an_integer_kind)ik_none) {
    int_kind = (an_integer_kind)int_kind;
  }  /* if */
#endif /* IA64_ABI */
  int_type = integer_type(int_kind);
  if (routine_might_exist_in_multiple_copies(
                                 innermost_function_scope->variant.routine.ptr)
#if IA64_ABI && TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE
      /* In the IA64 ABI this routine is used for static data members of
         template classes, too.  This routine is only called if the static
         data member has external linkage, in which case the guard variable
         must have external linkage too. */
      || guarded_var->is_template_static_data_member
#endif /* IA64_ABI && TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE */
                                                                         ) {
    /* The current routine is extern inline, so the guard variable has to
       be external (because the local static variable itself will be
       turned into an external variable). */
    *test_var = make_global_var_with_prefixed_name(
#if !IA64_ABI
                                                   "__LSG__",
#else /* IA64_ABI */
                                                   "_ZGV",
#endif /* IA64_ABI */
                                                   int_kind,
                                                 &guarded_var->source_corresp);
#if IA64_ABI
    if (guarded_var->comdat_group != NULL) {
      (*test_var)->comdat_group = guarded_var->comdat_group;
    }  /* if */
#endif /* IA64_ABI */
  } else {
    /* The guard variable need not be visible outside of the function,
       so an unnamed variable is fine. */
    *test_var = make_unnamed_local_static_variable(int_type,
                                                  /*in_function_scope=*/FALSE);
  }  /* if */
  /* Make "test_var == 0". */
#if !IA64_ABI
  test_var_node = var_rvalue_expr(*test_var);
  test_var_node->next = node_for_integer_constant(0L, (an_integer_kind)ik_int);
#else /* IA64_ABI */
  /* In the IA64 ABI, only the first byte of the variable is specified by 
     the ABI.  The remainder is reserved for use in multithreaded
     implementations. */
  test_var_node = add_cast_to_char_star(var_lvalue_expr(*test_var));
  test_var_node = add_indirection_to_node(test_var_node);
  test_var_node->next = node_for_integer_constant(0L, 
                                                  (an_integer_kind)ik_char);
#endif /* IA64_ABI */
  compare_node = make_operator_node((an_expr_operator_kind)eok_ieq,
                                    int_type, test_var_node);
  /* Make an "if" statement and insert it into the program. */
  insert_if_statement(compare_node, /*is_initialization_guard=*/TRUE,
                      insert_location,
#if IA64_ABI
#if IA64_ABI_USE_GUARD_ACQUIRE_RELEASE
                                       &outer_then,
#else /* !IA64_ABI_USE_GUARD_ACQUIRE_RELEASE */
                                       block_stmt,
#endif /* IA64_ABI_USE_GUARD_ACQUIRE_RELEASE */
#else /* !IA64_ABI */
                                       block_stmt,
#endif /* IA64_ABI */
                                                   insert_location2,
                      (an_insert_location *)NULL);
  /* Make "test_var = 1" and insert it inside the "if" statement. */
#if !IA64_ABI
  (void)insert_var_assignment_statement(*test_var,
                                        (an_expr_operator_kind)eok_iassign,
                                        node_for_integer_constant(1L,
                                                      (an_integer_kind)ik_int),
                                        insert_location2);
#else /* IA64_ABI */
#if IA64_ABI_USE_GUARD_ACQUIRE_RELEASE
  {
    /* To support multi-threading, make an inner
         "if (__cxa_guard_acquire(&test_var)) {
            ...
            __cxa_guard_release(&test_var);
          }"
       statement as the "then" part of the outer "if" above. Leave
       *insert_location2 ready for insertion at the ...
       This is done as two "if"s so that once the variable is
       initialized one doesn't pay the cost of calling the runtime
       routine. */
    an_expr_node_ptr acquire_node =
      make_runtime_rout_call("__cxa_guard_acquire", &guard_acquire_routine,
                             integer_type((an_integer_kind)ik_int),
                             var_lvalue_expr(*test_var));
    an_insert_location outer_block_insert_location,
                       release_insert_location;
    an_expr_node_ptr release_node =
       make_runtime_rout_call("__cxa_guard_release", &guard_release_routine,
                             void_type(),
                             var_lvalue_expr(*test_var));
    set_block_start_insert_location(outer_then, &outer_block_insert_location);
    insert_if_statement(acquire_node, /*is_initialization_guard=*/TRUE,
                        &outer_block_insert_location, block_stmt,
                        insert_location2, (an_insert_location *)NULL);
    /* Avoid moving "insert_location2" which is now the right place to
       put the initialization code. */
    release_insert_location = *insert_location2;
    (void)insert_expr_statement(release_node, &release_insert_location);
  }
#else /* !IA64_ABI_USE_GUARD_ACQUIRE_RELEASE */
  (void)insert_assignment_statement(add_cast_to_char_star(
                                                   var_lvalue_expr(*test_var)),
                                    (an_expr_operator_kind)eok_iassign,
                                    node_for_integer_constant(1L,
                                                     (an_integer_kind)ik_char),
                                    insert_location2);
#endif /* IA64_ABI_USE_GUARD_ACQUIRE_RELEASE */
#endif /* IA64_ABI */
}  /* add_first_time_test */


static void add_local_static_guard_var_cleanup(
                                 a_variable_ptr         local_static_guard_var,
                                 an_object_lifetime_ptr local_static_lifetime,
                                 an_insert_location_ptr insert_location)
/*
local_static_guard_var is the guard variable associated with the initialization
of a local static variable.  local_static_lifetime is the object lifetime
that surrounds the complete initialization.  Add a dynamic initialization
entry and associated region table entry to indicate to the runtime that
the guard variable must be reset to zero if an exception is thrown before
the initialization of the local static variable is completed.  If any
code is needed, insert it at *insert_location.
*/
{
  a_dynamic_init_ptr dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
  an_init_pos_descr  ipd;

  dip->variable = local_static_guard_var;
  dip->has_temporary_lifetime = TRUE;
  dip->is_guard_var_for_local_static_var_init = TRUE;
  add_to_end_of_destructions_list(dip, local_static_lifetime);
  dip->destructible_entity_descr = alloc_destructible_entity_descr();
  set_var_init_pos_descr(local_static_guard_var, &ipd);
  add_dyn_init_cleanup(dip, &ipd, /*set_cond_flag_if_any=*/FALSE,
                       curr_context, insert_location);
}  /* add_local_static_guard_var_cleanup */


static void adjust_cleanup_state_for_aggregate_init(
                                             a_dynamic_init_ptr dip,
                                             a_dynamic_init_ptr preceding_init,
                                             a_boolean          *some_cloned)
/*
An aggregate initialization has just been completed.  dip is a destruction
preceding that aggregate initialization (usually, one indicating a
destruction for a partial aggregate initialization), and preceding_init
indicates the initialization that precedes the start of the entire aggregate
initialization.  (It doesn't span object lifetimes, so NULL means there is
no preceding initialization in the current lifetime.)  Adjust the cleanup
state to the latest initialization that is not a partial aggregate
initialization.  When generating EH tables, some region table entries
may have to be cloned.  If any are, *some_cloned is returned TRUE.
*/
{
  check_assertion_str(dip != NULL,
                      "adjust_cleanup_state_for_aggregate_init: NULL dip");
  *some_cloned = FALSE;
  if (dip->next_in_destruction_list == preceding_init) {
    /* End of the list. */
    curr_context->latest_initialization = preceding_init;
  } else {
    /* Do a recursive call to process the rest of the list. */
    adjust_cleanup_state_for_aggregate_init(dip->next_in_destruction_list,
                                            preceding_init,
                                            some_cloned);
  }  /* if */
  if (!dip->destruction_is_for_partially_constructed_aggregate) {
#if GENERATE_EH_TABLES
    if (exceptions_enabled) {
      /* This entry is being kept, as it is for a non-aggregate initialization.
         If there are any partial aggregate initializations between this
         entry and the destruction beyond the overall aggregate initialization,
         we need to clone this entry. */
      a_destructible_entity_descr_ptr dedp = dip->destructible_entity_descr;
      a_boolean                       need_clone = FALSE;

      if (dedp->next_in_region_table != curr_context->latest_initialization) {
        /* The next destruction after this one is for a partial aggregate
           destruction, so link around the partial aggregate and clone the
           current entry. */
        need_clone = TRUE;
        dedp->cleanup_state_to_set_when_starting_destruction = 
                                     curr_context->curr_cleanup_state;
        dedp->next_in_region_table = curr_context->latest_initialization;
      } else if (*some_cloned) {
        /* Once some entry has been cloned, all those following it have
           to be cloned as well. */
        need_clone = TRUE;
      }  /* if */
      if (need_clone) {
        clone_region_table_entry_list(dip, dedp->next_in_region_table);
        *some_cloned = TRUE;
      }  /* if */
    }  /* if */
#endif /* GENERATE_EH_TABLES */
    /* Remember the latest initialization that is not a partial aggregate
       initialization. */
    curr_context->latest_initialization = dip;
  }  /* if */
  set_curr_cleanup_state_to_latest_initialization();
}  /* adjust_cleanup_state_for_aggregate_init */

#if IA64_ABI

static a_routine_ptr helper_routine_to_zero_entity(a_type_ptr type)
/*
Build a routine to zero-initialize an entity of the indicated type.
This is needed in the IA-64 ABI because pointers to data members use
-1 as the NULL value.
*/
{
  a_routine_ptr                 rp;
  a_routine_type_supplement_ptr rtsp;
  a_type_ptr                    pointer_type, count_type;
  a_memory_region_number        il_region;
  a_scope_ptr                   scope;
  an_insert_location            insert_location;
  a_generated_routine_context   context;
  a_variable_ptr                model_var, entity_var, count_var;
  a_statement_ptr               loop_stmt, assign_stmt;
  an_expr_node_ptr              entity_expr, assign_expr;
  
  /* Build the routine entry.  It has two parameters: a pointer to an entity
     of the indicated type and a count of the number of entities to
     initialize. */
  pointer_type = make_pointer_type(skip_typerefs(type));
  count_type = integer_type(targ_size_t_int_kind);
  rp = make_rout_entry((char *)NULL, (a_storage_class)sc_static,
                       void_type(), pointer_type);
  rtsp = rp->type->variant.routine.extra_info;
  rtsp->param_type_list->next = alloc_param_type(count_type);
  /* Build the definition of the routine.  */
  scope = make_routine_definition(rp, /*make_return=*/FALSE, &il_region);
  push_generated_routine_context(scope, il_region, &context);
  /* Create the parameters. */
  scope->variant.routine.parameters = entity_var = 
                     make_lowered_param_variable(rtsp->param_type_list->type);
  scope->variant.routine.parameters->next = count_var = 
               make_lowered_param_variable(rtsp->param_type_list->next->type);
  set_block_start_insert_location(scope->assoc_block, &insert_location);
  /* Build a model for the zero-initialized entity. */
  model_var = make_temporary_in_scope(type, scope, /*force_static=*/FALSE);
  model_var->init_kind = (an_init_kind)initk_zero;
  lower_initializer(model_var, &model_var->init_kind, &model_var->initializer);
  /* Build a loop to zero-initialize the entities. */
  loop_stmt = alloc_statement((a_statement_kind)stmk_while);
  loop_stmt->expr = make_operator_node((an_expr_operator_kind)eok_ipost_decr,
                                       count_type,
                                       var_lvalue_expr(count_var));
  /* Build the body of the loop. */
  entity_expr = make_operator_node((an_expr_operator_kind)eok_ipost_incr,
                                   pointer_type,
                                   var_lvalue_expr(entity_var));
  assign_expr = make_assignment_expr(entity_expr, 
                                     is_class_struct_union_type(type) ?
                                     (an_expr_operator_kind)eok_sassign :
                                     (an_expr_operator_kind)eok_iassign,
                                     var_rvalue_expr(model_var));
  assign_stmt = alloc_expr_statement(assign_expr);
  loop_stmt->variant.loop_statement = assign_stmt;
  insert_statement(loop_stmt, &insert_location);
  /* Clean up. */
  pop_generated_routine_context(scope, il_region, &context);
  return rp;
}  /* helper_routine_to_zero_entity */


static void insert_call_to_helper_routine_to_zero_entity(
                                          a_type_ptr         entity_type,
                                          an_expr_node_ptr   entity_node,
                                          an_expr_node_ptr   num_elements,
                                          an_insert_location *insert_location)
/*
Insert, at insert_location, a call to the helper routine that zero-initializes
entities of entity_type.  The entity_node is the first entity to initialize;
there are num_elements at that location.
*/
{
  entity_node->next = num_elements;
  (void)make_call_node(helper_routine_to_zero_entity(entity_type),
                       entity_node, /*honor_virtual=*/FALSE,
                       insert_location);
}  /* insert_call_to_helper_routine_to_zero_entity */

#endif /* IA64_ABI */

/*
Pointer to the routine entry for the runtime routine __memzero.  NULL until
created.
*/
static a_routine_ptr
		memzero_routine;


static void insert_call_to_zero_entity(an_expr_node_ptr   entity_node,
                                       an_expr_node_ptr   entity_size_node,
                                       an_insert_location *insert_location)
/*
Create a runtime routine call to zero the entity whose address is given
by entity_node, with size given by entity_size_node.  Insert the code at
*insert_location.
*/
{
  an_expr_node_ptr memzero_call;

#if IA64_ABI
  /* We cannot rely on "__memzero"; the ABI does not provide this routine in
     the runtime library. */
#if !__BSD__
  entity_node = add_cast_if_necessary(entity_node, void_star_type());
  entity_node->next = node_for_integer_constant(0L, (an_integer_kind)ik_int);
  entity_node->next->next = entity_size_node;
  memzero_call = make_runtime_rout_call("memset", &memzero_routine,
                                        void_star_type(), entity_node);

#else /* __BSD__ */
  entity_node = add_cast_if_necessary(entity_node, char_star_type());
  entity_node->next = entity_size_node;
  memzero_call = make_runtime_rout_call("bzero", &memzero_routine,
                                        void_type(), entity_node);
#endif /* __BSD__ */
#else /* !IA64_ABI */
  entity_node = add_cast_if_necessary(entity_node, void_star_type());
  entity_node->next = entity_size_node;
  memzero_call = make_runtime_rout_call("__memzero", &memzero_routine,
                                        void_type(), entity_node);
#endif /* !IA64_ABI */
  (void)insert_expr_statement(memzero_call, insert_location);
}  /* insert_call_to_zero_entity */


void lower_dynamic_init(a_dynamic_init_ptr     dip,
                        an_init_pos_descr_ptr  ipdp,
                        an_expr_node_ptr       implied_arg_list,
                        an_expr_node_ptr       end_implied_arg_list,
                        a_constructor_init_ptr ctor_init,
                        a_lower_dynamic_init_options_set
                                               options,
                        a_boolean              others_follow_in_aggr,
                        an_insert_location_ptr insert_location,
                        a_boolean              *keep_dynamic_init,
                        a_constant_ptr         *constant_to_keep)
/*
Do IL lowering of the indicated dynamic initialization and everything under
it.  ipdp indicates the entity to be initialized.  Ordinarily, that is the
entire variable indicated in the dynamic initialization entry (that happens
when the entry is pointed to by an stmk_init statement or when it appears
on a file-scope dynamic_inits list).  ipdp can, however, indicate a part of
an aggregate.

If implied_arg_list and end_implied_arg_list are non-NULL, they point to
the beginning and end of a list of implied arguments for a constructor
call (for implicit virtual base class arguments).

If the dynamic initialization is part of a constructor initializer,
ctor_init points to the constructor-init entry.

If the dynamic initialization is a full expression (e.g., in an
stmk_init), (options & LDIO_FULL_EXPR) is set.

If the dynamic initialization is the top-level one for a throw,
(options & LDIO_THROW) is set.

others_follow_in_aggr is TRUE if this constant is followed by others in
an aggregate initialization (i.e., it's not the last).


This routine will always generate some executable code (well, almost always: 
A dynamic initialization that contains a destructor but that could otherwise 
be rendered as a static initialization will be turned into the static
initialization, which means no code will be generated).  The code will be
inserted at *insert_location.  *insert_location will be updated to indicate 
a location after the inserted code.  This code is usually only called for
non-C code, but in C99 mode it may also be called to handle compound literals:
The caller should then make sure that this only happens in function scope
(where executable statements can be added in C mode).

On return, *keep_dynamic_init is TRUE if the dynamic init entry is to
be kept, FALSE if it should be deleted.  If the caller passes in
keep_dynamic_init == NULL, no value is returned; the value determined
in this routine must be FALSE in that case.

On return, *constant_to_keep is set to point to a constant part of the
initialization that should be kept.  If this feature is not needed,
constant_to_keep can be passed in as NULL.

When Microsoft extensions are allowed, this routine is called in C mode to
lower initialization for nonconstant aggregates.  It's also called in
C99 mode for the same reason.
*/
{
  an_expr_node_ptr   entity_node, source_node;
  a_variable_ptr     variable;
  a_boolean          simple_constant_init = FALSE, keep_constant;
  a_constant_ptr     simple_constant;
  a_source_position  saved_error_position, saved_code_pos;
  a_statement_ptr    block_stmt = NULL;
  a_type_ptr         ctor_routine_type;
  a_type_ptr         this_param_type;
  a_param_type_ptr   param;
  a_boolean          static_var_init;
  a_local_static_variable_init_ptr
                     lsvip = NULL;
  an_insert_location insert_location2;
  an_insert_location *eff_insert_location = insert_location;
  an_object_lifetime_ptr
                     init_expr_lifetime, local_static_lifetime;
  a_context          context, static_context, static_context2;
  a_context_ptr      eff_context = curr_context;
  a_boolean          expr_is_lvalue, local_keep_dynamic_init = FALSE;
  a_boolean          constructor_array_init = FALSE;
  a_variable_ptr     local_static_guard_var;
  a_boolean          do_simple_constant_init_opt = FALSE;
  a_boolean          local_static_promoted_out_of_extern_inline = FALSE;
  a_dynamic_init_ptr latest_initialization_on_entry;

  saved_code_pos = code_pos_for_lowering;
  saved_error_position = error_position;
  if (constant_to_keep != NULL) *constant_to_keep = NULL;
  variable = dip->variable;
  if (variable != NULL) {
    /* Whole-variable initialization. */
    /* Track the source position. */
    if (variable->source_corresp.decl_position.seq != 0) {
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
  if (variable != NULL) {
    /* Decide whether the optimization of rewriting a dynamic initialization
       to a constant as a static initialization to the constant is allowed.
       It is not allowed if the variable is automatic and the context is
       something other than an stmk_init (that's the keep_dynamic_init
       test). */
    if (dip->kind == (a_dynamic_init_kind)dik_constant &&
        (static_var_init || keep_dynamic_init != NULL)) {
      do_simple_constant_init_opt = TRUE;
    }  /* if */
    /* See if this is a local static variable promoted out of an extern inline
       function (or template instantiated wherever used). */
    if (variable->promoted_local_static &&
        variable->storage_class == (a_storage_class)sc_unspecified) {
      local_static_promoted_out_of_extern_inline = TRUE;
      /* Don't allow this case to be turned into a simple constant
         initialization, because we want the variable to be a tentative
         definition (and therefore it must be uninitialized). */
      do_simple_constant_init_opt = FALSE;
    }  /* if */
    /* For local static variables, find the associated local static variable
       initialization entry. */
    if (variable->init_kind == (an_init_kind)initk_function_local) {
      lsvip = find_local_static_variable_init(variable, curr_context->scope);
    } else if (variable->promoted_local_static_init) {
      /* This is an initialized local static variable that has already been
         promoted to the file scope (see
         promote_static_variables_out_of_function).  Its local static
         initialization entry was unlinked and saved on a list. */
      for (lsvip = promoted_local_static_variable_inits;
           lsvip != NULL;
           lsvip = lsvip->next) {
        if (lsvip->variable == variable) break;
      }  /* for */
      check_assertion_str(lsvip != NULL,
                          "lower_dynamic_init: local static init not found");
    }  /* if */
    /* For local static variables, add a first-time flag and a test,
       but not if the initialization will be turned into a constant
       initialization. */
    if (lsvip != NULL && !do_simple_constant_init_opt) {
      insert_location2 = *insert_location;
      add_first_time_test(variable, &insert_location2, insert_location,
                          &block_stmt, &local_static_guard_var);
    }  /* if */
  }  /* if */
  if (dip->lifetime != NULL) {
    an_object_lifetime_ptr lifetime = dip->lifetime;
    /* This dynamic initialization is on the destructions list of an
       object lifetime, so it must indicate a destruction.  Activate
       the right object lifetime if it's not the current one. */
    if (curr_object_lifetime == lifetime) {
      /* The current lifetime is the right one. */
    } else if (lifetime->kind == (an_object_lifetime_kind)olk_function_static){
      /* For local static initializations, make the function static lifetime
         the effective lifetime. */
      push_context(&static_context, (a_scope_ptr)NULL, lifetime);
      eff_context = curr_context;
      /* Pop the context and object lifetime off the stack, but keep them
         around and use them as the effective context. */
      pop_context();
    } else if (curr_object_lifetime->kind ==
                                 (an_object_lifetime_kind)olk_expr_temporary &&
               curr_object_lifetime->parent_lifetime == lifetime) {
      /* This is a case where a temporary has had its lifetime extended because
         a reference was bound to it.  The temporary is in a lifetime outside
         of the current one, and a context outside the current one. */
      eff_context = context_for_lifetime(lifetime);
    } else if (processing_file_scope_init_routine &&
               lifetime->kind == (an_object_lifetime_kind)olk_global_static) {
      /* Initialization of a global variable from inside the routine
         generated for file-scope initializations. */
      eff_context = context_for_lifetime(lifetime);
    } else {
      unexpected_condition_str(
     "lower_dynamic_init: dynamic init has lifetime other than curr lifetime");
    }  /* if */
  }  /* if */
  local_static_lifetime = NULL;
  if (lsvip != NULL) {
    /* Local static variable.  If it has an associated lifetime, push that. */
    local_static_lifetime = lsvip->lifetime;
    if (local_static_lifetime != NULL) {
      push_context(&static_context2, (a_scope_ptr)NULL, local_static_lifetime);
      begin_object_lifetime(local_static_lifetime, insert_location);
      unbind_object_lifetime(local_static_lifetime);
      if (keep_object_lifetime_info_in_lowered_il) {
        bind_object_lifetime(local_static_lifetime, iek_block,
                             (char *)block_stmt->variant.block.extra_info);
      }  /* if */
      if (exceptions_enabled) {
        /* Add a dynamic init entry to represent the conditional flag.  This
           is turned into a region table entry that indicates that the
           conditional flag must be cleared if an exception is thrown before
           the initialization is completed. */
        add_local_static_guard_var_cleanup(local_static_guard_var,
                                           local_static_lifetime,
                                           insert_location);
      }  /* if */
    }  /* if */
  }  /* if */
  init_expr_lifetime = dip->init_expr_lifetime;
  /* See if this is an initialization of an array via a constructor.  For
     such initializations certain things get delayed because the actual
     initialization gets done by a runtime routine. */
  if (dip->kind == (a_dynamic_init_kind)dik_constructor &&
      ipdp->array_element_sequence) {
    constructor_array_init = TRUE;
    /* The init_expr_lifetime comes up in this case only if default arguments
       of the constructor require temporaries.  Leave that lifetime to be
       handled in default_version_of_routine. */
    init_expr_lifetime = NULL;
  }  /* if */
  if (init_expr_lifetime != NULL) {
    /* The dynamic init defines a lifetime that surrounds the
       initialization.  Push that lifetime onto the context stack. */
    push_init_expr_lifetime(&init_expr_lifetime,
                            processing_file_scope_init_routine,
                            &context,
                            insert_location,
                            &insert_location2,
                            &eff_insert_location);
  }  /* if */
  if (processing_file_scope_init_routine) {
    /* When processing an initialization in the file-scope initialization
       routine, the expressions pointed to are in the file scope, but we
       want to use them in the function scope, so copy them.  Note that
       (a) this must be done before they are lowered (so the temporaries
       have not yet been made into variables), and (b) this copies the
       object lifetimes too.  Also note that these entries will have been
       copied already if they're inside a higher-level initialization
       that has already been copied. */
    if (dip->kind == (a_dynamic_init_kind)dik_expression ||
        dip->kind == (a_dynamic_init_kind)dik_call_returning_class_via_cctor) {
      an_expr_node_ptr expr = dip->variant.expression;
      if (in_file_scope(expr)) {
        dip->variant.expression = copy_expr_to_function_memory_region(expr);
      }  /* if */
    } else if (dip->kind == (a_dynamic_init_kind)dik_constructor) {
      /* Don't copy for the constructor array case; a copy will be done later
         for that, so a copy here would be redundant. */
      if (!constructor_array_init) {
        an_expr_node_ptr expr_list = dip->variant.constructor.args;
        if (expr_list != NULL && in_file_scope(expr_list)) {
          dip->variant.constructor.args =
                       copy_list_of_expr_trees(expr_list,
                                               CE_UNLINK_SOURCE_DESTRUCTIONS |
                                               CE_TRANSFER_DESTR_ENTITY_DESCR);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (init_expr_lifetime != NULL) {
    /* Begin the object lifetime defined by this initialization.  Note that
       this is done late so that when processing the file-scope initialization
       routine (a) the lifetime has been copied and (b) any lifetimes under
       this one have been copied and attached to it. */
    /* Restore the pointer from the dynamic init to the lifetime, which is
       required for some processing when removing destructions that aren't
       needed.  The pointer was cleared when the object lifetime was
       rebound to a block statement because the dynamic initialization
       entry is not going to stay in the IL. */
    an_object_lifetime_ptr saved_init_expr_lifetime = dip->init_expr_lifetime;
    dip->init_expr_lifetime = init_expr_lifetime;
    begin_object_lifetime(init_expr_lifetime, eff_insert_location);
    dip->init_expr_lifetime = saved_init_expr_lifetime;
  }  /* if */
  switch (dip->kind) {
    case dik_none:
      break;
    case dik_zero:
      /* Initialize to zero. */
      if (variable != NULL) {
        /* Entire variable initialized to zero.  Do nothing here.
           Processing is below (setting init_kind to initk_zero). */
      } else {
        /* Not entire variable. */
        a_type_ptr entity_type = type_from_init_pos_descr(ipdp);
#if IA64_ABI
        if (contains_ptr_to_data_member(entity_type)) {
          /* If the entity type contains pointers to data members they must
             be initialized to -1, not zero, for the IA-64 ABI. */
          a_type_ptr       element_type;
          a_targ_size_t    num_elements;
          if (is_array_type(entity_type)) {
            element_type = underlying_array_element_type(entity_type);
            num_elements = num_array_elements(entity_type);
          } else {
            element_type = entity_type;
            num_elements = 1;
          } /* if */
          if (ipdp->array_element_sequence) {
            num_elements *= ipdp->array_element_count;
          }  /* if */
          entity_node = make_init_entity_node(ipdp, 
                                              /*using_as_address=*/TRUE,
                                              /*using_as_dest=*/TRUE);
          insert_call_to_helper_routine_to_zero_entity(
                   element_type,
                   entity_node,
                   node_for_host_large_integer(
                                            (a_host_large_integer)num_elements,
                                            targ_size_t_int_kind),
                   eff_insert_location);
        } else
#endif /* IA64_ABI */
        /* Do not insert code here.  */
        if (is_aggregate_or_union_type(entity_type) ||
            is_or_was_ptr_to_member_function_type(entity_type) ||
            ipdp->array_element_sequence) {
          /* Aggregate.  Use a runtime routine call to zero it. */
          a_targ_size_t    entity_size;
          an_expr_node_ptr entity_size_node;
          entity_node = make_init_entity_node(ipdp, /*using_as_address=*/TRUE,
                                              /*using_as_dest=*/TRUE);
          entity_size = f_skip_typerefs(entity_type)->size;
          if (ipdp->array_element_sequence) {
            /* For a sequence of array elements, multiply by the number of
               elements. */
            check_assertion_str(ipdp->array_element_count > 0,
                      "lower_dynamic_init: dik_zero array_element_count <= 0");
            entity_size *= ipdp->array_element_count;
          }  /* if */
          entity_size_node = node_for_host_large_integer(
                                             (a_host_large_integer)entity_size,
                                             targ_size_t_int_kind);
          insert_call_to_zero_entity(entity_node, entity_size_node,
                                     eff_insert_location);
        } else {
          /* Setting a scalar to zero; can be done by an assignment. */
          goto do_assignment;
        }  /* if */
      }  /* if */
      break;
    case dik_constant:
      /* Assign a constant to the entity to be initialized. */
      if (C_mode()) {
#if DO_C99_IL_LOWERING
        if (c99_mode || gcc_mode) {
          /* When lowering C99 code, use the C99 lowering routines. */
          lower_c99_constant(dip->variant.constant);
        }  /* if */
#endif /* DO_C99_IL_LOWERING */
      } else {
        /* C++ mode. */
        lower_constant(dip->variant.constant);
      }  /* if */
      /* If there is a whole variable of the right kind, this dynamic
         initialization can be rendered as a static initialization. */
      if (do_simple_constant_init_opt) {
        simple_constant_init = TRUE;
        simple_constant = dip->variant.constant;
        break;
      }  /* if */
      /* For the normal cases, go on and generate an assignment. */
      goto do_assignment;
    case dik_expression:
      /* Assign an expression to the entity to be initialized. */
      /* Lower the source expression. */
      source_node = dip->variant.expression;
      if (C_mode()) {
#if DO_C99_IL_LOWERING
        if (c99_mode || gcc_mode) {
          /* When lowering C99 code, use the C99 lowering routines. */
          if (options & LDIO_FULL_EXPR) {
            lower_c99_full_expr(source_node);
          } else {
            lower_c99_expr(source_node, /*used_as_lvalue=*/FALSE);
          }  /* if */
        }  /* if */
#endif /* DO_C99_IL_LOWERING */
      } else {
        /* It's an lvalue if the thing being initialized is a reference. */
        expr_is_lvalue = is_reference_type(type_from_init_pos_descr(ipdp));
        if ((options & LDIO_FULL_EXPR) && init_expr_lifetime == NULL) {
          lower_full_expr(source_node, expr_is_lvalue, (a_statement_ptr)NULL);
        } else {
          /* Normal case: not a full expression. */
          lower_expr(source_node, expr_is_lvalue);
        }  /* if */
      }  /* if */
do_assignment:;
      check_assertion_str(!ipdp->array_element_sequence,
                          "lower_dynamic_init: repeated const or expr init");
      /* Make a node for the entity to be initialized. */
      entity_node = make_init_entity_node(ipdp, /*using_as_address=*/FALSE,
                                          /*using_as_dest=*/TRUE);
      add_init_assignment(dip, (a_constant *)NULL, entity_node,
                          eff_insert_location);
      break;
    case dik_call_returning_class_via_cctor:
      /* Initialize the entry by calling a routine that returns its result
         via a copy constructor. */
      /* The address of the temporary being initialized is added as an
         implicit argument of the call. */
      lower_call(dip->variant.expression, ipdp, (a_statement_ptr)NULL);
      (void)insert_expr_statement_set_pos(dip->variant.expression,
                                          eff_insert_location);
      break;
    case dik_constructor:
      /* Initialize the entity by calling a constructor. */
      /* The routine does not need to be lowered from here. */
      /* Make a node for the entity to be initialized. */
      entity_node = make_init_entity_node(ipdp, /*using_as_address=*/TRUE,
                                          /*using_as_dest=*/TRUE);
      /* Cast the entity node pointer to the right type to eliminate
         qualifier and type-as-subobject differences. */
      ctor_routine_type = dip->variant.constructor.ptr->type;
      ctor_routine_type = skip_typerefs(ctor_routine_type);
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
#if IA64_ABI
        { a_routine_ptr ctor_routine = dip->variant.constructor.ptr;
          if (ctor_routine->ctor_dtor_kind == 
                                          (a_ctor_or_dtor_kind)cdk_subobject &&
              type_pointed_to(this_param_type)->variant.class_struct_union.
                                                   any_virtual_base_classes) {
            /* Skip the VTT parameter. */
            param = param->next;
          }  /* if */
        }
#endif /* IA64_ABI */
        source_node = add_cast_if_necessary(source_node,
                                            make_pointer_type(
                                                type_pointed_to(param->type)));
        /* Leave the parameter pointer set for lowering any additional
           arguments below. */
        param = param->next;
      }  /* if */
      if (ipdp->array_element_sequence) {
        /* Construct a sequence of array elements. */
#if CHECKING
        if (implied_arg_list != NULL) {
          internal_error("lower_dynamic_init: implied arg list for array");
        }  /* if */
#endif /* CHECKING */
        /* Note that dip->variant.constructor.args has not been lowered,
           which is what the subroutine requires. */
        add_array_constructor_call(dip, entity_node, source_node,
                                   ipdp->array_element_count,
                                   eff_insert_location);
      } else {
        /* Construct a simple entity (not an array). */
        /* Lower any added arguments. */
        lower_arg_expr_list(dip->variant.constructor.args, ctor_routine_type,
                            param);
#if ABI_COMPATIBILITY_VERSION >= 233
        if (exceptions_enabled && (options & LDIO_THROW) &&
            dip->variant.constructor.is_implicit_copy_for_copy_initialization){
          /* This is the top-level copy of a throw, and it does the implied
             copy constructor call to copy the object to the runtime.  This is
             considered "inside" the throw, so we need to add code to tell the
             runtime that. */
          an_expr_node_ptr arg_node = dip->variant.constructor.args;
          check_assertion(arg_node != NULL);
          if (!is_invariant_expr(arg_node, /*vars_can_change=*/FALSE)) {
            /* The source node can have side effects, so evaluate it before
               the exception is considered started and use a temporary with
               its value in the actual copy constructor call. */
            an_expr_node_ptr arg_node_next = arg_node->next;
            (void)insert_expr_statement_set_pos(arg_node, eff_insert_location);
            arg_node = assign_expr_to_temp_and_make_expr_for_reuse(arg_node);
            arg_node->next = arg_node_next;
            dip->variant.constructor.args = arg_node;
          }  /* if */
          record_exception_started(eff_insert_location);
        }  /* if */
#endif /* ABI_COMPATIBILITY_VERSION >= 233 */
        /* Generate the constructor call. */
        add_constructor_call(dip, entity_node, source_node,
                             implied_arg_list, end_implied_arg_list,
                             eff_insert_location);
      }  /* if */
      break;
    case dik_nonconstant_aggregate:
      /* Initialization with a nonconstant aggregate constant.  This is usually
         a whole-variable initialization, but can be used in a ctor-initializer
         to iterate over an array initialization, etc. */
      if (!C_mode()) {
        latest_initialization_on_entry = eff_context->latest_initialization;
      }  /* if */
      keep_constant = FALSE;
      lower_dynamic_init_aggregate_constant(dip->variant.constant, ipdp,
                                            /*dtor_case=*/FALSE, ctor_init,
                                            others_follow_in_aggr,
                                            eff_insert_location,
                                            &keep_constant);
      if (keep_constant) {
        /* There is a constant part of the initialization to be kept. */
        if (variable == NULL) {
          /* There is no variable, so we are down inside an aggregate
             initialization.  Pass this constant back to the caller. */
          check_assertion(constant_to_keep != NULL);
          *constant_to_keep = dip->variant.constant;
        } else {
          /* Keep a (now-)constant aggregate value as the static initial value
             of the variable.  The nonconstant parts have been put out as
             code and replaced with placeholder constants. */
          simple_constant_init = TRUE;
          simple_constant = dip->variant.constant;
          if (local_static_promoted_out_of_extern_inline) {
            /* A static variable of an extern inline function initialized
               to a constant.  The constant is the constant part of the
               nonconstant aggregate.  Insert an assignment to set the variable
               to the constant, preceding any generated initialization code.
               This is done because we want the variable to be a tentative
               definition, which means it must be uninitialized. */
            a_variable_ptr   temp_var;
            an_expr_node_ptr init_val_node;
            set_block_start_insert_location(block_stmt, &insert_location2);
            entity_node = make_init_entity_node(ipdp,
                                                /*using_as_address=*/FALSE,
                                                /*using_as_dest=*/TRUE);
            check_assertion(simple_constant->kind ==
                            (a_constant_repr_kind)ck_aggregate);
            temp_var = make_lowered_temporary(variable->type);
            temp_var->init_kind = (an_init_kind)initk_static;
            temp_var->initializer.constant = simple_constant;
            check_assertion(in_file_scope(simple_constant) ==
                            in_file_scope(temp_var));
            init_val_node = var_lvalue_expr(temp_var);
            (void)insert_assignment_statement(entity_node,
                                            (an_expr_operator_kind)eok_bassign,
                                              init_val_node,
                                              &insert_location2);
            variable->init_kind = (an_init_kind)initk_none;
            variable->initializer.constant = NULL;
            simple_constant_init = FALSE;
          }  /* if */
        }  /* if */
      }  /* if */
      break;
    case dik_bitwise_copy:
      /* Bitwise copy of a value.  The source location is implied.
         This is used for copying members of classes in ctor-initializers
         of copy constructors, and for the parameter of catch clauses.
         ctor_init is non-NULL for the first of those cases. */
      add_bitwise_copy(ipdp, ctor_init, eff_insert_location);
      break;
#if CHECKING
    default:
      internal_error("lower_dynamic_init: bad kind");
#endif /* CHECKING */
  }  /* switch */
  /* If the dynamic init entry indicates a destructor call, it requires
     processing to get the destruction done at the right time. */
  if (dip->destructor != NULL) {
    if (static_var_init &&
        !dip->destruction_is_for_partially_constructed_aggregate) {
      /* For static variables (local or global), generate code to record
         at runtime the need for a destruction later. */
      record_needed_destruction(dip, ipdp, eff_insert_location);
    } else {
      /* Initializations of nonstatic variables. */
      a_destructible_entity_descr_ptr dedp = dip->destructible_entity_descr;
      check_assertion_str(dedp != NULL, "lower_dynamic_init: missing dedp");
      dedp->initialization_done = TRUE;
      if (dip->kind == (a_dynamic_init_kind)dik_nonconstant_aggregate &&
          !C_mode() &&
          latest_initialization_on_entry !=
                                          eff_context->latest_initialization) {
        /* This is an aggregate for which some partial-aggregate
           initializations were done.  Adjust the cleanup state now that
           the entire aggregate is completed. */
        a_boolean some_cloned;

        adjust_cleanup_state_for_aggregate_init(dip->next_in_destruction_list,
                                                latest_initialization_on_entry,
                                                &some_cloned);
      }  /* if */
      if (dip->destruction_is_for_partially_constructed_aggregate &&
          !others_follow_in_aggr) {
        /* A cleanup entry is not needed for a partial initialization
           in an aggregate if it is not followed by anything else, because
           there is no code executed after the partial initialization and
           before the initialization is completed where an exception could be
           thrown. */
      } else {
        /* Update the cleanup information so that this entity will be
           destroyed at the appropriate time. */
        /* Do not set the conditional flag to TRUE for virtual base
           class constructor inits; the flag is shared among all virtual
           base class initializations and is already set. */
        add_dyn_init_cleanup(dip, ipdp,
                             /*set_cond_flag_if_any=*/(ctor_init == NULL),
                             eff_context, eff_insert_location);
      }  /* if */
    }  /* if */
    if (dip->lifetime == NULL && dip->destructible_entity_descr != NULL) {
      /* If the dynamic initialization has been removed from its lifetime
         (because the cleanup has been handled some other way), free the
         destructible entity description entry now.  The normal freeing
         process finds the entries by walking the object lifetime tree,
         but this dynamic initialization isn't in the tree anymore. */
      free_destructible_entity_descr(dip->destructible_entity_descr);
      dip->destructible_entity_descr = NULL;
    }  /* if */
  }  /* if */
  /* If the dynamic init defines a lifetime that surrounds the initialization,
     pop the context for that lifetime. */
  if (init_expr_lifetime != NULL) {
    gen_cleanup_actions(init_expr_lifetime, eff_insert_location);
    pop_context();
  }  /* if */
  /* If this is the initialization of a local static variable and a lifetime
     surrounds that, pop the lifetime. */
  if (local_static_lifetime != NULL) {
    gen_cleanup_actions(local_static_lifetime, eff_insert_location);
    pop_context();
  }  /* if */
  /* In the whole-variable cases, adjust the initialization specified in
     the variable (it points to the dynamic init entry). */
  if (variable != NULL) {
    if (simple_constant_init) {
      /* Initialization to a simple constant, including a fully-constant
         aggregate. */
      if (static_var_init) {
        /* Initialization of a static variable to a constant.  Can be
           done as a static initialization. */
        /* If this variable is a local static variable that was promoted
           to file scope, we have to copy the remaining constant to the file
           scope (it was formerly pointed to by a local-static-variable-init
           entry in the function scope, and then the variable was promoted
           by promote_local_entities_to_file_scope). */
        if (!in_file_scope((char *)simple_constant)) {
          a_boolean  saved_flag_value = initial_value_for_il_lowering_flag;

          a_memory_region_number region_to_switch_back_to = NULL_region_number;
          switch_to_file_scope_region(&region_to_switch_back_to);
          /* Make sure the copy is created with flags indicating it
             has not been lowered yet. */
          initial_value_for_il_lowering_flag = FALSE;
          simple_constant = copy_unshared_constant(simple_constant);
          initial_value_for_il_lowering_flag = saved_flag_value;
          switch_back_to_original_region(region_to_switch_back_to);
        }  /* if */
        variable->init_kind = (an_init_kind)initk_static;
        variable->initializer.constant = simple_constant;
      } else {
        /* Initialization of an automatic variable to a constant.  Can be done
           by keeping the dynamic init entry. */
        local_keep_dynamic_init = TRUE;
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
      if ((static_var_init && !variable->source_corresp.is_local_to_function &&
           force_variable_definition_via_zeroing && !C_mode() &&
           !local_static_promoted_out_of_extern_inline) ||
          variable->is_partially_initialized) {
        variable->init_kind = (an_init_kind)initk_zero;
#if IA64_ABI
        /* Check for the need to generate code to zero pointers to data
           members. */
        lower_initializer(variable, &variable->init_kind,
                          &variable->initializer);
#endif /* IA64_ABI */
      } else {
        variable->init_kind = (an_init_kind)initk_none;
      }  /* if */
    }  /* if */
  }  /* if */
  if (!local_keep_dynamic_init) {
    /* Clear the initialization part of the dynamic init now that it has
       been rewritten.  This is important because the dynamic init may
       stay in the IL tree attached to an object lifetime destructions
       list, and we don't want to walk the obsolete initializations when
       we walk the tree. */
    set_dynamic_init_kind(dip, (a_dynamic_init_kind)dik_none);
  }  /* if */
  error_position = saved_error_position;
  code_pos_for_lowering = saved_code_pos;
  if (keep_dynamic_init != NULL) {
    *keep_dynamic_init = local_keep_dynamic_init;
  } else {
    check_assertion_str(!local_keep_dynamic_init,
   "lower_dynamic_init: keep_dynamic_init param NULL and want to return TRUE");
  }  /* if */
}  /* lower_dynamic_init */


void lower_constant_init_of_static_in_extern_inline(a_variable_ptr variable,
                                                    a_scope_ptr    scope)
/*
The given variable is a local static variable of an extern inline function
(or a template instantiated wherever used) that is initialized to a constant.
Rewrite its initialization as executable code so that the variable (already
promoted to the file scope and made external) can be a tentative definition
(i.e., uninitialized). scope is the scope in which the variable's definition
appears.
*/
{
  a_constant_ptr        constant;
  an_insert_location    insert_location;
  an_expr_operator_kind op;
  an_expr_node_ptr      source_node;
  a_statement_ptr       block_stmt, assign_stmt;
  a_variable_ptr        test_var;
  a_source_position     saved_error_position, saved_code_pos;

  check_assertion(variable->storage_class == (a_storage_class)sc_unspecified &&
                  variable->init_kind == (an_init_kind)initk_static);
  saved_code_pos = code_pos_for_lowering;
  saved_error_position = error_position;
  code_pos_for_lowering = error_position =
                                        variable->source_corresp.decl_position;
  constant = variable->initializer.constant;
  /* Make sure pointers-to-members in the constant get lowered when the
     file scope is lowered. */
  possibly_add_orphaned_file_scope_il_entry((char *)constant, iek_constant);
  variable->init_kind = (an_init_kind)initk_none;
  /* The general strategy is to add an assignment that copies the constant
     value into the variable. */
  if (constant->kind != (a_constant_repr_kind)ck_aggregate &&
      !is_array_type(variable->type)) {
    /* For the simple, non-aggregate case, the constant can be assigned
       directly. */
    source_node = make_node_for_il_constant(constant);
    op = lowered_assignment_operator(variable->type);
  } else {
    /* For aggregate cases, create an unnamed temporary that
       gets the original initialization, then use an eok_bassign to
       copy that to the initial variable.  This avoids taking the
       address of an aggregate constant, which is not allowed in the
       IL (except for string literals). */
    a_variable_ptr temp_var = make_file_scope_temporary(variable->type);
    temp_var->init_kind = (an_init_kind)initk_static;
    temp_var->initializer.constant = constant;
    op = (an_expr_operator_kind)eok_bassign;
    source_node = var_lvalue_expr(temp_var);
  }  /* if */
  /* The WP [stmt.dcl] paragraph 3 says "A local object of POD type with
     static storage duration initialized with constant-expressions is
     initialized before its block is first entered."  Non-POD type
     variables can also be initialized early in some cases.
     So we put the assignment at the start of the block in which the
     variable is declared. */
  set_block_start_insert_location(scope->assoc_block, &insert_location);
  /* Put a first-time test around the initialization. */
  add_first_time_test(variable, &insert_location, &insert_location,
                      &block_stmt, &test_var);
  assign_stmt = insert_assignment_statement(var_lvalue_expr(variable),
                                            op, source_node,
                                            &insert_location);
  set_stmt_pos_to_code_pos_for_lowering(assign_stmt);
  variable->initialization_rewritten_as_assignment = TRUE;
  error_position = saved_error_position;
  code_pos_for_lowering = saved_code_pos;
}  /* lower_constant_init_of_static_in_extern_inline */


static void lower_destructor_dynamic_init(
                                   a_dynamic_init_ptr     dip,
                                   an_init_pos_descr_ptr  ipdp,
                                   a_boolean              have_complete_object,
                                   an_expr_node_ptr       vtt_addr_node,
                                   an_insert_location_ptr insert_location)
/*
Do IL lowering of a destruction indicated in a dynamic initialization entry
attached to a constructor_init in a destructor.  dip points to the dynamic
initialization, and ipdp identifies the entity to be destroyed.
If have_complete_object is TRUE, the entity being destroyed is a
complete object.  If vtt_addr_node is not NULL, it is the expression for the
virtual table table pointer that should be passed to the destructor.  The
statements are inserted at *insert_location and *insert_location is updated.
*/
{
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
  if (exceptions_enabled) {
    a_destructible_entity_descr_ptr dedp = dip->destructible_entity_descr;
    /* Put the entity position in the destructible_entity_descr. */
    copy_init_pos_descr(ipdp, &dedp->init_pos_descr);
    /* Set the cleanup state to what it should be after the destruction,
       because as soon as we start the destruction it's the destructor's
       job to deal with partial destruction.  Note that this is not done
       when exceptions are not enabled, because dedp is NULL in that case,
       and curr_context->curr_cleanup_state need not be maintained. */
    curr_context->curr_cleanup_state =
                          dedp->cleanup_state_to_set_when_starting_destruction;
    insert_code_to_indicate_cleanup_state(curr_context->curr_cleanup_state,
                                          insert_location,
                                          /*unreachable=*/FALSE);
  }  /* if */
  add_destructor_call(dip->destructor, ipdp, have_complete_object,
                      vtt_addr_node, insert_location);
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


static an_expr_node_ptr copy_arg_list_for_placement_delete(
                                                an_expr_node_ptr orig_arg_list)
/*
Make a copy of the indicated argument list (for a placement new call) to
be used for a placement delete call, and return a pointer to it.  Each
argument in the original list is assigned to a temporary, and the temporary
is referenced in the second list.  (If an argument expression is invariant,
no temporary is needed; a copy is made.)
*/
{
  an_expr_node_ptr arg_list = NULL, end_arg_list = NULL, orig_arg, arg;

  for (orig_arg = orig_arg_list; orig_arg != NULL; orig_arg = orig_arg->next) {
    arg = make_reusable_copy(orig_arg, /*vars_can_change=*/TRUE);
    if (arg_list == NULL) {
      arg_list = arg;
    } else {
      end_arg_list->next = arg;
    }  /* if */
    end_arg_list = arg;
  }  /* for */
  return arg_list;
}  /* copy_arg_list_for_placement_delete */

#if !IA64_ABI

/*
Variable entry for the runtime global variable __array_new_prefix_size,
which gives the size in bytes of the array allocation prefix.  NULL until
created.  Used only with ABI_CHANGES_FOR_PLACEMENT_DELETE set to TRUE.
*/
static a_variable_ptr
		array_new_prefix_size_var;

#endif /* !IA64_ABI */

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
  a_routine_ptr               new_routine = ndsp->routine;
  a_type_ptr                  array_type, elem_type, ptr_elem_type;
  an_expr_node_ptr            entity_node, new_node, compare_node;
  an_expr_node_ptr            assign_node, num_elem_node, vec_new_node;
  a_constant                  null_constant;
  a_variable_ptr              temp_var, zero_temp_var;
  a_constant                  num_elem_constant;
  a_boolean                   preserve_size_node;
  a_targ_size_t               elem_size;
  an_expr_node_ptr            size_node, constant_node, nonconstant_node;
  a_constant                  size_constant;
  a_targ_size_t               con_for_size;
  a_boolean                   ovflo;
  a_routine_ptr               ctor_routine, dtor_routine, delete_routine;
  an_insert_location          insert_location;
#if ABI_CHANGES_FOR_PLACEMENT_DELETE
  an_expr_node_ptr            delete_args = NULL;
#endif /* ABI_CHANGES_FOR_PLACEMENT_DELETE */
  a_boolean                   zero_storage = FALSE;
#if ABI_CHANGES_FOR_PLACEMENT_DELETE
  an_expr_node_ptr            prefix_size_node = NULL;
#endif /* ABI_CHANGES_FOR_PLACEMENT_DELETE */

  /* Get the array element type. */
  array_type = skip_typerefs(ndsp->type);
  elem_type = new_delete_base_type_from_operation_type(ndsp->type);
  ptr_elem_type = make_pointer_type(elem_type);
  set_expr_creation_insert_location(&insert_location);
  /* Build the node for the address of the array (entity_node). */
#if !NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE
 #error -- NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE wrong
#endif /* !NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE */
  if (!ndsp->placement_new) {
    /* This is a normal (not placement) new, the usual case.  The __vec_new
       routine should do the allocation of the array. */
    /* Note that new_routine might be non-NULL here, if the allocation
       requires a non-default "operator new[]" i.e., a class-specific one.
       __array_new will be called, and is given a pointer to the allocation
       routine to use. */
    entity_node = NULL;  /* Allocate in __vec_new. */
    /* Lower "arg" even though it is usually ignored.  It is used when the
       array size is nonconstant.  Note that it is not necessary to lower
       this as an argument list because it will not be used directly as
       such (pieces might be put into an argument list). */
    lower_expr_list(ndsp->arg, 0, 0);
    preserve_size_node = FALSE;
  } else {
    /* This is a placement new, so the allocation must be done before
       calling the __vec_new routine.  This happens for something like
         A *p = new (x, y, z) A[3];
       The "new" call is assigned to a temporary, and entity_node uses
       the temporary, as in
         ((temp = (type *)new-call(...)) != NULL ?
                                 (type *)__vec_new(temp, ...) : NULL)
    */
    /* Prepare the argument list for the "new" call. */
    check_assertion_str(new_routine != NULL,
                       "lower_array_new: placement new with null new_routine");
    lower_arg_expr_list(ndsp->arg, new_routine->type,
                        (a_param_type_ptr)NULL);
#if ABI_CHANGES_FOR_PLACEMENT_DELETE
    if (dip != NULL && ndsp->freeing_of_storage_on_exception != NULL) {
      /* This is a placement new for which there is a corresponding placement
         delete.  Make a copy of the argument list for the new call, to
         be used in the delete call.  Note that this is done after IL lowering,
         so the argument expressions are evaluated only once.  But that
         also means temporaries used to pass class objects via copy
         constructor are shared. */
      /* Note that the copy skips the first argument (the size). */
      delete_args = copy_arg_list_for_placement_delete(ndsp->arg->next);
    }  /* if */
#endif /* ABI_CHANGES_FOR_PLACEMENT_DELETE */
    size_node = ndsp->arg;
#if ABI_CHANGES_FOR_PLACEMENT_DELETE
    /* Add the size of the runtime prefix used to keep track of the array
       size to the argument for the operator new[] call. */
    { an_expr_node_ptr size_node_next = size_node->next;

#if !IA64_ABI
      if (array_new_prefix_size_var == NULL) {
        /* Create the variable for the runtime __array_new_prefix_size
           variable. */
        array_new_prefix_size_var =
                      make_lowered_variable("__array_new_prefix_size",
                                            /*already_il_name=*/FALSE,
                                            integer_type(targ_size_t_int_kind),
                                            (a_storage_class)sc_extern);
      }  /* if */
      prefix_size_node = var_rvalue_expr(array_new_prefix_size_var);
      prefix_size_node = add_cast_if_necessary(prefix_size_node,
                                               size_node->type);
#else /* IA64_ABI */
      prefix_size_node = get_array_new_padding(elem_type, new_routine,
                                               /*even_if_zero=*/FALSE);
      if (prefix_size_node != NULL) {
#endif /* IA64_ABI  */
        size_node->next = prefix_size_node;
        size_node = make_operator_node((an_expr_operator_kind)eok_iadd,
                                       size_node->type, size_node);
        size_node->next = size_node_next;
#if IA64_ABI
      }  /* if */
#endif /* IA64_ABI */
    }
#endif /* ABI_CHANGES_FOR_PLACEMENT_DELETE */
    /* Make the "new" call. */
    new_node = make_call_node(new_routine, size_node,
                              /*honor_virtual=*/FALSE,
                              (an_insert_location *)NULL);
    /* Make "temp = (type *)new-call(...)". */
    temp_var = make_local_temporary(ptr_elem_type);
    assign_node = make_var_assignment_expr(temp_var,
                                           (an_expr_operator_kind)eok_passign,
                                           add_cast_if_necessary(new_node,
                                                               ptr_elem_type));
    /* Add the != NULL test. */
    make_zero_of_proper_type(ptr_elem_type, &null_constant);
    assign_node->next = alloc_node_for_constant(&null_constant);
    compare_node = make_operator_node((an_expr_operator_kind)eok_pne,
                                      integer_type((an_integer_kind)ik_int),
                                      assign_node);
#if ABI_CHANGES_FOR_PLACEMENT_DELETE
    /* Add the array prefix size to get from the address returned to
       the actual starting address of the array. */
#if IA64_ABI
    if (prefix_size_node != NULL) 
#endif /* IA64_ABI */
    { an_expr_node_ptr temp_var_node, add_node;

      /* Make "temp = (type *)((char *)temp + __array_new_prefix_size)". */
      temp_var_node = var_rvalue_expr(temp_var);
      temp_var_node = add_cast_if_necessary(temp_var_node, char_star_type());
#if !IA64_ABI
      temp_var_node->next = var_rvalue_expr(array_new_prefix_size_var);
#else /* IA64_ABI */
      temp_var_node->next = make_reusable_copy(prefix_size_node,
                                               /*vars_can_change=*/FALSE);
#endif /* IA64_ABI */
      add_node = make_operator_node((an_expr_operator_kind)eok_padd,
                                    temp_var_node->type, temp_var_node);
      add_node = add_cast_if_necessary(add_node, ptr_elem_type);
      assign_node = make_var_assignment_expr(temp_var,
                                            (an_expr_operator_kind)eok_passign,
                                             add_node);
      insert_expr(assign_node, &insert_location);
    }  /* if */
#endif /* ABI_CHANGES_FOR_PLACEMENT_DELETE */
    new_routine = NULL;  /* Allocation done outside of __vec_new. */
    entity_node = var_rvalue_expr(temp_var);
    /* The size node is used in the "new" call, so it cannot be destroyed. */
    preserve_size_node = TRUE;
  }  /* if */
  /* Here, we have entity_node pointing to an expression for the address
     of the entity, or entity_node == NULL if the __vec_new call or
     equivalent will be allocating the storage. */
  /* Make a node for the number of elements in the array. */
  if (array_type->size != 0) {
    /* The easy and usual case -- the array has a constant number of
       elements.  Do a division to get the right answer for the
       multi-dimensional array case. */
    set_unsigned_integer_constant(&num_elem_constant,
                     (a_host_large_unsigned)array_type->size / elem_type->size,
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
                                   con_for_size);
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
  if (dip != NULL && dip->kind != (a_dynamic_init_kind)dik_zero) {
    /* There is a dynamic init entry to initialize the storage after it is
       allocated.  dik_zero initialization is handled below. */
    /* Get a pointer to the dynamic init entry that applies to the array
       elements instead of the whole array. */
    elem_dip = elem_dynamic_init(dip);
    check_assertion(elem_dip->kind == (a_dynamic_init_kind)dik_constructor);
    zero_storage = need_zeroing_for_value_initialization(elem_dip);
    /* Get the constructor routine to call. */
    ctor_routine = elem_dip->variant.constructor.ptr;
#if IA64_ABI
    ctor_routine = alternate_entry_point(ctor_routine,
                                         (a_ctor_or_dtor_kind)cdk_complete,
                                         /*define_now=*/FALSE);
#endif /* IA64_ABI */
    /* If the constructor has default arguments, make a routine that
       calls the constructor with the necessary default arguments. */
    /* Note that elem_dip->variant.constructor.args must not be lowered
       before passing it to default_version_of_routine. */
    ctor_routine = default_version_of_routine(
                                          ctor_routine,
                                          elem_dip->variant.constructor.args);
    if (elem_dip->init_expr_lifetime != NULL) {
      unbind_object_lifetime(elem_dip->init_expr_lifetime);
    }  /* if */
    /* If exceptions are enabled, a destructor will be specified if
       appropriate. */
    dtor_routine = elem_dip->destructor;
#if IA64_ABI
    if (dtor_routine != NULL) {
      dtor_routine = alternate_entry_point(dtor_routine,
                                           (a_ctor_or_dtor_kind)cdk_complete,
                                           /*define_now=*/FALSE);
    }  /* if */
#endif /* IA64_ABI */
  } else {
    /* There is no dynamic init entry; the storage is not initialized after
       allocation. */
    ctor_routine = NULL;
    dtor_routine = NULL;
  }  /* if */
  if (ndsp->freeing_of_storage_on_exception != NULL) {
    /* The allocated storage must be freed if an exception is thrown before
       the storage is allocated. */
    delete_routine = ndsp->freeing_of_storage_on_exception->destructor;
  } else {
    /* No deletion on throw. */
    delete_routine = NULL;
  }  /* if */
#if ABI_CHANGES_FOR_PLACEMENT_DELETE
  if (!ndsp->placement_new) {
#endif /* ABI_CHANGES_FOR_PLACEMENT_DELETE */
    /* Construct the call of __vec_new or __array_new. */
    vec_new_node = make_vec_new_call(entity_node, ptr_elem_type, num_elem_node,
                                     ctor_routine, dtor_routine,
                                     new_routine, delete_routine,
                                     zero_storage);
#if ABI_CHANGES_FOR_PLACEMENT_DELETE
  } else {
    /* Placement new.  Construct a call of __placement_array_new. */
    vec_new_node = make_placement_array_new_call(entity_node,
                                                 ptr_elem_type, num_elem_node,
                                                 prefix_size_node,
                                                 ctor_routine, dtor_routine,
                                                 delete_routine, delete_args,
                                                 zero_storage);
  }  /* if */
#endif /* ABI_CHANGES_FOR_PLACEMENT_DELETE */
  if (dip != NULL && dip->kind == (a_dynamic_init_kind)dik_zero) {
    /* Add a runtime routine call to zero the allocated storage.  The
       address has to be saved in a temporary and then returned after
       the zeroing call. */
    zero_temp_var = make_lowered_temporary(vec_new_node->type);
    /* Assign the result of the "new" call to the temporary. */
    vec_new_node = make_var_assignment_expr(zero_temp_var,
                                            (an_expr_operator_kind)eok_last,
                                            vec_new_node);
  }  /* if */
  insert_expr(vec_new_node, &insert_location);
  if (dip != NULL && dip->kind == (a_dynamic_init_kind)dik_zero) {
#if IA64_ABI
    if (contains_ptr_to_data_member(elem_type)) {
      /* If the element type contains pointers to data members the storage
         cannot simply be set to zero; the pointers to data members must be
         initialized to -1. */
      insert_call_to_helper_routine_to_zero_entity(
                                elem_type,
                                var_rvalue_expr(zero_temp_var),
                                make_reusable_copy(num_elem_node,
                                                   /*vars_can_change=*/TRUE),
                                &insert_location);
    } else
#endif /* IA64_ABI */
    /* Do not insert code here. */
    {
      /* Continue generating the code for zeroing. */
      an_expr_node_ptr entity_size_node;
      if (array_type->size != 0) {
        /* The array size is a known constant. */
        entity_size_node = node_for_host_large_integer(
                                        (a_host_large_integer)array_type->size,
                                        targ_size_t_int_kind);
      } else {
        /* The array size is computed. */
        entity_size_node = make_reusable_copy(num_elem_node,
                                              /*vars_can_change=*/TRUE);
        /* Cast to size_t. */
        entity_size_node = add_cast_if_necessary(
                                           entity_size_node,
                                           integer_type(targ_size_t_int_kind));
        /* Multiply by the element size if it's not 1. */
        if (elem_size != 1) {
          entity_size_node->next = 
                   node_for_host_large_integer((a_host_large_integer)elem_size,
                                               targ_size_t_int_kind);
          entity_size_node = make_operator_node(
                                          (an_expr_operator_kind)eok_imultiply,
                                          entity_size_node->type,
                                          entity_size_node);
        }  /* if */
      }  /* if */
      insert_call_to_zero_entity(var_rvalue_expr(zero_temp_var),
                                 entity_size_node,
                                 &insert_location);
    }  /* if */
    /* Insert the value of the temporary as the final value of the
       expression. */
    insert_expr(var_rvalue_expr(zero_temp_var), &insert_location);
  }  /* if */
  vec_new_node = insert_location.variant.expr;
  if (ndsp->placement_new) {
    /* Placement new.  Add the "?" operator over the whole expression. */
    compare_node->next = vec_new_node;
    make_zero_of_proper_type(vec_new_node->type, &null_constant);
    vec_new_node->next = alloc_node_for_constant(&null_constant);
    vec_new_node = make_operator_node((an_expr_operator_kind)eok_question,
                                      vec_new_node->type, compare_node);
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
  a_dynamic_init_ptr          dip = ndsp->dynamic_init;
  a_routine_ptr               delete_routine = ndsp->routine;
  a_routine_ptr               dtor_routine;
  an_expr_node_ptr            ptr_node = ndsp->arg, vec_delete_node;

  /* Lower "arg". */
  lower_expr(ptr_node, /*is_lvalue=*/FALSE);
  if (dip != NULL) {
    /* A destructor must be called. */
    dtor_routine = dip->destructor;
    check_assertion(dtor_routine != NULL);
#if IA64_ABI
    dtor_routine = alternate_entry_point(dtor_routine,
                                         (a_ctor_or_dtor_kind)cdk_complete,
                                         /*define_now=*/FALSE);
#endif /* IA64_ABI */
  } else {
    /* There is no dynamic init entry, and therefore no destruction need be
       done along with the deallocation. */
    dtor_routine = NULL;
  }  /* if */
  vec_delete_node = make_vec_delete_call(ptr_node,
                                         /*array_element_count=*/
                                                          (a_targ_ptrdiff_t)-1,
                                         dtor_routine,
                                         delete_routine,
                                         /*free_storage=*/TRUE);
  /* Overwrite the original node with the __vec_delete call. */
  overwrite_node(expr, vec_delete_node);
}  /* lower_array_delete */


static void set_up_freeing_of_storage_on_exception(
                                  a_new_delete_supplement_ptr ndsp,
                                  an_init_pos_descr_ptr       ipdp,
                                  an_insert_location          *insert_location)
/*
ndsp points to the new/delete supplement for a "new".  If necessary, set
up to ensure that the storage allocated will be freed if an exception is
thrown before the initialization of the entity is completed.  ipdp
describes the location of the allocated storage.  Any code required is
inserted at *insert_location.
*/
{
  a_dynamic_init_ptr dyn_init_to_free_storage =
                                         ndsp->freeing_of_storage_on_exception;

  if (dyn_init_to_free_storage != NULL) {
    /* The storage for this "new" is supposed to be freed if an exception
       is thrown before the initialization is completed.  The fact
       that this pointer is non-NULL means exceptions are enabled. */
    if (ndsp->placement_new) {
      /* The placement delete case is handled later, by inserting an
         internal "try" block. */
    } else {
      /* For a default operator delete, the cleanup can be done through a
         cleanup region table entry. */
      add_dyn_init_cleanup(dyn_init_to_free_storage, ipdp,
                           /*set_cond_flag_if_any=*/TRUE,
                           curr_context, insert_location);
    }  /* if */
  }  /* if */
}  /* set_up_freeing_of_storage_on_exception */


static void turn_off_freeing_of_storage_on_exception(
                                  a_new_delete_supplement_ptr ndsp,
                                  an_init_pos_descr_ptr       ipdp,
                                  an_expr_node_ptr            delete_args,
                                  an_insert_location          *insert_location)
/*
ndsp points to the new/delete supplement for a "new".  We're now at a
location after the initialization related to the "new" has been done,
so do the second part of the processing begun by
set_up_freeing_of_storage_on_exception.  ipdp describes the location
of the allocated storage.  delete_args points to the list of arguments
for a placement delete call, if one if needed.  *insert_location indicates
the point at which code should be inserted.
*/
{
  a_dynamic_init_ptr dyn_init_to_free_storage =
                                         ndsp->freeing_of_storage_on_exception;

  if (dyn_init_to_free_storage != NULL) {
    if (ndsp->placement_new) {
      /* Placement delete.  Insert an internal "try" block here,
         with the "catch" an appropriate call of the delete routine. */
      an_expr_node_ptr delete_call, init_expr;
      /* Put a pointer to the allocated storage on the front of the argument
         list for the delete routine. */
      an_expr_node_ptr entity_node = make_init_entity_node(
                                               ipdp, /*using_as_address=*/TRUE,
                                               /*using_as_dest=*/FALSE);
      /* Cast the argument to "void *", which is what the delete routine
         expects. */
      entity_node = add_cast_if_necessary(entity_node, void_star_type());
      entity_node->next = delete_args;
      /* Make a call of the placement delete routine. */
      delete_call = make_call_node(dyn_init_to_free_storage->destructor,
                                   entity_node, /*honor_virtual=*/FALSE,
                                   (an_insert_location *)NULL);
      /* Extract the overall initialization expression from the insert
         location, and wrap a "try" expression around it, with the placement
         delete call as the "catch". */
      check_assertion(is_expr_insert_location(insert_location));
      init_expr = insert_location->variant.expr;
      init_expr = make_internal_try_expr(init_expr, delete_call);
      /* Give back to the caller an insert location that allows insertion
         after the overall expression as modified. */
      set_expr_creation_insert_location(insert_location);
      insert_expr(init_expr, insert_location);
    } else {
      /* Normal, non-placement delete case. */
      a_destructible_entity_descr_ptr dedp =
                           dyn_init_to_free_storage->destructible_entity_descr;
      if (dedp->conditional_flag_var != NULL) {
        /* Reset the flag that indicates that the freeing must be done. */
        reset_conditional_flag_var(dedp->conditional_flag_var,
                                   insert_location);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* turn_off_freeing_of_storage_on_exception */


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
  an_expr_node_ptr            assign_node, compare_node;
  an_expr_node_ptr            init_node, call_node, null_node, delete_args;
  a_constant                  null_constant;
  an_insert_location          insert_location;
  an_init_pos_descr           ipd;

  if (!ndsp->placement_new && ndsp->routine != NULL) {
    a_param_type_ptr params = unlowered_param_type_list(ndsp->routine->type);
    if (params != NULL && params->next != NULL) {
      /* Treat an operator new with default arguments as a placement new. */
      check_assertion_str(params->next->has_default_arg,
                     "lower_new: placement_new not set but more than one arg");
      ndsp->placement_new = TRUE;
    }  /* if */
  }  /* if */
  base_type = new_delete_base_type_from_operation_type(ndsp->type);
  if (is_array_type(ndsp->type) &&
      new_or_delete_type_requires_array_handling(base_type,
                                                 /*check_construtor=*/TRUE)) {
    /* An array "new". */
    lower_array_new(expr);
#if NEW_CAN_BE_FOLDED_INTO_CTOR
  } else if (ndsp->routine == NULL) {
    /* The "new" call has been folded into the constructor call. */
    a_routine_ptr    ctor_routine = dip->variant.constructor.ptr;
    an_expr_node_ptr implied_arg_list, end_implied_arg_list;
    /* ndsp->arg is not lowered because it is thrown away. */
    check_assertion(dip->kind == (a_dynamic_init_kind)dik_constructor &&
                    !dip->variant.constructor.value_initialization);
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
                               /*honor_virtual=*/FALSE,
                               (an_insert_location *)NULL);
    /* The constructor call returns a pointer to the object initialized.
       Cast the pointer to the right type if necessary. */
    call_node = add_cast_if_necessary(call_node, expr->type);
    /* Overwrite the enk_new_delete node with the call/cast. */
    overwrite_node(expr, call_node);
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
  } else {
    /* Non-array case, or array case that does not require special handling. */
    /* Lower the arguments for the "new" call. */
    lower_arg_expr_list(ndsp->arg, ndsp->routine->type,
                        (a_param_type_ptr)NULL);
    delete_args = NULL;
    if (ndsp->placement_new && dip != NULL &&
        ndsp->freeing_of_storage_on_exception != NULL) {
      /* This is a placement new for which there is a corresponding placement
         delete.  Make a copy of the argument list for the new call, to
         be used in the delete call.  Note that this is done after IL lowering,
         so the argument expressions are evaluated only once.  But that
         also means temporaries used to pass class objects via copy
         constructor are shared. */
      /* Note that the copy skips the first argument (the size). */
      delete_args = copy_arg_list_for_placement_delete(ndsp->arg->next);
    }  /* if */
    /* Create a call of the "new" routine. */
    call_node = make_call_node(ndsp->routine, ndsp->arg,
                               /*honor_virtual=*/FALSE,
                               (an_insert_location *)NULL);
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
      temp_var = make_local_temporary(ptr_base_type);
      /* Assign the entity address expression to the temporary. */
      assign_node = make_var_assignment_expr(temp_var,
                                            (an_expr_operator_kind)eok_passign,
                                             add_cast_if_necessary(call_node,
                                                               ptr_base_type));
      /* Compare the assignment node to a NULL constant of the right type. */
      make_zero_of_proper_type(ptr_base_type, &null_constant);
      null_node = alloc_node_for_constant(&null_constant);
      assign_node->next = null_node;
      compare_node = make_operator_node((an_expr_operator_kind)eok_pne,
                                        integer_type((an_integer_kind)ik_int),
                                        assign_node);
      /* Build a description of the entity to be initialized.  Adjust the
         type so that it is an array if necessary. */
      set_var_indirect_init_pos_descr(temp_var, &ipd);
      ipd.base_type = ndsp->type;
      set_expr_creation_insert_location(&insert_location);
      /* If exceptions are enabled, and if necessary, set up to free the
         storage allocated if an exception is thrown before the storage
         is initialized. */
      set_up_freeing_of_storage_on_exception(ndsp, &ipd, &insert_location);
      /* Generate code for the initialization. */
      lower_dynamic_init(dip, &ipd,
                         (an_expr_node_ptr)NULL, (an_expr_node_ptr)NULL,
                         (a_constructor_init_ptr)NULL, LDIO_NONE,
                         /*others_follow_in_aggr=*/FALSE,
                         &insert_location, (a_boolean *)NULL,
                         (a_constant **)NULL);
      /* Now that the entity is initialized, turn off the freeing on
         exception. */
      turn_off_freeing_of_storage_on_exception(ndsp, &ipd, delete_args,
                                               &insert_location);
      /* End the initialization code with an expression that gets the
         value of the temporary. */
      init_node = var_rvalue_expr(temp_var);
      insert_expr(init_node, &insert_location);
      init_node = insert_location.variant.expr;
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


static an_expr_node_ptr make_delete_call(a_routine_ptr    delete_routine,
                                         a_type_ptr       delete_type,
                                         an_expr_node_ptr arg_node)
/*
Create an expression for a call of the delete routine indicated by
delete_routine, with arg_node as the argument.  Return a pointer to
the call expression.
*/
{
  an_expr_node_ptr call_node, second_arg_node;

  /* Cast the argument to "void *", which is what the delete routine
     expects. */
  arg_node = add_cast_if_necessary(arg_node, void_star_type());
  /* If the delete routine is one with two arguments, pass the size
     of the entity as the second argument. */
  second_arg_node = NULL;
  if (is_two_argument_delete(delete_routine)) {
    /* Two-argument form.  Add a second argument of type size_t that
       indicates the (static) size of the object. */
    second_arg_node = node_for_host_large_integer(
                    (a_host_large_integer)(f_skip_typerefs(delete_type)->size),
                    targ_size_t_int_kind);
    arg_node->next = second_arg_node;
  }  /* if */
  /* Make the call. */
  call_node = make_call_node(delete_routine, arg_node,
                             /*honor_virtual=*/FALSE,
                             (an_insert_location *)NULL);
  return call_node;
}  /* make_delete_call */


static an_expr_node_ptr make_dtor_call_for_delete(
                                             a_dynamic_init_ptr dip,
                                             an_expr_node_ptr   ptr_node,
                                             a_routine_ptr      delete_routine)
/*
Generate code for a delete operation that involves a destructor call.
dip points to a dynamic initialization entry that indicates the destructor.
ptr_node points to the object to be destroyed/deleted.  delete_routine
indicates the delete routine to be called, or is NULL to indicate that
the default delete for the class should be used.  If the destructor is
virtual, it is called as a virtual function, which involves some special
tricks.
*/
{
  an_expr_node_ptr ptr_node_test, ptr_node_delete, call_node;
  an_expr_node_ptr compare_node;
  a_type_ptr       class_type;
  a_constant       null_constant;
  a_routine_ptr    dtor_routine = dip->destructor;
  a_boolean        need_null_ptr_test = FALSE;
#if !IA64_ABI
  long             bit_mask;
#endif /* !IA64_ABI */

  check_assertion(dtor_routine != NULL &&
                  dtor_routine->source_corresp.is_class_member);
  class_type = dtor_routine->source_corresp.parent.class_type;
  /* Cast the expression to the type of the destructor parameter, if
     necessary.  This is needed for the case where a pointer to an array
     is deleted without the "delete []" syntax.  That's undefined
     behavior, and only the first element will be destroyed, but we
     want to avoid generating incorrect code. */
  ptr_node = add_cast_if_necessary(
                   ptr_node, implicit_this_param_type_of(dtor_routine->type));
#if IA64_ABI
  /* Call the deleting version of the destructor.  However, for a class
     with a non-virtual destructor, call the complete object destructor
     and then call the delete routine.  The IA-64 ABI spec requires this
     unless one is willing to put out a definition of the deleting
     destructor everywhere it is used. */
  if (dtor_routine->is_virtual) {
    dtor_routine = alternate_entry_point(dtor_routine,
                                         (a_ctor_or_dtor_kind)cdk_deleting,
                                         /*define_now=*/FALSE);
  } else {
    if (delete_routine == NULL) {
      /* Get the default operator delete for the class. */
      delete_routine = class_type->variant.class_struct_union.extra_info->
                                                 assoc_operator_delete_routine;
      /* The assoc_operator_delete_routine field can be NULL, e.g., for
         an ambiguous class-specific operator delete, but if so the
         front end should have issued an error on this delete operation. */
      check_assertion(delete_routine != NULL);
    }  /* if */
    dtor_routine = alternate_entry_point(dtor_routine,
                                         (a_ctor_or_dtor_kind)cdk_complete,
                                         /*define_now=*/FALSE);
    /* The destructor shouldn't be called if the object pointer is null. */
    need_null_ptr_test = TRUE;
  }  /* if */
#endif /* IA64_ABI */
  if (dtor_routine->is_virtual) {
    /* A null-pointer test is required around the destructor call
       (you can't do a virtual call on a null pointer). */
    need_null_ptr_test = TRUE;
  }  /* if */
  if (need_null_ptr_test) {
    /* Make a copy of the object pointer so we can use it later in building
       the null-pointer test.  Force use of a temporary now if we would
       be using one for the copy for the delete call anyway. */
    ptr_node_test = ptr_node;
    ptr_node = make_reusable_copy(ptr_node,
                                 /*vars_can_change=*/(delete_routine != NULL));
  }  /* if */
  if (delete_routine != NULL) {
    /* Make a copy of the object pointer so that we can use it later in
       building the call of the delete routine. */
    ptr_node_delete = make_reusable_copy(ptr_node, /*vars_can_change=*/TRUE);
  }  /* if */
#if !IA64_ABI
  /* Add an implicit parameter to the destructor call with bits
     0x2 (whole object) + 0x1 (free storage, if deallocate is TRUE). */
  bit_mask = 2L;
  if (delete_routine == NULL) bit_mask |= 1L;
  ptr_node->next = node_for_integer_constant(bit_mask,
                                             (an_integer_kind)ik_int);
#endif /* !IA64 */
  /* Make a call of the destructor. */
  call_node = make_call_node(dtor_routine, ptr_node, /*honor_virtual=*/TRUE,
                             (an_insert_location *)NULL);
  if (dtor_routine->is_virtual) {
    /* The destructor is virtual, so rewrite the virtual call. */
    lower_virtual_function_call(call_node);
  }  /* if */
  if (delete_routine != NULL) {
    /* Add a call of the delete routine, so we have a comma expression
         (dtor(...), delete(...))
    */
    an_expr_node_ptr delete_call_node =
                 make_delete_call(delete_routine, class_type, ptr_node_delete);
    call_node = make_comma_node(call_node, delete_call_node);
  }  /* if */
  if (need_null_ptr_test) {
    /* Add a null pointer test, producing
         (ptr_node != NULL) ? dtor(...) : (void)0
                                       ^ plus possible delete call here
    */
    /* Make "ptr_node != NULL". */
    make_zero_of_proper_type(ptr_node_test->type, &null_constant);
    ptr_node_test->next = alloc_node_for_constant(&null_constant);
    compare_node = make_operator_node((an_expr_operator_kind)eok_pne,
                                      integer_type((an_integer_kind)ik_int),
                                      ptr_node_test);
    /* Make "(ptr_node != NULL) ? dtor(...) : (void)0". */
    compare_node->next = call_node;
    compare_node->next->next = zero_cast_to_void();
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
  a_routine_ptr               delete_routine = ndsp->routine;

  base_type = new_delete_base_type_from_operation_type(ndsp->type);
  if (ndsp->array_delete &&
      new_or_delete_type_requires_array_handling(base_type,
                                                 /*check_constructor=*/TRUE)) {
    /* An array "delete". */
    lower_array_delete(expr);
#if !DELETE_CAN_BE_FOLDED_INTO_DTOR
/* IL lowering requires that it be possible to fold the delete call into
   a destructor.  Without that, it has no way of getting the right size
   on a delete of a pointer to a class with a virtual destructor. */
 #error -- DELETE_CAN_BE_FOLDED_INTO_DTOR set wrong.
#endif /* !DELETE_CAN_BE_FOLDED_INTO_DTOR */
  } else if (dip != NULL) {
    /* The deletion is for a class type and involves calling a
       destructor.  delete_routine is NULL to indicate that the
       default delete routine for the class should be used; this
       may be handled by the destructor itself. */
    /* Lower "arg"; do it as a list in case the delete routine is the
       two-argument version.  Drop the second argument if present. */
    lower_expr_list(ptr_node, 0, 0);
    ptr_node->next = NULL;
    dtor_call_node = make_dtor_call_for_delete(dip, ptr_node, delete_routine);
    /* Overwrite the enk_new_delete node with the call. */
    overwrite_node(expr, dtor_call_node);
  } else {
    /* Non-array case, or array case that does not require special handling,
       and not a case that requires calling a destructor. */
    check_assertion(delete_routine != NULL);
    /* Lower "arg". */
    lower_expr(ptr_node, /*is_lvalue=*/FALSE);
    /* Make the "delete" call.  It is not necessary to test for non-NULL;
       the delete routine does that. */
    call_node = make_delete_call(delete_routine, ndsp->type, ptr_node);
    /* Overwrite the enk_new_delete node with the final expression. */
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
  a_boolean          result_is_addr, result_is_not_used;
  an_insert_location insert_location;
  a_boolean          is_constructor_init;
  a_variable_ptr     temp_var;
  a_boolean          keep_dynamic_init;

  dip = expr->variant.init.dynamic_init;
  result_is_addr = expr->variant.init.result_is_addr;
  if (dip->kind == (a_dynamic_init_kind)dik_expression && !result_is_addr &&
      dip->destructor == NULL) {
    /* For a simple expression temporary case where the address of the
       temporary is not taken, just lower the expression and create no
       temporary.  This is a useful for cases where a function returns
       a class by value (i.e., the class has no copy constructor). */
    lower_expr(dip->variant.expression, /*is_lvalue=*/FALSE);
    overwrite_node(expr, dip->variant.expression);
  } else {
    result_is_not_used = expr->result_is_not_used;
    /* Determine the type of the temporary. */
    temp_type = expr->type;
    if (result_is_addr) {
      /* The value of the enk_temp_init node is the address of the temporary,
         so drop the pointer-to to get the temporary type. */
      temp_type = type_pointed_to(temp_type);
    }  /* if */
    /* Create a temporary variable.  Make it static if necessary. */
    if (!expr->variant.init.static_temp && !long_lifetime_temps &&
        dip->has_temporary_lifetime) {
      /* Simple case; a temporary that lasts until the end of the full
         expression will do. */
      temp_var = make_local_temporary(temp_type);
    } else {
      temp_var = make_temporary_in_scope(temp_type,
                                         (a_scope_ptr)NULL,
                                         (a_boolean)
                                               expr->variant.init.static_temp);
    }  /* if */
    dip->variable = temp_var;
    if (dip->is_partially_initialized_compound_literal) {
      /* Note that compound literals created
         outside of functions do not use enk_temp_init so they are not
         seen here (the front end creates an initialized static variable
         for them). */
      temp_var->is_partially_initialized = TRUE;
    }  /* if */
    /* Change the enk_temp_init to a reference to the value or address
       of the temporary. */
    if (result_is_addr) {
      set_expr_node_kind(expr, (an_expr_node_kind)enk_variable_address);
      /* The address of the temporary escapes (or might escape) into the
         surrounding context, so set its address_taken flag. */
      set_lowering_variable_address_taken(temp_var);
    } else {
      set_expr_node_kind(expr, (an_expr_node_kind)enk_variable);
    }  /* if */
    expr->variant.variable = temp_var;
    /* Generate code for the dynamic init. */
    set_var_init_pos_descr(temp_var, &ipd);
    /* Test the kind before calling lower_dynamic_init because that routine
       clears the kind in some cases. */
    is_constructor_init = (dip->kind == (a_dynamic_init_kind)dik_constructor);
    /* Any code generated for the dynamic initialization will be
       inserted before the (modified) original expression. */
    set_expr_insert_location(expr, &insert_location);
    /* Lower the initialization. */
    if (dip->kind == (a_dynamic_init_kind)dik_constant ||
        dip->kind == (a_dynamic_init_kind)dik_nonconstant_aggregate) {
      lower_designated_initializers(dip->variant.constant);
    }  /* if */
    lower_dynamic_init(dip, &ipd,
                       (an_expr_node_ptr)NULL, (an_expr_node_ptr)NULL,
                       (a_constructor_init_ptr)NULL, LDIO_NONE,
                       /*others_follow_in_aggr=*/FALSE,
                       &insert_location, &keep_dynamic_init,
                       (a_constant **)NULL);
    if (keep_dynamic_init) {
      /* Record any dynamic initialization that might have been created
         while lowering a compound literal. */
      add_stmk_init_for_compound_literal(temp_var, dip);
    }  /* if */
    if (temp_var->init_kind == (an_init_kind)initk_zero &&
        !has_static_storage_duration(temp_var->storage_class)) {
      /* We need to zero an automatic temporary, which can't be done by
         setting its init_kind to initk_zero, because we don't know that
         the block of the temporary will be entered at the top.  Make
         a zeroed static variable and copy it to the temporary. */
      a_variable_ptr static_temp = make_temporary_in_scope(
                                                        temp_type,
                                                        (a_scope_ptr)NULL,
                                                        /*force_static=*/TRUE);
      static_temp->init_kind = (an_init_kind)initk_zero;
      (void)insert_assignment_statement(var_lvalue_expr(temp_var),
                                        (an_expr_operator_kind)eok_bassign,
                                        var_lvalue_expr(static_temp),
                                        &insert_location);
      temp_var->init_kind = (an_init_kind)initk_none;
    }  /* if */
    /* Optimization -- if the initialization is done by a constructor,
       and the enk_temp_init returns the address of the temporary,
       use the pointer returned from the constructor as the value of
       the expression.  Likewise, if the result of the expression is not
       used, the node for the temporary value or address is not needed. */
    if ((result_is_addr && is_constructor_init) || result_is_not_used) {
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
          /* If necessary, add a cast to adjust qualification.  We check the
             second level for compatibility because the first is likely to be
             a pointer in one case and a reference in the other. */
          if (!result_is_not_used) {
            a_type_ptr expr_und_type = type_pointed_to(expr->type);
            a_type_ptr first_op_und_type= type_pointed_to(first_operand->type);
            if (!il_identical_types(expr_und_type, first_op_und_type)) {
              first_operand->next = NULL;
              first_operand->result_is_not_used = FALSE;
              first_operand = add_cast(first_operand, expr->type);
            }  /* if */
          }  /* if */
          overwrite_node(expr, first_operand);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* lower_temp_init */

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
#if !IA64_ABI
  an_expr_node_ptr       test_var_node, compare_node;
  a_constant             minus_one_constant;
  a_memory_region_number region_to_switch_back_to;
#endif /* !IA64_ABI */
  a_boolean              guard_code_emitted = FALSE;

  /* If the variable has internal linkage (e.g., in -tlocal mode), do not
     put out guard code at all. */
  if (variable->source_corresp.name_linkage ==
                        (a_name_linkage_kind)nlk_internal) goto end_of_routine;
#if !IA64_ABI
  /* Make the guard variable at the file scope. */
  test_var = make_global_var_with_prefixed_name("__SDG__",
                                                (an_integer_kind)ik_int,
                                                &variable->source_corresp);
  if (variable->is_specialized) {
    /* This variable is a specialization of a template entity, so its
       initialization should take precedence over any initialization code
       for other instances.  Initialize the guard variable to -1 to lock out
       all other initialization code.  No test of the guard variable is
       needed here. */
    test_var->init_kind = (an_init_kind)initk_static;
    set_integer_constant(&minus_one_constant, (a_host_large_integer)-1,
                         (an_integer_kind)ik_int);
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
    insert_if_statement(compare_node, /*is_initialization_guard=*/TRUE,
                        insert_location, (a_statement_ptr *)NULL,
                        insert_location2, (an_insert_location *)NULL);
    /* Make "test_var = 1" and insert it inside the "if" statement. */
    (void)insert_var_assignment_statement(test_var,
                                          (an_expr_operator_kind)eok_iassign,
                                          node_for_integer_constant(1L,
                                                      (an_integer_kind)ik_int),
                                          insert_location2);
  }  /* if */
#else /* IA64_ABI */
  add_first_time_test(variable, insert_location, insert_location2,
                      (a_statement_ptr *)NULL, &test_var);
  guard_code_emitted = TRUE;
#endif /* IA64_ABI */
end_of_routine:
  return guard_code_emitted;
}  /* add_static_data_member_init_guard_test */

#endif /* TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE */

void lower_stmk_init(a_statement_ptr statement)
/*
Generate code for a stmk_init (dynamic initialization) statement.
*/
{
  a_dynamic_init_ptr dip = statement->variant.dynamic_init;
  a_variable_ptr     var = dip->variable;
  a_boolean          non_C_case = FALSE;

  /* Only lower the cases that do not come up in C: */
  if (dip->destructor != NULL) {
    /* Initialization with a later destructor. */
    non_C_case = TRUE;
  } else if (dip->init_expr_lifetime != NULL) {
    /* Initialization that wraps a lifetime around the initialization (because
       there are temporaries created in it). */
    non_C_case = TRUE;
  } else if (has_static_storage_duration(var->storage_class)) {
    /* Initialization of a local static variable cannot be dynamic in C.
       Code must be used to do the initialization. */
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
    }  /* if */
    lower_dynamic_init(dip, &ipd,
                       (an_expr_node_ptr)NULL, (an_expr_node_ptr)NULL,
                       (a_constructor_init_ptr)NULL, LDIO_FULL_EXPR,
                       /*others_follow_in_aggr=*/FALSE,
                       &insert_location, &keep_dynamic_init,
                       (a_constant **)NULL);
    if (!keep_dynamic_init) {
      /* Delete the stmk_init statement. */
      turn_statement_into_noop(statement);
    }  /* if */
  } else {
    /* Normal C case.  Lower the subtree if any. */
    switch (dip->kind) {
      case dik_constant:
        lower_constant(dip->variant.constant);
        break;
      case dik_expression:
        lower_full_expr(dip->variant.expression,
                        /*is_lvalue=*/is_reference_type(dip->variable->type),
                        (a_statement_ptr)NULL);
        break;
#if CHECKING
      default:
        internal_error("lower_stmk_init: bad dynamic init kind (2)");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */
}  /* lower_stmk_init */


void insert_temp_init_statements(a_statement_ptr  statement)
/*
If there are any pending statements (as the result of lowering an enk_temp_init
node), insert them before the given statement.  (This happens when lowering
compound literals.)
*/
{
  if (temp_init_statements != NULL) {
    /* Insert statements before the given statement. */
    an_insert_location insert_location;
    a_statement_ptr    orig_stmt;
    change_statement_into_block(statement, &orig_stmt);
    set_block_start_insert_location(statement, &insert_location);
    while (temp_init_statements != NULL) {
      a_statement_ptr stmt = temp_init_statements;
      temp_init_statements = stmt->next;
      stmt->next = NULL;
      insert_statement(stmt, &insert_location);
    }  /* while */
  }  /* if */
}  /* insert_temp_init_statements */


void add_stmk_init_for_compound_literal(a_variable_ptr      var,
                                        a_dynamic_init_ptr  dip)
/*
var represents a temporary variable created to hold the value of compound
literal, while dip describes the required dynamic initialization.  Create
the stmk_init statement required for this initialization, and add it to the
temp_init_statements list.
*/
{
  a_statement_ptr  stmk_init_stmt =
                                 alloc_statement((a_statement_kind)stmk_init);

  stmk_init_stmt->variant.dynamic_init = dip;
  /* Put the statement on a list to be inserted when we get back to
     statement level. */
  add_to_end_of_temp_init_statements_list(stmk_init_stmt);
  /* Reflect the initialization method in the variable entry. */
  var->init_kind = (an_init_kind)initk_dynamic;
  var->initializer.dynamic = dip;
}  /* add_stmk_init_for_compound_literal */


void add_to_end_of_temp_init_statements_list(a_statement_ptr stmt)
/*
Add the indicated statement to the end of the temp_init_statements list.
*/
{
  if (temp_init_statements == NULL) {
    temp_init_statements = stmt;
  } else {
    a_statement_ptr end_of_list = temp_init_statements;
    while (end_of_list->next != NULL) end_of_list = end_of_list->next;
    end_of_list->next = stmt;
  }  /* if */
  stmt->next = NULL;
}  /* add_to_end_of_temp_init_statements_list */

#if MICROSOFT_EXTENSIONS_ALLOWED
#if LOWER_MICROSOFT_NONCONSTANT_AGGREGATE

void lower_microsoft_C_mode_nonconstant_aggregate_init(
                                                    a_variable_ptr  vp,
                                                    a_statement_ptr init_stmt)
/*
In Microsoft C mode, an auto variable is allowed to be initialized with a
nonconstant aggregate.  This construct is not something usually expected
by back ends, so lower it to normal C.  vp is the initialized variable.
init_stmt is the stmk_init statement.
*/
{
  an_init_pos_descr  ipd;
  an_insert_location insert_location;
  a_boolean          keep_dynamic_init;

  check_assertion(vp->init_kind == (an_init_kind)initk_dynamic &&
                  init_stmt != NULL &&
                  init_stmt->kind == (a_statement_kind)stmk_init);
  if (!suppress_il_lowering && total_errors == 0) {
    set_var_init_pos_descr(vp, &ipd);
    set_insert_location(init_stmt, &insert_location);
    lower_dynamic_init(vp->initializer.dynamic, &ipd,
                       (an_expr_node_ptr)NULL, (an_expr_node_ptr)NULL,
                       (a_constructor_init_ptr)NULL, LDIO_FULL_EXPR,
                       /*others_follow_in_aggr=*/FALSE,
                       &insert_location, &keep_dynamic_init,
                       (a_constant **)NULL);
    if (!keep_dynamic_init) {
      /* Delete the stmk_init statement. */
      turn_statement_into_noop(init_stmt);
    }  /* if */
  } /* if */
}  /* lower_microsoft_C_mode_nonconstant_aggregate_init */

#endif /* LOWER_MICROSOFT_NONCONSTANT_AGGREGATE */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if LOWER_DESIGNATED_INITIALIZERS

/*
Structure used to hold information about the current position in
an aggregate, for lowering of designated initializers.
*/
typedef struct an_aggregate_position {
  a_boolean	array_init;
			/* TRUE if the aggregate is an array, FALSE for
			   a struct/union. */
  a_field_ptr	curr_field;
			/* Current field, when array_init == FALSE. */
  a_targ_size_t	curr_elem;
			/* Current element in the array, when array_init ==
			   TRUE. */
  a_type_ptr	member_type;
			/* Current member type. */
} an_aggregate_position;


static void set_aggregate_position_for_field(a_field_ptr           field,
                                             an_aggregate_position *aggr_pos)
/*
Set *aggr_pos to indicate the position of the given field.
*/
{
  aggr_pos->curr_field = field;
  aggr_pos->member_type = field->type;
}  /* set_aggregate_position_for_field */


static void init_aggregate_position(a_constant_ptr        aggr_con,
                                    an_aggregate_position *aggr_pos)
/*
Initialize the indicated aggregate position block, indicating the
position of the first member of the aggregate constant aggr_con.
*/
{
  a_type_ptr aggr_type;

  check_assertion(aggr_con != NULL &&
                  aggr_con->kind == (a_constant_repr_kind)ck_aggregate);
  aggr_type = f_skip_typerefs(aggr_con->type);
  aggr_pos->array_init = (aggr_type->kind == (a_type_kind)tk_array);
  aggr_pos->curr_field = NULL;
  aggr_pos->curr_elem = 0;
  aggr_pos->member_type = NULL;
  if (aggr_pos->array_init) {
    /* Initializing members of an array. */
    aggr_pos->member_type = f_skip_typerefs(array_element_type(aggr_type));
  } else {
    /* Initializing members of a struct or union. */
    a_field_ptr first_field =
                    next_initializable_field(
                             aggr_type->variant.class_struct_union.field_list);
    if (first_field != NULL) {
      set_aggregate_position_for_field(first_field, aggr_pos);
    }  /* if */
  }  /* if */
}  /* init_aggregate_position */


static void advance_aggregate_position_to_next_member(
                                               an_aggregate_position *aggr_pos)
/*
Advance the indicated position within an aggregate to the next member of
the aggregate.
*/
{
  if (aggr_pos->array_init) {
    aggr_pos->curr_elem++;
  } else {
    a_field_ptr field = aggr_pos->curr_field;
    check_assertion(field != NULL);
    field = next_initializable_field(field->next);
    check_assertion(field != NULL);
    set_aggregate_position_for_field(field, aggr_pos);
  }  /* if */
}  /* advance_aggregate_position_to_next_member */


static a_constant_ptr make_init_zero_constant(a_type_ptr type)
/*
Make and return an unshared constant that is a zero of the indicated type.
If the type is an aggregate, return an aggregate constant that initializes
the first member of the aggregate.
*/
{
  a_constant_ptr con;

  if (!is_aggregate_or_union_type(type)) {
    /* Simple scalar case. */
    a_constant zero_constant;
    make_zero_of_proper_type(rvalue_type(type), &zero_constant);
    con = alloc_unshared_constant(&zero_constant);
  } else {
    /* Aggregate type. */
    an_aggregate_position aggr_pos;
    con = alloc_constant((a_constant_repr_kind)ck_aggregate);
    con->type = type;
    init_aggregate_position(con, &aggr_pos);
    /* Check that there is at least one initializable member. */
    if (aggr_pos.member_type != NULL) {
      con->variant.aggregate.first_constant =
      con->variant.aggregate.last_constant =
                                 make_init_zero_constant(aggr_pos.member_type);
    }  /* if */
  }  /* if */
  return con;
}  /* make_init_zero_constant */


/*
Return TRUE if the aggregate position aggr_pos and the ck_designator
constant con indicate the same aggregate member.
*/
#define same_aggregate_member(aggr_pos, con) \
  ((aggr_pos)->array_init ? \
        ((con)->variant.designator.array_element == (aggr_pos)->curr_elem) : \
        ((con)->variant.designator.field == (aggr_pos)->curr_field))


/*
Structure used to describe a position in an initializer constant list.
In particular, it deals with positions within repeated constants.
*/
typedef struct an_init_con_pos {
  a_constant_ptr
		ptr;
			/* The constant. */
  a_targ_size_t	repeat_count;
			/* If the constant is a repeated constant, the
			   number of repetitions yet to be processed.
			   Zero otherwise. */
} an_init_con_pos;


static void set_init_con_pos(a_constant_ptr  con,
                             an_init_con_pos *init_con_pos)
/*
Set an init constant position for the indicated constant.  It's okay for
con to be NULL, to set a null position.
*/
{
  init_con_pos->ptr = con;
  init_con_pos->repeat_count = 0;
  if (con != NULL && con->kind == (a_constant_repr_kind)ck_init_repeat) {
    /* For a repeated constant, indicate the number of repetitions yet to
       be handled (all of them). */
    init_con_pos->repeat_count = con->variant.init_repeat.count;
  }  /* if */
}  /* set_init_con_pos */


static void advance_init_con_pos(an_init_con_pos *init_con_pos)
/*
Advance the initializer constant position given to the next constant in
the list, or the next iteration of a repeated constant.
*/
{
  if (init_con_pos->repeat_count > 0) {
    init_con_pos->repeat_count--;
  } else if (init_con_pos->ptr == NULL) {
    /* Do not advance at end of list. */
  } else {
    set_init_con_pos(init_con_pos->ptr->next, init_con_pos);
  }  /* if */
}  /* advance_init_con_pos */
  

static void split_constant_if_repeated(an_init_con_pos *con_pos)
/*
If the indicated initializer constant position is in a repeated constant,
split the constant to produce a simple constant that can be handled
directly.  *con_pos will be set to indicate the simple constant.
*/
{
  a_constant_ptr con = con_pos->ptr;

  if (con->kind == (a_constant_repr_kind)ck_init_repeat) {
    a_targ_size_t  count = con->variant.init_repeat.count;
    a_constant_ptr rep_con = con->variant.init_repeat.constant;
    a_constant_ptr simple_con, next_con;
    a_targ_size_t  first_count = (count - con_pos->repeat_count);
    a_targ_size_t  second_count = con_pos->repeat_count - 1;

    next_con = con->next;
    if (first_count != 0) {
      a_constant_ptr first_repeat_con;
      /* We need a constant before the simple constant at the current
         position.  The simple constant is a new allocation. */
      first_repeat_con = con;
      first_repeat_con->variant.init_repeat.count = first_count;
      /* If the repeat count is one, skip the ck_init_repeat. */
      if (first_count == 1) copy_constant(rep_con, first_repeat_con);
      simple_con = copy_unshared_constant(rep_con);
      first_repeat_con->next = simple_con;
#if DEBUG
      if (db_flag_is_set("designators")) {
        (void)fprintf(f_debug, "Splitting constant, constant before = ");
        db_constant(first_repeat_con);
        (void)fprintf(f_debug, ", simple_con = ");
        db_constant(simple_con);
        (void)fprintf(f_debug, "\n");
      }  /* if */
#endif /* DEBUG */
    } else {
      /* The position is at the beginning of the repeat, so there is no
         repeated constant preceding the simple constant.  Overwrite the
         original ck_init_repeat constant with the value of the underlying
         constant (thus making the first repetition). */
      simple_con = con;
      copy_constant(rep_con, con);
#if DEBUG
      if (db_flag_is_set("designators")) {
        (void)fprintf(f_debug,
                      "Splitting constant, no constant before, simple_con = ");
        db_constant(simple_con);
        (void)fprintf(f_debug, "\n");
      }  /* if */
#endif /* DEBUG */
    }  /* if */
    if (second_count != 0) {
      a_constant_ptr second_repeat_con, rep_con_copy;
      /* We need a constant following the simple constant at the current
         position. */
      rep_con_copy = copy_unshared_constant(rep_con);
      /* If the repeat count is one, skip the ck_init_repeat. */
      if (second_count == 1) {
        second_repeat_con = rep_con_copy;
      } else {
        second_repeat_con =
                          alloc_constant((a_constant_repr_kind)ck_init_repeat);
        second_repeat_con->variant.init_repeat.count = second_count;
        second_repeat_con->variant.init_repeat.constant = rep_con_copy;
      }  /* if */
      simple_con->next = second_repeat_con;
      second_repeat_con->next = next_con;
#if DEBUG
      if (db_flag_is_set("designators")) {
        (void)fprintf(f_debug,
                      "Splitting constant, constant after = ");
        db_constant(second_repeat_con);
        (void)fprintf(f_debug, "\n");
      }  /* if */
#endif /* DEBUG */
    } else {
      /* The position is at the end of the repeat, so there is no repeated
         constant following the simple constant. */
      simple_con->next = next_con;
#if DEBUG
      if (db_flag_is_set("designators")) {
        (void)fprintf(f_debug,
                      "Splitting constant, no constant after.\n");
      }  /* if */
#endif /* DEBUG */
    }  /* if */
    /* The new current position is on the non-repeated actual constant. */
    set_init_con_pos(simple_con, con_pos);
  }  /* if */
}  /* split_constant_if_repeated */
  

static void explode_string_initializer(an_init_con_pos *con_pos)
/*
If the indicated initializer constant position is on a string literal
constant, explode the string into an aggregate initializer for the
individual characters.  *con_pos will be set to indicate the aggregate.
*/
{
  a_constant_ptr con = con_pos->ptr;

  if (con->kind == (a_constant_repr_kind)ck_string) {
    a_targ_size_t  i;
    a_targ_size_t  len = con->variant.string.length;
    char           *str = con->variant.string.value;
    a_boolean      is_wide = !is_char_array_type(con->type);

    set_constant_kind(con, (a_constant_repr_kind)ck_aggregate);
    for (i = 0; i < len; i += (is_wide ? targ_sizeof_wchar_t : 1)) {
      a_constant     char_val;
      a_constant_ptr char_con;

      /* Make a constant for one character of the string. */
      if (!is_wide) {
        set_integer_constant(&char_val, (a_host_large_integer)str[i],
                             (an_integer_kind)ik_char);
      } else {
        /* Wide string case. */
        unsigned long val = extract_wide_char_from_string(str+i);
        set_unsigned_integer_constant(&char_val,
                                      (a_host_large_unsigned)val,
                                      targ_wchar_t_int_kind);
      }  /* if */
      char_con = alloc_unshared_constant(&char_val);
      /* Add the constant to the aggregate list. */
      if (con->variant.aggregate.first_constant == NULL) {
        con->variant.aggregate.first_constant = char_con;
      } else {
        con->variant.aggregate.last_constant->next = char_con;
      }  /* if */
      con->variant.aggregate.last_constant = char_con;
    }  /* for */
  }  /* if */
}  /* explode_string_initializer */


static void find_designator_insert_point(a_constant_ptr  desig_con,
                                         a_constant_ptr  aggr_con,
                                         a_constant_ptr  *previous_con,
                                         an_init_con_pos *earlier_con)
/*
The ck_designator constant desig_con appeared at the top level of the
aggregate constant aggr_con.  It and the constants following it have been
removed from the aggregate.  Determine the right insert point to re-insert
the constants following, and set *previous_con to the constant after which
to insert (or NULL for insertion at the beginning of the aggregate).
If the new constants will overwrite earlier initialization constants,
set *earlier_con to indicate the first of the constants being
overwritten; otherwise, set it to indicate no constant.  This routine
is not called for union initializations.
*/
{
  an_aggregate_position aggr_pos;
  an_init_con_pos       con;
  a_constant_ptr        prev_con;
  a_targ_size_t         count;

  check_assertion(desig_con != NULL &&
                  desig_con->kind == (a_constant_repr_kind)ck_designator);
  init_aggregate_position(aggr_con, &aggr_pos);
  set_init_con_pos(aggr_con->variant.aggregate.first_constant, &con);
  prev_con = NULL;
  /* Find the right insert point. */
  while (!same_aggregate_member(&aggr_pos, desig_con)) {
    if (con.ptr == NULL) {
      /* Inserting after the end of the aggregate constant list.
         Add a zero constant for a skipped member. */
      a_constant_ptr zero_con = make_init_zero_constant(aggr_pos.member_type);
      if (prev_con == NULL) {
        aggr_con->variant.aggregate.first_constant = zero_con;
      } else {
        prev_con->next = zero_con;
      }  /* if */
#if DEBUG
      if (db_flag_is_set("designators")) {
        (void)fprintf(f_debug, "Finding insert point: inserting at end\n");
      }  /* if */
#endif /* DEBUG */
      set_init_con_pos(zero_con, &con);
      if (aggr_pos.array_init) {
        /* For an array, we can add a repeat count to initialize multiple
           elements. */
        count = (desig_con->variant.designator.array_element -
                 aggr_pos.curr_elem);
        if (count > 1) {
          a_constant_ptr repeat_con =
                          alloc_constant((a_constant_repr_kind)ck_init_repeat);
          repeat_con->variant.init_repeat.count = count;
          repeat_con->variant.init_repeat.constant = zero_con;
#if DEBUG
          if (db_flag_is_set("designators")) {
            (void)fprintf(f_debug, "Array repeat const = ");
            db_constant(repeat_con);
            (void)fprintf(f_debug, "\n");
          }  /* if */
#endif /* DEBUG */
          set_init_con_pos(repeat_con, &con);
          if (prev_con == NULL) {
            aggr_con->variant.aggregate.first_constant = repeat_con;
          } else {
            prev_con->next = repeat_con;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    /* Advance to the next member. */
    if (con.repeat_count > 0) {
      /* When dealing with a repeated constant, we can skip directly over
         all the corresponding elements. */
      count = (desig_con->variant.designator.array_element -
               aggr_pos.curr_elem);
      if (count > con.repeat_count) count = con.repeat_count;
      check_assertion(count > 0);
      aggr_pos.curr_elem += count;
      con.repeat_count -= count;
      if (con.repeat_count == 0) {
        prev_con = con.ptr;
        advance_init_con_pos(&con);
      }  /* if */
    } else {
      /* Normal single-member advance. */
      advance_aggregate_position_to_next_member(&aggr_pos);
      prev_con = con.ptr;
      advance_init_con_pos(&con);
    }  /* if */
  }  /* while */
  if (con.ptr != NULL) {
    /* If the position found is in a repeated constant, split the constant
       so we can give the caller a simple constant. */
    a_constant_ptr temp_con = con.ptr;
    split_constant_if_repeated(&con);
    /* Reset the previous constant pointer. */
    if (temp_con != con.ptr) {
      prev_con = temp_con;
      check_assertion(prev_con->next == con.ptr);
    }  /* if */
  }  /* if */
  *previous_con = prev_con;
  *earlier_con = con;
#if DEBUG
  if (db_flag_is_set("designators")) {
    (void)fprintf(f_debug, "Found insert point, previous_con = ");
    db_constant(*previous_con);
    (void)fprintf(f_debug, ", earlier_con.ptr = ");
    db_constant(earlier_con->ptr);
    (void)fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
}  /* find_designator_insert_point */


static void process_union_designators(
                                    a_constant_ptr  old_con,
                                    a_constant_ptr  old_designator,
                                    a_constant_ptr  new_designator,
                                    an_init_con_pos *earlier_con,
                                    a_constant_ptr  *saved_union_init_constant)
/*
Process designators for a union, as part of lowering designated initializers.
old_con and old_designator give the previous value and member for the union,
and new_designator identifies the member that is next to be initialized.
old_designator and new_designator are NULL to indicate the first member
of the union, and point to a ck_designator constant for the member to
be initialized otherwise.  If the new member is the same as the old member,
*earlier_con is set to the old value; otherwise (if the new member is
different than the old member), the old value is added to
*saved_union_init_constant, which is a list of superseded initializations.
*/
{
  if ((old_designator == NULL || new_designator == NULL) ?
              (old_designator == new_designator) :
              (old_designator->variant.designator.field ==
                        new_designator->variant.designator.field)) {
    /* Same member.  Move the old constant to *earlier_con. */
    set_init_con_pos(old_con, earlier_con);
#if DEBUG
    if (db_flag_is_set("designators")) {
      (void)fprintf(f_debug,
                    "Initializing same member of union, earlier_con->ptr = ");
      db_constant(earlier_con->ptr);
      (void)fprintf(f_debug, "\n");
    }  /* if */
#endif /* DEBUG */
  } else {
    /* Different member.  Add the old constant to saved_union_init_constant. */
    if (old_con != NULL) {
      if (*saved_union_init_constant != NULL) {
        combine_initializer_constants(*saved_union_init_constant, old_con);
      }  /* if */
      *saved_union_init_constant = old_con;
    }  /* if */
#if DEBUG
    if (db_flag_is_set("designators")) {
      (void)fprintf(f_debug, "saved_union_init_constant = ");
      db_constant(*saved_union_init_constant);
      (void)fprintf(f_debug, "\n");
    }  /* if */
#endif /* DEBUG */
    /* Clear *earlier_con. */
    set_init_con_pos((a_constant_ptr)NULL, earlier_con);
  }  /* if */
}  /* process_union_designators */
  

static void lower_aggregate_designated_initializers(
                                               a_constant_ptr aggr_con,
                                               a_constant_ptr earlier_aggr_con)
/*
Lower designated initializers in the indicated aggregate constant to
standard C.  If earlier_aggr_con is non-NULL, aggr_con is a replacement
for earlier_aggr_con (it initializes the same aggregate, overwriting
the earlier initialization).  The constants under earlier_aggr_con
have already had their designated initializers lowered.
*/
{
  a_constant_ptr  temp_con;
  a_constant_ptr  prev_con;
  a_constant_ptr  union_designator = NULL, saved_union_init_constant = NULL;
  a_type_ptr      aggr_type = skip_typerefs(aggr_con->type);
  a_boolean       union_init = is_union_type(aggr_type);
  an_init_con_pos con, earlier_con;

  check_assertion(aggr_con->kind == (a_constant_repr_kind)ck_aggregate);
  set_init_con_pos(aggr_con->variant.aggregate.first_constant, &con);
  prev_con = NULL;
  if (earlier_aggr_con != NULL) {
    /* There is an earlier list of constants, being overwritten. */
    check_assertion(earlier_aggr_con->kind ==
                                           (a_constant_repr_kind)ck_aggregate);
    temp_con = earlier_aggr_con->variant.aggregate.first_constant;
    set_init_con_pos(temp_con, &earlier_con);
    if (union_init) {
      /* For a union, see whether the previous initialization and the
         new one initialize the same member. */
      a_constant_ptr prev_union_designator = NULL;
      if (temp_con != NULL &&
          temp_con->kind == (a_constant_repr_kind)ck_designator) {
        /* Take the designator off the old list. */
        prev_union_designator = temp_con;
        advance_init_con_pos(&earlier_con);
        temp_con = temp_con->next;
#if DEBUG
        if (db_flag_is_set("designators")) {
          (void)fprintf(f_debug, "prev_union_designator = ");
          db_constant(prev_union_designator);
          (void)fprintf(f_debug, "\n");
        }  /* if */
#endif /* DEBUG */
      }  /* if */
      if (con.ptr != NULL &&
          con.ptr->kind == (a_constant_repr_kind)ck_designator) {
        /* Take the designator off the new list. */
        union_designator = con.ptr;
        advance_init_con_pos(&con);
        aggr_con->variant.aggregate.first_constant = con.ptr;
      }  /* if */
      /* See how the old and new union initializations interact, and set
         up for correct processing as we continue in this routine. */
      process_union_designators(temp_con,
                                prev_union_designator,
                                union_designator,
                                &earlier_con,
                                &saved_union_init_constant);
    }  /* if */
  } else {
    /* No earlier constant was provided. */
    set_init_con_pos((a_constant_ptr)NULL, &earlier_con);
  }  /* if */
  /* The outer loop is repeated for each ck_designator list found. */
  for (;;) {
    /* Go through the list of constants pointed to by con, looking for
       a ck_designator entry that must be rewritten.  If there is a
       list of previous initialization constants being overwritten
       (earlier_con.ptr != NULL), preserve any part of the old initialization
       that is needed. */
    while (con.ptr != NULL &&
           con.ptr->kind != (a_constant_repr_kind)ck_designator) {
      if (earlier_con.ptr != NULL) {
        /* If merging old and new values, rewrite string constants as
           aggregate initializers to allow operation at the character
           level. */
        explode_string_initializer(&con);
        explode_string_initializer(&earlier_con);
      }  /* if */
      if (con.ptr->kind == (a_constant_repr_kind)ck_aggregate) {
        /* Process a sub-aggregate. */
        a_constant_ptr superseded_con = earlier_con.ptr;
        if (superseded_con != NULL) {
          split_constant_if_repeated(&earlier_con);
          superseded_con = earlier_con.ptr;
          if (superseded_con->kind == (a_constant_repr_kind)ck_dynamic_init) {
            /* The previous initialization sets the whole aggregate with
               a single value.  Save it off to the side and combine it
               with the initializer afterwards. */
            superseded_con = NULL;
          }  /* if */
        }  /* if */
        lower_aggregate_designated_initializers(con.ptr, superseded_con);
        if (superseded_con != earlier_con.ptr) {
          /* See comment above.  Combine the old and new initializers. */
          combine_initializer_constants(earlier_con.ptr, con.ptr);
        }  /* if */
      } else {
        /* Non-aggregate constant. */
        if (earlier_con.ptr != NULL) {
          /* con overwrites an earlier initialization at the same location,
             given by earlier_con. */
          split_constant_if_repeated(&earlier_con);
          split_constant_if_repeated(&con);
          /* Combine the two initializers. */
          combine_initializer_constants(earlier_con.ptr, con.ptr);
#if DEBUG
          if (db_flag_is_set("designators")) {
            (void)fprintf(f_debug, "Combined initializer consts = ");
            db_constant(con.ptr);
            (void)fprintf(f_debug, "\n");
          }  /* if */
#endif /* DEBUG */
        }  /* if */
      }  /* if */
      advance_init_con_pos(&earlier_con);
      prev_con = con.ptr;
      advance_init_con_pos(&con);
    }  /* while */
    /* End of list found, either the real end of list or a ck_designator
       that ends this part of the list. */
    if (con.ptr != NULL) {
      check_assertion(con.ptr->kind == (a_constant_repr_kind)ck_designator);
#if DEBUG
      if (db_flag_is_set("designators")) {
        (void)fprintf(f_debug, "Starting on designator ");
        db_constant(con.ptr);
        (void)fprintf(f_debug, " in aggregate of type ");
        db_abbreviated_type(aggr_con->type);
        (void)fprintf(f_debug, "\n");
        (void)fprintf(f_debug, "aggr_con = ");
        db_constant(aggr_con);
        (void)fprintf(f_debug, "\n");
      }  /* if */
#endif /* DEBUG */
      /* Disconnect the ck_designator and the list that follows it from
         the aggregate. */
      if (prev_con == NULL) {
        aggr_con->variant.aggregate.first_constant = NULL;
      } else {
        prev_con->next = NULL;
      }  /* if */
#if DEBUG
      if (db_flag_is_set("designators")) {
        (void)fprintf(f_debug, "aggr_con after detaching designator = ");
        db_constant(aggr_con);
        (void)fprintf(f_debug, "\n");
      }  /* if */
#endif /* DEBUG */
    }  /* if */
    /* Keep the end of list pointer up to date. */
    aggr_con->variant.aggregate.last_constant = prev_con;
    if (earlier_con.ptr != NULL) {
      /* There are entries on the earlier constants list that initialize
         members beyond the end of the new list.  Move those initializations
         to the new list. */
      if (prev_con == NULL) {
        aggr_con->variant.aggregate.first_constant = earlier_con.ptr;
      } else {
        prev_con->next = earlier_con.ptr;
      }  /* if */
      /* Find the end of the list. */
      while (earlier_con.ptr->next != NULL) {
        earlier_con.ptr = earlier_con.ptr->next;
      }  /* while */
      aggr_con->variant.aggregate.last_constant = earlier_con.ptr;
#if DEBUG
      if (db_flag_is_set("designators")) {
        (void)fprintf(f_debug, "aggr_con after adding to end = ");
        db_constant(aggr_con);
        (void)fprintf(f_debug, "\n");
      }  /* if */
#endif /* DEBUG */
    }  /* if */
    /* Exit the outer loop unless we've run into a ck_designator. */
    if (con.ptr == NULL) break;
    check_assertion(con.ptr->kind == (a_constant_repr_kind)ck_designator);
    /* A ck_designator constant indicates a skip to a new initialization
       position within the aggregate. */
    if (union_init) {
      /* When initializing a union, always insert at the beginning, and
         keep the ck_designator for later re-insertion if it requests
         initialization of a member other than the first. */
      a_constant_ptr prev_union_designator = union_designator;
      prev_con = NULL;
      if (con.ptr->variant.designator.field ==
                  next_initializable_field(
                           aggr_type->variant.class_struct_union.field_list)) {
        /* The ck_designator is not needed when initializing the first
           field. */
        union_designator = NULL;
      } else {
        union_designator = con.ptr;
#if DEBUG
        if (db_flag_is_set("designators")) {
          (void)fprintf(f_debug, "union_designator = ");
          db_constant(union_designator);
          (void)fprintf(f_debug, "\n");
        }  /* if */
#endif /* DEBUG */
      }  /* if */
      /* Overwrite the previous value if it's for the same member of the
         union, otherwise save it off to the side to be combined with
         the final value later. */
      check_assertion(aggr_con->variant.aggregate.first_constant == NULL ||
                      aggr_con->variant.aggregate.first_constant->next==NULL);
      process_union_designators(aggr_con->variant.aggregate.first_constant,
                                prev_union_designator,
                                union_designator,
                                &earlier_con,
                                &saved_union_init_constant);
      aggr_con->variant.aggregate.first_constant = NULL;
    } else {
      /* Array or struct initialization. */
      /* Find the right point to insert the constants after the designator. */
      find_designator_insert_point(con.ptr, aggr_con, &prev_con, &earlier_con);
    }  /* if */
    /* Advance to the constant following the ck_designator. */
    advance_init_con_pos(&con);
    check_assertion(con.ptr != NULL &&
                    con.ptr->kind != (a_constant_repr_kind)ck_designator);
    /* Relink the previous constant (at the insert point) to the first
       constant following the ck_designator. */
    if (prev_con == NULL) {
      aggr_con->variant.aggregate.first_constant = con.ptr;
    } else {
      prev_con->next = con.ptr;
    }  /* if */
#if DEBUG
    if (db_flag_is_set("designators")) {
      (void)fprintf(f_debug, "After relinking around designator = ");
      db_constant(aggr_con);
      (void)fprintf(f_debug, "\n");
    }  /* if */
#endif /* DEBUG */
  }  /* for */
  /* For a union in which more than one member was initialized, combine
     the old and new initializations to preserve any side effects of the
     old initializer. */
  if (saved_union_init_constant != NULL) {
    check_assertion(aggr_con->variant.aggregate.first_constant != NULL &&
                    aggr_con->variant.aggregate.first_constant->next == NULL);
    combine_initializer_constants(saved_union_init_constant,
                                  aggr_con->variant.aggregate.first_constant);
#if DEBUG
    if (db_flag_is_set("designators")) {
      (void)fprintf(f_debug, "After combining union initializers = ");
      db_constant(aggr_con->variant.aggregate.first_constant);
      (void)fprintf(f_debug, "\n");
    }  /* if */
#endif /* DEBUG */
  }  /* if */
  /* For a union initialization, re-insert a ck_designator if the field
     initialized is not the first field. */
  if (union_designator != NULL) {
    union_designator->next = aggr_con->variant.aggregate.first_constant;
    aggr_con->variant.aggregate.first_constant = union_designator;
    check_assertion(union_designator->next != NULL);
#if DEBUG
    if (db_flag_is_set("designators")) {
      (void)fprintf(f_debug, "After reinsertion of union designator = ");
      db_constant(aggr_con);
      (void)fprintf(f_debug, "\n");
    }  /* if */
#endif /* DEBUG */
  }  /* if */
#if EXPENSIVE_CHECKING
  /* Check that the types of the initializer constants match the types
     of the aggregate members to be initialized. */
  { an_aggregate_position aggr_pos;
    an_init_con_pos       con_pos;
    a_constant_ptr        last_con = NULL;
    init_aggregate_position(aggr_con, &aggr_pos);
    temp_con = aggr_con->variant.aggregate.first_constant;
    if (temp_con != NULL &&
        temp_con->kind == (a_constant_repr_kind)ck_designator) {
      /* A ck_designator left in for an initialization of a union member
         other than the first. */
      check_assertion(temp_con->variant.designator.field != NULL);
      set_aggregate_position_for_field(temp_con->variant.designator.field,
                                       &aggr_pos);
      temp_con = temp_con->next;
    }  /* if */
    set_init_con_pos(temp_con, &con_pos);
    while (con_pos.ptr != NULL) {
      temp_con = con_pos.ptr;
      if (temp_con->kind == (a_constant_repr_kind)ck_init_repeat) {
        temp_con = temp_con->variant.init_repeat.constant;
      }  /* if */
      { a_type_ptr con_type = skip_typerefs(temp_con->type);
        a_type_ptr member_type = skip_typerefs(aggr_pos.member_type);
        check_assertion_str(
                     identical_types(con_type, member_type) ||
                     /* A short string literal can initialize a longer
                        char array. */
                     (is_string_type(con_type) &&
                      is_string_type(member_type) &&
                      temp_con->kind == (a_constant_repr_kind)ck_string) ||
                     /* In GNU C mode, zero-length array fields can be
                        initialized with arbitrary-length arrays. */
                     (gcc_mode && is_array_type(con_type) &&
                      is_array_type(member_type) &&
                      skip_typerefs(con_type)->variant.array.bound_is_zero),
                     "lower_aggregate_designated_initializers: type mismatch");
      }
      last_con = con_pos.ptr;
      advance_init_con_pos(&con_pos);
      if (con_pos.ptr != NULL) {
        advance_aggregate_position_to_next_member(&aggr_pos);
      }  /* if */
    }  /* while */
    check_assertion(aggr_con->variant.aggregate.last_constant == last_con);
  }
#endif /* EXPENSIVE_CHECKING */
}  /* lower_aggregate_designated_initializers */


void lower_designated_initializers(a_constant_ptr init_con)
/*
If the initial value constant indicated by init_con contains any
designated initializers, rewrite them as standard C.  Note that this is
called in C mode.
*/
{
  check_assertion(C_mode() || gpp_mode);
  if (!suppress_il_lowering && total_errors == 0) {
    if (init_con->kind == (a_constant_repr_kind)ck_aggregate) {
      lower_aggregate_designated_initializers(init_con,
                                              (a_constant_ptr)NULL);
    }  /* if */
  }  /* if */
}  /* lower_designated_initializers */

#endif /* LOWER_DESIGNATED_INITIALIZERS */

#if !IA64_ABI

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
      if (same_entities(bcp->type, base_class_type)) break;
    }  /* if */
  }  /* for */
#if CHECKING
  { a_type_ptr param_base_type =
                       f_skip_typerefs(type_pointed_to(vbase_param_var->type));
    if (!same_entities(param_base_type, base_class_type) &&
        !same_entities(param_base_type,
                       base_class_type->variant.class_struct_union.
                                              extra_info->type_as_subobject)) {
      internal_error(
                    "implicit_virtual_base_parameter: param type not correct");
    }  /* if */
  }
#endif /* CHECKING */
  return vbase_param_var;
}  /* implicit_virtual_base_parameter */

#endif /* !IA64_ABI */

#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS

static a_variable_ptr make_construction_vtbl_temporary(void)
/*
Make a temporary to be used in a constructor or destructor to point to the
array of special virtual function table pointers.  Return a pointer to
the temporary variable.
*/
{
  a_variable_ptr var;

  var = make_lowered_temporary(make_pointer_type(
                                   make_pointer_type(make_vtbl_entry_type())));
  return var;
}  /* make_construction_vtbl_temporary */


/* Determine whether or not make define_construction_vtbls_array needs to be
   external. */
#if IA64_ABI
#define DEFINE_CONSTRUCTION_VTBLS_ARRAY_LINKAGE /*external*/
#else /* !IA64_ABI */
#define DEFINE_CONSTRUCTION_VTBLS_ARRAY_LINKAGE static
#endif /* !IA64_ABI */

#if !IA64_ABI
/*ARGSUSED*/ /* <-- class_type is unused in that case. */
#endif /* !IA64_ABI */
DEFINE_CONSTRUCTION_VTBLS_ARRAY_LINKAGE
void define_construction_vtbls_array(a_type_ptr              class_type,
                                     a_variable_ptr          var,
                                     a_construction_vtbl_ptr elements)
/* 
Define var, a construction virtual function table array, whose contents are
given by the elements.
*/
{
  a_construction_vtbl_array_index num_elements = 0;
  a_constant_ptr                  aggr_con;
  a_memory_region_number          region_to_switch_back_to;
  a_type_ptr                      array_type;
#if IA64_ABI
  a_class_type_supplement_ptr     ctsp;
#endif /* IA64_ABI */

#if IA64_ABI
  ctsp = class_type->variant.class_struct_union.extra_info;
  if (var == NULL) {
    var = make_construction_vtbls_array(class_type, elements);
  }  /* if */
#endif /* IA64_ABI */
  /* Because the variable is not automatic, it and its initializer must be
     allocated in the file scope memory region. */
  switch_to_file_scope_region(&region_to_switch_back_to);
  /* Allocate an aggregate constant under which the initial values will be
     placed. */
  aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
  /* Go through the list and generate an initializer value for each
     element. */
  for (; elements != NULL; elements = elements->next) {
    a_constant                  con;
    a_constant_ptr              conp;
#if IA64_ABI
    a_virtual_table_index       vtbl_index;
#endif /* IA64_ABI */

    num_elements++;
    /* Make a constant for the address of the virtual function table. */
    set_variable_address_constant(elements->virtual_function_table_var, &con,
                                  /*set_address_taken_flag=*/TRUE);
    /* Do the array --> pointer decay. */
    implicit_cast(&con, make_pointer_type(make_vtbl_entry_type()));
#if IA64_ABI
    /* In the IA64 ABI, the value of the vptr in the object is not the same as
       the address of the virtual function table variable.  */
    vtbl_index = elements->virtual_function_table_index;
    con.variant.address.offset = vtbl_index * make_vtbl_entry_type()->size;
#endif /* IA64_ABI */
    elements->virtual_function_table_var->source_corresp.referenced = TRUE;
    conp = alloc_unshared_constant(&con);
    /* Add the constant to the aggregate constant's list. */
    if (aggr_con->variant.aggregate.first_constant == NULL) {
      aggr_con->variant.aggregate.first_constant = conp;
    } else {
      aggr_con->variant.aggregate.last_constant->next = conp;
    }  /* if */
    aggr_con->variant.aggregate.last_constant = conp;
  }  /* for */
  array_type = var->type;
  array_type->variant.array.variant.number_of_elements = num_elements;
  set_type_size(array_type);
  aggr_con->type = array_type;
  var->init_kind = (an_init_kind)initk_static;
  var->initializer.constant = aggr_con;
#if IA64_ABI
  var->storage_class = ctsp->virtual_function_table_var->storage_class;
  var->comdat_group = ctsp->virtual_function_table_var->comdat_group;
#endif /* IA64_ABI */
  switch_back_to_original_region(region_to_switch_back_to);
}  /* define_construction_vtbls_array */


#if !IA64_ABI
/*ARGSUSED*/ /* <-- class_type is unused in that case. */
#endif /* !IA64_ABI */
static a_variable_ptr make_construction_vtbls_array(
                                           a_type_ptr              class_type,
                                           a_construction_vtbl_ptr elements)
/*
Create an array whose initial value is an array of pointers to virtual
function tables as described by "elements".  class_type gives the type of the
constructor or destructor that we are presently generating.  Return a pointer
to the variable.
*/
{
  a_variable_ptr                  var;
  a_type_ptr                      array_type;
#if IA64_ABI
  a_class_type_supplement_ptr     ctsp;
  char                            *var_name;
#endif /* IA64_ABI */

  check_assertion(elements != NULL);
#if IA64_ABI
  ctsp = class_type->variant.class_struct_union.extra_info;
  if (ctsp->virtual_table_table_var != NULL) {
    /* If the variable has already been created, do not create it again. */
    var = ctsp->virtual_table_table_var;
    goto done;
  }  /* if */
#endif /* IA64_ABI */
  /* Create the array type. */
  array_type = alloc_type((a_type_kind)tk_array);
  array_type->variant.array.element_type = 
                                     make_pointer_type(make_vtbl_entry_type());
#if !IA64_ABI
  /* Create the local static array variable. */
  var = make_unnamed_local_static_variable(array_type,
                                           /*in_function_scope=*/TRUE);
  define_construction_vtbls_array(class_type, var, elements);
#else /* IA64_ABI */
  /* Create the array variable. */
  var_name = mangled_virtual_table_table_name(class_type);
  var = make_lowered_variable(var_name, /*alrady_il_name=*/FALSE, array_type,
                              (a_storage_class)sc_extern);
  var->source_corresp.name_has_been_mangled = TRUE;
  ctsp->virtual_table_table_var = var;
done:
#endif /* IA64_ABI */
  return var;
}  /* make_construction_vtbls_array */


static an_expr_node_ptr vtbl_addr_from_construction_vtbls_array(
                        a_variable_ptr                  construction_vtbls_var,
                        a_boolean                       var_is_array,
                        a_construction_vtbl_array_index index)
/*
Construct an expression for an lvalue for the "index-1"-th element of the
indicated array of special virtual function table values.  Return a pointer
to the expression.  If var_is_array is TRUE, construction_vtbls_var is the
array itself; if FALSE, it is a pointer to the first element of the array.
*/
{
  an_expr_node_ptr expr;

  check_assertion(construction_vtbls_var != NULL);
  if (var_is_array) {
    expr = array_var_lvalue_expr(construction_vtbls_var);
  } else {
    expr = var_rvalue_expr(construction_vtbls_var);
  }  /* if */
  /* Compensate for 0-origin of array versus 1-origin of index. */
  index--;
  if (index != 0) {
    /* The entry is not at offset 0 of the array, so add the right offset. */
    expr->next = node_for_integer_constant((long)index,
                                           targ_size_t_int_kind);
    expr = make_operator_node((an_expr_operator_kind)eok_padd,
                              expr->type,
                              expr);
  }  /* if */
  return expr;
}  /* vtbl_addr_from_construction_vtbls_array */


static void insert_default_construction_vtbls_assignment(
                                a_type_ptr              class_type,
                                a_construction_vtbl_ptr construction_vtbls,
                                a_variable_ptr          construction_vtbls_var,
                                an_insert_location      *insert_location)
/*
Insert an assignment statement to set the construction_vtbls temporary to
point to the default array of virtual function table pointers to be used
when constructing or destroying a complete object.  construction_vtbls
points to a list describing the array contents.  *insert_location indicates
the insert location.  class_type is the type of the class whose constructor or
destructor is being generated.
*/
{
  a_variable_ptr   array_var =
                            make_construction_vtbls_array(class_type, 
                                                          construction_vtbls);
  an_expr_node_ptr array_addr = array_var_lvalue_expr(array_var);

  (void)insert_var_assignment_statement(construction_vtbls_var,
                                        (an_expr_operator_kind)eok_passign,
                                        array_addr,
                                        insert_location);
}  /* insert_default_construction_vtbls_assignment */

#if !IA64_ABI

static an_expr_node_ptr make_construction_vtbl_transfer_pointer_lvalue(
                                                   an_expr_node_ptr expr,
                                                   a_type_ptr       class_type)
/*
expr is an expression for the address of a class object.  class_type
is the type of object pointed to, provided because the underlying type of
expr might be a type-as-subobject.  Modify the expression so that it is
an lvalue for the transfer pointer in the object, and return a pointer
to the modified expression.  The transfer pointer is a virtual function
table pointer or virtual base class pointer within the indicated object
(including non-virtual base classes) which is available to be used to
pass information to a subobject constructor or destructor for the
subobject pointed to by expr.
*/
{
  if (class_type->variant.class_struct_union.any_virtual_functions) {
    /* The class has a virtual function table pointer (possibly allocated
       in and shared with a nonvirtual base class).  Use it as the transfer
       pointer. */
    expr = make_vptr_field_lvalue(expr);
  } else {
    a_base_class_ptr bcp;

    /* Look at the base classes to find a virtual function table pointer in
       a base class or a virtual base class pointer in class_type. */
    for (bcp = base_classes_of(class_type); bcp != NULL; bcp = bcp->next) {
      /* Consider virtual function pointers only in non-virtual base classes,
         i.e., those allocated within class_type. */
      if (!any_virtual_steps_in_derivation(bcp)) {
        if (bcp->type->variant.class_struct_union.any_virtual_functions) {
          /* This base class has a virtual function pointer.  Use that. */
          expr = make_base_class_lvalue(expr, bcp, /*complete_object=*/FALSE);
          expr = make_vptr_field_lvalue(expr);
          goto have_pointer;
        }  /* if */
      }  /* if */
      if (bcp->is_virtual) {
        /* There is a virtual base class pointer to this base class.  Use
           that. */
        expr = make_vbptr_field_lvalue(expr, bcp);
        goto have_pointer;
      }  /* if */
    }  /* for */
#if CHECKING
#if DEBUG
    fprintf(f_debug, "class_type: ");
    db_abbr_type(class_type);
    fprintf(f_debug, "\n");
#endif /* DEBUG */
    unexpected_condition_str2("make_construction_vtbl_transfer_pointer_lvalue",
                              "did not find usable pointer");
#endif /* CHECKING */
  }  /* if */
have_pointer:
  return expr;
}  /* make_construction_vtbl_transfer_pointer_lvalue */

#endif /* !IA64_ABI */

#if IA64_ABI
/*ARGSUSED*/ /* <-- class_type is not used in that case. */
#else /* !IA64_ABI */
/*ARGSUSED*/ /* <-- is_destructor is not used in that case. */
#endif /* !IA64_ABI */
static void receive_construction_vtbls_in_subobject_constructor(
                                     a_variable_ptr     construction_vtbls_var,
                                     a_type_ptr         class_type,
                                     a_variable_ptr     this_param_var,
                                     a_boolean          is_destructor,
                                     an_insert_location *insert_location)
/*
Insert an assignment statement to set the construction_vtbls_var temporary to
the pointer to an array of special virtual function tables passed into
a subobject constructor or destructor via the so-called transfer pointer
in the object.  class_type is the subobject class type.  this_param_var
is the "this" parameter variable for the constructor or destructor.  If
is_destructor is TRUE then we are processing a destructor; otherwise,
we are processing a constructor.
*/
{
#if !IA64_ABI
  an_expr_node_ptr trans_ptr_node;

  trans_ptr_node = var_rvalue_expr(this_param_var);
  /* Get the address of a pointer in the object that is used to
     do the transfer. */
  trans_ptr_node =
                 make_construction_vtbl_transfer_pointer_lvalue(trans_ptr_node,
                                                                class_type);
  trans_ptr_node = add_indirection_to_node(trans_ptr_node);
  trans_ptr_node = add_cast(trans_ptr_node, construction_vtbls_var->type);
  (void)insert_var_assignment_statement(construction_vtbls_var,
                                        (an_expr_operator_kind)eok_passign,
                                        trans_ptr_node,
                                        insert_location);
#else /* IA64_ABI */
  if (is_destructor) {
    /* Skip the complete object variable. */
    this_param_var = this_param_var->next;
  }  /* if */
  (void)insert_var_assignment_statement(construction_vtbls_var,
                                        (an_expr_operator_kind)eok_passign,
                                        var_rvalue_expr(this_param_var->next),
                                        insert_location);
#endif /* IA64_ABI */
}  /* receive_construction_vtbls_in_subobject_constructor */

#if !IA64_ABI

static void pass_construction_vtbls_to_subobject_constructor(
                        a_variable_ptr                  construction_vtbls_var,
                        a_boolean                       var_is_array,
                        a_type_ptr                      subobject_class_type,
                        a_construction_vtbl_array_index index,
                        an_init_pos_descr_ptr           ipdp,
                        an_insert_location              *insert_location)
/*
Insert an assignment statement to store the address of the "index-1"-th
element of the array of special virtual functions pointed to by
construction_vtbls_var into the so-called transfer pointer in the
subobject described by ipdp to pass the array to a subobject constructor
or destructor.  If var_is_array is TRUE, construction_vtbls_var is the
array itself; if FALSE, it is a pointer to the first element of the array.
The subobject class type is subobject_class_type (this is passed because
the type of the expression produced from ipdp may have the type-as-subobject).
*/
{
  an_expr_node_ptr array_addr, trans_ptr_node;

  array_addr = vtbl_addr_from_construction_vtbls_array(construction_vtbls_var,
                                                       var_is_array,
                                                       index);
  /* Get the address of the subobject. */
  trans_ptr_node = make_init_entity_node(ipdp, /*using_as_address=*/FALSE,
                                         /*using_as_dest=*/TRUE);
  /* Get the address of a pointer in the object that is used to
     do the transfer. */
  trans_ptr_node =
          make_construction_vtbl_transfer_pointer_lvalue(trans_ptr_node,
                                                         subobject_class_type);
  array_addr = add_cast(array_addr, type_pointed_to(trans_ptr_node->type));
  (void)insert_assignment_statement(trans_ptr_node,
                                    (an_expr_operator_kind)eok_passign,
                                    array_addr,
                                    insert_location);
}  /* pass_construction_vtbls_to_subobject_constructor */

#endif /* !IA64_ABI */

#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */

#if !ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
/*ARGSUSED*/ /* <-- construction_vtbls_var is not used in that case. */
#endif /* !ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
#if IA64_ABI
/*ARGSUSED*/ /* <-- class_type and use_implicit_param are not used in
                    that case. */
#endif /* IA64_ABI */
static void lower_ctor_init(a_constructor_init_ptr ctor_init,
                            a_variable_ptr         this_param_var,
                            a_boolean              use_implicit_param,
                            a_type_ptr             class_type,
                            a_variable_ptr         construction_vtbls_var,
                            an_insert_location_ptr insert_location)
/*
Generate code to implement the constructor_init entry pointed to by ctor_init.
this_param_var is the "this" parameter variable for the overall object
being initialized, whose class is class_type.  Implicit parameters for
virtual base classes, if any, follow the this_param_var.  If use_implicit_param
is TRUE, the entity being initialized is a virtual base class of class_type
and its address is available in an implicit parameter.  If this
initialization is for a base class whose constructor needs to be passed
an array of special virtual function table addresses, generate code
to do that; construction_vtbls_var provides the variable for the
complete class array if necessary.  The statement(s) created are
inserted at *insert_location, and *insert_location is updated.
*/
{
  a_variable_ptr       param_var;
  an_expr_node_ptr     implied_arg_node;
  an_expr_node_ptr     implied_arg_list = NULL, end_implied_arg_list = NULL;
  a_dynamic_init_ptr   dip;
  an_init_pos_descr    ipd;
  an_init_pos_modifier ipm;

  dip = ctor_init->initializer;
  if (ctor_init->kind == (a_constructor_init_kind)cik_virtual_base_class ||
      ctor_init->kind == (a_constructor_init_kind)cik_direct_base_class) {
    /* Initializing a base class. */
    a_base_class_ptr base_class = ctor_init->variant.base_class;
    a_type_ptr       base_class_type = base_class->type;
#if !IA64_ABI
    a_base_class_ptr bcp;
    /* Develop a position description for the entity to initialize. */
    if (use_implicit_param) {
      /* The sub-entity is a virtual base class and there is a parameter
         pointing to it. */
      param_var = implicit_virtual_base_parameter(class_type,
                                                  base_class_type,
                                                  this_param_var);
      set_var_indirect_init_pos_descr(param_var, &ipd);
      ipd.base_class_subobject = TRUE;
    } else 
#endif /* !IA64_ABI */
    /* Do not add code here. */
    {
      /* Simple case; develop the entity position description. */
      develop_ctor_init_pos_descr(ctor_init, this_param_var, &ipd, &ipm);
    }  /* if */
    /* For a base class initialized by a constructor call, build a list
       of implicit virtual base class pointer arguments.  The required
       entries are expressions providing the value of the associated virtual
       base class pointer parameter for each virtual base class of the base
       class. */
    if (dip->kind == (a_dynamic_init_kind)dik_constructor) {
#if !IA64_ABI
      for (bcp = base_class_type->variant.class_struct_union.extra_info->
                                                                  base_classes;
           bcp != NULL;
           bcp = bcp->next) {
        if (bcp->is_virtual) {
          /* Find the implicit virtual base parameter under the main class
             that is for this same virtual base class. */
          param_var = implicit_virtual_base_parameter(class_type,
                                                      bcp->type,
                                                      this_param_var);
          /* Build an expression specifying the value of the appropriate
             virtual base class parameter, and add it to the list. */
          implied_arg_node = var_rvalue_expr(param_var);
          if (implied_arg_list == NULL) {
            implied_arg_list = implied_arg_node;
          } else {
            end_implied_arg_list->next = implied_arg_node;
          }  /* if */
          end_implied_arg_list = implied_arg_node;
        }  /* if */
      }  /* for */
#else /* IA64_ABI */
      if (base_class_type->variant.class_struct_union.
                                                   any_virtual_base_classes) {
        /* Compute the VTT pointer for the base class. */
        param_var = this_param_var->next;
        if (base_class->base_subarray_index_in_construction_vtbl_array != 0) {
          /* The base class requires a VTT parameter; find the right entry in
             the VTT. */
          check_assertion(construction_vtbls_var != NULL);
          implied_arg_node = vtbl_addr_from_construction_vtbls_array(
                   construction_vtbls_var,
                   /*var_is_array=*/FALSE,
                   base_class->base_subarray_index_in_construction_vtbl_array);
        } else {
          /* The base class does not make use of the VTT parameter; just pass a
             NULL pointer. */
          a_constant null_constant;
          make_zero_of_proper_type(param_var->type, &null_constant);
          implied_arg_node = alloc_node_for_constant(&null_constant);
        }  /* if */
        implied_arg_list = end_implied_arg_list = implied_arg_node;
      }  /* if */
      /* Use the subobject entry point. */
      dip->variant.constructor.ptr = 
                     alternate_entry_point(dip->variant.constructor.ptr,
                                           (a_ctor_or_dtor_kind)cdk_subobject,
                                           /*define_now=*/FALSE);
#endif /* IA64_ABI */
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS && !IA64_ABI
      /* See if the base class constructor needs to be passed an array
         of virtual function table pointers to use during the subobject
         construction.  If so, pass it by setting the transfer pointer
         to the address of the proper array. */
      if (base_class->base_subarray_index_in_construction_vtbl_array != 0) {
        check_assertion(!base_class->is_virtual);
        /* Yes, this base class constructor needs the special information.
           Pass the address of a subarray of the overall class array of
           virtual function table pointers. */
        check_assertion(construction_vtbls_var != NULL);
        pass_construction_vtbls_to_subobject_constructor(
                  construction_vtbls_var,
                  /*var_is_array=*/FALSE,
                  base_class->type,
                  base_class->base_subarray_index_in_construction_vtbl_array,
                  &ipd,
                  insert_location);
      } else if (base_class->is_virtual) {
        if (base_class->base_construction_vtbls != 0) {
          /* Yes, this virtual base class constructor needs the
             special information.  Pass the address of an array of
             virtual function table pointers specific to this case.
             Note that we are calling the constructor directly from
             the constructor for a complete object. */
          a_variable_ptr array_var =
                            make_construction_vtbls_array(
                                          class_type,
                                          base_class->base_construction_vtbls);
          pass_construction_vtbls_to_subobject_constructor(
                            array_var,
                            /*var_is_array=*/TRUE,
                            base_class->type,
                            (a_construction_vtbl_array_index)1,
                            &ipd,
                            insert_location);
        }  /* if */
      }  /* if */
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS && !IA64_ABI */
    }  /* if */
  } else {
    /* Initializing something other than a base class. */
    /* Develop a position description for the entity to initialize. */
    develop_ctor_init_pos_descr(ctor_init, this_param_var, &ipd, &ipm);
  }  /* if */
  /* Generate the code to do the initialization. */
  lower_dynamic_init(dip, &ipd,
                     implied_arg_list, end_implied_arg_list, ctor_init,
                     LDIO_FULL_EXPR, /*others_follow_in_aggr=*/FALSE,
                     insert_location, (a_boolean *)NULL,
                     (a_constant **)NULL);
}  /* lower_ctor_init */


#if !IA64_ABI
/*ARGSUSED*/ /* <-- ctor_vtbl_var is not used in that case. */
#endif /* !IA64_ABI */
static
void insert_primary_vtbl_assignment(a_type_ptr             class_type,
                                    a_variable_ptr         this_param_var,
                                    a_variable_ptr         ctor_vtbl_var,
                                    an_insert_location_ptr insert_location)
/*
If class_type has a virtual function table, set the vptr in the object pointed
to by this_param_var to that virtual function table.  If ctor_vtbl_var
is non-NULL, the primary virtual function table can be found in the 
location pointed to by the ctor_vtbl_var.  Otherwise, the primary 
virtual function table used is the virtual function table for class_type.
Insert the code at the location given by insert_location.
*/
{
  a_variable_ptr              primary_vtbl_var;
  an_expr_node_ptr            vtbl_addr_node, vptr_node;
  a_class_type_supplement_ptr ctsp;

#if IA64_ABI
  if (ctor_vtbl_var != NULL) {
    vtbl_addr_node = add_indirection_to_node(var_rvalue_expr(ctor_vtbl_var));
  } else
#endif /* IA64_ABI */
  /* Do not add code here. */
  {
    ctsp = class_type->variant.class_struct_union.extra_info;
    primary_vtbl_var = ctsp->virtual_function_table_var;
    if (primary_vtbl_var != NULL) {
      vtbl_addr_node = make_vtbl_address_node(primary_vtbl_var, class_type,
                                              (a_base_class_ptr)NULL);
      set_lowering_variable_address_taken(primary_vtbl_var);
      primary_vtbl_var->source_corresp.referenced = TRUE;
    } else {
      vtbl_addr_node = NULL;
    } /* if */
  }  /* if */
  if (vtbl_addr_node != NULL) {
    /* Assign the primary virtual table address to the virtual table
       pointer in the current class. */
    vptr_node = make_vptr_field_lvalue_from_var(this_param_var);
    (void)insert_assignment_statement(vptr_node,
                                      (an_expr_operator_kind)eok_passign,
                                      vtbl_addr_node,
                                      insert_location);
  }  /* if */
}  /* insert_primary_vtbl_assignment */
                                     
#if IA64_ABI

static a_boolean is_direct_or_indirect_virtual_primary_base(
                                                        a_base_class_ptr  bcp)
/*
Return TRUE if and only if the given base class is a direct or indirect
primary base class.
*/
{
  a_boolean         result;
  a_type_ptr        class_type = bcp->derived_class;
  a_base_class_ptr  primary_bcp =
                             class_type->variant.class_struct_union.extra_info
                                       ->primary_base_class;

  check_assertion(bcp->is_virtual);
  for (;;) {
    if (primary_bcp == bcp) {
      result = TRUE;
      break;
    } else if (primary_bcp == NULL) {
      result = FALSE;
      break;
    } else {
      a_base_class_ptr  descendent_bcp = primary_bcp;
      primary_bcp = primary_bcp->type->variant.class_struct_union.extra_info
                                     ->primary_base_class;
      if (primary_bcp != NULL) {
        a_base_class_ptr  disambiguator =
                              find_disambiguator(descendent_bcp, primary_bcp);
        primary_bcp = corresponding_base_class(primary_bcp, class_type,
                                               disambiguator);
      }  /* if */
    }  /* if */
  }  /* for */
  return result;
}  /* is_direct_or_indirect_virtual_primary_base */

#endif /* IA64_ABI */

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
  an_insert_location     insert_location2, else_insert_location;
  an_expr_node_ptr       null_constant_node, vbase_param_node, compare_node;
#if !IA64_ABI
  an_expr_node_ptr       vaddr_node, vbptr_node, assign_node;
#endif /* !IA64_ABI */
  an_expr_node_ptr       vtbl_addr_node, vptr_node, complete_var_node;
  a_variable_ptr         vtbl_var;
  a_source_position      saved_error_position, saved_code_pos;
  a_variable_ptr         construction_vtbls_var = NULL;
  a_variable_ptr         complete_var;
#if DO_FULL_PORTABLE_EH_LOWERING
  a_handle_number        complete_var_handle;
#endif /* DO_FULL_PORTABLE_EH_LOWERING */

  /* The following pseudo-code shows both the processing in this routine
     and the code added to the constructor routine.  Lines enclosed in [...]
     are tests and loops done in the processing in this routine; other
     lines are the code added to the constructor routine.

     [If current class has any virtual base classes:]
       int complete = (first added parameter == NULL);
           (indicating a complete object is being initialized and virtual base
            classes must be constructed)
       If complete:
         Set the construction_vtbls temp to point to a local static array
           containing vtbl pointer values to be used for a complete object.
#if !IA64_ABI
         [For each virtual base class of the current class:]
           Set the parameter to the address of the virtual base class.
           [If the virtual base class pointer for the base class is allocated
               in the current class (the pointers allocated in base classes
               are set by the constructor calls for those base classes):]
             Initialize the virtual base class pointer to point to the base
                 class, using the address just computed.
           [endif]
         [endfor]
#endif // !IA64_ABI
         [For each virtual base class on the ctor-initializer list:]
           Call the constructor for the base class (arguments as indicated by
               the ctor-initializer list, plus any virtual base class pointer
               arguments, using the added parameters for those).  If the
               constructor needs an array of construction vtbl pointers,
               store the address of an array specific to this base class
               in the transfer pointer in the subobject, as a way of
               passing that information to the subobject constructor.
         [endfor]
       else (not initializing a complete object)
         Set the construction_vtbls temp to the value in the transfer pointer
           in the class (the caller uses that to pass in the address of
           the array of vtbl pointers to be used during the subobject
           construction).
#if !IA64_ABI
         [For each virtual base class of the current class:]
           [If the virtual base class pointer for the base class is allocated
               in the current class:]
             Initialize the virtual base class pointer to point to the base
                 class, using the address from the corresponding parameter.
           [endif]
         [endfor]
#endif // !IA64_ABI
       endif
     [endif]
     [For each initialized direct nonvirtual base class (entries for these
         appear as the middle of the ctor-initializer list):]
       Call the constructor for the base class (arguments as indicated by
         the ctor-initializer list, plus any virtual base class pointer
         arguments, using the added parameters for those).  If the
         constructor needs an array of construction vtbl pointers, store
         the address of the proper subarray of the array pointed to by
         the construction_vtbls temp into the transfer pointer of the
         subobject, as a way of passing that information to the subobject
         constructor.
     [endfor]
     [If the current class has any virtual functions:]
       Set the virtual function table pointer in the current class.
     [endif]
     [For each base class of the current class:]
       [If the base class needs a virtual function table instance distinct
           from the derived class instance:]
         Set the virtual function table pointer in the base class.  Virtual
             base classes must be accessed using the virtual base class
             pointer parameters.  If the construction_vtbls temp is in use,
             copy the proper element of the array to the virtual function
             table pointer instead of using a specific virtual function table
             instance.
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
  set_position_from_stmt_source_position(code_pos_for_lowering,
                                         scope->assoc_block->position);
  error_position = code_pos_for_lowering;
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
  class_type = scope->variant.routine.ptr->source_corresp.parent.class_type;
  /* Mark the class as referenced because, at the very least, the
     "this" parameter uses it. */
  class_type->source_corresp.referenced = TRUE;
  ctsp = class_type->variant.class_struct_union.extra_info;
  if (class_type->variant.class_struct_union.any_virtual_base_classes) {
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
    if (ctsp->construction_vtbls != NULL) {
      /* This class is one that has overridden virtual functions in virtual
         base classes, and needs special versions of the virtual function
         tables when used to construct a subobject. */
      /* Create a temporary that will point to an array of virtual function
         table addresses. */
      construction_vtbls_var = make_construction_vtbl_temporary();
    }  /* if */
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
    /* Put out code that tests whether or not the virtual base classes need
       to be initialized.  This is done by testing whether or not the
       first added parameter is NULL.  A local variable (called "complete"
       in the pseudocode above) is initialized to TRUE if a complete
       object is being initialized. */
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
    /* Set the local variable. */
    complete_var = make_lowered_temporary(
                                        integer_type((an_integer_kind)ik_int));
    (void)insert_var_assignment_statement(complete_var,
                                          (an_expr_operator_kind)eok_iassign,
                                          compare_node,
                                          insert_location);
#if DO_FULL_PORTABLE_EH_LOWERING
    if (exceptions_enabled) {
      an_init_pos_descr ipd;
      /* Assign the object address table slot for the conditional
         variable. */
      complete_var_handle = object_addr_table_index();
      /* Put the address of the variable into the object address table. */
      set_var_init_pos_descr(complete_var, &ipd);
      init_object_addr_table_entry(&ipd, complete_var_handle,
                                   insert_location);
    }  /* if */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
    /* Make an "if" statement with a block statement under it:
         if (complete != 0) {}
                             ^--- additional statements will be inserted.
    */
    complete_var_node = var_rvalue_expr(complete_var);
    complete_var_node->next = 
                        node_for_integer_constant(0L, (an_integer_kind)ik_int);
    compare_node = make_operator_node((an_expr_operator_kind)eok_ine,
                                      integer_type((an_integer_kind)ik_int),
                                      complete_var_node);
    insert_if_statement(compare_node,
                        /*is_initialization_guard=*/FALSE,
                        insert_location, (a_statement_ptr *)NULL,
                        &insert_location2, &else_insert_location);
    /* Inserting under insert_location2, in the "then" part of the "if"
       (a complete object is being initialized): */
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
    if (construction_vtbls_var != NULL) {
      /* Set the construction_vtbls temporary to point to the default array
         of virtual function table pointers to be used when constructing a
         complete object. */
      insert_default_construction_vtbls_assignment(class_type,
                                                   ctsp->construction_vtbls,
                                                   construction_vtbls_var,
                                                   &insert_location2);
    }  /* if */
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
#if !IA64_ABI
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
        assign_node = make_var_assignment_expr(vbase_param_var,
                                            (an_expr_operator_kind)eok_passign,
                                               vaddr_node);
        /* Set the base class pointer if it is allocated in this class.
           If it is shared with a base class, the base class constructor
           will set it. */
        if (bcp->pointer_base_class == NULL) {
          /* Make an expression node for the address of the virtual base
             class pointer. */
          vbptr_node = make_vbptr_field_lvalue_from_var(this_param_var, bcp);
          /* Add a cast if necessary to convert from a pointer to the
             type-as-subobject for the base class type to a pointer to the
             base class type. */
          assign_node = add_cast_if_necessary(assign_node,
                                            type_pointed_to(vbptr_node->type));
          /* Assign the base class address to the virtual base class
             pointer. */
          vbptr_node->next = assign_node;
          assign_node = make_operator_node((an_expr_operator_kind)eok_passign,
                                           assign_node->type, vbptr_node);
        }  /* if */
        /* Insert the assignment statement. */
        (void)insert_expr_statement(assign_node, &insert_location2);
        /* Move on to the next added parameter for the next iteration of
           the loop. */
        vbase_param_var = vbase_param_var->next;
      }  /* if */
    }  /* for */
#endif /* !IA64_ABI */
    /* Initialize any virtual base classes on the ctor_init list. */
    for (; ctor_init != NULL &&
            ctor_init->kind == (a_constructor_init_kind)cik_virtual_base_class;
         ctor_init = ctor_init->next) {
      if (ctor_init->initializer->destructor != NULL) {
        /* Add complete_var as a conditional flag.  The virtual base
           class should be destroyed only if it was constructed in this
           constructor. */
        a_destructible_entity_descr_ptr dedp = 
                             ctor_init->initializer->destructible_entity_descr;
        check_assertion(dedp != NULL);
        dedp->conditional_flag_var = complete_var;
#if DO_FULL_PORTABLE_EH_LOWERING
        if (exceptions_enabled) {
          dedp->conditional_flag_handle = complete_var_handle;
        }  /* if */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
      }  /* if */
#if IA64_ABI
      /* Set the virtual table pointer for the complete object so that we 
         can find the virtual base. */
      insert_primary_vtbl_assignment(class_type, this_param_var,
                                     construction_vtbls_var,
                                     &insert_location2);
#endif /* IA64_ABI */
      lower_ctor_init(ctor_init, this_param_var, /*use_implicit_param=*/TRUE,
                      class_type, construction_vtbls_var, &insert_location2);
    }  /* for */
    /* Inserting under else_insert_location, in the "else" of the "if"
       (a subobject is being initialized): */
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
    if (construction_vtbls_var != NULL) {
      /* Copy the value of the transfer pointer to the local
         construction_vtbls temporary.  The caller constructor uses the
         transfer pointer to pass information down to the subclass
         constructor. */
      receive_construction_vtbls_in_subobject_constructor(
                                                       construction_vtbls_var,
                                                       class_type,
                                                       this_param_var,
                                                       /*is_destructor=*/FALSE,
                                                       &else_insert_location);
    }  /* if */
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
#if !IA64_ABI
    /* For each virtual base class of the current class, set the
       virtual base class pointer in the current class to point to the value
       of the associated virtual base class parameter, i.e., the address
       of the virtual base class. */
    vbase_param_var = this_param_var;
    for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
      if (bcp->is_virtual) {
        vbase_param_var = vbase_param_var->next;
        /* Do not set the pointer if it is shared with a base class. */
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
             value into the virtual base class pointer. */
          (void)insert_assignment_statement(vbptr_node,
                                            (an_expr_operator_kind)eok_passign,
                                            vbase_param_node,
                                            &else_insert_location);
        }  /* if */
      }  /* if */
    }  /* for */
#endif /* !IA64_ABI */
  }  /* if */
  /* Generate initialization for each non-virtual base class that appears on
     the ctor_init list. */
  for (; ctor_init != NULL &&
          ctor_init->kind == (a_constructor_init_kind)cik_direct_base_class;
       ctor_init = ctor_init->next) {
#if IA64_ABI
    /* Set the virtual table pointer for the complete object so that we 
       can find the bases. */
    insert_primary_vtbl_assignment(class_type, this_param_var,
                                   construction_vtbls_var, insert_location);
#endif /* IA64_ABI */
    lower_ctor_init(ctor_init, this_param_var, /*use_implicit_param=*/FALSE,
                    class_type, construction_vtbls_var, insert_location);
  }  /* for */
  /* If the current class has any virtual functions, generate code to
     set the virtual function table pointer in the current class. */
  insert_primary_vtbl_assignment(class_type, this_param_var,
                                 construction_vtbls_var, insert_location);
  /* Set the virtual function table pointer in any base classes for which
     that is required. */
  /* Loop through the base classes of the current class. */
  for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
    /* Set the pointer if there is one. */
#if !IA64_ABI
    vtbl_var = bcp->virtual_function_table_var;
#else /* IA64_ABI */
    if (needs_virtual_function_table(bcp->type)) {
      vtbl_var = ctsp->virtual_function_table_var;
    } else {
      vtbl_var = NULL;
    }  /* if */
#endif /* IA64_ABI */
    if (vtbl_var != NULL) {
      /* The base class's virtual function table pointer must be set to
         reflect the fact that it exists as a subobject inside the current
         class. */
      vtbl_addr_node = NULL;
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
      if (bcp->index_in_construction_vtbl_array != 0) {
        /* The virtual function table to use is specified by an element of the
           array of construction virtual function table pointers. */
        vtbl_addr_node = vtbl_addr_from_construction_vtbls_array(
                                        construction_vtbls_var,
                                        /*var_is_array=*/FALSE,
                                        bcp->index_in_construction_vtbl_array);
        vtbl_addr_node = add_indirection_to_node(vtbl_addr_node);
      } else
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
      /* Do not insert code here; this is the "else" of an "if". */
#if IA64_ABI
      if (base_class_has_vtbl(bcp))
#endif /* IA64_ABI */
      {
        vtbl_addr_node = make_vtbl_address_node(vtbl_var, class_type, bcp);
        set_lowering_variable_address_taken(vtbl_var);
        vtbl_var->source_corresp.referenced = TRUE;
      }  /* if */
      if (vtbl_addr_node != NULL) {
#if !IA64_ABI
        if (bcp->is_virtual) {
          /* For virtual base classes, access the class by using the implicit
             parameter.  That works even when the current class is not a
             complete object, and is a little better than the general code. */
          vbase_param_var = implicit_virtual_base_parameter(class_type,
                                                            bcp->type,
                                                            this_param_var);
          vptr_node = var_rvalue_expr(vbase_param_var);
        } else 
#endif /* !IA64_ABI */
        /* Do not insert code here. */
        {
          /* Use the usual code.  Note that if the base class here is
             non-virtual itself but is inside a virtual base class, the code
             will use a pointer to get to the virtual base class and then
             field selection(s) to get to the non-virtual base class within
             that.  It would be possible to use the implicit parameter for the
             virtual base class to do better, but this code works (the virtual
             base class pointers are all set by this point). */
          vptr_node = make_base_class_lvalue_from_var(
                                                   this_param_var, bcp,
                                                   /*complete_object=*/FALSE);
        }  /* if */
        vptr_node = make_vptr_field_lvalue(vptr_node);
        /* Make and insert the assignment statement. */
        (void)insert_assignment_statement(vptr_node,
                                          (an_expr_operator_kind)eok_passign,
                                          vtbl_addr_node,
                                          insert_location);
#if IA64_ABI
        if (bcp->is_virtual &&
            is_direct_or_indirect_virtual_primary_base(bcp)) {
          /* If a primary virtual base is located at the origin of the
             subobject being constructed, we should not have clobbered its
             virtual table pointer.  We could devise a run-time test to
             detect such cases, but it's simpler and probably just as
             efficient to reload the primary virtual table pointer of the
             subobject being constructed. */
          insert_primary_vtbl_assignment(class_type, this_param_var,
                                         construction_vtbls_var,
                                         insert_location);
        }  /* if */
#endif /* IA64_ABI */
      }  /* if */
    }  /* if */
  }  /* for */
  /* Generate initialization for each data member that appears on the
     ctor_init list. */
  for (; ctor_init != NULL; ctor_init = ctor_init->next) {
    lower_ctor_init(ctor_init, this_param_var, /*use_implicit_param=*/FALSE,
                    class_type, (a_variable_ptr)NULL, insert_location);
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
  a_statement_ptr    user_code_stmts;
  a_boolean          has_function_try_block = FALSE;
  a_statement_ptr    top_stmt = scope->assoc_block;
  a_statement_ptr    last_statement, wrapper_code = NULL;
  an_insert_location insert_location;
  a_source_position  saved_error_position, saved_code_pos;
#if NEW_CAN_BE_FOLDED_INTO_CTOR || ASSIGNMENT_TO_THIS_ALLOWED
  a_routine_ptr      ctor_routine = scope->variant.routine.ptr;
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR || ASSIGNMENT_TO_THIS_ALLOWED */
#if NEW_CAN_BE_FOLDED_INTO_CTOR
  a_type_ptr         class_type =
                          ctor_routine->source_corresp.parent.class_type;
  a_class_type_supplement_ptr
                     ctsp = class_type->variant.class_struct_union.extra_info;
  a_routine_ptr      new_routine = ctsp->assoc_operator_new_routine;
  a_variable_ptr     this_param_var = scope->variant.routine.parameters;
  an_expr_node_ptr   if_node;
#if GENERATE_EH_TABLES
  a_destructible_entity_descr_ptr
                     dedp = NULL;
#endif /* GENERATE_EH_TABLES */
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */

  saved_code_pos = code_pos_for_lowering;
  saved_error_position = error_position;
  set_position_from_stmt_source_position(code_pos_for_lowering,
                                         top_stmt->position);
  error_position = code_pos_for_lowering;
  if (top_stmt->kind == (a_statement_kind)stmk_try_block) {
    /* This constructor has a function-try-block as the top statement. */
    has_function_try_block = TRUE;
    /* Add a compound statement as the top statement of the function. */
    put_block_around_try_block(top_stmt, &insert_location, &user_code_stmts);
  } else {
    /* Normal case -- no function-try-block */
    check_assertion(top_stmt->kind == (a_statement_kind)stmk_block);
    user_code_stmts = top_stmt->variant.block.statements;
    set_block_start_insert_location(top_stmt, &insert_location);
  }  /* if */
  /* Start an object lifetime if appropriate. */
  begin_block_object_lifetime(scope->lifetime, &insert_location);
#if ASSIGNMENT_TO_THIS_ALLOWED
  /* Assignment to "this" is allowed. */
  /* If there is an assignment to "this" in the body of the constructor,
     do not issue the wrapper code here; it is issued after each
     assignment to "this". */
  if (!ctor_routine->assignment_to_this_done)
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
  /* Don't insert code here. */
  {
#if NEW_CAN_BE_FOLDED_INTO_CTOR
    /* Add code to allocate storage if "this" is NULL:
         if (this != NULL || (this = new-rout(size)) != NULL)
       The entire rest of the routine (both wrapper code and user code)
       is placed in the dependent statement of the "if". */
    /* Ordering issue: we want to do the call of make_region_table_entry
       before any region table entries have been created for anything else,
       but we don't want to enclose the whole routine in an "if" until the
       user code has been lowered, because we want cleanup code emitted
       on the return at the end of the user code.  So we do everything
       short of inserting the "if" and do that at the end. */
    a_type_ptr         int_type, unqual_this_param_type;
    an_expr_node_ptr   size_node, call_node, assign_node;
    an_expr_node_ptr   new_compare_node, this_param_node;
    an_expr_node_ptr   null_constant_node, this_compare_node;
    a_constant         null_constant;

    /* If there is no default new routine for the class, do not put out
       the code.  This happens if the class has a class-specific new but
       not one that takes a single argument. */
    if (new_routine != NULL) {
      /* Make "new-rout(size)". */
      size_node = node_for_host_large_integer(
                                        (a_host_large_integer)class_type->size,
                                        targ_size_t_int_kind);
      call_node = make_call_node(new_routine, size_node,
                                 /*honor_virtual=*/FALSE,
                                 (an_insert_location *)NULL);
      /* Make "this = new_rout(size)". */
      unqual_this_param_type = f_skip_typerefs(this_param_var->type);
      call_node = add_cast_if_necessary(call_node, unqual_this_param_type);
      assign_node = make_var_assignment_expr(this_param_var,
                                            (an_expr_operator_kind)eok_passign,
                                             call_node);
      if (exceptions_enabled &&
          /* The delete routine pointer can be null if the operator delete for
             the class is ambiguous. */
          ctsp->assoc_operator_delete_routine != NULL) {
        an_insert_location expr_insert_location;
        an_init_pos_descr  ipd;
        a_dynamic_init_ptr dyn_init_to_free_storage;

        /* Exceptions are enabled.  Record the allocation so it can
           be freed if a throw occurs while this routine is running. */
        /* "this = new_rout(size)" is turned into
             (this = new_rout(size), (exception_code, this))
        */
        this_param_node = var_rvalue_expr(this_param_var);
        assign_node = make_comma_node(assign_node, this_param_node);
        set_expr_insert_location(this_param_node, &expr_insert_location);
        /* Make a dynamic initialization entry that describes the deletion. */
        dyn_init_to_free_storage =
                             alloc_dynamic_init((a_dynamic_init_kind)dik_none);
        dyn_init_to_free_storage->destructor =
                                           ctsp->assoc_operator_delete_routine;
        dyn_init_to_free_storage->has_temporary_lifetime = TRUE;
        dyn_init_to_free_storage->is_freeing_of_storage_on_exception = TRUE;
        /* The front end is supposed to guarantee that a constructor of
           this kind has an object lifetime even if it has no other
           destructions. */
        check_assertion_str(scope->lifetime != NULL,
                            "lower_constructor_code: no lifetime");
        /* Add the dynamic initialization to the object lifetime list. */
        add_to_end_of_destructions_list(dyn_init_to_free_storage,
                                        scope->lifetime);
        /* Allocate a destructible entity description and add a conditional
           flag variable. */
        /* Note that NULL for the insert location here indicates that
           no initialization code should be added (it gets added below). */
        initial_processing_on_destructible_initialization(
                                                    dyn_init_to_free_storage,
                                                    (an_insert_location*)NULL);
#if GENERATE_EH_TABLES
        dedp = dyn_init_to_free_storage->destructible_entity_descr;
#endif /* GENERATE_EH_TABLES */
        set_var_indirect_init_pos_descr(this_param_var, &ipd);
        check_assertion(curr_context->latest_initialization == NULL);
        /* Add cleanup information. */
        add_dyn_init_cleanup(dyn_init_to_free_storage, &ipd,
                             /*set_cond_flag_if_any=*/TRUE,
                             curr_context, &expr_insert_location);
      }  /* if */
      /* Make "(this = new_rout(size)) != NULL". */
      make_zero_of_proper_type(unqual_this_param_type, &null_constant);
      null_constant_node = alloc_node_for_constant(&null_constant);
      assign_node->next = null_constant_node;
      int_type = integer_type((an_integer_kind)ik_int);
      new_compare_node = make_operator_node((an_expr_operator_kind)eok_pne,
                                            int_type, assign_node);
      /* Make "this != NULL || (this = new-rout(size)) != NULL". */
      this_param_node = var_rvalue_expr(this_param_var);
      make_zero_of_proper_type(unqual_this_param_type, &null_constant);
      null_constant_node = alloc_node_for_constant(&null_constant);
      this_param_node->next = null_constant_node;
      this_compare_node = make_operator_node((an_expr_operator_kind)eok_pne,
                                             int_type, this_param_node);
      this_compare_node->next = new_compare_node;
      if_node = make_operator_node((an_expr_operator_kind)eok_lor,
                                   int_type, this_compare_node);
      /* The "if" statement is inserted later. */
    }  /* if */
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
    if (has_function_try_block) {
      /* When the constructor has a function-try-block, generate the wrapper
         code now in a block off to the side, and insert it later while
         lowering the try statement.  The code must be generated now so
         it's in the right current object lifetime. */
      wrapper_code = alloc_statement((a_statement_kind)stmk_block);
      set_block_start_insert_location(wrapper_code, &insert_location);
    }  /* if */
    /* Generate member and base initialization code. */
    add_constructor_wrapper_code(scope, &insert_location);
    if (has_function_try_block) {
      /* If no code was generated, throw away the block for the wrapper
         code. */
      if (wrapper_code->variant.block.statements == NULL) {
        wrapper_code = NULL;
      }  /* if */
    }  /* if */
  }
  /* Lower the user code in the constructor. */
  if (has_function_try_block) {
    lower_try_block(user_code_stmts, /*is_function_try_block=*/TRUE,
                    wrapper_code,
                    (a_destructor_wrapper_info_block_ptr)NULL);
  } else {
    lower_statement_list(user_code_stmts, &last_statement);
  }  /* if */
#if NEW_CAN_BE_FOLDED_INTO_CTOR
#if ASSIGNMENT_TO_THIS_ALLOWED
  /* Again, if an assignment to "this" was done, the wrapper code is
     not generated. */
  if (!ctor_routine->assignment_to_this_done)
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
  /* Don't insert code here. */
  {
    if (new_routine != NULL) {
      /* Insert an "if" around the whole routine, specifically
         "if (this != NULL || (this = new-rout(size)) != NULL)".
         As mentioned above, this must be done after the user code is
         lowered. */
      enclose_routine_in_if(scope, if_node, this_param_var);
#if GENERATE_EH_TABLES
      if (exceptions_enabled &&
          /* dedp is NULL if the operator delete is ambiguous. */
          dedp != NULL && dedp->conditional_flag_var != NULL) {
        /* Initialize the conditional flag to zero.  This must be done after
           enclose_routine_in_if is called so that the initialization is
           done at the right place (i.e., outside the "if"). */
        set_block_start_insert_location(top_stmt, &insert_location);
        init_conditional_flag_var(dedp, &insert_location);
      }  /* if */
#endif /* GENERATE_EH_TABLES */
    }  /* if */
  }
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
  /* Clear the list of constructor inits (it can't be cleared by
     add_constructor_wrapper_code because that routine can be called
     more than once when assignments to "this" are present). */
  scope->variant.routine.constructor_inits = NULL;
  error_position = saved_error_position;
  code_pos_for_lowering = saved_code_pos;
}  /* lower_constructor_code */


#if !ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
/*ARGSUSED*/ /* <-- destruction_vtbls_var is not used in that case. */
#endif /* !ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
static void lower_dtor_init(a_constructor_init_ptr ctor_init,
                            a_variable_ptr         this_param_var,
                            a_boolean              have_complete_object,
                            a_variable_ptr         destruction_vtbls_var,
                            an_insert_location_ptr insert_location)
/*
Generate code to implement the constructor_init entry pointed to by ctor_init,
one that appears on the constructor_init list for a destructor.
this_param_var is the "this" parameter variable for the overall object
being destroyed.  If have_complete_object is TRUE, the entity being
destroyed is a complete object.  If this destruction is for a base
class whose destructor needs to be passed an array of special virtual
function table addresses, generate code to do that; destruction_vtbls_var
provides the variable for the complete class array if necessary.
The statements created are inserted at *insert_location, and
*insert_location is updated.
*/
{
  an_init_pos_descr    ipd;
  an_init_pos_modifier ipm;
  a_dynamic_init_ptr   dip;
  a_boolean            keep_constant;
  an_expr_node_ptr     vtt_addr_node = NULL;

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
                                          /*others_follow_in_aggr=*/FALSE,
                                          insert_location,
                                          &keep_constant);
#if CHECKING
    if (keep_constant) {
      internal_error("lower_dtor_init: keep_constant unexpected");
    }  /* if */
#endif /* CHECKING */
  } else {
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
    if (ctor_init->kind == (a_constructor_init_kind)cik_virtual_base_class ||
        ctor_init->kind == (a_constructor_init_kind)cik_direct_base_class) {
      a_base_class_ptr base_class = ctor_init->variant.base_class;
      /* See if the base class destructor needs to be passed an array
         of virtual function table pointers to use during the subobject
         destruction.  If so, pass it by setting the transfer pointer
         to the address of the proper array. */
      if (base_class->base_subarray_index_in_construction_vtbl_array != 0) {
#if !IA64_ABI
        check_assertion(!base_class->is_virtual);
        /* Yes, this base class destructor needs the special information.
           Pass the address of a subarray of the overall class array of
           virtual function table pointers. */
        check_assertion(destruction_vtbls_var != NULL);
        pass_construction_vtbls_to_subobject_constructor(
                  destruction_vtbls_var,
                  /*var_is_array=*/FALSE,
                  base_class->type,
                  base_class->base_subarray_index_in_construction_vtbl_array,
                  &ipd,
                  insert_location);
#else /* IA64_ABI */
        vtt_addr_node = vtbl_addr_from_construction_vtbls_array(
                   destruction_vtbls_var,
                   /*var_is_array=*/FALSE,
                   base_class->base_subarray_index_in_construction_vtbl_array);
#endif /* IA64_ABI */
#if !IA64_ABI
      } else if (base_class->is_virtual) {
        if (base_class->base_construction_vtbls != 0) {
          /* Yes, this base class destructor needs the special information.
             Pass the address of an array of virtual function table pointers
             specific to this case.  Note that we are calling the destructor
             directly from the destructor for a complete object. */
          a_variable_ptr array_var =
                            make_construction_vtbls_array(
                                          base_class->derived_class,
                                          base_class->base_construction_vtbls);
          pass_construction_vtbls_to_subobject_constructor(
                            array_var,
                            /*var_is_array=*/TRUE,
                            base_class->type,
                            (a_construction_vtbl_array_index)1,
                            &ipd,
                            insert_location);
        }  /* if */
#endif /* !IA64_ABI */
      }  /* if */
    }  /* if */
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
    /* Normal case; generate the code to do the destruction. */
    lower_destructor_dynamic_init(dip, &ipd, have_complete_object,
                                  vtt_addr_node, insert_location);
  }  /* if */
}  /* lower_dtor_init */


static void initialize_dtor_init_for_cleanup(a_dynamic_init_ptr dip)
/*
Do cleanup initialization for the indicated destruction (from
the constructor_inits list of a destructor) and to its successors.
This is done early so the information is available when each entry
is processed.  Called only when exceptions are enabled.
*/
{
  a_destructible_entity_descr_ptr dedp = dip->destructible_entity_descr;
  a_dynamic_init_ptr              next_dip = dip->next_in_destruction_list;

  check_assertion_str(exceptions_enabled,
          "initialize_dtor_init_for_cleanup: called with exceptions disabled");
  dedp->cleanup_state_to_set_when_starting_destruction = next_dip;
  /* Do a recursive call to process the rest of the list. */
  if (next_dip != NULL) initialize_dtor_init_for_cleanup(next_dip);
#if GENERATE_EH_TABLES
  { a_cleanup_region_number         region_number;
    /* Each destruction gets a region number one higher than the region
       number of the next destruction, or the next available number
       (zero) if there is no next destruction.  Note that the
       recursive call above reverses the entries, which gives entry
       numbers in the desired order. */
    if (next_dip != NULL) {
      a_destructible_entity_descr_ptr next_dedp =
                                           next_dip->destructible_entity_descr;
      region_number = cleanup_region_number(next_dip) + 1;
      /* Add one more if there is a conditional flag (e.g., for a virtual
         base class). */
      if (next_dedp->conditional_flag_var != NULL) region_number++;
    } else {
      region_number = 0;  /* That is, the first region number. */
    }  /* if */
    dedp->region_number = region_number;
  }
#endif /* GENERATE_EH_TABLES */
}  /* initialize_dtor_init_for_cleanup */

#if GENERATE_EH_TABLES

static void make_dtor_init_region_table_entries(
                                           a_dynamic_init_ptr dip,
                                           an_insert_location *insert_location)
/*
Generate region table entries for the destructions on the indicated
list (they are the constructor_init destructions from a destructor).
Use recursion to put out the list backwards.  Any required code
is inserted at *insert_location.
*/
{
  a_dynamic_init_ptr      next_dip = dip->next_in_destruction_list;
#if CHECKING
  a_cleanup_region_number old_region_number = cleanup_region_number(dip);
#endif /* CHECKING */

  if (next_dip != NULL) {
    /* Use recursion to handle the rest of the list. */
    make_dtor_init_region_table_entries(next_dip, insert_location);
  }  /* if */
  /* Do the first entry on the list. */
  make_dyn_init_region_table_entry(dip, next_dip, insert_location);
#if CHECKING
  /* The region number assigned should be the one we pre-assigned in
     initialize_dtor_init_for_cleanup. */
  check_assertion_str(old_region_number == cleanup_region_number(dip),
                      "make_dtor_init_region_table_entries: wrong region num");
#endif /* CHECKING */
}  /* make_dtor_init_region_table_entries */

#endif /* GENERATE_EH_TABLES */

#if !GENERATE_EH_TABLES
/*ARGSUSED*/  /* <-- prologue_insert_location is not used in some versions. */
#endif /* !GENERATE_EH_TABLES */
static void gen_dtor_member_and_base_destructions(
                     an_insert_location              *insert_location,
                     an_insert_location              *prologue_insert_location,
                     a_destructor_wrapper_info_block *dtor_info)
/*
The current function is a destructor.  Generate code to destroy bases
and members, if necessary, and insert it at *insert_location.  If any
code is needed preceding the user code in the destructor, insert it at
*prologue_insert_location.  *dtor_info is used to pass information
between this function and lower_destructor_code and
insert_dtor_member_and_base_destructions.
*/
{
  a_routine_ptr          dtor_routine =
                                 innermost_function_scope->variant.routine.ptr;
  a_variable_ptr         this_param_var, complete_obj_param_var;
  an_expr_node_ptr       zero_constant_node, complete_obj_param_node;
  an_expr_node_ptr       compare_node;
  a_type_ptr             class_type;
  a_constructor_init_ptr ctor_init, ctor_init_list;
  a_dynamic_init_ptr     first_epilogue_destruction;
  an_insert_location     insert_location2;

  /* The following pseudo-code shows both the processing in this routine
     and the code added to the destructor routine.  Lines enclosed in [...]
     are tests and loops done in the processing in this routine; other
     lines are the code added to the destructor routine.  Note that
     there is other wrapper code added by lower_destructor_code.

     [For each data member on the ctor-initializer list:]
       Call the destructor.  The complete-object implicit argument is 0x2.
     [endfor]
     [For each direct nonvirtual base class on the ctor-initializer list:]
       Call the destructor.  The complete-object implicit argument is 0.
           If the destructor needs an array of destruction vtbl pointers,
           store the address of the proper subarray of the array pointed
           to by the destruction_vtbls temp into the transfer pointer in
           the subobject, as a way of passing that information to the
           subobject destructor.
     [endfor]
     [If there are any items left on the ctor-initializer list (which
         must be for virtual base classes):]
       If the added parameter != 0 (indicating a complete object is
           being destroyed and virtual base classes must be destroyed):
         [For each virtual base class on the ctor-initializer list:]
           Call the destructor.  The complete-object implicit argument is 0.
               If the destructor needs an array of destruction vtbl
               pointers, store the address of an array specific to this
               base class in the transfer pointer of the subobject, as
               a way of passing that information to the subobject
               destructor.
         [endfor]
       endif
     [endif]
  */
  /* Get a pointer to the "this" parameter variable. */
  this_param_var = innermost_function_scope->variant.routine.parameters;
  complete_obj_param_var = this_param_var->next;
  class_type = dtor_routine->source_corresp.parent.class_type;
  /* The constructor_inits list contains a list of destructions.  Each
     destruction is a default call supplied by the front end.  Every
     base class and member that requires a destructor appears, in the
     order (1) data members, (2) normal base classes, (3) virtual base
     classes.  The order within each section is source declaration order. */
  ctor_init = ctor_init_list =
                   innermost_function_scope->variant.routine.constructor_inits;
  innermost_function_scope->variant.routine.constructor_inits = NULL;
  if (exceptions_enabled) {
#if DO_FULL_PORTABLE_EH_LOWERING
    a_handle_number complete_obj_param_handle;
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
    /* Assign cleanup region numbers to the destructions.  This is done
       early so that we will know the right value to set __eh_curr_region
       to when beginning each destruction. */
    /* See whether there are any virtual base classes. */
    if (class_type->variant.class_struct_union.any_virtual_base_classes) {
      /* The added parameter indicating a complete object will be used
         as a conditional flag for the destructions of the virtual base
         classes (we don't destroy the virtual base classes unless we
         are working on a complete object). */
#if DO_FULL_PORTABLE_EH_LOWERING
      an_init_pos_descr ipd;
      /* Assign the object address table slot for the conditional
         variable. */
      complete_obj_param_handle = object_addr_table_index();
      /* Put the address of the variable into the object address table. */
      set_var_init_pos_descr(complete_obj_param_var, &ipd);
      init_object_addr_table_entry(&ipd, complete_obj_param_handle,
                                   prologue_insert_location);
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
      /* Process the ctor-inits for virtual base classes. */
      for (; ctor_init != NULL; ctor_init = ctor_init->next) {
        if (ctor_init->kind ==
                             (a_constructor_init_kind)cik_virtual_base_class) {
          /* Add complete_obj_param_var as a conditional flag. */
          a_destructible_entity_descr_ptr dedp = 
                             ctor_init->initializer->destructible_entity_descr;
          check_assertion(dedp != NULL);
          dedp->conditional_flag_var = complete_obj_param_var;
#if DO_FULL_PORTABLE_EH_LOWERING
          if (exceptions_enabled) {
            dedp->conditional_flag_handle = complete_obj_param_handle;
          }  /* if */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
        }  /* if */
      }  /* for */
      ctor_init = ctor_init_list;
    }  /* if */
    /* Find the first destruction in the epilogue. */
    first_epilogue_destruction = ctor_init->initializer;
    /* Watch out for the case of an array initialization; the top-level
       dynamic initialization is not on the destructions list. */
    if (first_epilogue_destruction->lifetime == NULL) {
      for (first_epilogue_destruction =
                              innermost_function_scope->lifetime->destructions;
           !first_epilogue_destruction->is_constructor_init;
           first_epilogue_destruction =
                       first_epilogue_destruction->next_in_destruction_list) {}
    }  /* if */
    /* Pass the pointer back to the caller. */
    dtor_info->first_epilogue_destruction = first_epilogue_destruction;
    /* Do cleanup initialization for the destructions on the ctor-initializer
       list of the destructor. */
    initialize_dtor_init_for_cleanup(first_epilogue_destruction);
  }  /* if */
  /* Generate a destructor call for each data member that appears on the
     ctor_init list. */
  for (; ctor_init != NULL &&
                         ctor_init->kind == (a_constructor_init_kind)cik_field;
       ctor_init = ctor_init->next) {
    lower_dtor_init(ctor_init, this_param_var, /*have_complete_object=*/TRUE,
                    (a_variable_ptr)NULL, insert_location);
  }  /* for */
  /* Generate a destructor call for each non-virtual direct base class
     that appears on the ctor_init list. */
  for (; ctor_init != NULL &&
             ctor_init->kind == (a_constructor_init_kind)cik_direct_base_class;
       ctor_init = ctor_init->next) {
#if IA64_ABI
    /* Set the virtual table pointer for the complete object so that we 
       can find the base. */
    insert_primary_vtbl_assignment(class_type, this_param_var,
                                   dtor_info->destruction_vtbls_var, 
                                   insert_location);
#endif /* IA64_ABI */
    lower_dtor_init(ctor_init, this_param_var,
                    /*have_complete_object=*/FALSE,
                    dtor_info->destruction_vtbls_var,
                    insert_location);
  }  /* for */
  /* If any items remain on the ctor_init list, they must be for virtual
     base classes. */
  if (ctor_init != NULL) {
    check_assertion_str2(ctor_init->kind ==
                               (a_constructor_init_kind)cik_virtual_base_class,
                         "gen_dtor_member_and_base_destructions:",
                         "bad ctor_init item kind");
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
                                      integer_type((an_integer_kind)ik_int),
                                      complete_obj_param_node);
    /* Make an "if" statement with a block statement under it:
         if (param != 0) {}
                          ^--- additional statements will be inserted.
    */
    insert_if_statement(compare_node, /*is_initialization_guard=*/FALSE,
                        insert_location, (a_statement_ptr *)NULL,
                        &insert_location2, (an_insert_location *)NULL);
    /* Destroy any virtual base classes on the ctor_init list. */
    for (; ctor_init != NULL; ctor_init = ctor_init->next) {
#if IA64_ABI
      /* Set the virtual table pointer for the complete object so that we 
         can find the virtual base. */
      insert_primary_vtbl_assignment(class_type, this_param_var,
                                     dtor_info->destruction_vtbls_var, 
                                     &insert_location2);
#endif /* IA64_ABI */
      lower_dtor_init(ctor_init, this_param_var,
                      /*have_complete_object=*/FALSE,
                      dtor_info->destruction_vtbls_var,
                      &insert_location2);
    }  /* for */
    /* Note that the "if" created above effectively ends here. */
  }  /* if */
  if (exceptions_enabled) {
#if GENERATE_EH_TABLES
    /* Make the region table entries for the epilogue destructions.
       This is done late because we want to put out the entries in
       reversed order, and we need to wait until they all have position
       information recorded. */
    make_dtor_init_region_table_entries(first_epilogue_destruction,
                                        prologue_insert_location);
#endif /* GENERATE_EH_TABLES */
  } /* if */
}  /* gen_dtor_member_and_base_destructions */


void set_cleanup_state_before_destructor_user_code(
                     an_insert_location              *insert_location,
                     a_destructor_wrapper_info_block *dtor_info)
/*
Insert code at *insert_location to establish the appropriate cleanup state
at the beginning of the user-written code in a destructor.  This cleanup
state calls for destruction of members and bases of the class.  *dtor_info
provides information developed by gen_dtor_member_and_base_destructions.
In particular dtor_info->first_epilogue_destruction indicates the
first member/base destruction to be done.
*/
{
  if (exceptions_enabled) {
    curr_context->curr_cleanup_state =
        curr_context->latest_initialization =
            dtor_info->first_epilogue_destruction;
    insert_code_to_indicate_cleanup_state(curr_context->curr_cleanup_state,
                                          insert_location,
                                          /*unreachable=*/FALSE);
  } /* if */
}  /* set_cleanup_state_before_destructor_user_code */


a_label_ptr insert_temp_label(an_insert_location *insert_location)
/*
Create a new temporary label and return a pointer to it.  Also create a
statement for the label and insert it at *insert_location.
*/
{
  a_label_ptr            temp_label = alloc_label();
  a_statement_ptr        label_stmt;
  an_object_lifetime_ptr lifetime;

  label_stmt = alloc_statement((a_statement_kind)stmk_label);
  label_stmt->variant.label.ptr = temp_label;
  temp_label->variant.exec_stmt = label_stmt;
  temp_label->source_corresp.referenced = TRUE;
  add_to_labels_list(temp_label);
  /* The label should get the current object lifetime.  However, if the
     current object lifetime is the global static lifetime, the current
     function has no object lifetimes, and the lifetime for the label should
     be NULL. */
  lifetime = curr_context->lifetime;
  if (lifetime != NULL && lifetime == il_header.primary_scope->lifetime) {
    lifetime = NULL;
  }  /* if */
  label_stmt->variant.label.lifetime = lifetime;
  insert_statement(label_stmt, insert_location);
  return temp_label;
}  /* insert_temp_label */


static void add_epilogue_label(an_insert_location *insert_location,
                               a_statement_ptr    insert_block,
                               a_boolean          *label_added)
/*
If the current routine has multiple returns, add an epilogue label
at *insert_location, change the returns to gotos to the label, and
return *label_added TRUE.  insert_block indicates the block in which
*insert_location appears.  On return, there will always be a return
statement at the insert point (either one that was already present as
the statement to insert after, or one that was added) and
*insert_location will be set to insert before the return.  Note
that this routine cannot be called multiple times, because the
precondition (insert after return) does not match the postcondition
(insert before return).
*/
{
  a_statement_ptr   top_level_return, stmt, prev_stmt;
  a_return_memo_ptr rmp, rmp_next;
  a_label_ptr       epilogue_label;
  a_boolean         added_return = FALSE;

  *label_added = FALSE;
  if (insert_location->kind == ilk_after_statement &&
      insert_location->variant.stmt->kind == (a_statement_kind)stmk_block) {
    /* We are adding after a block.  See whether the last statement of the
       block is a return.  If so, move it out of the block. */
    a_statement_ptr block_stmt = insert_location->variant.stmt;
    if (move_final_return_out_of_block(block_stmt, block_stmt)) {
      /* A return was moved out of the block.  Set the insert location
         to the return.  This allows further optimization below. */
      set_insert_location(block_stmt->next, insert_location);
    }  /* if */
  }  /* if */
  if (insert_location->kind == ilk_after_statement &&
      insert_location->variant.stmt->kind == (a_statement_kind)stmk_return) {
    /* We're inserting after a top-level return.  We can insert in
       front of it and avoid adding another return. */
    top_level_return = insert_location->variant.stmt;
    /* Find the previous statement, which is needed for the insert
       location. */
    for (prev_stmt = NULL, stmt = insert_block->variant.block.statements;
         stmt != top_level_return;
         prev_stmt = stmt, stmt = stmt->next) {
      check_assertion_str(stmt != NULL,
                    "add_epilogue_label: insert_location not in insert_block");
    }  /* for */
    /* Make an insert location preceding the return. */
    if (prev_stmt == NULL) {
      set_block_start_insert_location(insert_block, insert_location);
    } else {
      set_insert_location(prev_stmt, insert_location);
    }  /* if */
  } else {
    /* We're not adding after/before a return, so add a return at the end. */
    an_insert_location saved_insert_location;
    top_level_return = alloc_statement((a_statement_kind)stmk_return);
    saved_insert_location = *insert_location;
    insert_statement(top_level_return, insert_location);
    *insert_location = saved_insert_location;
    /* Add the return to the return memo list. */
    add_to_return_memo_list(top_level_return);
    added_return = TRUE;
  }  /* if */
  /* Now there is a top-level return statement and insert_location is set to
     insert in front of it.  The return statement is pointed to by
     top_level_return and by the first entry of the return memo list,
     and prev_stmt points to the statement preceding the return. */
  /* The return should match the first entry on the return memo list. */
  check_assertion(return_memo_list != NULL &&
                  top_level_return == return_memo_list->stmt);
  /* Leave just the entry for this return on the memo list.  The rest are
     processed and freed. */
  rmp = return_memo_list->next;
  return_memo_list->next = NULL;
  if (rmp != NULL) {
    /* There are returns to rewrite.  Add an epilogue label and change
       the returns to gotos to that label. */
    epilogue_label = insert_temp_label(insert_location);
    if (added_return) epilogue_label->reachable_by_fall_through = FALSE;
    *label_added = TRUE;
    /* Change the other returns to gotos. */
    for (; rmp != NULL; rmp = rmp_next) {
      stmt = rmp->stmt;
      rmp_next = rmp->next;
      set_statement_kind(stmt, (a_statement_kind)stmk_goto);
      stmt->variant.label.ptr = epilogue_label;
      rmp->next = NULL;
      free_return_memo_list(rmp);
    }  /* for */
  }  /* if */
}  /* add_epilogue_label */


void insert_dtor_member_and_base_destructions(
                              a_statement_ptr                 destruction_code,
                              an_insert_location              *insert_location,
                              a_statement_ptr                 insert_block,
                              a_destructor_wrapper_info_block *dtor_info)
/*
Code to destroy members and bases in a destructor was generated earlier by
gen_dtor_member_and_base_destructions.  destruction_code points to
the generated code, which is not currently attached to the IL tree,
or is NULL if there is no such code.  Insert the generated code at
*insert_location (which is either at the top level of the destructor,
or inside a function-try-block).  This insertion is after the
user-written code in the destructor or function-try-block.
insert_block indicates the block in which *insert_location appears.
On return, *insert_location is set to allow further insertion in
the epilogue, preceding a return statement (one that was present or
one that was added).  *dtor_info is used to pass information from
gen_dtor_member_and_base_destructions.
*/
{
  a_boolean label_added;

  /* If there are multiple returns in the destructor, add an epilogue
     label and change the returns to gotos to the label.  Even when
     there is no destruction code, this is done to ensure that there
     is a return statement; for the function-try-block, we need that
     so we can eliminate it and fall through to the end of the try block
     to do the stack pop, which was suppressed on returns in the
     user code. */
  add_epilogue_label(insert_location, insert_block, &label_added);
  if (destruction_code != NULL) {
    if (label_added) {
      if (exceptions_enabled &&
          innermost_function_scope->lifetime != NULL) {
        /* Set the cleanup state to the first destruction in the epilogue, if
           there is one. */
        curr_context->curr_cleanup_state =
            curr_context->latest_initialization =
                dtor_info->first_epilogue_destruction;
        insert_code_to_indicate_cleanup_state(curr_context->curr_cleanup_state,
                                              insert_location,
                                              /*unreachable=*/FALSE);
      }  /* if */
    }  /* if */
    /* Insert the code to destroy members and bases. */
    insert_statement(destruction_code, insert_location);
  } else {
#if !DO_FULL_PORTABLE_EH_LOWERING
    /* Insert a cleanup state indication that says there is nothing to
       do. */
    if (label_added) {
      if (exceptions_enabled &&
          innermost_function_scope->lifetime != NULL) {
        curr_context->curr_cleanup_state =
            curr_context->latest_initialization = NULL;
        insert_code_to_indicate_cleanup_state(curr_context->curr_cleanup_state,
                                              insert_location,
                                              /*unreachable=*/FALSE);
      }  /* if */
    }  /* if */
#endif /* !DO_FULL_PORTABLE_EH_LOWERING */
  }  /* if */
}  /* insert_dtor_member_and_base_destructions */


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
  an_insert_location     insert_location, insert_location2;
  a_statement_ptr        top_stmt = scope->assoc_block;
  a_statement_ptr        user_code_stmts, epilogue_block = NULL;
  a_boolean              has_function_try_block = FALSE;
  a_constructor_init_ptr ctor_init;
  a_boolean              epilogue_setup_done = FALSE;
  an_expr_node_ptr       zero_constant_node, complete_obj_param_node;
  an_expr_node_ptr       vtbl_addr_node, vptr_node;
  a_variable_ptr         vtbl_var;
  a_routine_ptr          dtor_routine = scope->variant.routine.ptr;
  a_routine_ptr          delete_routine;
  a_source_position      saved_error_position, saved_code_pos;
  a_source_position      opening_brace_pos, closing_brace_pos;
  an_insert_location     prologue_insert_location;
  a_destructor_wrapper_info_block
                         dtor_info;

  /* The following pseudo-code shows both the processing in this routine
     and the code added to the destructor routine.  Lines enclosed in [...]
     are tests and loops done in the processing in this routine; other
     lines are the code added to the destructor routine.

     [If a delete can be folded into the destructor:]
       If this != NULL test around entire routine.
     [endif]
     [If the current class requires a special array of virtual function table
         addresses (which is true when the class has virtual functions
         in virtual bases that are overridden):]
       If the added parameter != 0 (indicating a complete object is
           being destroyed):
         Set the destruction_vtbls temp to point to a local static array
           containing vtbl pointer values to be used for a complete object.
       else
         Set the destruction_vtbls temp to the value of the transfer
             pointer in the class (the caller uses that to pass in the
             address of the array of vtbl pointers to be used during the
             subobject destruction).
       endif
     [endif]
     [If the current class has any virtual functions:]
       Set the virtual function table pointer in the current class.
     [endif]
     [For each base class of the current class:]
       [If the base class needs a virtual function table instance distinct
           from the derived class instance:]
         Set the virtual function table pointer in the base class.  Virtual
             base classes must be accessed through the virtual base class
             pointer.  If the destruction_vtbls temp is in use, copy the
             proper element of the array to the virtual function table
             pointer instead of using a specific virtual function table
             instance.
       [endif]
     [endfor]
     ... user destructor code goes here ...
         -- returns in the user code are turned into gotos to the following
            code:
     Member and base destruction code (see
         gen_dtor_member_and_base_destructions).
     If (added parameter & 0x1) != 0:
       delete((void)*this)
     endif
     return;
  */
  saved_code_pos = code_pos_for_lowering;
  saved_error_position = error_position;
  /* Set the current position to the opening brace of the destructor. */
  set_position_from_stmt_source_position(opening_brace_pos,
                                         top_stmt->position);
  error_position = code_pos_for_lowering = opening_brace_pos;
  int_type = integer_type((an_integer_kind)ik_int);
  /* Get a pointer to the "this" parameter variable. */
  this_param_var = scope->variant.routine.parameters;
  complete_obj_param_var = this_param_var->next;
  class_type = dtor_routine->source_corresp.parent.class_type;
  /* Mark the class as referenced because, at the very least, the
     "this" parameter uses it.  For some cases involving generated virtual
     destructors, this is necessary. */
  class_type->source_corresp.referenced = TRUE;
  ctor_init = scope->variant.routine.constructor_inits;
  ctsp = class_type->variant.class_struct_union.extra_info;
  if (top_stmt->kind == (a_statement_kind)stmk_try_block) {
    /* This destructor has a function-try-block as the top statement. */
    has_function_try_block = TRUE;
    /* Add a compound statement as the top statement of the function. */
    put_block_around_try_block(top_stmt, &insert_location, &user_code_stmts);
  } else {
    /* Normal case -- no function-try-block */
    check_assertion(top_stmt->kind == (a_statement_kind)stmk_block);
    user_code_stmts = top_stmt->variant.block.statements;
    set_block_start_insert_location(top_stmt, &insert_location);
  }  /* if */
  /* Get the position of the closing brace of the destructor.  Note that
     if the top statement was a try-block it has been rewritten as a
     block, and the source position was preserved. */
  set_position_from_stmt_source_position(
                           closing_brace_pos,
                           top_stmt->variant.block.extra_info->final_position);
  /* Start an object lifetime if appropriate. */
  begin_block_object_lifetime(scope->lifetime, &insert_location);
  dtor_info.first_epilogue_destruction = NULL;
  dtor_info.destruction_vtbls_var = NULL;
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
  if (ctsp->construction_vtbls != NULL) {
    an_insert_location else_insert_location;
    an_expr_node_ptr   compare_node;
    /* This class is one that has overridden virtual functions in virtual
       base classes, and needs special versions of the virtual function
       tables when used to destruct a subobject. */
    /* Create a temporary that will point to an array of virtual function
       table addresses. */
    dtor_info.destruction_vtbls_var = make_construction_vtbl_temporary();
    /* Put out code that tests the added parameter to determine whether
       we are destroying a complete object. */
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
    insert_if_statement(compare_node, /*is_initialization_guard=*/FALSE,
                        &insert_location, (a_statement_ptr *)NULL,
                        &insert_location2, &else_insert_location);
    /* Inserting under insert_location2, in the "then" part of the "if"
       (a complete object is being destroyed): */
    /* Set the destruction_vtbls temporary to point to the default array
       of virtual function table pointers to be used when destroying a
       complete object. */
    insert_default_construction_vtbls_assignment(class_type,
                                                 ctsp->construction_vtbls,
                                                 dtor_info.
                                                         destruction_vtbls_var,
                                                 &insert_location2);
    /* Inserting under else_insert_location, in the "else" of the "if"
       (a subobject is being destroyed): */
    /* Copy the value of the transfer pointer to the local
       destruction_vtbls temporary.  The caller destructor uses the
       transfer pointer to pass information down to the subclass
       destructor. */
    receive_construction_vtbls_in_subobject_constructor(dtor_info.
                                                         destruction_vtbls_var,
                                                        class_type,
                                                        this_param_var,
                                                        /*is_destructor=*/TRUE,
                                                        &else_insert_location);
  }  /* if */
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
  /* If the current class has any virtual functions, generate code to
     set the virtual function table pointer in the current class. */
  insert_primary_vtbl_assignment(class_type, this_param_var,
                                 dtor_info.destruction_vtbls_var,
                                 &insert_location);
  /* For each base class of this class that needs it, generate code to
     set the virtual function table pointer in the base class.  This gets
     rid of entries in the virtual function table that point to functions
     of classes derived from the current class. */
  for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
#if !IA64_ABI
    vtbl_var = bcp->virtual_function_table_var;
#else /* IA64_ABI */
    if (needs_virtual_function_table(bcp->type)) {
      vtbl_var = ctsp->virtual_function_table_var;
    } else {
      vtbl_var = NULL;
    }  /* if */
#endif /* IA64_ABI */
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
      vtbl_addr_node = NULL;
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
      if (bcp->index_in_construction_vtbl_array != 0) {
        /* The virtual function table to use is specified by an element of the
           array of destruction virtual function table pointers. */
        vtbl_addr_node = vtbl_addr_from_construction_vtbls_array(
                                        dtor_info.destruction_vtbls_var,
                                        /*var_is_array=*/FALSE,
                                        bcp->index_in_construction_vtbl_array);
        vtbl_addr_node = add_indirection_to_node(vtbl_addr_node);
      } else
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
      /* Do not insert code here; this is the "else" of an "if". */
#if IA64_ABI
      if (base_class_has_vtbl(bcp))
#endif /* IA64_ABI */
      {
        vtbl_addr_node = make_vtbl_address_node(vtbl_var, class_type, bcp);
        set_lowering_variable_address_taken(vtbl_var);
        vtbl_var->source_corresp.referenced = TRUE;
      }
      if (vtbl_addr_node != NULL) {
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
#if IA64_ABI
        if (bcp->is_virtual &&
            is_direct_or_indirect_virtual_primary_base(bcp)) {
          /* If a primary virtual base is located at the origin of the
             subobject being destroyed, we should not have clobbered its
             virtual table pointer.  We could devise a run-time test to
             detect such cases, but it's simpler and probably just as
             efficient to reload the primary virtual table pointer of the
             subobject being destroyed. */
          insert_primary_vtbl_assignment(class_type, this_param_var,
                                         dtor_info.destruction_vtbls_var,
                                         &insert_location);
        }  /* if */
#endif /* IA64_ABI */
      }  /* if */
    }  /* if */
  }  /* for */
  /* Now generate epilogue wrapper code to destroy members and base classes.
     This is done early, and into a block off to the side, so that the
     proper exception cleanup actions can be put on the cleanup list before
     the user code is lowered.  Later, the epilogue block will be inserted
     into the destructor at the right place. */
  if (ctor_init != NULL) {
    /* Save the prologue insert location as the place to insert exception
       handling initialization code. */
    prologue_insert_location = insert_location;
    epilogue_block = alloc_statement((a_statement_kind)stmk_block);
    set_block_start_insert_location(epilogue_block, &insert_location);
    /* Set the current position to the closing brace of the destructor. */
    code_pos_for_lowering = error_position = closing_brace_pos;
    /* Create code to destroy members and bases. */
    gen_dtor_member_and_base_destructions(&insert_location,
                                          &prologue_insert_location,
                                          &dtor_info);
    /* Set the current position to the opening brace of the destructor. */
    error_position = code_pos_for_lowering = opening_brace_pos;
  }  /* if */
  /* Now lower the user code. */
  if (has_function_try_block) {
    /* The top statement of the destructor is a function-try-block.
       The code to destroy members and bases is inserted inside the
       try block. */
    lower_try_block(user_code_stmts, /*is_function_try_block=*/TRUE,
                    epilogue_block, &dtor_info);
    set_insert_location(user_code_stmts, &insert_location);
  } else {
    /* Normal case (not function-try-block). */
    a_statement_ptr last_stmt;
    if (ctor_init != NULL) {
      set_cleanup_state_before_destructor_user_code(&prologue_insert_location,
                                                    &dtor_info);
    }  /* if */
    lower_statement_list(user_code_stmts, &last_stmt);
    set_insert_location(last_stmt, &insert_location);
    /* Insert the code to destroy members and bases, generated earlier. */
    /* This also changes the insert location from after the final return
       to before it. */
    insert_dtor_member_and_base_destructions(epilogue_block,
                                             &insert_location,
                                             top_stmt,
                                             &dtor_info);
    epilogue_setup_done = TRUE;
  }  /* if */
  /* Set the current position to the closing brace of the destructor. */
  code_pos_for_lowering = error_position = closing_brace_pos;
  /* Add code to free the storage if the "free" bit (0x1) is on in the
     added parameter:
       if ((param & 0x1) != 0) delete-routine((void *)this);
     Watch out for the case where the delete routine pointer is NULL; this
     happens if a derived class inherits more than one delete routine, and
     therefore they're ambiguous.
  */
  delete_routine = ctsp->assoc_operator_delete_routine;
  if (delete_routine != NULL) {
    an_expr_node_ptr this_param_node;
    an_expr_node_ptr and_node, two_constant_node, if_node;
    a_param_type_ptr param1;

    if (!epilogue_setup_done) {
      a_boolean label_added;
      /* If there are any returns in the catch clauses of the
         function-try-block, add an epilogue label and change the returns
         to gotos.  In the simplest case, changes the insert location from
         after the return at the end of the routine to before it. */
      add_epilogue_label(&insert_location, top_stmt, &label_added);
      epilogue_setup_done = TRUE;
    }  /* if */
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
      make_zero_of_proper_type(f_skip_typerefs(this_param_var->type),
                               &null_constant);
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
    insert_if_statement(if_node, /*is_initialization_guard=*/FALSE,
                        &insert_location, (a_statement_ptr *)NULL,
                        &insert_location2, (an_insert_location *)NULL);
    /* Make "delete-routine((void *)this);" under the "if". */
    this_param_node = var_rvalue_expr(this_param_var);
    this_param_node = add_cast_if_necessary(this_param_node, void_star_type());
    /* If the delete routine takes two arguments, add a second argument
       of type size_t that gives the size of the class. */
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
              node_for_host_large_integer(
                                      (a_host_large_integer)(class_type->size),
                                      targ_size_t_int_kind);
    }  /* if */
    delete_routine->source_corresp.referenced = TRUE;
    make_call_statement(delete_routine, this_param_node, &insert_location2);
  }  /* if */
  { an_expr_node_ptr this_param_node, null_constant_node, if_node;
    a_constant       null_constant;

    /* Make and add "if (this != NULL)" around the entire routine body.
       This is needed when the delete call can be folded into the
       destructor call, and is handy to avoid a test before the call
       even when that is not allowed. */
    this_param_node = var_rvalue_expr(this_param_var);
    make_zero_of_proper_type(f_skip_typerefs(this_param_var->type),
                             &null_constant);
    null_constant_node = alloc_node_for_constant(&null_constant);
    this_param_node->next = null_constant_node;
    if_node = make_operator_node((an_expr_operator_kind)eok_pne,
                                 int_type, this_param_node);
    /* Make the "if" statement. */
    enclose_routine_in_if(scope, if_node, (a_variable_ptr)NULL);
  }
  error_position = saved_error_position;
  code_pos_for_lowering = saved_code_pos;
}  /* lower_destructor_code */

#if ONE_INSTANTIATION_PER_OBJECT

static void mark_expr_list_slice_dyn_inits(an_expr_node_ptr expr);
static void mark_slice_dyn_inits(a_dynamic_init_ptr dip);


static void mark_constant_slice_dyn_inits(a_constant_ptr con)
/*
Set the included_in_slice flag in any dynamic initializations under
the given constant.
*/
{
  switch (con->kind) {
    case ck_error:
    case ck_integer:
    case ck_string:
    case ck_float:
    case ck_address:
    case ck_ptr_to_member:
#if GENERATE_EH_TABLES && !DO_FULL_PORTABLE_EH_LOWERING
    case ck_stack_offset:
#endif /* DO_IL_LOWERING && ... */
      /* No processing. */
      break;
    case ck_dynamic_init:
      mark_slice_dyn_inits(con->variant.dynamic_init);
      break;
    case ck_aggregate:
      for (con = con->variant.aggregate.first_constant;
           con != NULL;
           con = con->next) {
        mark_constant_slice_dyn_inits(con);
      }  /* if */
      break;
    case ck_init_repeat:
      mark_constant_slice_dyn_inits(con->variant.init_repeat.constant);
      break;
    case ck_template_param:
    default:
      unexpected_condition_str(
                           "mark_constant_slice_dyn_inits: bad constant kind");
  }  /* switch */
}  /* mark_constant_slice_dyn_inits */


static void mark_expr_slice_dyn_inits(an_expr_node_ptr expr)
/*
Set the included_in_slice flag in any dynamic initializations under
the given expression.
*/
{
  if (expr != NULL) {
    switch (expr->kind) {
      case enk_error:
      case enk_constant:
      case enk_variable:
      case enk_variable_address:
      case enk_field:
      case enk_runtime_sizeof:
      case enk_address_of_ellipsis:
      case enk_routine_address:
        /* No processing. */
        break;
      case enk_operation:
        mark_expr_list_slice_dyn_inits(expr->variant.operation.operands);
        break;
      case enk_temp_init:
        mark_slice_dyn_inits(expr->variant.init.dynamic_init);
        break;
      case enk_new_delete:
        mark_expr_list_slice_dyn_inits(expr->variant.new_delete->arg);
        mark_slice_dyn_inits(expr->variant.new_delete->dynamic_init);
        mark_slice_dyn_inits(expr->variant.new_delete->
                                              freeing_of_storage_on_exception);
        break;
      case enk_throw:
        if (expr->variant.throw_info != NULL){ 
          mark_slice_dyn_inits(expr->variant.throw_info->dynamic_init);
        }  /* if */
        break;
      case enk_object_lifetime:
        /* Note that we do go into another object lifetime, because
           there might be dynamic inits in there that were promoted into
           the outer lifetime. */
        mark_expr_slice_dyn_inits(expr->variant.object_lifetime.expr);
        break;
      case enk_typeid:
        mark_expr_slice_dyn_inits(expr->variant.typeid_info.expr);
        break;
      case enk_condition:
#if !DO_FULL_PORTABLE_EH_LOWERING
      case enk_lowered_eh_construct:
#endif /* !DO_FULL_PORTABLE_EH_LOWERING */
#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
      case enk_result_of_overriding_function:
#endif /* ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
#if GNU_EXTENSIONS_ALLOWED
      case enk_statement:  /* Used only in C mode. */
#endif /* GNU_EXTENSIONS_ALLOWED */
      default:
        unexpected_condition_str("mark_expr_slice_dyn_inits: bad expr kind");
    }  /* switch */
  }  /* if */
}  /* mark_expr_slice_dyn_inits */


static void mark_expr_list_slice_dyn_inits(an_expr_node_ptr expr)
/*
Set the included_in_slice flag in any dynamic initializations under
the given expression list.
*/
{
  for (; expr != NULL; expr = expr->next) {
    mark_expr_slice_dyn_inits(expr);
  }  /* for */
}  /* mark_expr_list_slice_dyn_inits */


static void mark_slice_dyn_inits(a_dynamic_init_ptr dip)
/*
Set the included_in_slice flag in the given dynamic initialization and
in all dynamic initializations under it.
*/
{
  if (dip != NULL) {
    dip->included_in_slice = TRUE;
    switch (dip->kind) {
      case dik_none:
      case dik_zero:
      case dik_constant:
        /* No processing. */
        break;
      case dik_expression:
      case dik_call_returning_class_via_cctor:
        mark_expr_slice_dyn_inits(dip->variant.expression);
        break;
      case dik_constructor:
        mark_expr_list_slice_dyn_inits(dip->variant.constructor.args);
        break;
      case dik_nonconstant_aggregate:
        mark_constant_slice_dyn_inits(dip->variant.constant);
        break;
      case dik_bitwise_copy:
      default:
        /* Not expected. */
        unexpected_condition_str("mark_slice_dyn_inits: bad dyn init kind");
    }  /* switch */
  }  /* if */
}  /* mark_slice_dyn_inits */

#endif /* ONE_INSTANTIATION_PER_OBJECT */

#if !ONE_INSTANTIATION_PER_OBJECT
/*ARGSUSED*/ /* residual_destrs is not used in that case. */
#endif /* !ONE_INSTANTIATION_PER_OBJECT */
static void b_lower_file_scope_dynamic_inits(
                                         unsigned long needed_bit_number,
                                         a_dynamic_init_ptr   *residual_destrs)
/*
Do lowering on the file-scope dynamic initializations list.  Generate
an initialization routine and make sure it will get called at program
startup.  If needed_bit_number is non-zero, it is the needed flag bit number
for an instantiation, and only initializations for that bit number should
be included in the initialization routine.  Any destructions associated
with the initializations to be done that remain on the object lifetime
list after lowering are moved to the residual_destrs list.  This is so
they can be kept off the object lifetime list now and added back in
after all initialization routines for instantiations have been generated.
*/
{
  a_dynamic_init_ptr dip, dip_next;
  an_insert_location insert_location;
  a_scope_ptr        file_scope = il_header.primary_scope, scope;
  an_init_pos_descr  ipd;
  a_generated_routine_context
                     grcontext;
  a_memory_region_number
                     region_number;
  unsigned long      eff_needed_bit_number = needed_bit_number;
#if ONE_INSTANTIATION_PER_OBJECT
  a_dynamic_init_ptr process_list, end_process_list;
  a_dynamic_init_ptr dtor_process_list, end_dtor_process_list;
  a_dynamic_init_ptr delay_list, end_delay_list;
  a_dynamic_init_ptr dtor_delay_list, end_dtor_delay_list;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if !USE_INIT_SECTION_IN_GENERATED_C
  a_routine_ptr      init_rout;
#endif /* !USE_INIT_SECTION_IN_GENERATED_C */

  dip = file_scope->dynamic_inits;
#if ONE_INSTANTIATION_PER_OBJECT
  if (needed_bit_number == 1) eff_needed_bit_number = 0;
  if (needed_bit_number != 0) {
    /* We're putting out separate initialization routines for each
       instantiation.   Split the dynamic initializations list into two
       lists: one that gets processed on this call (because the variables
       are assigned to the current slice), and another that does not get
       processed and goes back on the list after we're done with this call,
       for processing on a subsequent call. */
    process_list = end_process_list = NULL;
    delay_list = end_delay_list = NULL;
    for (; dip != NULL; dip = dip_next) {
      dip_next = dip->next;
      dip->next = NULL;
      if (dip->variable->instantiation_needed_bit_number ==
                                                       eff_needed_bit_number) {
        /* This dynamic initialization gets processed on this call. */
        if (end_process_list == NULL) {
          process_list = dip;
        } else {
          end_process_list->next = dip;
        }  /* if */
        end_process_list = dip;
        /* Mark any destructions associated with this initialization so we can
           recognize them. */
        mark_slice_dyn_inits(dip);
      } else {
        /* This dynamic initialization does not get processed on this call
           and goes back on the list. */
        if (end_delay_list == NULL) {
          delay_list = dip;
        } else {
          end_delay_list->next = dip;
        }  /* if */
        end_delay_list = dip;
      }  /* if */
    }  /* for */
    /* Sweep backwards through the destructions to split the list that way
       too. */
    dtor_process_list = end_dtor_process_list = NULL;
    dtor_delay_list = end_dtor_delay_list = NULL;
    if (file_scope->lifetime != NULL) {
      for (dip = file_scope->lifetime->destructions;
           dip != NULL;
           dip = dip_next) {
        dip_next = dip->next_in_destruction_list;
        dip->next_in_destruction_list = NULL;
        /* See if this destruction was marked by mark_slice_dyn_inits.
           If so, it's related to the initializations being processed on
           this call. */
        if (dip->included_in_slice) {
          /* This destruction gets processed on this call. */
          if (end_dtor_process_list == NULL) {
            dtor_process_list = dip;
          } else {
            end_dtor_process_list->next_in_destruction_list = dip;
          }  /* if */
          end_dtor_process_list = dip;
        } else {
          /* This destruction does not get processed on this call and goes
             back on the list. */
          if (end_dtor_delay_list == NULL) {
            dtor_delay_list = dip;
          } else {
            end_dtor_delay_list->next_in_destruction_list = dip;
          }  /* if */
          end_dtor_delay_list = dip;
        }  /* if */
      }  /* for */
    }  /* if */
    file_scope->dynamic_inits = dip = process_list;
    if (file_scope->lifetime != NULL) {
      file_scope->lifetime->destructions = dtor_process_list;
    }  /* if */
  }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  if (dip != NULL) {
    /* There are some file-scope dynamic initializations.  Generate a routine
       containing them. */
    scope = file_scope_init_insert_location(eff_needed_bit_number,
                                            &insert_location, &region_number,
                                            &grcontext);
    processing_file_scope_init_routine = TRUE;
#if !USE_INIT_SECTION_IN_GENERATED_C
    init_rout = scope->variant.routine.ptr;
#endif /* !USE_INIT_SECTION_IN_GENERATED_C */
    if (file_scope->lifetime != NULL) {
      begin_object_lifetime(file_scope->lifetime, &insert_location);
    }  /* if */
    /* Generate the initializations. */
    for (; dip != NULL; dip = dip_next) {
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
      /* Break the link between dynamic inits.  After lowering, no dynamic
         inits remain on the file scope list.  However, they may remain on
         object lifetime lists, and in those cases it's not good to have the
         "next" pointer pointing off to dynamic inits that are otherwise
         not linked into the IL. */
      dip_next = dip->next;
      dip->next = NULL;
      set_var_init_pos_descr(dip->variable, &ipd);
      lower_dynamic_init(dip, &ipd,
                         (an_expr_node_ptr)NULL, (an_expr_node_ptr)NULL,
                         (a_constructor_init_ptr)NULL, LDIO_FULL_EXPR,
                         /*others_follow_in_aggr=*/FALSE,
                         eff_insert_location, (a_boolean *)NULL,
                         (a_constant **)NULL);
    }  /* for */
    if (exceptions_enabled) {
      /* Add prologue/epilogue code for exceptions if needed. */
      add_eh_function_prologue(scope);
    }  /* if */
    processing_file_scope_init_routine = FALSE;
    pop_generated_routine_context(scope, region_number, &grcontext);
    /* Generate code to ensure that the initialization routine is called
       at program startup.  If a .init section will be used for
       initialization, skip this stuff. */
#if !USE_INIT_SECTION_IN_GENERATED_C
    make_code_to_invoke_file_scope_init_routine(init_rout);
#endif /* !USE_INIT_SECTION_IN_GENERATED_C */
  }  /* if */
#if ONE_INSTANTIATION_PER_OBJECT
  if (needed_bit_number != 0) {
    file_scope->dynamic_inits = delay_list;
    if (file_scope->lifetime != NULL) {
      if (file_scope->lifetime->destructions != NULL) {
        /* There are some destructions that remain on the list after lowering,
           e.g., ones for temporaries that were built during construction of
           an aggregate.  Save them on a side list so that they will not
           be on the primary list and therefore will not accidentally
           be processed again.  They will be put back on the list after
           all initialization routines have been generated. */
        a_dynamic_init_ptr last_destr = file_scope->lifetime->destructions;
        while (last_destr->next_in_destruction_list != NULL) {
          last_destr = last_destr->next_in_destruction_list;
        }  /* if */
        last_destr->next_in_destruction_list = *residual_destrs;
        *residual_destrs = file_scope->lifetime->destructions;
      }  /* if */
      file_scope->lifetime->destructions = dtor_delay_list;
    }  /* if */
  }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
}  /* b_lower_file_scope_dynamic_inits */


void lower_file_scope_dynamic_inits(void)
/*
Do lowering on the file-scope dynamic initializations list.  Also insert
code to cause the generated initialization routine to be called at startup.
*/
{
  a_scope_ptr file_scope = il_header.primary_scope;

#if ONE_INSTANTIATION_PER_OBJECT
  if (one_instantiation_per_object) {
    a_dynamic_init_ptr residual_destrs = NULL;
    /* When generating one instantiation per object, each instantiation gets
       its own initialization file. */
    /* Each instantiation has an associated bit number.  The bit numbers
       are assigned in increments of 2, to leave room for a class
       definition needed bit associated with each instantiation. */
    unsigned long needed_bit_number;
    for (needed_bit_number = 1;
         needed_bit_number <
                 (il_header.number_of_external_nonclass_template_entities+1)*2;
         needed_bit_number += 2) {
      b_lower_file_scope_dynamic_inits(needed_bit_number, &residual_destrs);
    }  /* for */
    check_assertion_str(file_scope->dynamic_inits == NULL,
                    "lower_file_scope_dynamic_inits: not all entries lowered");
    if (file_scope->lifetime != NULL) {
      /* Restore any residual destructions left after lowering. */
      check_assertion_str(file_scope->lifetime->destructions == NULL,
                       "lower_file_scope_dynamic_inits: non-NULL destrs list");
      file_scope->lifetime->destructions = residual_destrs;
    }  /* if */
  } else
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  /* Do not insert code here; this is the "else" of an "if". */
  {
    b_lower_file_scope_dynamic_inits((unsigned long)0,
                                     (a_dynamic_init_ptr *)0);
    file_scope->dynamic_inits = NULL;
  }
}  /* lower_file_scope_dynamic_inits */

#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN

void add_body_for_covariant_return_type_entry_routine(a_routine_ptr routine)
/*
Add a definition to the indicated function, which is an entry/wrapper
used to call an overriding virtual function that has a covariant
return type, or a thunk in the IA-64 ABI.  The body is a return of
an enk_result_of_overriding_function cast to the proper base class.
The overriding function must have a definition in the current compilation.
*/
{
  a_scope_ptr            scope;
  a_memory_region_number region_number;
  a_generated_routine_context
                         grcontext;
  a_statement_ptr        return_stmt;
  an_expr_node_ptr       expr;
  a_param_type_ptr       ptp;
  a_variable_ptr         param_var, last_param_var;
  a_type_ptr             routine_type = skip_typerefs(routine->type);
  a_routine_ptr          overriding_function, overridden_function;
  a_base_class_ptr       bcp;
  a_type_ptr             overriding_return_type, overridden_return_type;

  /* The routine type must be already lowered so that, among other things,
     the implicit "this" parameter is already in the parameter type list. */
  check_assertion(visited_yet(routine_type));
  /* Make the basic definition (memory_region, scope, top-level block). */
  scope = make_routine_definition(routine, /*make_return=*/TRUE,
                                  &region_number);
  push_generated_routine_context(scope, region_number, &grcontext);
  /* Add parameter variables. */
  last_param_var = NULL;
  for (ptp = skip_typerefs(routine->type)->
                                   variant.routine.extra_info->param_type_list;
       ptp != NULL;
       ptp = ptp->next) {
    a_type_qualifier_set qualifiers = ptp->qualifiers;
#if IA64_ABI
    if (last_param_var == NULL && (routine->delta != 0 ||
                                   routine->vcall_index != 0)) {
      /* We will be modifying the "this" pointer so it cannot be const. */
      qualifiers &= ~TQ_CONST;
    }  /* if */
#endif /* IA64_ABI */
    param_var = make_lowered_param_variable(make_qualified_type(ptp->type,
                                                                qualifiers));
    if (last_param_var == NULL) {
      scope->variant.routine.parameters = param_var;
      param_var->is_this_parameter = TRUE;
    } else {
      last_param_var->next = param_var;
    }  /* if */
    last_param_var = param_var;
    param_var->next = NULL;
  }  /* for */
  overriding_function = routine->overriding_function_for_covariant_return_type;
  overridden_function = routine->overridden_function_for_covariant_return_type;
  overriding_return_type = skip_typerefs(overriding_function->type)->
                                                   variant.routine.return_type;
  overridden_return_type = skip_typerefs(overridden_function->type)->
                                                   variant.routine.return_type;
  /* The overriding function must have a definition in this compilation. */
  check_assertion(overriding_function->assoc_scope != NULL_region_number &&
                  !overriding_function->suppress_inline_body);
  /* Make an expression that is an enk_result_of_overriding_function cast
     to the right base class pointer. */
  expr = alloc_expr_node((an_expr_node_kind)enk_result_of_overriding_function);
  expr->type = overriding_return_type;
#if IA64_ABI
  if (is_ptr_or_ref_type(overriding_return_type) && 
      is_class_struct_union_type(type_pointed_to(overriding_return_type)) &&
      !same_entities(overriding_return_type, overridden_return_type)) {
#endif /* IA64_ABI */
    bcp = find_base_class_of_full(type_pointed_to(overriding_return_type),
                                  type_pointed_to(overridden_return_type),
                                  /*instantiate_if_necessary=*/FALSE);
    check_assertion(bcp != NULL);
    add_base_class_casts(bcp, overridden_return_type,
                         /*check_cast_access=*/FALSE,
                         /*is_implicit_cast=*/TRUE,
                         /*implicit_in_naming=*/FALSE,
                         &expr,
                         &overriding_function->source_corresp.decl_position);
#if IA64_ABI
  }  /* if */
#endif /* IA64_ABI */
  lower_expr(expr, /*is_lvalue=*/FALSE);
  /* Put the expression into the return statement in the body. */
  check_assertion(scope->assoc_block->kind == (a_statement_kind)stmk_block);
  return_stmt = scope->assoc_block->variant.block.statements;
  check_assertion(return_stmt != NULL &&
                  return_stmt->kind == (a_statement_kind)stmk_return);
  return_stmt->expr = expr;
#if IA64_ABI
  if (overriding_function->use_comdat) {
    put_routine_into_comdat_group(routine);
  }  /* if */
  /* If necessary, adjust the "this" pointer.  Do this after handling the
     return statement because the logic above assumes that the return
     statement is the first thing in the block. */
  if (routine->delta != 0 || routine->vcall_index != 0) {
    a_variable_ptr     this_param;
    an_expr_node_ptr   this_adjustment = NULL, delta_expr, vcall_expr;
    an_expr_node_ptr   index_expr, this_expr;
    an_insert_location insert_location;
    this_param = scope->variant.routine.parameters;
    if (routine->delta != 0) {
      /* Add the "delta". */
      /* Cast the "this" parameter to "char *" to suppress scaling on the 
         pointer addition. */
      this_adjustment = add_cast_to_char_star(var_rvalue_expr(this_param));
      delta_expr = node_for_integer_constant((long)routine->delta, 
                                             targ_ptrdiff_t_int_kind);
      this_adjustment->next = delta_expr;
      this_adjustment = make_operator_node((an_expr_operator_kind)eok_padd,
                                           this_adjustment->type, 
                                           this_adjustment);
      /* Cast back to the type of "this". */
      this_adjustment = add_cast_if_necessary(this_adjustment, 
                                              this_param->type);
      /* Perform the assignment. */
      this_adjustment = 
                      make_var_assignment_expr(this_param,
                                               (an_expr_operator_kind)eok_last,
                                               this_adjustment);
    }  /* if */
    if (routine->vcall_index != 0) {
      /* Adjust from the virtual base to the final overrider.  This code
         depends on the fact that the vptr is always at offset zero in the
         object; we do not even know what the static type of the virtual base
         is at this point. */
      vcall_expr = var_rvalue_expr(this_param);
      /* Treat the object as a pointer to a pointer to a virtual function
         table. */
      vcall_expr = add_cast_if_necessary(vcall_expr,
                 make_pointer_type(make_pointer_type(make_vtbl_entry_type())));
      /* Dereference to get a pointer to the virtual function table. */
      vcall_expr = add_indirection_to_node(vcall_expr);
      /* Add the vcall index to find the vcall offset. */
      index_expr = node_for_integer_constant((long)routine->vcall_index,
                                             targ_ptrdiff_t_int_kind);
      vcall_expr->next = index_expr;
      vcall_expr = make_operator_node((an_expr_operator_kind)eok_padd,
                                      vcall_expr->type,
                                      vcall_expr);
      /* Dereference to get the offset. */
      vcall_expr = add_indirection_to_node(vcall_expr);
      /* Add that to the this pointer. */
      this_expr = var_rvalue_expr(this_param);
      /* Cast to "char *" to suppress pointer scaling. */
      this_expr = add_cast_to_char_star(this_expr);
      this_expr->next = vcall_expr;
      vcall_expr = make_operator_node((an_expr_operator_kind)eok_padd,
                                     this_expr->type,
                                     this_expr);
      /* Cast back to the type of "this". */
      vcall_expr = add_cast_if_necessary(vcall_expr,
                                         this_param->type);
      /* Perform the assignment. */
      vcall_expr = make_var_assignment_expr(this_param,
                                            (an_expr_operator_kind)eok_last,
                                            vcall_expr);
      /* If there was already a delta adjustment, combine the two. */
      if (this_adjustment != NULL) {
        this_adjustment = make_comma_node(this_adjustment, vcall_expr);
      } else {
        this_adjustment = vcall_expr;
      }  /* if */
    }  /* if */
    /* Insert the statement. */
    set_block_start_insert_location(scope->assoc_block, &insert_location);
    (void)insert_expr_statement(this_adjustment, &insert_location);
  }  /* if */
#endif /* IA64_ABI */
  pop_generated_routine_context(scope, region_number, &grcontext);
}  /* add_body_for_covariant_return_type_entry_routine */

#endif /* ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
#if MICROSOFT_EXTENSIONS_ALLOWED

/*
Pointer to the struct type for the Microsoft _GUID, once it is created.
NULL until created.
*/
static a_type_ptr
		guid_type;
static a_type_ptr
		guid_array_type;
			/* Array type for the Data4 member of _GUID. */
static a_variable_ptr
		null_guid_variable;
			/* Variable for a NULL GUID, once created. */

static a_type_ptr make_guid_type(void)
/*
Make the struct type for the Microsoft _GUID, used for the __uuidof
operator (an extension).  Its definition is

  struct _GUID {
    unsigned long  Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[8];
  };

*/
{
  a_field_ptr last_field;

  if (guid_type == NULL) {
    /* Make the _GUID struct type.  It doesn't actually have a name. */
    guid_type = alloc_type((a_type_kind)tk_struct);
    add_to_front_of_file_scope_types_list(guid_type);
    last_field = NULL;
    /* field: unsigned long Data1; */
    make_lowered_field("Data1",
                       integer_type((an_integer_kind)ik_unsigned_long),
                       guid_type, &last_field);
    /* field: unsigned short Data2; */
    make_lowered_field("Data2",
                       integer_type((an_integer_kind)ik_unsigned_short),
                       guid_type, &last_field);
    /* field: unsigned short Data3; */
    make_lowered_field("Data3",
                       integer_type((an_integer_kind)ik_unsigned_short),
                       guid_type, &last_field);
    /* field: unsigned char Data4[8]; */
    guid_array_type = alloc_type((a_type_kind)tk_array);
    guid_array_type->variant.array.element_type =
                               integer_type((an_integer_kind)ik_unsigned_char);
    guid_array_type->variant.array.variant.number_of_elements = 8;
    set_type_size(guid_array_type);
    make_lowered_field("Data4", guid_array_type, guid_type, &last_field);
    finish_class_type(guid_type);
  }  /* if */
  return guid_type;
}  /* make_guid_type */


static a_constant_ptr conv_uuid_constant(char            **ptr,
                                         int             ndigits,
                                         an_integer_kind ikind)
/*
Convert ndigits hexadecimal digits of the uuid string at *ptr, and increment
*ptr by ndigits.  Put the converted digits into an integer constant with
kind ikind, allocate an unshared copy, and return a pointer to the
allocated integer constant.
*/
{
  char             *local_ptr = *ptr;
  a_constant       con;
  a_constant_ptr   con_ptr;
  a_boolean        err;
  an_integer_value digit;

  /* Start with zero. */
  make_zero_of_proper_type(integer_type(ikind), &con);
  /* Loop to convert each hexadecimal digit. */
  for (; ndigits > 0; ndigits--) {
    char ch = *local_ptr++;
    int  intdigit = hexvalue(ch);
    /* Multiply previous value by 16. */
    shift_left_integer_value(&con.variant.integer_value, 4, &err);
    /* Or in digit. */
    set_unsigned_integer_value(&digit, (a_host_large_unsigned)intdigit);
    or_integer_values(&con.variant.integer_value, &digit);
  }  /* for */
  *ptr = local_ptr;
  /* Allocate the final constant. */
  con_ptr = alloc_unshared_constant(&con);
  return con_ptr;
}  /* conv_uuid_constant */


static a_variable_ptr uuid_variable_for_type(a_type_ptr type)
/*
Return a pointer to the uuid variable for the indicated class or enum type,
creating the variable if necessary.  This relates to the Microsoft extensions
that deal with GUIDs for the COM by way of the __declspec(uuid(...))
modifier and the __uuidof() expression operator.  The uuid variable is
initialized with the right values for the uuid associated with the
class or enum type.  type is NULL to request the uuid variable for a null
GUID.
*/
{
  a_variable_ptr              *p_uuid_var;
  a_variable_ptr              uuid_var;
  char                        *uuid_string;

  if (type != NULL) {
    if (is_immediate_class_type(type)) {
      p_uuid_var = &type->variant.class_struct_union.extra_info->uuid_variable;
      uuid_string = type->variant.class_struct_union.extra_info->uuid_string;
    } else if (is_immediate_enum_type(type)) {
      p_uuid_var = &type->variant.integer.uuid_variable;
      uuid_string = type->variant.integer.uuid_string;
    } else {
      unexpected_condition_str("uuid_variable_for_type: bad type kind");
    }  /* if */
  } else {
    /* NULL GUID is wanted. */
    p_uuid_var = &null_guid_variable;
    uuid_string = "00000000-0000-0000-0000-000000000000";
  }  /* if */
  uuid_var = *p_uuid_var;
  if (uuid_var == NULL) {
    a_memory_region_number
                   region_to_switch_back_to;
    char           *ptr = uuid_string;
    a_constant_ptr aggr, con1, con2, con3, con4, prev_con;
    int            i;

    /* Create the (unnamed) uuid variable. */
    /* Note that the Microsoft implementation uses linker support to allocate
       a single structure per GUID across all compilation units.  We don't
       have the ability to do that in a portable way.  This should be
       changed on implementations that want to make compilers for a
       Microsoft environment. */
    uuid_var = make_lowered_variable((char *)NULL, /*already_il_name=*/TRUE,
                                     make_guid_type(),
                                     (a_storage_class)sc_static);
    *p_uuid_var = uuid_var;
    switch_to_file_scope_region(&region_to_switch_back_to);
    /* Convert the uuid string to a list of initializer constants. */
    /* The string looks like ("h" is a hexadecimal digit):
         hhhhhhhh-hhhh-hhhh-hhhh-hhhhhhhhhhhh
         --Data1- -D2- -D3- ------Data4------
    */
    check_assertion_str(ptr != NULL,
                        "uuid_variable_for_type: null uuid_string");
    prev_con = NULL;
    /* Data1. */
    con1 = conv_uuid_constant(&ptr, 8, (an_integer_kind)ik_unsigned_long);
    ptr++;  /* Skip "-". */
    /* Data2. */
    con2 = conv_uuid_constant(&ptr, 4, (an_integer_kind)ik_unsigned_short);
    ptr++;  /* Skip "-". */
    /* Data3. */
    con3 = conv_uuid_constant(&ptr, 4, (an_integer_kind)ik_unsigned_short);
    ptr++;  /* Skip "-". */
    /* Data4. */
    /* This is an aggregate constant with 8 constants under it, one for
       each element of the unsigned char array. */
    con4 = alloc_constant((a_constant_repr_kind)ck_aggregate);
    con4->type = guid_array_type;
    prev_con = NULL;
    for (i = 0; i < 8; i++) {
      a_constant_ptr con4e =
                conv_uuid_constant(&ptr, 2, (an_integer_kind)ik_unsigned_char);
      if (prev_con == NULL) {
        con4->variant.aggregate.first_constant = con4e;
      } else {
        prev_con->next = con4e;
      }  /* if */
      prev_con = con4e;
      /* Skip "-" after first 4 hex digits. */
      if (i == 1) ptr++;
    }  /* for */
    con4->variant.aggregate.last_constant = prev_con;
    check_assertion_str(*ptr == '\0',
            "uuid_variable_for_type: uuid string does not end where expected");
    /* Assemble the four constants under another aggregate constant. */
    aggr = alloc_constant((a_constant_repr_kind)ck_aggregate);
    aggr->type = uuid_var->type;
    aggr->variant.aggregate.first_constant = con1;
    con1->next = con2;
    con2->next = con3;
    con3->next = con4;
    aggr->variant.aggregate.last_constant = con4;
    /* Attach the aggregate constant as the initial value of the variable. */
    uuid_var->init_kind = (an_init_kind)initk_static;
    uuid_var->initializer.constant = aggr;
    switch_back_to_original_region(region_to_switch_back_to);
  }  /* if */
  return uuid_var;
}  /* uuid_variable_for_type */


void lower_uuidof(a_constant *con)
/*
Lower a constant generated for the Microsoft C++ extension __uuidof().
Its value is the address of a struct of type _GUID, which provides
information about the __declspec(uuid(...)) attribute with which the
associated class or enum was declared.
*/
{
  a_type_ptr     type = con->variant.address.variant.type;
  a_type_ptr     orig_con_type = con->type;
  a_source_correspondence
                 orig_source_corresp;
  a_variable_ptr uuid_var;
  a_constant_ptr con_next = con->next;

  orig_source_corresp = con->source_corresp;
  /* Create the initialized uuid variable for the type, if it doesn't
     exist already. */
  uuid_var = uuid_variable_for_type(type);
  /* Replace the constant with one that is the address of the uuid
     variable, cast to the right type.  The cast is needed at least
     to add "const", but it also covers any mismatch between the runtime
     idea of _GUID and the actual declaration in the user's source. */
  set_variable_address_constant(uuid_var, con,
                                /*set_address_taken_flag=*/TRUE);
  implicit_cast(con, orig_con_type);
  con->source_corresp = orig_source_corresp;
  con->next = con_next;
#if MAINTAIN_NEEDED_FLAGS
  /* If the constant has already been marked as needed, mark it as
     needed again and visit its new subtree. */
  remark_as_needed((char *)con, iek_constant);
#endif /* MAINTAIN_NEEDED_FLAGS */
}  /* lower_uuidof */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

void init_lower_one_time_init(void)
/*
Do one-time initialization of static variables declared in lower_init.c.
*/
{
  /* Save variables from lower_init.c that are needed for precompiled
     headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(vec_new_routine),
#if !IA64_ABI
      pch_saved_var_array_elem(vec_new_eh_routine),
      pch_saved_var_array_elem(vec_new_eh_zero_routine),
      pch_saved_var_array_elem(array_new_routine),
      pch_saved_var_array_elem(array_new_zero_routine),
      pch_saved_var_array_elem(placement_array_new_routine),
      pch_saved_var_array_elem(placement_array_new_zero_routine),
#else /* IA64_ABI */
      pch_saved_var_array_elem(vec_new2_routine),
      pch_saved_var_array_elem(vec_new3_routine),
      pch_saved_var_array_elem(vec_ctor_routine),
#endif /* !IA64_ABI */
      pch_saved_var_array_elem(vec_cctor_routine),
#if !IA64_ABI
      pch_saved_var_array_elem(vec_cctor_eh_routine),
#endif /* !IA64_ABI */
      pch_saved_var_array_elem(vec_delete_routine),
#if !IA64_ABI
      pch_saved_var_array_elem(array_delete_routine),
#else /* IA64_ABI */
      pch_saved_var_array_elem(vec_delete2_routine),
      pch_saved_var_array_elem(vec_delete3_routine),
      pch_saved_var_array_elem(vec_dtor_routine),
#endif /* IA64_ABI */
      pch_saved_var_array_elem(memzero_routine),
      pch_saved_var_array_elem(record_needed_destruction_routine),
#if !IA64_ABI
      pch_saved_var_array_elem(needed_destruction_type),
      pch_saved_var_array_elem(needed_destruction_object_field),
      pch_saved_var_array_elem(array_new_prefix_size_var),
#else /* IA64_ABI */
#if IA64_ABI_USE_GUARD_ACQUIRE_RELEASE
      pch_saved_var_array_elem(guard_acquire_routine),
      pch_saved_var_array_elem(guard_release_routine),
#endif /* IA64_ABI_USE_GUARD_ACQUIRE_RELEASE */
      pch_saved_var_array_elem(dso_handle_var),
#endif /* IA64_ABI */
#if !USE_INIT_SECTION_IN_GENERATED_C
      pch_saved_var_array_elem(linkl_type),
#endif /* !USE_INIT_SECTION_IN_GENERATED_C */
#if MICROSOFT_EXTENSIONS_ALLOWED
      pch_saved_var_array_elem(guid_type),
      pch_saved_var_array_elem(guid_array_type),
      pch_saved_var_array_elem(null_guid_variable),
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  /* Register variables that must be saved and restored when switching
     between translation units. */
  register_trans_unit_variable(vec_new_routine);
#if !IA64_ABI
  register_trans_unit_variable(vec_new_eh_routine);
  register_trans_unit_variable(vec_new_eh_zero_routine);
  register_trans_unit_variable(array_new_routine);
  register_trans_unit_variable(array_new_zero_routine);
  register_trans_unit_variable(placement_array_new_routine);
  register_trans_unit_variable(placement_array_new_zero_routine);
#else /* !IA64_ABI */
  register_trans_unit_variable(vec_new2_routine),
  register_trans_unit_variable(vec_new3_routine),
  register_trans_unit_variable(vec_ctor_routine),
#endif /* !IA64_ABI */
  register_trans_unit_variable(vec_cctor_routine);
#if !IA64_ABI
  register_trans_unit_variable(vec_cctor_eh_routine);
#endif /* !IA64_ABI */
  register_trans_unit_variable(vec_delete_routine);
#if !IA64_ABI
  register_trans_unit_variable(array_delete_routine);
#else /* IA64_ABI */
  register_trans_unit_variable(vec_delete2_routine);
  register_trans_unit_variable(vec_delete3_routine);
  register_trans_unit_variable(vec_dtor_routine);
#endif /* IA64_ABI */
  register_trans_unit_variable(memzero_routine);
  register_trans_unit_variable(record_needed_destruction_routine);
#if !IA64_ABI
  register_trans_unit_variable(needed_destruction_type);
  register_trans_unit_variable(needed_destruction_object_field);
  register_trans_unit_variable(array_new_prefix_size_var);
#else /* IA64_ABI */
#if IA64_ABI_USE_GUARD_ACQUIRE_RELEASE
  register_trans_unit_variable(guard_acquire_routine);
  register_trans_unit_variable(guard_release_routine);
#endif /* IA64_ABI_USE_GUARD_ACQUIRE_RELEASE */
  register_trans_unit_variable(dso_handle_var);
#endif /* IA64_ABI */
#if !USE_INIT_SECTION_IN_GENERATED_C
  register_trans_unit_variable(linkl_type);
#endif /* !USE_INIT_SECTION_IN_GENERATED_C */
#if MICROSOFT_EXTENSIONS_ALLOWED
  register_trans_unit_variable(guid_type);
  register_trans_unit_variable(guid_array_type);
  register_trans_unit_variable(null_guid_variable);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* init_lower_one_time_init */


void init_lower_trans_unit_init(void)
/*
Initialize static variables related to this file that must be initialized
for each translation unit.
*/
{
  vec_new_routine = NULL;
#if !IA64_ABI
  vec_new_eh_routine = NULL;
  vec_new_eh_zero_routine = NULL;
  array_new_routine = NULL;
  array_new_zero_routine = NULL;
  placement_array_new_routine = NULL;
  placement_array_new_zero_routine = NULL;
#else /* IA64_ABI */
  vec_new2_routine = NULL;
  vec_new3_routine = NULL;
  vec_ctor_routine = NULL;
#endif /* IA64_ABI */
  vec_cctor_routine = NULL;
#if !IA64_ABI
  vec_cctor_eh_routine = NULL;
#endif /* !IA64_ABI */
  vec_delete_routine = NULL;
#if !IA64_ABI
  array_delete_routine = NULL;
#else /* IA64_ABI */
  vec_delete2_routine = NULL;
  vec_delete3_routine = NULL;
  vec_dtor_routine = NULL;
#endif /* IA64_ABI */
  memzero_routine = NULL;
  record_needed_destruction_routine = NULL;
#if !IA64_ABI
  needed_destruction_type = NULL;
  needed_destruction_object_field = NULL;
  array_new_prefix_size_var = NULL;
#else /* IA64_ABI */
#if IA64_ABI_USE_GUARD_ACQUIRE_RELEASE
  guard_acquire_routine = NULL;
  guard_release_routine = NULL;
#endif /* IA64_ABI_USE_GUARD_ACQUIRE_RELEASE */
  dso_handle_var = NULL;
#endif /* IA64_ABI */
#if !USE_INIT_SECTION_IN_GENERATED_C
  linkl_type = NULL;
#endif /* !USE_INIT_SECTION_IN_GENERATED_C */
#if MICROSOFT_EXTENSIONS_ALLOWED
  guid_type = NULL;
  guid_array_type = NULL;
  null_guid_variable = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* init_lower_trans_unit_init */


void init_lower_init(void)
/*
Initialize static variables related to this file that must be initialized
for each compilation.
*/
{
  processing_file_scope_init_routine = FALSE;
  /* init_lower_trans_unit_init is called from il_lower_trans_unit_init. */
}  /* init_lower_init */

#endif /* DO_IL_LOWERING */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
