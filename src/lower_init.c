/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2013 Edison Design Group Inc.                   [_]          *
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
#include "il_walk.h"


/* Declarations needed because of forward references: */
static void lower_destructor_dynamic_init(
                                   a_dynamic_init_ptr     dip,
                                   an_init_pos_descr_ptr  ipdp,
                                   a_boolean              have_complete_object,
                                   an_expr_node_ptr       vtt_addr_node,
                                   an_insert_location_ptr insert_location);
static void reset_conditional_flag_var(a_variable_ptr     conditional_flag_var,
                                       an_insert_location *insert_location);
static an_expr_node_ptr make_assignment_expr_with_subobject_fix(
                                    an_expr_node_ptr      dest_node,
                                    a_boolean             have_complete_object,
                                    an_expr_operator_kind op,
                                    an_expr_node_ptr      source_node);
#if !IA64_ABI
static a_variable_ptr implicit_virtual_base_parameter(
                                                a_type_ptr     class_type,
                                                a_type_ptr     base_class_type,
                                                a_variable_ptr this_param_var);
#endif /* !IA64_ABI */
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
static void build_construction_vtbls_pointer_for_subobject_construction(
                                 a_dynamic_init_ptr     dip,
                                 a_base_class_ptr       base_class,
                                 an_init_pos_descr      *ipdp,
                                 a_variable_ptr         construction_vtbls_var,
                                 an_insert_location_ptr insert_location,
                                 an_expr_node_ptr       *implied_arg_node,
                                 a_boolean              *just_test);
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
static a_routine_ptr helper_routine_to_initialize_entity(
                                            a_type_ptr    type,
                                            a_boolean     have_complete_object,
                                            a_boolean     need_array_count,
                                            a_boolean     zero_entity,
                                            a_routine_ptr ctor_routine);
static void insert_call_to_initialize_entity(
                                       a_type_ptr         entity_type,
                                       a_boolean          have_complete_object,
                                       an_expr_node_ptr   entity_node,
                                       an_expr_node_ptr   num_elem_node,
                                       a_targ_size_t      array_element_count,
                                       an_expr_node_ptr   source_node,
                                       an_insert_location *insert_location);
static void insert_call_to_zero_entity(a_type_ptr         entity_type,
                                       a_boolean          have_complete_object,
                                       an_expr_node_ptr   entity_node,
                                       an_expr_node_ptr   num_elem_node,
                                       a_targ_size_t      array_element_count,
                                       an_insert_location *insert_location);
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
static a_variable_ptr make_construction_vtbls_array(
                                           a_type_ptr              class_type,
                                           a_construction_vtbl_ptr elements);
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
static void make_delete_call_statement(a_routine_ptr      delete_routine,
                                       a_type_ptr         delete_type,
                                       an_expr_node_ptr   arg_node,
                                       an_insert_location *insert_location);
#if LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS
static a_boolean call_to_ctor_or_dtor_has_no_effect(
                                         a_routine_ptr    routine,
                                         an_expr_node_ptr args,
                                         a_boolean        call_can_be_virtual);
#endif /* LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS */
static void set_up_freeing_of_storage_on_exception(
                                 a_new_delete_supplement_ptr ndsp,
                                 an_init_pos_descr_ptr       ipdp,
                                 an_insert_location          *insert_location);
static void turn_off_freeing_of_storage_on_exception(
                             a_new_delete_supplement_ptr ndsp,
                             an_init_pos_descr_ptr       ipdp,
                             an_expr_node_ptr            delete_args,
                             a_routine_ptr               new_routine,
                             an_expr_node_ptr            init_expr,
                             an_insert_location          *insert_location);
static void insert_pending_stmk_init_statements_at_mark(
                                          an_insert_location *insert_location);
static an_expr_node_ptr num_elem_node_if_array(an_init_pos_descr_ptr ipdp);
static void lower_ctor_init(a_constructor_init_ptr ctor_init,
                            a_variable_ptr         this_param_var,
                            a_boolean              base_of_complete_object,
                            a_variable_ptr         construction_vtbls_var,
                            an_insert_location_ptr insert_location);


static a_type_ptr make_function_type(a_type_ptr return_type,
                                     a_type_ptr param_1_type,
                                     a_type_ptr param_2_type)
/*
Make a function type for a prototyped function prototyped as
having parameters with type param_1_type and param_2_type, and
returning return_type, and return a pointer to it.  param_1_type
and param_2_type can be NULL if fewer parameters are needed.
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
    if (param_2_type != NULL) {
      ptp = alloc_param_type(param_2_type);
      /* It is not necessary to clear il_lowering_flag; the entry does not need
         to be lowered. */
      rout_type->variant.routine.extra_info->param_type_list->next = ptp;
    }  /* if */
  }  /* if */
  return rout_type;
}  /* make_function_type */


static a_routine_ptr make_rout_entry_no_add(a_const_char    *name,
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

  rout_type = make_function_type(return_type, param_1_type, (a_type_ptr)NULL);
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


static a_routine_ptr make_rout_entry(a_const_char    *name,
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


static a_routine_ptr find_existing_runtime_routine(a_const_char *name,
                                                   a_type_ptr   rout_type)
/*
See if an existing runtime routine entry named "name" with a type that
matches rout_type can be found.  If so, return a pointer to the routine,
otherwise return NULL.
*/
{
  a_symbol_locator  locator, ext_locator;
  a_symbol_ptr      sym;
  a_routine_ptr     routine = NULL;

  clear_locator(&locator, &null_source_position);
  (void)find_symbol(name, strlen(name), &locator);
  /* Keep find_external_symbol from using current namespace if any. */
  locator.is_file_scope_qualified_name = TRUE;
  check_assertion(!C_mode());
  sym = find_external_symbol(&locator, (a_name_linkage_kind)nlk_external,
                             rout_type, &ext_locator);
  /* See if we found a suitable symbol.  Require an exact match on
     the symbol name (to prevent re-using an existing routine that may
     differ only in case sensitivity or number of unique significant
     characters).  This may cause linker errors later, but it's safer
     to create a new routine entry. */
  if (sym != NULL && sym->kind == (a_symbol_kind)sk_extern_routine &&
      param_types_are_compatible(skip_typerefs(rout_type),
                        skip_typerefs(sym->variant.extern_symbol_descr->type),
                        TCF_NO_FLAGS) &&
      strcmp(name, sym->header->identifier) == 0) {
    /* We found an existing external routine with the correct name, type
       and linkage. */
    routine = sym->variant.extern_symbol_descr->variant.routine.ptr;
  }  /* if */
  return routine;
}  /* find_existing_runtime_routine */


a_routine_ptr make_runtime_routine(a_const_char  *name,
                                   a_routine_ptr *routine,
                                   a_type_ptr    return_type)
/*
Make a routine entry for the runtime routine named "name" and return a
pointer to it.  Also save the pointer in *routine.  If *routine is non-NULL
on entry, use that pointer.  The routine has unprototyped arguments and
its return type is return_type.  Note that an existing routine definition
may exist for this function (e.g., when compiling the run time library),
in which case this will create a second routine entry for the same function.
*/
{
  if (*routine == NULL) {
    *routine = make_rout_entry(name, (a_storage_class)sc_extern, return_type,
                               (a_type_ptr)NULL);
    (*routine)->type->variant.routine.extra_info->prototyped = FALSE;
  }  /* if */
  return *routine;
}  /* make_runtime_routine */


a_routine_ptr make_prototyped_runtime_routine(a_const_char     *name,
                                              a_routine_ptr    *routine,
                                              a_type_ptr       return_type,
                                              a_type_ptr       param1_type,
                                              a_type_ptr       param2_type,
                                              a_type_ptr       param3_type)
/*
Make a routine entry for the runtime routine named "name" and return a
pointer to it.  Also save the pointer in *routine.  If *routine is non-NULL
on entry, use that pointer.  The routine has the indicated return type
and parameter types and is prototyped.  Parameters can be left out by
passing NULL parameter types (e.g., a non-NULL param1_type and a NULL
param2_type creates a prototype for a function taking a single argument).
If building the run time library and we don't already have a specified
routine entry, see if an existing routine entry matches the specified
calling sequence.
*/
{
  a_type_ptr rout_type;

  if (*routine == NULL && building_runtime) {
    /* See if a routine entry already exists before we create one.  This
       happens only when compiling the run time library and prevents
       multiple routine entries for the same routine. */
    rout_type = make_routine_type(return_type, param1_type, param2_type,
                                  param3_type, (a_type_ptr)NULL);
    *routine = find_existing_runtime_routine(name, rout_type);
  }  /* if */
  if (*routine == NULL) {
    /* No existing routine, create one. */
    *routine = make_rout_entry(name, (a_storage_class)sc_extern, return_type,
                               (a_type_ptr)NULL);
    (*routine)->compiler_generated = TRUE;
    rout_type = (*routine)->type;
    rout_type->variant.routine.extra_info->prototyped = TRUE;
    if (param1_type != NULL) {
      a_param_type_ptr first_param = alloc_param_type(param1_type);
      rout_type->variant.routine.extra_info->param_type_list = first_param;
      if (param2_type != NULL) {
        first_param->next = alloc_param_type(param2_type);
        if (param3_type != NULL) {
          first_param->next->next = alloc_param_type(param3_type);
        }  /* if */
      } else {
        check_assertion(param3_type == NULL);
      }  /* if */
    } else {
      check_assertion(param2_type == NULL && param3_type == NULL);
    }  /* if */
  }  /* if */
  return *routine;
}  /* make_prototyped_runtime_routine */


#if !IA64_ABI
/*ARGSUSED*/ /* <-- class_type and bcp are unused in that case. */
#endif /* !IA64_ABI */
void make_vtbl_address_constant(a_variable_ptr   var,
                                a_type_ptr       class_type,
                                a_base_class_ptr bcp,
                                a_constant       *addr_constant)
/*
Make an address constant for the address of a virtual function table variable
(var) and return it in *addr_constant.  class_type is the type whose
constructor or destructor is being generated; bcp is the base whose virtual
function table is being addressed, or NULL if the primary virtual function
table is being addressed.  The variable has an array type.  The pointer has
type pointer to element.
*/
{
  a_type_ptr                  ptr_element_type;
#if IA64_ABI
  a_virtual_table_index       vtbl_index;
#endif /* IA64_ABI */

  ptr_element_type = make_pointer_type(array_element_type(var->type));
  /* Make a constant for the address of the array, implicitly cast it to
     pointer-to-element-type, and make an expression whose value is the
     address constant.  This gives an address with the right type. */
  set_variable_address_constant(var, addr_constant,
                                /*set_address_taken_flag=*/FALSE);
  implicit_cast(addr_constant, ptr_element_type);
  set_variable_address_taken(var);
  var->source_corresp.referenced = TRUE;
#if IA64_ABI
  /* Add the offset from the start of the variable to the actual address
     point. */
  check_assertion(bcp == NULL || !bcp->shares_virtual_function_info);
  vtbl_index = num_negative_vtable_entries(class_type, bcp);
  if (bcp != NULL) {
    vtbl_index += bcp->virtual_function_table_offset;
  }  /* if */
  addr_constant->variant.address.offset = vtbl_index * (long)vtbl_entry_size();
#endif /* IA64_ABI */
}  /* make_vtbl_address_constant */


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

  make_vtbl_address_constant(var, class_type, bcp, &addr_constant);
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
#if LOWER_FIXED_POINT
  if (fixed_point_enabled &&
      is_fixed_point_type(arg_type)) {
    /* Fixed-point types get turned into integer types. */
    arg_type = lowered_integer_type_for_fixed_point_type(arg_type);
  }  /* if */
#endif /* LOWER_FIXED_POINT */
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

    cast_node(&expr_cast, promoted_type,
              /*check_cast_access=*/FALSE, /*check_ambiguity=*/FALSE,
              /*is_implicit_cast=*/TRUE, /*is_reinterpret_cast=*/FALSE,
              /*reinterpret_semantics=*/FALSE,
              /*within_expr_processing=*/FALSE,
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


a_type_ptr lowered_return_type_of(a_type_ptr routine_type)
/*
Return the type that is the return type of the given function type, as
it would appear as the type on a call of the function in the lowered IL.
*/
{
  a_type_ptr return_type;

  routine_type = skip_typerefs(routine_type);
#if CTORS_RETURN_THIS
  if (routine_type->variant.routine.extra_info->assoc_routine_is_ctor) {
    /* Constructors return "pointer to class" in the Cfront-like ABI and
       a variant of the IA-64 ABI. */
    a_type_ptr class_type =
                          routine_type->variant.routine.extra_info->this_class;
    check_assertion(class_type != NULL);
    return_type = make_pointer_type(class_type);
  } else
#endif /* CTORS_RETURN_THIS */
#if DTORS_RETURN_THIS
  if (routine_type->variant.routine.extra_info->assoc_routine_is_dtor) {
    /* Destructors return "void *" in a variant of the IA-64 ABI.
       Note that deleting destructors return void even in that
       variant, but those do not have assoc_routine_is_dtor set (it's
       not set for entry points) so there's no problem here. */
    return_type = void_star_type();
  } else
#endif /* DTORS_RETURN_THIS */
  /* Do not insert code here. */
  {
    return_type = il_return_type_of(routine_type);
    if (is_reference_type(return_type)) {
      /* Turn a reference type into a pointer type. */
      return_type = make_pointer_type(type_pointed_to(return_type));
    }  /* if */
  }  /* if */
  return return_type;
}  /* lowered_return_type_of */


static an_expr_node_ptr make_call_node_full(
                                           a_routine_ptr      routine,
                                           an_expr_node_ptr   arg_list,
                                           an_expr_node_ptr   return_value,
                                           a_boolean          is_virtual_call,
                                           an_insert_location *insert_location)
/*
Make an expression that calls routine "routine" with arguments "arg_list",
and return a pointer to it (or to the assignment expression generated if
return_value is non-NULL).  arg_list is assumed to be lowered already.
If return_value is non-NULL, it represents an lvalue to which the return
value from the call operation will be assigned (otherwise any value returned
by the call is ignored).  If insert_location is not NULL, an expression
statement containing the created call node (or assignment expression) is
inserted at *insert_location.  If is_virtual_call is TRUE, the call is virtual,
and the caller will do further lowering on the returned expression.  In cases
where a non-NULL insert_location is specified and inlining of the routine is
possible, the returned expression is NULL (reflecting that the call has been
inlined and inserted into the specified location).  For this reason, use of
either make_call_node or make_call_statement is preferred when possible.
*/
{
  an_expr_node_ptr      call_node, rout_node, top_level_node;
  a_type_ptr            rout_return_type;
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
  /* Make a node for the routine. */
  rout_node = function_rvalue_expr(routine);
  routine->called = TRUE;
  rout_node->next = arg_list;
  /* Lower the function type (or record it as an orphan).  This is important
     when calling routines mentioned in dynamic initialization entries that
     are declared now and get defined later in this compilation.  The type
     pointer in the routine entry will be changed to a new (equivalent) type
     at the point of definition, which means the type here will not be
     attached to any list and will not get lowered unless we do it here. */
  lower_os_type(routine->type);
  /* Make the call node. */
  rout_return_type = lowered_return_type_of(routine->type);
  call_node = make_operator_node((an_expr_operator_kind)eok_call,
                                 rout_return_type, rout_node);
  /* If the caller requested that the return value from the call be
     assigned to an lvalue expression, create the assignment expression. */
  if (return_value != NULL) {
    check_assertion(return_value->is_lvalue);
    top_level_node = make_assignment_expr(return_value,
                                          (an_expr_operator_kind)eok_assign,
                                          call_node);
  } else {
    top_level_node = call_node;
  }  /* if */
  if (insert_location != NULL) {
#if MINIMAL_INLINING
    call_stmt = insert_expr_statement_set_pos(top_level_node, insert_location);
#else /* !MINIMAL_INLINING */
    (void)insert_expr_statement_set_pos(top_level_node, insert_location);
#endif /* MINIMAL_INLINING */
  }  /* if */
  if (is_virtual_call) {
    call_node->variant.operation.is_virtual_call = TRUE;
  } else {
    routine->source_corresp.referenced = TRUE;
#if MINIMAL_INLINING
    if (inlining_enabled && top_level_node == call_node) {
      a_boolean expr_has_been_detached;
      do_inlining_of_call(call_node, call_stmt, &expr_has_been_detached);
      if (expr_has_been_detached) top_level_node = NULL;
    }  /* if */
#endif /* MINIMAL_INLINING */
  }  /* if */
  return top_level_node;
}  /* make_call_node_full */


an_expr_node_ptr make_call_node(a_routine_ptr      routine,
                                an_expr_node_ptr   arg_list)
/*
Make an expression that calls routine "routine" with arguments "arg_list",
and return a pointer to it.  arg_list is assumed to be lowered already.
If inlining is enabled and the called routine is inline, inlining of the call
will be attempted.
*/
{
  an_expr_node_ptr call_node;

  call_node = make_call_node_full(routine, arg_list, (an_expr_node_ptr)NULL,
                                  /*is_virtual_call=*/FALSE,
                                  (an_insert_location *)NULL);
  check_assertion(call_node != NULL);
  return call_node;
}  /* make_call_node */


void make_call_statement(a_routine_ptr      routine,
                         an_expr_node_ptr   arg_list,
                         an_expr_node_ptr   return_value,
                         an_insert_location *insert_location)
/*
Make a statement that calls routine "routine" with arguments "arg_list"
and insert it at *insert_location.  arg_list is assumed to be lowered already.
If return_value is non-NULL, it represents an lvalue to which the result
of the call is assigned.
*/
{
  (void)make_call_node_full(routine, arg_list, return_value,
                            /*is_virtual_call=*/FALSE, insert_location);
}  /* make_call_statement */


an_expr_node_ptr make_runtime_rout_call(a_const_char     *name,
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
  node = make_call_node(*routine, arg_expr_list);
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
Add an "if" statement around the entire body of the routine (a constructor,
destructor, or generated thread_local initialization routine) whose scope is
pointed to by scope.  if_node is the expression to be tested in the "if".
return_var is the variable to be returned if a "return" statement must be
generated, or NULL if no value needs to be returned.  This routine should only
be used when adding boilerplate code to constructors or destructors.
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
#if LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS
  /* Mark this statement as okay to ignore when trying to decide later
     whether this routine has an effect. */
  if_stmt->is_lowering_boilerplate = TRUE;
#endif /* LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS */
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
  scope->parent = il_header.primary_scope;
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
    /* Make a return statement at the end of the block. */
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
#if GNU_VECTOR_TYPES_ALLOWED
  ipmp->is_vector_element  = FALSE;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
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

#if GNU_VECTOR_TYPES_ALLOWED

static a_boolean initializing_vector_element(an_init_pos_modifier_ptr ipmp)
/*
Return TRUE if one of the modifiers on the specified list indicates that
this initialization is modifying an element of a vector.
*/
{
  a_boolean result = FALSE;

  for (; ipmp != NULL; ipmp = ipmp->next) {
    if (ipmp->is_vector_element) {
      result = TRUE;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* initializing_vector_element */

#endif /* GNU_VECTOR_TYPES_ALLOWED */

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
  ipdp->base_of_complete_object   = FALSE;
  ipdp->base_type                 = NULL;
  ipdp->modifiers                 = NULL;
  ipdp->array_element_count       = 0;
  ipdp->array_element_type        = NULL;
  ipdp->num_elem_node             = NULL;
  ipdp->partial_initialization_starting_element = -1;
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
                     var_has_static_or_thread_storage_duration(ipdp->variable);
  return is_for_static_var;
}  /* init_pos_is_static */


static void clear_destructible_entity_descr(
                                          a_destructible_entity_descr_ptr dedp)
/*
Clear the fields of a destructible entity description to default values.
*/
{
  dedp->next = NULL;
  clear_init_pos_descr(&dedp->init_pos_descr);
  dedp->cleanup_state_to_set_when_starting_destruction = NULL;
  dedp->conditional_flag_var = NULL;
#if DO_FULL_PORTABLE_EH_LOWERING
  dedp->conditional_flag_handle = 0;
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
#if GENERATE_EH_TABLES
  dedp->region_number = null_eh_region_number;
  dedp->region_table_entry = NULL;
  dedp->next_in_region_table = NULL;
#endif /* GENERATE_EH_TABLES */
  dedp->initialization_done = FALSE;
  dedp->needs_subobject_construction_vtbl = FALSE;
  dedp->construction_vtbls_var_is_array = FALSE;
  dedp->use_delegation_dtor = FALSE;
  dedp->delegation_dtor_arg = NULL;
  dedp->construction_vtbls_var = NULL;
  dedp->subobject_construction_base_class = NULL;
}  /* clear_destructible_entity_descr */


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
  clear_destructible_entity_descr(dedp);
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
}  /* free_destructible_entity_descr */


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
    case cik_delegation:
      /* No modification is necessary. */
      break;
    default:
      unexpected_condition_str("modify_ctor_init_pos_descr: bad kind");
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
                                             an_expr_node_ptr      entity_node)
/*
The entity given by the expression entity_node is to be initialized by
executable code.  entity_node can be either an rvalue pointer or an lvalue.
If it is "const", drop the const by casting so the entity can be written to.
ipdp is the init position description for the complete entity being initialized
(or NULL for an internal adjustment, e.g., for an array element).
*/
{
  a_type_ptr entity_type = entity_node->type;

  if (!entity_node->is_lvalue) {
    check_assertion(is_pointer_type(entity_type));
    entity_type = type_pointed_to(entity_type);
  }  /* if */
  /* If the entity is an array, don't drop the const at this level.  It
     will be dropped on the address of the array element once that is
     extracted. */
  if (is_const_qualified_type(entity_type) && !is_array_type(entity_type)) {
    a_type_qualifier_set qualifiers = get_type_qualifiers(entity_type);
    qualifiers &= ~(a_type_qualifier_set)TQ_CONST;
    entity_type = make_unqualified_type(entity_type);
    entity_type = make_qualified_type(entity_type, qualifiers);
    if (entity_node->is_lvalue) {
      entity_node = add_cast_to_glvalue_if_necessary(entity_node, entity_type);
    } else {
      entity_node = add_cast(entity_node, make_pointer_type(entity_type));
    }  /* if */
  }  /* if */
  return entity_node;
}  /* drop_const_on_init_entity_node */


static an_expr_node_ptr modify_init_entity_node(
                                        an_expr_node_ptr         entity_node,
                                        an_init_pos_modifier_ptr modifiers,
                                        a_boolean                using_as_dest,
                                        a_boolean                is_vla)
/*
Add the address modifiers from the list given by "modifiers" (from an
init position description) to the entity lvalue expression "entity_node"
and return a pointer to the modified expression tree.  If using_as_dest is
TRUE, the entity is the destination of an initialization operation.
If is_vla is TRUE, this is the first modifier on a base variable that
is a variable-length array.
*/
{
  an_expr_node_ptr elem_num_node;

  check_assertion(entity_node->is_lvalue);
  /* If there are no modifiers, return the original node. */
  if (modifiers != NULL) {
#if GNU_VECTOR_TYPES_ALLOWED
    check_assertion(!modifiers->is_vector_element);
#endif /* GNU_VECTOR_TYPES_ALLOWED */
    /* Process the modifiers preceding the final modifier, then add the final
       qualifier (recall that the modifiers are in order from the innermost
       to the outermost). */
    entity_node = modify_init_entity_node(entity_node, modifiers->next,
                                          using_as_dest, /*is_vla=*/FALSE);
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
      check_assertion(is_array_type(entity_node->type));
      /* Do the pointer decay from array to pointer to element. */
      if (is_vla) {
        /* We have an lvalue for a VLA (type "array[EXPR] of T").  Take
           the address of the node (yielding type 
           "pointer to array[EXPR] of T" -- which will be rewritten
           (by lower_vla_types) to "pointer to T" -- the same type that
           results from an array decay). */
        entity_node = add_address_of_to_node(entity_node);
      } else {
        /* Perform array to pointer decay. */
        entity_node = make_array_to_pointer_node(entity_node);
      }  /* if */
      if (using_as_dest) {
        /* The entity will be used as the destination of an initialization, so
           drop "const" (if present) from the type to make it modifiable. */
        entity_node = drop_const_on_init_entity_node(entity_node);
      }  /* if */
      if (modifiers->curr_elem == 0) {
        /* Use *x rather than x[0] if the subscript is zero. */
        entity_node = add_indirection_to_node(entity_node);
      } else {
        /* Add the subscript if it's non-zero. */
        elem_num_node = node_for_host_large_integer(
             (a_host_large_integer)modifiers->curr_elem, targ_size_t_int_kind);
        entity_node->next = elem_num_node;
        entity_node = make_lvalue_operator_node(
                                          (an_expr_operator_kind)eok_subscript,
                                          type_pointed_to(entity_node->type),
                                          entity_node);
      }  /* if */
    }  /* if */
  }  /* if */
  return entity_node;
}  /* modify_init_entity_node */


static an_expr_node_ptr make_init_entity_node(
                                        an_init_pos_descr_ptr ipdp,
                                        a_boolean             result_is_lvalue,
                                        a_boolean             using_as_dest)
/*
Make an expression for the entity described by ipdp, and return
a pointer to it.  If result_is_lvalue is TRUE, the expression will
be used as an lvalue by the caller.  If using_as_dest is TRUE, the entity
is the destination of an initialization operation.
*/
{
  an_expr_node_ptr entity_node;
  a_variable_ptr   var = ipdp->variable;
  a_boolean        is_vla = FALSE;

  /* Make a node for the base entity. */
#if !DO_FULL_PORTABLE_EH_LOWERING
  if (ipdp->thrown_object_address) {
    /* The address is the address in the runtime to which a thrown object
       should be copied. */
    entity_node = make_thrown_object_address_node();
    entity_node = add_cast_if_necessary(entity_node,
                                        make_pointer_type(ipdp->base_type));
    check_assertion(!is_const_qualified_type(ipdp->base_type));
    if (result_is_lvalue) {
      entity_node = add_indirection_to_node(entity_node);
    }  /* if */
  } else
#endif /* !DO_FULL_PORTABLE_EH_LOWERING */
  /* Do not insert code here; this is the else of the above if. */
  {
    check_assertion(var != NULL);
    if (ipdp->indirect_through_variable) {
      /* Indirect through the variable. */
      entity_node = add_indirection_to_node(var_rvalue_expr(var));
    } else {
      /* Normal case, a simple variable. */
      entity_node = var_lvalue_expr(var);
#if LOWER_VARIABLE_LENGTH_ARRAYS
      /* When VLAs are lowered, the array variable becomes a pointer to the
         allocated space. */
      if (var->is_vla) {
        lower_vla_variable_lvalue(entity_node);
        is_vla = TRUE;
      }  /* if */
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
    }  /* if */
    if (using_as_dest) {
      /* The entity will be used as the destination of an initialization, so
         drop "const" (if present) from the type to make it modifiable. */
      entity_node = drop_const_on_init_entity_node(entity_node);
    }  /* if */
    /* Add the modifiers to the base entity address. */
    if (ipdp->base_of_complete_object) {
      /* A base class of a complete object can be addressed more
         efficiently. */
      check_assertion(ipdp->modifiers != NULL &&
                      ipdp->modifiers->curr_base != NULL);
#if GNU_VECTOR_TYPES_ALLOWED
      check_assertion(!ipdp->modifiers->is_vector_element);
#endif /* GNU_VECTOR_TYPES_ALLOWED */
      entity_node = make_base_class_lvalue(entity_node,
                                           ipdp->modifiers->curr_base,
                                           /*complete_object=*/TRUE);
    } else {
      /* Normal case. */
      entity_node = modify_init_entity_node(entity_node, ipdp->modifiers,
                                            using_as_dest, is_vla);
    }  /* if */
    if (ipdp->array_element_sequence) {
      /* For an array element sequence that covers more than one dimension
         of an array, get the type right for the underlying element. */
      entity_node = add_cast_to_glvalue_if_necessary(entity_node,
                                                     ipdp->array_element_type);
    }  /* if */
    check_assertion(entity_node->is_lvalue);
    if (!result_is_lvalue) {
      /* Convert this to an rvalue. */
      entity_node = rvalue_expr_for_lvalue(entity_node);
    }  /* if */
  }  /* if */
  return entity_node;
}  /* make_init_entity_node */


an_expr_node_ptr make_address_of_init_entity_node(
                                           an_init_pos_descr_ptr ipdp,
                                           a_boolean             using_as_dest)
/*
Make an rvalue pointer expression for the entity described by ipdp, and return
a pointer to it.  If using_as_dest is TRUE, the entity is the destination
of an initialization operation.
*/
{
  an_expr_node_ptr node;

  node = make_init_entity_node(ipdp, /*result_is_lvalue=*/TRUE, using_as_dest);
  node = add_address_of_to_node(node);
  return node;
}  /* make_address_of_init_entity_node */


static void make_lowered_zero_of_proper_type(a_type_ptr desired_type,
                                             a_constant *zero_constant)
/*
Make a zero constant of type desired_type (a scalar type) and put it in
*zero_constant.  No IL allocation is done.  This routine is also handy
for making NULL pointer constants.  This wrapper over
make_zero_of_proper_type handles returning a -1 constant for a
NULL pointer-to-data member in the IA-64 ABI.  Note that the type of the
constant that is returned may be a lowered version of desired_type (and not
desired_type itself).
*/
{
  make_zero_of_proper_type(desired_type, zero_constant);
  if (is_or_was_ptr_to_data_member_type(desired_type)) {
    if (zero_constant->kind == (a_constant_repr_kind)ck_ptr_to_member) {
      /* An un-lowered pointer-to-data-member type; generate a lowered
         constant of the appropriate type. */
      lower_ptr_to_member_constant(zero_constant);
    } else {
      /* Type has already been lowered; select the appropriate constant
         value to represent a NULL pointer-to-data-member. */
      check_assertion(zero_constant->kind == (a_constant_repr_kind)ck_integer);
      set_integer_constant(zero_constant,
#if IA64_ABI
                           (a_host_large_integer)-1,
#else /* !IA64_ABI */
                           (a_host_large_integer)0,
#endif /* IA64_ABI */
                           (an_integer_kind)ik_int);
    }  /* if */
#if LOWER_COMPLEX
  } else if (is_complex_type(desired_type)) {
    /* Make sure a zero complex constant is lowered (note that the
       type of the constant is also lowered here). */
    lower_constant(zero_constant);
#endif /* LOWER_COMPLEX */
  }  /* if */
}  /* make_lowered_zero_of_proper_type */


static void add_init_assignment(a_dynamic_init_ptr     dip,
                                a_constant_ptr         con,
                                an_expr_node_ptr       entity_node,
                                a_boolean              have_complete_object,
                                an_insert_location_ptr insert_location,
                                a_boolean              is_lambda_capture,
                                an_init_pos_descr_ptr  ipdp)
/*
Make an assignment statement (or call statement when
ipdp->array_element_sequence is TRUE) to implement the dynamic initialization
described by dip.  If dip is NULL, con indicates the constant value of the
initializer.  entity_node is an lvalue expression of the entity to be
initialized.  have_complete_object is TRUE if the entity being initialized is a
complete object; FALSE means a base class subobject.  Insert the statement at
*insert_location and update *insert_location.  The assignment is being
performed as part of a lambda capture operation if is_lambda_capture is TRUE.
The constant or expression initial value pointed to by dip or con is already
lowered.  ipdp describes the destination for the initialization and is used
in the case where an array is being initialized by a repeated constant
initialization (when ipdp->array_element_sequence is TRUE).
*/
{
  an_expr_node_ptr      init_val_node, assign_node, num_elem_node = NULL;
  a_statement_ptr       assign_stmt;
  an_expr_operator_kind op;
  a_boolean             array_assignment = FALSE, needs_cast = FALSE;
  a_type_ptr            entity_type = entity_node->type;

  check_assertion(entity_node->is_lvalue);
  switch ((dip == NULL) ? (a_dynamic_init_kind)dik_constant : dip->kind) {
    case dik_zero:
      /* Set the entity to zero (default initialization). */
      { a_constant     zero_constant;
        make_lowered_zero_of_proper_type(entity_type, &zero_constant);
        init_val_node = alloc_node_for_constant(&zero_constant);
      }
      break;
    case dik_constant:
      /* Assign a constant to the entity to be initialized. */
      /* The constant has already been lowered. */
      if (dip != NULL) con = dip->variant.constant;
      if (con->kind == (a_constant_repr_kind)ck_string &&
          !con->implicit_cast) {
        /* An character array initialized by a string literal, e.g., in
           a ctor-initializer.  Create an lvalue string constant. */
        init_val_node = alloc_node_for_constant(con);
        init_val_node->is_lvalue = TRUE;
        if (string_literals_are_const) {
          /* The ck_string constant has been lowered, removing the original
             const qualifier.  Add an lvalue cast to restore the original
             type. */
          a_type_ptr new_type = alloc_type((a_type_kind)tk_array);
          copy_type(con->type, new_type);
          new_type->variant.array.element_type = make_qualified_type(
                                                 array_element_type(con->type),
                                                 TQ_CONST);
          init_val_node = add_cast_to_glvalue_if_necessary(init_val_node,
                                                           new_type);
        }  /* if */
        array_assignment = TRUE;
      } else {
        /* Normal case, not a string literal. */
        init_val_node = make_node_for_il_constant(con);
        if (init_val_node->is_lvalue) {
          /* We're assigning from one array to another; use eok_bassign. */
          check_assertion(is_array_type(con->type));
          array_assignment = TRUE;
        }  /* if */
      }  /* if */
      break;
    case dik_expression:
      /* Assign an expression to the entity to be initialized. */
      /* The expression has already been lowered. */
      init_val_node = dip->variant.expression;
      break;
    default:
      unexpected_condition_str("add_init_assignment: bad kind");
  }  /* switch */
  if (array_assignment &&
      skip_typerefs(init_val_node->type)->
                              variant.array.variant.number_of_elements == 0) {
    /* Don't bother to create an assignment from an array with zero elements.
       These come up in cases like "new int[0]{};". */
  } else if (ipdp->array_element_sequence) {
    /* We're assigning a constant value to an entire array or some portion
       thereof. */
    check_assertion(dip == NULL ||
                    dip->kind == (a_dynamic_init_kind)dik_constant);
    /* Use (or create) the temporary variable associated with this constant. */
    init_val_node = add_address_of_to_node(var_lvalue_expr(
                                                 assoc_var_for_constant(con)));
    if (ipdp->array_element_count == 0) {
      num_elem_node = num_elem_node_if_array(ipdp);
    }  /* if */
    /* Create a helper routine to do the initialization and call it. */
    insert_call_to_initialize_entity(type_from_init_pos_descr(ipdp),
                                     have_complete_object,
                                     add_address_of_to_node(entity_node),
                                     num_elem_node,
                                     ipdp->array_element_count,
                                     init_val_node,
                                     insert_location);
  } else {
    /* Make an assignment statement.  Note that we know that no constructor
       (copy or other) is involved because we have this kind of dynamic
       initialization. */
    op = (an_expr_operator_kind)(array_assignment ? eok_bassign : eok_assign);
    if (needs_cast_because_type_has_param_passed_via_cctor(entity_type)) {
      /* If the entity being assigned has a type that contains a
         function with a parameter that is passed via a copy constructor,
         we need to add a cast to the destination type to avoid a type
         mismatch. */
      needs_cast = TRUE;
    } else if (is_lambda_capture &&
               is_ptr_or_ref_type(init_val_node->type) &&
               is_incomplete_type(type_pointed_to(init_val_node->type))) {
      /* Handle a case like:
           int x[] = {37, 47, [&x]{return x[0] + x[1];}()};
         where a lambda capture's type is incomplete at the time of the
         capture.  Add an explicit cast to the incomplete type (since the
         source type, while incomplete now, will be complete in the generated
         C code). */
      needs_cast = TRUE;
    }  /* if */
    if (needs_cast) {
      /* If we need a cast, make sure we don't change the lvalueness. */
      if (init_val_node->is_lvalue) {
        init_val_node = add_cast_to_glvalue(init_val_node, entity_type);
      } else {
        init_val_node = add_cast(init_val_node, entity_type);
      }  /* if  */
    }  /* if */
    assign_node = make_assignment_expr_with_subobject_fix(entity_node,
                                                          have_complete_object,
                                                          op,
                                                          init_val_node);
    assign_stmt = insert_expr_statement(assign_node, insert_location);
    set_stmt_pos_to_code_pos_for_lowering(assign_stmt);
  }  /* if */
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
  class_type = parent_class_of(curr_routine);
  if (class_type->variant.class_struct_union.any_virtual_base_classes) {
    for (bcp = class_type_supp(class_type)->base_classes;
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
  if (ctor_needs_vtt_argument(curr_routine)) {
    /* Skip over the VTT parameter. */
    source_param_var = source_param_var->next;
    check_assertion_str(source_param_var != NULL,
                  "var_for_copy_constructor_source: source param missing (3)");
  }  /* if */
#endif /* IA64_ABI */
  return source_param_var;
}  /* var_for_copy_constructor_source */


void clear_implied_copy_source(an_implied_copy_source *source_desc)
/*
Clear the fields of the specified implied copy source.
*/
{
  source_desc->ctor_init = NULL;
  source_desc->capture = NULL;
  source_desc->runtime_throw = FALSE;
}  /* clear_implied_copy_source */


static void advance_to_next_lambda_capture_if_necessary(
                                           an_implied_copy_source *source_desc)
/*
If the source description refers to a lambda capture, advance to the
next local variable in the capture list.  source_desc can be NULL.
*/
{
  if (source_desc != NULL && source_desc->capture != NULL) {
    check_assertion(source_desc->ctor_init == NULL && 
                    !source_desc->runtime_throw);
    source_desc->capture = source_desc->capture->next;
  }  /* if */
}  /* advance_to_next_lambda_capture_if_necessary */


static an_expr_node_ptr implied_source_of_copy(
                                       an_implied_copy_source *source_desc,
                                       an_init_pos_descr_ptr  dest,
                                       a_boolean              result_is_lvalue)
/*
We're processing a dynamic initialization entry that represents a copy of
something from an implied source location to the thing being initialized.
source_desc describes the source of the implied copy (e.g., a
constructor-initializer entry).  Create an expression to describe the implied
source and return a pointer to it.  dest describes the entity being
initialized.  The returned expression will be an lvalue if result_is_lvalue is
TRUE, otherwise an rvalue.  result_is_lvalue must be TRUE when the implied
source is represented by the source_expr of a constructor-initializer (i.e.,
for an array initialization in GNU C++ mode).
*/
{
  an_init_pos_descr    source_ipd;
  an_init_pos_modifier source_ipm;
  an_expr_node_ptr     source_node;

  check_assertion(source_desc != NULL);
  if (source_desc->ctor_init != NULL) {
    check_assertion(source_desc->capture == NULL &&
                    !source_desc->runtime_throw);
    if (source_desc->ctor_init->source_expr == NULL) {
      /* The implied source is the member being copied by the
         ctor-initializer. */
      set_var_indirect_init_pos_descr(var_for_copy_constructor_source(),
                                      &source_ipd);
      modify_ctor_init_pos_descr(source_desc->ctor_init, &source_ipd,
                                 &source_ipm);
      source_node = make_init_entity_node(&source_ipd, result_is_lvalue,
                                          /*using_as_dest=*/FALSE);
    } else {
      /* The implied source is given by the specified expression (which has
         already been lowered).  Currently used only when copying an
         array in GNU C++ mode.  Result must be an lvalue. */
      source_node = source_desc->ctor_init->source_expr;
      check_assertion(result_is_lvalue && source_node->is_lvalue);
    }  /* if */
  } else if (source_desc->capture != NULL) {
    a_boolean needs_indirection = FALSE;
    a_boolean source_node_result_is_lvalue = result_is_lvalue;
    check_assertion(!source_desc->runtime_throw);
    if (source_desc->capture->source_closure_field == NULL) {
      a_variable_ptr  var = source_desc->capture->variable;
      /* The implied source is a local variable from a lambda capture. */
      if (is_reference_type(var->type) ||
          (var->is_parameter &&
           var->assoc_param_type != NULL &&
           var->assoc_param_type->passed_via_copy_constructor)) {
        /* Variable is a reference or a parameter passed via copy constructor,
           add an indirection. */
        set_var_indirect_init_pos_descr(var, &source_ipd);
      } else {
        set_var_init_pos_descr(var, &source_ipd);
      }  /* if */
    } else {
      /* The source of the implied copy is a variable that has been captured by
         an intervening enclosing lambda; use the corresponding field of
         that lambda's closure class to access it. */
      check_assertion(innermost_function_scope != NULL &&
        innermost_function_scope->variant.routine.this_param_variable != NULL);
      set_var_indirect_init_pos_descr(
                 innermost_function_scope->variant.routine.this_param_variable,
                 &source_ipd);
      add_init_pos_modifier(&source_ipm, &source_ipd);
      source_ipm.curr_field = source_desc->capture->source_closure_field;
      source_ipm.type = source_desc->capture->source_closure_field->type;
      if (is_reference_type(source_ipm.type)) {
        /* In the reference case, we need to add an additional indirection
           on top of the source description, but we don't have an appropriate
           modifier, so set a flag and add the indirection after converting
           to an expression. */
        needs_indirection = TRUE;
        source_node_result_is_lvalue = FALSE;
      }  /* if */
    }  /* if */
    source_node = make_init_entity_node(&source_ipd,
                                        source_node_result_is_lvalue,
                                        /*using_as_dest=*/FALSE);
    if (needs_indirection) {
      /* Add an indirection for the reference case of a variable captured
         by an intervening enclosing lambda. */
      source_node = add_indirection_to_node(source_node);
      if (!result_is_lvalue) {
        source_node = rvalue_expr_for_lvalue(source_node);
      }  /* if */
    }  /* if */
    /* Note that advance_to_next_lambda_capture_if_necessary should be
       called to move from the current lambda capture variable to the next.
       implied_source_of_copy may be called multiple times for the same
       capture variable (e.g., by add_bitwise_copy), thus the need for
       an explicit advancement. */
  } else if (source_desc->runtime_throw) {
    /* The implied source is a thrown object. */
    a_variable_ptr catch_parameter;
    a_type_ptr     param_type, object_type;

    /* We expect a simple catch parameter as the destination. */
    check_assertion(dest->modifiers == NULL &&
                    !dest->indirect_through_variable);
    catch_parameter = dest->variable;
    param_type = catch_parameter->type;
    /* Get an rvalue pointer to the caught object. */
    source_node = make_caught_object_address_node();
    object_type = param_type;
    if (is_reference_type(param_type)) {
      check_assertion(!result_is_lvalue);
      /* When the catch parameter has reference type, the caught object
         has the underlying type, not the reference type.  (When you catch
         a reference-to-A, the thrown object has type A.) */
      object_type = type_pointed_to(param_type);
    }  /* if */
    /* Cast the source node to a pointer to the type of the object being
       copied. */
    source_node = add_cast_if_necessary(source_node,
                                        make_pointer_type(object_type));
    if (!is_reference_type(param_type)) {
      /* For the reference-catch case, we copy the address into the reference,
         so no extra indirection is wanted.  Change back to an rvalue if
         the caller requested such. */
      source_node = add_indirection_to_node(source_node);
      if (!result_is_lvalue) {
        source_node = rvalue_expr_for_lvalue(source_node);
      }  /* if */
    }  /* if */
  } else {
    unexpected_condition();
  }  /* if */
  check_assertion(source_node->is_lvalue == result_is_lvalue);
  return source_node;
}  /* implied_source_of_copy */


static void add_bitwise_copy(an_init_pos_descr_ptr  dest,
                             an_expr_node_ptr       source_node,
                             a_boolean              have_complete_object,
                             an_insert_location_ptr insert_location)
/*
Generate code to implement an initialization by bitwise copy.  dest describes
the destination of the move.  source_node describes the implied source of the
bitwise copy (e.g., a constructor initialization entry).  have_complete_object
is TRUE if we are copying a complete object, FALSE if we are copying a base
class subobject.  If dest->array_element_sequence is TRUE, the initialization
is for a sequence of array elements (but only an entire array initialization
is currently handled here).  Insert the statement at *insert_location and
update *insert_location.
*/
{
  an_expr_node_ptr      dest_node, assign_node;
  a_type_ptr            type;
  an_expr_operator_kind op;

  type = source_node->type;
  if (!have_complete_object &&
      is_class_struct_union_type(type) &&
      skip_typerefs(type)->variant.class_struct_union.is_empty_class) {
    /* Do not put out code to copy an empty base class. */
  } else {
    if (is_reference_type(type)) {
      /* Replace a reference type by a pointer type. */
      type = make_pointer_type(type_pointed_to(type));
    }  /* if */
    /* Make an assignment statement.  For lvalue copies, use a block copy. */
    if (!source_node->is_lvalue) {
      op = (an_expr_operator_kind)eok_assign;
    } else {
      op = (an_expr_operator_kind)eok_bassign;
    }  /* if */
    /* Make an expression for the destination entity. */
    if (dest->array_element_sequence) {
      /* The caller has specified a sequence of array elements to be
         initialized via bitwise copy.  Handle only the case where an entire
         array is being initialized via bitwise copy (the front end doesn't
         currently generate IL to partially initialize an implied source
         array via bitwise copy). */
      an_init_pos_modifier *ipmp = dest->modifiers;
      check_assertion(is_array_type(source_node->type) &&
                      num_array_elements(source_node->type) ==
                                     (a_targ_size_t)dest->array_element_count);
      /* In preparation for an array element copy, an array modifier has
         already been added to the destination; remove it now so that the
         assignment generated below is an array-to-array assignment. */
      check_assertion(ipmp != NULL &&
                      ipmp->curr_base == NULL &&
                      ipmp->curr_field == NULL);
      dest->modifiers = ipmp->next;
      ipmp->next = NULL;
      free_init_pos_modifier_list(ipmp);
      /* Destination is no longer an array element sequence. */
      dest->array_element_sequence = FALSE;
    }  /* if */
    dest_node = make_init_entity_node(dest, /*result_is_lvalue=*/TRUE,
                                      /*using_as_dest=*/TRUE);
    assign_node = make_assignment_expr_with_subobject_fix(dest_node,
                                                          have_complete_object,
                                                          op,
                                                          source_node);
    (void)insert_expr_statement(assign_node, insert_location);
  }  /* if */
}  /* add_bitwise_copy */


#if IA64_ABI
/*ARGSUSED*/ /* is_target_ctor is unused in that case. */
#endif /* IA64_ABI */
void make_ctor_implied_arg_list(a_routine_ptr    ctor_routine,
                                a_boolean        is_target_ctor,
                                an_expr_node_ptr *implied_arg_list,
                                an_expr_node_ptr *end_implied_arg_list)
/*
Build and return a list of the implied arguments to be added to a call of the
constructor ctor_routine.  If is_target_ctor is TRUE, the ctor_routine is
the target constructor of a delegating constructor (and the implied arguments,
if any, created for the call -- in the Cfront ABI -- are passed through
from the parameters of innermost_function_scope).  The beginning and end of the
list are returned in *implied_arg_list and *end_implied_arg_list.  For an empty
list, both will be set to NULL.
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

  *implied_arg_list = *end_implied_arg_list = NULL;
  /* Get the class type. */
  class_type = parent_class_of(ctor_routine);
  prelower_class_type(class_type);
  if (ctor_needs_implied_arg_list(ctor_routine)) {
#if !IA64_ABI
    a_type_ptr       subobject_type;
    a_base_class_ptr bcp;
    a_variable_ptr   delegated_params = NULL;
    /* The class has at least one virtual base class. */
    check_assertion(innermost_function_scope != NULL);
    if (is_target_ctor) {
      check_assertion(innermost_function_scope->variant.routine.ptr->
                                                           is_delegating_ctor);
      /* A delegating constructor is being lowered and this is the call to
         the target constructor.  Make an implied argument list that passes
         the parameter values from the delegating constructor to the target
         constructor.  For example:
           delegating(this, vbptr1, vbptr2, ...) {
             target(this, vbptr1, vbrpt2, ...);
             ...
           }
         */
      delegated_params =
                    innermost_function_scope->variant.routine.parameters->next;
    }  /* if */
    for (bcp = class_type->variant.class_struct_union.extra_info->base_classes;
         bcp != NULL;
         bcp = bcp->next) {
      if (bcp->is_virtual) {
        /* Allocate an expression that is a NULL pointer to the virtual
           base class.  Use the type of the base class when used as a
           subobject. */
        subobject_type = bcp->type->variant.class_struct_union.extra_info->
                                                             type_as_subobject;
        if (delegated_params == NULL) {
          /* Use zero for the value of the implied argument. */
          make_zero_of_proper_type(make_pointer_type(subobject_type),
                                   &null_constant);
          implied_arg_node = alloc_node_for_constant(&null_constant);
        } else {
          /* Use the corresponding parameter from the delegating constructor
             as the value for the implied argument (i.e., pass the argument
             through unchanged). */
          implied_arg_node = var_rvalue_expr(delegated_params);
          delegated_params = delegated_params->next;
        }  /* if */
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

#if !IA64_ABI

static an_expr_node_ptr dtor_control_argument(a_boolean have_complete_object,
                                              a_boolean free_storage)
/*
Build a node passed to a destructor to control the kind of processing
done.  have_complete_object is TRUE if the destruction is of a
complete object.  free_storage is TRUE if the destructor should
free the object's storage.
*/
{
  an_expr_node_ptr node;

  /* 0x2 bit means "have complete object".  0x1 bit means "free storage". */
  node = node_for_integer_constant(
                    (have_complete_object? 2L : 0L) | (free_storage ? 1L : 0L),
                    (an_integer_kind)ik_int);
  return node;
}  /* dtor_control_argument */

#endif /* !IA64_ABI */

void make_dtor_implied_arg_list(a_routine_ptr    dtor_routine,
                                a_boolean        have_complete_object,
                                an_expr_node_ptr *implied_arg_list,
                                an_expr_node_ptr *end_implied_arg_list)
/*
Build and return the implied argument to be added to a call of the destructor
dtor_routine.  The beginning and end of the list are returned in
*implied_arg_list and *end_implied_arg_list.  For an empty list, both will be
set to NULL.  If have_complete_object is TRUE, we know we are calling the
destructor for a complete object (that must be TRUE for the IA-64 ABI).
*/
{
  a_type_ptr       class_type;
  an_expr_node_ptr implied_arg_node;

  *implied_arg_list = *end_implied_arg_list = NULL;
  /* Get the class type. */
  class_type = parent_class_of(dtor_routine);
  prelower_class_type(class_type);
#if !IA64_ABI
  /* The first argument indicates whether we have a complete object. */
  implied_arg_node = dtor_control_argument(have_complete_object,
                                           /*free_storage=*/FALSE);
  *implied_arg_list = implied_arg_node;
  *end_implied_arg_list = implied_arg_node;
#else /* IA64_ABI */
  check_assertion(have_complete_object);
  if (dtor_needs_vtt_argument(dtor_routine)) {
    /* Add a NULL VTT argument. */
    a_constant null_constant;
    make_zero_of_proper_type(make_virtual_table_table_pointer_type(),
                             &null_constant);
    implied_arg_node = alloc_node_for_constant(&null_constant);
    *implied_arg_list = implied_arg_node;
    *end_implied_arg_list = implied_arg_node;
  }  /* if */
#endif /* IA64_ABI */
}  /* make_dtor_implied_arg_list */


a_boolean need_zeroing_for_value_initialization(a_dynamic_init_ptr dip)
/*
Return TRUE if the dik_constructor initialization in the indicated
dynamic initialization is value-initialization that requires zeroing
of the storage before the constructor is called.
*/
{
  a_boolean     need_zeroing = FALSE;
  a_routine_ptr ctor_routine = dip->variant.constructor.ptr;

  /* Zeroing is required if the initialization is value-initialization
     for a class that has no user-provided constructor, and the class
     has data members that require zero initialization. */
#if IA64_ABI
  /* If this is an alternate entry point in the IA-64 ABI, go to the
     primary routine.  The alternate entry point is always marked
     as compiler generated. */
  if (ctor_routine->primary_ctor_or_dtor != NULL) {
    ctor_routine = ctor_routine->primary_ctor_or_dtor;
  }  /* if */
#endif /* IA64_ABI */
  if (dip->variant.constructor.value_initialization &&
      !special_member_is_user_provided(ctor_routine) &&
      parent_class_of(ctor_routine)->
                          variant.class_struct_union.has_zero_init_component) {
    need_zeroing = TRUE;
  }  /* if */
  return need_zeroing;
}  /* need_zeroing_for_value_initialization */


#if !ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
/*ARGSUSED*/  /* <-- construction_vtbls_var and ipdp are not used in
                     that case. */
#endif /* !ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
static void add_constructor_call(a_dynamic_init_ptr     dip,
                                 an_expr_node_ptr       entity_node,
                                 an_expr_node_ptr       source_node,
                                 a_boolean              have_complete_object,
                                 an_init_pos_descr_ptr  ipdp,
                                 a_constructor_init_ptr ctor_init,
                                 a_variable_ptr         construction_vtbls_var,
                                 an_insert_location_ptr insert_location)
/*
Make a call statement that invokes a constructor as required in the dynamic
initialization entry pointed to by dip.  entity_node is an rvalue expression
that gives the address of the entity to be initialized.  If source_node
is non-NULL, it points to an expression that is the source for a copy
constructor call.  Both entity_node and source_node have already been
cast to the proper type for the corresponding parameter to eliminate
qualifier and type-as-subobject differences.  have_complete_object 
is TRUE if we are constructing a complete object, FALSE for a base
class subobject.  ipdp describes how to address the entity to be
initialized (it is used only in some modes, for base classes).
If this constructor call is for a ctor-initializer, ctor_init points
to it; otherwise, it is NULL.  construction_vtbls_var points to a
variable for an array of construction vtables, if needed, and NULL
otherwise.  Insert the statement at *insert_location and update
*insert_location.  The additional-arguments list given by
dip->variant.constructor.args has already been lowered.
*/
{
  a_routine_ptr    ctor_routine = dip->variant.constructor.ptr;
  an_expr_node_ptr last_node, return_value = NULL;
  an_expr_node_ptr implied_arg_node;
  an_expr_node_ptr implied_arg_list = NULL, end_implied_arg_list = NULL;
#if IA64_ABI
  a_ctor_or_dtor_kind
                   kind = (a_ctor_or_dtor_kind)cdk_complete;
#endif /* IA64_ABI */
  a_boolean        is_target_ctor_call = FALSE;

#if CHECKING
  if (dip->kind != (a_dynamic_init_kind)dik_constructor) {
    internal_error("add_constructor_call: bad kind");
  }  /* if */
#endif /* CHECKING */
  check_assertion(!entity_node->is_lvalue &&
                  is_pointer_type(entity_node->type));
  if (ctor_init != NULL &&
      ctor_init->kind == (a_constructor_init_kind)cik_delegation) {
    /* The routine being called is a target constructor (and is being called
       from a delegating constructor).  Special treatment is needed in
       this case. */
    is_target_ctor_call = TRUE;
  }  /* if */
  if (need_zeroing_for_value_initialization(dip)) {
    /* To do value-initialization on a class without a user-written
       constructor, zero the object and then call the default constructor. */
    an_expr_node_ptr entity_node_copy =
                                make_reusable_copy(entity_node,
                                                   /*vars_can_change=*/FALSE);
    a_type_ptr       class_type = parent_class_of(ctor_routine);
    insert_call_to_zero_entity(class_type,
                               have_complete_object,
                               entity_node,
                               (an_expr_node_ptr)NULL,
                               (a_targ_size_t)0,
                               insert_location);
    entity_node = entity_node_copy;
  }  /* if */
  if (ctor_init != NULL &&
      (ctor_init->kind == (a_constructor_init_kind)cik_virtual_base_class ||
       ctor_init->kind == (a_constructor_init_kind)cik_direct_base_class)) {
    /* Initializing a base class. */
    a_base_class_ptr base_class = ctor_init->variant.base_class;
    if (dip->kind == (a_dynamic_init_kind)dik_constructor) {
      /* A base class initialized by a constructor call. */
#if !IA64_ABI
      a_type_ptr       base_class_type = base_class->type;
      a_variable_ptr   param_var;
      a_base_class_ptr bcp;
      /* Build a list of implicit virtual base class pointer arguments.
         The required entries are expressions providing the value of the
         associated virtual base class pointer parameter for each virtual
         base class of the base class. */
      for (bcp = base_class_type->variant.class_struct_union.extra_info->
                                                                  base_classes;
           bcp != NULL;
           bcp = bcp->next) {
        if (bcp->is_virtual) {
          /* Find the implicit virtual base parameter under the main class
             that is for this same virtual base class. */
          a_variable_ptr this_param_var =
                          innermost_function_scope->variant.routine.parameters;
          param_var = implicit_virtual_base_parameter(
                                                     base_class->derived_class,
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
      /* Use the subobject entry point. */
      kind = (a_ctor_or_dtor_kind)cdk_subobject;
#endif /* IA64_ABI */
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
      /* Set up for passing an array of virtual function table pointers
         to use during the subobject construction, if one is necessary. */
      build_construction_vtbls_pointer_for_subobject_construction(
                                                      dip,
                                                      base_class,
                                                      ipdp,
                                                      construction_vtbls_var,
                                                      insert_location,
                                                      &implied_arg_node,
                                                      (a_boolean *)NULL);
#if IA64_ABI
      /* The VTT pointer gets passed as an implied argument. */
      implied_arg_list = end_implied_arg_list = implied_arg_node;
#endif /* IA64_ABI */
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
    }  /* if */
  }  /* if */
#if !IA64_ABI
  /* If no implied_arg_list is supplied and the constructor needs one
     (because it initializes a class that has virtual base classes), make
     the implied_arg_list (all entries are NULL pointer values). */
  if (implied_arg_list == NULL) {
    if (is_target_ctor_call) {
      /* This ctor_init is for a delegating constructor's call to a
         target constructor. */
#if NEW_CAN_BE_FOLDED_INTO_CTOR
      /* If the "new" operation is folded into the target constructor,
         use the value returned from that constructor as the value for
         "this" in the designated constructor, i.e., create
            this = target(this, ...);
         */
      a_variable_ptr this_param =
                          innermost_function_scope->variant.routine.parameters;
      return_value = var_lvalue_expr(this_param);
      this_param->param_value_has_been_changed = TRUE;
#if MINIMAL_INLINING
      this_param->param_used_as_lvalue = TRUE;
#endif /* MINIMAL_INLINING */
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
    }  /* if */
    make_ctor_implied_arg_list(ctor_routine, is_target_ctor_call,
                               &implied_arg_list, &end_implied_arg_list);
  }  /* if */
#endif /* !IA64_ABI */
#if IA64_ABI
  if (is_target_ctor_call &&
      innermost_function_scope->variant.routine.ptr->ctor_dtor_kind ==
                                         (a_ctor_or_dtor_kind)cdk_delegation) {
    /* The "common" cdk_delegation constructor is invoked by both subobject
       and complete alternate entry points and must invoke either the
       target complete object constructor or the target subobject constructor
       (depending on whether the delegating complete object constructor or
       delegating subobject constructor invoked the cdk_delegation
       constructor respectively).  To accomplish this, add an "if" statement
       which checks for a NULL VTT argument being passed in, and call the
       appropriate target constructor at run-time.

       Note that the arguments have already been lowered, but any side-effects
       that they have are duplicated in the two branches of the "if" below.

       Add this lowered code:

         if (vtt_param) {
           target_subobject(this, vtt_param[, ...]);
         } else {
           target_complete_object(this[, ...]);
         }

       If exception handling cleanup is required for the initialization,
       a special cdk_delegation destructor is created to effectively
       perform a similar destruction -- it invokes either the complete or
       subobject destructor as determined by a NULL VTT parameter.
    */
    an_expr_node_ptr    test_node, arg, arg_list, arg_copy, so_arg_list;
    an_insert_location  then_insert_location, else_insert_location;
    a_variable_ptr      this_param, vtt_param;

    /* Create a node to test whether the VTT parameter is NULL. */
    this_param = innermost_function_scope->variant.routine.parameters;
    check_assertion(this_param != NULL && this_param->next != NULL);
    vtt_param = this_param->next;
    check_assertion(vtt_param != NULL &&
                    f_identical_types(vtt_param->type,
                                      make_virtual_table_table_pointer_type(),
                                      ITF_IL_IDENTICAL));
    /* Create the argument list for the complete object constructor. */
    arg_list = entity_node;
    arg_list->next = dip->variant.constructor.args;
    /* Copy the complete object argument list and add the VTT parameter for
       the subobject constructor. */
    so_arg_list = copy_expr_tree(arg_list, CE_NO_OPTIONS);
    so_arg_list->next = var_rvalue_expr(vtt_param);
    arg_copy = so_arg_list->next;
    for (arg = arg_list->next; arg != NULL; arg = arg->next) {
      arg_copy->next = copy_expr_tree(arg, CE_NO_OPTIONS);
      arg_copy = arg_copy->next;
    }  /* for */
    /* Create the "if" statement. */
    test_node = var_rvalue_expr(vtt_param);
    test_node = boolean_controlling_expr(test_node);
    insert_if_statement(test_node, /*is_initialization_guard=*/FALSE,
                        insert_location, (a_statement_ptr *)NULL,
                        &then_insert_location, &else_insert_location);
    /* Call the target subobject constructor in the "then" clause. */
    make_call_statement(alternate_entry_point(dip->variant.constructor.ptr,
                                            (a_ctor_or_dtor_kind)cdk_subobject,
                                            /*define_now=*/FALSE),
                        so_arg_list, return_value, &then_insert_location);
    /* Call the target complete object constructor in the "else" clause. */
    make_call_statement(alternate_entry_point(dip->variant.constructor.ptr,
                                             (a_ctor_or_dtor_kind)cdk_complete,
                                             /*define_now=*/FALSE),
                        arg_list, return_value, &else_insert_location);
    /* Note that dip->variant.constructor.ptr is unchanged in this case;
       it can't be set to either alternate entry point, so it's left pointing
       to the primary (cdk_delegation) routine. */
  } else
#endif /* IA64_ABI */
  /* Do not insert code here. */
  {
#if IA64_ABI
    if (is_target_ctor_call) {
      /* Invoke the corresponding target constructor kind for this delegating
         constructor. */
      kind = innermost_function_scope->variant.routine.ptr->ctor_dtor_kind;
      if (kind == (a_ctor_or_dtor_kind)cdk_subobject) {
        /* The VTT parameter gets passed as an implied argument. */
        implied_arg_list = end_implied_arg_list = var_rvalue_expr(
                   innermost_function_scope->variant.routine.parameters->next);
      }  /* if */
    }  /* if */
    /* Use the proper entry point (complete or subobject). */
    ctor_routine = dip->variant.constructor.ptr =
                                   alternate_entry_point(ctor_routine,
                                                         kind,
                                                         /*define_now=*/FALSE);
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
    /* Make and insert an expression statement containing the call
       expression. */
    make_call_statement(ctor_routine, entity_node, return_value,
                        insert_location);
  }  /* if */
  if (is_target_ctor_call && exceptions_enabled &&
#if IA64_ABI
      innermost_function_scope->variant.routine.ptr->ctor_dtor_kind ==
                                         (a_ctor_or_dtor_kind)cdk_delegation &&
#else /* !IA64_ABI */
      implied_arg_list != NULL &&
#endif /* IA64_ABI */
      ctor_init->initializer->destructible_entity_descr != NULL) {
    /* This delegating constructor initialization requires a corresponding
       destruction (and the class has virtual base classes).  Let the
       normal exception handling mechanism create region table entries as
       appropriate, but use a special "delegation" destructor to perform the
       destruction (because it's not known until run-time whether the object is
       a complete object or a subobject).  Pass the second argument from the
       constructor call (a VTT parameter in the IA-64 ABI or a virtual base
       class pointer in the Cfront ABI) as the second argument to the
       delegation destructor. */
    ctor_init->initializer->destructible_entity_descr->use_delegation_dtor =
                                                                          TRUE;
    ctor_init->initializer->destructible_entity_descr->delegation_dtor_arg =
                    innermost_function_scope->variant.routine.parameters->next;
  }  /* if */
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
array (that's used only in the Cfront-like ABI).  The node has type int
for the Cfront-like ABI, type size_t for the IA-64 ABI.
*/
{
  an_expr_node_ptr num_elem_node;
  a_constant       num_elem_constant;

#if IA64_ABI
  check_assertion(array_element_count >= 0);
#endif /* IA64_ABI */
  set_integer_constant_with_overflow_check(
                 &num_elem_constant, (a_host_large_integer)array_element_count,
#if IA64_ABI
                 targ_size_t_int_kind,
#else /* !IA64_ABI */
                 targ_runtime_elem_count_int_kind,
#endif /* IA64_ABI */
                 (a_type_ptr)NULL,
                 /*preserve_needed_flag=*/FALSE);
  /* Allocate an expression node for the constant. */
  num_elem_node = alloc_node_for_constant(&num_elem_constant);
  return num_elem_node;
}  /* num_elem_node_from_count */


static an_expr_node_ptr num_elem_node_if_array(an_init_pos_descr_ptr ipdp)
/*
ipdp gives the position of an entity.  If it is an array, construct an
expression that gives the effective number of elements in the array and return
a pointer to it.  Note that the "effective" number of elements in the array
may be fewer than the actual number of elements in the array (e.g., when
called to initialize "new A[5] {99}", will return 4, not 5 -- as specified
by the array_element_count or partial_initialization_starting_element fields
of ipdp).  Returns NULL for non-array types.  The node has type int
for the Cfront-like ABI, type size_t for the IA-64 ABI.  For a
multi-dimensional array, the number of elements is the total across
all dimensions.
*/
{
  an_expr_node_ptr num_elem_node = NULL;

  if (ipdp->variable != NULL &&
      ipdp->variable->is_vla &&
      !ipdp->indirect_through_variable &&
      (ipdp->modifiers == NULL || ipdp->array_element_sequence)) {
    /* The entity being accessed is a variable-length array (VLA). */
    a_variable_ptr num_elem_var;
    /* Get the variable that has been set to the number of elements in the
       VLA.  For multi-dimensional arrays, it gives the total across all
       bounds. */
    num_elem_var = ipdp->variable->vla_element_count_variable;
    num_elem_node = var_rvalue_expr(num_elem_var);
    num_elem_node = add_cast_if_necessary(num_elem_node,
                                          integer_type(
#if IA64_ABI
                                               targ_size_t_int_kind
#else /* !IA64_ABI */
                                               targ_runtime_elem_count_int_kind
#endif /* IA64_ABI */
                                               ));
  } else {
    /* Not a VLA. */
    a_boolean        is_array = FALSE;
    a_targ_ptrdiff_t array_element_count = 0;
    if (ipdp->array_element_sequence) {
      /* Accessing a sequence of array elements. */
      is_array = TRUE;
      array_element_count = ipdp->array_element_count;
    } else {
      a_type_ptr entity_type = type_from_init_pos_descr(ipdp);
      if (is_array_type(entity_type)) {
        /* Accessing a whole array. */
        is_array = TRUE;
        array_element_count = num_array_elements(entity_type);
      }  /* if */
    }  /* if */
    if (is_array) {
      if (ipdp->array_element_count == 0 && ipdp->num_elem_node != NULL) {
        /* For variably-sized arrays, use the expression that has already
           been created to describe the number of elements in the array. */
        num_elem_node = make_reusable_copy(ipdp->num_elem_node,
                                           /*vars_can_change=*/TRUE);
        if (ipdp->partial_initialization_starting_element > 0) {
          /* If we're partially initializing the array, we need to compute
             at run-time the effective number of elements being accessed
             (by subtracting the number already initialized).  Note that
             we're purposely skipping the case where
             partial_initialization_starting_element is zero (since there's
             no need to subtract zero in this case). */
          a_constant starting_elem_constant;
          set_integer_constant_with_overflow_check(
                                 &starting_elem_constant,
                                 ipdp->partial_initialization_starting_element,
#if IA64_ABI
                                 targ_size_t_int_kind,
#else /* !IA64_ABI */
                                 targ_runtime_elem_count_int_kind,
#endif /* IA64_ABI */
                                 (a_type_ptr)NULL,
                                 /*preserve_needed_flag=*/FALSE);
          num_elem_node->next = alloc_node_for_constant(
                                                      &starting_elem_constant);
          num_elem_node = make_operator_node(
                                           (an_expr_operator_kind)eok_subtract,
                                           num_elem_node->type,
                                           num_elem_node);
        }  /* if */
      } else {
        num_elem_node = num_elem_node_from_count(array_element_count);
      }  /* if */
    }  /* if */
  }  /* if */
  return num_elem_node;
}  /* num_elem_node_if_array */


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


static an_expr_node_ptr expr_for_pointer_to_routine(a_routine_ptr routine,
                                                    a_type_ptr    ptr_type)
/*
Build and return an expression node for the address of the indicated routine.
If the routine pointer is NULL, build an expression that is a null function
pointer and return that.  In either case, the node is cast to the given
function pointer type.
*/
{
  an_expr_node_ptr expr;
  a_constant       null_constant;

  if (routine != NULL) {
    expr = function_addr_expr(routine);
    /* Cast the function pointer to the generic function type. */
    expr = add_cast_if_necessary(expr, ptr_type);
  } else {
    /* No routine; use 0 cast to the right function pointer type. */
    make_zero_of_proper_type(ptr_type, &null_constant);
    expr = alloc_node_for_constant(&null_constant);
  }  /* if */
  return expr;
}  /* expr_for_pointer_to_routine */


/*
Pointer to the generic function pointer types used for passing constructors,
destructors, copy constructors, operator new, and operator delete functions
to runtime routines for array construction/destruction.  NULL until created.
*/
static a_type_ptr
		ctor_ptr_type,
		dtor_ptr_type,
		cctor_ptr_type,
		new_routine_ptr_type,
		delete_routine_ptr_type;


static an_expr_node_ptr expr_for_pointer_to_constructor(a_routine_ptr routine)
/*
Build and return an expression node for the address of the indicated
constructor, to be used to pass it to a runtime routine for array
construction/destruction.  If the routine pointer is NULL, build an
expression that is a null function pointer and return that.
*/
{
  an_expr_node_ptr expr;

  if (ctor_ptr_type == NULL) {
    /* Make the generic constructor pointer type.  It is
         void  (*)(void *);  // IA-64 ABI
         void *(*)(void *);  // Cfront-like ABI
    */
    a_type_ptr function_type;
#if IA64_ABI
    function_type = make_function_type(void_type(),
                                       void_star_type(),
                                       (a_type_ptr)NULL);
#else /* !IA64_ABI */
    function_type = make_function_type(void_star_type(),
                                       void_star_type(),
                                       (a_type_ptr)NULL);
#endif /* IA64_ABI */
    ctor_ptr_type = make_pointer_type(function_type);
  }  /* if */
  expr = expr_for_pointer_to_routine(routine, ctor_ptr_type);
  return expr;
}  /* expr_for_pointer_to_constructor */


static an_expr_node_ptr expr_for_pointer_to_destructor(a_routine_ptr routine)
/*
Build and return an expression node for the address of the indicated
destructor, to be used to pass it to a runtime routine for array
construction/destruction.  If the routine pointer is NULL, build an
expression that is a null function pointer and return that.
*/
{
  an_expr_node_ptr expr;

  if (dtor_ptr_type == NULL) {
    /* Make the generic destructor pointer type.  It is
         void (*)(void *);       // IA-64 ABI
         void (*)(void *, int);  // Cfront-like ABI
    */
    a_type_ptr function_type;
#if IA64_ABI
    function_type = make_function_type(void_type(),
                                       void_star_type(),
                                       (a_type_ptr)NULL);
#else /* !IA64_ABI */
    function_type = make_function_type(void_type(),
                                       void_star_type(),
                                       integer_type((an_integer_kind)ik_int));
#endif /* IA64_ABI */
    dtor_ptr_type = make_pointer_type(function_type);
  }  /* if */
  expr = expr_for_pointer_to_routine(routine, dtor_ptr_type);
  return expr;
}  /* expr_for_pointer_to_destructor */


static an_expr_node_ptr expr_for_pointer_to_copy_constructor(
                                                         a_routine_ptr routine)
/*
Build and return an expression node for the address of the indicated
copy constructor, to be used to pass it to a runtime routine for array
construction/destruction.  If the routine pointer is NULL, build an
expression that is a null function pointer and return that.
*/
{
  an_expr_node_ptr expr;

  if (cctor_ptr_type == NULL) {
    /* Make the generic copy constructor pointer type.  It is
         void  (*)(void *, void *);  // IA-64 ABI
         void *(*)(void *, void *);  // Cfront-like ABI
    */
    a_type_ptr function_type;
#if IA64_ABI
    function_type = make_function_type(void_type(),
                                       void_star_type(),
                                       void_star_type());
#else /* !IA64_ABI */
    function_type = make_function_type(void_star_type(),
                                       void_star_type(),
                                       void_star_type());
#endif /* IA64_ABI */
    cctor_ptr_type = make_pointer_type(function_type);
  }  /* if */
  expr = expr_for_pointer_to_routine(routine, cctor_ptr_type);
  return expr;
}  /* expr_for_pointer_to_copy_constructor */


static an_expr_node_ptr expr_for_pointer_to_new(a_routine_ptr routine)
/*
Build and return an expression node for the address of the indicated
operator new routine, to be used to pass it to a runtime routine for array
construction/destruction.  If the routine pointer is NULL, build an
expression that is a null function pointer and return that.
*/
{
  an_expr_node_ptr expr;

  if (new_routine_ptr_type == NULL) {
    /* Make the generic operator new routine pointer type.  It is
         void *(*)(size_t);
    */
    a_type_ptr function_type;
    function_type = make_function_type(void_star_type(),
                                       integer_type(targ_size_t_int_kind),
                                       (a_type_ptr)NULL);
    new_routine_ptr_type = make_pointer_type(function_type);
  }  /* if */
  expr = expr_for_pointer_to_routine(routine, new_routine_ptr_type);
  return expr;
}  /* expr_for_pointer_to_new */


static an_expr_node_ptr expr_for_pointer_to_delete(a_routine_ptr routine)
/*
Build and return an expression node for the address of the indicated
operator delete routine, to be used to pass it to a runtime routine for array
construction/destruction.  If the routine pointer is NULL, build an
expression that is a null function pointer and return that.
*/
{
  an_expr_node_ptr expr;

  if (delete_routine_ptr_type == NULL) {
    /* Make the generic operator delete routine pointer type.  It is
         void (*)(void *);
    */
    a_type_ptr function_type;
    function_type = make_function_type(void_type(),
                                       void_star_type(),
                                       (a_type_ptr)NULL);
    delete_routine_ptr_type = make_pointer_type(function_type);
  }  /* if */
  expr = expr_for_pointer_to_routine(routine, delete_routine_ptr_type);
  return expr;
}  /* expr_for_pointer_to_delete */

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
    a_param_type_ptr param;
    param = unlowered_param_type_list_for_routine(new_routine);
    if (!is_class_or_namespace_member(new_routine) &&
        param->next != NULL && param->next->next == NULL && 
        is_void_star_type(param->next->type)) {
      need_padding = FALSE;
    }  /* if */
  }  /* if */
  if (need_padding) {
    /* The amount of padding is equal to the maximum of the size of the
       cookie and the alignment of an element in the array. */
    /* The standard IA-64 ABI cookie is a size_t. */
    padding_size = integer_type((an_integer_kind)targ_size_t_int_kind)->size;
#if IA64_ABI_USE_VARIANT_ARRAY_COOKIES
    /* The variant cookie is a struct containing two size_t fields. */
    padding_size *= 2;
#endif /* IA64_ABI_USE_VARIANT_ARRAY_COOKIES */
    { a_targ_alignment  alignment = alignment_of_type(type);
      if (alignment > padding_size) {
        padding_size = alignment;
      }  /* if */
    }
  } /* if */
  if (padding_size != 0 || even_if_zero) {
    /* Make the expression. */
    padding_node = node_for_integer_constant((long)padding_size, 
                                             targ_size_t_int_kind);
  }  /* if */
  return padding_node;
}  /* get_array_new_padding */

#else /* !IA64_ABI */

/*
Variable entry for the runtime global variable __array_new_prefix_size,
which gives the size in bytes of the array allocation prefix.  NULL until
created.  Used only with ABI_CHANGES_FOR_PLACEMENT_DELETE set to TRUE.
*/
static a_variable_ptr
		array_new_prefix_size_var;

#endif /* !IA64_ABI */
#if ABI_CHANGES_FOR_PLACEMENT_DELETE

#if !IA64_ABI
/*ARGSUSED*/ /* <-- elem_type, new_routine are not used in that case. */
#endif /* !IA64_ABI */
static an_expr_node_ptr get_prefix_size_node(a_type_ptr    elem_type,
                                             a_routine_ptr new_routine)
/*
Utility routine to return an expression that represents the size of the
prefix that is added to an array whose type is elem_type or NULL (in the
IA-64 ABI) if no prefix is needed.  If new_routine is non-NULL, it is the
placement new routine that is being called to allocate the memory.
*/
{
  an_expr_node_ptr prefix_size_node = NULL;

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
#else /* IA64_ABI */
  prefix_size_node = get_array_new_padding(elem_type, new_routine,
                                           /*even_if_zero=*/FALSE);
#endif /* IA64_ABI  */
  return prefix_size_node;
}  /* get_prefix_size_node */

#endif /* ABI_CHANGES_FOR_PLACEMENT_DELETE */

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
  if (zero_storage) {
    /* The IA-64 runtime does not have variants that zero storage,
       nor could it because of the pointer-to-data-member problem.
       If zeroing is needed, use a wrapper routine that zeroes the
       storage and then calls the constructor (if any). */
    ctor_routine = helper_routine_to_initialize_entity(
                                          type_pointed_to(entity_type),
                                          /*have_complete_object=*/TRUE,
                                          /*need_array_count=*/FALSE,
                                          /*zero_entity=*/TRUE,
                                          ctor_routine);
  }  /* if */
#else /* !IA64_ABI */
  /* Cast to the proper type for the element count parameter. */
  num_elem_node = add_cast_if_necessary(num_elem_node,
                               integer_type(targ_runtime_elem_count_int_kind));
#endif /* IA64_ABI */
  /* Build an expression for the address of the constructor. */
  ctor_addr_node = expr_for_pointer_to_constructor(ctor_routine);
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
      dtor_addr_node = expr_for_pointer_to_destructor(dtor_routine);
      ctor_addr_node->next = dtor_addr_node;
      call_node = make_runtime_rout_call("__vec_new_eh_zero",
                                         &vec_new_eh_zero_routine,
                                         void_star_type(), arg_expr_list);
    } else if (exceptions_enabled && dtor_routine != NULL) {
      /* __vec_new_eh call, with destructor. */
      dtor_addr_node = expr_for_pointer_to_destructor(dtor_routine);
      ctor_addr_node->next = dtor_addr_node;
      call_node = make_runtime_rout_call("__vec_new_eh", &vec_new_eh_routine,
                                         void_star_type(), arg_expr_list);
    } else {
      /* __vec_new call, without destructor. */
      call_node = make_runtime_rout_call("__vec_new", &vec_new_routine,
                                         void_star_type(), arg_expr_list);
    }  /* if */
#else /* IA64_ABI */
    dtor_addr_node = expr_for_pointer_to_destructor(dtor_routine);
    ctor_addr_node->next = dtor_addr_node;
    if (entity_node != NULL) {
      call_node = make_runtime_rout_call("__cxa_vec_ctor", &vec_ctor_routine,
                                         void_type(), arg_expr_list);
      /* Unfortunately, __cxa_vec_ctor actually returns void, so we can't
         use its return value.  Instead we have to return the value of the
         entity_node. */
      {
        an_expr_node_ptr entity_copy =
                                  make_reusable_copy(entity_node,
                                                     /*vars_can_change=*/TRUE);
        call_node = make_comma_node(call_node, entity_copy);
      }
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
    dtor_addr_node = expr_for_pointer_to_destructor(dtor_routine);
    is_two_arg_delete = (delete_routine != NULL &&
                         is_two_argument_delete(delete_routine));
#if !IA64_ABI
    is_two_arg_node = node_for_integer_constant((long)is_two_arg_delete,
                                                (an_integer_kind)ik_int);
#else /* IA64_ABI */
    /* In the IA-64 ABI, if the allocation or deallocation routine is
       missing we have to pass the implied default routine because we
       can't pass NULL for that pointer. */
    /* See the interface for __cxa_vec_new2. */
    if (new_routine == NULL) {
      a_boolean    ambiguous;
      a_symbol_ptr new_sym =
                         opname_function_symbol((an_opname_kind)onk_array_new);
      check_assertion(new_sym != NULL);
      new_sym = find_default_operator_new_sym(new_sym, &ambiguous);
      check_assertion(new_sym != NULL);
      new_routine = new_sym->variant.routine.ptr;
    }  /* if */
    if (delete_routine == NULL) {
      a_boolean    ambiguous;
      a_symbol_ptr delete_sym =
                      opname_function_symbol((an_opname_kind)onk_array_delete);
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (delete_sym == NULL && microsoft_mode) {
        /* In Microsoft mode operator delete[] is not predeclared.  Use
           the non-array operator delete instead. */
        delete_sym = opname_function_symbol((an_opname_kind)onk_delete);
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      check_assertion(delete_sym != NULL && !is_two_arg_delete);
      delete_sym = find_default_operator_delete_sym(delete_sym, &ambiguous);
      check_assertion(delete_sym != NULL);
      delete_routine = delete_sym->variant.routine.ptr;
    }  /* if */
#endif /* !IA64_ABI */
    new_addr_node = expr_for_pointer_to_new(new_routine);
    delete_addr_node = expr_for_pointer_to_delete(delete_routine);
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
  /* Cast to the proper type for the element count parameter. */
  num_elem_node = add_cast_if_necessary(num_elem_node,
                               integer_type(targ_runtime_elem_count_int_kind));
  /* Build a constant node for the size of the array elements. */
  size_elem_node = size_elem_node_from_pointer_type(entity_type);
  ctor_addr_node = expr_for_pointer_to_constructor(ctor_routine);
  dtor_addr_node = expr_for_pointer_to_destructor(dtor_routine);
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
  an_expr_node_ptr assign_node, arg_entity_node = entity_node;
#if IA64_ABI_USE_VARIANT_ARRAY_COOKIES
  an_expr_node_ptr assign_elem_size_node;
#endif /* IA64_ABI_USE_VARIANT_ARRAY_COOKIES */
  if (prefix_size_node != NULL) {
    an_expr_node_ptr cookie_ptr_node, cookie_value_node;
#if IA64_ABI_USE_VARIANT_ARRAY_COOKIES
    /* Generate code to set the array element size field in the variant
       array cookie.  The variant cookie is a struct containing two size_t
       fields, in the order element_size, element_count.  The normal
       code below will set the second field to the number of elements. */
    /* Build a constant node for the size of the array elements. */
    an_expr_node_ptr size_elem_node =
                                 size_elem_node_from_pointer_type(entity_type);
    /* entity_node contains the address of the allocation.  Cast it to
       "size_t *" then subtract 2 to get to the first field. */
    cookie_ptr_node = add_cast_if_necessary(
                                          entity_node,
                                          make_pointer_type(
                                          integer_type(targ_size_t_int_kind)));
    entity_node = make_reusable_copy(entity_node, /*vars_can_change=*/FALSE);
    cookie_ptr_node->next = node_for_integer_constant(2L,
                                                      targ_size_t_int_kind);
    cookie_ptr_node = make_operator_node((an_expr_operator_kind)eok_psubtract,
                                         cookie_ptr_node->type,
                                         cookie_ptr_node);
    cookie_ptr_node = add_indirection_to_node(cookie_ptr_node);
    /* Generate the assignment expression.  It is inserted below. */
    assign_elem_size_node = make_assignment_expr(
                                            cookie_ptr_node,
                                            (an_expr_operator_kind)eok_assign,
                                            size_elem_node);
#endif /* IA64_ABI_USE_VARIANT_ARRAY_COOKIES */
    /* If there was padding, we must set the value indicating how many
       elements there are.  Compute the address of the "cookie". */

    cookie_ptr_node = add_cast_if_necessary(entity_node,
                                            make_pointer_type
                                                    (prefix_size_node->type));
    arg_entity_node = make_reusable_copy(entity_node,
                                         /*vars_can_change=*/FALSE);
    cookie_ptr_node->next = node_for_integer_constant(1L,
                                                      targ_size_t_int_kind);
    cookie_ptr_node = make_operator_node((an_expr_operator_kind)eok_psubtract,
                                         cookie_ptr_node->type,
                                         cookie_ptr_node);
    cookie_ptr_node = add_indirection_to_node(cookie_ptr_node);
    /* Compute the value. */
    cookie_value_node = num_elem_node;
    num_elem_node = make_reusable_copy(num_elem_node, 
                                       /*vars_can_change=*/FALSE);
    /* Perform the assignment. */
    assign_node = make_assignment_expr(cookie_ptr_node, 
                                       (an_expr_operator_kind)eok_assign,
                                       cookie_value_node);
  }  /* if */
  call_node = make_vec_new_call(arg_entity_node, entity_type, num_elem_node, 
                                ctor_routine, dtor_routine,
                                (a_routine_ptr)NULL, (a_routine_ptr)NULL,
                                zero_storage);
  if (prefix_size_node != NULL) {
#if IA64_ABI_USE_VARIANT_ARRAY_COOKIES
    assign_node = make_comma_node(assign_elem_size_node, assign_node);
#endif /* IA64_ABI_USE_VARIANT_ARRAY_COOKIES */
    call_node = make_comma_node(assign_node, call_node);
  }  /* if */
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
    if (prefix_size_node != NULL) {
      /* Subtract the array prefix size. */
      entity_node_copy = add_cast_if_necessary(entity_node_copy,
                                               char_star_type());
      entity_node_copy->next = prefix_size_node;
      entity_node_copy = make_operator_node(
                                          (an_expr_operator_kind)eok_psubtract,
                                          entity_node_copy->type,
                                          entity_node_copy);
    }  /* if */
    /* Cast the argument to "void *", which is what the delete routine
       expects. */
    entity_node_copy = add_cast_if_necessary(entity_node_copy,
                                             void_star_type());
    entity_node_copy->next = delete_args;
    /* Make a call of the placement delete routine. */
    delete_call = make_call_node(delete_routine, entity_node_copy);
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
                                          an_expr_node_ptr num_elem_node,
                                          an_expr_node_ptr dtor_addr_node,
                                          a_routine_ptr    delete_routine,
                                          a_boolean        free_storage)
/*
Make a call to a runtime routine (__vec_delete or __array_delete
for the Cfront-like ABI, __cxa_vec_dtor etc. for the IA-64 ABI)
that will call a destructor for each element of an array and then
deallocate the array.  entity_node gives the address of the array.
num_elem_node is an expression giving the number of elements
in the array, or NULL for an array allocated with new[] (whose
size is known to the runtime).  dtor_addr_node is an expression
for the address of the destructor routine to be called, or NULL if no
destructor is to be called.  delete_routine is the delete routine to
be called, or NULL if the normal delete routine should be called.
free_storage is TRUE if the storage for the array is to be freed.
A pointer to the expression created is returned.
*/
{
  an_expr_node_ptr call_node, arg_expr_list, size_elem_node;
  an_expr_node_ptr delete_addr_node;
#if !IA64_ABI
  an_expr_node_ptr is_two_arg_node, free_storage_node;
#else /* IA64_ABI */
  an_expr_node_ptr prefix_size_node;
#endif /* IA64_ABI */

  /* Build a constant node for the size of the array elements. */
  size_elem_node = size_elem_node_from_pointer_type(entity_node->type);
#if !IA64_ABI
  if (num_elem_node == NULL) {
    /* -1 tells the runtime to use the array size from the "new[]". */
    num_elem_node = num_elem_node_from_count((a_targ_ptrdiff_t)-1);
  }  /* if */
  if (delete_routine == NULL) {
    /* The call looks like
         __vec_delete(entity_node, num_elems, size_elem, dtor_addr_node,
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
       __array_delete(entity_node, num_elems, size_elem, dtor_addr_node,
                      delete_routine, is_two_arg)
       is_two_arg is 1 to indicate that the delete routine is a 2-argument
       routine, 0 otherwise.
    */
    delete_addr_node = expr_for_pointer_to_delete(delete_routine);
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
  if (num_elem_node != NULL) {
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
    if (num_elem_node != NULL) {
      /* The call looks like
           __cxa_vec_dtor(entity_node, num_elems, size_elem, dtor_addr_node)
      */
      check_assertion(!free_storage);
      /* Splice in the node for the number of elements. */
      entity_node->next = num_elem_node;
      num_elem_node->next = size_elem_node;
      call_node = make_runtime_rout_call("__cxa_vec_dtor", &vec_dtor_routine,
                                         void_type(), arg_expr_list);
    } else {
      /* The call looks like
           __cxa_vec_delete(entity_node, size_elem, padding, addr_node)
         The runtime uses a cookie to determine the array size.
      */
      check_assertion(free_storage);
      call_node = make_runtime_rout_call("__cxa_vec_delete", 
                                         &vec_delete_routine, void_type(),
                                         arg_expr_list);
    }  /* if */
  } else {
    delete_addr_node = expr_for_pointer_to_delete(delete_routine);
    dtor_addr_node->next = delete_addr_node;
    check_assertion(num_elem_node == NULL && free_storage);
    if (is_two_argument_delete(delete_routine)) {
      /* The call looks like
           __cxa_vec_delete3(entity_node, size_elem, padding, dtor_addr_node,
                             delete_routine)
         The runtime uses a cookie to determine the array size.  The
         delete routine is a two-argument version.
      */
      call_node = make_runtime_rout_call("__cxa_vec_delete3",
                                         &vec_delete3_routine, void_type(),
                                         arg_expr_list);
    } else {
      /* The call looks like
           __cxa_vec_delete2(entity_node, size_elem, padding, dtor_addr_node,
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
                                          an_expr_node_ptr      entity_node,
                                          an_expr_node_ptr      source_node,
                                          an_init_pos_descr_ptr ipdp,
                                          a_routine_ptr         cctor_routine,
                                          a_routine_ptr         dtor_routine)
/*
Make a call to a runtime routine (__vec_cctor for the Cfront-like ABI,
__cxa_vec_cctor for the IA-64 ABI) that will call a copy constructor
for each element of an array.  entity_node gives the address of the
array.  source_node gives the source for the copy.  ipdp gives more
information on the destination (in particular, it gives the count of
array elements).  cctor_routine is the copy constructor routine to be
called.  dtor_routine is the destructor to be called if an exception
is thrown during the operation, or NULL if there isn't one.  A pointer
to the expression created is returned.
*/
{
  an_expr_node_ptr call_node, arg_expr_list, num_elem_node, size_elem_node;
  an_expr_node_ptr func_addr_node, dtor_addr_node;

  /* Build a node for the number of array elements. */
  num_elem_node = num_elem_node_if_array(ipdp);
  check_assertion(num_elem_node != NULL);
#if !IA64_ABI
  /* The num_elems parameter of __vec_cctor has type size_t, which
     is different than most of the similar routines. */
  num_elem_node = add_cast_if_necessary(num_elem_node,
                                        integer_type(targ_size_t_int_kind));
#endif /* !IA64_ABI */
  /* Build a constant node for the size of the array elements. */
  size_elem_node = size_elem_node_from_pointer_type(entity_node->type);
  /* Build an expression for the address of the copy constructor. */
  func_addr_node = expr_for_pointer_to_copy_constructor(cctor_routine);
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
    dtor_addr_node = expr_for_pointer_to_destructor(dtor_routine);
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
  dtor_addr_node = expr_for_pointer_to_destructor(dtor_routine);
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

#if MAINTAIN_NEEDED_FLAGS

static a_boolean generated_routine_needed_even_if_unreferenced(
                                                            a_routine_ptr rout)
/*
rout is a generated routine with a definition.  Return TRUE if it should be
considered needed even if it is not referenced, e.g., because it's an external
definition.
*/
{
  a_boolean needed = FALSE;

  check_assertion(rout->compiler_generated &&
                  rout->assoc_scope != NULL_region_number);
  /* If the routine is external (but not extern inline), mark it as needed. */
  if (rout->storage_class == (a_storage_class)sc_unspecified &&
      !rout->is_inline) {
    a_routine_ptr assoc_rout = NULL;
    /* If this is an entry point of some other routine, it's needed only
       if the primary routine is needed. */
#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
    if (rout->overriding_function_for_wrapper != NULL) {
      assoc_rout = rout->overriding_function_for_wrapper;
      rout = assoc_rout;  /* Allow both thunk and alternate entry point. */
    }  /* if */
#endif /* ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
#if IA64_ABI
    if (rout->primary_ctor_or_dtor != NULL) {
      assoc_rout = rout->primary_ctor_or_dtor;
    }  /* if */
#endif /* IA64_ABI */
    if (assoc_rout == NULL ||
        assoc_rout->source_corresp.needed) needed = TRUE;
  }  /* if */
  return needed;
}  /* generated_routine_needed_even_if_unreferenced */

#endif /* MAINTAIN_NEEDED_FLAGS */

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
  a_statement_ptr
		pending_stmk_init_statements;
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
Can also be used when lowering C to generate a routine context.
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
  grcontext->pending_stmk_init_statements = pending_stmk_init_statements;
  pending_stmk_init_statements = NULL;
  if (!C_mode()) {
    save_eh_lowering_context(&grcontext->ehcontext);
    add_object_lifetime_to_function_scope(scope);
  }  /* if */
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

  /* promote_local_entities_to_file_scope is not called here.  A generated
     routine shouldn't have the kinds of entities that need to be
     promoted, and calling it here would promote variables generated
     for exception handling (which doesn't happen for not-generated
     routines, because the promotion for them is done at the beginning
     of lowering, before those variables are generated).  In particular,
     if promote_local_entities_to_file_scope is changed always to return
     TRUE, the promotion here fouls up the calls of finish_array_var
     that record a type in aggregate initializers (triggered by
     the call of add_eh_function_prologue below): the promotion copies
     the constant to the file scope in its incomplete state, and
     the original instance is finished later, because the EH lowering
     has a pointer to the original instance and is unaware that it has
     been moved. */
  if (!C_mode()) {
    (void)pop_object_lifetime();
    clean_up_all_object_lifetimes(scope);
    if (exceptions_enabled
#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
        /* Don't add EH code to thunks. */
        && rout->overriding_function_for_wrapper == NULL
#endif /* ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
#if IA64_ABI && !HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS
        /* Don't add EH code to alternate entry points, unless the complete
           ctor/dtor contains exception handling code. */
        && rout->primary_ctor_or_dtor == NULL
#endif /* IA64_ABI && !HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS */
                                             ) {
      /* Add prologue/epilogue code for exceptions if needed.  This is done
         after the object lifetime is popped so we can tell whether any
         EH processing is really needed. */
      add_eh_function_prologue(scope);
    }  /* if */
  }  /* if */
  pop_context();
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
  /* Mark the routine as needed if it's external. */
  if (generated_routine_needed_even_if_unreferenced(rout)) {
    mark_as_needed((char *)rout, iek_routine);
  }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
  set_routine_defined(rout);
  if (!C_mode()) {
    restore_eh_lowering_context(&grcontext->ehcontext);
  }  /* if */
  promoted_local_static_variable_inits =
                               grcontext->promoted_local_static_variable_inits;
  free_return_memo_list(return_memo_list);
  return_memo_list = grcontext->return_memo_list;
  processing_file_scope_init_routine =
                                 grcontext->processing_file_scope_init_routine;
  check_assertion(pending_stmk_init_statements == NULL);
  pending_stmk_init_statements = grcontext->pending_stmk_init_statements;
  innermost_function_scope = grcontext->innermost_function_scope;
  depth_innermost_function_scope = grcontext->depth_innermost_function_scope;
  check_for_done_with_memory_region(region_number);
  switch_il_region(grcontext->region_to_switch_back_to);
}  /* pop_generated_routine_context */


static void add_null_test_around_routine(a_scope_ptr scope)
/*
Add "if (this)" around the whole routine whose top scope is given by "scope".
*/
{
  a_variable_ptr   this_param_var = scope->variant.routine.parameters;
  an_expr_node_ptr this_param_node, if_node;

  /* Make boolean controlling expression "this". */
  this_param_node = var_rvalue_expr(this_param_var);
  if_node = boolean_controlling_expr(this_param_node);
  /* Add the "if" statement. */
  enclose_routine_in_if(scope, if_node, (a_variable_ptr)NULL);
}  /* add_null_test_around_routine */

#if HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS

static void replace_parameter_in_node(an_expr_node_ptr                expr,
                                      an_expr_or_stmt_traversal_block *tblock)
/*
Called (via traverse_expr_list) by replace_parameters_in_expr_list to replace
a parameter in expr (if one exists) with a corresponding parameter.
*/
{
  a_variable_ptr  orig_ptr, new_ptr;

  if (is_variable_node(expr) && expr->variant.variable->is_parameter) {
    for (orig_ptr = tblock->orig_params, new_ptr = tblock->new_params;
         orig_ptr != NULL && new_ptr != NULL;
         orig_ptr = orig_ptr->next, new_ptr = new_ptr->next) {
      if (expr->variant.variable == orig_ptr) {
        check_assertion(identical_types(orig_ptr->type, new_ptr->type) &&
                    orig_ptr->is_parameter &&
                    new_ptr->is_parameter &&
                    (orig_ptr->is_this_parameter ||
                     orig_ptr->assoc_param_type != NULL) &&
                    (new_ptr->is_this_parameter ||
                     new_ptr->assoc_param_type != NULL) &&
                    (orig_ptr->assoc_param_type == NULL ||
                     new_ptr->assoc_param_type == NULL ||
                     orig_ptr->assoc_param_type->passed_via_copy_constructor ==
                      new_ptr->assoc_param_type->passed_via_copy_constructor));
        expr->variant.variable = new_ptr;
        break;
      }  /* if */
      /* Skip the VTT parameter that follows the "this" parameter in
         the list of original parameters. */
      if (orig_ptr->is_this_parameter) orig_ptr = orig_ptr->next;
    }  /* for */
    check_assertion(orig_ptr != NULL && new_ptr != NULL);
  } else if (expr->kind == (an_expr_node_kind)enk_lambda) {
    /* Look for parameters that may be in a lambda capture list and
       replace them. */
    a_lambda_capture_ptr ptr = expr->variant.lambda.ptr->capture_list;
    for (; ptr != NULL; ptr = ptr->next) {
      for (orig_ptr = tblock->orig_params, new_ptr = tblock->new_params;
           orig_ptr != NULL && new_ptr != NULL;
           orig_ptr = orig_ptr->next, new_ptr = new_ptr->next) {
        if (ptr->variable == orig_ptr) {
          check_assertion(identical_types(orig_ptr->type, new_ptr->type) &&
                    orig_ptr->is_parameter &&
                    new_ptr->is_parameter &&
                    (orig_ptr->is_this_parameter ||
                     orig_ptr->assoc_param_type != NULL) &&
                    (new_ptr->is_this_parameter ||
                     new_ptr->assoc_param_type != NULL) &&
                    (orig_ptr->assoc_param_type == NULL ||
                     new_ptr->assoc_param_type == NULL ||
                     orig_ptr->assoc_param_type->passed_via_copy_constructor ==
                      new_ptr->assoc_param_type->passed_via_copy_constructor));
          ptr->variable = new_ptr;
          break;
        }  /* if */
        /* Skip the VTT parameter that follows the "this" parameter in
           the list of original parameters. */
        if (orig_ptr->is_this_parameter) orig_ptr = orig_ptr->next;
      }  /* for */
    }  /* for */
  }  /* if */
}  /* replace_parameter_in_node */


static void replace_parameters_in_dynamic_init(a_dynamic_init_ptr dip,
                                               a_variable_ptr     orig_params,
                                               a_variable_ptr     new_params)
/*
This function is used to replace all parameters that occur in dip with
corresponding parameters from an alternate entry point.  dip has just been
copied from one scope to an alternate entry point scope; expressions referred
to by dip may contain references to parameters from the original scope
(orig_params) and they need to refer to corresponding parameters in the new
scope (new_params).  Note that the parameter list from the original scope has
an additional VTT parameter (which is skipped when matching parameters).
*/
{
  an_expr_or_stmt_traversal_block tblock;

  clear_expr_or_stmt_traversal_block(&tblock);
  tblock.process_expr = replace_parameter_in_node;
  tblock.orig_params = orig_params;
  tblock.new_params = new_params;
  traverse_dynamic_init(dip, &tblock);
}  /* replace_parameters_in_dynamic_init */


static a_constructor_init_ptr copy_ctor_init_with_remap(
                                           a_constructor_init_ptr   ctor_init,
                                           a_scope_ptr              from_scope,
                                           a_scope_ptr              to_scope,
                                           an_expr_copy_options_set options)
/*
Return a copy of the specified constructor init.  ctor_init is being copied
from from_scope to to_scope.  options is a set of options for the copy.
Assumes ctor_init has not been lowered yet.
*/
{
  a_constructor_init_ptr copy = copy_ctor_init(ctor_init, options);

  if (copy->initializer != NULL) {
    /* Expressions in the dynamic init may have variables (actually parameters)
       that refer to parameters in "from_scope"; they need to be replaced with
       the corresponding parameters in "to_scope". */
    replace_parameters_in_dynamic_init(copy->initializer,
                                       from_scope->variant.routine.parameters,
                                       to_scope->variant.routine.parameters);
  }  /* if */
  return copy;
}  /* copy_ctor_init_with_remap */


static void copy_ctor_inits(a_scope_ptr         from_scope,
                            a_scope_ptr         to_scope,
                            a_boolean           remove_originals,
                            a_ctor_or_dtor_kind kind)
/*
This routine copies constructor initializers of type "kind" from the
specified "from_scope" to the specified "to_scope" (both of which must be
function scopes for constructors and/or destructors).  When remove_originals
is TRUE, the original constructor initializers are removed from from_scope.
*/
{
  a_routine_ptr            from_routine;
  a_constructor_init_ptr   ctor_init, copy, prev = NULL;
  a_constructor_init_ptr   *delete_at =
                              &from_scope->variant.routine.constructor_inits;

#if DEBUG
  if (db_flag_is_set("copy_ctor_inits")) {
    (void)fprintf(f_debug, "Before: from lifetime = ");
    db_object_lifetime(from_scope->lifetime);
    (void)fprintf(f_debug, "from ctor_inits:\n");
    for (ctor_init = from_scope->variant.routine.constructor_inits;
         ctor_init != NULL;
         ctor_init = ctor_init->next) {
      db_dynamic_initializer(ctor_init->initializer, 0);
    }  /* for */
  }  /* if */
#endif /* DEBUG */
  check_assertion(from_scope->kind == (a_scope_kind)sck_function &&
                  to_scope->kind == (a_scope_kind)sck_function);
  from_routine = from_scope->variant.routine.ptr;
  check_assertion((from_routine->special_kind ==
                                    (a_special_function_kind)sfk_constructor ||
                   from_routine->special_kind ==
                                    (a_special_function_kind)sfk_destructor) &&
                  to_scope->variant.routine.constructor_inits == NULL);
  for (ctor_init = from_scope->variant.routine.constructor_inits;
       ctor_init != NULL;
       ctor_init = ctor_init->next) {
    if (ctor_init->kind == kind) {
      /* Found a ctor init of the proper kind. */
      a_dynamic_init_ptr  save_dip = NULL;
      check_assertion(ctor_init->initializer != NULL);
      if (from_routine->special_kind ==
                                     (a_special_function_kind)sfk_destructor) {
        /* During the copy below, a destruction will be queued on the lifetime
           of the current scope.  For destructors, this lifetime will be
           queued in the wrong order, so save the existing destructions
           and queue the new one after these. */
        save_dip = to_scope->lifetime->destructions;
        to_scope->lifetime->destructions = NULL;
      }  /* if */
      /* Copy the constructor init into the current memory region and link
         it onto the list. */
      copy = copy_ctor_init_with_remap(ctor_init, from_scope, to_scope,
                                       CE_COPYING_FROM_ONE_FUNC_TO_ANOTHER);
      if (from_routine->special_kind ==
                                     (a_special_function_kind)sfk_destructor) {
        /* For destructors, queue the new destruction at the end of the
           list. */
        if (save_dip != NULL) {
          a_dynamic_init_ptr dip;
          for (dip = save_dip;
               dip->next_in_destruction_list != NULL;
               dip = dip->next_in_destruction_list) {}
          dip->next_in_destruction_list =
                                        to_scope->lifetime->destructions;
          to_scope->lifetime->destructions = save_dip;
        }  /* if */
      }  /* if */
      if (prev == NULL) {
        to_scope->variant.routine.constructor_inits = copy;
      } else {
        prev->next = copy;
      }  /* if */
      prev = copy;
      if (remove_originals) {
        /* We're removing the ctor_inits, so remove them from the destruction
           list. */
        (*delete_at) = ctor_init->next;
        remove_from_destruction_list(ctor_init->initializer);
      }  /* if */
    } else if (remove_originals) {
      /* For destructors, the list is backwards. */
      delete_at = &(ctor_init->next);
    }  /* if */
  }  /* for */
  if (remove_originals &&
      from_scope->lifetime != NULL &&
      is_useless_object_lifetime(from_scope->lifetime)) {
    /* Remove the from_scope lifetime if it's no longer needed (i.e., if we've
       removed all of the constructor inits -- and any associated destructions
       from it). */
    unbind_object_lifetime(from_scope->lifetime);
    from_scope->lifetime = NULL;
  }  /* if */
#if DEBUG
  if (db_flag_is_set("copy_ctor_inits")) {
    (void)fprintf(f_debug, "After: from lifetime = ");
    db_object_lifetime(from_scope->lifetime);
    (void)fprintf(f_debug, "After: to lifetime = ");
    db_object_lifetime(to_scope->lifetime);
    (void)fprintf(f_debug, "from ctor_inits:\n");
    for (ctor_init = from_scope->variant.routine.constructor_inits;
         ctor_init != NULL;
         ctor_init = ctor_init->next) {
      db_dynamic_initializer(ctor_init->initializer, 0);
    }  /* for */
    (void)fprintf(f_debug, "to ctor_inits:\n");
    for (ctor_init = to_scope->variant.routine.constructor_inits;
         ctor_init != NULL;
         ctor_init = ctor_init->next) {
      db_dynamic_initializer(ctor_init->initializer, 0);
    }  /* for */
  }  /* if */
#endif /* DEBUG */
}  /* copy_ctor_inits */

#endif /* HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS */

/* Forward declarations. */
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
#if HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS || !IA64_ABI
static a_variable_ptr make_construction_vtbl_temporary(void);
#endif /* HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS || !IA64_ABI */
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */

static void add_virtual_base_init_code(
                                     a_scope_ptr        scope,
                                     a_variable_ptr     complete_var,
                                     a_handle_number    complete_var_handle,
                                     a_variable_ptr     construction_vtbls_var,
                                     an_insert_location *insert_location);

#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS

static void insert_default_construction_vtbls_assignment(
                                a_type_ptr              class_type,
                                a_variable_ptr          construction_vtbls_var,
                                an_insert_location      *insert_location);

#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */

static void lower_dtor_init(a_constructor_init_ptr ctor_init,
                            a_variable_ptr         this_param_var,
                            a_boolean              have_complete_object,
                            a_boolean              base_of_complete_object,
                            a_variable_ptr         destruction_vtbls_var,
                            an_insert_location_ptr insert_location);

static void initialize_dtor_init_for_cleanup(
                                        a_dynamic_init_ptr     dip,
                                        a_constructor_init_ptr ctor_init_list);

#if GENERATE_EH_TABLES

static void make_dtor_init_region_table_entries(
                                          a_dynamic_init_ptr dip,
                                          an_insert_location *insert_location);

#endif /* GENERATE_EH_TABLES */

static void insert_epilogue_cleanup_state(
                                 a_dynamic_init_ptr first_epilogue_destruction,
                                 an_insert_location *insert_location)
/*
Called at the end of generating code for a destructor, this routine creates
the region table entries (if needed), and inserts the initial cleanup state.
first_epilogue_destruction is a pointer to the first destruction in the
epilogue, and insert_location specifies where the resulting code should
be inserted.
*/
{
  if (exceptions_enabled && first_epilogue_destruction != NULL) {
#if GENERATE_EH_TABLES
    /* Make the region table entries for the epilogue destructions.
       This is done late because we want to put out the entries in
       reversed order, and we need to wait until they all have position
       information recorded. */
    make_dtor_init_region_table_entries(first_epilogue_destruction,
                                        insert_location);
#endif /* GENERATE_EH_TABLES */
    /* Insert code to establish the appropriate cleanup state at the
       beginning of the user-written code in the destructor.  This
       cleanup state calls for destruction of members and bases of
       the class. */
    curr_context->curr_cleanup_state =
        curr_context->latest_initialization = first_epilogue_destruction;
    insert_code_to_indicate_cleanup_state(curr_context->curr_cleanup_state,
                                          insert_location,
                                          /*unreachable=*/FALSE);
  }  /* if */
}  /* insert_epilogue_cleanup_state */


void define_default_version_of_routine(a_routine_ptr    routine,
                                       a_routine_ptr    new_routine,
                                       an_expr_node_ptr default_arg_list)
/*
Define new_routine, which is an alternate entry point for routine, with fewer
arguments.  Replacements for trailing arguments in routine are given by the
default_arg_list.  In configurations where
HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS is TRUE, this routine is called
twice (for classes that have virtual bases), once early to define the complete
constructor/destructor so that ctor_inits may be moved (or copied) into the
scope, and a second time during regular processing.  In the second case no
action is necessary (since the routine has already been defined).  It is
also called to define the alternate (static) entry point for the call
operator of a no-capture lambda.
*/
{
  an_expr_node_ptr implied_arg_list = NULL, end_implied_arg_list = NULL;
  an_expr_node_ptr call_node;
  a_scope_ptr      new_routine_scope;
  a_type_ptr       routine_type = skip_typerefs(routine->type);
  a_type_ptr       this_param_type;
  a_param_type_ptr first_actual_param_type;
  a_param_type_ptr src_param_type, param_type;
  a_routine_type_supplement_ptr
                   rtsp, new_rtsp;
  an_insert_location
                   insert_location;
  a_memory_region_number
                   new_routine_il_region;
  a_variable_ptr   this_param_var = NULL, param_var, last_param_var;
  an_expr_node_ptr this_arg, pass_through_arg, first_arg;
  a_statement_ptr  return_stmt;
  a_generated_routine_context
                   grcontext;
  a_boolean        insert_as_statement, void_return, is_lambda_entry_point;
  an_object_lifetime_ptr
                   init_expr_lifetime = NULL;
  a_context        def_arg_context;
#if HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS
  a_boolean        remove_originals;
  a_boolean        construct_virtual_bases = FALSE;
  a_boolean        destroy_virtual_bases = FALSE;
  a_variable_ptr   construction_vtbls_var = NULL;
  a_type_ptr       class_type = parent_class_of(routine);
  an_insert_location     
                   prologue_insert_location;
#endif /* HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS */

  /* Only define the new routine if we haven't already defined one. */
  if (new_routine->assoc_scope == NULL_region_number) {
    rtsp = routine->type->variant.routine.extra_info;
    new_rtsp = new_routine->type->variant.routine.extra_info;
    is_lambda_entry_point = new_routine->special_kind ==
                               (a_special_function_kind)sfk_lambda_entry_point;
    if (is_lambda_entry_point) {
      /* The alternate entry point of a lambda call operator is a static
         function and has no "this" parameter. */
      /* It's possible that the lambda call operator returns its value in
         a class type that requires a copy constructor and has been lowered
         to take a pointer to the return value as an extra parameter; make
         sure we handle this case (this is only a problem in the IA-64 ABI
         because "this" is first in the Cfront ABI). */
      if (rtsp->value_returned_as_parameter &&
         !rtsp->return_value_parameter_follows_this) {
        /* "this" is the second parameter. */
        this_param_type = rtsp->param_type_list->next->type;
      } else {
        /* "this" is the first parameter. */
        this_param_type = rtsp->param_type_list->type;
      }  /* if */
      first_actual_param_type = new_rtsp->param_type_list;
    } else {
      check_assertion(!rtsp->value_returned_as_parameter);
      this_param_type = new_rtsp->param_type_list->type;
      first_actual_param_type = new_rtsp->param_type_list->next;
    }  /* if */
    /* Make a memory region, scope, and block for the routine definition. */
    new_routine_scope = make_routine_definition(new_routine,
                                                /*make_return=*/FALSE,
                                                &new_routine_il_region);
    set_block_start_insert_location(new_routine_scope->assoc_block,
                                    &insert_location);
    push_generated_routine_context(new_routine_scope, new_routine_il_region,
                                   &grcontext);
    if (!is_lambda_entry_point) {
      /* Make a parameter variable for the "this" parameter (in lowered
         form as a normal parameter). */
      new_routine_scope->variant.routine.parameters = this_param_var =
                                  make_lowered_param_variable(this_param_type);
      new_routine_scope->variant.routine.this_param_variable = this_param_var;
      this_param_var->is_this_parameter = TRUE;
      last_param_var = this_param_var;
    } else {
      last_param_var = NULL;
    }  /* if */
    /* Make expression lists for constructor or destructor implied
       arguments. */
#if HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS
    if (!routine->is_delegating_ctor &&
        class_type->variant.class_struct_union.any_virtual_base_classes &&
        new_routine->ctor_dtor_kind == (a_ctor_or_dtor_kind)cdk_complete &&
        (new_routine->special_kind ==
                                    (a_special_function_kind)sfk_constructor ||
         new_routine->special_kind ==
                                    (a_special_function_kind)sfk_destructor)) {
      /* This is a complete object ctor/dtor that contains virtual bases and
         those virtual bases will be constructed/destructed in this
         ctor/dtor. */
      check_assertion(class_type->variant.class_struct_union.extra_info->
                                                   construction_vtbls != NULL);
      /* Create a temporary that will point to an array of virtual function
         table addresses. */
      construction_vtbls_var = make_construction_vtbl_temporary();
      /* Add the construction vtable as an implied argument to the subobject
         ctor/dtor. */
      implied_arg_list = var_rvalue_expr(construction_vtbls_var);
      end_implied_arg_list = implied_arg_list;
      if (new_routine->special_kind ==
                                     (a_special_function_kind)sfk_destructor) {
        /* Set the destruction_vtbls temporary to point to the default array
           of virtual function table pointers to be used when destroying a
           complete object. */
        insert_default_construction_vtbls_assignment(class_type,
                                                     construction_vtbls_var,
                                                     &insert_location);
        /* Save this insert location for the exception handling prologue. */
        prologue_insert_location = insert_location;
        destroy_virtual_bases = TRUE;
      } else {
        /* Note that insert_default_construction_vtbls_assignment is called
           later (from add_virtual_base_init_code). */
        /* Set a flag to indicate that construction of virtual bases should
           occur later in this ctor (after the parameter lists have been
           established). */
        construct_virtual_bases = TRUE;
      }  /* if */
    } else
#endif /* HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS */
    /* Do not add code here. */
    if (routine->special_kind == (a_special_function_kind)sfk_constructor) {
#if IA64_ABI
      if (new_routine->special_kind ==
                                    (a_special_function_kind)sfk_constructor &&
          ctor_needs_implied_arg_list(new_routine)) {
        /* We're calling a constructor from a constructor entry point, and the
           entry point routine has parameters for the implied arguments.  They
           will be copied below so we don't need implied arguments. */
      } else
#endif /* IA64_ABI */
      /* Do not add code here. */
      {
        /* We're calling a constructor from something that doesn't have
           parameters for the implied arguments, so make them if necessary. */
        make_ctor_implied_arg_list(routine, /*is_target_ctor=*/FALSE,
                                   &implied_arg_list, &end_implied_arg_list);
      }  /* if */
    } else if (routine->special_kind ==
                                     (a_special_function_kind)sfk_destructor) {
#if IA64_ABI
      if (new_routine->special_kind ==
                                     (a_special_function_kind)sfk_destructor &&
          dtor_needs_vtt_argument(new_routine)) {
        /* We're calling a destructor from a destructor entry point, and the
           entry point routine has parameters for the implied arguments.  They
           will be copied below so we don't need implied arguments. */
      } else
#endif /* IA64_ABI */
      /* Do not add code here. */
      {
        /* We're calling a destructor from something that doesn't have
           parameters for the implied arguments, so make them if necessary. */
        make_dtor_implied_arg_list(routine, /*have_complete_object=*/TRUE,
                                   &implied_arg_list, &end_implied_arg_list);
      }  /* if */
    }  /* if */
    /* Do not process parameters with default argument values, since they
       are removed from the routine's interface. */
    for (param_type = first_actual_param_type;
         param_type != NULL;
         param_type = param_type->next) {
      param_var = make_lowered_param_variable(param_type->type);
      param_var->assoc_param_type = param_type;
      if (last_param_var == NULL) {
        new_routine_scope->variant.routine.parameters = param_var;
      } else {
        last_param_var->next = param_var;
      }  /* if */
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
      /* Find the original parameter type that corresponds to this
         parameter. */
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
      default_arg_list = copy_list_of_expr_trees(default_arg_list,
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
      /* Mark the location where any generated stmk_init statements should
         go (these can be generated by default arguments). */
      set_insert_location_mark(&insert_location);
      /* Lower the default argument expressions.  Note that this must be done
         after the copy because you can't copy an expression once it has been
         lowered -- temporaries might have been added. */
      lower_arg_expr_list(default_arg_list, routine_type, routine,
                          src_param_type, /*maintain_sequencing=*/FALSE,
                          (an_insert_location *)NULL);
      /* Insert any generated stmk_inits at the previously marked location. */
      insert_pending_stmk_init_statements_at_mark(&insert_location);
    }  /* if */
    if (implied_arg_list != NULL) {
      /* Add the implicit arguments to the front of the default argument
         list. */
      end_implied_arg_list->next = default_arg_list;
      default_arg_list = implied_arg_list;
    }  /* if */
#if HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS
    if (construct_virtual_bases || destroy_virtual_bases) {
      a_routine_ptr complete_routine;
      remove_originals =
#if HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS
                       FALSE;
#else /* !HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS */
                       TRUE;
#endif /* HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS */
      /* Move (or copy) virtual base ctor_inits from the subobject ctor/dtor
         into the complete object ctor/dtor so the construction/destruction
         of the virtual bases will take place in the complete object
         ctor/dtor. */
      complete_routine = alternate_entry_point(routine,
                                             (a_ctor_or_dtor_kind)cdk_complete,
                                             /*define_now=*/FALSE);
      copy_ctor_inits(scope_for_routine(routine),
                      scope_for_routine(complete_routine),
                      remove_originals,
                      (a_constructor_init_kind)cik_virtual_base_class);
      /* Start an object lifetime. */
      begin_block_object_lifetime(new_routine_scope->lifetime,
                                  &insert_location);
      if (construct_virtual_bases) {
        /* Emit code to initialize the virtual bases.  Must be done after the
           parameter list is complete (var_for_copy_constructor_source depends
           on it). */
        add_virtual_base_init_code(new_routine_scope, (a_variable_ptr)NULL,
                                   (a_handle_number)0, construction_vtbls_var,
                                   &insert_location);
      }  /* if */
    }  /* if */
#endif /* HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS */
    /* Add the "this" parameter at the appropriate point in the argument
       list. */
    if (is_lambda_entry_point) {
      /* Pass NULL as the "this" argument. */
      a_constant null_this;
      make_zero_of_proper_type(this_param_type, &null_this);
      this_arg = alloc_node_for_constant(&null_this);
    } else {
      this_arg = var_rvalue_expr(this_param_var);
    }  /* if */
    if (rtsp->value_returned_as_parameter &&
        !rtsp->return_value_parameter_follows_this) {
      /* The "this" argument is the second argument to the function. */
      check_assertion(default_arg_list != NULL);
      first_arg = default_arg_list;
      this_arg->next = default_arg_list->next;
      default_arg_list->next = this_arg;
    } else {
      /* The "this" argument is the first argument to the function. */
      first_arg = this_arg;
      this_arg->next = default_arg_list;
    }  /* if */
#if IA64_ABI
    if (new_routine->ctor_dtor_kind == (a_ctor_or_dtor_kind)cdk_delegation) {
      /* Create a cdk_delegation destructor alternate entry point that
         invokes the complete or subobject destructor depending on the
         value of the VTT parameter.  That is:

           if (vtt_param) {
             subobject-dtor(this, vtt_param);
           } else {
             complete-dtor(this);
           }
        */
      an_insert_location  dtor_insert_location;
      an_insert_location  then_insert_location, else_insert_location;
      an_expr_node_ptr    test_node;

      check_assertion(new_routine->special_kind ==
                                     (a_special_function_kind)sfk_destructor &&
                      routine->ctor_dtor_kind ==
                                     (a_ctor_or_dtor_kind)cdk_subobject);
      set_expr_creation_insert_location(&dtor_insert_location);
      test_node =
          var_rvalue_expr(new_routine_scope->variant.routine.parameters->next);
      test_node = boolean_controlling_expr(test_node);
      /* Create the "if" statement. */
      insert_if_statement(test_node, /*is_initialization_guard=*/FALSE,
                          &dtor_insert_location, (a_statement_ptr *)NULL,
                          &then_insert_location, &else_insert_location);
      /* Call the subobject destructor in the "then" clause. */
      make_call_statement(routine,
                          first_arg,
                          (an_expr_node_ptr)NULL,
                          &then_insert_location);
      /* Call the complete object destructor in the "else" clause. */
      make_call_statement(alternate_entry_point(routine,
                                             (a_ctor_or_dtor_kind)cdk_complete,
                                             /*define_now=*/FALSE),
                          var_rvalue_expr(this_param_var),
                          (an_expr_node_ptr)NULL,
                          &else_insert_location);
      call_node = dtor_insert_location.variant.expr;
    } else
#endif /* IA64_ABI */
    /* Do not insert code here. */
    {
      /* Make a call node that calls the original routine with all
         the implicit arguments, i.e., that passes all the extra arguments
         to the original routine. */
      call_node = make_call_node(routine, first_arg);
    }  /* if */
    /* If the routine has a void type, insert a statement for the call
       followed by a return statement.  Otherwise, attach the call directly
       to the return. */
    void_return = is_void_type(lowered_return_type_of(new_routine->type));
#if HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS
    if (exceptions_enabled || 
        (destroy_virtual_bases &&
         new_routine_scope->variant.routine.constructor_inits != NULL)) {
      /* Force insertion as a statement because we have exception handling code
         (potentially inserted during epilogue processing of return statements)
         or destructions that must follow the call. */
      insert_as_statement = TRUE;
    } else
#endif /* HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS */
    /* Do not insert code. */
    if (new_rtsp->has_ellipsis) {
      /* Force ctor/dtors that have an ellipsis argument to use a call or
         assignment expression statement (rather than a return statement).
         This creates the necessary temporary and makes things simpler for the
         C generating back end. */
      insert_as_statement = TRUE;
    } else { 
      insert_as_statement = void_return;
      /* If we might have to insert destructor calls, insert the call as
         a statement. */
      if (init_expr_lifetime != NULL) insert_as_statement = TRUE;
    }  /* if */
    if (insert_as_statement) {
      /* The call will be inserted as a separate statement. */
      a_variable_ptr temp_var = NULL;
      /* If the routine has a non-void return, put the value in a temporary
         and then return the temporary later. */
      if (!void_return) {
        temp_var = make_lowered_temporary(call_node->type);
        call_node = make_var_assignment_expr(temp_var, call_node);
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
#if HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS
      if (destroy_virtual_bases &&
          new_routine_scope->variant.routine.constructor_inits != NULL) {
        a_constructor_init_ptr ctor_init, ctor_init_list;
        a_dynamic_init_ptr     first_epilogue_destruction;

        ctor_init_list = new_routine_scope->variant.routine.constructor_inits;
        first_epilogue_destruction = ctor_init_list->initializer;
        check_assertion(first_epilogue_destruction != NULL);
        if (exceptions_enabled) {
          /* Do cleanup initialization for the virtual base destructions (only)
             on the ctor-initializer list of the destructor. */
          initialize_dtor_init_for_cleanup(first_epilogue_destruction,
                                           ctor_init_list);
        }  /* if */
        /* Destroy any virtual base classes on the ctor_init list. */
        for (ctor_init = ctor_init_list;
             ctor_init != NULL;
             ctor_init = ctor_init->next) {
          check_assertion(ctor_init->kind ==
                              (a_constructor_init_kind)cik_virtual_base_class);
          lower_dtor_init(ctor_init, this_param_var,
                          /*have_complete_object=*/FALSE,
                          /*base_of_complete_object=*/TRUE,
                          construction_vtbls_var,
                          &insert_location);
        }  /* for */
        /* Insert the region table and initial cleanup state. */
        insert_epilogue_cleanup_state(first_epilogue_destruction,
                                      &prologue_insert_location);
      }  /* if */
#endif /* HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS */
#if IA64_ABI
      if (new_routine->special_kind ==
                                     (a_special_function_kind)sfk_destructor &&
          new_routine->ctor_dtor_kind == (a_ctor_or_dtor_kind)cdk_deleting) {
        /* Add the deletion code for the IA-64 ABI deleting destructor. */
        a_type_ptr    new_class_type = parent_class_of(new_routine);
        a_routine_ptr delete_routine = class_type_supp(new_class_type)->
                                                 assoc_operator_delete_routine;
        check_assertion(delete_routine != NULL);
        this_arg = var_rvalue_expr(this_param_var);
        make_delete_call_statement(delete_routine, new_class_type, this_arg,
                                   &insert_location);
      }  /* if */
#endif /* IA64_ABI */
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
#if IA64_ABI
    if (new_routine->special_kind == (a_special_function_kind)sfk_destructor &&
        new_routine->ctor_dtor_kind == (a_ctor_or_dtor_kind)cdk_deleting &&
        !routine->is_virtual) {
      /* Add "if (this != NULL)" around the whole routine for a deleting
         destructor (it can be called with a null "this" pointer).   No such
         test is needed if the destructor is virtual (because "this" must be
         non-NULL to compute the address of the virtual destructor). */
      add_null_test_around_routine(new_routine_scope);
    }  /* if */
#endif /* IA64_ABI */
#if HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS
    /* Constructor inits were handled above and can now be discarded. */
    new_routine_scope->variant.routine.constructor_inits = NULL;
#endif /* HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS */
    pop_generated_routine_context(new_routine_scope, new_routine_il_region,
                                  &grcontext);
#if MINIMAL_INLINING
    if (new_routine->is_inline && inlining_enabled) {
      set_up_routine_for_inlining(new_routine_scope);
    }  /* if */
#endif /* MINIMAL_INLINING */
  }  /* if */
}  /* define_default_version_of_routine */


static void copy_and_lower_param_type_list(a_routine_ptr    routine,
                                           a_param_type_ptr last_param_type,
                                           a_boolean        do_default_args,
                                           a_boolean        do_lowering)
/*
Copy the parameter type entries given by the unlowered parameter list of the
given routine, adding them to the list of which last_param_type is presently
the end.  Add indirections to parameters with copy constructors as the types
are processed.  If do_default_args is FALSE, parameter types corresponding to
default arguments are not copied.  If do_lowering is TRUE, the routine type is
modified; if not, only the new parameters are modified.
*/
{
  a_type_ptr       pass_through_param_type;
  a_param_type_ptr src_param_type, param_type;

  for (src_param_type = unlowered_param_type_list_for_routine(routine); 
       src_param_type != NULL && 
         (do_default_args || !src_param_type->has_default_arg);
       src_param_type = src_param_type->next) {
    /* Create the qualified type for the parameter. */
    pass_through_param_type = make_qualified_type(src_param_type->type,
                                                  src_param_type->qualifiers);
    /* If the parameter is passed via a copy constructor and it has
       not been lowered, replace it by a pointer to the object.
       Note that a second copy constructor call (i.e., one within
       the generated routine) is not necessary. */
    if (src_param_type->passed_via_copy_constructor &&
        !visited_yet(src_param_type)) {
      if (do_lowering) {
        add_indirection_to_cctor_param_type(src_param_type);
      } else {
        pass_through_param_type = 
                 type_of_cctor_param_after_adding_indirection(src_param_type);
      }  /* if */
    }  /* if */
    param_type = alloc_param_type(pass_through_param_type);
    param_type->has_default_arg = src_param_type->has_default_arg;
    param_type->default_arg_appeared_in_class_definition =
                      src_param_type->default_arg_appeared_in_class_definition;
    param_type->passed_via_copy_constructor =
                                   src_param_type->passed_via_copy_constructor;
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
                                  lowered_return_type_of(routine_type),
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
    copy_and_lower_param_type_list(routine, new_rtsp->param_type_list, 
                                   /*do_default_args=*/FALSE,
                                   /*do_lowering=*/TRUE);
    define_default_version_of_routine(routine, new_routine, 
                                      default_arg_list);
    routine = new_routine;
  }  /* if */
  return routine;
}  /* default_version_of_routine */

#if IA64_ABI

void set_primary_ctor_or_dtor_kind(a_routine_ptr routine)
/*
If the indicated primary constructor or destructor routine has not
yet been assigned as one of the required IA-64 entry points, do
that now by setting its ctor_dtor_kind field.  Note that the delegating
constructor case (cdk_delegation) is handled separately
(see add_delegating_constructor_wrapper_code), and it's possible (even
likely) that a delegating constructor routine will have its primary
ctor_dtor_kind set to cdk_subobject here only to later be changed to
cdk_delegation (but externally that should be okay since the external
interface to the two routines is identical).
*/
{
  check_assertion(routine->primary_ctor_or_dtor == NULL);
  if (routine->ctor_dtor_kind == (a_ctor_or_dtor_kind)cdk_none) {
    a_type_ptr class_type = parent_class_of(routine);
    if (class_type->variant.class_struct_union.any_virtual_base_classes) {
      /* The class has virtual bases.  The primary routine is the subobject
         constructor, and the complete object constructor calls that. */
      routine->ctor_dtor_kind = (a_ctor_or_dtor_kind)cdk_subobject;
    } else {
      /* The class has no virtual bases.  The primary routine is the
         complete object constructor, and the subobject constructor
         is an entry point (that does nothing additional, i.e., it's
         an alias). */
      routine->ctor_dtor_kind = (a_ctor_or_dtor_kind)cdk_complete;
    }  /* if */
  }  /* if */
}  /* set_primary_ctor_or_dtor_kind */


a_routine_ptr alternate_entry_point(a_routine_ptr       routine,
                                    a_ctor_or_dtor_kind kind,
                                    a_boolean           define_now)
/*
Return a pointer to the alternate entry point for "routine" indicated by
"kind".  If the alternate entry point does not already exist, it is created.
If define_now is TRUE, the routine is defined if appropriate.  This
is used to create alternate entry points for constructors and
destructors in the IA-64 ABI.  Note that the primary routine will
be assigned to be one of the entry points (by setting ctor_dtor_kind to
the right value); in that case, the routine pointer returned by this
routine will be the same as the one passed in.
*/
{
  a_routine_ptr    new_routine = NULL;
  a_routine_list_entry_ptr 
                   rlep;

  check_assertion(routine->special_kind ==
                                  (a_special_function_kind)sfk_constructor ||
                  routine->special_kind ==
                                  (a_special_function_kind)sfk_destructor);
  check_assertion(kind == (a_ctor_or_dtor_kind)cdk_complete ||
                  kind == (a_ctor_or_dtor_kind)cdk_subobject ||
                  kind == (a_ctor_or_dtor_kind)cdk_delegation ||
                  kind == (a_ctor_or_dtor_kind)cdk_deleting);
  /* routine should not be a secondary entry point. */
  check_assertion(routine->primary_ctor_or_dtor == NULL);
  if (routine->ctor_dtor_kind == (a_ctor_or_dtor_kind)cdk_none) {
    /* The primary routine has not been assigned to be one of the entry
       points yet, so do that now. */
    set_primary_ctor_or_dtor_kind(routine);
  }  /* if */
  if (routine->ctor_dtor_kind == kind) {
    /* The primary routine is the entry point we want. */
    new_routine = routine;
  } else {
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
      a_type_ptr                    routine_type =skip_typerefs(routine->type);
      a_type_ptr                    this_param_type, return_type;
      a_param_type_ptr              param_type, last_param_type;
      a_routine_type_supplement_ptr rtsp, new_rtsp;
      a_storage_class               new_storage_class;
      rtsp = routine->type->variant.routine.extra_info;
      /* Make a type and routine entry for the routine. */
      /* The "this" parameter is generated in its lowered form (i.e., as a
         normal parameter). */
      this_param_type = implicit_this_param_type_of(routine_type);
      if (!should_drop_const_on_this_param_variable(rtsp->assoc_routine,
                                                    routine_type)) {
        /* Add const qualification to the "this" parameter type if
           appropriate. */
        this_param_type = make_qualified_type(this_param_type, TQ_CONST);
      }  /* if */
      return_type = lowered_return_type_of(routine_type);
#if IA64_ABI_VARIANT_CTORS_AND_DTORS_RETURN_THIS
      /* Deleting and delegation destructors return void even in the
         variant. */
      if ((kind == (a_ctor_or_dtor_kind)cdk_deleting ||
           kind == (a_ctor_or_dtor_kind)cdk_delegation) &&
          routine->special_kind == (a_special_function_kind)sfk_destructor) {
        return_type = void_type();
      }  /* if */
#endif /* IA64_ABI_VARIANT_CTORS_AND_DTORS_RETURN_THIS */
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
      new_routine = make_rout_entry_no_add((char *)NULL, new_storage_class,
                                           return_type,
                                           this_param_type);
      set_inline_flag(new_routine, routine->is_inline);
#if DECL_MODIFIERS_IN_USE
      new_routine->decl_modifiers = routine->decl_modifiers;
#endif /* DECL_MODIFIERS_IN_USE */
#if INSTANTIATE_EXTERN_INLINE
      new_routine->inline_instance_required =routine->inline_instance_required;
#endif /* INSTANTIATE_EXTERN_INLINE */
      new_routine->source_corresp.is_class_member = TRUE;
      new_routine->source_corresp.parent_scope = parent_scope_of(routine);
      set_routine_special_kind(new_routine, routine->special_kind);
      new_routine->ctor_dtor_kind = kind;
      new_routine->primary_ctor_or_dtor = routine;
      new_routine->compiler_generated = TRUE;
      new_routine->pure_virtual = routine->pure_virtual;
      new_routine->is_deleted = routine->is_deleted;
#if ONE_INSTANTIATION_PER_OBJECT
      new_routine->instantiation_needed_bit_number =
                                      routine->instantiation_needed_bit_number;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
      new_routine->is_delegating_ctor = routine->is_delegating_ctor;
      new_rtsp = new_routine->type->variant.routine.extra_info;
      new_rtsp->this_class = rtsp->this_class;
      mangle_alternate_entry_point_name(new_routine, routine);
      /* Make the new routine virtual if the old one is so that virtual
         destructors work correctly.  The virtual function number for the
         deleting destructor is one greater than for the complete object
         destructor.  Delegation destructors aren't invoked through the
         virtual function table and therefore are never virtual. */
      if (routine->is_virtual &&
          (kind != (a_ctor_or_dtor_kind)cdk_subobject &&
           kind != (a_ctor_or_dtor_kind)cdk_delegation)) {
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
      /* The new routine has an ellipsis if the old one does. */
      new_rtsp->has_ellipsis = rtsp->has_ellipsis;
#if HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS
      if (parent_class_of(routine)->
                         variant.class_struct_union.any_virtual_base_classes &&
          kind == (a_ctor_or_dtor_kind)cdk_complete &&
          (routine->assoc_scope != NULL_region_number &&
           scope_for_routine(routine)->
                                  variant.routine.constructor_inits != NULL) &&
          (new_routine->special_kind ==
                                    (a_special_function_kind)sfk_constructor ||
           new_routine->special_kind ==
                                    (a_special_function_kind)sfk_destructor)) {
        /* The new routine needs an exception specification if the old one
           has one and there are virtual base initializations/destructions
           (which necessitate exception handling). */
        new_rtsp->exception_specification = rtsp->exception_specification;
      }  /* if */
#endif /* HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS */
      if (kind == (a_ctor_or_dtor_kind)cdk_subobject &&
          routine->ctor_dtor_kind == (a_ctor_or_dtor_kind)cdk_complete) {
        /* When the primary routine is the complete object constructor or
           destructor (happens when there are no virtual base classes),
           the subobject entry point is an alias for the complete object
           routine. */
        new_routine->is_alias_entry = TRUE;
      }  /* if */
      /* Copy various Microsoft and GNU attributes that can affect constructors
         and/or destructors, and which naturally carry over to their alternate
         entry points. */
#if GNU_EXTENSIONS_ALLOWED
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
      new_routine->ELF_visibility = routine->ELF_visibility;
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
      new_routine->is_weak = routine->is_weak;
      new_routine->is_weakref = routine->is_weakref;
      new_routine->section = routine->section;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if DECL_MODIFIERS_IN_USE && MICROSOFT_EXTENSIONS_ALLOWED
      new_routine->decl_modifiers = (routine->decl_modifiers & DM_DLLFLAGS);
#endif /* DECL_MODIFIERS_IN_USE && MICROSOFT_EXTENSIONS_ALLOWED */
      /* Add new_routine to the list of alternate entry points for 
         routine. */
      rlep = alloc_list_entry_for_routine();
      rlep->routine = new_routine;
      rlep->next = routine->variant.ctor_dtor.alternate_entry_points;
      routine->variant.ctor_dtor.alternate_entry_points = rlep;
      last_param_type = new_rtsp->param_type_list;
      if ((new_routine->special_kind ==
                                    (a_special_function_kind)sfk_constructor &&
           ctor_needs_vtt_argument(new_routine)) ||
          (new_routine->special_kind ==
                                     (a_special_function_kind)sfk_destructor &&
           dtor_needs_vtt_argument(new_routine))) {
        /* Add a VTT parameter if necessary. */
        param_type = alloc_param_type(make_virtual_table_table_pointer_type());
        last_param_type->next = param_type;
        last_param_type = param_type;
      }  /* if */
      /* Copy the remainder of the parameters. */
      copy_and_lower_param_type_list(routine, last_param_type, 
                                     /*do_default_args=*/TRUE,
                                     /*do_lowering=*/FALSE);
    }  /* if */
    /* Define the routine if appropriate. */
    if (routine->assoc_scope != NULL_region_number &&
        define_now) {
      a_routine_ptr routine_to_call = routine;
      if (kind == (a_ctor_or_dtor_kind)cdk_deleting) {
        /* The deleting destructor has to call the complete-object
           destructor, not the subobject destructor, in case the deleting
           destructor is generated by us and the subobject destructor
           that's called is generated by g++.  (This has been observed.) */
        routine_to_call = alternate_entry_point(routine,
                                             (a_ctor_or_dtor_kind)cdk_complete,
                                                /*define_now=*/FALSE);
      }  /* if */
      if (new_routine->storage_class == (a_storage_class)sc_extern) {
        new_routine->storage_class = routine->storage_class;
      }  /* if */
      /* Set is_inline again because templates don't have a reliable value
         before they are defined. */
      set_inline_flag(new_routine, routine->is_inline);
      new_routine->suppress_inline_body = routine->suppress_inline_body;
      define_default_version_of_routine(routine_to_call, new_routine, 
                                        (an_expr_node_ptr)NULL);
#if LOWER_EXTERN_INLINE
      if (routine->use_comdat) {
        put_routine_into_comdat_group(new_routine);
      }  /* if */
#endif /* LOWER_EXTERN_INLINE */
    }  /* if */
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
  if (exceptions_enabled &&
      routine->special_kind == (a_special_function_kind)sfk_destructor &&
      routine->ctor_dtor_kind == (a_ctor_or_dtor_kind)cdk_subobject &&
      define_now) {
    /* cdk_delegation destructors are only needed when the class contains
       a delegating constructor and the class has virtual bases (i.e.,
       the primary routine is a cdk_subobject).  This isn't always
       known at the point where the destructor is lowered. */
    (void)alternate_entry_point(routine,
                                (a_ctor_or_dtor_kind)cdk_delegation,
                                define_now);
  }  /* if */
}  /* create_alternate_entry_points */

#endif /* IA64_ABI */

static void add_array_constructor_call(
                                   a_dynamic_init_ptr     dip,
                                   an_expr_node_ptr       entity_node,
                                   an_expr_node_ptr       source_node,
                                   an_init_pos_descr_ptr  ipdp,
                                   an_insert_location_ptr insert_location)
/*
Generate code that calls a constructor for each element of an array.
dip indicates the initialization to be performed; entity_node gives the
address of the array; source_node (if non-NULL) gives the address of
the source for a copy constructor call; and ipdp gives more information
on the destination (in particular, it gives the count of array elements).
Insert the statements at *insert_location and update *insert_location.
The additional-arguments list given by dip->variant.constructor.args
must NOT already be lowered (see comment in default_version_of_routine).
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
    call_node = make_vec_cctor_call(entity_node, source_node, ipdp,
                                    ctor_routine, dtor_routine);
  } else {
    /* Normal constructor case. */
    /* Build a node for the number of array elements. */
    num_elem_node = num_elem_node_if_array(ipdp);
    check_assertion(num_elem_node != NULL);
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
table table address that should be passed to the base class destructor
(IA-64 ABI only), or NULL if one is not needed.  Insert the statement
at *insert_location and update *insert_location.  The call generated
is not a virtual call even if the destructor is virtual.  Note: this
routine takes separate dtor_routine and ipdp parameters instead of a
dynamic init pointer because of the make_destruction_routine case.
*/
{
  an_expr_node_ptr entity_node, call_node, num_elem_node;
  an_expr_node_ptr implied_arg_list, dtor_addr_node;
  a_type_ptr       this_param_type;

  /* Make an expression for the object to be destroyed. */
  entity_node = make_address_of_init_entity_node(ipdp,
                                                 /*using_as_dest=*/FALSE);
  /* If the object is an array, make an expression node for the number
     of elements, or NULL if the object is not an array. */
  num_elem_node = num_elem_node_if_array(ipdp);
#if IA64_ABI
  if (dtor_routine != NULL) {
    dtor_routine = alternate_entry_point(dtor_routine, 
                                         (have_complete_object ? 
                                          (a_ctor_or_dtor_kind)cdk_complete :
                                          (a_ctor_or_dtor_kind)cdk_subobject),
                                         /*define_now=*/FALSE);
  }  /* if */
#endif /* IA64_ABI */
  /* Generate code for the destructor call. */
  if (num_elem_node != NULL) {
#if !IA64_ABI
    /* default_version_of_routine is not called on purpose; __vec_delete
       knows about the implicit argument for destructors and generates
       it automatically. */
#endif /* !IA64_ABI */
    /* Generate the __vec_delete call. */
    /* Build an expression for the address of the destructor. */
    dtor_addr_node = expr_for_pointer_to_destructor(dtor_routine);
    call_node = make_vec_delete_call(entity_node, num_elem_node,
                                     dtor_addr_node, (a_routine *)NULL,
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
    if (dtor_needs_vtt_argument(dtor_routine)) {
      check_assertion(vtt_addr_node != NULL);
      implied_arg_list = vtt_addr_node;
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
    make_call_statement(dtor_routine, entity_node, (an_expr_node_ptr)NULL,
                        insert_location);
  }  /* if */
}  /* add_destructor_call */


static void add_conditional_flag_test(a_variable_ptr         test_var,
                                      an_insert_location_ptr insert_location,
                                      an_insert_location_ptr insert_location2)
/*
Add a sequence of code that tests conditional flag set on initialization
of a variable.  This is used in deciding whether or not to call a
destructor on the variable.  The sequence is

    if (test_var) {
      ... destruction of variable being destroyed
    }

The sequence is inserted at *insert_location.  *insert_location is updated
for further insertion following the "if".  *insert_location2 is set for
insertion within the "if".
*/
{
  an_expr_node_ptr test_var_node;

  /* Make the boolean controlling expression "test_var". */
  test_var_node = var_rvalue_expr(test_var);
  test_var_node = boolean_controlling_expr(test_var_node);
  /* Make an "if" statement and insert it into the program. */
  insert_if_statement(test_var_node, /*is_initialization_guard=*/FALSE,
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
  check_assertion(dip->destructor != NULL);
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
  /* If the entity is a conditionally-created temporary, generate an
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
        reset_conditional_flag_var(dedp->conditional_flag_var,
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

#if VLA_DEALLOCATION_REQUIRED

void gen_vla_deallocation(a_dynamic_init_ptr dip,
                          an_insert_location *insert_location)
/*
dip points to a dynamic-init entry that represents the deallocation of
a variable-length array.  Generate code to do the deallocation.
The code is inserted at *insert_location and *insert_location is updated.
*/
{
  a_destructible_entity_descr_ptr dedp = dip->destructible_entity_descr;
  an_expr_node_ptr                dealloc_expr;

  check_assertion(dedp != NULL);
  /* Set the cleanup state to what it should be after the deallocation. */
  curr_context->curr_cleanup_state =
                          dedp->cleanup_state_to_set_when_starting_destruction;
  if (exceptions_enabled) {
    insert_code_to_indicate_cleanup_state(curr_context->curr_cleanup_state,
                                          insert_location,
                                          /*unreachable=*/FALSE);
  }  /* if */
  dealloc_expr = alloc_expr_node((an_expr_node_kind)enk_vla_dealloc);
  dealloc_expr->type = void_type();
  check_assertion(dip->variable != NULL);
  dealloc_expr->variant.vla_variable = dip->variable;
#if LOWER_VARIABLE_LENGTH_ARRAYS
  lower_vla_dealloc(dealloc_expr);
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
  (void)insert_expr_statement(dealloc_expr, insert_location);
}  /* gen_vla_deallocation */

#endif /* VLA_DEALLOCATION_REQUIRED */

static void lower_ck_dynamic_init(a_constant_ptr         con_ptr,
                                  an_init_pos_descr_ptr  ipdp,
                                  a_boolean              dtor_case,
                                  an_implied_copy_source *source_desc,
                                  a_boolean              others_follow_in_aggr,
                                  an_insert_location_ptr insert_location,
                                  a_boolean              *keep_constant,
                                  a_lower_dynamic_init_options_set
                                                         options)
/*
Generate executable code to handle a ck_dynamic_init constant (pointed
to by con_ptr).  The entity to be initialized is described by ipdp.
The necessary statements are inserted at *insert_location and
*insert_location is updated.  If ipdp->array_element_sequence is TRUE,
this call is handling a sequence of elements in an array.  If dtor_case
is TRUE, we are generating a destructor wrapper; do the destruction
indicated in the dynamic init but ignore any initialization.  If the dynamic
initialization is part of an implied copy, source_desc describes the source of
that copy.  others_follow_in_aggr is TRUE if this constant is followed by
others in an aggregate initialization (i.e., it's not the last).  If the
initialization is of an aggregate and there some parts of the initialization
that are constant, the ck_dynamic_init constant will be changed to an aggregate
constant for the constant parts and *keep_constant will be set to TRUE.
*keep_constant is also set to TRUE if the ck_dynamic_init is used to
initialize an element of a vector.  Individual vector elements cannot be
individually assigned, so they must remain as part of the aggregate
initializer.  options is a bit mask specifying any special treatment of this
initialization (e.g., whether this initialization represents a full
expression).
*/
{
  a_constant_ptr     next_con;
  a_type_ptr         desired_type;
  a_constant_ptr     constant_to_keep = NULL;
  a_dynamic_init_ptr dip = con_ptr->variant.dynamic_init;
  a_boolean          need_copy_to_file_scope = FALSE;
#if GNU_VECTOR_TYPES_ALLOWED
  a_boolean          keep_dynamic_init = FALSE;
#endif /* GNU_VECTOR_TYPES_ALLOWED */

  if (processing_file_scope_init_routine && in_file_scope(dip) &&
      dip->destruction_is_for_partially_constructed_aggregate) {
    /* The subtree of this dynamic initialization will be copied into the
       function scope memory region because the code for it must be generated
       in a startup initialization routine.  Copy all destructions that
       involve partially constructed aggregates.  The copy is done at this
       level so that any overlap is indicated properly on the function-scope
       copies so the cleanup lists will be right. */
    dip = copy_dynamic_init(dip, CE_UNLINK_SOURCE_DESTRUCTIONS |
                                 CE_TRANSFER_DESTR_ENTITY_DESCR);
    /* If any constants were copied above, they were copied into the function
       scope.  Make sure we copy them back into the file scope. */
    need_copy_to_file_scope = TRUE;
  }  /* if */
  if (dtor_case) {
    /* In a destructor case, so the "initialization" is really
       destruction. */
    lower_destructor_dynamic_init(dip, ipdp, /*have_complete_object=*/TRUE,
                                  (an_expr_node_ptr)NULL,
                                  insert_location);
  } else {
    /* Normal initialization. */
    lower_dynamic_init(dip, ipdp, source_desc, (a_variable_ptr)NULL,
                       options, others_follow_in_aggr,
                       insert_location, 
#if GNU_VECTOR_TYPES_ALLOWED
                       &keep_dynamic_init,
#else /* !GNU_VECTOR_TYPES_ALLOWED */
                       (a_boolean *)NULL,
#endif /* GNU_VECTOR_TYPES_ALLOWED */
                       &constant_to_keep);
  }  /* if */
  if (constant_to_keep != NULL) {
    /* There's a constant part of the initialization that needs to be
       kept.  Replace the ck_dynamic_init constant with that constant. */
    a_constant_ptr con_ptr_next = con_ptr->next;
    if (need_copy_to_file_scope) {
      /* This constant is the remnant of a dynamic init that was copied
         to the function scope.  The rest of the constant is in the file
         scope, so make sure any pieces that were copied above are
         (re-)copied to file scope. */
      a_memory_region_number region_to_switch_back_to = NULL_region_number;
      check_assertion(in_file_scope(con_ptr));
      switch_to_file_scope_region(&region_to_switch_back_to);
      (void)copy_constant_full(constant_to_keep, con_ptr, CE_NO_OPTIONS);
      switch_back_to_original_region(region_to_switch_back_to);
    } else {
      copy_constant(constant_to_keep, con_ptr);
    }  /* if */
    con_ptr->next = con_ptr_next;
    *keep_constant = TRUE;
#if GNU_VECTOR_TYPES_ALLOWED
  } else if (keep_dynamic_init) {
    /* A dik_expression dynamic initialization has been kept in the
       aggregate initializer.  Signal to our caller that the dip needs to be
       kept around (and that the type of dip needs to be left as
       dik_nonconstant_aggregate). */
    check_assertion(initializing_vector_element(ipdp->modifiers));
    *keep_constant = TRUE;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
  } else {
    /* Overwrite the constant with a harmless constant of the right kind.
       It's just a place-holder that gets overwritten by the dynamic
       initialization. */
    desired_type = ipdp->modifiers->type;
#if DO_C99_IL_LOWERING
    if (C_mode()) {
#if LOWER_COMPLEX && C99_IL_EXTENSIONS_SUPPORTED
      if (is_imaginary_type(desired_type)) {
        /* In C99, create a float constant for an imaginary type. */
        desired_type = skip_typerefs(desired_type);
        desired_type = float_type(desired_type->variant.float_kind);
      } else
#endif /* LOWER_COMPLEX && C99_IL_EXTENSIONS_SUPPORTED */
      /* Do not insert code here. */
#if LOWER_FIXED_POINT
      if (is_fixed_point_type(desired_type)) {
        /* A fixed-point type becomes an integer. */
        desired_type=lowered_integer_type_for_fixed_point_type(desired_type);
      } else
#endif /* LOWER_FIXED_POINT */
      /* Do not insert code here. */
      {
        /* Nothing to be done. */
      }  /* if */
    } else
#endif /* DO_C99_IL_LOWERING */
    /* Do not insert code here. */
    {
      /* C++ mode. */
      /* For pointers to members, switch to the implementation type. */
      if (is_or_was_ptr_to_data_member_type(desired_type)) {
        desired_type = integer_type(targ_ptr_to_data_member_int_kind);
      } else if (is_or_was_ptr_to_member_function_type(desired_type)) {
        desired_type = make_mptr_type();
      }  /* if */
    }  /* if */
    if (is_aggregate_or_union_type(desired_type)
#if GNU_VECTOR_TYPES_ALLOWED
        || is_vector_type(desired_type)
#endif /* GNU_VECTOR_TYPES_ALLOWED */
#if DO_C99_IL_LOWERING && LOWER_COMPLEX
        || is_complex_type(desired_type)
#endif /* DO_C99_IL_LOWERING && LOWER_COMPLEX */
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
      make_zero_of_proper_type(desired_type, con_ptr);
      con_ptr->next = next_con;
    }  /* if */
  }  /* if */
}  /* lower_ck_dynamic_init */


static void lower_dynamic_init_aggregate_constant(
                          a_constant_ptr         aggr_const,
                          an_init_pos_descr_ptr  ipdp,
                          a_boolean              dtor_case,
                          an_implied_copy_source *source_desc,
                          a_boolean              others_follow_in_aggr,
                          an_insert_location_ptr insert_location,
                          a_boolean              *contains_vector_dynamic_init,
                          a_boolean              *keep_constant,
                          a_lower_dynamic_init_options_set
                                                 options)
/*
aggr_const points to a ck_aggregate constant that contains one or more
ck_dynamic_init dynamic initializations.  The ck_aggregate constant is
the initial value for the entity described by ipdp.  If dtor_case is TRUE,
we are generating a destructor wrapper; do the destruction indicated in
the aggregate init but ignore any initialization.  If the dynamic
initialization is part of an implied copy, source_desc describes the source
of that copy.  others_follow_in_aggr is TRUE if this constant
is followed by others in an aggregate initialization (i.e., it's not the
last).  Insert statements to implement the initialization at *insert_location
and update *insert_location.  If contains_vector_dynamic_init is non-NULL,
set *contains_vector_dynamic_init to TRUE if aggr_con is a vector that
contains a dynamic initialization for a vector element, FALSE otherwise.
If there are any (genuine) constants in the aggregate, set *keep_constant to
TRUE.  options is a bit mask specifying any special treatment of this
initialization (e.g., whether this initialization represents a full
expression).
*/
{
  an_init_pos_descr    ipd;
  an_init_pos_modifier ipm, *ipmp;
  a_type_ptr           aggr_type;
  a_constant_ptr       con_ptr, repeated_con, prev_con, next_con;
  a_boolean            array_aggr, array_or_vector = FALSE;

  if (contains_vector_dynamic_init != NULL) {
    *contains_vector_dynamic_init = FALSE;
  }  /* if */
  /* Mark the constant as visited.  This is necessary if the aggregate
     constant ends up being kept because something constant remains after
     the non-constant parts have been rewritten. */
  mark_as_visited(aggr_const);
  /* Determine the type of the aggregate being initialized. */
  aggr_type = type_from_init_pos_descr(ipdp);
  aggr_type = skip_typerefs(aggr_type);
#if LOWER_COMPLEX
  if (is_complex_type(aggr_type)) {
    /* GNU allows initializer-list style initialization of complex objects,
       e.g., "__complex float z {x, 1.0};".  Complex types are typically
       maintained through the lowering process and changed to their lowered
       types at the end of lowering the file scope, but here we need the
       lowered type (because the constant contains some type of dynamic
       initialization that needs to be rewritten as executable code).
       Use the lowered complex type (a struct with an array of two elements
       of the appropriate type) and also change the aggregate constant to
       match the lowered form. */
    check_assertion(is_complex_type(aggr_const->type));
    aggr_type = lowered_complex_type(aggr_type->variant.float_kind);
    lower_c99_complex_aggregate_constant(aggr_const);
  }  /* if */
#endif /* LOWER_COMPLEX */
  /* Start a new level in the init_pos_modifier chain. */
  ipd = *ipdp;
  if (!C_mode()) {
    /* In C++ mode, remove field selections for anonymous union parent
       objects.  They will be put back in later.  See the comment in
       au_field_lvalue_selection_expr for an explanation. */
    ipmp = ipd.modifiers;
    if (ipmp != NULL && ipmp->curr_field != NULL &&
        ipmp->curr_field->is_anonymous_parent_object) {
      ipd.modifiers = ipmp->next;
    }  /* if */
  }  /* if */
  ipmp = &ipm;
  add_init_pos_modifier(ipmp, &ipd);
  /* Determine the type of the first element of the aggregate being
     initialized. */
  array_aggr = (aggr_type->kind == (a_type_kind)tk_array);
  if (array_aggr) {
    /* Array -- get the element type. */
    ipmp->curr_elem = 0;
    ipmp->type = aggr_type->variant.array.element_type;
    array_or_vector = TRUE;
#if GNU_VECTOR_TYPES_ALLOWED
  } else if (aggr_type->kind == (a_type_kind)tk_vector) {
    /* Vector.  This is similar to the array case. */
    ipmp->curr_elem = 0;
    ipmp->type = aggr_type->variant.vector.element_type;
    ipmp->is_vector_element = TRUE;
    array_or_vector = TRUE;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
  } else {
    check_assertion_str(is_immediate_class_type(aggr_type),
                       "lower_dynamic_init_aggregate_constant: bad aggr kind");
    /* Class, struct, or union -- get first field (nonstatic data member). */
    /* Adjust the aggregate constant so that it agrees with the lowered
       type of the aggregate. */
    prelower_aggregate_constant(aggr_const);
    ipmp->curr_field = next_initializable_field(
                             aggr_type->variant.class_struct_union.field_list);
  }  /* if */
  con_ptr = aggr_const->variant.aggregate.first_constant;
  /* Work through the list of constants, pairing each one with a member of
     the aggregate. */
  for (prev_con = NULL;
       con_ptr != NULL;
       prev_con = con_ptr, con_ptr = next_con) {
    a_boolean others_follow;
    if (con_ptr->kind == (a_constant_repr_kind)ck_designator) {
      /* A designator appears (e.g., in a C99 nonconstant aggregate
         initialization).  Update the current position. */
      if (con_ptr->variant.designator.field != NULL) {
        check_assertion(!array_or_vector);
        ipmp->curr_field = con_ptr->variant.designator.field;
      } else {
        check_assertion(array_aggr);
        ipmp->curr_elem = con_ptr->variant.designator.array_element;
      }  /* if */
      con_ptr = con_ptr->next;
      check_assertion(con_ptr != NULL &&
                      con_ptr->kind != (a_constant_repr_kind)ck_designator);
    }  /* if */
    next_con = con_ptr->next;
    others_follow = (others_follow_in_aggr || next_con != NULL);
    if (!array_or_vector) {
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
      lower_ck_dynamic_init(con_ptr, &ipd, dtor_case, source_desc,
                            others_follow, insert_location, keep_constant,
                            options);
#if GNU_VECTOR_TYPES_ALLOWED
      if (aggr_type->kind == (a_type_kind)tk_vector) {
        /* This is a dynamic initialization of an element of a vector type,
           which is always kept in the constant.  Signal to our caller that
           the dip that contains this should be a dik_nonconstant_aggregate
           rather than a dik_constant. */
        check_assertion(*keep_constant &&
                        contains_vector_dynamic_init != NULL);
        *contains_vector_dynamic_init = TRUE;
      }  /* if */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
    } else if (con_ptr->kind == (a_constant_repr_kind)ck_init_repeat) {
      /* Repeated constant.  Must be initializing members of an array. */
      check_assertion_str(array_aggr,
                 "lower_dynamic_init_aggregate_constant: repeat on non-array");
      repeated_con = con_ptr->variant.init_repeat.constant;
      /* Repeat the constant the right number of times. */
      if (repeated_con->kind != (a_constant_repr_kind)ck_dynamic_init) {
        check_assertion(designators_allowed ||
                        repeated_con->is_result_of_constexpr_call);
#if DO_C99_IL_LOWERING
        if (c99_mode || gcc_mode) {
          lower_c99_constant(repeated_con);
        } else
#endif /* DO_C99_IL_LOWERING */
        {
          lower_constant(repeated_con);
        }  /* if */
        if (ipd.indirect_through_variable) {
          /* The entity being initialized is not a simple variable, so we
             don't want to keep any part of the initialization as a constant
             aggregate initialization.  This can occur when initializing
             a (portion of a) variably-sized array with a constant value
             (from a constexpr constructor).  Generate executable statements
             to perform the initialization. */
          an_expr_node_ptr entity_node;
          ipd.array_element_sequence = TRUE;
          ipd.array_element_type = repeated_con->type;
          if (con_ptr->variant.init_repeat.count == 0) {
            /* If the repeat count is zero, this initialization is being used
               to complete a partial-initialization of a variably-sized array.
               Make a note of the starting element that needs initialization
               (which could be zero, in cases like "new A[n] {}"). */
            check_assertion(ipd.num_elem_node != NULL);
            ipd.partial_initialization_starting_element = ipmp->curr_elem;
            if (is_array_type(array_element_type(aggr_type))) {
              /* For the multi-dimensional array case, ensure that the
                 starting element takes into account all of the elements
                 that have already been initialized. */
              ipd.partial_initialization_starting_element *=
                             num_array_elements(array_element_type(aggr_type));
            }  /* if */
          } else {
            ipd.array_element_count =
                          (a_targ_ptrdiff_t)con_ptr->variant.init_repeat.count;
          }  /* if */
          entity_node = make_init_entity_node(&ipd,
                                              /*result_is_lvalue=*/TRUE,
                                              /*using_as_dest=*/TRUE);
          repeated_con->next = NULL;
          add_init_assignment((a_dynamic_init *)NULL, repeated_con,
                              entity_node,
                              /*have_complete_object=*/FALSE,
                              insert_location,
                              /*is_lambda_capture=*/FALSE,
                              &ipd);
        } else {
          /* Repeated non-dynamic constants are possible with designators and
             when constexpr default constructors are folded.  Either way, leave
             leave it alone, except for lowering the underlying constant.  This
             comes up in C mode when IL lowering is used to lower nonconstant
             initializers.  (However, the repeated constant will be actually
             constant.) */
          *keep_constant = TRUE;
        }  /* if */
      } else {
        /* Repeated ck_dynamic_init constant. */
        check_assertion(!C_mode());
        ipd.array_element_sequence = TRUE;
        ipd.array_element_type = repeated_con->type;
        if (con_ptr->variant.init_repeat.count == 0) {
          /* If the repeat count is zero, this initialization is being used
             to complete a partial-initialization of a variably-sized array.
             Make a note of the starting element that needs initialization
             (which could be zero, in cases like "new A[n] {}"). */
          check_assertion(ipd.num_elem_node != NULL);
          ipd.partial_initialization_starting_element = ipmp->curr_elem;
          if (is_array_type(array_element_type(aggr_type))) {
            /* For the multi-dimensional array case, ensure that the
               starting element takes into account all of the elements
               that have already been initialized. */
            ipd.partial_initialization_starting_element *=
                             num_array_elements(array_element_type(aggr_type));
          }  /* if */
        } else {
          ipd.array_element_count =
                          (a_targ_ptrdiff_t)con_ptr->variant.init_repeat.count;
        }  /* if */
        lower_ck_dynamic_init(repeated_con, &ipd, dtor_case, source_desc,
                              others_follow, insert_location, keep_constant,
                              options);
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
                                            dtor_case, source_desc,
                                            others_follow, insert_location,
                                            contains_vector_dynamic_init,
                                            keep_constant, options);
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
                                            /*result_is_lvalue=*/TRUE,
                                            /*using_as_dest=*/TRUE);
        con_ptr->next = NULL;
        add_init_assignment((a_dynamic_init *)NULL, con_ptr, entity_node,
                            /*have_complete_object=*/FALSE,
                            insert_location,
                            /*is_lambda_capture=*/FALSE,
                            &ipd);
      } else {
        /* Normal case.  Keep this as part of a constant aggregate. */
        *keep_constant = TRUE;
      }  /* if */
    }  /* if */
    /* Find the next member in the aggregate. */
    if (array_or_vector) {
      /* Array or vector -- go on to next element. */
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
    if (!array_or_vector) {
      /* If we're initializing fields of a lambda closure object,
         advance the source of an implied copy to the next variable in
         the capture list. */
      advance_to_next_lambda_capture_if_necessary(source_desc);
    }  /* if */
    /* Loop while there are more constants. */
  }  /* for */
}  /* lower_dynamic_init_aggregate_constant */

#if USE_PATCH_INIT_STARTUP

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
    linkl_type = make_lowered_class_type((a_type_kind)tk_struct);
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

#endif /* USE_PATCH_INIT_STARTUP */

#if !ONE_INSTANTIATION_PER_OBJECT
/*ARGSUSED*/ /* <-- needed_bit_number is not used in that case. */
#endif /* !ONE_INSTANTIATION_PER_OBJECT */
#if !GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
/*ARGSUSED*/ /* <-- init_priority is not used in that case. */
#endif /* !GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
#if !SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
/*ARGSUSED*/ /* <-- unique_id is not used in that case. */
#endif /* !SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */
static a_scope_ptr make_file_scope_init_or_term_routine(
                                 a_type_ptr                  param1_type,
                                 unsigned long               needed_bit_number,
                                 int                         init_priority,
                                 a_const_char                *prefix,
                                 unsigned long               unique_id,
                                 a_boolean                   do_thread_local,
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
state for later restoration.  When do_thread_local is TRUE, the routine
is being created for (one or more) thread_local dynamic initialization(s).
*/
#if ONE_INSTANTIATION_PER_OBJECT
/*
needed_bit_number, if non-zero, indicates a per-instantiation "needed"
bit number; each instantiation is being put in a separate file, and this
initialization routine is being generated for the instantiation associated
with the indicated bit number.  When do_thread_local is TRUE, all
thread_local initializations for this slice are included in the
initialization routine (which is given a unique name for the slice, i.e.,
__tls_init__N where N is the needed_bit_number).
*/
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
/*
If init_priority is non-zero, this routine is an initialization routine
for variables with the GNU init_priority set to that value.
*/
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
#if SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
/*
If unique_id is non-zero, this routine is an initialization routine for a
specific variable and unique_id is added to the routine's name to ensure
that the routine name is differentiated from other routines.  This can
be combined with needed_bit_number and/or init_priority specified above.
When do_thread_local is TRUE, separate initialization routines are created
for each thread_local dynamic initialization.  The separate initialization
routines are queued on lists (one for thread_local initializations and
one for non-thread_local initializations) in the IL header so a back end
can easily access them.
*/
#endif /* SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */
{
  a_routine_ptr   init_rout;
  a_scope_ptr     scope;
  char            *name;
  sizeof_t        prefix_len, alloc_length;
  a_statement_ptr return_stmt;
  a_storage_class storage_class;
#if ONE_INSTANTIATION_PER_OBJECT
  char            buffer[50];
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
  char            buffer2[50];
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
#if SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
  char            buffer3[50];
#endif /* SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */

  if (do_thread_local) {
#if !IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS
    unexpected_condition();
#endif /* !IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS */
#if USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES || \
    !SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
    /* Create a static __tls_init routine to contain all of the thread_local
       initializations. */
    name = (char *)"__tls_init";
    storage_class = (a_storage_class)sc_static;
#if ONE_INSTANTIATION_PER_OBJECT
    if (needed_bit_number != 0) {
      /* Add a suffix to distinguish initialization routines for
         specific instantiations. */
      (void)sprintf(buffer, "__%lu", needed_bit_number);
      prefix = name;
      prefix_len = strlen(prefix);
      alloc_length = prefix_len + strlen(buffer) + 1;
      name = alloc_lowered_name_string(alloc_length);
      (void)memcpy(name, prefix, size_t_arg(prefix_len));
      (void)strcpy(&name[prefix_len], buffer);
    }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#else /* !(USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES || !...) */
    /* Create separate routines for each thread_local initialization.  The
       names of these routines are unimportant (they will be referred to
       from the il_header). */
    name = NULL;
    storage_class = (a_storage_class)sc_unspecified;
#endif /* USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES || !... */
  } else if (prefix == NULL) {
    /* Make an unnamed static routine. */
    name = NULL;
    storage_class = (a_storage_class)sc_static;
  } else {
    /* Combine the prefix and an identifier for the current module to make
       a name that is likely to be unique. */
    a_const_char *module_id;
    char         *end;
    storage_class = (a_storage_class)sc_unspecified;
    module_id = get_module_id();
    check_assertion(module_id != NULL);
    prefix_len = strlen(prefix);
    alloc_length = prefix_len + strlen(module_id) + 1;
#if ONE_INSTANTIATION_PER_OBJECT
    if (needed_bit_number != 0) {
      /* Add a suffix to distinguish initialization routines for
         specific instantiations. */
      (void)sprintf(buffer, "__%lu", needed_bit_number);
      alloc_length += strlen(buffer);
    }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
    if (init_priority != 0) {
      /* Add a suffix to distinguish initialization routines for
         specific init_priority values. */
      (void)sprintf(buffer2, "__prio%d", (int)init_priority);
      alloc_length += strlen(buffer2);
    }  /* if */
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
#if SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
    if (unique_id != 0) {
      /* Add a suffix to distinguish initialization routines for
         specific init_priority values. */
      (void)sprintf(buffer3, "__%lu", unique_id);
      alloc_length += strlen(buffer3);
    }  /* if */
#endif /* SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */
    name = alloc_lowered_name_string(alloc_length);
    (void)memcpy(name, prefix, size_t_arg(prefix_len));
    end = name + prefix_len;
    (void)strcpy(end, module_id);
    end += strlen(module_id);
#if ONE_INSTANTIATION_PER_OBJECT
    if (needed_bit_number != 0) {
      (void)strcpy(end, buffer); /*lint !e645*/
      end += strlen(buffer);
    }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
    if (init_priority != 0) {
      (void)strcpy(end, buffer2); /*lint !e645*/
      end += strlen(buffer2);
    }  /* if */
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
#if SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
    if (unique_id != 0) {
      (void)strcpy(end, buffer3); /*lint !e645*/
      end += strlen(buffer3);
    }  /* if */
#endif /* SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */
  }  /* if */
  /* Make a type and routine entry for the routine. */
  init_rout = make_rout_entry(name, storage_class, void_type(), param1_type);
  if (do_thread_local) {
    init_rout->source_corresp.name_has_been_mangled = TRUE;
    init_rout->is_tls_init_routine = TRUE;
  }  /* if */
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
  if (init_priority != 0) init_rout->init_priority = init_priority;
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
  /* Make a memory region, scope, and block for the routine definition. */
  scope = make_routine_definition(init_rout, /*make_return=*/TRUE, il_region);
  /* Save the current state and push a new context for the generated
     routine. */
  push_generated_routine_context(scope, *il_region, grcontext);
#if IA64_ABI
  if (param1_type != NULL) {
    scope->variant.routine.parameters = 
                                      make_lowered_param_variable(param1_type);
  }  /* if */
#endif /* IA64_ABI */
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
                                 int                         init_priority,
                                 unsigned long               unique_id,
                                 a_boolean                   do_thread_local,
                                 an_insert_location_ptr      insert_location,
                                 a_memory_region_number      *region_number,
                                 a_generated_routine_context *grcontext)
/*
Create a file-scope initialization routine.  Set *insert location so it
can be used to insert code in that routine, and set *region_number to
the memory region number for the routine.  Return the scope for the routine.
A generated routine context is pushed, with *grcontext used to save the
old state for later restoration.  If needed_bit_number is non-zero, it
is the per-instantiation "needed" bit number associated with an instantiation,
and the routine being generated is the initialization routine for that
instantiation.  If init_priority is non-zero, this routine is an
initialization routine for variables with the GNU init_priority
set to that value.  If unique_id is non-zero, the name of the generated
initialization routine will contain a representation of this value.
If do_thread_local is TRUE, the routine will contain only dynamic
initializations of thread_local variables.
*/
{
  a_scope_ptr scope = make_file_scope_init_or_term_routine(
                                       (a_type_ptr)NULL,
                                       needed_bit_number,
                                       init_priority,
                                       do_thread_local ?
                                               NULL :
                                               IL_LOWERING_INIT_ROUTINE_PREFIX,
                                       unique_id,
                                       do_thread_local,
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
                                       0,
                                       (char *)NULL,  /* Unnamed. */
                                       (unsigned long)0,
                                       /*do_thread_local=*/FALSE,
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
                                        (an_expr_operator_kind)eok_assign,
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
                            node_for_integer_constant(0L,
                                                      (an_integer_kind)ik_int),
                            insert_location);
}  /* reset_conditional_flag_var */


static a_dynamic_init_ptr adjust_cleanup_state_for_inner_lifetime_temporaries(
                                                   a_dynamic_init_ptr temp_dip,
                                                   a_dynamic_init_ptr dip)
/*
dip points to a destruction for an entity that is created while
destructible temporaries in an inner object lifetime are still in
existence.  temp_dip points to the destruction for one of those
temporaries, or NULL if we've reached the end of the list.  Update the
region table information for the temporary and those following it in
its object lifetime so that the cleanup list includes the temporaries
and then the outer-lifetime entity.  Note that some entries on the
list may be ones indicating freeing of storage on exceptions, rather
than temporaries in the strict sense.  Also, there may be
partial-aggregate cleanups in the inner lifetime, and those should be
ignored.  Return a pointer to the first destruction for a "real"
temporary on the list, or NULL if only partial-aggregate cleanup
entries were seen.
*/
#if GENERATE_EH_TABLES
/*
This may involve cloning some of the region table entries for the
temporaries (but not any for partially constructed aggregates), since
currently the last temporary points past the outer-lifetime entity to
the next thing to be destroyed after that.  The region table entry for
dip has already been created.
*/
#endif /* GENERATE_EH_TABLES */
{
  a_dynamic_init_ptr first_real_temp = NULL;

#if GENERATE_EH_TABLES
  { a_dynamic_init_ptr              orig_temp_dip = temp_dip;
    a_destructible_entity_descr_ptr dedp;
    a_dynamic_init_ptr              next_dip;
    /* Skip over any partial-aggregate destructions on the list. */
    while (temp_dip != NULL &&
           temp_dip->destruction_is_for_partially_constructed_aggregate) {
      temp_dip = temp_dip->destructible_entity_descr->next_in_region_table;
    }  /* while */
    /* If we ran off the list, do nothing and return NULL. */
    if (temp_dip == NULL) goto end_of_routine;
    first_real_temp = temp_dip;
    dedp = temp_dip->destructible_entity_descr;
    /* Find and process the next real temporary following this one. */
    next_dip = dedp->next_in_region_table;
    next_dip = adjust_cleanup_state_for_inner_lifetime_temporaries(next_dip,
                                                                   dip);
    /* Link this temp destruction to the next real temp destruction, if any. */
    dedp->next_in_region_table = next_dip;
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
    if (orig_temp_dip != temp_dip->lifetime->destructions) {
      clone_region_table_entry_list(temp_dip, next_dip);
    }  /* if */
  }
#else /* !GENERATE_EH_TABLES */
  /* Find the last destruction entry for a temporary and reset its
     cleanup_state_to_set_when_starting_destruction to dip.
     Partial-aggregate cleanups stay on the list, so they are treated like
     any other entries. */
  if (temp_dip == NULL) goto end_of_routine;
  { a_dynamic_init_ptr last_dip;
    a_destructible_entity_descr_ptr last_dedp;
    for (last_dip = temp_dip;
         last_dip->next_in_destruction_list != NULL;
         last_dip = last_dip->next_in_destruction_list) {
      if (!last_dip->destruction_is_for_partially_constructed_aggregate &&
          first_real_temp == NULL) {
        first_real_temp = last_dip;
      }  /* if */
    }  /* for */
    last_dedp = last_dip->destructible_entity_descr;
    last_dedp->cleanup_state_to_set_when_starting_destruction = dip;
  }
#endif /* GENERATE_EH_TABLES */
end_of_routine:
  curr_context->latest_initialization = temp_dip;
  set_curr_cleanup_state_to_latest_initialization();
  return first_real_temp;
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
      a_dynamic_init_ptr temp_dip;
      check_assertion_str(curr_context != context,
                          "add_dyn_init_cleanup: curr_context == context");
      temp_dip = adjust_cleanup_state_for_inner_lifetime_temporaries(
                                           curr_context->latest_initialization,
                                           dip);
      if (temp_dip != NULL) {
        if (temp_dip->lifetime->destructions == temp_dip) {
          /* There's no need to emit code to set the cleanup state here: it's
             not necessary because the cleanup state will be set in a moment
             when the destruction of the last temporary begins.  If we were to
             try to set the cleanup state here, we would be referring to the
             region table for that last temporary, which was not cloned because
             it's not needed. */
        } else {
          /* We have something like multiple outer-lifetime initializations
             that overlap with the same set of inner-lifetime temporaries,
             so we will not be immediately destroying the temporaries and
             we need to set the cleanup state to the last-constructed of the
             temporaries. */
          check_assertion(curr_context->curr_cleanup_state == temp_dip);
          insert_code_to_indicate_cleanup_state(
                                              curr_context->curr_cleanup_state,
                                              insert_location,
                                              /*unreachable=*/FALSE);
        }  /* if */
      } else {
        /* If there was nothing in the inner lifetime, or nothing but partial
           aggregate cleanups, we do need to set the cleanup state here,
           because there will not be any destructions of temporaries in the
           inner lifetime. */
        insert_code_to_indicate_cleanup_state(context->curr_cleanup_state,
                                              insert_location,
                                              /*unreachable=*/FALSE);
      }  /* if */
#if !GENERATE_EH_TABLES
      /* Insert an leck_initialization_completed node that indicates the
         point at which the initialization has been done. */
      { an_expr_node_ptr node = alloc_lowered_eh_construct_node(
                   (a_lowered_eh_construct_kind)leck_initialization_completed);
        node->variant.lowered_eh.variant.dynamic_init = dip;
        (void)insert_expr_statement(node, insert_location);
      }
#endif /* !GENERATE_EH_TABLES */
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
    needed_destruction_type = make_lowered_class_type((a_type_kind)tk_struct);
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
needed call of a destructor at process termination and at thread
termination.  NULL until created.
*/
static a_routine_ptr
		record_needed_destruction_routine,
                record_needed_thread_destruction_routine;

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
time of thread or program termination as appropriate.  Insert any generated
code at *insert_location and update *insert_location accordingly.
*/
{
#if IA64_ABI
  an_expr_node_ptr       dtor_node, dso_handle_node;
#else /* !IA64_ABI */
  a_variable_ptr         var;
  a_constant_ptr         aggr_con, next_con;
#endif /* IA64_ABI */
  a_constant_ptr         object_con, dtor_con;
  a_boolean              complex_cleanup, complex_address;
  a_routine_ptr          dtor_routine;
  an_expr_node_ptr       object_node, call_node;
  a_type_ptr             entity_type = type_from_init_pos_descr(ipdp);

  check_assertion(dip->destructor != NULL);
#if LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS
  if (call_to_ctor_or_dtor_has_no_effect(dip->destructor,
                                         (an_expr_node_ptr)NULL,
                                         /*call_can_be_virtual=*/FALSE)) {
#if DEBUG
    if (db_flag_is_set("remove_ctors_dtors")) {
      (void)fprintf(f_debug, "Removing static destruction for: ");
      db_dynamic_initializer(dip, 0);
    }  /* if */
#endif /* DEBUG */
    /* There's no need to call the destructor; skip the destruction processing
       altogether. */
    goto remove_from_list;
  }  /* if */
#endif /* LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS */
  /* Record the required destruction by generating a call of the runtime
     routine __record_needed_destruction or __record_needed_thread_destruction
     (as appropriate).  A data structure passed to the routine describes the
     destruction to be done:

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
  /* For the IA-64 ABI, the runtime routine is __cxa_atexit for
     destruction at the end of the process and __cxa_thread_atexit for
     destruction at the end of the thread. */
  complex_cleanup = ipdp->array_element_sequence ||
                    is_array_type(entity_type);
  /* Compute the object address (instead of doing static initialization to
     the address) if it is more than a simple variable.  The address of
     a thread_local variable isn't a constant at compile-time, so it's also
     considered "complex". */
  complex_address = ipdp->indirect_through_variable ||
                    ipdp->variable->is_thread_local ||
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
#if IA64_ABI
  if (!complex_cleanup && complex_address) {
    /* For simple cleanup with a complex address, get an rvalue pointer for
       the object to be destroyed at exit.  This will be used as the second
       argument to the runtime routine. */
    object_node = make_address_of_init_entity_node(ipdp,
                                                   /*using_as_dest=*/FALSE);
  } else {
    object_node = alloc_node_for_constant(object_con);
  }  /* if */
  dtor_node = alloc_node_for_constant(dtor_con);
  if (dso_handle_var == NULL) {
    /* Make the hidden variable that identifies the current DSO, i.e. it
       discriminates between user code and dynamically loaded libraries. */
    dso_handle_var = make_lowered_variable("__dso_handle",
                                           /*already_il_name=*/FALSE,
                                           void_star_type(),
                                           (a_storage_class)sc_extern);
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
    dso_handle_var->ELF_visibility = (an_ELF_visibility_kind)evk_hidden;
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
  }  /* if */
  dso_handle_node = var_addr_expr(dso_handle_var);
  dtor_node->next = object_node;
  object_node->next = dso_handle_node;
  /* Make a call of __cxa_atexit or __cxa_thread_atexit as appropriate.
     Their arguments are the expressions created above. */
  if (ipdp->variable != NULL && ipdp->variable->is_thread_local) {
    call_node = make_runtime_rout_call("__cxa_thread_atexit", 
                                     &record_needed_thread_destruction_routine,
                                     integer_type((an_integer_kind)ik_int),
                                     dtor_node);
  } else {
    call_node = make_runtime_rout_call("__cxa_atexit", 
                                       &record_needed_destruction_routine,
                                       integer_type((an_integer_kind)ik_int),
                                       dtor_node);
  }  /* if */
#else /* !IA64_ABI */
  implicit_cast(dtor_con, make_vptp_type());
  /* Link the aggregate constant together. */
  aggr_con->variant.aggregate.first_constant = next_con;
  next_con->next = object_con;
  object_con->next = dtor_con;
  aggr_con->variant.aggregate.last_constant = dtor_con;
  if (!complex_cleanup && complex_address) {
    /* For simple cleanup with a complex address, get an rvalue pointer for
       the object and store it in the object field of the struct. */
    an_expr_node_ptr field_node;
    a_statement_ptr  assign_stmt;
    object_node = make_address_of_init_entity_node(ipdp,
                                                   /*using_as_dest=*/FALSE);
    check_assertion(is_pointer_type(object_node->type));
    object_node = add_cast_if_necessary(object_node,
                                        needed_destruction_object_field->type);
    field_node = field_lvalue_selection_expr(var_lvalue_expr(var),
                                             needed_destruction_object_field);
    assign_stmt = insert_assignment_statement(field_node,
                                            (an_expr_operator_kind)eok_assign,
                                              object_node,
                                              insert_location);
    set_stmt_pos_to_code_pos_for_lowering(assign_stmt);
  }  /* if */
  /* Make a call of __record_needed_destruction or
     __record_needed_thread_destruction as appropriate.  Their argument is the
     address of the structure variable created above. */
  if (ipdp->variable != NULL && ipdp->variable->is_thread_local) {
    call_node = make_runtime_rout_call("__record_needed_thread_destruction",
                                     &record_needed_thread_destruction_routine,
                                     void_type(), var_addr_expr(var));
  } else {
    call_node = make_runtime_rout_call("__record_needed_destruction",
                                       &record_needed_destruction_routine,
                                       void_type(), var_addr_expr(var));
  }  /* if */
#endif /* IA64_ABI */
  /* Make a statement containing the call and insert it at the right
     location. */
  (void)insert_expr_statement_set_pos(call_node, insert_location);
#if LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS
remove_from_list:
#endif /* LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS */
  /* Remove the dynamic initialization from the destruction list, since
     its destruction is now handled by the static cleanup mechanism
     (or the destruction wasn't needed). */
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
local static variable guarded_var.  In effect (Cfront-like ABI):

  static int test_var;  // Global test var, implicitly init to 0
  {
    if (test_var == 0) {
      test_var = 1;
      ... real initialization of guarded_var
    }
  }

For the IA-64 ABI, the guard variable is set at the end of the
initialization (see set_local_static_guard_var):

  static int test_var;  // Global test var, implicitly init to 0
  {
    if (test_var == 0) {
      ... real initialization of guarded_var
      test_var = 1;
    }
  }

See also the additional code below to call __cxa_guard_acquire et al.
in some configurations.

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
#if !IA64_ABI || IA64_ABI_USE_INT_STATIC_INIT_GUARD
  int_kind = (an_integer_kind)ik_int;
#else /* !(IA64_ABI || IA64_ABI_USE_INT_STATIC_INIT_GUARD) */
  /* The ABI specifies that we use a 64-bit integer type.  Try that, and
     then fall back to "int". */
  int_kind = int_kind_for_bit_size(64, /*is_signed=*/FALSE);
  if (int_kind == (an_integer_kind)ik_none) {
    int_kind = (an_integer_kind)ik_int;
  }  /* if */
#endif /* (IA64_ABI || IA64_ABI_USE_INT_STATIC_INIT_GUARD) */
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
                                               &guarded_var->source_corresp,
                                               (an_il_entry_kind)iek_variable);
    /* Treat the guard variable as a promoted local static so that it will
       be removed during needed flag processing if the static variable ends
       up not being needed (e.g., because the inline routine that contains
       it is not invoked). */
    (*test_var)->promoted_local_static = TRUE;
#if IA64_ABI
    if (guarded_var->comdat_group != NULL) {
      (*test_var)->comdat_group = guarded_var->comdat_group;
    }  /* if */
#endif /* IA64_ABI */
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* The guard variable must have the same DLL flags as the guarded
       variable. */
    (*test_var)->decl_modifiers |= (guarded_var->decl_modifiers & DM_DLLFLAGS);
    if (((*test_var)->decl_modifiers & DM_DLLIMPORT) != 0) {
      /* If the variable is dllimport, it should not be defined here. */
      (*test_var)->storage_class = (a_storage_class)sc_extern;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
    /* The guard variable should also have the same ELF visibility as the
       guarded variable. */
    (*test_var)->ELF_visibility = guarded_var->ELF_visibility;
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
  } else {
    /* The guard variable need not be visible outside of the function,
       so an unnamed variable is fine. */
    *test_var = make_unnamed_local_static_variable(int_type,
                                                  /*in_function_scope=*/FALSE);
  }  /* if */
  /* Thread-local variables call for thread-local test variables. */
  (*test_var)->is_thread_local = guarded_var->is_thread_local;
#if MICROSOFT_EXTENSIONS_ALLOWED || THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
  if (guarded_var->decl_modifiers & DM_THREAD) {
    (*test_var)->decl_modifiers |= DM_THREAD;
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || THREAD_LOCAL_STORAGE_SPECIFIER_... */
  /* Make "test_var == 0". */
#if !IA64_ABI
  test_var_node = var_rvalue_expr(*test_var);
  test_var_node->next = node_for_integer_constant(0L, (an_integer_kind)ik_int);
#else /* IA64_ABI */
#if IA64_ABI_USE_INT_STATIC_INIT_GUARD
  /* The ARM EABI test is "(test_var & 1) == 0" */
  test_var_node = var_rvalue_expr(*test_var);
  test_var_node->next = node_for_integer_constant(1L,
                                                  (an_integer_kind)ik_int);
  test_var_node = make_operator_node((an_expr_operator_kind)eok_and,
                                     int_type, test_var_node);
  test_var_node->next = node_for_integer_constant(0L,
                                                  (an_integer_kind)ik_int);
#else /* !IA64_ABI_USE_INT_STATIC_INIT_GUARD */
  /* In the IA64 ABI, only the first byte of the variable is specified by 
     the ABI.  The remainder is reserved for use in multithreaded
     implementations. */
  test_var_node = add_cast_to_char_star(var_addr_expr(*test_var));
  test_var_node = add_indirection_to_node(test_var_node);
  test_var_node = rvalue_expr_for_lvalue(test_var_node);
  test_var_node->next = node_for_integer_constant(0L, 
                                                  (an_integer_kind)ik_char);
#endif /* IA64_ABI_USE_INT_STATIC_INIT_GUARD */
#endif /* IA64_ABI */
  compare_node = make_operator_node((an_expr_operator_kind)eok_eq,
                                    integer_type((an_integer_kind)ik_int),
                                    test_var_node);
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
#if !IA64_ABI
  /* Make "test_var = 1" and insert it inside the "if" statement. */
  (void)insert_var_assignment_statement(*test_var,
                                        node_for_integer_constant(1L,
                                                      (an_integer_kind)ik_int),
                                        insert_location2);
#else /* IA64_ABI */
#if IA64_ABI_USE_GUARD_ACQUIRE_RELEASE
  if (guarded_var->is_thread_local) {
    /* No need to worry about multi-threading (this variable is
       local to the thread, so a simple flag will ensure that the guarded
       initialization is performed only once in this thread). */
    /* Return the block statement created above (since we won't be creating
       the one below). */
    if (block_stmt != NULL) *block_stmt = outer_then;
  } else {
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
                             var_addr_expr(*test_var));
    an_insert_location outer_block_insert_location,
                       release_insert_location;
    an_expr_node_ptr release_node =
       make_runtime_rout_call("__cxa_guard_release", &guard_release_routine,
                             void_type(),
                             var_addr_expr(*test_var));
    /* Make the acquire call a boolean controlling expression. */
    acquire_node = boolean_controlling_expr(acquire_node);
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
  /* The guard variable is set to 1 at the end of the initialization.
     See set_local_static_guard_var. */
#endif /* IA64_ABI_USE_GUARD_ACQUIRE_RELEASE */
#endif /* IA64_ABI */
}  /* add_first_time_test */

#if IA64_ABI

static void set_local_static_guard_var(
                                 a_variable_ptr         local_static_guard_var,
                                 an_insert_location_ptr insert_location)
/*
local_static_guard_var is the guard variable associated with the initialization
of a local static variable.  If necessary, add code to set the guard variable
to indicate that the initialization is complete.  Insert the code at
*insert_location.
*/
{
#if IA64_ABI_USE_GUARD_ACQUIRE_RELEASE
  if (!local_static_guard_var->is_thread_local) {
    /* In non-thread-local cases, the call to __cxa_guard_release has already
       been emitted, so there's nothing to do here. */
  } else
#endif /* IA64_ABI_USE_GUARD_ACQUIRE_RELEASE */
  /* Do not insert code here. */
  {
#if IA64_ABI_USE_INT_STATIC_INIT_GUARD
    /* ARM EABI specifies to use least significant bit for guard test. */
    (void)insert_assignment_statement(var_lvalue_expr(local_static_guard_var),
                                      (an_expr_operator_kind)eok_assign,
                                      node_for_integer_constant(1L,
                                                      (an_integer_kind)ik_int),
                                      insert_location);
#else /* !IA64_ABI_USE_INT_STATIC_INIT_GUARD */
    /* IA-64 ABI specifies to use first byte for guard test. */
    (void)insert_assignment_statement(add_indirection_to_node(
                                       add_cast_to_char_star(
                                       var_addr_expr(local_static_guard_var))),
                                      (an_expr_operator_kind)eok_assign,
                                      node_for_integer_constant(1L,
                                                     (an_integer_kind)ik_char),
                                      insert_location);
#endif /* IA64_ABI_USE_INT_STATIC_INIT_GUARD */
  }  /* if */
}  /* set_local_static_guard_var */

#endif /* IA64_ABI */

#if !IA64_ABI
#define USE_EH_GUARD_VAR_CLEANUP TRUE
#else /* IA64_ABI */
#if IA64_ABI_USE_GUARD_ACQUIRE_RELEASE
#define USE_EH_GUARD_VAR_CLEANUP TRUE
#endif /* IA64_ABI_USE_GUARD_ACQUIRE_RELEASE */
#endif /* !IA64_ABI*/

#ifndef USE_EH_GUARD_VAR_CLEANUP
/*ARGSUSED*/ /* The parameters are not used in that case. */
#endif /* ifndef USE_EH_GUARD_VAR_CLEANUP */
static void add_local_static_guard_var_cleanup(
                                 a_variable_ptr         local_static_guard_var,
                                 an_object_lifetime_ptr local_static_lifetime,
                                 an_insert_location_ptr insert_location)
/*
local_static_guard_var is the guard variable associated with the initialization
of a local static variable.  local_static_lifetime is the object lifetime
that surrounds the complete initialization.  Add a dynamic initialization
entry and associated region table entry to indicate to the runtime that
the guard variable must be reset if an exception is thrown before
the initialization of the local static variable is completed.  If any
code is needed, insert it at *insert_location.
*/
{
#ifdef USE_EH_GUARD_VAR_CLEANUP
#undef USE_EH_GUARD_VAR_CLEANUP
  a_dynamic_init_ptr dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
  an_init_pos_descr  ipd;

  /* In the IA-64 ABI, the runtime calls __cxa_guard_abort instead of
     clearing the variable. */
  dip->variable = local_static_guard_var;
  dip->has_temporary_lifetime = TRUE;
  dip->is_guard_var_for_local_static_var_init = TRUE;
  add_to_end_of_destructions_list(dip, local_static_lifetime);
  dip->destructible_entity_descr = alloc_destructible_entity_descr();
  set_var_init_pos_descr(local_static_guard_var, &ipd);
  add_dyn_init_cleanup(dip, &ipd, /*set_cond_flag_if_any=*/FALSE,
                       curr_context, insert_location);
#endif /* ifdef USE_EH_GUARD_VAR_CLEANUP */
}  /* add_local_static_guard_var_cleanup */

#if GENERATE_EH_TABLES
#if ABI_COMPATIBILITY_VERSION > 310

void add_runtime_exception_object_cleanup(an_insert_location *insert_location)
/*
Add a cleanup entry to destroy the runtime exception object allocated by the
runtime if a catch clause is terminated by an exception throw.  The current
context is the scope of the catch handler, at the beginning of generation
of code for the handler.  Insert any required code at *insert_location.
*/
{
  a_dynamic_init_ptr dip;
  an_init_pos_descr  ipd;

  /* This is done only when EH tables are generated because in that mode
     the runtime doesn't know how the region table entries correlate to
     the catch clauses.  In the unlowered EH configuration the cleanup
     entries are grouped by object lifetime and there is a separate
     lifetime for the catch clause, so it's easy to see where the
     object destruction should happen. */
  /* The cleanup doesn't need an object -- the runtime knows where the
     exception object is. */
  clear_init_pos_descr(&ipd);
  /* Add a dynamic init entry for the cleanup. */
  dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
  dip->is_freeing_of_exception_object = TRUE;
  dip->destructible_entity_descr = alloc_destructible_entity_descr();
  add_to_end_of_destructions_list(dip, curr_object_lifetime);
  add_dyn_init_cleanup(dip, &ipd, /*set_cond_flag_if_any=*/FALSE,
                       curr_context, insert_location);
}  /* add_runtime_exception_object_cleanup */

#endif /* ABI_COMPATIBILITY_VERSION > 310 */
#endif /* GENERATE_EH_TABLES */

static void adjust_cleanup_state_for_static_aggregate_init(
                                             a_dynamic_init_ptr preceding_init)
/*
A static aggregate initialization has just been completed.  Adjust the
cleanup state to the latest initialization that is not a partial aggregate
initialization, or preceding_init (which indicates the initialization
that precedes the start of the entire aggregate initialization), whichever
is first on the destruction list.
*/
{
  a_dynamic_init_ptr dip = curr_context->latest_initialization;

  while (dip != NULL && dip != preceding_init &&
         dip->destruction_is_for_partially_constructed_aggregate) {
    dip = dip->next_in_destruction_list;
  }  /* while */
  curr_context->latest_initialization = dip;
  set_curr_cleanup_state_to_latest_initialization();
}  /* adjust_cleanup_state_for_static_aggregate_init */


static void adjust_cleanup_state_for_aggregate_init(
                                           a_dynamic_init_ptr dip,
                                           a_dynamic_init_ptr preceding_init,
                                           an_insert_location *insert_location,
                                           a_boolean          *some_cloned,
                                           a_boolean          *some_ordered)
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
If any entries are found that are NOT unordered, *some_ordered is set
to TRUE; this controls cloning as recursive calls are unwound.
insert_location is an insert location for any code that has to be generated.
*/
{
  check_assertion_str(dip != NULL,
                      "adjust_cleanup_state_for_aggregate_init: NULL dip");
  *some_cloned = FALSE;
  *some_ordered = FALSE;
  if (dip->next_in_destruction_list == preceding_init) {
    /* End of the list. */
    curr_context->latest_initialization = preceding_init;
  } else {
    /* Do a recursive call to process the rest of the list. */
    adjust_cleanup_state_for_aggregate_init(dip->next_in_destruction_list,
                                            preceding_init,
                                            insert_location,
                                            some_cloned,
                                            some_ordered);
  }  /* if */
#if GENERATE_EH_TABLES && DO_UNORDERED_EH_PROCESSING
  if (!dip->unordered || dip->next_in_destruction_list == NULL) {
    *some_ordered = TRUE;
  }  /* if */
#endif /* GENERATE_EH_TABLES && DO_UNORDERED_EH_PROCESSING */
  if (!dip->destruction_is_for_partially_constructed_aggregate) {
#if GENERATE_EH_TABLES
    /* When the entries are marked as unordered, we do not do any cloning,
       because the entries are all linked together weirdly in one large
       clump.  Just clear the flags on the partial-aggregate entries (see
       below). */
    if (exceptions_enabled
#if DO_UNORDERED_EH_PROCESSING
        && *some_ordered
#endif /* DO_UNORDERED_EH_PROCESSING */
       ) {
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
#if GENERATE_EH_TABLES && DO_UNORDERED_EH_PROCESSING
  } else {
    /* Partial-aggregate cleanup entry.  If unordered, reset the flag. */
    if (exceptions_enabled && dip->unordered && !*some_ordered) {
      a_destructible_entity_descr_ptr dedp = dip->destructible_entity_descr;
      reset_conditional_flag_var(dedp->conditional_flag_var,
                                 insert_location);
    }  /* if */
#endif /* GENERATE_EH_TABLES && DO_UNORDERED_EH_PROCESSING */
  }  /* if */
  set_curr_cleanup_state_to_latest_initialization();
}  /* adjust_cleanup_state_for_aggregate_init */


/*
Pointer to the routine entry for the C library routine memcpy.  NULL until
created.
*/
static a_routine_ptr
		memcpy_routine;

void rewrite_class_assignment_if_necessary(an_expr_node_ptr expr)
/*
expr is a struct assignment.  It's defined to do what the C++ generated
bitwise operator= would do, which is copy the data of the class but not any
tail padding.  If a C structure assignment would copy too much, replace the
assignment with the proper operation.  For an empty class, eliminate the copy
(since it's supposed to copy nothing) but keep any side effects.  The given
expression can be either an lvalue or rvalue and lvalueness is preserved.
*/
{
  a_type_ptr class_type;
  a_boolean  returns_lvalue = expr->is_lvalue;

  class_type = skip_typerefs(expr->type);
  /* The is_immediate_class_type test avoids problems with lowered
     pointer-to-member-function assignments. */
  if (is_immediate_class_type(class_type)) {
    an_expr_node_ptr op1 = expr->variant.operation.operands;
    an_expr_node_ptr op2 = op1->next;
    check_assertion(op1->is_lvalue && !op2->is_lvalue);
    if (class_type->variant.class_struct_union.is_empty_class) {
      /* An empty class.  Eliminate the assignment but keep any side
         effects. */
      /* We're either going to overwrite the expression with op1 or create
         a comma node with op1 as the second argument.  In either case op1
         needs to have the same lvalueness as the original expression. */
      if (!returns_lvalue) {
        op1 = rvalue_expr_for_lvalue(op1);
      }  /* if */
      /* If op2 has no side effects, just overwrite the original expression
         with the (possibly adjusted) op1. */
      if (!node_has_side_effects(op2, (a_boolean *)NULL)) {
        overwrite_node(expr, op1);
      } else {
        /* Rewrite the expression as a comma node. */
        /* Flip the operands so that the left-side operand is returned, for
           the case where the assignment returns an lvalue. */
        op2->next = op1;
        op1->next = NULL;
        set_node_operator(expr, (an_expr_operator_kind)eok_comma,
                          expr->type, op1->is_lvalue, op2);
      }  /* if */
    } else {
      a_targ_size_t entity_size =
               class_type_supp(class_type)->size_without_virtual_base_classes;
      if (entity_size != class_type->size) {
        /* A class with tail padding.  Rewrite the copy as a memcpy call. */
        an_expr_node_ptr call_node;
        op1->next = NULL;
        op1 = add_address_of_to_node(op1);
        op1 = add_cast(op1, void_star_type());
        /* op2 is an rvalue, but we need a pointer for the memcpy. */
        op2 = rvalue_pointer_for_class_rvalue(op2);
        op2 = add_cast(op2, make_pointer_type(
                                make_qualified_type(void_type(), TQ_CONST)));
        op1->next = op2;
        op2->next = node_for_host_large_integer(
                                           (a_host_large_integer)entity_size,
                                           targ_size_t_int_kind);
        call_node = make_runtime_rout_call("memcpy", &memcpy_routine,
                                           void_star_type(), op1);
        if (!expr->result_is_not_used) {
          /* Make sure the node has the correct type. */
          call_node = add_cast(call_node, make_pointer_type(expr->type));
          call_node = add_indirection_to_node(call_node);
          if (!returns_lvalue) {
            call_node = rvalue_expr_for_lvalue(call_node);
          }  /* if */
        }  /* if */
        check_assertion(expr->is_lvalue == call_node->is_lvalue);
        overwrite_node(expr, call_node);
      }  /* if */
    }  /* if */
  }  /* if */
  check_assertion(expr->is_lvalue == returns_lvalue);
}  /* rewrite_class_assignment_if_necessary */


static an_expr_node_ptr make_assignment_expr_with_subobject_fix(
                                    an_expr_node_ptr      dest_node,
                                    a_boolean             have_complete_object,
                                    an_expr_operator_kind op,
                                    an_expr_node_ptr      source_node)
/*
Create and return an assignment node that assigns source_node to dest_node
using the assignment operator op.  have_complete_object is TRUE if the
destination is a complete object; FALSE means a base class subobject.
If the assignment is to a subobject, alter the assignment appropriately.
*/
{
  an_expr_node_ptr assign_node;

  assign_node = make_assignment_expr(dest_node, op, source_node);
  if (!have_complete_object &&
      node_operator_type_kind_is(assign_node, tk_struct)) {
    /* Fix subobject assignments. */
    rewrite_class_assignment_if_necessary(assign_node);
  }  /* if */
  return assign_node;
}  /* make_assignment_expr_with_subobject_fix */


static a_routine_ptr helper_routine_to_initialize_entity(
                                            a_type_ptr    type,
                                            a_boolean     have_complete_object,
                                            a_boolean     need_array_count,
                                            a_boolean     zero_entity,
                                            a_routine_ptr ctor_routine)
/*
Build a routine to initialize an entity of the indicated type
(which should have its typerefs, if any, in place).  When zero_entity
is TRUE, the entity is initialized to "zero" (needed in the IA-64 ABI because
pointers to data members use -1 as the NULL value).  When zero_entity
is FALSE, each element of the entity is initialized to the value pointed to
by a second or third parameter (see below).  If have_complete_object is TRUE we
have a complete object; if it is FALSE, we have a base class subobject.  If
need_array_count is TRUE, the generated routine has a second parameter of type
size_t that indicates the number of elements of an array to be initialized, and
the routine body has a loop to do the initializations.  If ctor_routine is
non-NULL, it points to a constructor to be called after the zeroing have been
done.

Depending on the values for need_array_count and zero_entity, the generated
routine will have one, two, or three parameters.  The first parameter is
always a pointer to the entity to be initialized.  When need_array_count
is TRUE a second parameter is added to specify the number of elements in
the entity to initialize.  When zero_entity is FALSE, a (second or) third
parameter is added that points to a value which is copied to each element
of the array.
*/
{
  a_routine_ptr                 rp;
  a_routine_type_supplement_ptr rtsp;
  a_type_ptr                    pointer_type, count_type, model_type;
  a_memory_region_number        il_region;
  a_scope_ptr                   scope;
  an_insert_location            insert_location;
  a_generated_routine_context   context;
  a_variable_ptr                model_var, entity_var, count_var = NULL;
  a_statement_ptr               loop_stmt = NULL, copy_stmt;
  an_expr_node_ptr              entity_expr, ctor_entity_expr = NULL;
  an_expr_node_ptr              copy_expr;
  an_expr_node_ptr              source_expr;
  a_param_type_ptr              *last_param_type;
  a_param_type_ptr              count_param_type = NULL;
  a_param_type_ptr              model_param_type = NULL;
  a_variable_ptr                *last_param;
  
  /* Build the routine entry.  It has one, two, or three parameters
     depending on its purpose. */
  /* Skip any cv-qualifiers on the type for the purposes of the parameter
     to the routine, but leave the typerefs (especially the underlying
     typeref that indicates the type was a lowered pointer-to-data member)
     when making the temporary below. */
  pointer_type = make_pointer_type(skip_typerefs(type));
  model_type = make_qualified_type(type, TQ_CONST);
  count_type = integer_type(targ_size_t_int_kind);
  rp = make_rout_entry((char *)NULL, (a_storage_class)sc_static,
                       void_type(), pointer_type);
  rtsp = rp->type->variant.routine.extra_info;
  /* In addition to the parameter for the pointer to the entity, optionally
     add one or two additional parameters. */
  last_param_type = &rtsp->param_type_list;
  if (need_array_count) {
    count_param_type = alloc_param_type(count_type);
    (*last_param_type)->next = count_param_type;
    last_param_type = &(*last_param_type)->next;
  }  /* if */
  if (!zero_entity) {
    model_param_type = alloc_param_type(make_pointer_type(model_type));
    (*last_param_type)->next = model_param_type;
  }  /* if */
  /* Build the definition of the routine.  */
  scope = make_routine_definition(rp, /*make_return=*/TRUE, &il_region);
  push_generated_routine_context(scope, il_region, &context);
  /* Create the parameters (there may be one, two, or three). */
  scope->variant.routine.parameters = entity_var = 
                     make_lowered_param_variable(rtsp->param_type_list->type);
  last_param = &scope->variant.routine.parameters;
  if (need_array_count) {
    count_var = make_lowered_param_variable(count_param_type->type);
    (*last_param)->next = count_var;
    last_param = &(*last_param)->next;
  }  /* if */
  if (!zero_entity) {
    /* The "model" for the initialization is passed to us by the caller. */
    model_var = make_lowered_param_variable(model_param_type->type);
    (*last_param)->next = model_var;
  } else {
#if IA64_ABI
    /* Build a model for the zero-initialized entity. */
    model_var = make_temporary_in_scope(model_type,
                                        scope, /*force_static=*/FALSE,
                                        /*promote_if_necessary=*/FALSE);
    model_var->init_kind = (an_init_kind)initk_zero;
    lower_initializer(model_var, &model_var->init_kind,
                      &model_var->initializer);
#else /* !IA64_ABI */
    unexpected_condition();
#endif /* IA64_ABI */
  }  /* if */
  set_block_start_insert_location(scope->assoc_block, &insert_location);
  /* Mark the location where any generated stmk_init statements should go. */
  set_insert_location_mark(&insert_location);
  /* Insert any generated stmk_inits at the previously marked location. */
  insert_pending_stmk_init_statements_at_mark(&insert_location);
  if (need_array_count) {
    an_expr_node_ptr  expr;
    /* Build a loop to initialize the entities. */
    loop_stmt = alloc_statement((a_statement_kind)stmk_while);
    expr = make_operator_node((an_expr_operator_kind)eok_post_decr,
                              count_type, var_lvalue_expr(count_var));
    loop_stmt->expr = boolean_controlling_expr(expr);
    /* The access to the entity increments it each time a store is done. */
    entity_expr = make_operator_node((an_expr_operator_kind)eok_post_incr,
                                     pointer_type,
                                     var_lvalue_expr(entity_var));
    entity_expr = add_indirection_to_node(entity_expr);
    if (ctor_routine != NULL) {
      /* When a constructor has to be called after the zeroing, increment
         the source pointer in the reference in the constructor call,
         not in the copy. */
      ctor_entity_expr = entity_expr;
      entity_expr = var_lvalue_expr(entity_var);
    }  /* if */
  } else {
    entity_expr = var_rvalue_expr(entity_var);
    entity_expr = add_indirection_to_node(entity_expr);
    if (ctor_routine != NULL) {
      ctor_entity_expr = var_rvalue_expr(entity_var);
    }  /* if */
  }  /* if */
  /* Build an expression to copy the model variable to the entity to
     be initialized. */
  source_expr = var_rvalue_expr(model_var);
  if (!zero_entity) {
    /* An indirection is needed in the case where we're not zero-initializing
       the entity. */
    source_expr = rvalue_expr_for_lvalue(add_indirection_to_node(source_expr));
  }  /* if */
  copy_expr = make_assignment_expr_with_subobject_fix(
                                            entity_expr, have_complete_object,
                                            (an_expr_operator_kind)eok_assign,
                                            source_expr);
  if (ctor_routine != NULL) {
    /* Add a call of the indicated constructor after the copying/zeroing
       code. */
    an_expr_node_ptr ctor_call;
    ctor_call = make_call_node(ctor_routine, ctor_entity_expr);
    copy_expr = make_comma_node(copy_expr, ctor_call);
  }  /* if */
  /* Perform a lowering post-pass on the expression to optimize it and clean up
     any remaining issues. */
  perform_post_pass_on_lowered_expression(copy_expr);
  copy_stmt = alloc_expr_statement(copy_expr);
  if (need_array_count) {
    /* Loop case -- the copy statement is the body of the loop. */
    loop_stmt->variant.loop_statement = copy_stmt;
    insert_statement(loop_stmt, &insert_location);
  } else {
    /* Non-loop case -- the copy statement is just inserted in the body
       of the function. */
    insert_statement(copy_stmt, &insert_location);
  }  /* if */
  /* Clean up. */
  pop_generated_routine_context(scope, il_region, &context);
  return rp;
}  /* helper_routine_to_initialize_entity */


/*
Pointer to the routine entry for the runtime routine __memzero.  NULL until
created.  For the IA-64 ABI, points to memset or bzero instead.
*/
static a_routine_ptr
		memzero_routine;


static void insert_runtime_zeroing_call(an_expr_node_ptr   entity_node,
                                        an_expr_node_ptr   entity_size_node,
                                        an_insert_location *insert_location)
/*
Create a runtime routine call to zero the entity pointed to by the rvalue
expression entity_node.  The number of bytes to zero is given by
entity_size_node.  Insert the code at *insert_location.
*/
{
  an_expr_node_ptr memzero_call;

  check_assertion(!entity_node->is_lvalue &&
                  is_pointer_type(entity_node->type));
  entity_size_node = add_cast_if_necessary(entity_size_node,
                                           integer_type(targ_size_t_int_kind));
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
}  /* insert_runtime_zeroing_call */


static void insert_call_to_initialize_entity(
                                       a_type_ptr         entity_type,
                                       a_boolean          have_complete_object,
                                       an_expr_node_ptr   entity_node,
                                       an_expr_node_ptr   num_elem_node,
                                       a_targ_size_t      array_element_count,
                                       an_expr_node_ptr   source_node,
                                       an_insert_location *insert_location)
/*
Create a runtime routine call to initialize the entity specified by
entity_node, whose type is entity_type; it is an rvalue pointer that points
to a complete object if have_complete_object is TRUE.  If source_node is
NULL, the entity is zero-initialized; otherwise each element of the
entity is set to the value pointed to by source_node.  If num_elem_node
is non-NULL, the entity is an array and the expression value gives
the number of elements (the entity_type in that case is the array
element type).  num_elem_node must be non-NULL when initializing a
variably-sized array.  If array_element_count is non-zero, it gives the
number of elements in an array sequence (and again, entity_type is
the array element type).  If neither of those provides information,
the entity can still be an array; the array attributes are fetched
from entity_type itself.  Insert the code for the call at *insert_location.
*/
{
  a_type_ptr element_type, orig_element_type = entity_type;

  check_assertion(!entity_node->is_lvalue &&
                  is_pointer_type(entity_node->type) &&
                  (num_elem_node == NULL || array_element_count == 0));
  if (array_element_count == 0) array_element_count = 1;
  if (is_array_type(entity_type)) {
    orig_element_type = underlying_array_element_type(entity_type);
    if (is_incomplete_array_type(entity_type)) {
      /* For variably-sized arrays, make sure we have a run-time count of
         the number of elements in the array. */
      check_assertion(num_elem_node != NULL);
    } else {
      array_element_count *= num_array_elements(entity_type);
    }  /* if */
  }  /* if */
  element_type = skip_typerefs(orig_element_type);
  if (is_immediate_class_type(element_type) &&
      element_type->variant.class_struct_union.is_empty_class) {
    /* Put out no code at all to zero an empty class. */
  } else if (source_node != NULL
#if IA64_ABI
             || contains_ptr_to_data_member(orig_element_type)
#endif /* IA64_ABI */
                                                              ) {
    /* Each element in the array is being set to the same value -- either
       the value pointed to by source_node or to a zero-value (but the
       entity type contains pointers to data members that must be initialized
       to -1, not zero, for the IA-64 ABI). */
    a_boolean         array_case;
    an_expr_node_ptr  *last_arg;
    if (num_elem_node == NULL) {
      array_case = (array_element_count != 1);
      if (array_case) {
        num_elem_node = node_for_host_large_integer(
                                     (a_host_large_integer)array_element_count,
                                     targ_size_t_int_kind);
      }  /* if */
    } else {
      /* num_elem_node gives the number of elements in the array. */
      array_case = TRUE;
      if (array_element_count != 1) {
        /* Multiply num_elem_node by the value of array_element_count to
           get the actual number of elements. */
        num_elem_node = add_cast_if_necessary(
                                           num_elem_node,
                                           integer_type(targ_size_t_int_kind));
        num_elem_node->next = node_for_host_large_integer(
                                     (a_host_large_integer)array_element_count,
                                     targ_size_t_int_kind);
        num_elem_node = make_operator_node(
                                          (an_expr_operator_kind)eok_multiply,
                                          num_elem_node->type,
                                          num_elem_node);
      }  /* if */
    }  /* if */
    /* Call a helper routine to initialize the entity.  Note that in this case
       the routine gets the count of array elements (or 1 for a non-array)
       rather than the size in bytes. */
    entity_node = add_cast_if_necessary(entity_node,
                                        make_pointer_type(element_type));
    last_arg = &entity_node;
    if (array_case) {
      (*last_arg)->next = num_elem_node;
      last_arg = &(*last_arg)->next;
    }  /* if */
    if (source_node != NULL) (*last_arg)->next = source_node;
    make_call_statement(helper_routine_to_initialize_entity(orig_element_type,
                                                       have_complete_object,
                                                       array_case,
                                                       source_node == NULL,
                                                       (a_routine_ptr)NULL),
                        entity_node, (an_expr_node_ptr)NULL, insert_location);
  } else {
    /* The entity (not an empty base class) must be set to all zeroes. */
    an_expr_node_ptr entity_size_node;
    a_targ_size_t    entity_size = element_type->size;
    if (!have_complete_object && is_immediate_class_type(element_type)) {
      /* For a subobject of class type, use the size without virtual
         base classes. */
      entity_size = element_type->variant.class_struct_union.extra_info->
                                            size_without_virtual_base_classes;
    }  /* if */
    if (array_element_count != 1) {
      entity_size *= array_element_count;
    }  /* if */
    if (num_elem_node == NULL) {
      entity_size_node = node_for_host_large_integer(
                                             (a_host_large_integer)entity_size,
                                             targ_size_t_int_kind);
    } else {
      /* num_elem_node gives the count of elements.  Multiply it by the
         size of each element to get the total size. */
      num_elem_node= add_cast_if_necessary(num_elem_node,
                                           integer_type(targ_size_t_int_kind));
      num_elem_node->next = node_for_host_large_integer(
                                             (a_host_large_integer)entity_size,
                                             targ_size_t_int_kind);
      entity_size_node=make_operator_node((an_expr_operator_kind)eok_multiply,
                                          num_elem_node->type,
                                          num_elem_node);
    }  /* if */
    /* Call a runtime routine to zero the entity.  In this case the
       runtime routine gets the size in bytes of the whole entity. */
    insert_runtime_zeroing_call(entity_node, entity_size_node,
                                insert_location);
  }  /* if */
}  /* insert_call_to_initialize_entity */


static void insert_call_to_zero_entity(a_type_ptr         entity_type,
                                       a_boolean          have_complete_object,
                                       an_expr_node_ptr   entity_node,
                                       an_expr_node_ptr   num_elem_node,
                                       a_targ_size_t      array_element_count,
                                       an_insert_location *insert_location)
/*
A utility routine to zero the entity specified by entity_node.  See
insert_call_to_initialize_entity above for a description of the arguments.
*/
{
  insert_call_to_initialize_entity(entity_type,
                                   have_complete_object,
                                   entity_node,
                                   num_elem_node,
                                   array_element_count,
                                   (an_expr_node_ptr)NULL,
                                   insert_location);
}  /* insert_call_to_zero_entity */


static void lower_optimized_class_rvalue_question_mark(
                                           an_expr_node_ptr   expr,
                                           an_insert_location *insert_location)
/*
Lower an optimized "?" operation that returns a class rvalue.  Specifically,
expr is the expression from a dynamic initialization with
is_optimized_class_rvalue_question_mark set to TRUE, which initializes
the temporary that is the result of the "?" operation.  The expression
is supposed to be evaluated to initialize the temporary, but its value
is discarded.  The initialization code is inserted at *insert_location.
The expression passed in has not been lowered yet and must be lowered.
*/
{
  an_expr_node_ptr operand_1, operand_2, operand_3;

  check_assertion(is_operation_node(expr) &&
                  expr->variant.operation.kind ==
                                          (an_expr_operator_kind)eok_question);
  operand_1 = expr->variant.operation.operands;
  operand_2 = operand_1->next;
  operand_3 = operand_2->next;
  operand_2->next = NULL;
  /* Cast the operands to void and mark their results as not used so we
     can get better code generation. */
  set_expr_result_not_used(operand_2);
  set_expr_result_not_used(operand_3);
  operand_2 = add_cast_if_necessary(operand_2, void_type());
  operand_3 = add_cast_if_necessary(operand_3, void_type());
  operand_1->next = operand_2;
  operand_2->next = operand_3;
  expr->type = void_type();
  set_expr_result_not_used(expr);
  /* Lower the operation now that the operands indicate the result is
     not used. */
  lower_expr(expr);
  (void)insert_expr_statement(expr, insert_location);
}  /* lower_optimized_class_rvalue_question_mark */

#if VLA_DEALLOCATION_REQUIRED

static a_dynamic_init_ptr add_vla_deallocation_dynamic_init(
                                                        a_dynamic_init_ptr dip)
/*
Insert a new dynamic-init entry that represents the deallocation of
the variable-length array (VLA) whose allocation/initialization is
indicated by dip.  Return a pointer to the new entry.  The new entry is
placed immediately following dip on the next_in_destruction_list chain.
*/
{
  a_dynamic_init_ptr dealloc_dip;

  check_assertion(vla_enabled && !C_mode());
  dealloc_dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
  dealloc_dip->variable = dip->variable;
  dealloc_dip->is_vla_deallocation = TRUE;
  dealloc_dip->destructible_entity_descr = alloc_destructible_entity_descr();
  add_to_destructions_list_following(dip, dealloc_dip);
  return dealloc_dip;
}  /* add_vla_deallocation_dynamic_init */

#endif /* VLA_DEALLOCATION_REQUIRED */

static void add_cast_for_cv_qualified_cctor_param_if_necessary(
                                                       a_constant_ptr constant)
/*
Scan the specified constant looking for any piece(s) of the constant that
could be function pointers that have a cv-qualified copy constructor passed
as an argument.  Such pointers are adjusted during lowering and an implicit
cast needs to be added to these constants to avoid warnings when compiling
the generated C code.  Note that setting implicit_cast on constants such
as ck_address/abk_routine constants doesn't generate a cast unless the actual
and desired types are different, so this must be handled external to
this function.
*/
{
  a_constant_ptr cp;

  check_assertion(constant != NULL);
  if (constant->type != NULL) {
    if (constant->kind == (a_constant_repr_kind)ck_aggregate) {
      if (is_array_type(constant->type)) {
        /* The underlying type of all array elements is the same, meaning
           that all elements need a cast or none do. */
        a_type_ptr elem_type = underlying_array_element_type(constant->type);
        if (needs_cast_because_type_has_param_passed_via_cctor(elem_type)) {
          if (is_array_type(array_element_type(constant->type))) {
            /* Recurse to get to the bottom most level of the array. */
            for (cp = constant->variant.aggregate.first_constant;
                 cp != NULL;
                 cp = cp->next) {
              add_cast_for_cv_qualified_cctor_param_if_necessary(cp);
            }  /* for */
          } else {
            /* We could recurse here, but since we know we're at the
               bottom of an array, all of whose elements need a cast,
               save some time and just set the implicit_cast field on
               each element. */
            for (cp = constant->variant.aggregate.first_constant;
                 cp != NULL;
                 cp = cp->next) {
              if (cp->kind == (a_constant_repr_kind)ck_init_repeat) {
                /* Recurse for repeated constants. */
                add_cast_for_cv_qualified_cctor_param_if_necessary(
                                             cp->variant.init_repeat.constant);
              } else {
                cp->implicit_cast = TRUE;
              }  /* if */
            }  /* for */
          }  /* if */
        }  /* if */
      } else {
        /* We have to check each field of a non-array aggregate initialization
           individually. */
        for (cp = constant->variant.aggregate.first_constant;
             cp != NULL;
             cp = cp->next) {
          add_cast_for_cv_qualified_cctor_param_if_necessary(cp);
        }  /* for */
      }  /* if */
    } else if (needs_cast_because_type_has_param_passed_via_cctor(
                                                             constant->type)) {
      /* This component of the constant needs a cast. */
      constant->implicit_cast = TRUE;
    }  /* if */
  } else if (constant->kind == (a_constant_repr_kind)ck_init_repeat) {
    /* Recurse for repeated constants. */
    add_cast_for_cv_qualified_cctor_param_if_necessary(
                                       constant->variant.init_repeat.constant);
  } else if (constant->kind == (a_constant_repr_kind)ck_designator) {
    /* Ignore designators. */
  } else {
    /* Only ck_init_repeat and ck_designator should have NULL types. */
    unexpected_condition();
  }  /* if */
}  /* add_cast_for_cv_qualified_cctor_param_if_necessary */


void copy_non_static_data_member_initializers_if_necessary(a_scope_ptr scope)
/*
If scope represents a constructor whose class has non-static data member
initialized fields, copy the dynamic initialization from the field to
the constructor initializer.
*/
{
  an_object_lifetime_ptr  olp = scope->lifetime;
  an_object_lifetime_ptr  saved_curr_object_lifetime = NULL;
  a_dynamic_init_ptr      field_dip, dip, prev_dip = NULL;
  a_constructor_init_ptr  ctor_init;
  a_routine_ptr           routine = scope->variant.routine.ptr;
#if LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS
  a_constructor_init_ptr  prev_ctor_init = NULL;
#endif /* LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS */

  check_assertion(scope->kind == (a_scope_kind)sck_function);
  if (routine->special_kind == (a_special_function_kind)sfk_constructor &&
      scope->variant.routine.constructor_inits != NULL &&
      parent_class_of(routine)->variant.class_struct_union.field_list != NULL){
    for (ctor_init = scope->variant.routine.constructor_inits;
         ctor_init != NULL;
         ctor_init = ctor_init->next) {
      if (ctor_init->kind == (a_constructor_init_kind)cik_field &&
          ctor_init->use_field_initializer) {
        /* A non-static data member is initialized with a brace-or-equal
           initializer.  In this case, the dynamic initialization is associated
           with the field itself and must be copied before being lowered. */
        field_dip = ctor_init->variant.field->initializer;
        check_assertion(field_dip != NULL && field_dip->lifetime == NULL);
#if LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS
        if ((field_dip->kind == (a_dynamic_init_kind)dik_none ||
             (field_dip->kind == (a_dynamic_init_kind)dik_constructor &&
              call_to_ctor_or_dtor_has_no_effect(
                                           field_dip->variant.constructor.ptr,
                                           field_dip->variant.constructor.args,
                                           /*call_can_be_virtual=*/FALSE))) &&
            (field_dip->destructor == NULL ||
             (field_dip->destructor->special_kind ==
                                     (a_special_function_kind)sfk_destructor &&
              call_to_ctor_or_dtor_has_no_effect(
                                            field_dip->destructor,
                                            (an_expr_node_ptr)NULL,
                                            /*call_can_be_virtual=*/FALSE)))) {
          /* Neither the construction nor destruction have any effect;
             this ctor_init can be safely eliminated before it is copied.
             Note that in cases where one of construction/destruction has
             an effect but not the other, the ctor_init is copied below and
             the do-nothing operation is eliminated later during normal
             processing (this is just an optimization to prevent unnecessary
             copies of dynamic inits). */
          if (prev_ctor_init == NULL) {
            scope->variant.routine.constructor_inits = ctor_init->next;
          } else {
            prev_ctor_init->next = ctor_init->next;
          }  /* if */
        } else
#endif /* LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS */
        /* Do not insert code here. */
        {
          /* Copy the dynamic init into the ctor initializer so that
             it will be processed by lowering like any other ctor initializer.
             If a destruction is associated with the initialization,
             make sure it is placed in the proper location in the object
             lifetime of the constructor's scope. */
          if (olp == NULL) {
            /* If the scope has no destructible objects (aside from those
               being added here), the function scope may have no object
               lifetime, in which case we allocate one here (before we
               copy the dynamic init because the lifetime may be needed
               in that case).  If the lifetime ends up being useless, it
               is removed below. */
            saved_curr_object_lifetime = curr_object_lifetime;
            add_object_lifetime_to_function_scope(scope);
            olp = scope->lifetime;
            curr_object_lifetime = olp;
          }  /* if */
          dip = copy_dynamic_init(field_dip, CE_NO_OPTIONS);
          if (dip->destructor != NULL) {
            if (prev_dip == NULL) {
              add_to_end_of_destructions_list(dip, olp);
            } else {
              add_to_destructions_list_following(prev_dip, dip);
            }  /* if */
          }  /* if */
          ctor_init->initializer = dip;
          /* Set the flag that indicates that a dynamic initializer is pointed
             to by a constructor_init entry (now that it is). */
          dip->is_constructor_init = TRUE;
        }  /* if */
      }  /* if */
      dip = ctor_init->initializer;
      if (dip != NULL && dip->destructor != NULL) {
        prev_dip = dip;
      }  /* if */
#if LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS
      prev_ctor_init = ctor_init;
#endif /* LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS */
    }  /* for */
  }  /* if */
  if (saved_curr_object_lifetime != NULL) {
    if (is_useless_object_lifetime(scope->lifetime)) {
      /* Remove useless object lifetime that was allocated above.  No need to
         unlink it from its parent (function scope object lifetimes
         aren't queued on the file scope object lifetime). */
      unbind_object_lifetime(scope->lifetime);
    }  /* if */
    curr_object_lifetime = saved_curr_object_lifetime;
  }  /* if */
}  /* copy_non_static_data_member_initializers_if_necessary */

#if LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS

static a_boolean call_to_ctor_or_dtor_has_no_effect(
                                         a_routine_ptr     routine,
                                         an_expr_node_ptr  args,
                                         a_boolean         call_can_be_virtual)
/*
Returns TRUE if a call to the specified constructor or destructor is known to
have no effect and is therefore a candidate to be removed during lowering.
args is a list of un-lowered, user-specified arguments to a constructor (there
are no user-specified arguments to a destructor).  The arguments are checked to
see if they have any side effects (in which case it isn't possible to remove
the call to this instance of the constructor).  If call_can_be_virtual is
TRUE, assume that the (destructor) routine may be called through a virtual
function call.  It is always safe to return FALSE.

In some configurations, constructors and/or destructors can return "this".
It is the caller's responsibility to ensure that the return value of the
routine is not used before eliding a call to the routine.
*/
{
  a_boolean   result = routine->has_no_effect;

  check_assertion(args == NULL ||
                  routine->special_kind ==
                                     (a_special_function_kind)sfk_constructor);
  check_assertion(!call_can_be_virtual ||
                  routine->special_kind ==
                                      (a_special_function_kind)sfk_destructor);
  if (result) {
    if (call_can_be_virtual && routine->is_virtual) {
      /* If a virtual destructor is known to be empty (which would require a
         local modification since all virtual destructors currently have
         at least an assignment to a virtual table pointer), then the call
         to the destructor can be elided if we're making a call directly to the
         destructor, but not if it's possible that the call will be through
         a virtual function pointer.  In this example:
           struct A { virtual ~A() {} };
           void f(A *pa) {
             A a;
             delete pa;
           }
         the destruction for "a" can be removed, but not for "pa". */
      result = FALSE;
    } else {
      for (; args != NULL; args = args->next) {
        if (node_has_side_effects(args, (a_boolean*)NULL)) {
          /* One of the arguments to this routine has a side effect so this
             invocation of this routine can't be elided (though it's still
             possible that other invocations may be elided).  Note that it
             may also be possible to elide the call to the constructor and
             execute the arguments (so their side-effects occur), but we don't
             do that. */
          result = FALSE;
          break;
        }  /* if */
      }  /* for */
    }  /* if */
#if IA64_ABI
    if (result &&
        routine->ctor_dtor_kind == (a_ctor_or_dtor_kind)cdk_delegation) {
      /* Typically, alternate entry points have the same setting for
         "has_no_effect" (because one is just a wrapper for the other).  In
         the case of delegating constructors, it's possible for there to be
         no user code in the delegating constructor, which creates a
         "common" cdk_delegation routine whose "has_no_effect" is correctly
         set to TRUE, but since the cdk_delegation routine is the primary
         routine (i.e., represents the entire construction operation even
         though the actual routine contains only the common code), make sure
         that any effects that the subobject or complete constructor have
         are taken into account. */
      a_routine_ptr subobject_routine = alternate_entry_point(routine,
                                            (a_ctor_or_dtor_kind)cdk_subobject,
                                            /*define_now=*/FALSE);
#if CHECKING
      a_routine_ptr complete_routine = alternate_entry_point(routine,
                                             (a_ctor_or_dtor_kind)cdk_complete,
                                             /*define_now=*/FALSE);
      check_assertion(subobject_routine->has_no_effect ==
                      complete_routine->has_no_effect);
#endif  /* CHECKING */
      result = subobject_routine->has_no_effect;
    }  /* if */
#endif /* IA64_ABI */
  }  /* if */
  return result;
}  /* call_to_ctor_or_dtor_has_no_effect */

#endif /* LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS */

static void ctor_or_dtor_statement_has_no_effect(
                                 a_statement_ptr                     statement,
                                 an_expr_or_stmt_traversal_block_ptr tblock)
/*
Called during a statement traversal of a (lowered or unlowered) constructor or
destructor to see if the routine has any effect.  Any "boilerplate" statements
added during the creation of the routine by lowering are deemed to have no
effect.
*/
{
  if (statement->is_lowering_boilerplate) {
    /* This statement has been added by lowering as part of the "normal"
       operation of any constructor or destructor, even constructors and
       destructors that have no effect, so it is ignored for our purposes. */
  } else {
    switch (statement->kind) {
      case stmk_empty:
      case stmk_block:
      case stmk_decl:
      case stmk_label:
        /* These statements have no effect. */
        break;
      case stmk_return:
        if (statement->expr != NULL) {
#if CTORS_RETURN_THIS || DTORS_RETURN_THIS
          if (is_variable_node(statement->expr) &&
              statement->expr->variant.variable->is_this_parameter) {
            /* It's okay for a constructor or destructor to return "this". */
          } else
#endif /* CTORS_RETURN_THIS || DTORS_RETURN_THIS */
          /* Do not insert code here. */
          {
            /* Assume a non-NULL expression means the routine has an effect. */
            tblock->result = FALSE;
          }  /* if */
        }  /* if */
        break;
      default:
        /* Assume everything else has an effect. */
        tblock->result = FALSE;
        break;
    }  /* switch */
    if (!tblock->result) tblock->terminate = TRUE;
  }  /* if */
}  /* ctor_or_dtor_statement_has_no_effect */


a_boolean ctor_or_dtor_body_has_no_effect(a_scope_ptr scope)
/*
Traverse the statements in the (lowered or unlowered) scope of a constructor or
destructor scope to determine if the routine has no effect when called with
"typical" arguments.  In some configurations, lowered constructors and
destructors will have various "boilerplate" statements added (see
lower_constructor_code and lower_destructor_code) which are ignored for the
purposes of determining whether or not the routine has an effect.

It's not always possible to know all cases where constructors or destructors
have no effect; for example, this case doesn't result in the destruction for
"a" being elided (because we don't know that ~A() is empty at that time --
though instances that appear after ~A()'s definition will be elided):

  struct A { ~A(); };
  void f() {
    A a;
  }
  inline A::~A() {}

This would require a multi-pass version of lowering.
*/
{
  an_expr_or_stmt_traversal_block tblock;

  check_assertion(scope->kind == (a_scope_kind)sck_function &&
                  scope->assoc_block != NULL);
  clear_expr_or_stmt_traversal_block(&tblock);
  tblock.process_statement = ctor_or_dtor_statement_has_no_effect;
  tblock.result = TRUE;
  traverse_statement(scope->assoc_block, &tblock);
#if DEBUG
  if (db_flag_is_set("remove_ctors_dtors")) {
    db_scope(scope);
    (void)fprintf(f_debug, " has %s effect.\n", tblock.result ? "no" : "an");
  }  /* if */
#endif /* DEBUG */
  return tblock.result;
}  /* ctor_or_dtor_body_has_no_effect */

#if LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS

static void remove_constructor_with_no_effect(a_dynamic_init_ptr dip)
/*
It's been determined that the constructor call specified in the dynamic
initialization has no effect; replace it with zero-initialization if needed,
otherwise specify no initialization.
*/
{
  check_assertion(dip->kind == (a_dynamic_init_kind)dik_constructor);
  if (need_zeroing_for_value_initialization(dip)) {
    dip->kind = (a_dynamic_init_kind)dik_zero;
  } else {
    dip->kind = (a_dynamic_init_kind)dik_none;
  }  /* if */
}  /* remove_constructor_with_no_effect */


static void remove_unneeded_destructions_from_lifetime(
                                               an_object_lifetime_ptr lifetime)
/*
Remove any unneeded destructions from the specified lifetime (and recursively
from any child lifetimes).  Note that as a result of removing unneeded
destructions, the object lifetime lifetime may become "useless", and
could potentially be removed from the IL tree.  Removing these useless
lifetimes would require re-visiting the object lifetimes that are stored
in goto and label statements and re-computing new common object lifetimes.
*/
{
  a_dynamic_init_ptr      dip, dip_next;
  an_object_lifetime_ptr  olp;

  check_assertion (lifetime != NULL);
  /* First, visit any child lifetimes. */
  for (olp = lifetime->child_lifetime; olp != NULL; olp = olp->next) {
    remove_unneeded_destructions_from_lifetime(olp);
  }  /* for */
  for (dip = lifetime->destructions;
       dip != NULL;
       dip = dip_next) {
    dip_next = dip->next_in_destruction_list;
    if (dip->destructor != NULL &&
        dip->destructor->special_kind ==
                                     (a_special_function_kind)sfk_destructor &&
        call_to_ctor_or_dtor_has_no_effect(dip->destructor,
                                           (an_expr_node_ptr)NULL,
                                           /*call_can_be_virtual=*/FALSE)) {
      /* This destruction isn't on a constructor_inits list and isn't
         needed so remove it from the destruction list (it will still
         be referred to by, e.g., a stmk_init for the initialization). */
      check_assertion(dip->destructible_entity_descr == NULL);
      remove_from_destruction_list(dip);
      dip->destructor = NULL;
    }  /* if */
  }  /* for */
}  /* remove_unneeded_destructions_from_lifetime */


void remove_unneeded_constructions_and_destructions(a_scope_ptr scope)
/*
This routine is called to remove certain unneeded constructions and
destructions from the specified scope before the scope is lowered.  In
particular it's important that any unneeded destructions that appear on the
destructions list for the scope are removed here (so that an exception handling
prologue can be avoided if possible).  During the constructor inits traversal,
unneeded constructor inits are also removed now (so they won't be unnecessarily
copied or moved later).  Other constructions and destructions (e.g., in
aggregates, stmk_inits, new/deletes) are processed during the
statement/expression lowering process (mostly by lower_dynamic_init).
*/
{
  an_object_lifetime_ptr  olp = scope->lifetime;
  a_dynamic_init_ptr      dip;
  a_constructor_init_ptr  ctor_init;
  a_constructor_init_ptr  prev = NULL;
  a_boolean               has_ctor_inits;
  a_routine_ptr           routine = scope->variant.routine.ptr;

  check_assertion(scope->kind == (a_scope_kind)sck_function);
  has_ctor_inits = ((routine->special_kind ==
                                    (a_special_function_kind)sfk_constructor ||
                     routine->special_kind ==
                                    (a_special_function_kind)sfk_destructor) &&
                     scope->variant.routine.constructor_inits != NULL);
#if DEBUG
  if (db_flag_is_set("remove_ctors_dtors")) {
    db_scope(scope);
    (void)fprintf(f_debug, "\nBefore: lifetime = ");
    db_object_lifetime(scope->lifetime);
    if (has_ctor_inits) {
      (void)fprintf(f_debug, "ctor_inits:\n");
      for (ctor_init = scope->variant.routine.constructor_inits;
           ctor_init != NULL;
           ctor_init = ctor_init->next) {
        db_dynamic_initializer(ctor_init->initializer, 0);
      }  /* for */
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  /* For scopes with constructor_inits (i.e., constructors and destructors),
     start with those and remove any calls to constructors and destructors
     that have no effect. */
  if (has_ctor_inits) {
    for (ctor_init = scope->variant.routine.constructor_inits;
         ctor_init != NULL;
         ctor_init = ctor_init->next) {
      /* Nonstatic data member initializers use a field initializer. */
      if (ctor_init->use_field_initializer) {
        dip = ctor_init->variant.field->initializer;
      } else {
        dip = ctor_init->initializer;
      }  /* if */
      check_assertion(dip != NULL);
      if (dip->kind == (a_dynamic_init_kind)dik_constructor &&
          call_to_ctor_or_dtor_has_no_effect(dip->variant.constructor.ptr,
                                             dip->variant.constructor.args,
                                             /*call_can_be_virtual=*/FALSE)) {
        /* There's no need to call this constructor; replace it with
           zero-initialization if the object is value-initialized otherwise
           no initialization is necessary. */
        remove_constructor_with_no_effect(dip);
      }  /* if */
      if (dip->destructor != NULL &&
          dip->destructor->special_kind ==
                                     (a_special_function_kind)sfk_destructor &&
          call_to_ctor_or_dtor_has_no_effect(dip->destructor,
                                             (an_expr_node_ptr)NULL,
                                             /*call_can_be_virtual=*/FALSE)) {
        /* This destruction has no effect and can be removed. */
        check_assertion(dip->destructible_entity_descr == NULL);
        remove_from_destruction_list(dip);
        dip->destructor = NULL;
      }  /* if */
      if (dip->kind == (a_dynamic_init_kind)dik_none &&
          dip->destructor == NULL) {
        /* If this dynamic initialization has no construction and no
           destruction, there's no need to keep it; unlink it from the list. */
        if (prev == NULL) {
          scope->variant.routine.constructor_inits = ctor_init->next;
        } else {
          prev->next = ctor_init->next;
        }  /* if */
      } else {
        prev = ctor_init;
      }  /* if */
    }  /* for */
  }  /* if */
  if (olp != NULL) {
    /* If there's an object lifetime associated with this function, examine
       each destruction in this lifetime and any child lifetimes. */
    remove_unneeded_destructions_from_lifetime(olp);
  }  /* if */
#if DEBUG
  if (db_flag_is_set("remove_ctors_dtors")) {
    (void)fprintf(f_debug, "After: lifetime = ");
    db_object_lifetime(scope->lifetime);
    if (has_ctor_inits) {
      (void)fprintf(f_debug, "ctor_inits:\n");
      for (ctor_init = scope->variant.routine.constructor_inits;
           ctor_init != NULL;
           ctor_init = ctor_init->next) {
        db_dynamic_initializer(ctor_init->initializer, 0);
      }  /* for */
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  if (routine->is_delegating_ctor &&
      scope->variant.routine.constructor_inits == NULL) {
    /* The target constructor for this delegating constructor does nothing and
       has been removed, making this no longer a delegating constructor. */
    routine->is_delegating_ctor = FALSE;
  }  /* if */
}  /* remove_unneeded_constructions_and_destructions */

#endif /* LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS */

static void stretch_partial_initialization_if_necessary(
                                   a_dynamic_init_ptr     dip,
                                   an_init_pos_descr_ptr  ipdp,
                                   a_boolean              have_complete_object,
                                   an_insert_location_ptr insert_location)
/*
The specified dynamic initialization of the entity represented by ipdp
has the is_partially_initialized flag set, indicating that the initializer
did not specify a value for every element of the aggregate.  The entity
points to a complete object if have_complete_object is TRUE.  If necessary,
generate code to initialize the elements of the aggregate (by adding code
at insert_location).  Note that this routine could be optimized to
zero-initialize only the elements/fields of the aggregate that are
un-initialized.
*/
{
  a_boolean     needs_initializing = FALSE;
  a_type_ptr    entity_type = f_skip_typerefs(type_from_init_pos_descr(ipdp));

  check_assertion(dip->is_partially_initialized &&
                  (dip->kind == (a_dynamic_init_kind)dik_constant ||
                   dip->kind ==
                              (a_dynamic_init_kind)dik_nonconstant_aggregate));
  if (ipdp->indirect_through_variable) {
    /* We're partially initializing an aggregate through a pointer,
       which could indicate a ctor-initializer or braced-initializer list for
       a new expression.  Partially initialized variables (with either static
       or automatic storage duration) are assumed to be zeroed by the back end
       (so no explicit zeroing is performed here). */
    if (is_array_type(entity_type)) {
      if (is_incomplete_array_type(entity_type)) {
        /* If we're initializing a variably-sized array whose size isn't known
           until run-time, initialization is needed. */
        needs_initializing = TRUE;
      } else if (dip->variant.constant->
                               type->variant.array.variant.number_of_elements <
                 entity_type->variant.array.variant.number_of_elements) {
        /* The constant doesn't entirely initialize the entity. */
        needs_initializing = TRUE;
      }  /* if */
    }  /* if */
#if IA64_ABI
    if (contains_ptr_to_data_member(entity_type)) {
      /* In the IA-64 ABI, pointers-to-data-members need to be initialized
         to -1.  Be safe and pre-initialize the entire entity (we could
         optimize this by seeing if only the un-initialized fields
         contain pointer-to-data-members). */
      needs_initializing = TRUE;
    }  /* if */
#endif /* IA64_ABI */
    if (dip->kind == (a_dynamic_init_kind)dik_nonconstant_aggregate) {
      /* A non-constant aggregate will be lowered to executable code (there
         will be no constant kept to initialize the entity). */
      needs_initializing = TRUE;
    }  /* if */
  }  /* if */
  if (needs_initializing) {
    insert_call_to_zero_entity(entity_type,
                               have_complete_object,
                               make_address_of_init_entity_node(ipdp,
                                                   /*using_as_dest=*/TRUE),
                               ipdp->num_elem_node,
                               (a_targ_size_t)0,
                               insert_location);
  }  /* if */
}  /* stretch_partial_initialization_if_necessary */


void lower_dynamic_init(a_dynamic_init_ptr     dip,
                        an_init_pos_descr_ptr  ipdp,
                        an_implied_copy_source *source_desc,
                        a_variable_ptr         construction_vtbls_var,
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

If the dynamic initialization has an implied copy, source_desc describes
the implied source of that copy (e.g., a constructor initializer).
source_desc is NULL otherwise.  In the case where source_desc describes a
constructor initializer, construction_vtbls_var provides the variable for a
array of construction virtual function tables, if needed, or NULL otherwise.

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
in this routine must be FALSE in that case.  *keep_dynamic_init will always
be set to TRUE if the dynamic init is for an element of a vector type;
these are kept in the aggregate because vector elements are not individually
addressable.  The caller should only provide a non-NULL keep_dynamic_init
value if it's okay to have an initial value for the variable (i.e., not
in the case of a temporary variable that may be reused).

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
  a_constant_ptr     simple_constant = NULL;
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
  a_boolean          local_keep_dynamic_init = FALSE;
  a_boolean          constructor_array_init = FALSE;
  a_variable_ptr     local_static_guard_var = NULL;
  a_boolean          do_simple_constant_init_opt = FALSE;
  a_boolean          simple_constant_init_opt_ruled_out = FALSE;
  a_boolean          local_static_that_requires_dynamic_init = FALSE;
  a_dynamic_init_ptr latest_initialization_on_entry = NULL;
  a_boolean          have_complete_object = TRUE;
  a_routine_ptr      ctor_routine;
#if GNU_VECTOR_TYPES_ALLOWED
  a_boolean          contains_vector_dynamic_init = FALSE;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
  a_constructor_init_ptr
                     ctor_init = NULL;
  a_dynamic_init_kind
                     orig_dip_kind = dip->kind;

  saved_code_pos = code_pos_for_lowering;
  saved_error_position = error_position;
  if (source_desc != NULL) ctor_init = source_desc->ctor_init;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (ctor_init != NULL && ctor_init->ctor_init_range.start.seq != 0) {
    /* Track the source position. */
    code_pos_for_lowering = error_position = ctor_init->ctor_init_range.start;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (constant_to_keep != NULL) *constant_to_keep = NULL;
#if LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS
  if (dip->kind == (a_dynamic_init_kind)dik_constructor &&
      call_to_ctor_or_dtor_has_no_effect(dip->variant.constructor.ptr,
                                         dip->variant.constructor.args,
                                         /*call_can_be_virtual=*/FALSE)) {
    /* There's no need to call this constructor; replace it with
       zero-initialization if the object is value-initialized otherwise
       no initialization is necessary. */
#if DEBUG
    if (db_flag_is_set("remove_ctors_dtors")) {
      (void)fprintf(f_debug, "Removing construction for: ");
      db_dynamic_initializer(dip, 0);
    }  /* if */
#endif /* DEBUG */
    remove_constructor_with_no_effect(dip);
  }  /* if */
#endif /* LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS */
  variable = dip->variable;
  if (dip->master_entry != NULL) {
    /* A dependent initialization isn't considered the initialization of
       the variable indicated as a whole. (One doesn't want to allow
       changing the initializer of that variable to indicate this
       initialization.) */
    variable = NULL;
  }  /* if */
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
    if (!static_var_init && keep_dynamic_init == NULL) {
      /* The optimization of rewriting a dynamic initialization to a constant
         is not allowed if the variable is automatic and the context is
         something other than an stmk_init (that's the keep_dynamic_init
         test). */
      simple_constant_init_opt_ruled_out = TRUE;
    }  /* if */
    /* See if this is a local static variable promoted out of an extern inline
       function (or template instantiated wherever used). */
    if (variable->promoted_local_static &&
        variable->storage_class == (a_storage_class)sc_unspecified
#if IA64_ABI
        && variable->comdat_group == NULL
#endif /* IA64_ABI */
                                         ) {
      local_static_that_requires_dynamic_init = TRUE;
      /* Don't allow this case to be turned into a simple constant
         initialization, because we want the variable to be a tentative
         definition (and therefore it must be uninitialized). */
      simple_constant_init_opt_ruled_out = TRUE;
    }  /* if */
    /* Decide whether the optimization of rewriting a dynamic initialization
       to a constant as a static initialization to the constant is allowed. */
    if (!simple_constant_init_opt_ruled_out &&
        dip->kind == (a_dynamic_init_kind)dik_constant) {
      do_simple_constant_init_opt = TRUE;
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
      /* Don't allow simple constant initialization for promoted local
         static variables (in cases where the constant contains a "troublesome"
         aggregate constant, the temporary that is created will be in the
         function scope and the variable has been promoted to the file
         scope).  This restriction might be able to be lifted in some cases. */
      do_simple_constant_init_opt = FALSE;
    }  /* if */
    /* For local static variables, add a first-time flag and a test,
       but not if the initialization will be turned into a constant
       initialization. */
    if (lsvip != NULL && !do_simple_constant_init_opt) {
      insert_location2 = *insert_location;
      add_first_time_test(variable, &insert_location2, insert_location,
                          &block_stmt, &local_static_guard_var);
    }  /* if */
  } else {
    /* Not whole variable initialization. */
    simple_constant_init_opt_ruled_out = TRUE;
    if (ctor_init != NULL &&
        (ctor_init->kind == (a_constructor_init_kind)cik_virtual_base_class ||
         ctor_init->kind == (a_constructor_init_kind)cik_direct_base_class)) {
      /* Initializing a base class, so not a complete object. */
      have_complete_object = FALSE;
    }  /* if */
    if (dip->is_optimized_class_rvalue_question_mark) {
      /* Note the destination position for use down the tree in
         lower_temp_init. */
      dip->init_destination = ipdp;
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
        check_assertion(block_stmt != NULL);
        bind_object_lifetime(local_static_lifetime, iek_block,
                             (char *)block_stmt->variant.block.extra_info);
      }  /* if */
      if (exceptions_enabled) {
        /* Add a dynamic init entry to represent the conditional flag.  This
           is turned into a region table entry that indicates that the
           conditional flag must be cleared if an exception is thrown before
           the initialization is completed. */
        check_assertion(local_static_guard_var != NULL);
        add_local_static_guard_var_cleanup(local_static_guard_var,
                                           local_static_lifetime,
                                           insert_location);
      }  /* if */
    }  /* if */
  }  /* if */
#if VLA_DEALLOCATION_REQUIRED
  if (is_dynamic_init_for_vla(dip)) {
    a_dynamic_init_ptr dealloc_dip;
    /* This initialization is for a variable-length array (VLA). */
    if (dip->destructor != NULL) {
      /* The VLA requires destruction of its elements.  We need to add
         a separate dynamic-init entry to represent the deallocation of
         the storage, because destruction and deallocation have to be
         distinct cleanup steps for exception handling. */
      dealloc_dip = add_vla_deallocation_dynamic_init(dip);
    } else {
      /* The VLA requires no destruction of its elements (e.g., it's
         an array of a non-class type or of a POD class).  The dynamic
         init entry becomes the indication of the deallocation point
         for the array. */
      dealloc_dip = dip;
      dip->is_vla_deallocation = TRUE;
    }  /* if */
    /* Update the cleanup information so that this entity will be
       deallocated at the appropriate time. */
    add_dyn_init_cleanup(dealloc_dip, ipdp, /*set_cond_flag_if_any=*/FALSE,
                         eff_context, eff_insert_location);
  }  /* if */
#endif /* VLA_DEALLOCATION_REQUIRED */
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
    a_boolean copy_lifetime = (processing_file_scope_init_routine && 
                               in_file_scope(init_expr_lifetime));
    push_init_expr_lifetime(&init_expr_lifetime,
                            copy_lifetime,
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
    } else if (dip->kind == (a_dynamic_init_kind)dik_nonconstant_aggregate) {
      a_constant_ptr aggr_con = dip->variant.constant;
      check_assertion(aggr_con != NULL);
      if (in_file_scope(aggr_con)) {
        dip->variant.constant = copy_constant_full(aggr_con, (a_constant*)NULL,
                                               CE_UNLINK_SOURCE_DESTRUCTIONS |
                                               CE_TRANSFER_DESTR_ENTITY_DESCR);
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
        if (is_aggregate_or_union_type(entity_type) ||
            is_or_was_ptr_to_member_function_type(entity_type) ||
            ipdp->array_element_sequence) {
          /* Aggregate.  Use a runtime routine call to zero it. */
          a_targ_size_t array_element_count = 0;
          if (ipdp->array_element_sequence) {
            array_element_count = ipdp->array_element_count;
            entity_type = ipdp->array_element_type;
          }  /* if */
          entity_node = make_address_of_init_entity_node(ipdp, 
                                                      /*using_as_dest=*/FALSE);
          insert_call_to_zero_entity(entity_type,
                                     have_complete_object,
                                     entity_node,
                                     (an_expr_node_ptr)NULL,
                                     array_element_count,
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
      if (do_simple_constant_init_opt && !dip->is_partially_initialized) {
        simple_constant_init = TRUE;
        simple_constant = dip->variant.constant;
        break;
      }  /* if */
#if GNU_VECTOR_TYPES_ALLOWED
      if (initializing_vector_element(ipdp->modifiers)) {
        /* We're initializing an element of a vector.  Normally, we'd
           create an assignment for this element, but since
           vector elements aren't individually addressable, keep the
           (now) lowered constant in the aggregate initializer. */
        check_assertion(constant_to_keep != NULL &&
                        !dip->is_partially_initialized);
        *constant_to_keep = dip->variant.constant;
        break;
      }  /* if */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
      if (dip->is_partially_initialized) {
        /* For cases where the initialization only partially covers the
           entity being initialized, initialize the remaining portion
           of the entity if necessary. */
        stretch_partial_initialization_if_necessary(dip, ipdp,
                                                    have_complete_object,
                                                    eff_insert_location);
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
            lower_c99_expr(source_node);
          }  /* if */
        }  /* if */
#endif /* DO_C99_IL_LOWERING */
      } else {
        if (dip->is_optimized_class_rvalue_question_mark) {
          /* For the optimized "?" class rvalue case, the expression must
             be evaluated (it initializes the temporary) but its value is
             not stored into the temporary. */
          lower_optimized_class_rvalue_question_mark(source_node,
                                                     eff_insert_location);
          break;
        }  /* if */
        if ((options & LDIO_FULL_EXPR) && init_expr_lifetime == NULL) {
          lower_full_expr(source_node, (a_statement_ptr)NULL);
        } else {
          /* Normal case: not a full expression (or a full expression
             with a non-null init_expr_lifetime). */
          lower_expr(source_node);
          if (options & LDIO_FULL_EXPR) {
            /* Make sure that end-of-full-expression processing is performed
               on expressions marked as full-expressions. */
            end_of_full_expr_processing(source_node);
          }  /* if */
        }  /* if */
        { a_constant con;
          if (!simple_constant_init_opt_ruled_out &&
              !processing_file_scope_init_routine &&
              is_pointer_type(source_node->type) &&
              constant_prvalue_pointer(source_node, &con,
                                       /*address_escapes=*/TRUE)) {
            /* The initial value is a simple constant.  Rewrite the
               initialization as a simple static initialization.  We can't do
               this optimization when we're processing file scope
               initializations (otherwise we may generate address constants
               that are not available at file-scope).  */
            simple_constant_init = TRUE;
            simple_constant = alloc_unshared_constant(&con);
            /* Even though the newly allocated constant is marked as having
               been lowered, it may contain a "troublesome" aggregate constant,
               so make sure it is truly lowered. */
            mark_as_not_visited(simple_constant);
            lower_os_constant(simple_constant);
            break;
          }  /* if */
        }
      }  /* if */
#if GNU_VECTOR_TYPES_ALLOWED
      if (initializing_vector_element(ipdp->modifiers)) {
        /* We're initializing an element of a vector.  Normally, we'd
           create an assignment statement for this element, but since
           vector elements aren't individually addressable, leave the
           (now) lowered dynamic expression in the aggregate initializer. */
        dip->variant.expression = source_node;
        local_keep_dynamic_init = TRUE;
        break;
      }  /* if */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
do_assignment:;
      /* Make a node for the entity to be initialized. */
      entity_node = make_init_entity_node(ipdp,
                                          /*result_is_lvalue=*/TRUE,
                                          /*using_as_dest=*/TRUE);
      add_init_assignment(dip, (a_constant *)NULL, entity_node,
                          have_complete_object, eff_insert_location,
                          (source_desc != NULL &&
                           source_desc->capture != NULL),
                          ipdp);
      break;
    case dik_call_returning_class_via_cctor:
      /* Initialize the entry by calling a routine that returns its result
         via a copy constructor. */
      /* The address of the temporary being initialized is added as an
         implicit argument of the call. */
      lower_call(dip->variant.expression, ipdp, (a_statement_ptr)NULL,
                 (a_boolean *)NULL);
      (void)insert_expr_statement_set_pos(dip->variant.expression,
                                          eff_insert_location);
#if IA64_ABI
      if (ctor_init != NULL &&
          dip->destructor != NULL &&
          dtor_needs_vtt_argument(dip->destructor) &&
          (ctor_init->kind == (a_constructor_init_kind)cik_virtual_base_class||
           ctor_init->kind == (a_constructor_init_kind)cik_direct_base_class)){
        /* This object is being used to initialize a base class and the
           (subobject) destructor for the base class needs a VTT pointer
           for the region table entry; create one here (though it's unused
           here, it is recorded in the dip and will be used when the
           region table entry for the destructor is created). */
        an_expr_node_ptr  dummy_vtt_ptr;
        build_construction_vtbls_pointer_for_subobject_construction(
                                                 dip,
                                                 ctor_init->variant.base_class,
                                                 ipdp,
                                                 construction_vtbls_var,
                                                 eff_insert_location,
                                                 &dummy_vtt_ptr,
                                                 (a_boolean *)NULL);
      }  /* if */
#endif /* IA64_ABI */
      break;
    case dik_constructor:
      /* Initialize the entity by calling a constructor. */
      /* The routine does not need to be lowered from here. */
      /* Make a node for the entity to be initialized. */
      entity_node = make_address_of_init_entity_node(ipdp,
                                                     /*using_as_dest=*/TRUE);
      /* Cast the entity node rvalue pointer to the right type to eliminate
         qualifier and type-as-subobject differences. */
      ctor_routine = dip->variant.constructor.ptr;
      ctor_routine_type = ctor_routine->type;
      ctor_routine_type = skip_typerefs(ctor_routine_type);
      this_param_type = implicit_this_param_type_of(ctor_routine_type);
      entity_node = add_cast_if_necessary(entity_node,
                                          f_skip_typerefs(this_param_type));
      source_node = NULL;
      param = NULL;
      if (dip->variant.constructor.is_copy_constructor_with_implied_source) {
        /* The constructor is a copy constructor, and the source of the
           copy is implied.  Determine the source location. */
        source_node = implied_source_of_copy(source_desc, ipdp,
                                             /*result_is_lvalue=*/TRUE);
        source_node = add_address_of_to_node(source_node);
        /* Cast the expression to the right type to eliminate qualifier and
           type-as-subobject differences.  Use the pointer version of
           the parameter reference type. */
        param = unlowered_param_type_list_for_routine(ctor_routine);
        source_node = add_cast_if_necessary(source_node,
                                            make_pointer_type(
                                                type_pointed_to(param->type)));
        /* Leave the parameter pointer set for lowering any additional
           arguments below. */
        param = param->next;
      }  /* if */
      if (ipdp->array_element_sequence) {
        /* Construct a sequence of array elements. */
        /* Note that dip->variant.constructor.args has not been lowered,
           which is what the subroutine requires. */
        add_array_constructor_call(dip, entity_node, source_node, ipdp,
                                   eff_insert_location);
      } else {
        /* Construct a simple entity (not an array). */
        /* Lower any added arguments.  Maintain the sequencing of the arguments
           in certain cases. */
        lower_arg_expr_list(dip->variant.constructor.args, ctor_routine_type,
                            ctor_routine, param,
                            dip->variant.constructor.has_sequenced_arguments,
                            eff_insert_location);
#if ABI_COMPATIBILITY_VERSION >= 233
        if (exceptions_enabled && (options & LDIO_THROW) &&
            dip->variant.constructor.is_implicit_copy_for_copy_initialization){
          /* This is the top-level copy of a throw, and it does the implied
             copy constructor call to copy the object to the runtime.  This is
             considered "inside" the throw, so we need to add code to tell the
             runtime that. */
          an_expr_node_ptr arg_node = dip->variant.constructor.args;
          check_assertion(arg_node != NULL);
          if (!is_invariant_expr(arg_node, /*vars_can_change=*/FALSE,
                                 /*treat_as_potential_prvalue=*/FALSE)) {
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
                             have_complete_object, ipdp, ctor_init,
                             construction_vtbls_var,
                             eff_insert_location);
      }  /* if */
      break;
    case dik_nonconstant_aggregate:
      /* Initialization with a nonconstant aggregate constant.  This is usually
         a whole-variable initialization, but can be used in a ctor-initializer
         or lambda capture to iterate over an array initialization, etc. */
      if (!C_mode()) {
        latest_initialization_on_entry = eff_context->latest_initialization;
      }  /* if */
      keep_constant = FALSE;
      if (dip->is_partially_initialized) {
        /* For cases where the initialization only partially covers the
           entity being initialized, initialize the remaining portion
           of the entity if necessary. */
        stretch_partial_initialization_if_necessary(dip, ipdp,
                                                    have_complete_object,
                                                    eff_insert_location);
      }  /* if */
      lower_dynamic_init_aggregate_constant(dip->variant.constant, ipdp,
                                            /*dtor_case=*/FALSE, source_desc,
                                            others_follow_in_aggr,
                                            eff_insert_location,
#if GNU_VECTOR_TYPES_ALLOWED
                                            &contains_vector_dynamic_init,
#else /* !GNU_VECTOR_TYPES_ALLOWED */
                                            (a_boolean *)NULL,
#endif /* GNU_VECTOR_TYPES_ALLOWED */
                                            &keep_constant,
                                            options);
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
#if GNU_VECTOR_TYPES_ALLOWED
          /* The constant may still have non-constant pieces (because vector
             elements can't be individually assigned to).  This "constant"
             will still be used as a static initial value. */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
          simple_constant_init = TRUE;
          simple_constant = dip->variant.constant;
          if (variable->promoted_local_static &&
              constant_must_remain_in_function_scope(simple_constant)) {
            /* The constant must remain in the function scope and can't be
               used to initialize the promoted static variable (now in the
               file scope).  Rewrite the initialization as executable code. */
            lower_constant_init_of_promoted_static(variable, simple_constant);
            simple_constant_init = FALSE;
          } else if (local_static_that_requires_dynamic_init) {
            /* A static variable of an extern inline function initialized
               to a constant.  The constant is the constant part of the
               nonconstant aggregate.  Insert an assignment to set the variable
               to the constant, preceding any generated initialization code.
               This is done because we want the variable to be a tentative
               definition, which means it must be uninitialized. */
            a_variable_ptr         temp_var;
            an_expr_node_ptr       init_val_node;
            a_memory_region_number region_to_switch_back_to;
            set_block_start_insert_location(block_stmt, &insert_location2);
            entity_node = make_init_entity_node(ipdp,
                                                /*result_is_lvalue=*/TRUE,
                                                /*using_as_dest=*/TRUE);
            check_assertion(simple_constant->kind ==
                            (a_constant_repr_kind)ck_aggregate &&
                            !in_file_scope(simple_constant) &&
                     !constant_must_remain_in_function_scope(simple_constant));
            /* Create a local static temporary and statically initialize it to
               the constant (but the constant must be copied to the file
               scope first). */
            temp_var = make_unnamed_local_static_variable(variable->type,
                                                   /*in_function_scope=*/TRUE);
            temp_var->init_kind = (an_init_kind)initk_static;
            switch_to_file_scope_region(&region_to_switch_back_to);
            temp_var->initializer.constant =
                           copy_constant_full(simple_constant,
                                              (a_constant_ptr)NULL,
                                              CE_REPLACE_STRINGS_BY_VARIABLES);
            switch_back_to_original_region(region_to_switch_back_to);
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
      /* Bitwise copy of a value. */
      if (dip->variant.bitwise_copy.source != NULL) {
        /* The source location is specified explicitly by the front end. */
        check_assertion(!C_mode());
        lower_expr(dip->variant.bitwise_copy.source);
        source_node = dip->variant.bitwise_copy.source;
      } else {
        /* The source location is implied.  This is used for copying members
           of classes in ctor-initializers of copy constructors, for the
           parameter of catch clauses, for captured lambda parameters, etc.
           ctor_init is non-NULL for the first of those cases. */
        /* Make an rvalue expression for the source entity. */
        source_node = implied_source_of_copy(source_desc, ipdp,
                                             /*result_is_lvalue=*/FALSE);
        if (is_array_type(source_node->type)) {
          /* For source locations with array type, an lvalue is required;
             get an lvalue representation for the implied source. */
          source_node = implied_source_of_copy(source_desc, ipdp,
                                               /*result_is_lvalue=*/TRUE);
        }  /* if */
      }  /* if */
      add_bitwise_copy(ipdp, source_node, have_complete_object,
                       eff_insert_location);
      break;
    default:
      unexpected_condition_str("lower_dynamic_init: bad kind");
  }  /* switch */
  /* If the dynamic init entry indicates a destructor call, it requires
     processing to get the destruction done at the right time. */
  if (dip->destructor != NULL) {
    check_assertion(dip->lifetime != NULL);
    if (static_var_init &&
        !dip->destruction_is_for_partially_constructed_aggregate) {
      if (dip->kind == (a_dynamic_init_kind)dik_nonconstant_aggregate &&
          processing_file_scope_init_routine && !C_mode()) {
        /* Now that the static aggregate has been fully constructed, remove
           any destructions for partially constructed aggregates that may
           still be a part of the cleanup state. */
        adjust_cleanup_state_for_static_aggregate_init(
                                               latest_initialization_on_entry);
      }  /* if */
      /* For static variables (local or global), generate code to record
         at runtime the need for a destruction later. */
      record_needed_destruction(dip, ipdp, eff_insert_location);
    } else {
      /* Initializations of nonstatic variables. */
      a_destructible_entity_descr_ptr dedp = dip->destructible_entity_descr;
      check_assertion_str(dedp != NULL, "lower_dynamic_init: missing dedp");
      dedp->initialization_done = TRUE;
      /* coverity[uninit_use] */
      if (dip->kind == (a_dynamic_init_kind)dik_nonconstant_aggregate &&
          latest_initialization_on_entry !=
                                          eff_context->latest_initialization) {
        /* This is an aggregate for which some partial-aggregate
           initializations were done.  Adjust the cleanup state now that
           the entire aggregate is completed.  Note that this cleans up
           partial-aggregate entries on the same level as the aggregate
           itself; any partial-aggregate entries within an inner lifetime
           are handled by adjust_cleanup_state_for_inner_lifetime_temporaries,
           called from add_dyn_init_cleanup below. */
        a_boolean some_cloned, some_ordered;
        check_assertion(!dip->overlaps_temps_in_inner_lifetime);
        adjust_cleanup_state_for_aggregate_init(
                                            eff_context->latest_initialization,
                                            latest_initialization_on_entry,
                                            eff_insert_location,
                                            &some_cloned,
                                            &some_ordered);
      }  /* if */
      if (dip->destruction_is_for_partially_constructed_aggregate &&
          init_expr_lifetime == NULL &&
          !others_follow_in_aggr) {
        /* As an optimization, don't emit a cleanup entry for a partial
           initialization in an aggregate if it is not followed by anything
           else, because there is no code executed after the partial
           initialization and before the initialization is completed where an
           exception could be thrown.  Exclude cases where init_expr_lifetime
           is non-NULL because it's possible that user code (in the form
           of a destructor) could be invoked. */
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
#if IA64_ABI
  if (lsvip != NULL) {
    /* Set the guard variable to indicate the local static is initialized
       after the initialization is completed. */
    set_local_static_guard_var(local_static_guard_var, insert_location);
  }  /* if */
#endif /* IA64_ABI */
  /* In the whole-variable cases, adjust the initialization specified in
     the variable (it points to the dynamic init entry). */
  if (variable != NULL) {
    if (simple_constant_init) {
      /* Initialization to a simple constant, including a fully-constant
         aggregate. */
      /* See if an implicit cast is necessary for this constant (or any
         sub-aggregate piece thereof). */
      add_cast_for_cv_qualified_cctor_param_if_necessary(simple_constant);
      if (static_var_init) {
        /* Initialization of a static variable to a constant.  Can be
           done as a static initialization. */
        /* If this variable is a local static variable that was promoted
           to file scope, we have to copy the remaining constant to the file
           scope (it was formerly pointed to by a local-static-variable-init
           entry in the function scope, and then the variable was promoted
           by promote_local_entities_to_file_scope). */
        if (!in_file_scope(simple_constant)) {
          a_memory_region_number region_to_switch_back_to = NULL_region_number;
          switch_to_file_scope_region(&region_to_switch_back_to);
          simple_constant = copy_unshared_constant(simple_constant);
          switch_back_to_original_region(region_to_switch_back_to);
        }  /* if */
        variable->init_kind = (an_init_kind)initk_static;
        variable->initializer.constant = simple_constant;
      } else {
        /* Initialization of an automatic variable to a constant.  Can be done
           by keeping the dynamic init entry. */
        local_keep_dynamic_init = TRUE;
#if GNU_VECTOR_TYPES_ALLOWED
        if (contains_vector_dynamic_init) {
          check_assertion(dip->kind ==
                               (a_dynamic_init_kind)dik_nonconstant_aggregate);
          /* Leave this dip as a dik_nonconstant_aggregate. */
        } else
#endif /* GNU_VECTOR_TYPES_ALLOWED */
        /* Do not insert code here. */
        {
          set_dynamic_init_kind(dip, (a_dynamic_init_kind)dik_constant);
        }  /* if */
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
         a library.  If the variable is dllimport-ed, it cannot have an
         initializer, but the initialized-vs-tentative definition issue does
         not arise.  There is also an issue with automatic variables that
         are aggregates: if the initialization was partial, we have to be
         sure the rest of the aggregate is initialized to zero.
         So we change the initialization kind to initialization to zero. */
      if ((static_var_init && !variable->source_corresp.is_local_to_function &&
           force_variable_definition_via_zeroing && !C_mode() &&
#if MICROSOFT_EXTENSIONS_ALLOWED
           (variable->decl_modifiers & DM_DLLIMPORT) == 0 &&
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
           !local_static_that_requires_dynamic_init) ||
          dip->is_partially_initialized) {
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
  } else if (local_keep_dynamic_init &&
             (orig_dip_kind == (a_dynamic_init_kind)dik_nonconstant_aggregate||
              orig_dip_kind == (a_dynamic_init_kind)dik_constant) &&
             variable != NULL) {
    /* The variable doesn't currently have an stmk_init statement, but
       needs one because some portion of the initialization is being kept;
       add an stmk_init to initialize the variable. */
    add_stmk_init_for_temp_init(variable, dip);
  } else {
    check_assertion_str(!local_keep_dynamic_init,
   "lower_dynamic_init: keep_dynamic_init param NULL and want to return TRUE");
  }  /* if */
}  /* lower_dynamic_init */


void lower_constant_init_of_promoted_static(a_variable_ptr variable,
                                            a_constant_ptr constant)
/*
The given variable is a local static variable that is initialized to the
specified constant.  The initialization must be rewritten because the variable
is in an extern inline function (or a template instantiated wherever used), or
the constant cannot be copied to the file scope (i.e., it contains a reference
to a GNU address label).  Rewrite the initialization as executable code so that
the variable (already promoted to the file scope and made external) can be a
tentative definition (i.e., uninitialized).  The executable code is placed at
the beginning of the block associated with the innermost function scope.  The
constant can be either in the file or function scope.
*/
{
  an_insert_location    insert_location;
  an_expr_operator_kind op;
  an_expr_node_ptr      source_node;
  a_statement_ptr       block_stmt, assign_stmt;
  a_variable_ptr        test_var;
  a_source_position     saved_error_position, saved_code_pos;

  check_assertion(variable->promoted_local_static);
  saved_code_pos = code_pos_for_lowering;
  saved_error_position = error_position;
  code_pos_for_lowering = error_position =
                                        variable->source_corresp.decl_position;
  check_assertion(constant != NULL);
  if (in_file_scope(constant)) {
    /* Make sure pointers-to-members in the constant get lowered when the
       file scope is lowered. */
    possibly_add_orphaned_file_scope_il_entry((char *)constant, iek_constant);
  }  /* if */
  variable->init_kind = (an_init_kind)initk_none;
  variable->initializer.constant = NULL;
  /* The general strategy is to add an assignment that copies the constant
     value into the variable. */
  if (constant->kind != (a_constant_repr_kind)ck_aggregate &&
      !is_array_type(variable->type)) {
    /* For the simple, non-aggregate case, the constant can be assigned
       directly. */
    source_node = make_node_for_il_constant(constant);
    check_assertion(!source_node->is_lvalue);
    op = (an_expr_operator_kind)eok_assign;
  } else {
    /* For aggregate cases, create an unnamed temporary that
       gets the original initialization, then use an eok_bassign to
       copy that to the initial variable.  This avoids taking the
       address of an aggregate constant, which is not allowed in the
       IL (except for string literals). */
    a_variable_ptr temp_var;
    if (in_file_scope(constant)) {
      temp_var = make_file_scope_temporary(variable->type);
      temp_var->init_kind = (an_init_kind)initk_static;
      temp_var->initializer.constant = constant;
    } else {
      /* The aggregate constant has some portion that requires it to stay
         in the function scope (i.e., a GNU address label).  Create a
         local static temporary rather than a file scope temporary. */
      temp_var = make_unnamed_local_static_variable(variable->type,
                                                   /*in_function_scope=*/TRUE);
      (void)make_local_static_variable_init(temp_var, curr_context->scope,
                                            (an_init_kind)initk_static,
                                            constant,
                                            (a_dynamic_init_ptr)NULL);
    }  /* if */
    op = (an_expr_operator_kind)eok_bassign;
    source_node = var_lvalue_expr(temp_var);
  }  /* if */
  /* The WP [stmt.dcl] paragraph 3 says "A local object of POD type with
     static storage duration initialized with constant-expressions is
     initialized before its block is first entered."  Non-POD type
     variables can also be initialized early in some cases.
     In some cases, the start of the block in which the variable is declared
     isn't reachable (for example in a switch statement), so insert the
     additional executable statements at the beginning of the block associated
     with the function scope (the variable has already been promoted to
     file scope). */
  check_assertion(innermost_function_scope != NULL &&
                  innermost_function_scope->assoc_block != NULL);
  set_block_start_insert_location(innermost_function_scope->assoc_block,
                                                             &insert_location);
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
}  /* lower_constant_init_of_promoted_static */


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
virtual table table pointer that should be passed to the destructor
(IA-64 ABI only).  The statements are inserted at *insert_location and
*insert_location is updated.
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

#if RUNTIME_SUPPORTS_ARRAY_LENGTH_CHECK && ABI_COMPATIBILITY_VERSION >= 406

static a_routine_ptr
                throw_bad_array_new_length_routine;
                        /* Pointer to __throw_bad_array_new_length runtime
                           routine. */


static void insert_runtime_array_length_check(
                                        a_dynamic_init_ptr     dip,
                                        a_type_ptr             elem_type,
                                        a_routine_ptr          new_routine,
                                        an_expr_node_ptr       *num_elem_node,
                                        an_insert_location_ptr insert_location)
/*
Insert code to validate the value of the number of elements being allocated
by an array new operation.  This is only done when the number of elements
being allocated is not known at compilation time.  If the number of elements
is too large, or too small (less than zero for signed types or less than
the number of initializers provided), then std::bad_array_new_length is
thrown.  dip describes any dynamic initialization being performed for this
allocation (and may be NULL).  elem_type gives the underlying element type
for the array.  If new_routine is non-NULL, it is the placement new routine
that is being called to allocate the memory.  *num_elem_node is an expression
for the total number of elements being allocated; it is replaced with an
expression for a temporary that gives the same value.  insert_location gives
the position to insert the necessary code.
*/
{
  an_insert_location  then_insert_location;
  an_expr_node_ptr    lt_node, test_node, call_node, temp_node;
  an_expr_node_ptr    prefix_size_node = NULL, max_elem_node;
  a_constant          zero_constant, elem_size_constant, max_elements_constant;
  a_boolean           err;
  a_variable_ptr      temp;
  a_type_ptr          num_elements_type = (*num_elem_node)->type;

  check_assertion(exceptions_enabled);
  /* Create a temporary for the number of elements in the array (because
     that value will typically be used multiple times in this routine). */
  temp = make_lowered_temporary(num_elements_type);
  temp_node = make_assignment_expr(var_lvalue_expr(temp),
                                   (an_expr_operator_kind)eok_assign,
                                   *num_elem_node);
  (void)insert_expr_statement(temp_node, insert_location);
  *num_elem_node = var_rvalue_expr(temp);
  /* Compute the maximum number of elements that an array of the specified
     element type can have, i.e.,
     (targ_size_t_max - sizeof(cookie))/sizeof(array element). */
  check_assertion(elem_type->size != 0);
  set_unsigned_integer_constant(&elem_size_constant,
                                (a_host_large_integer)elem_type->size,
                                targ_size_t_int_kind);
  set_unsigned_integer_constant(&max_elements_constant,
                                (a_host_large_integer)targ_size_t_max,
                                targ_size_t_int_kind);
#if ABI_CHANGES_FOR_PLACEMENT_DELETE
  prefix_size_node = get_prefix_size_node(elem_type, new_routine);
#endif /* ABI_CHANGES_FOR_PLACEMENT_DELETE */
  if (prefix_size_node != NULL) {
    /* A cookie is required.  If the cookie size is known, subtract it
       now, otherwise create an expression to do the subtraction. */
    if (is_constant_node(prefix_size_node)) {
      a_constant_ptr  cookie_size = prefix_size_node->variant.constant;
      check_assertion(cookie_size->type->kind == (a_type_kind)tk_integer);
      subtract_integer_values(&max_elements_constant.variant.integer_value,
                              &(cookie_size->variant.integer_value),
                              /*is_signed=*/FALSE, &err);
      check_assertion(!err);
      prefix_size_node = NULL;
    }  /* if */
  }  /* if */
  if (prefix_size_node == NULL) {
    /* No cookie, or a cookie whose size is known at compile time (and has
       already been subtracted above). */
    divide_integer_values(&max_elements_constant.variant.integer_value,
                          &elem_size_constant.variant.integer_value,
                          /*is_signed=*/FALSE, &err);
    check_assertion(!err);
    max_elem_node = alloc_node_for_constant(&max_elements_constant);
  } else {
    /* The cookie size isn't known at compile time; create an expression
       to perform the subtraction and division. */
    max_elem_node = alloc_node_for_constant(&max_elements_constant);
    max_elem_node->next = prefix_size_node;
    max_elem_node = make_operator_node((an_expr_operator_kind)eok_subtract,
                                       max_elem_node->type,
                                       max_elem_node);
    max_elem_node->next = alloc_node_for_constant(&elem_size_constant);
    max_elem_node = make_operator_node((an_expr_operator_kind)eok_divide,
                                       max_elem_node->type,
                                       max_elem_node);
  }  /* if */
  /* Make "num_elements > max_elements". */
  temp_node = var_rvalue_expr(temp);
  temp_node->next = max_elem_node;
  test_node = make_operator_node((an_expr_operator_kind)eok_gt,
                               integer_type((an_integer_kind)ik_int),
                               temp_node);
  if (dip != NULL && dip->is_braced_initializer) {
    /* This initialization has a braced initializer.  Make sure that the
       number of elements that have been allocated is at least as large
       as the number of initializers. */
    check_assertion(dip->is_partially_initialized &&
                    (dip->kind == (a_dynamic_init_kind)dik_constant ||
                     dip->kind == (a_dynamic_init_kind)
                                                  dik_nonconstant_aggregate) &&
                    is_array_type(dip->variant.constant->type));
    /* Add "|| num_elements < num_initializers" to the test above. */
    temp_node = var_rvalue_expr(temp);
    temp_node->next = node_for_host_large_integer(
                         (a_host_large_integer)
                               num_array_elements(dip->variant.constant->type),
                         targ_ptrdiff_t_int_kind);
    lt_node = make_operator_node((an_expr_operator_kind)eok_lt,
                                 integer_type((an_integer_kind)ik_int),
                                 temp_node);
    test_node->next = lt_node;
    test_node = make_operator_node((an_expr_operator_kind)eok_lor,
                                   integer_type((an_integer_kind)ik_int),
                                   test_node);
  } else if (is_signed_integral_type(num_elements_type)) {
    /* Add "|| num_elements < 0" to the test. */
    temp_node = var_rvalue_expr(temp);
    make_zero_of_proper_type(num_elements_type, &zero_constant);
    temp_node->next = alloc_node_for_constant(&zero_constant);
    lt_node = make_operator_node((an_expr_operator_kind)eok_lt,
                                 integer_type((an_integer_kind)ik_int),
                                 temp_node);
    test_node->next = lt_node;
    test_node = make_operator_node((an_expr_operator_kind)eok_lor,
                                   integer_type((an_integer_kind)ik_int),
                                   test_node);
  }  /* if */
  /* If the tests fails, invoke __throw_bad_array_new_length to
     throw std::bad_array_new_length. */
  insert_if_statement(test_node,
                      /*is_initialization_guard=*/FALSE,
                      insert_location,
                      (a_statement_ptr *)NULL,
                      &then_insert_location,
                      (an_insert_location *)NULL);
  call_node = make_runtime_rout_call(
#if IA64_ABI
                                     "__cxa_throw_bad_array_new_length",
#else /* !IA64_ABI */
                                     "__throw_bad_array_new_length",
#endif /* IA64_ABI */
                                     &throw_bad_array_new_length_routine,
                                     void_star_type(),
                                     (an_expr_node_ptr)NULL);
  insert_expr(call_node, &then_insert_location);
}  /* insert_runtime_array_length_check */

#endif /* RUNTIME_SUPPORTS_ARRAY_LENGTH_CHECK && ABI_COMPATIBILITY_VERSION...*/

static an_expr_node_ptr size_arg_for_new(
                                   a_new_delete_supplement_ptr ndsp,
                                   an_expr_node_ptr            *num_elem_node,
                                   an_insert_location_ptr      insert_location)
/*
Returns the size argument for a new or array new operation.  The new/delete
supplement (ndsp) contains a list of arguments for the operation, but the first
argument -- the total number of bytes to allocate -- is not provided by the
front end.  This routine creates that node.  Additionally, when num_elem_node
is non-NULL, an expression node (with the proper type for passing as an
argument to run-time routines) representing the number of elements in the array
is returned (further reusable copies of this node may be made).
insert_location points to the place to insert code that is generated to
initialize required temporary variables and must occur before any of the
temporary values are used (i.e., before any run-time library calls).  Note that
the caller often discards the first argument that is created herein, so
make_reusable_copy can't be used (instead, a temporary is explicitly created
and its initialization put in insert_location).  Note that the returned
expression has an indeterminate integral type (which may be signed or
unsigned).
*/
{
  an_expr_node_ptr      number_of_elements, number_of_bytes, temp_node;
  a_variable_ptr        temp;
  a_type_ptr            elem_type, underlying_elem_type;
  a_constant            constant;

  number_of_elements = ndsp->number_of_elements;
  if (number_of_elements != NULL) {
    /* An array new where the number of elements is specified at run time. */
    check_assertion(is_incomplete_array_type(ndsp->type) &&
                    is_integral_or_unscoped_enum_type(
                                                   number_of_elements->type) &&
                    !number_of_elements->is_lvalue);
    elem_type = array_element_type(ndsp->type);
    underlying_elem_type =
                          new_delete_base_type_from_operation_type(ndsp->type);
    lower_expr(number_of_elements);
    if (is_array_type(elem_type)) {
      /* For a multi-dimensional array, the total number of elements is
         determined by multiplying by the number of elements of the
         underlying array (which may itself be multi-dimensional). */
      an_integer_kind multiply_int_kind;
      if (is_unscoped_enum_type(number_of_elements->type)) {
        /* Use the underlying type for an unscoped enum type. */
        number_of_elements = add_cast(number_of_elements,
                                      integer_type(skip_typerefs(
                                                    number_of_elements->type)->
                                                    variant.integer.int_kind));
      }  /* if */
      /* The expression representing the number of elements may be any
         integral type (signed or unsigned).  Make sure the multiplication
         operation is performed on operands with the same type. */
      multiply_int_kind = is_signed_integral_type(number_of_elements->type) ?
                                                      targ_ptrdiff_t_int_kind :
                                                      targ_size_t_int_kind;
      number_of_elements = add_cast_if_necessary(number_of_elements,
                                                integer_type(
                                                           multiply_int_kind));
      temp_node = node_for_host_large_integer(
                           (a_host_large_integer)num_array_elements(elem_type),
                           multiply_int_kind);
      number_of_elements->next = temp_node;
      number_of_elements = make_operator_node(
                                           (an_expr_operator_kind)eok_multiply,
                                           number_of_elements->type,
                                           number_of_elements);
    }  /* if */
#if RUNTIME_SUPPORTS_ARRAY_LENGTH_CHECK && ABI_COMPATIBILITY_VERSION >= 406
    if (exceptions_enabled && cpp11_mode) {
      /* Insert code to check, at run-time, that the number of elements
         has a valid value; throw std::bad_array_new_length otherwise. */
      insert_runtime_array_length_check(ndsp->dynamic_init,
                                        underlying_elem_type,
                                        ndsp->routine,
                                        &number_of_elements,
                                        insert_location);
    }  /* if */
#endif /* RUNTIME_SUPPORTS_ARRAY_LENGTH_CHECK && ABI_COMPATIBILITY_VERSION...*/
    /* Add a cast to size_t. */
    number_of_elements = add_cast_if_necessary(number_of_elements,
                                               integer_type(
                                                        targ_size_t_int_kind));
    if (num_elem_node != NULL) {
      /* If the caller requests, create a temporary that captures the
         number of elements in a suitably typed reusable node. */
      temp = make_lowered_temporary(number_of_elements->type);
      temp_node = make_assignment_expr(var_lvalue_expr(temp),
                                       (an_expr_operator_kind)eok_assign,
                                       number_of_elements);
      (void)insert_expr_statement(temp_node, insert_location);
      temp_node = var_rvalue_expr(temp);
      number_of_elements = var_rvalue_expr(temp);
      temp_node = add_cast_if_necessary(temp_node,
                                        integer_type(
#if IA64_ABI
                                          targ_size_t_int_kind
#else /* !IA64_ABI */
                                          targ_runtime_elem_count_int_kind
#endif /* IA64_ABI */
                                                                          ));
      *num_elem_node = temp_node;
    }  /* if */
    /* Multiply the number of elements by the size of an underlying element to
       get the number of bytes. */
    if (underlying_elem_type->size == 1) {
      /* If the underlying element size is 1, skip the multiplication. */
      number_of_bytes = number_of_elements;
    } else {
      check_assertion(underlying_elem_type->size != 0);
      temp_node = node_for_host_large_integer(
                              (a_host_large_integer)underlying_elem_type->size,
                              targ_size_t_int_kind);
      number_of_elements->next = temp_node;
      number_of_bytes = make_operator_node((an_expr_operator_kind)eok_multiply,
                                           temp_node->type,
                                           number_of_elements);
    }  /* if */
  } else {
    /* Non-array case, or array with constant size; size is known.  Note
       that in some cases (e.g., some Microsoft modes), an incomplete
       type can get here (resulting in a size of zero). */
    set_unsigned_integer_constant_with_overflow_check(&constant,
                                               skip_typerefs(ndsp->type)->size,
                                               targ_size_t_int_kind,
                                               (a_type_ptr)NULL,
                                               /*preserve_needed_flag=*/FALSE);
    number_of_bytes = alloc_node_for_constant(&constant);
    if (num_elem_node != NULL && is_array_type(ndsp->type)) {
      /* An array new where the number of elements is specified at compile
         time.  Note that the total number of elements (all dimensions
         for multi-dimensional arrays) is returned. */
      set_unsigned_integer_constant_with_overflow_check(
                                              &constant,
                                              num_array_elements(ndsp->type),
#if IA64_ABI
                                              targ_size_t_int_kind,
#else /* !IA64_ABI */
                                              targ_runtime_elem_count_int_kind,
#endif /* IA64_ABI */
                                              (a_type_ptr)NULL,
                                              /*preserve_needed_flag=*/FALSE);
      number_of_elements = alloc_node_for_constant(&constant);
      *num_elem_node = number_of_elements;
    }  /* if */
  }  /* if */
  return number_of_bytes;
}  /* size_arg_for_new */


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
  an_expr_node_ptr            entity_node, new_node, test_node = NULL;
  an_expr_node_ptr            assign_node, num_elem_node, vec_new_node;
  a_constant                  null_constant;
  a_variable_ptr              temp_var, new_temp_var = NULL;
  an_expr_node_ptr            size_node;
  a_routine_ptr               ctor_routine, dtor_routine, delete_routine;
  an_insert_location          insert_location, pre_call_insert_location;
  an_expr_node_ptr            args, delete_args = NULL;
  a_boolean                   zero_storage = FALSE;
  a_boolean                   needs_dynamic_initialization = FALSE;
#if ABI_CHANGES_FOR_PLACEMENT_DELETE
  an_expr_node_ptr            prefix_size_node = NULL;
#endif /* ABI_CHANGES_FOR_PLACEMENT_DELETE */

  /* Get the array element type. */
  array_type = skip_typerefs(ndsp->type);
  elem_type = new_delete_base_type_from_operation_type(ndsp->type);
  ptr_elem_type = make_pointer_type(elem_type);
  set_expr_creation_insert_location(&insert_location);
  set_expr_creation_insert_location(&pre_call_insert_location);
#if !NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE
 #error -- NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE wrong
#endif /* !NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE */
  /* For new operations, the front end doesn't specify the first
     "argument" to the operation -- build that now and also get an
     expression for the number of elements in the array (which we'll need
     to pass to a run-time routine). */
  args = size_arg_for_new(ndsp, &num_elem_node, &pre_call_insert_location);
  args->next = ndsp->arg;
  /* Build the node for the address of the array (entity_node). */
  if (!ndsp->placement_new) {
    /* This is a normal (not placement) new, the usual case.  The __vec_new
       routine should do the allocation of the array. */
    /* Note that new_routine might be non-NULL here, if the allocation
       requires a non-default "operator new[]" i.e., a class-specific one.
       __array_new will be called, and is given a pointer to the allocation
       routine to use. */
    /* There should be no arguments in this case. */
    check_assertion(ndsp->arg == NULL);
    entity_node = NULL;  /* Allocate in __vec_new. */
  } else {
    /* This is a placement new, so the allocation must be done before
       calling the __vec_new routine.  This happens for something like
         A *p = new (x, y, z) A[3];
       The "new" call is assigned to a temporary, and entity_node uses
       the temporary, as in
         (temp = (type *)new-call(...)) ? (type *)__vec_new(temp, ...) : NULL
    */
    /* Prepare the argument list for the "new" call.  Note that the first
       argument was created during lowering (but is lowered anyway). */
    check_assertion_str(new_routine != NULL,
                       "lower_array_new: placement new with null new_routine");
    lower_arg_expr_list(args, new_routine->type, new_routine,
                        (a_param_type_ptr)NULL, /*maintain_sequencing=*/FALSE,
                        (an_insert_location *)NULL);
#if ABI_CHANGES_FOR_PLACEMENT_DELETE
    if (dip != NULL && ndsp->freeing_of_storage_on_exception != NULL) {
      /* This is a placement new for which there is a corresponding placement
         delete.  Make a copy of the argument list for the new call, to
         be used in the delete call.  Note that this is done after IL lowering,
         so the argument expressions are evaluated only once.  But that
         also means temporaries used to pass class objects via copy
         constructor are shared. */
      /* Note that the copy skips the first argument (the size). */
      delete_args = copy_arg_list_for_placement_delete(args->next);
    }  /* if */
#endif /* ABI_CHANGES_FOR_PLACEMENT_DELETE */
    size_node = args;
#if ABI_CHANGES_FOR_PLACEMENT_DELETE
    /* Add the size of the runtime prefix used to keep track of the array
       size to the argument for the operator new[] call. */
    { an_expr_node_ptr size_node_next = size_node->next;
      prefix_size_node = get_prefix_size_node(elem_type, new_routine);
      if (prefix_size_node != NULL) {
#if !IA64_ABI
        prefix_size_node = add_cast_if_necessary(prefix_size_node,
                                                 size_node->type);
#endif /* !IA64_ABI  */
        size_node->next = prefix_size_node;
        size_node = make_operator_node((an_expr_operator_kind)eok_add,
                                       size_node->type, size_node);
        size_node->next = size_node_next;
      }  /* if */
    }
#endif /* ABI_CHANGES_FOR_PLACEMENT_DELETE */
    /* Make the "new" call. */
    new_node = make_call_node(new_routine, size_node);
    /* Make "temp = (type *)new-call(...)". */
    temp_var = make_local_temporary(ptr_elem_type);
    assign_node = make_var_assignment_expr(temp_var,
                                           add_cast_if_necessary(new_node,
                                                               ptr_elem_type));
    test_node = boolean_controlling_expr(assign_node);
#if ABI_CHANGES_FOR_PLACEMENT_DELETE
    /* Add the array prefix size to get from the address returned to
       the actual starting address of the array. */
    if (prefix_size_node != NULL) {
      an_expr_node_ptr temp_var_node, add_node;

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
      assign_node = make_var_assignment_expr(temp_var, add_node);
      insert_expr(assign_node, &insert_location);
    }  /* if */
#endif /* ABI_CHANGES_FOR_PLACEMENT_DELETE */
    new_routine = NULL;  /* Allocation done outside of __vec_new. */
    entity_node = var_rvalue_expr(temp_var);
  }  /* if */
  /* Here, we have entity_node pointing to an expression for the address
     of the entity, or entity_node == NULL if the __vec_new call or
     equivalent will be allocating the storage.  Also, num_elem_node
     is an expression for the number of elements in the array. */
  /* Determine the constructor routine (if any) to be called. */
  if (dip != NULL && dip->kind != (a_dynamic_init_kind)dik_zero) {
    /* There is a dynamic init entry to initialize the storage after it is
       allocated.  dik_zero initialization is handled below. */
    if (ndsp->new_initializer_is_brace_enclosed) {
      /* The array needs to be initialized after it is allocated, but it
         can't be done by a call to the runtime routine.  Indicate that
         dynamic initialization is needed after the storage is allocated. */
      ctor_routine = NULL;
      dtor_routine = NULL;
      needs_dynamic_initialization = TRUE;
    } else {
      /* The array can be initialized by performing the same initialization
         for every element in the array. */
      elem_dip = elem_dynamic_init(dip);
      check_assertion(elem_dip != NULL);
      if (elem_dip->kind == (a_dynamic_init_kind)dik_constructor) {
        /* All elements of the array receive the same initialization treatment
           so we can use a call to a runtime routine to initialize the
           entire array by calling the constructor for each element. */
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
#if LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS
        /* Remove unneeded construction/destructions if possible. */
        if (call_to_ctor_or_dtor_has_no_effect(
                                            elem_dip->variant.constructor.ptr,
                                            elem_dip->variant.constructor.args,
                                            /*call_can_be_virtual=*/FALSE)) {
          /* There's no need to call this constructor (zero_storage has already
             been set above if zero-initialization is required). */
#if DEBUG
          if (db_flag_is_set("remove_ctors_dtors")) {
            (void)fprintf(f_debug, "Removing array new construction for: ");
            db_dynamic_initializer(elem_dip, 0);
          }  /* if */
#endif /* DEBUG */
          remove_constructor_with_no_effect(elem_dip);
          ctor_routine = NULL;
        }  /* if */
        if (elem_dip->destructor != NULL &&
            call_to_ctor_or_dtor_has_no_effect(elem_dip->destructor,
                                               (an_expr_node_ptr)NULL,
                                               /*call_can_be_virtual=*/FALSE)){
          /* There's no need to call this destructor; any deletions that
             may be necessary (i.e., a throw during construction) are handled
             by the delete routine. */
#if DEBUG
          if (db_flag_is_set("remove_ctors_dtors")) {
            (void)fprintf(f_debug, "Removing array new destruction for: ");
            db_dynamic_initializer(elem_dip, 0);
          }  /* if */
#endif /* DEBUG */
          elem_dip->destructor = NULL;
        }  /* if */
#endif /* LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS */
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
        /* In this case a repeated constant is being used to initialize
           the array; the initialization will be performed for each element
           of the array. */
        check_assertion(elem_dip->kind == (a_dynamic_init_kind)dik_constant);
        ctor_routine = NULL;
        dtor_routine = NULL;
        needs_dynamic_initialization = TRUE;
      }  /* if */
    }  /* if */
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
  if (needs_dynamic_initialization ||
      (dip != NULL && dip->kind == (a_dynamic_init_kind)dik_zero)) {
    /* The newly allocated storage must be initialized in some way.  The
       address has to be saved in a temporary and then returned after
       being properly initialized. */
    new_temp_var = make_lowered_temporary(make_pointer_type(array_type));
    /* Assign the result of the "new" call to the temporary. */
    vec_new_node = make_var_assignment_expr(new_temp_var,
                                            add_cast_if_necessary(vec_new_node,
                                               make_pointer_type(array_type)));
  }  /* if */
  insert_expr(vec_new_node, &insert_location);
  if (dip != NULL && dip->kind == (a_dynamic_init_kind)dik_zero) {
    /* Generate a runtime routine call to zero the entity. */
    a_type_ptr       eff_type = array_type;
    an_expr_node_ptr eff_num_elem_node = NULL;
    if (array_type->size == 0) {
      eff_type = elem_type;
      eff_num_elem_node = make_reusable_copy(num_elem_node,
                                             /*vars_can_change=*/TRUE);
    }  /* if */
    insert_call_to_zero_entity(eff_type,
                               /*have_complete_object=*/TRUE,
                               var_rvalue_expr(new_temp_var),
                               eff_num_elem_node,
                               (a_targ_size_t)0,
                               &insert_location);
    /* Insert the value of the temporary as the final value of the
       expression. */
    insert_expr(var_rvalue_expr(new_temp_var), &insert_location);
  } else if (needs_dynamic_initialization) {
    /* Need dynamic initialization of the storage that has just been allocated;
       build a description of the entity to be initialized (as pointed to
       by the temporary variable).  Adjust the type so that it is an array. */
    an_init_pos_descr ipd;
    an_insert_location init_insert_location;
    set_var_indirect_init_pos_descr(new_temp_var, &ipd);
    ipd.base_type = array_type;
    if (is_incomplete_array_type(array_type)) {
      /* For a variably-sized array, create a run-time expression for the
         number of elements in the array. */
      ipd.num_elem_node = make_reusable_copy(num_elem_node,
                                             /*vars_can_change=*/TRUE);
    }  /* if */
    /* If exceptions are enabled, and if necessary, set up to free the
       storage allocated if an exception is thrown before the storage
       is initialized. */
    set_up_freeing_of_storage_on_exception(ndsp, &ipd, &insert_location);
    /* Generate code for the initialization. */
    set_expr_creation_insert_location(&init_insert_location);
    lower_dynamic_init(dip, &ipd,
                       (an_implied_copy_source *)NULL,
                       (a_variable_ptr)NULL,
                       LDIO_NONE,
                       /*others_follow_in_aggr=*/FALSE,
                       &init_insert_location,
                       (a_boolean *)NULL,
                       (a_constant **)NULL);
    /* Now that the entity is initialized, turn off the freeing on
       exception. */
    turn_off_freeing_of_storage_on_exception(ndsp, &ipd, delete_args,
                                             new_routine,
                                             init_insert_location.variant.expr,
                                             &insert_location);
    /* Insert the value of the temporary as the final value of the
       expression. */
    insert_expr(var_rvalue_expr(new_temp_var), &insert_location);
  }  /* if */
  vec_new_node = insert_location.variant.expr;
  if (ndsp->placement_new) {
    /* Placement new.  Add the "?" operator over the whole expression. */
    test_node->next = vec_new_node;
    make_zero_of_proper_type(vec_new_node->type, &null_constant);
    vec_new_node->next = alloc_node_for_constant(&null_constant);
    vec_new_node = make_operator_node((an_expr_operator_kind)eok_question,
                                      vec_new_node->type, test_node);
  }  /* if */
  /* Make sure that code created to set/check the number of elements
     occurs early in the initialization. */
  if (pre_call_insert_location.variant.expr != NULL) {
    vec_new_node = make_comma_node(pre_call_insert_location.variant.expr,
                                   vec_new_node);
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
  an_expr_node_ptr            dtor_addr_node, assign_node = NULL;
  an_expr_node_ptr            ptr_node_test = NULL;
  a_variable_ptr              vtbl_temp_var;

  /* Lower "arg". */
  lower_expr(ptr_node);
  if (dip != NULL) {
    /* A destructor must be called. */
    dtor_routine = dip->destructor;
    check_assertion(dtor_routine != NULL);
#if IA64_ABI
    dtor_routine = alternate_entry_point(dtor_routine,
                                         (a_ctor_or_dtor_kind)cdk_complete,
                                         /*define_now=*/FALSE);
#endif /* IA64_ABI */
    if (dtor_routine->is_virtual && (gnu_mode || microsoft_mode)) {
      /* We're doing an array delete on a class that has a virtual destructor.
         GNU and Microsoft use the destructor address found in the
         virtual function table.  This can cause errors at runtime if the
         sizes of the base and derived classes are not the same. */
      /* Dispatch through the virtual function table requires a
         null-pointer test.  Force use of a temporary which will be used
         later in the null-pointer test. */
      ptr_node_test = ptr_node;
      ptr_node = make_reusable_copy(ptr_node, /*vars_can_change=*/FALSE);
      /* Cast the expression to a pointer-to-element type (it typically
         already is, but may be a pointer-to-array type in some non-standard
         cases). */
      ptr_node = add_cast_if_necessary(ptr_node, make_pointer_type(
                                      new_delete_base_type_from_operation_type(
                                            type_pointed_to(ptr_node->type))));
      dtor_addr_node = get_virtual_function_address(
                                        function_addr_expr(dtor_routine),
                                        &ptr_node,
                                        /*object_node_has_side_effects=*/FALSE,
                                        &vtbl_temp_var,
                                        &assign_node);
    } else {
      /* Build an expression for the address of the destructor. */
      dtor_addr_node = expr_for_pointer_to_destructor(dtor_routine);
    }  /* if */
  } else {
    /* There is no dynamic init entry, and therefore no destruction need be
       done along with the deallocation. */
    dtor_addr_node = expr_for_pointer_to_destructor(NULL);
  }  /* if */
  vec_delete_node = make_vec_delete_call(ptr_node,
                                         /*num_elem_node=*/
                                                        (an_expr_node_ptr)NULL,
                                         dtor_addr_node,
                                         delete_routine,
                                         /*free_storage=*/TRUE);
  if (assign_node != NULL) {
    /* If an assignment to a temporary was necessary, create a comma node
       to perform the assignment before the temporary is used. */
    vec_delete_node = make_comma_node(assign_node, vec_delete_node);
  }  /* if */
  if (ptr_node_test != NULL) {
    /* Add a null pointer test, producing
         ptr_node ? vec_delete(...) : (void)0
         ^ plus possible assignment to temporary here
    */
    /* Make "ptr_node ? vec_delete(...) : (void)0". */
    ptr_node_test = boolean_controlling_expr(ptr_node_test);
    ptr_node_test->next = vec_delete_node;
    vec_delete_node->next = zero_cast_to_void();
    vec_delete_node = make_operator_node((an_expr_operator_kind)eok_question,
                                         vec_delete_node->type, ptr_node_test);
  }  /* if */
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
    if (ndsp->placement_new || dyn_init_to_free_storage->is_array_freeing) {
      /* These cases can't be handled by the runtime library; instead they
         are handled later, by inserting an internal "try" block. */
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
                             a_routine_ptr               new_routine,
                             an_expr_node_ptr            init_expr,
                             an_insert_location          *insert_location)
/*
ndsp points to the new/delete supplement for a "new".  We're now at a
location after the initialization related to the "new" has been done,
so do the second part of the processing begun by
set_up_freeing_of_storage_on_exception.  ipdp describes the location of the
allocated storage.  delete_args points to the list of arguments for a placement
delete call, if one is needed.  If new_routine is non-NULL, it is the placement
new routine that is being called to allocate the memory.

In the case of an initialized array (e.g., "new A[4] {1, 2}"), the caller
has generated two logical pieces of code: a run-time call to allocate the
array, and some executable code to initialize the array.  If an exception is
thrown during the allocation, the run-time library will handle the
exception (and this routine doesn't need to), but an exception thrown during
the initialization must be handled here.  That's not the case for the
placement new case (where the run-time routine doesn't do any exception
handling and this routine must generate proper cleanup for the allocation
phase as well).  init_expr represents the initialization code, if any, that the
caller has generated to initialize the entity.  *insert_location fills two
roles: on entry it is an expression insert location (created by
set_expr_creation_insert_location) that contains only the allocation of the
entity; on exit it will be updated to add the initialization (i.e., init_expr)
as well as any additional code needed to process the deletion.
*/
{
  an_expr_node_ptr   delete_call, alloc_expr, try_expr;
#if ABI_CHANGES_FOR_PLACEMENT_DELETE
  an_expr_node_ptr   prefix_size_node = NULL;
#endif /* ABI_CHANGES_FOR_PLACEMENT_DELETE */
  a_dynamic_init_ptr dyn_init_to_free_storage =
                                         ndsp->freeing_of_storage_on_exception;

  check_assertion(is_expr_insert_location(insert_location));
  if (dyn_init_to_free_storage != NULL) {
    alloc_expr = insert_location->variant.expr;
    if (ndsp->placement_new || dyn_init_to_free_storage->is_array_freeing) {
      /* Generally speaking, deletion is handled through region table entries
         but there are two cases that are handled here that use an internal
         "try" block with a "catch" to do the requisite deletion. */
      if (alloc_expr == NULL && init_expr == NULL) {
        /* If neither allocation nor initialization generated any code,
           there's nothing to do. */
      } else {
        an_expr_node_ptr entity_node = make_address_of_init_entity_node(ipdp, 
                                                      /*using_as_dest=*/FALSE);
#if ABI_CHANGES_FOR_PLACEMENT_DELETE
        if (is_array_type(ndsp->type) &&
            new_or_delete_type_requires_array_handling(
                          new_delete_base_type_from_operation_type(ndsp->type),
                          /*check_constructor=*/TRUE)) {
          /* Get the size of a prefix if there is one. */
          prefix_size_node = get_prefix_size_node(
                                                array_element_type(ndsp->type),
                                                new_routine);
          if (prefix_size_node != NULL) {
            /* Subtract the array prefix size. */
            entity_node = add_cast_if_necessary(entity_node, char_star_type());
            entity_node->next = prefix_size_node;
            entity_node = make_operator_node(
                                          (an_expr_operator_kind)eok_psubtract,
                                          entity_node->type,
                                          entity_node);
          }  /* if */
        }  /* if */
#endif /* ABI_CHANGES_FOR_PLACEMENT_DELETE */
        /* Cast the argument to "void *", which is what the delete routine
           expects. */
        entity_node = add_cast_if_necessary(entity_node, void_star_type());
        /* Put a pointer to the allocated storage on the front of the argument
           list for the delete routine.  Add any placement delete args if
           necessary. */
        entity_node->next = delete_args;
        /* Make a call of the appropriate delete routine. */
        delete_call = make_call_node(dyn_init_to_free_storage->destructor,
                                     entity_node);
        if (ndsp->placement_new) {
          /* Placement delete.  In the placement delete case, code must be
             generated to delete the entity if a failure occurs anywhere
             during the allocation or initialization process (the runtime
             library does not do any deletion during a throw -- because it
             doesn't know the arguments that need to be passed to the placement
             delete routine).  */
          try_expr = make_comma_node_if_necessary(alloc_expr, init_expr);
          try_expr = make_internal_try_expr(try_expr, delete_call);
        } else {
          /* Freeing an initialized array.  This case comes up when a repeated
             constant initializer or braced-initializer is used to initialize
             an array (e.g., "new A[4] {1, 2}").  In such cases, the allocation
             and initialization phases are handled separately.  The allocation
             portion is handled by the runtime library and any exception that
             occurs during that period is handled by the runtime library.  The
             initialization portion must be covered by the internal "try/catch"
             mechanism here (but not the allocation portion -- otherwise there
             would be multiple deletes in some cases). */
          check_assertion(dyn_init_to_free_storage->is_array_freeing);
          if (init_expr == NULL) {
            try_expr = init_expr;
          } else {
            try_expr = make_internal_try_expr(init_expr, delete_call);
            try_expr = make_comma_node_if_necessary(alloc_expr, try_expr);
          }  /* if */
        }  /* if */
        if (try_expr != NULL) {
          /* Give back to the caller an insert location that allows insertion
             after the overall expression as modified. */
          set_expr_creation_insert_location(insert_location);
          insert_expr(try_expr, insert_location);
        }  /* if */
      }  /* if */
    } else {
      a_destructible_entity_descr_ptr dedp =
                           dyn_init_to_free_storage->destructible_entity_descr;
      if (init_expr != NULL) {
        /* Add any initialization to the allocation. */
        insert_expr(init_expr, insert_location);
      }  /* if */
      /* Normal, non-placement, non-array delete case. */
      if (dedp->conditional_flag_var != NULL) {
        /* Reset the flag that indicates that the freeing must be done. */
        reset_conditional_flag_var(dedp->conditional_flag_var,
                                   insert_location);
      }  /* if */
    }  /* if */
  } else {
    if (init_expr != NULL) {
      /* Add any initialization to the allocation. */
      insert_expr(init_expr, insert_location);
    }  /* if */
  }  /* if */
}  /* turn_off_freeing_of_storage_on_exception */


static a_boolean is_constructor_call(an_expr_node_ptr expr)
/*
Return TRUE if the indicated expression is a call of a constructor.
*/
{
  a_boolean is_ctor_call = FALSE;

  if (is_operation_node(expr) &&
      expr->variant.operation.kind == (an_expr_operator_kind)eok_call) {
    an_expr_node_ptr first_operand = expr->variant.operation.operands;
    if (is_routine_node(first_operand)) {
      a_routine_ptr rout = first_operand->variant.routine.ptr;
      if (rout->special_kind == (a_special_function_kind)sfk_constructor) {
        is_ctor_call = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return is_ctor_call;
}  /* is_constructor_call */

#if ABI_CHANGES_FOR_PLACEMENT_DELETE

void treat_as_placement_new_if_has_default_args(
                                              a_new_delete_supplement_ptr ndsp)
/*
ndsp is additional information related to a "new" operation.  If the
operator new routine associated with this operation has default arguments,
treat it as a placement new.  See core issue 127.
*/
{
  if (!ndsp->placement_new && ndsp->routine != NULL) {
    a_param_type_ptr params =
                          unlowered_param_type_list_for_routine(ndsp->routine);
    if (params != NULL && params->next != NULL) {
      check_assertion_str(params->next->has_default_arg,
                          "placement_new not set but more than one arg");
      ndsp->placement_new = TRUE;
    }  /* if */
  }  /* if */
}  /* treat_as_placement_new_if_has_default_args */

#endif /* ABI_CHANGES_FOR_PLACEMENT_DELETE */

static void lower_new(an_expr_node_ptr expr)
/*
Do IL lowering of an enk_new_delete expression node for a "new".
The subtree of the node has not yet been lowered.
*/
{
  a_new_delete_supplement_ptr ndsp = expr->variant.new_delete;
  a_dynamic_init_ptr          dip = ndsp->dynamic_init;
  a_type_ptr                  base_type, ptr_new_type;
  a_variable_ptr              temp_var;
  an_expr_node_ptr            assign_node, test_node, args;
  an_expr_node_ptr            num_elem_node = NULL, *eff_num_elem_node = NULL;
  an_expr_node_ptr            init_node, call_node, null_node, delete_args;
  a_constant                  null_constant;
  an_insert_location          insert_location, pre_call_insert_location;

#if ABI_CHANGES_FOR_PLACEMENT_DELETE
  /* Treat an operator new with default arguments as a placement new.
     See core issue 127. */
  treat_as_placement_new_if_has_default_args(ndsp);
#endif /* ABI_CHANGES_FOR_PLACEMENT_DELETE */
  base_type = new_delete_base_type_from_operation_type(ndsp->type);
  if (is_array_type(ndsp->type) &&
      new_or_delete_type_requires_array_handling(base_type,
                                                 /*check_constructor=*/TRUE)) {
    /* An array "new". */
    lower_array_new(expr);
  } else if (ndsp->routine == NULL) {
#if NEW_CAN_BE_FOLDED_INTO_CTOR
#if IA64_ABI
 #error -- IA64_ABI requires NEW_CAN_BE_FOLDED_INTO_CTOR set to FALSE
#endif /* IA64_ABI */
#if LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS
    /* No attempt is made to remove an unneeded constructor here because
       the "new" call is made by the constructor, hence it's always needed. */
#endif /* LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS */
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
    make_ctor_implied_arg_list(ctor_routine, /*is_target_ctor=*/FALSE,
                               &implied_arg_list, &end_implied_arg_list);
    if (implied_arg_list != NULL) {
      null_node->next = implied_arg_list;
    } else {
      end_implied_arg_list = null_node;
    }  /* if */
    /* Preserve any additional parameters from the constructor call. */
    if (dip->variant.constructor.args != NULL) {
      check_assertion(!dip->variant.constructor.has_sequenced_arguments);
      lower_arg_expr_list(dip->variant.constructor.args,
                          ctor_routine->type, ctor_routine,
                          (a_param_type_ptr)NULL,
                          /*maintain_sequencing=*/FALSE,
                          (an_insert_location *)NULL);
      end_implied_arg_list->next = dip->variant.constructor.args;
    }  /* if */
    /* Make the constructor call. */
    call_node = make_call_node(ctor_routine, null_node);
    /* The constructor call returns a pointer to the object initialized.
       Cast the pointer to the right type if necessary. */
    call_node = add_cast_if_necessary(call_node, expr->type);
    /* Overwrite the enk_new_delete node with the call/cast. */
    overwrite_node(expr, call_node);
#else /* !NEW_CAN_BE_FOLDED_INTO_CTOR */
    /* The routine should not be NULL if the new cannot be folded into a
       constructor. */
    unexpected_condition();
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
  } else {
    /* Non-array case, or array case that does not require special handling. */
    /* Lower the arguments for the "new" call.  If the arguments need
       sequencing, that'll be handled later when the dip is lowered. */
    lower_arg_expr_list(ndsp->arg, ndsp->routine->type, ndsp->routine,
                        (a_param_type_ptr)NULL, /*maintain_sequencing=*/FALSE,
                        (an_insert_location *)NULL);
    set_expr_creation_insert_location(&pre_call_insert_location);
    if (dip != NULL && is_incomplete_array_type(ndsp->type)) {
      /* For initializations of variably-sized arrays, create a temporary
         that contains the number of elements in the array. */
      eff_num_elem_node = &num_elem_node;
    }  /* if */
    /* The first argument (number of bytes to allocate) needs to be
       computed. */
    args = size_arg_for_new(ndsp, eff_num_elem_node,
                            &pre_call_insert_location);
    args->next = ndsp->arg;
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
      /* This case is also used for an operator new call with default
         arguments (it is treated like a placement new).  See
         initial_processing_on_destructible_initialization. */
      delete_args = copy_arg_list_for_placement_delete(args->next);
    }  /* if */
    /* Create a call of the "new" routine. */
    call_node = make_call_node(ndsp->routine, args);
#if LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS
    if (dip != NULL &&
        dip->kind == (a_dynamic_init_kind)dik_constructor &&
        call_to_ctor_or_dtor_has_no_effect(dip->variant.constructor.ptr,
                                           dip->variant.constructor.args,
                                           /*call_can_be_virtual=*/FALSE)) {
#if DEBUG
      if (db_flag_is_set("remove_ctors_dtors")) {
        (void)fprintf(f_debug, "Removing new construction for: ");
        db_dynamic_initializer(dip, 0);
      }  /* if */
#endif /* DEBUG */
      /* There's no need to call this constructor. */
      remove_constructor_with_no_effect(dip);
    }  /* if */
#endif /* LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS */
    /* Note that the type of the "new" call might be unrelated to the type
       we are allocating, e.g., it might be "void *"; a cast is done later. */
    if (dip != NULL && dip->kind != (a_dynamic_init_kind)dik_none) {
      /* Initialization is required.  It must be done only if the allocation
         succeeds, so build an expression like
           ((temp = (type *)new-call(...)) != NULL) ?
                                     (initialization, temp) : NULL
      */
#if CTORS_RETURN_THIS
      a_boolean is_constructor_init =
                           (dip->kind == (a_dynamic_init_kind)dik_constructor);
#endif /* CTORS_RETURN_THIS */
      /* Allocate the temporary. */
      ptr_new_type = make_pointer_type(ndsp->type);
      temp_var = make_local_temporary(ptr_new_type);
      /* Assign the entity address expression to the temporary. */
      assign_node = make_var_assignment_expr(temp_var,
                                             add_cast_if_necessary(call_node,
                                                                ptr_new_type));
      set_expr_creation_insert_location(&insert_location);
      if (is_array_type(ndsp->type) &&
          dip->kind == (a_dynamic_init_kind)dik_zero &&
          skip_typerefs(ndsp->type)->size == 0) {
        /* lower_dynamic_init can't handle a variable-length array, so
           do that specially. */
        an_expr_node_ptr entity_size_node =
                                  make_reusable_copy(args,
                                                     /*vars_can_change=*/TRUE);
        insert_runtime_zeroing_call(var_rvalue_expr(temp_var),
                                    entity_size_node,
                                    &insert_location);
      } else {
        /* Build a description of the entity to be initialized.  Adjust the
           type so that it is an array if necessary. */
        an_init_pos_descr  ipd;
        an_insert_location init_insert_location;
        set_var_indirect_init_pos_descr(temp_var, &ipd);
        ipd.base_type = ndsp->type;
        if (is_incomplete_array_type(ndsp->type)) {
          /* For a variably-sized array, use the run-time expression for the
             number of elements in the array. */
          check_assertion(num_elem_node != NULL);
          ipd.num_elem_node = num_elem_node;
        }  /* if */
        /* If exceptions are enabled, and if necessary, set up to free the
           storage allocated if an exception is thrown before the storage
           is initialized. */
        set_up_freeing_of_storage_on_exception(ndsp, &ipd, &insert_location);
        /* Generate code for the initialization. */
        set_expr_creation_insert_location(&init_insert_location);
        lower_dynamic_init(dip, &ipd,
                           (an_implied_copy_source *)NULL,
                           (a_variable_ptr)NULL,
                           LDIO_NONE,
                           /*others_follow_in_aggr=*/FALSE,
                           &init_insert_location, (a_boolean *)NULL,
                           (a_constant **)NULL);
        /* Now that the entity is initialized, turn off the freeing on
           exception. */
        turn_off_freeing_of_storage_on_exception(ndsp, &ipd, delete_args,
                                             (a_routine_ptr)NULL,
                                             init_insert_location.variant.expr,
                                             &insert_location);
      }  /* if */
      {
#if CTORS_RETURN_THIS
        a_boolean can_optimize_away_temp = FALSE;
        if (is_constructor_init) {
          /* Try to use the result of the constructor call directly
             instead of the value of the temporary (the constructor
             returns the address of the object). */
          if (is_constructor_call(insert_location.variant.expr)) {
            can_optimize_away_temp = TRUE;
          }  /* if */
        }  /* if */
        if (!can_optimize_away_temp)
#endif /* CTORS_RETURN_THIS */
        /* Do not insert code here. */
        {
          /* End the initialization code with an expression that gets the
             value of the temporary. */
          init_node = var_rvalue_expr(temp_var);
          insert_expr(init_node, &insert_location);
        }  /* if */
      }
      init_node = insert_location.variant.expr;
      /* Build the ?: operation.  Its first argument is the test of the temp
         pointer; its second is the initialization code; and its third is a
         NULL constant of the right type. */
      test_node = boolean_controlling_expr(assign_node);
      make_zero_of_proper_type(ptr_new_type, &null_constant);
      null_node = alloc_node_for_constant(&null_constant);
      test_node->next = init_node;
      init_node->next = null_node;
      call_node = make_operator_node((an_expr_operator_kind)eok_question,
                                     ptr_new_type, test_node);
    }  /* if */
    if (pre_call_insert_location.variant.expr != NULL) {
      /* If there was any code generated to initialize the number of
         elements, insert it before any use. */
      call_node = make_comma_node(pre_call_insert_location.variant.expr,
                                  call_node);
    }  /* if */
    /* Turn the original enk_new_delete node into a cast to the right
       pointer type. */
    change_to_cast(expr, call_node, expr->type);
  }  /* if */
}  /* lower_new */


static an_expr_node_ptr modify_delete_call_args(
                                         a_routine_ptr      delete_routine,
                                         a_type_ptr         delete_type,
                                         an_expr_node_ptr   arg_node)
/*
Returns the modified argument list for a delete call to routine delete_routine.
A modification is necessary if the delete routine is one with two arguments
(in which case a size argument is added).  delete_type is the type of the
object being deleted.  arg_node is the argument list being passed to the
delete routine.
*/
{
  an_expr_node_ptr second_arg_node;

  /* Cast the argument to "void *", which is what the delete routine
     expects. */
  arg_node = add_cast_if_necessary(arg_node, void_star_type());
  /* If the delete routine is one with two arguments, pass the size
     of the entity as the second argument. */
  if (is_two_argument_delete(delete_routine)) {
    /* Two-argument form.  Add a second argument of type size_t that
       indicates the (static) size of the object. */
    second_arg_node = node_for_host_large_integer(
                    (a_host_large_integer)(f_skip_typerefs(delete_type)->size),
                    targ_size_t_int_kind);
    arg_node->next = second_arg_node;
  }  /* if */
  return arg_node;
}  /* modify_delete_call_args */


static an_expr_node_ptr make_delete_call_node(a_routine_ptr    delete_routine,
                                              a_type_ptr       delete_type,
                                              an_expr_node_ptr arg_node)
/*
Create an expression for a call of the delete routine indicated by
delete_routine, with arg_node as the argument.  delete_type is the type
of the object being deleted.  Return a pointer to the call expression (which
may have been inlined).
*/
{
  an_expr_node_ptr call_node;

  /* Make the call. */
  arg_node = modify_delete_call_args(delete_routine, delete_type, arg_node);
  call_node = make_call_node(delete_routine, arg_node);
  return call_node;
}  /* make_delete_call_node */


static void make_delete_call_statement(a_routine_ptr      delete_routine,
                                       a_type_ptr         delete_type,
                                       an_expr_node_ptr   arg_node,
                                       an_insert_location *insert_location)
/*
Create a statement for a call of the delete routine indicated by
delete_routine, with arg_node as the argument and insert the statement
at the location indicated by *insert_location.  delete_type is the type
of the object being deleted.
*/
{
  /* Make the call. */
  arg_node = modify_delete_call_args(delete_routine, delete_type, arg_node);
  make_call_statement(delete_routine, arg_node, (an_expr_node_ptr)NULL,
                      insert_location);
  return;
}  /* make_delete_call_statement */


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
  an_expr_node_ptr ptr_node_test = NULL, call_node, temp_assign_node = NULL;
  an_expr_node_ptr ptr_node_delete = NULL;
  a_type_ptr       class_type;
  a_routine_ptr    dtor_routine = dip->destructor;
  a_boolean        need_null_ptr_test = FALSE;
#if !IA64_ABI
  long             bit_mask;
#endif /* !IA64_ABI */

  check_assertion(dtor_routine != NULL &&
                  dtor_routine->source_corresp.is_class_member);
  class_type = parent_class_of(dtor_routine);
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
  if (dtor_routine->is_virtual && delete_routine == NULL) {
    dtor_routine = alternate_entry_point(dtor_routine,
                                         (a_ctor_or_dtor_kind)cdk_deleting,
                                         /*define_now=*/FALSE);
  } else {
    check_assertion(delete_routine != NULL);
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
    a_boolean vars_can_change = FALSE;
#if !DTORS_RETURN_THIS
    if (delete_routine != NULL) vars_can_change = TRUE;
#endif /* !DTORS_RETURN_THIS */
    ptr_node_test = ptr_node;
    ptr_node = make_reusable_copy(ptr_node, vars_can_change);
  }  /* if */
#if !DTORS_RETURN_THIS
  if (delete_routine != NULL) {
    /* Make a copy of the object pointer so that we can use it later in
       building the call of the delete routine. */
    ptr_node_delete = make_reusable_copy(ptr_node, /*vars_can_change=*/TRUE);
  }  /* if */
#endif /* !DTORS_RETURN_THIS */
#if ABI_CHANGES_FOR_RTTI
  if (delete_routine != NULL &&
      !delete_routine->source_corresp.is_class_member &&
      dtor_routine->is_virtual) {
    /* A non-class operator delete (presumably ::delete) is being used to
       delete an object with a virtual destructor.  Use the specified object
       pointer to invoke the virtual destructor, but use a pointer to the most
       derived object when invoking the delete routine.  Assign the value of
       dynamic_cast<void *>(object pointer) to a temporary (before the call
       of the virtual destructor), then use that value in the call to
       ::delete. */
    a_variable_ptr temp;
    check_assertion(need_null_ptr_test);
#if DTORS_RETURN_THIS
    /* A copy was not made above, so make one now. */
    ptr_node_delete = make_reusable_copy(ptr_node_test,
                                         /*vars_can_change=*/TRUE);
#endif /* DTORS_RETURN_THIS */
    /* Create a dynamic_cast<void *> and lower it. */
    ptr_node_delete = make_operator_node(
                                       (an_expr_operator_kind)eok_dynamic_cast,
                                       void_star_type(), ptr_node_delete);
    lower_dynamic_cast(ptr_node_delete);
    /* Assign the result of the dynamic_cast to a temporary for use later. */
    temp = make_lowered_temporary(void_star_type());
    temp_assign_node = make_var_assignment_expr(temp, ptr_node_delete);
    ptr_node_delete = var_rvalue_expr(temp);
  }  /* if */
#endif /* ABI_CHANGES_FOR_RTTI */
#if !IA64_ABI
  /* Add an implicit parameter to the destructor call with bits
     0x2 (whole object) + 0x1 (free storage, if deallocate is TRUE). */
  bit_mask = 2L;
  if (delete_routine == NULL) bit_mask |= 1L;
  ptr_node->next = node_for_integer_constant(bit_mask,
                                             (an_integer_kind)ik_int);
#endif /* !IA64 */
  /* Make a call of the destructor. */
  call_node = make_call_node_full(dtor_routine, ptr_node,
                                  (an_expr_node_ptr)NULL,
                                  /*is_virtual_call=*/dtor_routine->is_virtual,
                                  (an_insert_location *)NULL);
  check_assertion(call_node != NULL);
  if (dtor_routine->is_virtual) {
    /* The destructor is virtual, so rewrite the virtual call. */
    lower_virtual_function_call(call_node);
  }  /* if */
  if (delete_routine != NULL) {
#if DTORS_RETURN_THIS
    if (temp_assign_node != NULL)
#endif /* DTORS_RETURN_THIS */
    {
      /* Add a call of the delete routine, so we have a comma expression
           (dtor(...), delete(...))
      */
      an_expr_node_ptr delete_call_node =
                  make_delete_call_node(delete_routine, class_type,
                                        ptr_node_delete);
      call_node = make_comma_node(call_node, delete_call_node);
#if DTORS_RETURN_THIS
    } else {
      /* In the variant of the IA-64 ABI where destructors return "this",
         and "this" is a suitable argument for the delete routine,
         build delete(dtor(...)). */
      call_node = make_delete_call_node(delete_routine, class_type,
                                        call_node);
#endif /* DTORS_RETURN_THIS */
    }  /* if */
  } else {
    /* Just for safety, make sure that a destructor that returns a
       non-void value is cast to void. */
    call_node = add_cast_if_necessary(call_node, void_type());
  }  /* if */
  if (temp_assign_node != NULL) {
    /* We're using a temporary as an argument to the delete routine, make sure
       the temporary is initialized before it is used. */
    call_node = make_comma_node(temp_assign_node, call_node);
  }  /* if */
  if (need_null_ptr_test) {
    /* Add a null pointer test, producing
         ptr_node ? dtor(...) : (void)0
                             ^ plus possible delete call here
                   ^ plus possible assignment to temporary here
    */
    /* Make "ptr_node ? dtor(...) : (void)0". */
    ptr_node_test = boolean_controlling_expr(ptr_node_test);
    ptr_node_test->next = call_node;
    call_node->next = zero_cast_to_void();
    call_node = make_operator_node((an_expr_operator_kind)eok_question,
                                   call_node->type, ptr_node_test);
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
  a_boolean                   check_constructor = TRUE;

  base_type = new_delete_base_type_from_operation_type(ndsp->type);
#if LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS
  if (dip != NULL &&
      dip->destructor != NULL &&
      call_to_ctor_or_dtor_has_no_effect(dip->destructor,
                                         (an_expr_node_ptr)NULL,
                                         /*call_can_be_virtual=*/TRUE)) {
#if DEBUG
    if (db_flag_is_set("remove_ctors_dtors")) {
      (void)fprintf(f_debug, "Removing delete destruction for: ");
      db_dynamic_initializer(dip, 0);
    }  /* if */
#endif /* DEBUG */
    /* There's no need to call the destructor; change the destructor call
       into a "delete" call. */
    dip = NULL;
    ndsp->dynamic_init = NULL;
    if (delete_routine == NULL) {
      /* If not explicitly specified, use the delete operator for the class. */
      delete_routine =
                     class_type_supp(base_type)->assoc_operator_delete_routine;
    }  /* if */
  }  /* if */
#endif /* LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS */
#if IA64_ABI
  /* The IA-64 ABI requires no cookie for a class array new where the
     class has a constructor but no destructor. */
  check_constructor = FALSE;
#endif /* IA64_ABI */
  if (ndsp->array_delete &&
      new_or_delete_type_requires_array_handling(base_type,
                                                 check_constructor)) {
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
    lower_expr(ptr_node);
    /* Make the "delete" call.  It is not necessary to test for non-NULL;
       the delete routine does that. */
    call_node = make_delete_call_node(delete_routine, ndsp->type, ptr_node);
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


void zero_automatic_temporary(a_variable_ptr   temp_var,
                              an_expr_node_ptr expr)
/*
temp_var is a temporary variable with automatic storage duration
and initk_zero initialization kind.  Insert code to zero the variable,
placing it before "expr", and set the variable's initialization kind
to initk_none.  This is necessary because we don't know that the
block of the temporary will be entered at the top.
*/
{
  an_insert_location insert_location;

  set_expr_insert_location(expr, &insert_location);
  set_variable_address_taken(temp_var);
  insert_call_to_zero_entity(temp_var->type, /*have_complete_object=*/TRUE,
                             var_addr_expr(temp_var),
                             (an_expr_node_ptr)NULL,
                             (a_targ_size_t)0,
                             &insert_location);
  temp_var->init_kind = (an_init_kind)initk_none;
}  /* zero_automatic_temporary */


void lower_temp_init(an_expr_node_ptr expr)
/*
Do IL lowering of an enk_temp_init expression node.
*/
{
  a_dynamic_init_ptr dip;
  an_init_pos_descr  ipd;
  a_boolean          result_is_lvalue = expr->is_lvalue;
  an_insert_location insert_location;
  a_boolean          is_constructor_init, is_reusable_temp;
  a_variable_ptr     temp_var;
  a_boolean          keep_dynamic_init = FALSE, *eff_keep_dynamic_init = NULL;

  dip = expr->variant.init.dynamic_init;
  if (dip->kind == (a_dynamic_init_kind)dik_expression &&
      !result_is_lvalue &&
      dip->destructor == NULL &&
      !dip->is_reused_value &&
      !dip->is_optimized_class_rvalue_question_mark &&
      dip->master_entry == NULL &&
      !is_constant_node(dip->variant.expression)) {
    /* For a simple expression temporary case where the temporary is used
       directly as an rvalue, just lower the expression and create no
       temporary.  This is a useful for cases where a function returns a class
       by value (i.e., the class has no copy constructor). */
    lower_expr(dip->variant.expression);
    overwrite_node(expr, dip->variant.expression);
  } else {
    a_type_ptr         temp_type = expr->type;
    a_boolean          result_is_not_used = expr->result_is_not_used;
    a_boolean          variably_modified = (vla_enabled &&
                                        is_variably_modified_type(temp_type));
#if LOWER_VARIABLE_LENGTH_ARRAYS
    an_expr_node_ptr   vla_inits = NULL;
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
    if (variably_modified && !type_is_typedef(temp_type)) {
      /* If the construct introduces a VLA type, we need to lower its dimension
         expression (even when not lowering VLAs). */
      lower_vla_dimensions_in_type(temp_type);
#if LOWER_VARIABLE_LENGTH_ARRAYS
      /* If the compound literal introduces a VLA type, we need to compute its
         dimension variables (this is similar to cast operations). */
      vla_inits = lower_vla_dimensions(temp_type);
      record_vla_component_types_for_lowering(temp_type);
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
    }  /* if */
    if (dip->master_entry != NULL) {
      /* This entry initializes the temporary associated with another
         dynamic initialization.  Get the variable assigned for that. */
      temp_var = dip->master_entry->variable;
      /* temp_var can be NULL when the master entry is initializing
         something more complex than a variable or temporary (e.g.,
         a function return value).  A description of the destination
         is provided from higher up in the lowering process. */
      check_assertion(temp_var != NULL ||
                      dip->master_entry->init_destination != NULL);
    } else {
      /* Create a suitable temporary variable for the dynamic
         initialization. */
      temp_var = make_temporary_for_dynamic_init(temp_type, dip,
                                                 &is_reusable_temp);
      if (!is_reusable_temp) eff_keep_dynamic_init = &keep_dynamic_init;
    }  /* if */
    if (temp_var == NULL) {
      /* Initializing something more complex than a variable. */
      an_expr_node_ptr dest_expr;
      /* copy_init_pos_descr need not be called here; there's no point in
         allocating any modifiers in the heap. */
      ipd = *dip->master_entry->init_destination;
      dest_expr = make_init_entity_node(&ipd, result_is_lvalue,
                                        /*using_as_dest=*/FALSE);
      overwrite_node(expr, dest_expr);
      dip->master_entry = NULL;
    } else {
      /* Normal case (not return). */
      dip->variable = temp_var;
      if (variably_modified) {
        temp_var->has_variably_modified_type = TRUE;
      }  /* if */
      /* Change the enk_temp_init node to an enk_variable node that refers to
         the temporary variable.  The lvalueness of the node is unchanged. */
      set_expr_node_kind(expr, (an_expr_node_kind)enk_variable);
      expr->variant.variable = temp_var;
      /* Generate code for the dynamic init. */
      set_var_init_pos_descr(temp_var, &ipd);
    }  /* if */
    /* Test the kind before calling lower_dynamic_init because that routine
       clears the kind in some cases. */
    is_constructor_init = (dip->kind == (a_dynamic_init_kind)dik_constructor);
    /* Any code generated for the dynamic initialization will be
       inserted before the (modified) original expression. */
    set_expr_insert_location(expr, &insert_location);
    /* Lower the initialization. */
#if LOWER_DESIGNATED_INITIALIZERS
    lower_dynamic_init_designated_initializers(dip, (a_type_ptr)NULL);
#endif /* LOWER_DESIGNATED_INITIALIZERS */
    lower_dynamic_init(dip, &ipd,
                       (an_implied_copy_source *)NULL,
                       (a_variable_ptr)NULL,
                       LDIO_NONE,
                       /*others_follow_in_aggr=*/FALSE,
                       &insert_location,
                       eff_keep_dynamic_init,
                       (a_constant **)NULL);
    if (temp_var != NULL) {
#if LOWER_VARIABLE_LENGTH_ARRAYS
      /* After lowering, the type will no longer be variably-modified. */
      temp_var->has_variably_modified_type = FALSE;
#else /* !LOWER_VARIABLE_LENGTH_ARRAYS */
      if (temp_var->has_variably_modified_type) {
        /* If the variable has variably-modified type, put out an stmk_vla_decl
           for it. */
        a_statement_ptr stmk_vla_decl_stmt =
                              alloc_statement((a_statement_kind)stmk_vla_decl);
        stmk_vla_decl_stmt->variant.vla.is_typedef_decl = FALSE;
        stmk_vla_decl_stmt->variant.vla.variant.variable = temp_var;
        add_to_end_of_pending_stmk_init_statements_list(stmk_vla_decl_stmt);
      }  /* if */
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
      if (keep_dynamic_init) {
        /* If the initializer for the dynamic init is to be kept, allocate
           an stmk_init for it. */
        add_stmk_init_for_temp_init(dip->variable, dip);
      }  /* if */
      if (temp_var->init_kind == (an_init_kind)initk_zero) {
        if (!var_has_static_or_thread_storage_duration(temp_var)) {
          /* If an automatic temporary ends up with initk_zero initialization,
             insert code to do the zeroing because we can't count on the block
             being entered at the top. */
          zero_automatic_temporary(temp_var, expr);
#if IA64_ABI
        } else {
          /* static temporary.  Check for the need to change the initial
             value to set pointers to data members to -1. */
          lower_initializer(temp_var, &temp_var->init_kind,
                            &temp_var->initializer);
#endif /* IA64_ABI */
        }  /* if */
      }  /* if */
    }  /* if */
    /* Try to optimize away the final term that just returns the address
       or value of the temporary if that can be gotten from a constructor
       call or if it isn't needed because the result is not used. */
    if (is_constructor_init || result_is_not_used) {
      /* Check for the form (ctor-call(args),  temp)
                         or (ctor-call(args), &temp) as appropriate.
         Note that we do not do the optimization if some other terms have
         been inserted (e.g., setting a conditional destruction flag). */
      if (is_operation_node(expr) &&
          expr->variant.operation.kind == (an_expr_operator_kind)eok_comma) {
        an_expr_node_ptr first_operand = expr->variant.operation.operands;
        an_expr_node_ptr second_operand = first_operand->next;
        if (!(is_operation_node(second_operand) &&
              second_operand->variant.operation.kind ==
                                           (an_expr_operator_kind)eok_comma)) {
          /* The second operand is the original result value, usually
             a simple variable or temporary reference.  We can tell this
             from a comma-node check because of the way the expression
             insertion scheme works. */
          a_boolean can_optimize = FALSE;
          check_assertion(!node_has_side_effects(second_operand,
                                                 (a_boolean *)NULL) ||
                          /* Watch out for a volatile destination variable
                             (this is needed for optimized class rvalue "?"
                             operations). */
                          (is_variable_node(second_operand) &&
                           second_operand->variant.variable == dip->variable));
          if (result_is_not_used) {
            /* The result is not used and the second operand has no side
               effects (because it's a simple variable reference).  Do
               the optimization to just use the first operand.  Note that
               this includes the class rvalue "?" case.  Also note that
               we may be changing the type of the overall expression,
               but that's okay because the result is not used. */
            can_optimize = TRUE;
          } else if (is_constructor_init &&
                     is_ptr_or_ref_type(first_operand->type) &&
                     is_constructor_call(first_operand)) {
            /* The first operand is a constructor call. */
            /* The pointer type test rules out ABIs where the constructor
               returns void (e.g., the IA-64 ABI) and guards the
               type_pointed_to call below. */
            a_type_ptr first_op_type = type_pointed_to(first_operand->type);
            a_type_ptr expr_type = expr->type;
            /* See whether the type of the first operand is the same as the
               required result type or close enough that we can cast to adjust
               cv-qualifiers.  The first pointer level has been removed
               because it's likely to be a pointer in one case and a reference
               in the other. */
            if (same_type_with_added_qualifiers(first_op_type,
                                                expr_type,
                                                /*ignore_qualifiers=*/TRUE,
                                                (a_boolean *)NULL)) {
              /* The optimization can be done. */
              can_optimize = TRUE;
              first_operand->result_is_not_used = FALSE;
              /* If necessary, add a cast to adjust qualification. */
              if (!il_identical_types(expr_type, first_op_type)) {
                first_operand->next = NULL;
                first_operand = add_cast(first_operand,
                                         make_pointer_type(expr->type));
              }  /* if */
              first_operand = add_indirection_to_node(first_operand);
            }  /* if */
          }  /* if */
          if (can_optimize) {
            /* Do the optimization by replacing the overall expression by
               the first operand. */
            overwrite_node(expr, first_operand);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
#if LOWER_VARIABLE_LENGTH_ARRAYS
    if (vla_inits != NULL) {
      /* Be sure to compute any needed VLA dimension variables before any
         expressions inside the compound literal braces. */
      an_expr_node_ptr  new_expr = make_comma_node(vla_inits, copy_node(expr));
      overwrite_node(expr, new_expr);
    }  /* if */
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
    if (expr->is_lvalue && !result_is_lvalue) {
      /* Convert lvalue to an rvalue. */
      overwrite_node(expr, rvalue_expr_for_lvalue(expr));
    }  /* if */
    check_assertion(expr->is_lvalue == result_is_lvalue);
  }  /* if */
}  /* lower_temp_init */

#if TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE

static a_boolean add_static_data_member_init_guard_test(
                                          a_variable_ptr     variable,
                                          an_insert_location *insert_location,
                                          an_insert_location *insert_location2,
                                          a_variable_ptr     *guard_var)
/*
variable is a static data member of a template.  If its initialization
requires guard code, insert the code as follows (Cfront-like ABI):

  int guard_var;  // Global test var, implicitly init to 0
  {
    if (guard_var == 0) {
      guard_var = 1;
      ... real initialization of static data member being initialized
    }
  }

For the IA-64 ABI, the guard variable is set at the end of the
initialization (see set_local_static_guard_var):

  int guard_var;  // Global test var, implicitly init to 0
  {
    if (guard_var == 0) {
      ... real initialization of static data member being initialized
      guard_var = 1;
    }
  }

In some IA-64 ABI configurations the __cxa_guard_acquire et al. routines
are used to modify the guard variables rather than an assignment statement.

The sequence is inserted at *insert_location.  *insert_location is updated
for insertion after the "if"; *insert_location2 is set for insertion after
the assignment statement inside the "if".  In IA-64 ABI configurations where
the caller is responsible for setting the guard variable after the
initialization is complete, the guard variable is returned in *guard_var
(otherwise it is set to NULL).

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
#endif /* !IA64_ABI */
  a_constant             minus_one_constant;
  a_boolean              guard_code_emitted = FALSE;

  *guard_var = NULL;
  /* If the variable has internal linkage (e.g., in -tlocal mode), do not
     put out guard code at all. */
  if (variable->source_corresp.name_linkage ==
                        (a_name_linkage_kind)nlk_internal) goto end_of_routine;
#if !IA64_ABI
  /* Make the guard variable at the file scope. */
  test_var = make_global_var_with_prefixed_name("__SDG__",
                                               (an_integer_kind)ik_int,
                                               &variable->source_corresp,
                                               (an_il_entry_kind)iek_variable);
  if (variable->is_specialized) {
    /* This variable is a specialization of a template entity, so its
       initialization should take precedence over any initialization code
       for other instances.  Initialize the guard variable to -1 to lock out
       all other initialization code.  No test of the guard variable is
       needed here. */
    test_var->init_kind = (an_init_kind)initk_static;
    set_integer_constant(&minus_one_constant, (a_host_large_integer)-1,
                         (an_integer_kind)ik_int);
    test_var->initializer.constant = alloc_unshared_constant_in_region(
                                                      &minus_one_constant,
                                                      /*in_file_region=*/TRUE);
  } else {
    /* This is not a specialization, so the guard variable must be tested
       here. */
    guard_code_emitted = TRUE;
    /* Make "test_var == 0". */
    test_var_node = var_rvalue_expr(test_var);
    test_var_node->next = node_for_integer_constant(0L,
                                                    (an_integer_kind)ik_int);
    compare_node = make_operator_node((an_expr_operator_kind)eok_eq,
                                      integer_type((an_integer_kind)ik_int),
                                      test_var_node);
    /* Make an "if" statement and insert it into the program. */
    insert_if_statement(compare_node, /*is_initialization_guard=*/TRUE,
                        insert_location, (a_statement_ptr *)NULL,
                        insert_location2, (an_insert_location *)NULL);
    /* Make "test_var = 1" and insert it inside the "if" statement. */
    (void)insert_var_assignment_statement(test_var,
                                          node_for_integer_constant(1L,
                                                      (an_integer_kind)ik_int),
                                          insert_location2);
  }  /* if */
#else /* IA64_ABI */
  if (variable->is_specialized) {
    /* This variable is a specialization of a template entity, so its
       initialization should take precedence over any initialization code
       for other instances.  Initialize the guard variable to -1 to lock out
       all other initialization code.  No test of the guard variable is
       needed here. */
    test_var = make_global_var_with_prefixed_name("_ZGV",
                                               (an_integer_kind)ik_int,
                                               &variable->source_corresp,
                                               (an_il_entry_kind)iek_variable);
    test_var->init_kind = (an_init_kind)initk_static;
    set_integer_constant(&minus_one_constant, (a_host_large_integer)-1,
                         (an_integer_kind)ik_int);
    test_var->initializer.constant = alloc_unshared_constant_in_region(
                                                      &minus_one_constant,
                                                      /*in_file_region=*/TRUE);
  } else {
    /* Normal case -- emit the usual guard code. */
    add_first_time_test(variable, insert_location, insert_location2,
                        (a_statement_ptr *)NULL, &test_var);
    /* The guard variable is set to 1 at the end of the initialization.
       See set_local_static_guard_var. */
    *guard_var = test_var;
    guard_code_emitted = TRUE;
  }  /* if */
#endif /* IA64_ABI */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  /* The guard variable should also have the same ELF visibility as the
     guarded variable. */
  test_var->ELF_visibility = variable->ELF_visibility;
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
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

#if LOWER_DESIGNATED_INITIALIZERS
  lower_dynamic_init_designated_initializers(dip, (a_type_ptr)NULL);
#endif /* LOWER_DESIGNATED_INITIALIZERS */
  /* Only lower the cases that do not come up in C: */
  if (dip->destructor != NULL) {
    /* Initialization with a later destructor. */
    non_C_case = TRUE;
  } else if (dip->init_expr_lifetime != NULL) {
    /* Initialization that wraps a lifetime around the initialization (because
       there are temporaries created in it). */
    non_C_case = TRUE;
  } else if (var_has_static_or_thread_storage_duration(var)) {
    /* Initialization of a local static variable cannot be dynamic in C.
       Code must be used to do the initialization. */
    non_C_case = TRUE;
#if DO_RETURN_VALUE_OPTIMIZATION_IN_LOWERING
  } else if (var_is_return_value_variable(var)) {
    /* Initialization of the return value variable is a C++ case. */
    non_C_case = TRUE;
#endif /* DO_RETURN_VALUE_OPTIMIZATION_IN_LOWERING */
  } else if (var->is_vla) {
    /* Variable-length arrays (VLAs) require deallocation (treated as a
       kind of destruction). */
    non_C_case = TRUE;
  } else if (dip->is_optimized_class_rvalue_question_mark) {
    /* The initializer is an optimized class rvalue "?" operation. */
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
    default:
      unexpected_condition_str("lower_stmk_init: bad dynamic init kind");
  }  /* switch */
  if (non_C_case) {
    /* Rewrite a non-C case. */
    an_insert_location insert_location;
    a_boolean          keep_dynamic_init;
    an_init_pos_descr  ipd;

    set_insert_location(statement, &insert_location);
#if DO_RETURN_VALUE_OPTIMIZATION_IN_LOWERING
    if (var_is_return_value_variable(var)) {
      /* The variable being initialized is the return value optimization
         variable for the function.  Initialize the space provided by the
         caller instead; its address is given by an implicit parameter. */
      set_var_indirect_init_pos_descr(return_value_pointer_variable, &ipd);
      dip->variable = NULL;
      /* The current code in lower_temp_init can't handle an
         optimized class rvalue "?" that sets the return value directly.
         The front end proper is supposed to rule this out. */
      check_assertion(!dip->is_optimized_class_rvalue_question_mark);
    } else 
#endif /* DO_RETURN_VALUE_OPTIMIZATION_IN_LOWERING */
    /* Do not insert code here; this is the "else" of an "if". */
    {
      /* Normal case (not the return value optimization variable). */
      set_var_init_pos_descr(var, &ipd);
    }  /* if */
    lower_dynamic_init(dip, &ipd,
                       (an_implied_copy_source *)NULL,
                       (a_variable_ptr)NULL,
                       LDIO_FULL_EXPR,
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
        if (dip->variant.constant->kind == (a_constant_repr_kind)ck_address &&
            dip->variant.constant->variant.address.kind ==
                                          (an_address_base_kind)abk_routine &&
            needs_cast_because_type_has_param_passed_via_cctor(
                                                dip->variant.constant->type)) {
          /* Change the type of a routine address constant if needed. */
          check_assertion(var != NULL);
          dip->variant.constant->type = var->type;
          dip->variant.constant->implicit_cast = TRUE;
        } else {
          /* See if an implicit cast is necessary for this constant (or any
             sub-aggregate piece thereof). */
          add_cast_for_cv_qualified_cctor_param_if_necessary(
                                                        dip->variant.constant);
        }  /* if */
        break;
      case dik_expression:
        lower_full_expr(dip->variant.expression, (a_statement_ptr)NULL);
        if (needs_cast_because_type_has_param_passed_via_cctor(
                                              dip->variant.expression->type)) {
          /* If the expression in the dip has a type that contains a
             function with a parameter that is passed via a copy constructor,
             we need to add a cast to the destination type to avoid a type
             mismatch. */
          check_assertion(var != NULL);
          dip->variant.expression = add_cast(dip->variant.expression,
                                             var->type);
          /* We've just added a cast to a lowered expression; perform another
             post pass on the new expression (for example, to lower a
             pointer-to-member type_kind that might have just been added). */
          perform_post_pass_on_lowered_expression(dip->variant.expression);
        }  /* if */
        break;
      case dik_none:
        /* Delete the do-nothing stmk_init statement; these can occur in
           C++ configurations where lowering removes calls to unnecessary
           destructors. */
        check_assertion(dip->destructor == NULL);
        turn_statement_into_noop(statement);
        var->init_kind = (an_init_kind)initk_none;
        break;
      default:
        unexpected_condition_str("lower_stmk_init: bad dynamic init kind (2)");
    }  /* switch */
  }  /* if */
}  /* lower_stmk_init */


static void insert_pending_stmk_init_at_location(
                                           an_insert_location *insert_location)
/*
Insert any pending stmk_init statements at the specified insert_location
(updating it as necessary).
*/
{
  while (pending_stmk_init_statements != NULL) {
    a_statement_ptr stmt = pending_stmk_init_statements;
    pending_stmk_init_statements = stmt->next;
    stmt->next = NULL;
    insert_statement(stmt, insert_location);
  }  /* while */
}  /* insert_pending_stmk_init_at_location */


void insert_pending_stmk_init_statements(a_statement_ptr  statement)
/*
If there are any pending stmk_init statements (e.g., as the result of lowering
an enk_temp_init node), insert them before the given statement.  This happens
when lowering compound literals, non-constant aggregates, ptr-to-data-member
constants (in the IA-64 ABI) and some VLA s.  If there are pending statements,
the statement is turned into a block (if it is not one already).  Caller must
be aware that the statement kind may change (into an stmk_block).
*/
{
  if (pending_stmk_init_statements != NULL) {
    /* Insert statements before the given statement. */
    an_insert_location insert_location;
    if (statement->kind != (a_statement_kind)stmk_block) {
      a_statement_ptr    orig_stmt;
      change_statement_into_block(statement, &orig_stmt);
    }  /* if */
    set_block_start_insert_location(statement, &insert_location);
    insert_pending_stmk_init_at_location(&insert_location);
  }  /* if */
}  /* insert_pending_stmk_init_statements */


static void insert_pending_stmk_init_statements_at_mark(
                                           an_insert_location *insert_location)
/*
Insert any pending stmk_init statements at the location that has been
previously marked in *insert_location.  *insert_location is only updated
if the statements being added occur at the "end" of the insert_location.
Also resets the insert location mark.
*/
{
  check_assertion(!is_expr_insert_location(insert_location) &&
                  insert_location->variant.statement.is_marked);
  if (pending_stmk_init_statements != NULL) {
    if (insert_location->variant.statement.marker == NULL) {
      /* If the marker is NULL, then we're being asked to insert at
         the beginning of a block or statement creation location, so simply
         insert the stmk_inits at the beginning (which updates the
         heretofore empty insert_location). */
      check_assertion(is_empty_statement_insert_location(insert_location));
      insert_pending_stmk_init_at_location(insert_location);
    } else {
      /* Insert before the marked statement (but don't update the
         insert_location). */
      insert_pending_stmk_init_statements(
                                    insert_location->variant.statement.marker);
    }  /* if */
  }  /* if */
  reset_insert_location_mark(insert_location);
}  /* insert_pending_stmk_init_statements_at_mark */


void add_stmk_init_for_temp_init(a_variable_ptr      var,
                                 a_dynamic_init_ptr  dip)
/*
var represents a temporary variable created to hold the value of a compound
literal or array, while dip describes the required dynamic initialization.
Create the stmk_init statement required for this initialization, and add it to
the pending_stmk_init_statements list.
*/
{
  a_statement_ptr  stmk_init_stmt =
                                 alloc_statement((a_statement_kind)stmk_init);

  stmk_init_stmt->variant.dynamic_init = dip;
  /* Put the statement on a list to be inserted when we get back to
     statement level. */
  add_to_end_of_pending_stmk_init_statements_list(stmk_init_stmt);
  /* Reflect the initialization method in the variable entry. */
  var->init_kind = (an_init_kind)initk_dynamic;
  var->initializer.dynamic = dip;
  /* Conservatively set follows_an_exec_statement to TRUE to force the
     initialization to take place immediately before the temporary
     is used.  Cases involving a loop where a label might intervene
     between the temporary variable declaration and the temporary use
     make this necessary. */
  dip->follows_an_exec_statement = TRUE;
}  /* add_stmk_init_for_temp_init */


void add_to_end_of_pending_stmk_init_statements_list(a_statement_ptr stmt)
/*
Add the indicated statement to the end of the pending_stmk_init_statements
list.
*/
{
  check_assertion(seq_number_from_stmt_source_position(stmt->position) == 0);
  /* Set the statement's position to the current position (otherwise
     this statement might be mistaken by a back end as being part of the
     previous statement).  Applies to both compound literals and VLAs (when
     they're not being lowered). */
  set_stmt_pos_to_code_pos_for_lowering(stmt);
  if (pending_stmk_init_statements == NULL) {
    pending_stmk_init_statements = stmt;
  } else {
    a_statement_ptr end_of_list = pending_stmk_init_statements;
    while (end_of_list->next != NULL) end_of_list = end_of_list->next;
    end_of_list->next = stmt;
  }  /* if */
  stmt->next = NULL;
}  /* add_to_end_of_pending_stmk_init_statements_list */

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
                       (an_implied_copy_source *)NULL,
                       (a_variable_ptr)NULL,
                       LDIO_FULL_EXPR,
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
			/* TRUE if the aggregate is an array or vector, FALSE
			   for a struct/union. */
  a_field_ptr	curr_field;
			/* Current field, when array_init == FALSE. */
  a_targ_size_t	curr_elem;
			/* Current element in the array, when array_init ==
			   TRUE. */
  a_type_ptr	member_type;
			/* Current member type. */
  a_targ_size_t	number_of_elements;
			/* Number of elements in the array or vector when
			   array_init == TRUE. */
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
  aggr_pos->array_init = (aggr_type->kind == (a_type_kind)tk_array
#if GNU_VECTOR_TYPES_ALLOWED
                           || aggr_type->kind == (a_type_kind)tk_vector
#endif /* GNU_VECTOR_TYPES_ALLOWED */
                                                                       );
  aggr_pos->curr_field = NULL;
  aggr_pos->curr_elem = 0;
  aggr_pos->member_type = NULL;
  aggr_pos->number_of_elements = 0;
  if (aggr_pos->array_init) {
    if (aggr_type->kind == (a_type_kind)tk_array) {
      /* Initializing members of an array. */
      aggr_pos->member_type = f_skip_typerefs(array_element_type(aggr_type));
      aggr_pos->number_of_elements =
                           aggr_type->variant.array.variant.number_of_elements;
#if GNU_VECTOR_TYPES_ALLOWED
    } else {
      /* Initializing members of a vector. */
      check_assertion(aggr_type->kind == (a_type_kind)tk_vector);
      aggr_pos->member_type = f_skip_typerefs(aggr_type->
                                                  variant.vector.element_type);
      check_assertion(aggr_pos->member_type->size != 0);
      aggr_pos->number_of_elements = num_vector_elements(aggr_type);
#endif /* GNU_VECTOR_TYPES_ALLOWED */
    }  /* if */
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


static a_boolean any_more_members_in_aggregate(an_aggregate_position *aggr_pos)
/*
Return TRUE if there are additional initializable fields after the
position indicated by aggr_pos.
*/
{
  a_boolean more_members;

  if (aggr_pos->array_init) {
    more_members = (aggr_pos->curr_elem < aggr_pos->number_of_elements - 1);
  } else {
    a_field_ptr field = aggr_pos->curr_field;
    more_members = (field != NULL &&
                    next_initializable_field(field->next) != NULL);
  }  /* if */
  return more_members;
}  /* any_more_members_in_aggregate */


static a_constant_ptr make_init_zero_constant(a_type_ptr type)
/*
Make and return an unshared constant that is a zero of the indicated type.
If the type is an aggregate, return an aggregate constant that initializes
the first member of the aggregate.  The returned constant has not been
lowered.
*/
{
  a_constant_ptr con;

  if (is_aggregate_or_union_type(type)
#if GNU_VECTOR_TYPES_ALLOWED
      || is_vector_type(type)
#endif /* GNU_VECTOR_TYPES_ALLOWED */
                             ) {
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
  } else {
    /* Simple scalar case. */
    a_constant zero_constant;
    make_zero_of_proper_type(prvalue_type(type), &zero_constant);
    con = alloc_unshared_constant(&zero_constant);
  }  /* if */
  /* IL elements allocated during lowering are, by default, set as though
     they have been lowered.  Reset that flag so the constant will be
     lowered later. */
  mark_as_not_visited(con);
  return con;
}  /* make_init_zero_constant */


static a_constant_ptr alloc_repeated_constant(a_constant_ptr repeated_con,
                                              a_targ_size_t  count)
/*
Allocate a ck_init_repeat constant for "count" instances of repeated_con.
If repeated_con is un-lowered, the returned constant will also be
un-lowered.
*/
{
  a_constant_ptr con = alloc_constant((a_constant_repr_kind)ck_init_repeat);
  con->variant.init_repeat.count = count;
  con->variant.init_repeat.constant = repeated_con;
  if (!visited_yet(repeated_con)) {
    /* By default, con is marked as having been lowered, but that would
       prevent repeated_con from being lowered if it hasn't been lowered
       already, so mark con appropriately. */
    mark_as_not_visited(con);
  }  /* if */
  return con;
}  /* alloc_repeated_constant */


static a_constant_ptr make_one_or_more_init_zero_constants(
                                      a_type_ptr    type,
                                      a_targ_size_t number_of_constants_needed)
/*
Make and return an unshared constant that is either a zero or a
repeated zero of the indicated type.  If the type is an aggregate,
return an aggregate constant that initializes the first member of
the aggregate.  number_of_constants_needed specifies how many zero
constants are required.
*/
{
  a_constant_ptr zero_con;
  a_constant_ptr con;

  check_assertion(number_of_constants_needed != 0);
  zero_con = make_init_zero_constant(type);
  if (number_of_constants_needed == 1) {
    con = zero_con;
  } else {
    con = alloc_repeated_constant(zero_con, number_of_constants_needed);
  }  /* if */
  return con;
}  /* make_one_or_more_init_zero_constants */


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
    a_boolean      save_multidimensional_aggr_tail_not_repeated = 
                        con->variant.init_repeat.
                                       multidimensional_aggr_tail_not_repeated;

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
      if (con->variant.init_repeat.multidimensional_aggr_tail_not_repeated) {
        /* We're splitting a repeated multi-dimensional array designated
           constant, as in:
             int X[3][3] = { [0 ... 2][0] = 4, 5, 6 };
           In order to match the order of initialization used by GNU compilers,
           the repeated aggregate constant '{[0] = 4, 5, 6}' should
           only repeat the first initializer of the aggregate on the first
           (count-1) iterations and use the full value of the aggregate on the
           final iteration.  To implement this, we create a new copy of the
           repeated constant tree, traverse the copy finding any aggregate
           (without brace) entries whose first_constant is a designator and
           pruning all but this first constant.  The final case is handled
           when the ck_init_repeat constant is simply removed when its count
           would become 1, leaving the entire aggregate in place. */
        a_constant_ptr pruned_con = copy_unshared_constant(rep_con);
        a_constant_ptr const_ptr = pruned_con;
        while (const_ptr != NULL) {
          if (const_ptr->kind == (a_constant_repr_kind)ck_aggregate &&
              !const_ptr->explicit_braces_on_aggregate &&
              const_ptr->variant.aggregate.first_constant != NULL) {
            a_constant_ptr first_constant =
                                   const_ptr->variant.aggregate.first_constant;
            if (first_constant->kind == (a_constant_repr_kind)ck_designator) {
              /* Skip a designator if present. */
              first_constant = first_constant->next;
            }  /* if */
            /* Remove anything after the first constant in the aggregate. */
            first_constant->next = NULL;
            const_ptr->variant.aggregate.last_constant = first_constant;
            /* Look further into the first constant. */
            const_ptr = first_constant;
          } else if (const_ptr->kind == (a_constant_repr_kind)ck_init_repeat) {
            const_ptr = const_ptr->variant.init_repeat.constant;
          } else if (const_ptr->kind == (a_constant_repr_kind)ck_designator) {
            const_ptr = const_ptr->next;
          } else {
            break;
          }  /* if */
        }  /* while */
        copy_constant(pruned_con, con);
      } else {
        copy_constant(rep_con, con);
      }  /* if */
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
        second_repeat_con = alloc_repeated_constant(rep_con_copy,
                                                    second_count);
        /* Preserve the setting of the multidimensional_aggr_tail_not_repeated
           field. */
        second_repeat_con->variant.init_repeat.
                                     multidimensional_aggr_tail_not_repeated = 
                                  save_multidimensional_aggr_tail_not_repeated;
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
          a_constant_ptr repeat_con = alloc_repeated_constant(zero_con, count);
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


static a_boolean quickly_find_designator_insert_point(
                                              a_constant_ptr  desig_con,
                                              a_type_ptr      aggr_type,
                                              a_constant_ptr  prior_designator,
                                              a_constant_ptr  prior_constant,
                                              a_constant_ptr  earlier_aggr_con,
                                              a_constant_ptr  *prev_con,
                                              an_init_con_pos *earlier_con)
/*
Attempt to quickly find the correct designator insert point for
designator constant desig_con.  If the insert point is found,
the function returns TRUE and sets *prev_con to the constant after
which the designator constant should be queued, as well as setting
*earlier_con to point to any previous constants being overwritten.
aggr_type is the type of the aggregate.  prior_designator points
to the last designator that was placed in the aggregate initializer (or NULL).
prior_constant (if not NULL) points to the last constant that was
placed in the aggregate initializer (corresponds to prior_designator).
earlier_aggr_con (if not NULL) points to an aggregate constant whose
values are being overwritten by the current aggregate.
*/
{
  a_boolean found_insert_point = FALSE;

  if (aggr_type->kind == (a_type_kind)tk_array &&
      prior_designator != NULL && prior_constant != NULL &&
      prior_constant->kind != (a_constant_repr_kind)ck_init_repeat &&
      desig_con->variant.designator.array_element >
       prior_designator->variant.designator.array_element) {
    /* Some large arrays initializers use designated initializers
       whose values monotonically increase, causing exponential
       behavior during the lowering of the designators.  If the
       designator we're attempting to place immediately follows the
       last designator we placed, there is no need to search for
       the correct designator insert point.  If there is a gap
       between the last designator we placed and the current one,
       fill the gap with zeros unless the gap already contains constants
       (either from an earlier aggregate initialization or from
       earlier in this initialization). */
    a_targ_size_t number_of_zero_constants_needed = 
                    desig_con->variant.designator.array_element -
                    prior_designator->variant.designator.array_element - 1;
    if (number_of_zero_constants_needed == 0) {
      /* Designated constant follows prior constant. */
      *prev_con = prior_constant;
      set_init_con_pos((*prev_con)->next, earlier_con);
      found_insert_point = TRUE;
    } else if (prior_constant->next == NULL && earlier_aggr_con == NULL) {
      /* There is a gap between the prior constant and this one.
         Create an appropriate number of zero constants to fill it. */
      a_constant_ptr zero_con = make_one_or_more_init_zero_constants(
                             f_skip_typerefs(array_element_type(aggr_type)),
                             number_of_zero_constants_needed);
      prior_constant->next = zero_con;
      *prev_con = zero_con;
      set_init_con_pos((a_constant_ptr)NULL, earlier_con);
      found_insert_point = TRUE;
    }  /* if */
#if DEBUG
    if (found_insert_point && db_flag_is_set("designators")) {
      (void)fprintf(f_debug, "Quickly found insert point, prev_con = ");
      db_constant(*prev_con);
      (void)fprintf(f_debug, ", earlier_con.ptr = ");
      db_constant(earlier_con->ptr);
      (void)fprintf(f_debug, "\n");
    }  /* if */
#endif /* DEBUG */
  }  /* if */
  return found_insert_point;
}  /* quickly_find_designator_insert_point */


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
  

static void split_multidimensional_ck_init_repeat(a_constant_ptr constant)
/*
In cases where a single ck_init_repeat is used to initialize an entire
multi-dimensional array in the aggregate constant, split the ck_init_repeat
into multiple ck_init_repeats, one for each dimension of the array.  E.g.,
change "{<6 repetitions of {47}>}" to "{<2 repetitions of {<3 repetitions of
{47}>}>}" for an aggregate of type "array [2] of array [3] of A".  Note that
this routine only handles ck_init_repeat constants that encompass an
entire multi-dimensional array (and not just a portion thereof).
*/
{
  a_constant_ptr  cp, rep_con, aggr, prev_aggr = NULL;
  a_type_ptr      aggr_type = skip_typerefs(constant->type);

  check_assertion(constant->kind == (a_constant_repr_kind)ck_aggregate &&
                  is_array_type(constant->type));
  cp = constant->variant.aggregate.first_constant;
  if (cp != NULL &&
      cp->kind == (a_constant_repr_kind)ck_init_repeat &&
      cp->variant.init_repeat.count >
                         aggr_type->variant.array.variant.number_of_elements) {
    check_assertion(cp->variant.init_repeat.count ==
                                              num_array_elements(aggr_type) &&
                    constant->variant.aggregate.last_constant == cp);
    do {
      if (is_array_type(f_skip_typerefs(array_element_type(aggr_type)))) {
        /* Allocate an aggregate constant for each dimension of the array. */
        aggr = alloc_constant((a_constant_repr_kind)ck_aggregate);
        aggr->type = array_element_type(aggr_type);
      } else {
        /* Last time through, use the original repeated constant. */
        aggr = cp->variant.init_repeat.constant;
      }  /* if */
      if (aggr_type->variant.array.variant.number_of_elements == 1) {
        /* No repeat needed if count is one. */
        rep_con = aggr;
      } else {
        rep_con = alloc_repeated_constant(aggr,
                          aggr_type->variant.array.variant.number_of_elements);
      }  /* if */
      if (prev_aggr == NULL) {
        constant->variant.aggregate.first_constant = rep_con;
        constant->variant.aggregate.last_constant = rep_con;
      } else {
        prev_aggr->variant.aggregate.first_constant = rep_con;
        prev_aggr->variant.aggregate.last_constant = rep_con;
      }  /* if */
      prev_aggr = aggr;
      aggr_type = f_skip_typerefs(array_element_type(aggr_type));
    } while (is_array_type(aggr_type));
  }  /* if */
}  /* split_multidimensional_ck_init_repeat */


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
  a_constant_ptr  prior_designator = NULL;
  a_constant_ptr  prior_constant = NULL;
  a_constant_ptr  union_designator = NULL, saved_union_init_constant = NULL;
  a_type_ptr      aggr_type = skip_typerefs(aggr_con->type);
  a_boolean       union_init = is_union_type(aggr_type);
  an_init_con_pos con, earlier_con;

  check_assertion(aggr_con->kind == (a_constant_repr_kind)ck_aggregate);
  if (is_array_type(aggr_type)) {
    /* If the aggregate has a ck_init_repeat that spans multiple dimensions
       of an array, split it into a ck_init_repeat for each dimension so the
       code below will be able to process it correctly. */
    split_multidimensional_ck_init_repeat(aggr_con);
  }  /* if */
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
      a_field_ptr    first_field = next_initializable_field(
                             aggr_type->variant.class_struct_union.field_list);
      if (temp_con != NULL &&
          temp_con->kind == (a_constant_repr_kind)ck_designator) {
        /* Take the designator off the old list. */
        prev_union_designator = temp_con;
        if (prev_union_designator->variant.designator.field == first_field) {
          prev_union_designator = NULL;
        }  /* if */
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
        if (union_designator->variant.designator.field == first_field) {
          union_designator = NULL;
        }  /* if */
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
    while (con.ptr != NULL) {
      if (con.ptr->kind == (a_constant_repr_kind)ck_init_repeat) {
        /* If the constant is a repetition of something that requires
           special handling, split it. */
        a_constant_ptr repeated_con = con.ptr->variant.init_repeat.constant;
        if (repeated_con->kind == (a_constant_repr_kind)ck_designator ||
            repeated_con->kind == (a_constant_repr_kind)ck_aggregate) {
          split_constant_if_repeated(&con);
        }  /* if */
      }  /* if */
      /* Exit the loop if we've reached a designator. */
      if (con.ptr->kind == (a_constant_repr_kind)ck_designator) break;
      if (con.ptr != prior_constant) {
        /* Reset pointers to prior designator and associated constant
           once we reach a new constant without a designator. */
        prior_designator = NULL;
        prior_constant = NULL;
      }  /* if */
      if (earlier_con.ptr != NULL &&
          con.ptr->kind != (a_constant_repr_kind)ck_string) {
        /* If merging old and new values, rewrite string constants as
           aggregate initializers to allow operation at the character
           level.  According to DR 253, if the new constant is a
           string it initializes the entire aggregate and therefore
           overwrites the entire previous initialization. */
        explode_string_initializer(earlier_con.ptr);
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
          } else if (con.ptr->explicit_braces_on_aggregate) {
            /* According to DR 253 on C99, an aggregate value completely
               overwrites any previous value, even if it doesn't initialize
               all the fields that the previous value initializes.
               Note that this applies only when the new value is surrounded
               by braces, not when the braces are elided.  Save the old value
               off to the side and process it below. */
            superseded_con = NULL;
          }  /* if */
        }  /* if */
        lower_aggregate_designated_initializers(con.ptr, superseded_con);
        if (superseded_con != earlier_con.ptr) {
          /* See comments above.  Discard the old initializer for the
             aggregate except for preserving its side effects. */
          combine_initializer_constants(earlier_con.ptr, con.ptr);
        }  /* if */
      } else {
        /* Non-aggregate constant. */
        if (con.ptr->kind == (a_constant_repr_kind)ck_dynamic_init) {
          /* Lower designators in a dynamic initialization subtree. */
          lower_dynamic_init_designated_initializers(
                                                con.ptr->variant.dynamic_init,
                                                con.ptr->type);
        }  /* if */
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
      /* In cases where an aggregate array is initialized with monotonically
         increasing initializers, we may be able to quickly locate the
         designator insert point.  If not, use the slower method. */
      if (!quickly_find_designator_insert_point(con.ptr,
                                                aggr_type,
                                                prior_designator,
                                                prior_constant, 
                                                earlier_aggr_con,
                                                &prev_con,
                                                &earlier_con)) {
        /* Find the right point to insert the constants after the
           designator. */
        find_designator_insert_point(con.ptr, aggr_con, &prev_con, 
                                                                 &earlier_con);
      }  /* if */
    }  /* if */
    prior_designator = con.ptr;
    /* Advance to the constant following the ck_designator. */
    advance_init_con_pos(&con);
    check_assertion(con.ptr != NULL &&
                    con.ptr->kind != (a_constant_repr_kind)ck_designator);
    prior_constant = con.ptr;
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
#if CHECKING
    a_constant_ptr        last_con = NULL;
#endif /* CHECKING */
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
      a_type_ptr member_type = skip_typerefs(aggr_pos.member_type);
      temp_con = con_pos.ptr;
      if (temp_con->kind == (a_constant_repr_kind)ck_init_repeat) {
        temp_con = temp_con->variant.init_repeat.constant;
        /* For constructor initializations of a multi-dimensional array,
           the array is flattened to an initialization in one dimension. */
        if (is_array_type(member_type) &&
            !is_array_type(temp_con->type)) {
          member_type = underlying_array_element_type(member_type);
          member_type = skip_typerefs(member_type);   
        }  /* if */
      }  /* if */
#if CHECKING
      { a_type_ptr con_type = skip_typerefs(temp_con->type);
        check_assertion_str(
                   il_identical_types(con_type, member_type) ||
                   /* A short string literal can initialize a longer
                      char array.  Also an array of non-const chars
                      can initialize an array of const chars. */
                   (is_string_type(con_type) &&
                    is_string_type(member_type) &&
                    (temp_con->kind == (a_constant_repr_kind)ck_string ||
                     temp_con->kind == (a_constant_repr_kind)ck_aggregate)) ||
                   ((is_array_type(con_type) && 
                     is_array_type(member_type)) &&
                     /* In GNU C mode, zero-length array fields can be
                        initialized with arbitrary-length arrays. */
                     ((gcc_mode &&
                       skip_typerefs(con_type)->variant.array.bound_is_zero) ||
                      /* Allow an array initializer of any length to initialize
                         an incomplete array. */
                      is_incomplete_array_type(member_type))) ||
                   (is_immediate_class_type(con_type) &&
                    /* Allow a match if the class type is being used as a
                       subobject. */
                    class_type_supp(con_type)->type_as_subobject != NULL &&
                    identical_types(class_type_supp(con_type)->
                                                             type_as_subobject,
                                    member_type)),
                   "lower_aggregate_designated_initializers: type mismatch");
      }
      last_con = con_pos.ptr;
#endif /* CHECKING */
      advance_init_con_pos(&con_pos);
      if (con_pos.ptr != NULL) {
        advance_aggregate_position_to_next_member(&aggr_pos);
      }  /* if */
    }  /* while */
    check_assertion(aggr_con->variant.aggregate.last_constant == last_con);
  }
#endif /* EXPENSIVE_CHECKING */
}  /* lower_aggregate_designated_initializers */


static a_boolean recompute_partially_initialized_flag(a_constant_ptr aggr_con,
                                                      a_type_ptr     aggr_type)
/*
Check the initialization constant, aggr_con, to determine if it partially
initializes the aggregate type, aggr_type, that is being initialized.  Returns
TRUE if the constant only partially initializes the aggregate; otherwise
returns FALSE.  Also sets aggr_con->partial_aggr_value and
aggr_con->is_partially_initialized to reflect the new value.
*/
{ 
  a_constant_ptr        temp_con;
  an_aggregate_position aggr_pos;
  an_init_con_pos       con_pos;
  a_boolean             is_partially_initialized;

  if (aggr_con->kind == (a_constant_repr_kind)ck_string) {
    check_assertion(is_array_type(aggr_type));
    is_partially_initialized = (aggr_con->variant.string.length < 
           skip_typerefs(aggr_type)->variant.array.variant.number_of_elements);
  } else {
    check_assertion(aggr_con->kind == (a_constant_repr_kind)ck_aggregate);
    temp_con = aggr_con->variant.aggregate.first_constant;
    /* Set initial positions in both aggregate and constant. */
    init_aggregate_position(aggr_con, &aggr_pos);
    set_init_con_pos(temp_con, &con_pos);
    /* Iterate for each constant in the aggregate constant. */
    while (con_pos.ptr != NULL) {
      temp_con = con_pos.ptr;
      if (temp_con->kind == (a_constant_repr_kind)ck_designator) {
        /* Generally speaking, designators have been removed, but that's not
           the case for unions where a designator is left in place if a field
           other than the first field of the union is being initialized.
           Move the aggregate position accordingly. */
        check_assertion(is_union_type(aggr_type) && temp_con->next != NULL);
        if (temp_con->variant.designator.field == NULL) {
          aggr_pos.curr_elem = temp_con->variant.designator.array_element;
        } else {
          set_aggregate_position_for_field(temp_con->variant.designator.field,
                                           &aggr_pos);
        }  /* if */
        temp_con = temp_con->next;
        set_init_con_pos(temp_con, &con_pos);
      }  /* if */
      if (temp_con->kind == (a_constant_repr_kind)ck_init_repeat) {
        temp_con = temp_con->variant.init_repeat.constant;
      }  /* if */
      if (is_aggregate_or_union_type(aggr_pos.member_type)) {
        /* Aggregates that are initialized by a ck_dynamic_init are
           fully initialized. */
        if (temp_con->kind != (a_constant_repr_kind)ck_dynamic_init) {
          if (recompute_partially_initialized_flag(temp_con,
                                                   aggr_pos.member_type)) {
            /* Any partially initialized sub-aggregate results in a partially
               initialized aggregate. */
            is_partially_initialized = TRUE;
            goto done;
          }  /* if */
        }  /* if */
      }  /* if */
      if (con_pos.repeat_count > 0) {
        /* When dealing with a repeated constant, we can skip directly over
           all the corresponding elements. */
        aggr_pos.curr_elem += (con_pos.repeat_count - 1);
        con_pos.repeat_count = 0;
      }  /* if */
      /* Advance to next position in both constant and aggregate. */
      advance_init_con_pos(&con_pos);
      if (con_pos.ptr != NULL) {
        advance_aggregate_position_to_next_member(&aggr_pos);
      }  /* if */
    }  /* while */
    /* We've exhausted the list of constants.  If there are any more
       fields in the aggregate, this initializer only partially
       initializes the aggregate (unless the aggregate is a union). */
    if (is_union_type(aggr_type)) {
      is_partially_initialized = FALSE;
    } else {
      is_partially_initialized = any_more_members_in_aggregate(&aggr_pos);
    }  /* if */
  }  /* if */
done:
  /* Re-set partial_aggr_value for this aggregate based on our findings. */
  aggr_con->partial_aggr_value = is_partially_initialized;
  aggr_con->is_partially_initialized = is_partially_initialized;
  return is_partially_initialized;
}  /* recompute_partially_initialized_flag */


void lower_designated_initializers(a_constant_ptr init_con,
                                   a_dynamic_init *dip,
                                   a_type_ptr     aggr_type)
/*
If the initial value constant indicated by init_con contains any
designated initializers, rewrite them as standard C.  dip points to the
dynamic initialization (and is NULL if this is a static initialization).
If non-NULL, aggr_type specifies the type of the aggregate being initialized
(it can be NULL in cases where dip->variable is non-NULL, in which case
the type of dip->variable is used).  Note that this is called in C mode as well
as C++ mode.
*/
{
  if (init_con->kind == (a_constant_repr_kind)ck_aggregate &&
      init_con->uses_designated_initializers) {
    a_memory_region_number region_to_switch_back_to = NULL_region_number;
    if (in_file_scope(init_con)) {
      switch_to_file_scope_region(&region_to_switch_back_to);
    }  /* if */
    lower_aggregate_designated_initializers(init_con,
                                            (a_constant_ptr)NULL);
    /* Lowering may have changed the initializer from partially
       initialized to fully initialized, so re-compute it. */
    if (dip != NULL && dip->is_partially_initialized) {
      /* In most cases, the aggregate type is just the type of the variable
         being initialized, but in the sub-aggregate case, it's passed in
         explicitly. */
      if (aggr_type == NULL) {
        check_assertion(dip->variable != NULL);
        aggr_type = dip->variable->type;
      }  /* if */
      dip->is_partially_initialized =
                     recompute_partially_initialized_flag(init_con, aggr_type);
    }  /* if */
    switch_back_to_original_region(region_to_switch_back_to);
    init_con->uses_designated_initializers = FALSE;
  }  /* if */
}  /* lower_designated_initializers */


void lower_dynamic_init_designated_initializers(a_dynamic_init_ptr dip,
                                                a_type_ptr         aggr_type)
/*
If the dynamic initialization pointed to by dip contains any designated
initializers, rewrite them as standard C.  If non-NULL, aggr_type specifies the
type of the aggregate being initialized (it can be NULL in cases where
dip->variable is non-NULL, in which case the type of dip->variable is used).
Note that this is called in C mode as well as C++ mode.
*/
{
  if (designators_allowed &&
      (dip->kind == (a_dynamic_init_kind)dik_constant ||
       dip->kind == (a_dynamic_init_kind)dik_nonconstant_aggregate)) {
    lower_designated_initializers(dip->variant.constant, dip, aggr_type);
  }  /* if */
}  /* lower_dynamic_init_designated_initializers */

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

#if HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS || !IA64_ABI

static a_variable_ptr make_construction_vtbl_temporary(void)
/*
Make a temporary to be used in a constructor or destructor to point to the
array of special virtual function table pointers.  Return a pointer to
the temporary variable.
*/
{
  a_variable_ptr var;

  var = make_lowered_temporary(make_pointer_type(
                                    make_qualified_type(pointer_to_vtbl_type(),
                                                        TQ_CONST)));
  return var;
}  /* make_construction_vtbl_temporary */

#endif /* HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS || !IA64_ABI */

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
  a_variable_ptr                  primary_vtbl_var =
                                        primary_vtbl_var_for_class(class_type);

#if IA64_ABI
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
    implicit_cast(&con, pointer_to_vtbl_type());
#if IA64_ABI
    /* In the IA64 ABI, the value of the vptr in the object is not the same as
       the address of the virtual function table variable.  */
    vtbl_index = elements->virtual_function_table_index;
    con.variant.address.offset = vtbl_index * (long)vtbl_entry_size();
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
  var->is_optional_vtable = primary_vtbl_var->is_optional_vtable;
#if IA64_ABI
  var->storage_class = primary_vtbl_var->storage_class;
  var->comdat_group = primary_vtbl_var->comdat_group;
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
                         make_qualified_type(pointer_to_vtbl_type(), TQ_CONST);
#if !IA64_ABI
  /* Create the local static array variable. */
  var = make_unnamed_local_static_variable(array_type,
                                           /*in_function_scope=*/TRUE);
  define_construction_vtbls_array(class_type, var, elements);
#else /* IA64_ABI */
  /* Create the array variable. */
  var_name = mangled_virtual_table_table_name(class_type);
  var = make_lowered_variable(var_name, /*already_il_name=*/FALSE, array_type,
                              (a_storage_class)sc_extern);
  var->source_corresp.name_has_been_mangled = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  if ((ctsp->decl_modifiers & DM_DLLFLAGS) != 0) {
    /* Set any required dllimport/dllexport attributes. */
    var->decl_modifiers |= (ctsp->decl_modifiers & DM_DLLFLAGS);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  var->ELF_visibility = ctsp->ELF_visibility;
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
  ctsp->virtual_table_table_var = var;
done:
#endif /* IA64_ABI */
  return var;
}  /* make_construction_vtbls_array */


an_expr_node_ptr vtbl_addr_from_construction_vtbls_array(
                        a_variable_ptr                  construction_vtbls_var,
                        a_boolean                       var_is_array,
                        a_construction_vtbl_array_index idx)
/*
Construct an expression for an lvalue for the "idx-1"-th element of the
indicated array of special virtual function table values.  Return a pointer
to the expression.  If var_is_array is TRUE, construction_vtbls_var is the
array itself; if FALSE, it is a pointer to the first element of the array.
*/
{
  an_expr_node_ptr expr;

  check_assertion(construction_vtbls_var != NULL);
  if (var_is_array) {
    expr = array_first_element_addr_expr(construction_vtbls_var);
  } else {
    expr = var_rvalue_expr(construction_vtbls_var);
  }  /* if */
  /* Compensate for 0-origin of array versus 1-origin of index. */
  idx--;
  if (idx != 0) {
    /* The entry is not at offset 0 of the array, so add the right offset. */
    expr->next = node_for_integer_constant((long)idx,
                                           targ_size_t_int_kind);
    expr = make_operator_node((an_expr_operator_kind)eok_padd,
                              expr->type,
                              expr);
  }  /* if */
  return expr;
}  /* vtbl_addr_from_construction_vtbls_array */


static void insert_default_construction_vtbls_assignment(
                                a_type_ptr              class_type,
                                a_variable_ptr          construction_vtbls_var,
                                an_insert_location      *insert_location)
/*
Insert an assignment statement to set the construction_vtbls_var temporary to
point to the default array of virtual function table pointers to be used
when constructing or destroying a complete object of type class_type.
*insert_location indicates the insert location.
*/
{
  a_class_type_supplement_ptr
                   ctsp = class_type->variant.class_struct_union.extra_info;
  a_variable_ptr   array_var =
                       make_construction_vtbls_array(class_type, 
                                                     ctsp->construction_vtbls);
  an_expr_node_ptr array_addr = array_first_element_addr_expr(array_var);

#if IA64_ABI
  {
    /* Set the referenced flag in the virtual function table to say that
       the virtual function table and VTT must be put out even if they
       are static. */
    a_variable_ptr   vtbl_var = class_type->variant.class_struct_union.
                                        extra_info->virtual_function_table_var;
    check_assertion(vtbl_var != NULL);
    vtbl_var->source_corresp.referenced = TRUE;
  }
#endif /* IA64_ABI */
  (void)insert_var_assignment_statement(construction_vtbls_var, array_addr,
                                        insert_location);
}  /* insert_default_construction_vtbls_assignment */

#if !IA64_ABI

static an_expr_node_ptr make_construction_vtbl_transfer_pointer_lvalue(
                                                   an_expr_node_ptr expr,
                                                   a_type_ptr       class_type)
/*
expr is an lvalue expression for a class object.  class_type is
the type of class object pointed to, provided because the underlying type of
expr might be a type-as-subobject.  Modify the expression so that it is
an lvalue for the transfer pointer in the object, and return a pointer
to the modified expression.  The transfer pointer is a virtual function
table pointer or virtual base class pointer within the indicated object
(including non-virtual base classes) which is available to be used to
pass information to a subobject constructor or destructor for the
subobject pointed to by expr.
*/
{
  check_assertion(expr->is_lvalue);
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


static void receive_construction_vtbls_in_subobject_constructor(
                                     a_variable_ptr     construction_vtbls_var,
                                     a_type_ptr         class_type,
                                     a_variable_ptr     this_param_var,
                                     an_insert_location *insert_location)
/*
Insert an assignment statement to set the construction_vtbls_var temporary to
the pointer to an array of special virtual function tables passed into
a subobject constructor or destructor via the so-called transfer pointer
in the object.  class_type is the subobject class type.  this_param_var
is the "this" parameter variable for the constructor or destructor.
*/
{
  an_expr_node_ptr trans_ptr_node;

  trans_ptr_node = add_indirection_to_node(var_rvalue_expr(this_param_var));
  /* Get the address of a pointer in the object that is used to
     do the transfer. */
  trans_ptr_node =
                 make_construction_vtbl_transfer_pointer_lvalue(trans_ptr_node,
                                                                class_type);
  trans_ptr_node = rvalue_expr_for_lvalue(trans_ptr_node);
  trans_ptr_node = add_cast(trans_ptr_node, construction_vtbls_var->type);
  (void)insert_var_assignment_statement(construction_vtbls_var, trans_ptr_node,
                                        insert_location);
}  /* receive_construction_vtbls_in_subobject_constructor */


static void set_transfer_pointer(an_init_pos_descr_ptr ipdp,
                                 a_type_ptr            subobject_class_type,
                                 an_expr_node_ptr      array_addr,
                                 an_insert_location    *insert_location)
/*
Set the "transfer pointer" in the base class subobject described by
ipdp and subobject_class_type to the address given by array_addr.
Insert the code at *insert_location.
*/
{
  an_expr_node_ptr trans_ptr_node;

  /* Get the address of the subobject. */
  trans_ptr_node = make_init_entity_node(ipdp, /*result_is_lvalue=*/TRUE,
                                         /*using_as_dest=*/TRUE);
  /* Get the address of a pointer in the object that is used to
     do the transfer. */
  trans_ptr_node =
          make_construction_vtbl_transfer_pointer_lvalue(trans_ptr_node,
                                                         subobject_class_type);
  array_addr = add_cast(array_addr, trans_ptr_node->type);
  (void)insert_assignment_statement(trans_ptr_node,
                                    (an_expr_operator_kind)eok_assign,
                                    array_addr,
                                    insert_location);
}  /* set_transfer_pointer */


static void pass_construction_vtbls_to_subobject_constructor(
                        a_variable_ptr                  construction_vtbls_var,
                        a_boolean                       var_is_array,
                        a_type_ptr                      subobject_class_type,
                        a_construction_vtbl_array_index idx,
                        an_init_pos_descr_ptr           ipdp,
                        an_insert_location              *insert_location)
/*
Insert an assignment statement to store the address of the "idx-1"-th
element of the array of special virtual functions pointed to by
construction_vtbls_var into the so-called transfer pointer in the
subobject described by ipdp to pass the array to a subobject constructor
or destructor.  If var_is_array is TRUE, construction_vtbls_var is the
array itself; if FALSE, it is a pointer to the first element of the array.
The subobject class type is subobject_class_type (this is passed because
the type of the expression produced from ipdp may have the type-as-subobject).
*/
{
  an_expr_node_ptr array_addr;

  array_addr = vtbl_addr_from_construction_vtbls_array(construction_vtbls_var,
                                                       var_is_array,
                                                       idx);
  set_transfer_pointer(ipdp, subobject_class_type, array_addr,
                       insert_location);
}  /* pass_construction_vtbls_to_subobject_constructor */


a_routine_ptr make_subobject_destruction_routine(a_dynamic_init_ptr dip)
/*
Make a routine that contains the code necessary to do the destruction of
a base class subobject whose initialization is described by dip.
This is needed for the cases where a subobject destruction vtable must
be passed to the destructor via the transfer pointer in the Cfront-like
ABI.
*/
{
  a_scope_ptr            scope;
  an_insert_location     insert_location;
  a_memory_region_number region_number;
  a_generated_routine_context
                         grcontext;
  a_routine_ptr          routine;
  a_routine_ptr          dtor_routine = dip->destructor;
  a_type_ptr             this_param_type, vtt_ptr_type;
  a_variable_ptr         this_param_var, vtt_ptr_var;
  a_routine_type_supplement_ptr
                         rtsp;
  an_init_pos_descr      ipd;
  a_destructible_entity_descr_ptr
                         dedp = dip->destructible_entity_descr;

  check_assertion(dedp != NULL &&
                  dedp->subobject_construction_base_class != NULL);
  /* Create a routine. */
  this_param_type = implicit_this_param_type_of(dtor_routine->type);
  vtt_ptr_type = make_virtual_table_table_pointer_type();
  routine = make_rout_entry((char *)NULL,
                            (a_storage_class)sc_static,
                            void_type(),
                            this_param_type);
  rtsp = routine->type->variant.routine.extra_info;
  rtsp->param_type_list->next = alloc_param_type(vtt_ptr_type);
  /* Make a memory region, scope, and block for the routine definition. */
  scope = make_routine_definition(routine, /*make_return=*/TRUE,
                                  &region_number);
  push_generated_routine_context(scope, region_number, &grcontext);
  /* Make the first parameter, "this". */
  this_param_var = make_lowered_param_variable(this_param_type);
  scope->variant.routine.parameters = this_param_var;
  this_param_var->is_this_parameter = TRUE;
  /* Make the second parameter, the construction vtable pointer. */
  vtt_ptr_var = make_lowered_param_variable(vtt_ptr_type);
  this_param_var->next = vtt_ptr_var;
  set_block_start_insert_location(scope->assoc_block, &insert_location);
  /* Generate code to pass the subobject construction vtable. */
  set_var_indirect_init_pos_descr(this_param_var, &ipd);
  set_transfer_pointer(&ipd, dedp->subobject_construction_base_class->type,
                       var_rvalue_expr(vtt_ptr_var), &insert_location);
  /* Generate the code to call the destructor. */
  add_destructor_call(dtor_routine, &ipd, /*have_complete_object=*/FALSE,
                      (an_expr_node_ptr)NULL, &insert_location);
  pop_generated_routine_context(scope, region_number, &grcontext);
  return routine;
}  /* make_subobject_destruction_routine */


a_routine_ptr make_delegation_destruction_routine(a_dynamic_init_ptr dip)
/*
Create a "delegation" destructor that will be placed in the exception
handling region table in cases where it is unknown at compilation time
whether "0" (for base class subobject) or "2" (for complete objects) should
be passed to the destructor.  Use the second argument to the "delegation"
destructor to determine the proper value when invoking the class destructor.
Note that a similar mechanism is used in the IA-64 ABI, but it is implemented
in define_default_version_of_routine (as an alternate entry point).
*/
{
  a_scope_ptr            scope;
  an_insert_location     insert_location;
  a_memory_region_number region_number;
  a_generated_routine_context
                         grcontext;
  a_routine_ptr          routine;
  a_routine_ptr          dtor_routine = dip->destructor;
  a_type_ptr             this_param_type, delegation_dtor_arg_type;
  a_variable_ptr         this_param_var, delegation_dtor_param;
  a_routine_type_supplement_ptr
                         rtsp;
  a_destructible_entity_descr_ptr
                         dedp = dip->destructible_entity_descr;
  an_expr_node_ptr       args, question_node;

  check_assertion(dedp != NULL && dedp->use_delegation_dtor);
  /* Create a routine. */
  this_param_type = implicit_this_param_type_of(dtor_routine->type);
  delegation_dtor_arg_type = dedp->delegation_dtor_arg->type;
  routine = make_rout_entry((char *)NULL,
                            (a_storage_class)sc_static,
                            void_type(),
                            this_param_type);
  rtsp = routine->type->variant.routine.extra_info;
  rtsp->param_type_list->next = alloc_param_type(delegation_dtor_arg_type);
  /* Make a memory region, scope, and block for the routine definition. */
  scope = make_routine_definition(routine, /*make_return=*/TRUE,
                                  &region_number);
  push_generated_routine_context(scope, region_number, &grcontext);
  /* Make the first parameter, "this". */
  this_param_var = make_lowered_param_variable(this_param_type);
  scope->variant.routine.parameters = this_param_var;
  this_param_var->is_this_parameter = TRUE;
  /* Make the second parameter, a pointer to a virtual base class. */
  delegation_dtor_param =
                         make_lowered_param_variable(delegation_dtor_arg_type);
  this_param_var->next = delegation_dtor_param;
  set_block_start_insert_location(scope->assoc_block, &insert_location);
  /* Make "(delegation_dtor_param ? 0 : 2)" to differentiate between
     a subobject and complete object cases. */
  question_node = var_rvalue_expr(delegation_dtor_param);
  question_node = boolean_controlling_expr(question_node);
  question_node->next = dtor_control_argument(/*have_complete_object=*/FALSE,
                                              /*free_storage=*/FALSE);
  question_node->next->next =
                        dtor_control_argument(/*have_complete_object=*/TRUE,
                                              /*free_storage=*/FALSE);
  question_node = make_operator_node((an_expr_operator_kind)eok_question,
                                     question_node->next->type, question_node);
  args = var_rvalue_expr(this_param_var);
  args->next = question_node;
  /* Generate the code to call the destructor. */
  make_call_statement(dtor_routine, args, (an_expr_node_ptr)NULL,
                      &insert_location);
  pop_generated_routine_context(scope, region_number, &grcontext);
  return routine;
}  /* make_delegation_destruction_routine */

#endif /* !IA64_ABI */

#if IA64_ABI
/*ARGSUSED*/  /* <-- ipdp and insert_location are not used in that case. */
#else /* !IA64_ABI */
/*ARGSUSED*/  /* <-- implied_arg_node is not used in that case. */
#endif /* IA64_ABI */
void build_construction_vtbls_pointer(
                             a_destructible_entity_descr_ptr dedp,
                             an_init_pos_descr               *ipdp,
                             an_insert_location_ptr          insert_location,
                             an_expr_node_ptr                *implied_arg_node)
/*
If the destructible entity description dedp says so (as determined
by build_construction_vtbls_pointer_for_subobject_construction),
generate code to pass a pointer to an array of construction virtual
function tables to a subobject constructor or destructor.  ipdp tells
how to address the subobject base class (used for the Cfront-like ABI
only).  Code is inserted at *insert_location (Cfront-like ABI) or
returned in *implied_arg_node (IA-64 ABI; that's set to NULL if no
code is needed).
*/
{
  a_base_class_ptr base_class = dedp->subobject_construction_base_class;

#if !IA64_ABI
  if (dedp->needs_subobject_construction_vtbl) {
    /* Pass the construction vtable address via the transfer pointer. */
    pass_construction_vtbls_to_subobject_constructor(
                  dedp->construction_vtbls_var,
                  (a_boolean)dedp->construction_vtbls_var_is_array,
                  base_class->type,
                  (a_construction_vtbl_array_index)
                   (dedp->construction_vtbls_var_is_array ?
                   (a_construction_vtbl_array_index)1 :
                   base_class->base_subarray_index_in_construction_vtbl_array),
                  ipdp,
                  insert_location);
  }  /* if */
#else /* IA64_ABI */
  *implied_arg_node = NULL;
  if (dedp->needs_subobject_construction_vtbl) {
    /* Pass the construction vtable address via an added argument. */
    check_assertion(dedp->construction_vtbls_var != NULL);
    *implied_arg_node = vtbl_addr_from_construction_vtbls_array(
                   dedp->construction_vtbls_var,
                   (a_boolean)dedp->construction_vtbls_var_is_array,
                   base_class->base_subarray_index_in_construction_vtbl_array);
  }  /* if */
#endif /* !IA64_ABI */
}  /* build_construction_vtbls_pointer */

#if IA64_ABI
/*ARGSUSED*/  /* <-- ipdp and insert_location are not used in that case. */
#else /* !IA64_ABI */
/*ARGSUSED*/  /* <-- implied_arg_node is not used in that case. */
#endif /* IA64_ABI */
static void build_construction_vtbls_pointer_for_subobject_construction(
                                 a_dynamic_init_ptr     dip,
                                 a_base_class_ptr       base_class,
                                 an_init_pos_descr      *ipdp,
                                 a_variable_ptr         construction_vtbls_var,
                                 an_insert_location_ptr insert_location,
                                 an_expr_node_ptr       *implied_arg_node,
                                 a_boolean              *just_test)
/*
We are about to generate a call of a constructor or destructor for a
base class subobject, as part of a constructor or destructor for a larger
object.  Generate code to pass a pointer to an array of construction
virtual function tables to the subobject constructor or destructor,
if one is needed because the class has virtual base classes.
dip describes the initialization, base_class is the subobject being
initialized, ipdp tells how to address it (used for the Cfront-like
ABI only), construction_vtbls_var is the variable for the construction
vtables array for the complete class (if needed), any code added is
inserted at *insert_location, and (IA-64 ABI only) *implied_arg_node
is set to the expression for the VTT pointer, or NULL if one is not needed.
Information on whether a construction vtable is needed, and which
one, is recorded in the destructible entity description attached
to dip so that build_construction_vtbls_pointer can be called again later.
If just_test is non-NULL, just test whether a construction vtable
is needed, and set *just_test accordingly, but do not record that
under dip or generate code.
*/
{
  a_destructible_entity_descr_ptr dedp = dip->destructible_entity_descr;
  a_destructible_entity_descr     ded;

  if (just_test != NULL) *just_test = FALSE;
  /* The destructible entity description is not allocated if not
     needed, e.g., for a constructor-init in a destructor when
     exceptions are disabled.  For that case, use a dummy one
     just long enough to pass information between the two parts
     of this routine. */
  if (dedp == NULL) {
    dedp = &ded;
    clear_destructible_entity_descr(&ded);
  }  /* if */
#if !IA64_ABI
  /* See if the base class constructor needs to be passed an array
     of virtual function table pointers to use during the subobject
     construction.  If so, pass it by setting the transfer pointer
     to the address of the proper array. */
  if (base_class->base_subarray_index_in_construction_vtbl_array != 0) {
    check_assertion(!base_class->is_virtual);
    /* Yes, this base class constructor needs the special information.
       Pass the address of a subarray of the overall class array of
       virtual function table pointers. */
    if (just_test != NULL) {
      *just_test = TRUE;
    } else {
      check_assertion(construction_vtbls_var != NULL);
      dedp->needs_subobject_construction_vtbl = TRUE;
      dedp->construction_vtbls_var_is_array = FALSE;
      dedp->construction_vtbls_var = construction_vtbls_var;
      dedp->subobject_construction_base_class = base_class;
    }  /* if */
  } else if (base_class->is_virtual) {
    if (base_class->base_construction_vtbls != 0) {
      /* Yes, this virtual base class constructor needs the
         special information.  Pass the address of an array of
         virtual function table pointers specific to this case.
         Note that we are calling the constructor directly from
         the constructor for a complete object. */
      if (just_test != NULL) {
        *just_test = TRUE;
      } else {
        a_variable_ptr array_var =
                            make_construction_vtbls_array(
                                          base_class->derived_class,
                                          base_class->base_construction_vtbls);
        dedp->needs_subobject_construction_vtbl = TRUE;
        dedp->construction_vtbls_var_is_array = TRUE;
        dedp->construction_vtbls_var = array_var;
        dedp->subobject_construction_base_class = base_class;
      }  /* if */
    }  /* if */
  }  /* if */
#else /* IA64_ABI */
  if (just_test == NULL) *implied_arg_node = NULL;
  /* See if the base class constructor needs to be passed an array
     of virtual function table pointers (the VTT) to use during the subobject
     construction.  If so, pass it as an implied argument. */
  if (base_class->type->variant.class_struct_union.any_virtual_base_classes) {
    /* The constructor or destructor takes a VTT pointer. */
    if (just_test != NULL) {
      *just_test = TRUE;
    } else {
      /* Pass an element from the parent VTT. */
      check_assertion(base_class->
                         base_subarray_index_in_construction_vtbl_array != 0 &&
                      construction_vtbls_var != NULL);
      dedp->needs_subobject_construction_vtbl = TRUE;
      dedp->construction_vtbls_var_is_array = FALSE;
      dedp->construction_vtbls_var = construction_vtbls_var;
      dedp->subobject_construction_base_class = base_class;
    }  /* if */
  }  /* if */
#endif /* !IA64_ABI */
  if (just_test == NULL) {
    /* Generate the code if required. */
    build_construction_vtbls_pointer(dedp, ipdp, insert_location,
                                     implied_arg_node);
  }  /* if */
}  /* build_construction_vtbls_pointer_for_subobject_construction */

#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */

static void lower_ctor_init(a_constructor_init_ptr ctor_init,
                            a_variable_ptr         this_param_var,
                            a_boolean              base_of_complete_object,
                            a_variable_ptr         construction_vtbls_var,
                            an_insert_location_ptr insert_location)
/*
Generate code to implement the constructor_init entry pointed to by ctor_init.
this_param_var is the "this" parameter variable for the overall object
being initialized.  If base_of_complete_object is TRUE, the entity being
initialized is a virtual base class and its derived class is known to be a
complete object.  construction_vtbls_var points to a variable for an array of
construction vtables, if needed, and NULL otherwise.  The statement(s) created
are inserted at *insert_location, and *insert_location is updated.
*/
{
  a_dynamic_init_ptr     dip;
  an_init_pos_descr      ipd;
  an_init_pos_modifier   ipm;
  an_implied_copy_source source_desc;

  dip = ctor_init->initializer;
  check_assertion(dip != NULL);
  /* Develop a position description for the entity to initialize. */
  develop_ctor_init_pos_descr(ctor_init, this_param_var, &ipd, &ipm);
  if (base_of_complete_object) ipd.base_of_complete_object = TRUE;
  /* Set the source of the implied copy. */
  clear_implied_copy_source(&source_desc);
  source_desc.ctor_init = ctor_init;
  if (ctor_init->source_expr != NULL) {
    /* If there is an associated source expression as part of the implied copy,
       lower it. */
    lower_expr(ctor_init->source_expr);
  }  /* if */
  check_assertion(pending_stmk_init_statements == NULL);
  if (is_expr_insert_location(insert_location)) {
    /* An expression insert location is unusual here, but occurs in
       configurations where assignment to "this" is allowed.  In that case,
       no pending stmk_init statements should be generated during the
       lowering of such an assignment (but it's checked after the lowering). */
  } else {
    /* Mark the location where any generated stmk_init statements should go. */
    set_insert_location_mark(insert_location);
  }  /* if */
  /* Generate the code to do the initialization. */
  lower_dynamic_init(dip, &ipd,
                     &source_desc, construction_vtbls_var,
                     LDIO_FULL_EXPR, /*others_follow_in_aggr=*/FALSE,
                     insert_location, (a_boolean *)NULL,
                     (a_constant **)NULL);
  if (is_expr_insert_location(insert_location)) {
    /* Make sure no pending stmk_inits were generated. */
    check_assertion(pending_stmk_init_statements == NULL);
  } else {
    /* Insert any generated stmk_inits at the previously marked location. */
    insert_pending_stmk_init_statements_at_mark(insert_location);
  }  /* if */
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
    vtbl_addr_node = rvalue_expr_for_lvalue(vtbl_addr_node);
  } else
#endif /* IA64_ABI */
  /* Do not add code here. */
  {
    ctsp = class_type->variant.class_struct_union.extra_info;
    primary_vtbl_var = ctsp->virtual_function_table_var;
    if (primary_vtbl_var != NULL) {
      vtbl_addr_node = make_vtbl_address_node(primary_vtbl_var, class_type,
                                              (a_base_class_ptr)NULL);
    } else {
      vtbl_addr_node = NULL;
    } /* if */
  }  /* if */
  if (vtbl_addr_node != NULL) {
    /* Assign the primary virtual table address to the virtual table
       pointer in the current class. */
    vptr_node = make_vptr_field_lvalue_from_var(this_param_var);
    (void)insert_assignment_statement(vptr_node,
                                      (an_expr_operator_kind)eok_assign,
                                      vtbl_addr_node,
                                      insert_location);
  }  /* if */
}  /* insert_primary_vtbl_assignment */


#if !DO_FULL_PORTABLE_EH_LOWERING
/*ARGSUSED*/ /* <-- complete_var_handle is not used in that case. */
#endif /* !DO_FULL_PORTABLE_EH_LOWERING */
static void add_virtual_base_init_code(
                                     a_scope_ptr        scope,
                                     a_variable_ptr     complete_var,
                                     a_handle_number    complete_var_handle,
                                     a_variable_ptr     construction_vtbls_var,
                                     an_insert_location *insert_location)
/*
This routine emits initialization code for virtual base classes; it is called
during construction of the complete object constructor or the subobject
constructor.  In the Cfront ABI, this is called only from the subobject
constructor; it may be called from both constructors in the IA-64 ABI.
scope specifies the (complete or subobject) constructor function scope.
If the emitted code is to be conditionally executed at run-time (as is the case
for the subobject constructor), complete_var is the temporary that controls the
execution, and complete_var_handle is the index number associated with
complete_var when exception handling is enabled.  complete_var can be NULL to
indicate that the code is unconditionally executed.  construction_vtbls_var is
a temporary variable that has been set by the caller to contain an appropriate
construction VTT.  *insert_location specifies where to insert the
initialization code.
*/
{
  a_type_ptr                  class_type;
  a_constructor_init_ptr      ctor_init;
  a_variable_ptr              this_param_var;
#if !IA64_ABI
  a_class_type_supplement_ptr ctsp;
  a_base_class_ptr            bcp;
  a_variable_ptr              vbase_param_var;
#endif /* !IA64_ABI */

  this_param_var = scope->variant.routine.parameters;
  ctor_init = scope->variant.routine.constructor_inits;
  class_type = parent_class_of(scope->variant.routine.ptr);
  check_assertion(
              class_type->variant.class_struct_union.any_virtual_base_classes);
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
  if (construction_vtbls_var != NULL) {
    /* Set the construction_vtbls temporary to point to the default array
       of virtual function table pointers to be used when constructing a
       complete object. */
    insert_default_construction_vtbls_assignment(class_type,
                                                 construction_vtbls_var,
                                                 insert_location);
  }  /* if */
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
#if !IA64_ABI
  /* Set the added parameters to the addresses of the virtual base
     classes. */
  vbase_param_var = this_param_var->next;
  ctsp = class_type->variant.class_struct_union.extra_info;
  for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
    if (bcp->is_virtual) {
      an_expr_node_ptr  vaddr_node, assign_node, vbptr_node;
      /* Make an expression whose value is the address of the virtual
         base class. */
      vaddr_node = make_vbase_class_lvalue_from_var(this_param_var,
                                                    bcp,
                                                    /*complete_object=*/TRUE);
      /* Add a cast if necessary to convert from a pointer to the base
         class type to a pointer to the type-as-subobject for the base
         class type. */
      vaddr_node = add_address_of_to_node(vaddr_node);
      vaddr_node = add_cast_if_necessary(vaddr_node, vbase_param_var->type);
      /* Make an assignment to set the virtual base class parameter. */
      assign_node = make_var_assignment_expr(vbase_param_var, vaddr_node);
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
        assign_node = add_cast_if_necessary(assign_node, vbptr_node->type);
        /* Assign the base class address to the virtual base class
           pointer. */
        vbptr_node->next = assign_node;
        assign_node = make_operator_node((an_expr_operator_kind)eok_assign,
                                         assign_node->type, vbptr_node);
      }  /* if */
      /* Insert the assignment statement. */
      (void)insert_expr_statement(assign_node, insert_location);
      /* Move on to the next added parameter for the next iteration of
         the loop. */
      vbase_param_var = vbase_param_var->next;
    }  /* if */
  }  /* for */
#else /* IA64_ABI */
  /* Set the virtual function table now so that the virtual base classes
     can be accessed from within the base class constructors if those
     constructors don't set the virtual function table pointer themselves.
     This also guarantees that arguments to nonvirtual base class
     constructors can reference members of virtual base classes. */
  insert_primary_vtbl_assignment(class_type, this_param_var,
                                 construction_vtbls_var,
                                 insert_location);
#endif /* IA64_ABI */
  /* Initialize any virtual base classes on the ctor_init list. */
  for (; ctor_init != NULL &&
            ctor_init->kind == (a_constructor_init_kind)cik_virtual_base_class;
       ctor_init = ctor_init->next) {
    if (ctor_init->initializer->destructor != NULL && complete_var != NULL) {
      /* Add complete_var as a conditional flag.  The virtual base
         class should be destroyed only if it was constructed in this
         constructor. */
      a_destructible_entity_descr_ptr dedp = 
                             ctor_init->initializer->destructible_entity_descr;
      check_assertion(dedp != NULL);
      dedp->conditional_flag_var = complete_var;
#if DO_FULL_PORTABLE_EH_LOWERING
      if (exceptions_enabled) {
        /* coverity[uninit_use] */
        dedp->conditional_flag_handle = complete_var_handle;
      }  /* if */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
    }  /* if */
    lower_ctor_init(ctor_init, this_param_var,
                    /*base_of_complete_object=*/TRUE,
                    construction_vtbls_var, insert_location);
  }  /* for */
}  /* add_virtual_base_init_code */


static void add_usual_constructor_wrapper_code(
                                           a_scope_ptr        scope,
                                           an_insert_location *insert_location)
/*
Insert constructor wrapper code at the indicated location in the indicated
constructor scope.  The insert location is usually at the beginning of the
constructor, but may instead be after an assignment to "this".  Delegating
constructors are handled separately.
*/
{
  a_base_class_ptr       bcp;
  a_variable_ptr         this_param_var;
  a_type_ptr             class_type;
  a_class_type_supplement_ptr
                         ctsp;
  a_constructor_init_ptr ctor_init;
#if !IA64_ABI
  an_expr_node_ptr       vbptr_node;
  an_insert_location     else_insert_location;
#endif /* !IA64_ABI */
#if HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS || !IA64_ABI
  a_variable_ptr         vbase_param_var;
#endif /* HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS || !IA64_ABI */
  an_expr_node_ptr       vtbl_addr_node, vptr_node;
  a_variable_ptr         vtbl_var;
  a_source_position      saved_error_position, saved_code_pos;
  a_variable_ptr         construction_vtbls_var = NULL;

  /* The following pseudo-code shows both the processing in this routine
     and the code added to the constructor routine.  Lines enclosed in [...]
     are tests and loops done in the processing in this routine; other
     lines are the code added to the constructor routine.  Note that the
     processing for virtual base class initialization (as indicated below
     when HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS is TRUE) can be
     performed in either the subobject or complete object constructor (or
     both), depending upon the configuration.

#if HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS
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
               the ctor-initializer list, plus extra information needed
               to get proper subobject behavior).
         [endfor]
#if !IA64_ABI
       else (not initializing a complete object)
         Set the construction_vtbls temp to the value in the transfer pointer
           in the class (the caller uses that to pass in the address of
           the array of vtbl pointers to be used during the subobject
           construction).
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
#endif // HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS
     [For each initialized direct nonvirtual base class (entries for these
         appear as the middle of the ctor-initializer list):]
       Call the constructor for the base class (arguments as indicated by
         the ctor-initializer list, plus extra information needed to
         get proper subobject behavior).
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
  class_type = parent_class_of(scope->variant.routine.ptr);
  /* Mark the class as referenced because, at the very least, the
     "this" parameter uses it. */
  class_type->source_corresp.referenced = TRUE;
  ctsp = class_type->variant.class_struct_union.extra_info;
  if (class_type->variant.class_struct_union.any_virtual_base_classes) {
#if HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS
    an_insert_location     insert_location2;
    an_expr_node_ptr       null_constant_node, vbase_param_node, compare_node;
    an_expr_node_ptr       complete_var_node;
    a_constant             null_constant;
    a_variable_ptr         complete_var;
    a_handle_number        complete_var_handle = 0;
#endif /* HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS */

#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
#if IA64_ABI
    check_assertion(ctsp->construction_vtbls != NULL);
    /* Use the VTT parameter. */
    construction_vtbls_var = this_param_var->next;
#else /* !IA64_ABI */
    if (ctsp->construction_vtbls != NULL) {
      /* This class is one that has overridden virtual functions in virtual
         base classes, and needs special versions of the virtual function
         tables when used to construct a subobject. */
      /* Create a temporary that will point to an array of virtual function
         table addresses. */
      construction_vtbls_var = make_construction_vtbl_temporary();
    }  /* if */
#endif /* IA64_ABI */
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
#if HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS
    /* Put out code that tests whether or not the virtual base classes need
       to be initialized.  This is done by testing whether or not the
       first added parameter is NULL (in the IA-64 ABI, this is the VTT
       parameter).  A local variable (called "complete" in the pseudocode
       above) is initialized to TRUE if a complete object is being
       initialized.  A local variable is used so that it can serve as
       a conditional flag for EH cleanup. */
    vbase_param_var = this_param_var->next;
    /* Make a NULL pointer constant of the right type. */
    make_zero_of_proper_type(vbase_param_var->type, &null_constant);
    /* Make an expression node pointing to the NULL constant. */
    null_constant_node = alloc_node_for_constant(&null_constant);
    /* Make an expression node for the parameter. */
    vbase_param_node = var_rvalue_expr(vbase_param_var);
    /* Make a node comparing the parameter against NULL. */
    vbase_param_node->next = null_constant_node;
    compare_node = make_operator_node((an_expr_operator_kind)eok_eq,
                                      integer_type((an_integer_kind)ik_int),
                                      vbase_param_node);
    /* Set the local variable. */
    complete_var = make_lowered_temporary(
                                        integer_type((an_integer_kind)ik_int));
    (void)insert_var_assignment_statement(complete_var, compare_node,
                                          insert_location);
#if DO_FULL_PORTABLE_EH_LOWERING
    if (exceptions_enabled && scope->lifetime != NULL) {
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
         if (complete) {} else {}
                        ^-------^-- additional statements will be inserted.
    */
    complete_var_node = var_rvalue_expr(complete_var);
    insert_if_statement(boolean_controlling_expr(complete_var_node),
                        /*is_initialization_guard=*/FALSE,
                        insert_location, (a_statement_ptr *)NULL,
                        &insert_location2,
#if IA64_ABI
                        (an_insert_location *)NULL  /* No "else" needed. */
#else /* !IA64_ABI */
                        &else_insert_location
#endif /* IA64_ABI */
                                                  );
    /* Insert the code to do the actual virtual base class initialization
       under the "then" part of the "if" (a complete object is being
       initialized). */
    add_virtual_base_init_code(scope, complete_var, complete_var_handle,
                               construction_vtbls_var, &insert_location2);
#if !IA64_ABI
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
                                                       &else_insert_location);
    }  /* if */
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
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
                                                   vbptr_node->type);
          /* Make an assignment statement that copies the implicit parameter
             value into the virtual base class pointer. */
          (void)insert_assignment_statement(vbptr_node,
                                            (an_expr_operator_kind)eok_assign,
                                            vbase_param_node,
                                            &else_insert_location);
        }  /* if */
      }  /* if */
    }  /* for */
#endif /* !IA64_ABI */
#endif /* HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS */
  }  /* if */
  /* The virtual base class initializations have either been handled in
     add_virtual_base_init_code above or will be handled in the complete
     object constructor; skip over them. */
  while (ctor_init != NULL &&
         ctor_init->kind == (a_constructor_init_kind)cik_virtual_base_class) {
    ctor_init = ctor_init->next;
  }  /* while */
  /* Generate initialization for each non-virtual base class that appears on
     the ctor_init list. */
  for (; ctor_init != NULL &&
          ctor_init->kind == (a_constructor_init_kind)cik_direct_base_class;
       ctor_init = ctor_init->next) {
    lower_ctor_init(ctor_init, this_param_var,
                    /*base_of_complete_object=*/FALSE,
                    construction_vtbls_var, insert_location);
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
    vtbl_addr_node = NULL;
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
    if (bcp->index_in_construction_vtbl_array != 0) {
      /* Set the virtual function table pointer to an element from the
         array of construction virtual function table pointers. */
      vtbl_addr_node = vtbl_addr_from_construction_vtbls_array(
                                        construction_vtbls_var,
                                        /*var_is_array=*/FALSE,
                                        bcp->index_in_construction_vtbl_array);
      vtbl_addr_node = add_indirection_to_node(vtbl_addr_node);
      vtbl_addr_node = rvalue_expr_for_lvalue(vtbl_addr_node);
    } else
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
    /* Do not insert code here; this is the "else" of an "if". */
    {
#if !IA64_ABI
      vtbl_var = bcp->virtual_function_table_var;
#else /* IA64_ABI */
      if (base_class_has_vtbl(bcp)) {
        vtbl_var = ctsp->virtual_function_table_var;
      } else {
        vtbl_var = NULL;
      }  /* if */
#endif /* IA64_ABI */
      if (vtbl_var != NULL) {
        /* Set the virtual function table from the standard virtual function
           table for this base class. */
        vtbl_addr_node = make_vtbl_address_node(vtbl_var, class_type, bcp);
      }  /* if */
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
        vptr_node = add_indirection_to_node(var_rvalue_expr(vbase_param_var));
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
        vptr_node = make_base_class_lvalue_from_var(this_param_var, bcp,
                                                    /*complete_object=*/FALSE);
      }  /* if */
      vptr_node = make_vptr_field_lvalue(vptr_node);
      /* Make and insert the assignment statement. */
      (void)insert_assignment_statement(vptr_node,
                                        (an_expr_operator_kind)eok_assign,
                                        vtbl_addr_node,
                                        insert_location);
    }  /* if */
  }  /* for */
  /* Generate initialization for each data member that appears on the
     ctor_init list (there should be no delegation constructors on this
     list). */
  for (; ctor_init != NULL; ctor_init = ctor_init->next) {
    check_assertion(ctor_init->kind ==(a_constructor_init_kind)cik_field);
    lower_ctor_init(ctor_init, this_param_var,
                    /*base_of_complete_object=*/FALSE,
                    (a_variable_ptr)NULL, insert_location);
  }  /* for */
  error_position = saved_error_position;
  code_pos_for_lowering = saved_code_pos;
}  /* add_usual_constructor_wrapper_code */


static void add_delegating_constructor_wrapper_code(
                                           a_scope_ptr        scope,
                                           an_insert_location *insert_location)
/*
Add the constructor wrapper code that is specific to a delegating
constructor (at the specified insert_location).
*/
{
  a_constructor_init_ptr ctor_init = scope->variant.routine.constructor_inits;

  /* There is exactly one ctor_init for a delegating constructor. */
  check_assertion(scope->variant.routine.ptr->is_delegating_ctor &&
                  ctor_init != NULL &&
                  ctor_init->kind == (a_constructor_init_kind)cik_delegation &&
                  ctor_init->next == NULL);
#if IA64_ABI
  if (parent_class_of(scope->variant.routine.ptr)
                       ->variant.class_struct_union.any_virtual_base_classes) {
      /* Most delegating constructors can be handled through the "normal"
         mechanism (i.e., the cik_delegation constructor is simply lowered
         as another constructor init and it doesn't matter if the target
         subobject constructor or the target complete object constructor
         are called because they are aliases for each other), but when the
         parent class of the delegating constructor has virtual base classes,
         then the delegating subobject constructor must call the target
         subobject constructor and the delegating complete object constructor
         must call the target complete object constructor.  To handle this
         case, the primary routine (which contains the body of the delegating
         constructor) is a cdk_delegation constructor (an EDG addition), and
         the alternate entry points invoke the cdk_delegation routine. */
    if (ctor_init->initializer != NULL &&
                    ctor_init->initializer->kind ==
                                        (a_dynamic_init_kind)dik_constructor &&
        ctor_or_dtor_body_has_no_effect(scope) &&
        (!exceptions_enabled ||
         same_exception_spec(scope->variant.routine.ptr->type,
                     ctor_init->initializer->variant.constructor.ptr->type))) {
      /* Optimize the case where the body of a delegating constructor is
         empty (this check is made before the body is lowered because lowering
         of the cik_delegation constructor initializer will create lowered
         code in the body).  In this case, a cdk_delegation routine is
         not necessary. */
      check_assertion(scope->variant.routine.ptr->ctor_dtor_kind ==
                                           (a_ctor_or_dtor_kind)cdk_subobject);
    } else {
      scope->variant.routine.ptr->ctor_dtor_kind =
                                           (a_ctor_or_dtor_kind)cdk_delegation;
    }  /* if */
  }  /* if */
#endif /* IA64_ABI */
  /* Lower the delegating constructor init to invoke the target constructor.
     In the Cfront ABI, if parent class has virtual bases, the lowered
     call to the target constructor will forward any implied arguments
     for virtual base classes (see make_ctor_implied_arg_list). */
  lower_ctor_init(ctor_init,
                  scope->variant.routine.parameters,
                  /*base_of_complete_object=*/FALSE,
                  (a_variable_ptr)NULL, insert_location);
  scope->variant.routine.constructor_inits = NULL;
}  /* add_delegating_constructor_wrapper_code */


void add_constructor_wrapper_code(a_scope_ptr        scope,
                                  an_insert_location *insert_location)
/*
Insert constructor wrapper code at the indicated location in the indicated
constructor scope.  The insert location is usually at the beginning of the
constructor, but may instead be after an assignment to "this".
*/
{
  /* Handle delegating constructors separately from other constructors. */
  if (scope->variant.routine.ptr->is_delegating_ctor) {
    add_delegating_constructor_wrapper_code(scope, insert_location);
  } else {
    add_usual_constructor_wrapper_code(scope, insert_location);
  }  /* if */
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
  a_statement_ptr    last_statement;
  an_insert_location insert_location;
  a_source_position  saved_error_position, saved_code_pos;
  a_routine_ptr      ctor_routine = scope->variant.routine.ptr;
#if NEW_CAN_BE_FOLDED_INTO_CTOR || HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS
  a_type_ptr         class_type = parent_class_of(ctor_routine);
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR || HANDLE_VIRTUAL_BASES_IN_COMPLETE... */
#if NEW_CAN_BE_FOLDED_INTO_CTOR
  a_class_type_supplement_ptr
                     ctsp = class_type_supp(class_type);
  a_routine_ptr      new_routine = ctsp->assoc_operator_new_routine;
  a_variable_ptr     this_param_var = scope->variant.routine.parameters;
  an_expr_node_ptr   if_node = NULL;
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
    if (!ctor_routine->is_delegating_ctor) {
      /* Add code to allocate storage if "this" is NULL:
           if (this || (this = new_rout(size)))
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
      an_expr_node_ptr   this_param_node, this_test_node;

      /* If there is no default new routine for the class, do not put out
         the code.  This happens if the class has a class-specific new but
         not one that takes a single argument. */
      if (new_routine != NULL) {
        /* Make "new_rout(size)". */
        size_node = node_for_host_large_integer(
                                        (a_host_large_integer)class_type->size,
                                        targ_size_t_int_kind);
        call_node = make_call_node(new_routine, size_node);
        /* Make "this = new_rout(size)". */
        unqual_this_param_type = f_skip_typerefs(this_param_var->type);
        call_node = add_cast_if_necessary(call_node, unqual_this_param_type);
        assign_node = make_var_assignment_expr(this_param_var, call_node);
        if (exceptions_enabled &&
            /* The delete routine pointer can be null if the operator delete
               for the class is ambiguous. */
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
          /* Make a dynamic initialization entry that describes the
             deletion. */
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
        /* Make "this || (this = new_rout(size))". */
        this_param_node = var_rvalue_expr(this_param_var);
        this_test_node = boolean_controlling_expr(this_param_node);
        this_test_node->next = boolean_controlling_expr(assign_node);
        int_type = integer_type((an_integer_kind)ik_int);
        if_node = make_operator_node((an_expr_operator_kind)eok_lor,
                                     int_type, this_test_node);
        /* The "if" statement is inserted later. */
      }  /* if */
    }  /* if */
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
#if HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS
    if (class_type->variant.class_struct_union.any_virtual_base_classes &&
        scope->variant.routine.ptr->ctor_dtor_kind ==
                                          (a_ctor_or_dtor_kind)cdk_subobject) {
      /* Create the complete object constructor early so that we can move
         (or copy) any virtual base constructor inits into the complete
         object constructor scope.  This must be done before the constructor
         inits are lowered below. */
      (void)alternate_entry_point(scope->variant.routine.ptr,
                                  (a_ctor_or_dtor_kind)cdk_complete,
                                  /*define_now=*/TRUE);
    }  /* if */
#endif /* HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS */
    /* When the constructor has a function-try-block, the wrapper code
       is generated by lower_try_block. */
    if (!has_function_try_block) {
      /* Generate member and base initialization code. */
      add_constructor_wrapper_code(scope, &insert_location);
    }  /* if */
  }
  /* Lower the user code in the constructor. */
  if (has_function_try_block) {
    /* A function try block.  The call will also handle generating
       member and base initialization code inside the dependent
       block. */
    lower_try_block(user_code_stmts, /*is_function_try_block=*/TRUE,
                    (a_destructor_wrapper_info_block_ptr)NULL);
  } else {
    lower_statement_list(user_code_stmts, &last_statement);
  }  /* if */
  if (!ctor_routine->is_delegating_ctor) {
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
           "if (this || (this = new_rout(size)))".
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
#if HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS
    /* Clear the list of non-virtual base constructor inits (the virtual
       base constructors are still needed). */
    { a_constructor_init_ptr  ptr, prev = NULL;
      for (ptr = scope->variant.routine.constructor_inits;
           ptr != NULL;
           ptr = ptr->next) {
        if (ptr->kind != (a_constructor_init_kind)cik_virtual_base_class) {
          if (prev == NULL) {
            scope->variant.routine.constructor_inits = NULL;
          } else {
            prev->next = NULL;
          }  /* if */
          break;
        }  /* if */
        prev = ptr;
      }  /* for */
    }
#else /* !HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS */
    /* Clear the list of constructor inits (it can't be cleared by
       add_constructor_wrapper_code because that routine can be called
       more than once when assignments to "this" are present). */
    scope->variant.routine.constructor_inits = NULL;
#endif /* HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS */
  }  /* if */
  error_position = saved_error_position;
  code_pos_for_lowering = saved_code_pos;
}  /* lower_constructor_code */


#if !ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
/*ARGSUSED*/ /* <-- destruction_vtbls_var is not used in that case. */
#endif /* !ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
static void lower_dtor_init(a_constructor_init_ptr ctor_init,
                            a_variable_ptr         this_param_var,
                            a_boolean              have_complete_object,
                            a_boolean              base_of_complete_object,
                            a_variable_ptr         destruction_vtbls_var,
                            an_insert_location_ptr insert_location)
/*
Generate code to implement the constructor_init entry pointed to by ctor_init,
one that appears on the constructor_init list for a destructor.
this_param_var is the "this" parameter variable for the overall object
being destroyed.  If have_complete_object is TRUE, the entity being
destroyed is a complete object.  If base_of_complete_object is TRUE,
the entity being destroyed is a virtual base class and its derived
class is known to be a complete object.  If this destruction is for a
base class whose destructor needs to be passed an array of special
virtual function table addresses, generate code to do that;
destruction_vtbls_var provides the variable for the complete class
array if necessary.  The statements created are inserted at
*insert_location, and *insert_location is updated.
*/
{
  an_init_pos_descr    ipd;
  an_init_pos_modifier ipm;
  a_dynamic_init_ptr   dip;
  a_boolean            keep_constant;
  an_expr_node_ptr     vtt_addr_node = NULL;

  /* Develop a position description for the entity to destroy. */
  develop_ctor_init_pos_descr(ctor_init, this_param_var, &ipd, &ipm);
  if (base_of_complete_object) ipd.base_of_complete_object = TRUE;
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
                                          (an_implied_copy_source *)NULL,
                                          /*others_follow_in_aggr=*/FALSE,
                                          insert_location,
                                          (a_boolean *)NULL,
                                          &keep_constant,
                                          LDIO_FULL_EXPR);
#if CHECKING
    if (keep_constant) {
      internal_error("lower_dtor_init: unexpected result");
    }  /* if */
#endif /* CHECKING */
  } else {
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
    if (ctor_init->kind == (a_constructor_init_kind)cik_virtual_base_class ||
        ctor_init->kind == (a_constructor_init_kind)cik_direct_base_class) {
      a_base_class_ptr base_class = ctor_init->variant.base_class;
      /* Set up for passing an array of virtual function table pointers
         to use during the subobject destruction, if one is necessary. */
      build_construction_vtbls_pointer_for_subobject_construction(
                                                        dip,
                                                        base_class,
                                                        &ipd,
                                                        destruction_vtbls_var,
                                                        insert_location,
                                                        &vtt_addr_node,
                                                        (a_boolean *)NULL);
    }  /* if */
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
    /* Normal case; generate the code to do the destruction. */
    lower_destructor_dynamic_init(dip, &ipd, have_complete_object,
                                  vtt_addr_node, insert_location);
  }  /* if */
}  /* lower_dtor_init */


static void initialize_dtor_init_for_cleanup(
                                         a_dynamic_init_ptr     dip,
                                         a_constructor_init_ptr ctor_init_list)
/*
Do cleanup initialization for the indicated destruction (from
the constructor_inits list of a destructor) and to its successors.
This is done early so the information is available when each entry
is processed.  Called only when exceptions are enabled.
ctor_init_list points to the complete constructor-inits list
for the destructor.
*/
{
  a_destructible_entity_descr_ptr dedp = dip->destructible_entity_descr;
  a_dynamic_init_ptr              next_dip = dip->next_in_destruction_list;

  check_assertion_str(exceptions_enabled,
          "initialize_dtor_init_for_cleanup: called with exceptions disabled");
  check_assertion(dedp != NULL);
  dedp->cleanup_state_to_set_when_starting_destruction = next_dip;
  /* Do a recursive call to process the rest of the list. */
  if (next_dip != NULL) {
    initialize_dtor_init_for_cleanup(next_dip, ctor_init_list);
  }  /* if */
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
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
      /* And another if there is a construction vtable address to be
         passed to a subobject destructor. */
      if (next_dip->is_constructor_init) {
        a_constructor_init_ptr ctor_init;
        /* Find the associated constructor-init entry. */
        for (ctor_init = ctor_init_list;
             ;
             ctor_init = ctor_init->next) {
          check_assertion(ctor_init != NULL);
          if (ctor_init->initializer == next_dip) break;
        }  /* for */
        if (ctor_init->kind ==
                             (a_constructor_init_kind)cik_virtual_base_class ||
            ctor_init->kind ==
                             (a_constructor_init_kind)cik_direct_base_class) {
          a_boolean        needs_vtbl;
          a_base_class_ptr base_class = ctor_init->variant.base_class;
          /* This call just tests whether a vtable is needed; it doesn't
             actually build anything. */
          build_construction_vtbls_pointer_for_subobject_construction(
                                                    next_dip,
                                                    base_class,
                                                    (an_init_pos_descr *)NULL,
                                                    (a_variable_ptr)NULL,
                                                    (an_insert_location *)NULL,
                                                    (an_expr_node_ptr *)NULL,
                                                    &needs_vtbl);
          if (needs_vtbl) region_number++;
        }  /* if */
      }  /* if */
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
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
#if HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS

static an_expr_node_ptr expr_for_dtor_complete_object_test(
                                    a_destructor_wrapper_info_block *dtor_info)
/*
Create an expression that tests whether a destructor is dealing with a
complete object, and return a pointer to it.  dtor_info->complete_obj_var
points to an int variable that is non-zero if the object is complete.
*/
{
  an_expr_node_ptr complete_obj_node;

  /* Make an expression node for the whole-object-indicator variable. */
  complete_obj_node = var_rvalue_expr(dtor_info->complete_obj_var);
  return boolean_controlling_expr(complete_obj_node);
}  /* expr_for_dtor_complete_object_test */

#endif /* HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS */

#if !GENERATE_EH_TABLES
/*ARGSUSED*/  /* <-- prologue_insert_location is not used in some versions. */
#endif /* !GENERATE_EH_TABLES */
static void gen_dtor_member_and_base_destructions(
                     an_insert_location              *prologue_insert_location,
                     a_destructor_wrapper_info_block *dtor_info)
/*
The current function is a destructor.  Generate code to destroy bases
and members, if necessary, and put it in a new block unattached to the
IL tree.  Set dtor_info->epilogue_block to point to the block, or
NULL if no block was needed.  If any code is needed preceding the user
code in the destructor, insert it at *prologue_insert_location.
In particular, code will be inserted there to establish the current
cleanup state for the start of the user-written code in the
destructor, so the insert position should be right before the
user-written code.  *dtor_info is used to pass information between
this function and lower_destructor_code and
insert_dtor_member_and_base_destructions.
*/
{
  a_variable_ptr         this_param_var;
  a_constructor_init_ptr ctor_init, ctor_init_list;
  a_dynamic_init_ptr     first_epilogue_destruction = NULL;
  an_insert_location     insert_location;
  a_source_position      opening_brace_pos, closing_brace_pos;

  /* The following pseudo-code shows both the processing in this routine
     and the code added to the destructor routine.  Lines enclosed in [...]
     are tests and loops done in the processing in this routine; other
     lines are the code added to the destructor routine.  Note that
     there is other wrapper code added by lower_destructor_code.

     [For each data member on the ctor-initializer list:]
       Call the destructor for a complete object.
     [endfor]
     [For each direct nonvirtual base class on the ctor-initializer list:]
       Call the destructor for a subobject.
     [endfor]
     [If there are any items left on the ctor-initializer list (which
         must be for virtual base classes):]
#if HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS
       If a complete object is being destroyed (and therefore virtual base
           classes must be destroyed):
         [For each virtual base class on the ctor-initializer list:]
           Call the destructor for a subobject.
         [endfor]
       endif
#endif // HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS
     [endif]
  */
  /* Get a pointer to the "this" parameter variable. */
  this_param_var = innermost_function_scope->variant.routine.parameters;
  /* The constructor_inits list contains a list of destructions.  Each
     destruction is a default call supplied by the front end.  Every
     base class and member that requires a destructor appears, in the
     order (1) data members, (2) normal base classes, (3) virtual base
     classes.  The order within each section is source declaration order. */
  ctor_init_list = innermost_function_scope->variant.routine.constructor_inits;
#if !HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS
  innermost_function_scope->variant.routine.constructor_inits = NULL;
#endif /* !HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS */
  /* Get the position of the closing brace of the destructor.  Note that
     if the top statement was a try-block it has been rewritten as a
     block, and the source position was preserved. */
  set_position_from_stmt_source_position(
                           closing_brace_pos,
                           innermost_function_scope->assoc_block->variant.
                                             block.extra_info->final_position);
  /* Set the current position to the closing brace of the destructor. */
  code_pos_for_lowering = error_position = closing_brace_pos;
  if (exceptions_enabled) {
#if HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS
    /* Assign cleanup region numbers to the destructions.  This is done
       early so that we will know the right value to set __eh_curr_region
       to when beginning each destruction. */
    /* See whether there are any virtual base classes. */
    if (parent_class_of(innermost_function_scope->variant.routine.ptr)->
                         variant.class_struct_union.any_virtual_base_classes) {
#if DO_FULL_PORTABLE_EH_LOWERING
      /* The variable indicating a complete object will be used
         as a conditional flag for the destructions of the virtual base
         classes (we don't destroy the virtual base classes unless we
         are working on a complete object). */
      a_handle_number   complete_obj_handle;
      an_init_pos_descr ipd;
      /* Assign the object address table slot for the conditional
         variable. */
      complete_obj_handle = object_addr_table_index();
      /* Put the address of the variable into the object address table. */
      set_var_init_pos_descr(dtor_info->complete_obj_var, &ipd);
      init_object_addr_table_entry(&ipd, complete_obj_handle,
                                   prologue_insert_location);
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
      /* Process the ctor-inits for virtual base classes. */
      for (ctor_init = ctor_init_list;
           ctor_init != NULL;
           ctor_init = ctor_init->next) {
        if (ctor_init->kind ==
                             (a_constructor_init_kind)cik_virtual_base_class) {
          /* Add dtor_info->complete_obj_var as a conditional flag. */
          a_destructible_entity_descr_ptr dedp = 
                             ctor_init->initializer->destructible_entity_descr;
          check_assertion(dedp != NULL);
          dedp->conditional_flag_var = dtor_info->complete_obj_var;
#if DO_FULL_PORTABLE_EH_LOWERING
          if (exceptions_enabled) {
            dedp->conditional_flag_handle = complete_obj_handle;
          }  /* if */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
        }  /* if */
      }  /* for */
    }  /* if */
    /* Find the first destruction in the epilogue. */
    first_epilogue_destruction = ctor_init_list->initializer;
#else /* !HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS */
    /* Virtual base class destructions are handled in the complete object
       destructor, not here. */
    if (ctor_init_list == NULL) {
      first_epilogue_destruction = NULL;
    } else {
      first_epilogue_destruction = ctor_init_list->initializer;
      check_assertion(ctor_init_list->kind !=
                              (a_constructor_init_kind)cik_virtual_base_class);
    }  /* if */
#endif /* HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS */
    if (first_epilogue_destruction != NULL) {
      /* Watch out for the case of an array initialization; the top-level
         dynamic initialization is not on the destructions list. */
      if (first_epilogue_destruction->lifetime == NULL) {
        for (first_epilogue_destruction =
                              innermost_function_scope->lifetime->destructions;
             !first_epilogue_destruction->is_constructor_init;
             first_epilogue_destruction =
                     first_epilogue_destruction->next_in_destruction_list) {}
      }  /* if */
      /* Do cleanup initialization for the destructions on the ctor-initializer
         list of the destructor. */
      initialize_dtor_init_for_cleanup(first_epilogue_destruction,
                                       ctor_init_list);
    }  /* if */
    /* Pass the pointer (or NULL) back to the caller. */
    dtor_info->first_epilogue_destruction = first_epilogue_destruction;
  }  /* if */
  /* Create a block unattached to the IL tree, and insert destructions
     inside it. */
  dtor_info->epilogue_block = alloc_statement((a_statement_kind)stmk_block);
  set_block_start_insert_location(dtor_info->epilogue_block, &insert_location);
  /* Generate a destructor call for each data member that appears on the
     ctor_init list. */
  for (ctor_init = ctor_init_list;
       ctor_init != NULL &&
                         ctor_init->kind == (a_constructor_init_kind)cik_field;
       ctor_init = ctor_init->next) {
    lower_dtor_init(ctor_init, this_param_var,
                    /*have_complete_object=*/TRUE,
                    /*base_of_complete_object=*/FALSE,
                    (a_variable_ptr)NULL, &insert_location);
  }  /* for */
  /* Generate a destructor call for each non-virtual direct base class
     that appears on the ctor_init list. */
  for (; ctor_init != NULL &&
             ctor_init->kind == (a_constructor_init_kind)cik_direct_base_class;
       ctor_init = ctor_init->next) {
    lower_dtor_init(ctor_init, this_param_var,
                    /*have_complete_object=*/FALSE,
                    /*base_of_complete_object=*/FALSE,
                    dtor_info->destruction_vtbls_var,
                    &insert_location);
  }  /* for */
#if HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS
  /* If any items remain on the ctor_init list, they must be for virtual
     base classes. */
  if (ctor_init != NULL) {
    an_expr_node_ptr       compare_node;
    an_insert_location     insert_location2;
    check_assertion_str2(ctor_init->kind ==
                               (a_constructor_init_kind)cik_virtual_base_class,
                         "gen_dtor_member_and_base_destructions:",
                         "bad ctor_init item kind");
    /* Put out code that tests whether or not the virtual base classes need
       to be destroyed.  This is done by testing whether we have
       a complete object. */
    compare_node = expr_for_dtor_complete_object_test(dtor_info);
    /* Make an "if" statement with a block statement under it:
         if (complete-obj-test) {}
                                 ^--- additional statements will be inserted.
    */
    insert_if_statement(compare_node, /*is_initialization_guard=*/FALSE,
                        &insert_location, (a_statement_ptr *)NULL,
                        &insert_location2, (an_insert_location *)NULL);
    /* Destroy any virtual base classes on the ctor_init list. */
    for (; ctor_init != NULL; ctor_init = ctor_init->next) {
      lower_dtor_init(ctor_init, this_param_var,
                      /*have_complete_object=*/FALSE,
                      /*base_of_complete_object=*/TRUE,
                      dtor_info->destruction_vtbls_var,
                      &insert_location2);
    }  /* for */
    /* Note that the "if" created above effectively ends here. */
  }  /* if */
#else /* !HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS */
  /* Should be no virtual base ctor_inits. */
  check_assertion(ctor_init == NULL ||
                  ctor_init->kind !=
                              (a_constructor_init_kind)cik_virtual_base_class);
#endif /* HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS */
  /* Set the current position to the opening brace of the destructor. */
  set_position_from_stmt_source_position(opening_brace_pos,
                                         innermost_function_scope->
                                                        assoc_block->position);
  error_position = code_pos_for_lowering = opening_brace_pos;
  /* Insert the region table and initial cleanup state. */
  insert_epilogue_cleanup_state(first_epilogue_destruction,
                                prologue_insert_location);
  if (dtor_info->epilogue_block->variant.block.statements == NULL) {
    /* No destructions were emitted. */
    dtor_info->epilogue_block = NULL;
  }  /* if */
}  /* gen_dtor_member_and_base_destructions */


void add_function_try_wrapper_code(a_statement_ptr                 statement,
                                   a_destructor_wrapper_info_block *dtor_info)
/*
statement is an stmk_block statement that is the dependent statement
of a function try block for a constructor or destructor.  Add
any wrapper code needed to initialize members and bases.
(The wrapper code that would ordinarily be inserted at the top
level of the constructor or destructor body goes inside the
dependent statement of the try block, because the destructions
of the members and bases happen before the handler for the try
is entered, if an exception is thrown.)  *dtor_info is used to
pass information between this function and lower_destructor_code
and insert_dtor_member_and_base_destructions; dtor_info is NULL
for a constructor.
*/
{
#if ASSIGNMENT_TO_THIS_ALLOWED || CHECKING
  a_routine_ptr      routine = current_routine_entry();
#endif /* ASSIGNMENT_TO_THIS_ALLOWED || CHECKING */
  an_insert_location insert_location;

  set_block_start_insert_location(statement, &insert_location);
  if (dtor_info == NULL) {
    check_assertion(routine->special_kind ==
                                     (a_special_function_kind)sfk_constructor);
    /* Function try block in a constructor.  Insert constructor wrapper
       code before the dependent statement. */
#if ASSIGNMENT_TO_THIS_ALLOWED
    /* If an assignment to "this" was done, the wrapper code is
       generated at each assignment. */
    if (!routine->assignment_to_this_done)
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
    /* Do not insert code here. */
    {
      /* Generate member and base initialization code. */
      add_constructor_wrapper_code(innermost_function_scope,
                                   &insert_location);
    }  /* if */
  } else {
    check_assertion(routine->special_kind ==
                                      (a_special_function_kind)sfk_destructor);
    /* Function try block in a destructor.  Generate member and base
       destruction code in a block off to the side.  This must be done
       early so that the proper exception cleanup actions can be put on
       the cleanup list before the user code is lowered. */
    if (innermost_function_scope->variant.routine.constructor_inits != NULL) {
      gen_dtor_member_and_base_destructions(&insert_location,
                                            dtor_info);
    }  /* if */
  }  /* if */
}  /* add_function_try_wrapper_code */


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
  temp_label->exec_stmt = label_stmt;
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
      insert_location->variant.statement.stmt->kind ==
                                                (a_statement_kind)stmk_block) {
    /* We are adding after a block.  See whether the last statement of the
       block is a return.  If so, move it out of the block. */
    a_statement_ptr block_stmt = insert_location->variant.statement.stmt;
    if (move_final_return_out_of_block(block_stmt, block_stmt)) {
      /* A return was moved out of the block.  Set the insert location
         to the return.  This allows further optimization below. */
      set_insert_location(block_stmt->next, insert_location);
    }  /* if */
  }  /* if */
  if (insert_location->kind == ilk_after_statement &&
      insert_location->variant.statement.stmt->kind ==
                                               (a_statement_kind)stmk_return) {
    /* We're inserting after a top-level return.  We can insert in
       front of it and avoid adding another return. */
    top_level_return = insert_location->variant.statement.stmt;
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
                              an_insert_location              *insert_location,
                              a_statement_ptr                 insert_block,
                              a_destructor_wrapper_info_block *dtor_info)
/*
Code to destroy members and bases in a destructor was generated earlier by
gen_dtor_member_and_base_destructions.  dtor_info->epilogue_block
points to the generated code, which is not currently attached to the
IL tree, or is NULL if there is no such code.  Insert the generated code
at *insert_location (which is either at the top level of the destructor,
or inside a function-try-block).  This insertion is after the
user-written code in the destructor or function-try-block.
insert_block indicates the block in which *insert_location appears.
On return, *insert_location is set to allow further insertion in
the epilogue, preceding a return statement (one that was present or
one that was added).  *dtor_info is used to pass information from
gen_dtor_member_and_base_destructions.
*/
{
  a_statement_ptr destruction_code = dtor_info->epilogue_block;
  a_boolean       label_added;

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
  a_variable_ptr         this_param_var;
  a_type_ptr             class_type;
  a_class_type_supplement_ptr
                         ctsp;
  an_insert_location     insert_location;
  a_statement_ptr        top_stmt = scope->assoc_block;
  a_statement_ptr        user_code_stmts;
  a_boolean              has_function_try_block = FALSE;
  a_constructor_init_ptr ctor_init;
  an_expr_node_ptr       vtbl_addr_node, vptr_node;
  a_variable_ptr         vtbl_var;
  a_routine_ptr          dtor_routine = scope->variant.routine.ptr;
  a_source_position      saved_error_position, saved_code_pos;
  a_source_position      opening_brace_pos;
#if !IA64_ABI
  an_insert_location     insert_location3;
  a_source_position      closing_brace_pos;
  a_routine_ptr          delete_routine;
  a_boolean              epilogue_setup_done = FALSE;
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
  an_insert_location     else_insert_location;
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
#endif /* !IA64_ABI */
  a_destructor_wrapper_info_block
                         dtor_info;

  /* The following pseudo-code shows both the processing in this routine
     and the code added to the destructor routine.  Lines enclosed in [...]
     are tests and loops done in the processing in this routine; other
     lines are the code added to the destructor routine.  Note that the
     processing for virtual base class destruction (as indicated below when
     HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS is TRUE) can be performed in
     either the subobject or complete object destructor (or both), depending
     upon the configuration.

#if !IA64_ABI
     [If a delete can be folded into the destructor:]
       If this != NULL test around entire routine.
     [endif]
#endif // !IA64_ABI
#if HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS
     [If the current class requires a special array of virtual function table
         addresses (which is true when the class has virtual functions
         in virtual bases that are overridden):]
       If a complete object is being destroyed:
         Set the destruction_vtbls temp to point to a local static array
           containing vtbl pointer values to be used for a complete object.
#if !IA64_ABI
       else
         Set the destruction_vtbls temp to the value of the transfer
             pointer in the class (the caller uses that to pass in the
             address of the array of vtbl pointers to be used during the
             subobject destruction).
#endif // !IA64_ABI
       endif
     [endif]
#endif // HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS
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
#if !IA64_ABI
     If (added parameter & 0x1) != 0:
       delete((void*)this)
     endif
#endif // !IA64_ABI
     return;
  */
  saved_code_pos = code_pos_for_lowering;
  saved_error_position = error_position;
  /* Set the current position to the opening brace of the destructor. */
  set_position_from_stmt_source_position(opening_brace_pos,
                                         top_stmt->position);
  error_position = code_pos_for_lowering = opening_brace_pos;
  /* Get a pointer to the "this" parameter variable. */
  this_param_var = scope->variant.routine.parameters;
  class_type = parent_class_of(dtor_routine);
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
#if !IA64_ABI
  /* Get the position of the closing brace of the destructor.  Note that
     if the top statement was a try-block it has been rewritten as a
     block, and the source position was preserved. */
  set_position_from_stmt_source_position(
                           closing_brace_pos,
                           top_stmt->variant.block.extra_info->final_position);
#endif /* !IA64_ABI */
#if HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS
  if (class_type->variant.class_struct_union.any_virtual_base_classes &&
      dtor_routine->ctor_dtor_kind == (a_ctor_or_dtor_kind)cdk_subobject) {
    /* Create the complete object destructor early so that we can move
       (or copy) any virtual base destructions into the complete
       object destructor scope. */
    (void)alternate_entry_point(scope->variant.routine.ptr,
                                (a_ctor_or_dtor_kind)cdk_complete,
                                /*define_now=*/TRUE);
  }  /* if */
#endif /* HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS */
  /* Start an object lifetime if appropriate. */
  begin_block_object_lifetime(scope->lifetime, &insert_location);
  dtor_info.epilogue_block = NULL;
  dtor_info.first_epilogue_destruction = NULL;
  dtor_info.destruction_vtbls_var = NULL;
  dtor_info.complete_obj_var = NULL;
#if !IA64_ABI
  if (class_type->variant.class_struct_union.any_virtual_base_classes) {
    /* We need a variable that is non-zero to indicate that a complete
       object is being destroyed.  For the IA-64 ABI, see below. */
    /* For the Cfront-like ABI, we can test the complete-object parameter
       directly.  A "!= 0" test works okay because the 0x1 bit (for
       "free storage") would only be on for a whole object. */
    dtor_info.complete_obj_var = this_param_var->next;
  }  /* if */
#endif /* !IA64_ABI */
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
  if (ctsp->construction_vtbls != NULL) {
#if HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS
    an_expr_node_ptr   compare_node;
    an_insert_location insert_location2;
#endif /* HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS */
    /* This class is one that has overridden virtual functions in virtual
       base classes, and needs special versions of the virtual function
       tables when used to destruct a subobject. */
    /* Create a temporary that will point to an array of virtual function
       table addresses.  In the IA-64 ABI, the VTT parameter is used
       directly. */
#if !IA64_ABI
    dtor_info.destruction_vtbls_var = make_construction_vtbl_temporary();
#else /* IA64_ABI */
    dtor_info.destruction_vtbls_var = this_param_var->next;
#endif /* IA64_ABI */
#if HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS
#if IA64_ABI
    /* For the IA-64 ABI, the VTT parameter is NULL if we are destroying
       a complete object.  We need to generate a temporary that is non-zero
       when the VTT parameter is NULL, i.e., temp = (vtt-param == NULL). */
    { a_constant       null_constant;
      an_expr_node_ptr null_constant_node, vtt_param_node;
      a_variable_ptr   vtt_param_var = this_param_var->next;
      dtor_info.complete_obj_var =
                 make_lowered_temporary(integer_type((an_integer_kind)ik_int));
      make_zero_of_proper_type(f_skip_typerefs(vtt_param_var->type),
                               &null_constant);
      null_constant_node = alloc_node_for_constant(&null_constant);
      vtt_param_node = var_rvalue_expr(vtt_param_var);
      vtt_param_node->next = null_constant_node;
      compare_node = make_operator_node((an_expr_operator_kind)eok_eq,
                                        integer_type((an_integer_kind)ik_int),
                                        vtt_param_node);
      (void)insert_var_assignment_statement(dtor_info.complete_obj_var,
                                            compare_node, &insert_location);
    }
#endif /* IA64_ABI */
    /* Put out code that tests whether we are destroying a complete object. */
    compare_node = expr_for_dtor_complete_object_test(&dtor_info);
    /* Make an "if" statement with a block statement under it:
         if (complete-obj-test) {} else {}
                                 ^-------^-- statements will be inserted.
    */
    insert_if_statement(compare_node, /*is_initialization_guard=*/FALSE,
                        &insert_location, (a_statement_ptr *)NULL,
                        &insert_location2,
#if IA64_ABI
                        (an_insert_location *)NULL  /* No "else" needed. */
#else /* !IA64_ABI */
                        &else_insert_location
#endif /* IA64_ABI */
                                             );
    /* Inserting under insert_location2, in the "then" part of the "if"
       (a complete object is being destroyed): */
    /* Set the destruction_vtbls temporary to point to the default array
       of virtual function table pointers to be used when destroying a
       complete object. */
    insert_default_construction_vtbls_assignment(class_type,
                                                 dtor_info.
                                                         destruction_vtbls_var,
                                                 &insert_location2);
#if !IA64_ABI
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
                                                        &else_insert_location);
#endif /* !IA64_ABI */
#endif /* HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS */
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
    vtbl_addr_node = NULL;
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
    if (bcp->index_in_construction_vtbl_array != 0) {
      /* Set the virtual function table pointer to an element from the
         array of construction virtual function table pointers. */
      vtbl_addr_node = vtbl_addr_from_construction_vtbls_array(
                                        dtor_info.destruction_vtbls_var,
                                        /*var_is_array=*/FALSE,
                                        bcp->index_in_construction_vtbl_array);
      /* Add an indirection and convert it to an rvalue for use in
         the assignment statement below. */
      vtbl_addr_node = add_indirection_to_node(vtbl_addr_node);
      vtbl_addr_node = rvalue_expr_for_lvalue(vtbl_addr_node);
    } else
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
    /* Do not insert code here; this is the "else" of an "if". */
    {
#if !IA64_ABI
      vtbl_var = bcp->virtual_function_table_var;
      /* If class_type has no virtual functions but the base class does,
         it's possible that the virtual function table pointer in the base
         class is currently set for a class derived from class_type.  Consider:
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
#else /* IA64_ABI */
      if (base_class_has_vtbl(bcp)) {
        vtbl_var = ctsp->virtual_function_table_var;
      } else {
        vtbl_var = NULL;
      }  /* if */
#endif /* IA64_ABI */
      if (vtbl_var != NULL) {
        /* Set the virtual function table from the standard virtual function
           table for this base class. */
        vtbl_addr_node = make_vtbl_address_node(vtbl_var, class_type, bcp);
      }  /* if */
    }  /* if */
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
                                        (an_expr_operator_kind)eok_assign,
                                        vtbl_addr_node,
                                        &insert_location);
    }  /* if */
  }  /* for */
  /* Now generate epilogue wrapper code to destroy members and base classes.
     This is done early, and into a block off to the side, so that the
     proper exception cleanup actions can be put on the cleanup list before
     the user code is lowered.  Later, the epilogue block will be inserted
     into the destructor at the right place. */
  /* For a destructor with a function-try-block, this processing is
     done inside lower_try_block (so that the lifetimes are right). */
  if (ctor_init != NULL && !has_function_try_block) {
    /* Create code to destroy members and bases. */
    gen_dtor_member_and_base_destructions(&insert_location,
                                          &dtor_info);
  }  /* if */
  /* Now lower the user code. */
  if (has_function_try_block) {
    /* The top statement of the destructor is a function-try-block.
       The code to destroy members and bases is inserted inside the
       try block. */
    lower_try_block(user_code_stmts, /*is_function_try_block=*/TRUE,
                    &dtor_info);
    set_insert_location(user_code_stmts, &insert_location);
  } else {
    /* Normal case (not function-try-block). */
    a_statement_ptr last_stmt;
    lower_statement_list(user_code_stmts, &last_stmt);
    set_insert_location(last_stmt, &insert_location);
    /* Insert the code to destroy members and bases, generated earlier. */
    /* This also changes the insert location from after the final return
       to before it. */
    insert_dtor_member_and_base_destructions(&insert_location,
                                             top_stmt,
                                             &dtor_info);
#if !IA64_ABI
    epilogue_setup_done = TRUE;
#endif /* !IA64_ABI */
  }  /* if */
#if !IA64_ABI
  /* Set the current position to the closing brace of the destructor. */
  code_pos_for_lowering = error_position = closing_brace_pos;
  /* Add code to free the storage if the "free" bit (0x1) is on in the
     added parameter:
       if (param & 0x1) delete-routine((void *)this);
     Watch out for the case where the delete routine pointer is NULL; this
     happens if a derived class inherits more than one delete routine, and
     therefore they're ambiguous.
  */
  /* In the IA-64 ABI, this code is in the deleting destructor. */
  delete_routine = ctsp->assoc_operator_delete_routine;
  if (delete_routine != NULL) {
    an_expr_node_ptr this_param_node;
    an_expr_node_ptr and_node, two_constant_node, if_node;
    a_type_ptr       int_type = integer_type((an_integer_kind)ik_int);
    an_expr_node_ptr complete_obj_param_node;

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
    complete_obj_param_node = var_rvalue_expr(this_param_var->next);
    two_constant_node = node_for_integer_constant(1L, (an_integer_kind)ik_int);
    complete_obj_param_node->next = two_constant_node;
    and_node = make_operator_node((an_expr_operator_kind)eok_and,
                                  int_type, complete_obj_param_node);
    if_node = boolean_controlling_expr(and_node);
#if ASSIGNMENT_TO_THIS_ALLOWED
    /* If an assignment to "this" was done in the body of the destructor,
       also test that "this" isn't NULL. */
    if (dtor_routine->assignment_to_this_done) {
      an_expr_node_ptr this_test_node;
      this_param_node = var_rvalue_expr(this_param_var);
      this_test_node = boolean_controlling_expr(this_param_node);
      /* Make "this && (param & 0x1)". */
      this_test_node->next = if_node;
      if_node = make_operator_node((an_expr_operator_kind)eok_land,
                                   int_type, this_test_node);
    }  /* if */
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
    /* Make "if (param & 0x1)". */
    insert_if_statement(if_node, /*is_initialization_guard=*/FALSE,
                        &insert_location, (a_statement_ptr *)NULL,
                        &insert_location3, (an_insert_location *)NULL);
    /* Make "delete-routine((void *)this);" under the "if". */
    this_param_node = var_rvalue_expr(this_param_var);
    make_delete_call_statement(delete_routine, class_type, this_param_node,
                               &insert_location3);
#if LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS
    /* Mark the "if" statement and the "delete" call. */
    check_assertion(insert_location.kind == ilk_after_statement &&
                    insert_location3.kind == ilk_after_statement);
    insert_location.variant.statement.stmt->is_lowering_boilerplate = TRUE;
    insert_location3.variant.statement.stmt->is_lowering_boilerplate = TRUE;
#endif /* LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS */
  }  /* if */
  /* Add "if (this != NULL)" around the whole routine. */
  add_null_test_around_routine(scope);
#endif /* !IA64_ABI */
  error_position = saved_error_position;
  code_pos_for_lowering = saved_code_pos;
}  /* lower_destructor_code */

#if USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES

static void add_guard_code_to_thread_local_init(a_scope_ptr scope)
/*
Add guard code to ensure that the code contained in the scope (which contains
dynamic initialization for all thread_local variables in this translation
unit) is only performed once per thread.  A simple guard is all that is needed
(because these variables are already unique to a thread so no other thread
can access them simultaneously).
*/
{
  a_variable_ptr      guard;
  an_expr_node_ptr    test_var_node, compare_node;
  an_insert_location  insert_location;

  /* The guard variable need not be visible outside of the function,
     so an unnamed, thread-local variable is fine. */
  guard = make_temporary_in_scope(integer_type((an_integer_kind)ik_char),
                                  (a_scope_ptr)NULL,
                                  /*force_static=*/TRUE,
                                  /*promote_if_necessary=*/TRUE);
  guard->is_thread_local = TRUE;
  /* Add "guard = 1;" at the beginning of the block. */
  set_block_start_insert_location(scope->assoc_block, &insert_location);
  (void)insert_var_assignment_statement(guard,
                                        node_for_integer_constant(1L,
                                                     (an_integer_kind)ik_char),
                                        &insert_location);
  /* Enclose the entire scope in an "if (guard == 0)" statement. */
  test_var_node = var_rvalue_expr(guard);
  test_var_node->next = node_for_integer_constant(0L,
                                                  (an_integer_kind)ik_char);
  compare_node = make_operator_node((an_expr_operator_kind)eok_eq,
                                    integer_type((an_integer_kind)ik_int),
                                    test_var_node);
  enclose_routine_in_if(scope, compare_node, (a_variable_ptr)NULL);
}  /* add_guard_code_to_thread_local_init */


static a_routine_ptr thread_local_init_routine_for_variable(a_variable_ptr var)
/*
Return the routine to use for initializing the specified thread_local variable.
These routines are used in the "lazy initialization" for thread_local
variables and are typically aliases with a well-known name; calling such
a routine guarantees the caller that the thread_local variable has been
properly initialized.  The routine has no definition (the back end emits
an alias for __tls_init in the current translation unit if the variable
is defined and dynamically initialized in the current translation unit).
Note that in the case of a template static data member, the storage class of
var may be sc_extern and later change to sc_unspecified (if the template static
data member is instantiated in this translation unit) -- this is fixed up later
(in b_lower_file_scope_dynamic_inits) if necessary.
*/
{
  a_routine_ptr   init_routine;
  a_const_char    *init_name;

  if (var->init_routine.thread.init_routine == NULL) {
    /* This routine is an alias for the __tls_init routine. */
    init_name = make_prefixed_object_name(
#if IA64_ABI
                                          "_ZTH",
#else /* !IA64_ABI */
                                          "__THI__",
#endif /* IA64_ABI */
                                          &var->source_corresp,
                                          (an_il_entry_kind)iek_variable);
    init_routine = make_rout_entry(init_name,
                                   var->storage_class,
                                   void_type(),
                                   (a_type_ptr)NULL);
    init_routine->source_corresp.name_has_been_mangled = TRUE;
    init_routine->type->variant.routine.extra_info->prototyped = TRUE;
    init_routine->is_tls_init_alias = TRUE;
#if LAZY_INITIALIZATION_USES_WEAK_REFERENCES && GNU_EXTENSIONS_ALLOWED
    if (init_routine->storage_class != (a_storage_class)sc_static) {
      init_routine->is_weak = TRUE;
    }  /* if */
#endif /* LAZY_INITIALIZATION_USES_WEAK_REFERENCES && GNU_EXTENSIONS_ALLOWED */
    var->init_routine.thread.init_routine = init_routine;
  }  /* if */
  return var->init_routine.thread.init_routine;
}  /* thread_local_init_routine_for_variable */

#if !LAZY_INITIALIZATION_USES_WEAK_REFERENCES

void make_null_thread_local_init_routine_for_variable(a_variable_ptr var)
/*
If we're not using weak references, then every thread_local variable with
external linkage needs to have an initialization routine defined, even if it
does nothing.  This routine creates the do-nothing routine for such cases.
*/
{
  a_routine_ptr   routine;
  a_scope_ptr     scope;
  a_generated_routine_context
                  grcontext;
  a_statement_ptr return_stmt;
  a_memory_region_number
                  region_number;

  check_assertion(var->init_routine.thread.init_routine == NULL);
  routine = thread_local_init_routine_for_variable(var);
  /* Make a memory region, scope, and block for the routine definition. */
  scope = make_routine_definition(routine, /*make_return=*/TRUE,
                                  &region_number);
  push_generated_routine_context(scope, region_number, &grcontext);
  /* Add the return statement at the end of the routine to the return memo
     list. */
  return_stmt = scope->assoc_block->variant.block.statements;
  check_assertion(return_stmt != NULL &&
                  return_stmt->kind == (a_statement_kind)stmk_return);
  add_to_return_memo_list(return_stmt);
  pop_generated_routine_context(scope, region_number, &grcontext);
}  /* make_null_thread_local_init_routine_for_variable */

#endif /* !LAZY_INITIALIZATION_USES_WEAK_REFERENCES */

a_routine_ptr thread_local_wrapper_for_variable(a_variable_ptr var)
/*
Return the wrapper routine for the specified thread_local variable.  The
wrapper routine is used as a replacement for uses of thread_local variables
in cases where such variables may have a dynamic initialization associated
with them.  The wrapper calls the variable's init routine (if it has one)
and then returns a pointer to the variable itself.  If the wrapper routine
has not yet been defined, it is created here.
*/
{
  a_scope_ptr            scope;
  an_insert_location     insert_location;
  an_insert_location     *call_insert_location;
  a_memory_region_number region_number;
  a_generated_routine_context
                         grcontext;
  a_routine_ptr          wrapper_routine, init_routine;
  a_statement_ptr        return_stmt;
  a_const_char           *wrapper_name;
  a_type_ptr             wrapper_type;
#if LAZY_INITIALIZATION_USES_WEAK_REFERENCES
  an_insert_location     if_insert_location;
#endif /* LAZY_INITIALIZATION_USES_WEAK_REFERENCES */

  if (var->init_routine.thread.wrapper == NULL) {
    /* Create the routine and give it a well-known name (based on the
       variable's name). */
    wrapper_name = make_prefixed_object_name(
#if IA64_ABI
                                             "_ZTW",
#else /* !IA64_ABI */
                                             "__TWR__",
#endif /* IA64_ABI */
                                             &var->source_corresp,
                                             (an_il_entry_kind)iek_variable);
    if (is_reference_type(var->type)) {
      /* Replace a reference type with a pointer type. */
      wrapper_type = make_pointer_type(type_pointed_to(var->type));
    } else {
      wrapper_type = var->type;
    }  /* if */
    wrapper_type = make_pointer_type(wrapper_type);
    wrapper_routine = make_rout_entry(wrapper_name,
                                      var->storage_class,
                                      wrapper_type,
                                      (a_type_ptr)NULL);
    wrapper_routine->source_corresp.name_has_been_mangled = TRUE;
    wrapper_routine->type->variant.routine.extra_info->prototyped = TRUE;
    /* Make a memory region, scope, and block for the routine definition. */
    scope = make_routine_definition(wrapper_routine, /*make_return=*/FALSE,
                                    &region_number);
    if (wrapper_routine->storage_class == (a_storage_class)sc_static) {
      /* Set the inline flag. */
      set_inline_flag(wrapper_routine, TRUE);
#if MINIMAL_INLINING
      wrapper_routine->inlinable = TRUE;
#endif /* MINIMAL_INLINING */
    } else {
#if LAZY_INITIALIZATION_USES_WEAK_REFERENCES && GNU_EXTENSIONS_ALLOWED
      /* Set the "weak" attribute since this routine may be defined in more
         than one translation unit. */
      wrapper_routine->is_weak = TRUE;
#else /* !LAZY_INITIALIZATION_USES_WEAK_REFERENCES && GNU_EXTENSIONS_ALLOWED */
      /* Each translation unit that uses this thread_local variable will
         emit its own wrapper routine, so ensure they're all static
         (to avoid multiple definition errors from the linker). */
      wrapper_routine->storage_class = (a_storage_class)sc_static;
#endif /* LAZY_INITIALIZATION_USES_WEAK_REFERENCES && GNU_EXTENSIONS_ALLOWED */
    }  /* if */
    push_generated_routine_context(scope, region_number, &grcontext);
    set_block_start_insert_location(scope->assoc_block, &insert_location);
    /* In cases where we know the variable has a dynamic initialization
       (because it's in this translation unit), the wrapper looks like:

         extern void var_init() __attribute__ ((weak));
         inline T* var_wrapper() {
           var_init();
           return &var;
         }

       For the case where the variable is not defined in this translation
       unit (and we're using weak references), an additional "if" statement
       is added:

         extern void var_init() __attribute__ ((weak));
         inline T* var_wrapper() {
           if (var_init) var_init();
           return &var;
         }

        If weak references are being used and the variable is not
        dynamically initialized, there will be no definition of var_init.
        When weak references are not used, the "if" statement isn't needed --
        a var_init routine is emitted for all thread_local variables with
        external linkage in that case. */
    init_routine = thread_local_init_routine_for_variable(var);
    call_insert_location = &insert_location;
#if LAZY_INITIALIZATION_USES_WEAK_REFERENCES
    if (var->storage_class == (a_storage_class)sc_extern) {
      an_expr_node_ptr test_node;
      /* Make the boolean controlling expression "test_var". */
      test_node = function_addr_expr(init_routine);
      test_node = boolean_controlling_expr(test_node);
      /* Make an "if" statement and insert it into the program. */
      insert_if_statement(test_node, /*is_initialization_guard=*/FALSE,
                          &insert_location, (a_statement_ptr *)NULL,
                          &if_insert_location, (an_insert_location *)NULL);
      call_insert_location = &if_insert_location;
    }  /* if */
#endif /* LAZY_INITIALIZATION_USES_WEAK_REFERENCES */
    /* Call the initialization routine. */
    make_call_statement(init_routine, (an_expr_node_ptr)NULL,
                        (an_expr_node_ptr)NULL, call_insert_location);
    /* Return a pointer to the variable. */
    return_stmt = alloc_statement((a_statement_kind)stmk_return);
    return_stmt->expr = var_addr_expr(var);
    insert_statement(return_stmt, &insert_location);
    add_to_return_memo_list(return_stmt);
    /* Finish up. */
    pop_generated_routine_context(scope, region_number, &grcontext);
    var->init_routine.thread.wrapper = wrapper_routine;
  }  /* if */
  return var->init_routine.thread.wrapper;
}  /* thread_local_wrapper_for_variable */

#endif /* USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES */

/*ARGSUSED*/  /* <-- tblock is not used. */
static void set_dynamic_init_included_in_slice(
                                    a_dynamic_init_ptr                  dip,
                                    an_expr_or_stmt_traversal_block_ptr tblock)
/*
Called from the expression traversal routines.  Set the included_in_slice
flag in the indicated dynamic initialization entry.
*/
{
  dip->included_in_slice = TRUE;
}  /* set_dynamic_init_included_in_slice */


static void mark_slice_dyn_inits(a_dynamic_init_ptr dip)
/*
Set the included_in_slice flag in the given dynamic initialization and
in all dynamic initializations under it.
*/
{
  an_expr_or_stmt_traversal_block tblock;

  clear_expr_or_stmt_traversal_block(&tblock);
  tblock.process_dynamic_init = set_dynamic_init_included_in_slice;
  traverse_dynamic_init(dip, &tblock);
}  /* mark_slice_dyn_inits */


#if SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
static a_routine_list_entry_ptr
                file_scope_dynamic_init_routines_tail;
                        /* Points to the last entry on the list of routine
                           entries pointed to by
                           il_header.file_scope_dynamic_init_routines. */
#if !USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES
static a_routine_list_entry_ptr
                thread_local_dynamic_init_routines_tail;
                        /* Points to the last entry on the list of routine
                           entries pointed to by
                           il_header.thread_local_dynamic_init_routines. */
#endif /* !USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES */
#endif /* SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */

#if !SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
/*ARGSUSED*/ /* more_matching_inits is not used in that case. */
#endif /* !SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */
static void b_lower_file_scope_dynamic_inits(
                                      unsigned long       needed_bit_number,
                                      int                 init_priority,
                                      a_boolean           do_single_init,
                                      a_boolean           do_thread_local,
                                      a_boolean           *more_matching_inits)
/*
Do lowering on the file-scope dynamic initializations list.  Determine the set
of file-scope dynamic initializations that match the input criteria (described
below) and generate an initialization routine to process that set of dynamic
initializations.  No routine is created if no dynamic initializations match the
criteria.  If needed_bit_number is non-zero, it is the needed flag bit number
for an instantiation, and only initializations for that bit number should be
included in the initialization routine.  If init_priority is non-zero, only
variables with the GNU init_priority field equal to that value are included.
If do_single_init is TRUE, only the first initialization that matches the
criteria (if any -- as specified by needed_bit_number and init_priority) is
lowered and included in the generated initialization routine.  When
do_single_init is TRUE, *more_matching_inits is set to indicate whether
there are additional dynamic initializations that match the criteria.
When do_thread_local is TRUE, only dynamic initializations for thread_local
variables are considered.  When configured with USE_PATCH_INIT_STARTUP, the
generated routine is queued on a list of routines to be executed at startup;
in other cases, the name of the routine (typically with the __sti__ prefix) is
enough to cause the back end to invoke the routine at initialization.
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
  a_boolean          processing_partial_list = FALSE;
  a_dynamic_init_ptr process_list, end_process_list;
  a_dynamic_init_ptr dtor_process_list, end_dtor_process_list;
  a_dynamic_init_ptr delay_list = NULL, end_delay_list;
  a_dynamic_init_ptr dtor_delay_list = NULL, end_dtor_delay_list;
#if SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS || MAINTAIN_NEEDED_FLAGS ||\
    USE_PATCH_INIT_STARTUP
  a_routine_ptr      init_rout;
#endif /* SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS || ... */
  a_variable_ptr     var;

  dip = file_scope->dynamic_inits;
#if ONE_INSTANTIATION_PER_OBJECT
  if (needed_bit_number == 1) eff_needed_bit_number = 0;
  if (needed_bit_number != 0) processing_partial_list = TRUE;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
  if (init_priority != 0) processing_partial_list = TRUE;
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
  if (do_thread_local) processing_partial_list = TRUE;
#if SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
  if (do_single_init) {
    check_assertion(more_matching_inits != NULL);
    *more_matching_inits = FALSE;
    processing_partial_list = TRUE;
  }  /* if */
#endif /* SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */
  if (processing_partial_list) {
    /* We're putting out separate initialization routines for different
       groups of initializations.  Split the dynamic initializations list
       into two lists: one that gets processed on this call, and another that
       does not get processed and goes back on the list after we're done with
       this call, for processing on a subsequent call. */
    process_list = end_process_list = NULL;
    delay_list = end_delay_list = NULL;
    for (; dip != NULL; dip = dip_next) {
      a_boolean process_dip = TRUE;
      var = dip->variable;
      dip_next = dip->next;
      dip->next = NULL;
      /* Determine whether this variable initialization should be emitted
         in the current initialization routine. */
      if (do_thread_local && !var->is_thread_local) {
        /* If we're processing thread_local initializations and this isn't
           one of them, skip it. */
        process_dip = FALSE;
      }  /* if */
#if ONE_INSTANTIATION_PER_OBJECT
      if (one_instantiation_per_object &&
          var->instantiation_needed_bit_number != eff_needed_bit_number) {
        /* This variable initialization is not in the current slice;
           skip it. */
        process_dip = FALSE;
      }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
      if (var->init_priority != init_priority &&
          !do_thread_local) {
        /* The init_priority doesn't match (or we're processing thread_local
           initializations); skip it for now. */
        process_dip = FALSE;
      }  /* if */
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
      if (process_dip) {
#if SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
        if (do_single_init && process_list != NULL) {
          if (more_matching_inits != NULL) *more_matching_inits = TRUE;
          /* Put the remaining dips on the delay list. */
          dip->next = dip_next;
          if (end_delay_list == NULL) {
            delay_list = dip;
          } else {
            end_delay_list->next = dip;
          }  /* if */
          end_delay_list = dip;
          /* No need to go further. */
          break;
        }  /* if */
#endif /* SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */
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
  if (dip != NULL) {
    /* There are some applicable file-scope dynamic initializations.
       Generate a routine containing them. */
    scope = file_scope_init_insert_location(eff_needed_bit_number,
                                            init_priority,
                                            do_single_init ?
                                              unique_id_for_il_pointer(dip) :
                                              (unsigned long)0,
                                            do_thread_local,
                                            &insert_location, &region_number,
                                            &grcontext);
    processing_file_scope_init_routine = TRUE;
#if SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS || MAINTAIN_NEEDED_FLAGS ||\
    USE_PATCH_INIT_STARTUP
    init_rout = scope->variant.routine.ptr;
#endif /* SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS || ... */
    if (file_scope->lifetime != NULL) {
      begin_object_lifetime(file_scope->lifetime, &insert_location);
    }  /* if */
    /* Generate the initializations. */
    for (; dip != NULL; dip = dip_next) {
      an_insert_location_ptr eff_insert_location = &insert_location;
#if TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE
      an_insert_location     insert_location2;
      a_variable_ptr         guard_var = NULL;
#endif /* TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE */
      var = dip->variable;
#if TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE
      if (var->is_template_static_data_member) {
        /* This is the initialization of a static data member in a template.
           Add guard code around the initialization if necessary. */
        if (add_static_data_member_init_guard_test(var,
                                                   &insert_location,
                                                   &insert_location2,
                                                   &guard_var)) {
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
      set_var_init_pos_descr(var, &ipd);
#if LOWER_DESIGNATED_INITIALIZERS
      lower_dynamic_init_designated_initializers(dip, (a_type_ptr)NULL);
#endif /* LOWER_DESIGNATED_INITIALIZERS */
      check_assertion(pending_stmk_init_statements == NULL);
      /* Mark the location where any generated stmk_init statements should
         go. */
      set_insert_location_mark(eff_insert_location);
      lower_dynamic_init(dip, &ipd,
                         (an_implied_copy_source *)NULL,
                         (a_variable_ptr)NULL,
                         LDIO_FULL_EXPR,
                         /*others_follow_in_aggr=*/FALSE,
                         eff_insert_location, (a_boolean *)NULL,
                         (a_constant **)NULL);
      /* Insert any generated stmk_inits at the previously marked location. */
      insert_pending_stmk_init_statements_at_mark(eff_insert_location);
#if TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE && IA64_ABI
      if (guard_var != NULL) {
        /* Set the guard variable to indicate the local static is initialized
           after the initialization is completed. */
        set_local_static_guard_var(guard_var, eff_insert_location);
      }  /* if */
#endif /* TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE && ... */
#if SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
      if (do_single_init) {
        a_routine_list_entry_ptr rlep;
        check_assertion(dip_next == NULL && !var->is_thread_local);
        /* Create an association between the variable being initialized and
           the initialization routine. */
        var->init_routine.dynamic_init_routine = init_rout;
        /* Queue this routine on a list of initialization routines. */
        rlep = alloc_list_entry_for_routine();
        rlep->routine = init_rout;
#if !USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES
        if (var->is_thread_local) {
          /* Queue this routine on the list of thread_local routines. */
          if (thread_local_dynamic_init_routines_tail == NULL) {
            il_header.thread_local_dynamic_init_routines = rlep;
          } else {
            thread_local_dynamic_init_routines_tail->next = rlep;
          }  /* if */
          thread_local_dynamic_init_routines_tail = rlep;
        } else
#endif /* !USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES */
        /* Do not insert code here. */
        {
          /* Queue this routine on the list of file-scope init routines */
          if (file_scope_dynamic_init_routines_tail == NULL) {
            il_header.file_scope_dynamic_init_routines = rlep;
          } else {
            file_scope_dynamic_init_routines_tail->next = rlep;
          }  /* if */
          file_scope_dynamic_init_routines_tail = rlep;
        }  /* if */
      }  /* if */
#endif /* SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */
#if USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES
      if (var->is_thread_local &&
          var->source_corresp.name_linkage != (a_name_linkage_kind)nlk_none &&
          (var->storage_class == (a_storage_class)sc_unspecified ||
           var->storage_class == (a_storage_class)sc_extern)) {
        /* If a dynamically-initialized thread_local variable is defined in
           this translation unit, make sure that the initialization routine for
           the variable is created and marked as needed (the variable may not
           be used in this translation unit and if it is used in another
           translation unit, it'll invoke this alias). */
        if (var->init_routine.thread.init_routine == NULL) {
          (void)thread_local_init_routine_for_variable(var);
        } else {
          /* In the case of template static data members, it's possible that
             the static data member's storage class has changed since the init
             routine was created (i.e., it may be sc_extern during lowering of
             the routine, but later the static data member has since been
             instantiated, changing the storage class to sc_unspecified).
             Reflect that potential change in the storage class of the init
             routine. */
          var->init_routine.thread.init_routine->storage_class =
                                                            var->storage_class;
        }  /* if */
#if ONE_INSTANTIATION_PER_OBJECT
        var->init_routine.thread.init_routine->
                    instantiation_needed_bit_number =
                                          var->instantiation_needed_bit_number;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if MAINTAIN_NEEDED_FLAGS
        set_routine_definition_needed(var->init_routine.thread.init_routine);
#endif /* MAINTAIN_NEEDED_FLAGS */
      }  /* if */
#endif /* USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES */
    }  /* for */
    if (do_thread_local) {
#if USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES
      check_assertion(!do_single_init);
      /* If we're creating the thread_local initialization routine for the
         translation unit, add a test to ensure the initialization is performed
         only once per thread. */
      add_guard_code_to_thread_local_init(scope);
#endif /* USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES */
    }  /* if */
    pop_generated_routine_context(scope, region_number, &grcontext);
    processing_file_scope_init_routine = FALSE;
    if (do_thread_local) {
      /* Mark this routine as needed because it is only accessed through
         aliases. */
#if ONE_INSTANTIATION_PER_OBJECT
      init_rout->instantiation_needed_bit_number = needed_bit_number;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if MAINTAIN_NEEDED_FLAGS
      set_routine_definition_needed(init_rout);
#endif /* MAINTAIN_NEEDED_FLAGS */
    }  /* if */
    /* Generate code to ensure that the initialization routine is called
       at program startup.  If a .init section will be used for
       initialization, skip this stuff. */
#if USE_PATCH_INIT_STARTUP
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
 #error -- GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED cannot be TRUE if \
           USE_PATCH_INIT_STARTUP is TRUE
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
    make_code_to_invoke_file_scope_init_routine(init_rout);
#endif /* USE_PATCH_INIT_STARTUP */
  }  /* if */
  if (processing_partial_list) {
    /* Put the not-processed initializations back on the list. */
    file_scope->dynamic_inits = delay_list;
    if (file_scope->lifetime != NULL) {
      check_assertion(file_scope->lifetime->destructions == NULL);
      file_scope->lifetime->destructions = dtor_delay_list;
    }  /* if */
  }  /* if */
}  /* b_lower_file_scope_dynamic_inits */


static void s_lower_file_scope_dynamic_inits(unsigned long needed_bit_number,
                                             int           init_priority,
                                             a_boolean     do_thread_local)
/*
This routine is called to lower file-scope dynamic initializations that
match the needed_bit_number (in one-instantiation-per-object mode) and/or
init_priority (when GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED is TRUE).
Only consider thread_local initializations when do_thread_local is TRUE.  In
typical configurations (where SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
is FALSE), a single initialization routine will be generated that will
initialize all matching dynamic initializations.  When
SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS is TRUE, each matching
dynamic initialization is lowered into its own initialization routine.
*/
{
#if SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
  a_boolean more_matching_inits;

  do {
    b_lower_file_scope_dynamic_inits(needed_bit_number, init_priority,
                                     /*do_single_init=*/TRUE,
                                     do_thread_local,
                                     &more_matching_inits);
  } while (more_matching_inits);
#else /* !SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */
  b_lower_file_scope_dynamic_inits(needed_bit_number, init_priority,
                                   /*do_single_init=*/FALSE,
                                   do_thread_local,
                                   (a_boolean*)NULL);
#endif /* SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */
}  /* s_lower_file_scope_dynamic_inits */

#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED

static int first_init_priority(unsigned long needed_bit_number)
/*
Return the first non-zero GNU init_priority value in the list of file-scope
dynamic initializations.  If there is no non-zero value, return zero.
If one-instantiation-per-object mode is enabled, ignore entries whose
needed bit number does not match needed_bit_number.
*/
{
  int                first_priority = 0;
  a_dynamic_init_ptr dip;
  unsigned long      eff_needed_bit_number = needed_bit_number;

  if (needed_bit_number == 1) eff_needed_bit_number = 0;
  /* init_priority is enabled only in g++ mode. */
  if (gpp_mode) {
    for (dip = il_header.primary_scope->dynamic_inits;
         dip != NULL;
         dip = dip->next) {
#if ONE_INSTANTIATION_PER_OBJECT
      if (one_instantiation_per_object &&
          dip->variable->instantiation_needed_bit_number !=
                                               eff_needed_bit_number) continue;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
      if (dip->variable->init_priority != 0) {
        first_priority = dip->variable->init_priority;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return first_priority;
}  /* first_init_priority */

#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */

#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED || ONE_INSTANTIATION_PER_OBJECT

static void p_lower_file_scope_dynamic_inits(unsigned long needed_bit_number,
                                             a_boolean     do_thread_local)
/*
Wrapper around s_lower_file_scope_dynamic_inits.  When the GNU init_priority
attribute is allowed, loop through the initializations and call
s_lower_file_scope_dynamic_inits to generate a routine for each priority
level.  Only process thread_local initializations when do_thread_local is TRUE.
*/
{
  int priority = 0;

#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
  /* Note that the last time around processes priority == 0, the
     variables with no init_priority attribute.  Note also that that
     call will be more efficient in the case that needed_bit_number
     is 0, because it will just process everything on the list. */
  do {
    priority = first_init_priority(needed_bit_number);
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
    s_lower_file_scope_dynamic_inits(needed_bit_number, priority,
                                     do_thread_local);
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
  } while (priority != 0);
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
}  /* p_lower_file_scope_dynamic_inits */

#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED || ONE_INSTANTIATION_PER_OBJECT*/

void lower_file_scope_dynamic_inits(void)
/*
Do lowering on the file-scope dynamic initializations list.  Also insert
code to cause the generated initialization routine to be called at startup.
*/
{
  a_scope_ptr file_scope = il_header.primary_scope;

#if IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS
  if (std_thread_local_storage_specifier_enabled &&
      !one_instantiation_per_object) {
#if USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES || \
    !SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
    /* Create a single routine for all thread_local initialization in this
       translation unit. */
    b_lower_file_scope_dynamic_inits((unsigned long)0, 0,
                                     /*do_single_init=*/FALSE,
                                     /*do_thread_local=*/TRUE,
                                     (a_boolean*)NULL);
#else /* !USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES || ... */
    /* If we're not lowering thread_local uses, create an initialization
       routine for each thread_local dynamic initialization and let the
       back end call them as needed. */
    a_boolean more_matching_inits;
    do {
      b_lower_file_scope_dynamic_inits((unsigned long)0, 0,
                                       /*do_single_init=*/TRUE,
                                       /*do_thread_local=*/TRUE,
                                       &more_matching_inits);
    } while (more_matching_inits);
#endif /* USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES || ... */
  }  /* if */
#endif /* IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS */
#if ONE_INSTANTIATION_PER_OBJECT
  if (one_instantiation_per_object) {
    /* When generating one instantiation per object, each instantiation gets
       its own initialization file. */
    /* Each instantiation has an associated bit number.  The bit numbers
       are assigned in increments of 2, to leave room for a class
       definition needed bit associated with each instantiation. */
    unsigned long      needed_bit_number;
    for (needed_bit_number = 1;
         needed_bit_number <
                 (il_header.number_of_external_nonclass_template_entities+1)*2;
         needed_bit_number += 2) {
#if IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS
      if (std_thread_local_storage_specifier_enabled) {
        /* Do any thread_local initializations for this slice separately. */
        p_lower_file_scope_dynamic_inits(needed_bit_number,
                                         /*do_thread_local=*/TRUE);
      }  /* if */
#endif /* IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS */
      p_lower_file_scope_dynamic_inits(needed_bit_number,
                                       /*do_thread_local=*/FALSE);
    }  /* for */
    check_assertion_str(file_scope->dynamic_inits == NULL,
                    "lower_file_scope_dynamic_inits: not all entries lowered");
    if (file_scope->lifetime != NULL) {
      /* Restore any residual destructions left after lowering. */
      check_assertion_str(file_scope->lifetime->destructions == NULL,
                       "lower_file_scope_dynamic_inits: non-NULL destrs list");
    }  /* if */
  } else
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  /* Do not insert code here. */
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
  /* init_priority is enabled only in g++ mode. */
  if (gpp_mode) {
    /* If the GNU init_priority attribute is supported, make multiple
       passes through the list to generate separate routines for each
       priority value. */
    p_lower_file_scope_dynamic_inits((unsigned long)0,
                                     /*do_thread_local=*/FALSE);
  } else
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
  /* Do not insert code here; this is the "else" of an "if". */
  {
    s_lower_file_scope_dynamic_inits((unsigned long)0, 0,
                                     /*do_thread_local=*/FALSE);
    file_scope->dynamic_inits = NULL;
  }
}  /* lower_file_scope_dynamic_inits */

#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN

void add_body_for_wrapper_routine(a_routine_ptr routine)
/*
Add a definition to the indicated function, which is an entry/wrapper
used to call an overriding virtual function that has a covariant
return type, or a thunk in the IA-64 ABI.  The body is a return of
an enk_result_of_overriding_function cast to the proper base class.
The overriding function must have a definition in the current compilation.
dump_routine_definition makes assumptions about the IL generated here and may
need to be modified if changes are made here.
*/
{
  a_scope_ptr            scope;
  a_memory_region_number region_number;
  a_generated_routine_context
                         grcontext;
  a_statement_ptr        return_stmt;
  an_expr_node_ptr       expr, roof_expr;
  a_param_type_ptr       ptp, this_ptp;
  a_variable_ptr         param_var, last_param_var;
  a_type_ptr             routine_type = skip_typerefs(routine->type);
  a_routine_ptr          overriding_function, overridden_function;
  a_base_class_ptr       bcp;
  a_type_ptr             overriding_return_type, overridden_return_type;
#if IA64_ABI
  a_variable_ptr         this_param = NULL;
#endif /* IA64_ABI */
  an_insert_location     insert_location;
  a_variable_ptr         temp_var = NULL;

  /* The routine type must be already lowered so that, among other things,
     the implicit "this" parameter is already in the parameter type list. */
  check_assertion(visited_yet(routine_type));
  /* Make the basic definition (memory_region, scope, top-level block). */
  scope = make_routine_definition(routine, /*make_return=*/TRUE,
                                  &region_number);
  push_generated_routine_context(scope, region_number, &grcontext);
  this_ptp = param_type_for_this(routine_type);
  /* Add parameter variables. */
  last_param_var = NULL;
  for (ptp = routine->type->variant.routine.extra_info->param_type_list;
       ptp != NULL;
       ptp = ptp->next) {
    a_type_qualifier_set qualifiers = ptp->qualifiers;
#if IA64_ABI
    if (ptp == this_ptp) {
      /* This is the "this" parameter. */
      if (routine->delta != 0 || routine->vcall_index != 0) {
        /* We will be modifying the "this" pointer so it cannot be const. */
        qualifiers &= ~TQ_CONST;
      }  /* if */
    }  /* if */
#endif /* IA64_ABI */
    param_var = make_lowered_param_variable(make_qualified_type(ptp->type,
                                                                qualifiers));
    if (ptp == this_ptp) {
#if IA64_ABI
      this_param = param_var;
#endif /* IA64_ABI */
      param_var->is_this_parameter = TRUE;
    }  /* if */
    if (last_param_var == NULL) {
      scope->variant.routine.parameters = param_var;
    } else {
      last_param_var->next = param_var;
    }  /* if */
    last_param_var = param_var;
    param_var->next = NULL;
  }  /* for */
  overriding_function = routine->overriding_function_for_wrapper;
  overridden_function = routine->overridden_function_for_wrapper;
  overriding_return_type = lowered_return_type_of(overriding_function->type);
  overridden_return_type = lowered_return_type_of(overridden_function->type);
  /* The overriding function must have a definition in this compilation. */
  check_assertion(overriding_function->assoc_scope != NULL_region_number &&
                  !overriding_function->suppress_inline_body);
  /* Make an expression that is an enk_result_of_overriding_function cast
     to the right pointer type. */
  roof_expr = alloc_expr_node(
                         (an_expr_node_kind)enk_result_of_overriding_function);
  roof_expr->type = overriding_return_type;
#if IA64_ABI
  if (is_void_type(overriding_return_type)) {
    /* No need for a temporary. */
    expr = roof_expr;
  } else
#endif /* IA64_ABI */
  /* Do not insert code here. */
  {
    /* Strictly speaking, we don't need a temporary here, but using a
       temporary makes it easier for the C generating back end to replace
       the enk_result_of_overriding_function with an "inline" version of
       the overriding function.  The assignment to the temporary is performed
       later (after the "this" adjustment, if any). */
    temp_var = make_lowered_temporary(roof_expr->type);
    expr = var_rvalue_expr(temp_var);
  }  /* if */
#if IA64_ABI
  if (is_ptr_or_ref_type(overriding_return_type) && 
      is_class_struct_union_type(type_pointed_to(overriding_return_type)) &&
      !identical_types(overriding_return_type, overridden_return_type)) {
#endif /* IA64_ABI */
    bcp = find_base_class_of_full(type_pointed_to(overriding_return_type),
                                  type_pointed_to(overridden_return_type),
                                  /*instantiate_if_necessary=*/FALSE);
    if (bcp != NULL) {
      /* The return types point to a derived/base pair and not just types
         that differ by cv-qualification. */
      a_boolean error_detected;
      add_base_class_casts(bcp, type_pointed_to(overridden_return_type),
                           /*check_cast_access=*/FALSE,
                           /*check_ambiguity=*/FALSE,
                           /*is_implicit_cast=*/TRUE,
                           /*implicit_in_naming=*/FALSE,
                           &expr,
                           &overriding_function->source_corresp.decl_position,
                           &error_detected);
      check_assertion(!error_detected);
    } else {
      /* The return types point to the same class, so a regular cast is
         needed to handle the difference in cv-qualification. */
      expr = add_cast(expr, overridden_return_type);
    }  /* if */
#if IA64_ABI
  }  /* if */
#endif /* IA64_ABI */
  lower_expr(expr);
  /* Put the expression into the return statement in the body. */
  check_assertion(scope->assoc_block->kind == (a_statement_kind)stmk_block);
  return_stmt = scope->assoc_block->variant.block.statements;
  check_assertion(return_stmt != NULL &&
                  return_stmt->kind == (a_statement_kind)stmk_return);
#if IA64_ABI
  if (is_void_type(overriding_return_type)) {
    /* For a void thunk, make the enk_result_of_overriding_function a
       separate expression statement preceding the return. */
    set_block_start_insert_location(scope->assoc_block, &insert_location);
    (void)insert_expr_statement(expr, &insert_location);
    expr = NULL;
  }  /* if */
#endif /* IA64_ABI */
  return_stmt->expr = expr;
  set_block_start_insert_location(scope->assoc_block, &insert_location);
#if IA64_ABI
  if (overriding_function->use_comdat) {
    put_routine_into_comdat_group(routine);
  }  /* if */
  /* If necessary, adjust the "this" pointer.  Do this after handling the
     return statement because the logic above assumes that the return
     statement is the first thing in the block. */
  if (routine->delta != 0 || routine->vcall_index != 0) {
    an_expr_node_ptr   this_adjustment = NULL, delta_expr, vcall_expr;
    an_expr_node_ptr   index_expr, this_expr;
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
      this_adjustment = make_var_assignment_expr(this_param, this_adjustment);
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
                                    make_pointer_type(pointer_to_vtbl_type()));
      /* Dereference to get a pointer to the virtual function table. */
      vcall_expr = add_indirection_to_node(vcall_expr);
      vcall_expr = rvalue_expr_for_lvalue(vcall_expr);
      /* Add the vcall index to find the vcall offset. */
      index_expr = node_for_integer_constant((long)routine->vcall_index,
                                             targ_ptrdiff_t_int_kind);
      vcall_expr->next = index_expr;
      vcall_expr = make_operator_node((an_expr_operator_kind)eok_padd,
                                      vcall_expr->type,
                                      vcall_expr);
      /* Dereference to get the offset. */
      vcall_expr = add_indirection_to_node(vcall_expr);
      vcall_expr = rvalue_expr_for_lvalue(vcall_expr);
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
      vcall_expr = make_var_assignment_expr(this_param, vcall_expr);
      /* If there was already a delta adjustment, combine the two. */
      if (this_adjustment != NULL) {
        this_adjustment = make_comma_node(this_adjustment, vcall_expr);
      } else {
        this_adjustment = vcall_expr;
      }  /* if */
    }  /* if */
    /* Insert the statement. */
    (void)insert_expr_statement(this_adjustment, &insert_location);
  }  /* if */
#endif /* IA64_ABI */
  if (temp_var != NULL) {
    /* Insert the assignment to the temporary variable. */
    (void)insert_var_assignment_statement(temp_var, roof_expr,
                                          &insert_location);
  }  /* if */
  pop_generated_routine_context(scope, region_number, &grcontext);
#if MAINTAIN_NEEDED_FLAGS
  /* If this is an extern inline thunk, and we're instantiating extern
     inlines, mark the routine as needed.  We only get here if the
     caller has determined that the overriding function is needed,
     so the thunk is also needed (they always go out together).
     External routines other than extern inline were marked as needed
     in pop_generated_routine_context. */
  if (instantiate_extern_inline && treat_as_extern_inline(routine)) {
    mark_as_needed((char *)routine, iek_routine);
  }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
}  /* add_body_for_wrapper_routine */

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
    guid_type = make_lowered_class_type((a_type_kind)tk_struct);
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


static a_constant_ptr conv_uuid_constant(a_const_char    **ptr,
                                         int             ndigits,
                                         an_integer_kind ikind)
/*
Convert ndigits hexadecimal digits of the uuid string at *ptr, and increment
*ptr by ndigits.  Put the converted digits into an integer constant with
kind ikind, allocate an unshared copy, and return a pointer to the
allocated integer constant.
*/
{
  a_const_char     *local_ptr = *ptr;
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
    set_unsigned_integer_value(&digit,
                            (a_host_large_unsigned)intdigit /*lint --e(571)*/);
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
  a_const_char                *uuid_string;

  if (type != NULL) {
    if (is_immediate_class_type(type)) {
      p_uuid_var = &class_type_supp(type)->uuid_variable;
      uuid_string = class_type_supp(type)->uuid_string;
    } else if (is_immediate_enum_type(type)) {
      p_uuid_var = &integer_type_supp(type)->uuid_variable;
      uuid_string = integer_type_supp(type)->uuid_string;
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
    a_const_char   *ptr = uuid_string;
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

void lower_lambda(an_expr_node_ptr expr)
/*
Lower the specified lambda expression by creating a temporary variable with
the same type as the closure class and initializing each of the fields
with the value of their corresponding captured variables.
*/
{
  a_variable_ptr         closure_var;
  a_lambda_capture_ptr   capture;
  an_init_pos_descr      ipd;
  an_insert_location     insert_location;
  an_implied_copy_source source_desc;
  a_dynamic_init_ptr     dip;

  check_assertion(expr->kind == (an_expr_node_kind)enk_lambda &&
                  identical_types(expr->variant.lambda.ptr->closure_class,
                                  expr->type));
  capture = expr->variant.lambda.ptr->capture_list;
  dip = expr->variant.lambda.initialization;
  if (dip->has_temporary_lifetime && !dip->static_temp) {
    if (long_lifetime_temps) {
      /* Make a temporary that lasts longer than the full expression. */
      closure_var = make_lowered_temporary(expr->type);
    } else {
      /* Simple case; a temporary that lasts until the end of the full
         expression will do. */
      closure_var = make_local_temporary(expr->type);
    }  /* if */
  } else {
    /* Create a static temporary for the closure object. */
    closure_var = make_temporary_in_scope(expr->type,
                                          (a_scope_ptr)NULL,
                                          (a_boolean)dip->static_temp,
                                          /*promote_if_necessary=*/TRUE);
  }  /* if */
  /* Change the enk_lambda node to an enk_variable node that refers to
     the closure variable.  The type and lvalueness of the node are unchanged.
     Lambda-specific field values of expr cannot be accessed after the
     expression kind is changed.  This is done early because code might be
     inserted during lowering of capture initializations. */
  set_expr_node_kind(expr, (an_expr_node_kind)enk_variable);
  expr->variant.variable = closure_var;
  /* Initialization (if any) is inserted before the lambda expression. */
  set_expr_insert_location(expr, &insert_location);
  /* Set the variable for the initialization to point to the temporary. */
  set_var_init_pos_descr(closure_var, &ipd);
  /* Set the source of the implied copy to the first captured local variable
     in the capture list.  During the initialization,
     advance_to_next_lambda_capture_if_necessary is called to advance the
     source of the implied copy to the next local variable in the capture
     list. */
  clear_implied_copy_source(&source_desc);
  source_desc.capture = capture;
  /* The front end has created an aggregate dynamic init to initialize all
     fields of the lambda closure object with values from the corresponding
     local variables. */
  lower_dynamic_init(dip, &ipd,
                     &source_desc,
                     (a_variable_ptr)NULL,
                     LDIO_NONE,
                     /*others_follow_in_aggr=*/FALSE,
                     &insert_location, (a_boolean *)NULL,
                     (a_constant **)NULL);
  /* Verify that all captured variables were assigned during the
     initialization. */
  check_assertion(source_desc.capture == NULL);
}  /* lower_lambda */

#if LOWER_IFUNC

void lower_ifunc_routine(a_routine_ptr routine)
/*
Lower an ifunc routine by turning it into a "wrapper" routine that
invokes the resolver routine, saves it's value and then invokes the
resolved routine and returns it's value (if non-void).  For example:

  extern int (*resolved_f)();  // pointer to "resolved" f for the target
  int f(args...) {
    if (resolved_f == f) {
      resolved_f = resolver(); // one time invocation of resolver
    }
    return (*resolved_f)(args...);  // value returned only for non-void "f"
  }
  int (*resolved_f)() = f;     // initialized to wrapper function

Lowering also re-writes calls to this routine to be indirect through the
resolver variable (so the overhead of doing the resolving is only
incurred once).

Note: this is called when lowering C and C++.
*/
{
  a_generated_routine_context grcontext;
  a_statement_ptr  return_stmt;
  a_variable_ptr   resolver_var;
  a_type_ptr       routine_type = routine->type;
  an_expr_node_ptr arg_list = NULL, end_arg_list = NULL, assign_node;
  an_expr_node_ptr call_node, call_args, test_node, pass_through_arg;
  a_scope_ptr      scope;
  a_param_type_ptr first_actual_param_type;
  a_param_type_ptr param_type;
  an_insert_location
                   insert_location, then_insert_location;
  a_memory_region_number
                   il_region;
  a_routine_type_supplement_ptr
                   rtsp = routine_type->variant.routine.extra_info;
  a_variable_ptr   param_var, last_param_var;
  a_constant_ptr   function_constant;
  a_routine_ptr    resolver = routine->aliased_routine;

  check_assertion(routine->is_ifunc && routine->aliased_routine != NULL);
  /* Make a memory region, scope, and block for the routine definition. */
  scope = make_routine_definition(routine,
                                  /*make_return=*/FALSE,
                                  &il_region);
  set_block_start_insert_location(scope->assoc_block, &insert_location);
  push_generated_routine_context(scope, il_region, &grcontext);
  /* Create the parameters for this function, and while doing that,
     create a list of the arguments (to be used when calling the
     eventual target function).  Make sure the type has been lowered first. */
  check_assertion(visited_yet(routine_type) || C_mode());
  last_param_var = NULL;
  first_actual_param_type = rtsp->param_type_list;
  for (param_type = first_actual_param_type;
       param_type != NULL;
       param_type = param_type->next) {
    param_var = make_lowered_param_variable(param_type->type);
    param_var->assoc_param_type = param_type;
    if (last_param_var == NULL) {
      scope->variant.routine.parameters = param_var;
    } else {
      last_param_var->next = param_var;
    }  /* if */
    /* Add a reference to the parameter to the argument list to be used
       to call the function. */
    pass_through_arg = var_rvalue_expr(param_var);
    if (arg_list == NULL) {
      arg_list = pass_through_arg;
    } else {
      end_arg_list->next = pass_through_arg;
    }  /* if */
    end_arg_list = pass_through_arg;
    last_param_var = param_var;
  }  /* for */
  resolver_var = make_ifunc_resolver_var(routine);
  /* Create "if (resolver_var == f) {}" to prevent the need to determine
     the resolver function more than once (since all calls to this routine
     are supposed to be through the resolver variable, that should happen
     automatically, but in some cases, e.g., pointer-to-member constants,
     the resolver may be called multiple times). */
  test_node = var_rvalue_expr(resolver_var);
  function_constant = alloc_constant((a_constant_repr_kind)ck_address);
  set_routine_address_constant(routine, function_constant,
                               /*set_address_taken_flag=*/TRUE);
  test_node->next = alloc_node_for_allocated_constant(function_constant);
  test_node = make_operator_node((an_expr_operator_kind)eok_eq,
                                 integer_type((an_integer_kind)ik_int),
                                 test_node);
  test_node = boolean_controlling_expr(test_node);
  insert_if_statement(test_node, /*is_initialization_guard=*/FALSE,
                      &insert_location, (a_statement_ptr *)NULL,
                      &then_insert_location, (an_insert_location *)NULL);
  /* Insert code to call the resolver and store its value in resolver_var
     (i.e., "resolver_var = (decltype(resolver_var))resolver()"). */
  assign_node = make_call_node(resolver, (an_expr_node_ptr)NULL);
  assign_node = add_cast(assign_node, resolver_var->type);
  (void)insert_var_assignment_statement(resolver_var, assign_node,
                                        &then_insert_location);
  /* Make a call node that calls through *resolver_var. */
  call_args = var_rvalue_expr(resolver_var);
  call_args->next = arg_list;
  call_node = make_operator_node((an_expr_operator_kind)eok_call,
                                 lowered_return_type_of(routine_type),
                                 call_args);
  /* If the routine has a void type, insert a statement for the call
     followed by a return statement.  Otherwise, attach the call directly
     to the return. */
  if (is_void_type(lowered_return_type_of(routine_type))) {
    /* The call will be inserted as a separate statement. */
    (void)insert_expr_statement(call_node, &insert_location);
    call_node = NULL;
  }  /* if */
  /* Add the return statement. */
  return_stmt = alloc_statement((a_statement_kind)stmk_return);
  return_stmt->expr = call_node;
  insert_statement(return_stmt, &insert_location);
  add_to_return_memo_list(return_stmt);
  routine->compiler_generated = TRUE;
  pop_generated_routine_context(scope, il_region, &grcontext);
  return;
}  /* lower_ifunc_routine */

#endif /* LOWER_IFUNC */

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
      pch_saved_var_array_elem(memcpy_routine),
      pch_saved_var_array_elem(memzero_routine),
      pch_saved_var_array_elem(record_needed_destruction_routine),
      pch_saved_var_array_elem(record_needed_thread_destruction_routine),
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
#if USE_PATCH_INIT_STARTUP
      pch_saved_var_array_elem(linkl_type),
#endif /* USE_PATCH_INIT_STARTUP */
#if MICROSOFT_EXTENSIONS_ALLOWED
      pch_saved_var_array_elem(guid_type),
      pch_saved_var_array_elem(guid_array_type),
      pch_saved_var_array_elem(null_guid_variable),
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      pch_saved_var_array_elem(ctor_ptr_type),
      pch_saved_var_array_elem(dtor_ptr_type),
      pch_saved_var_array_elem(cctor_ptr_type),
      pch_saved_var_array_elem(new_routine_ptr_type),
      pch_saved_var_array_elem(delete_routine_ptr_type),
      pch_saved_var_array_terminating_elem(),
#if RUNTIME_SUPPORTS_ARRAY_LENGTH_CHECK && ABI_COMPATIBILITY_VERSION >= 406
      pch_saved_var_array_elem(throw_bad_array_new_length_routine)
#endif /* RUNTIME_SUPPORTS_ARRAY_LENGTH_CHECK && ABI_COMPATIBILITY_VERSION...*/
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
  register_trans_unit_variable(memcpy_routine);
  register_trans_unit_variable(memzero_routine);
  register_trans_unit_variable(record_needed_destruction_routine);
  register_trans_unit_variable(record_needed_thread_destruction_routine);
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
#if USE_PATCH_INIT_STARTUP
  register_trans_unit_variable(linkl_type);
#endif /* USE_PATCH_INIT_STARTUP */
#if MICROSOFT_EXTENSIONS_ALLOWED
  register_trans_unit_variable(guid_type);
  register_trans_unit_variable(guid_array_type);
  register_trans_unit_variable(null_guid_variable);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  register_trans_unit_variable(ctor_ptr_type);
  register_trans_unit_variable(dtor_ptr_type);
  register_trans_unit_variable(cctor_ptr_type);
  register_trans_unit_variable(new_routine_ptr_type);
  register_trans_unit_variable(delete_routine_ptr_type);
#if RUNTIME_SUPPORTS_ARRAY_LENGTH_CHECK && ABI_COMPATIBILITY_VERSION >= 406
  register_trans_unit_variable(throw_bad_array_new_length_routine);
#endif /* RUNTIME_SUPPORTS_ARRAY_LENGTH_CHECK && ABI_COMPATIBILITY_VERSION...*/
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
  memcpy_routine = NULL;
  memzero_routine = NULL;
  record_needed_destruction_routine = NULL;
  record_needed_thread_destruction_routine = NULL;
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
#if USE_PATCH_INIT_STARTUP
  linkl_type = NULL;
#endif /* USE_PATCH_INIT_STARTUP */
#if MICROSOFT_EXTENSIONS_ALLOWED
  guid_type = NULL;
  guid_array_type = NULL;
  null_guid_variable = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  ctor_ptr_type = NULL;
  dtor_ptr_type = NULL;
  cctor_ptr_type = NULL;
  new_routine_ptr_type = NULL;
#if RUNTIME_SUPPORTS_ARRAY_LENGTH_CHECK && ABI_COMPATIBILITY_VERSION >= 406
  throw_bad_array_new_length_routine = NULL;
#endif /* RUNTIME_SUPPORTS_ARRAY_LENGTH_CHECK && ABI_COMPATIBILITY_VERSION...*/
  delete_routine_ptr_type = NULL;
}  /* init_lower_trans_unit_init */


void init_lower_init(void)
/*
Initialize static variables related to this file that must be initialized
for each compilation.
*/
{
  processing_file_scope_init_routine = FALSE;
#if SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
  file_scope_dynamic_init_routines_tail = NULL;
#if !USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES
  thread_local_dynamic_init_routines_tail = NULL;
#endif /* !USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES */
#endif /* SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */
  /* init_lower_trans_unit_init is called from il_lower_trans_unit_init. */
}  /* init_lower_init */

#endif /* DO_IL_LOWERING */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2013 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
