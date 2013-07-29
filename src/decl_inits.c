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

decl_inits.c -- Scanning of initializers in declarations.

*/

/* Header files common to all files. */
#include "fe_common.h"
/* Header files used by files involved in declaration processing. */
#include "decl_hdrs.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Additional header files. */
#include "expr.h"
#include "exprutil.h"
#include "folding.h"
#include "statements.h"
#if DO_IL_LOWERING
#if MICROSOFT_EXTENSIONS_ALLOWED && LOWER_MICROSOFT_NONCONSTANT_AGGREGATE
#include "lower_init.h"
#include "lower_c99.h"
#endif /* MICROSOFT_EXTENSIONS_ALLOWED && ... */
#endif /* DO_IL_LOWERING */


#define array_element_count(array_type, elem_type)                      \
  ((array_type)->size == 0 ? 1 : (array_type)->size / (elem_type)->size)


#if !MICROSOFT_EXTENSIONS_ALLOWED
/*ARGSUSED*/  /* <-- init_info is not used in that case. */
#endif /* !MICROSOFT_EXTENSIONS_ALLOWED */
static void set_initialized_array_size(a_type_ptr    *type,
                                       a_targ_size_t size,
                                       a_boolean     unknown_dependent)
/*
Change *type to point to a new array type that is the same as the current
*type except that its number of elements is changed to "size".  If
unknown_dependent is TRUE, the size is dependent but unknown (size is
ignored).  This is used to set the length of an incomplete array when
its size becomes known because it is initialized.  The original type
may be shared, and therefore a copy is made and modified.
*/
{
  a_type_ptr array_type, incomplete_type = skip_typerefs(*type);

  check_assertion(!has_unknown_specified_bound(incomplete_type));
  array_type = alloc_type((a_type_kind)tk_array);
  copy_type(incomplete_type, array_type);
  if (unknown_dependent) {
    array_type->variant.array.is_template_dependent_size_array = TRUE;
    array_type->variant.array.variant.element_count_constant = NULL;
  } else {
    array_type->variant.array.variant.number_of_elements = size;
    if (size == 0) {
      /* In GNU C and C++ mode, an empty pair of braces can be a valid
         initializer for a zero-length array.  In C++11, this is also
         possible with something like "new T[n]{}". */
      array_type->variant.array.bound_is_zero = TRUE;
    }  /* if */
  }  /* if */
  set_type_size(array_type);
  *type = array_type;
}  /* set_initialized_array_size */


a_boolean check_string_constant_initializer_full(a_type_ptr      *dst_type,
                                                 a_constant_ptr  string_con,
                                                 a_boolean       *excess)
/*
*dst_type is an array of narrow or wide characters (or an array whose element
type is template dependent).  Return TRUE if and only if a variable or field of
that type can be initialized with the given string literal.  If excess is non-
NULL, an overlong string literal is not treated as an error but causes *excess
to be set to TRUE.  If the string literal is not too long (which includes the
standard C behavior of trimming the terminating null character if needed),
*excess is set to FALSE.  If necessary, the string literal is truncated to fit
*dst_type or *dst_type may be modified (e.g., to set the length of the string).
*/
{
  a_type_ptr     array_type;
  a_character_kind
                 char_kind = string_con->character_kind;
  a_targ_size_t  char_size = character_size[char_kind];
  a_targ_size_t  num_elems, array_length;
  a_boolean      is_template_dependent = is_template_dependent_type(*dst_type);
  a_boolean      err = FALSE;

  if (excess != NULL) *excess = FALSE;
  check_assertion(string_con->kind == (a_constant_repr_kind)ck_string);
  /* The object to be initialized is an array (possibly incomplete) of char,
     wchar_t, char16_t, or char32_t -- i.e., a string or wide string.  During
     prototype instantiations, we assume that any template-dependent array
     type may end up with an appropriate type during a real instantiation. */
  check_assertion(is_string_type(*dst_type) ||
                  (is_array_type(*dst_type) && is_template_dependent));
  if (!is_template_dependent) {
    /* The constant and the array should have the same underlying character
       element type -- e.g., it's a mismatch if one is a wide string
       and the other a normal string. */
    switch (string_con->character_kind) {
      case chk_char:
        err = !is_char_array_type(*dst_type);
        break;
      case chk_wchar_t:
        err = !is_wchar_t_array_type(*dst_type);
        break;
      case chk_char16_t:
        err = !is_char16_t_array_type(*dst_type);
        break;
      case chk_char32_t:
        err = !is_char32_t_array_type(*dst_type);
        break;
      default:
        unexpected_condition();
    }  /* switch */
  }  /* if */
  if (!err) {
    /* The constant is a string with characters that are compatible with
       the array element type.  (Note that an array of characters of any
       signedness can be initialized with a string literal: ANSI C 3.5.7.) */
    num_elems = string_con->variant.string.length;
    num_elems /= char_size;
    array_type = skip_typerefs(*dst_type);
    if (is_incomplete_type(array_type)) {
      /* The array type is incomplete, and therefore the array size
         is set from the string length. */
      set_initialized_array_size(&array_type, num_elems,
                                 /*unknown_dependent=*/FALSE);
      *dst_type = array_type;
    } else if (array_type->variant.array.is_template_dependent_size_array) {
      /* This should only happen during prototype instantiations where the
         array length is a template parameter dependent constant. */
    } else {
      /* The object being initialized is an array that has a definite
         size.  See if the string will fit in the array. */
      check_assertion(!has_unknown_specified_bound(array_type));
      array_length = array_type->variant.array.variant.number_of_elements;
      if (num_elems > array_length) {
        /* The string is longer than the array.  Check to see if the
           string will fit if we drop the final null.  See 3.5.7.  In C++
           the truncation of the final null is not supported (ARM 8.4.2). */
        if (C_mode() && num_elems-1 == array_length) {
          /* In C modes, if the string literal would fit without the final
             null character, that character is just dropped. */
        } else if (excess != NULL) {
          /* The caller indicated that (nonstandard) excess characters should
             not be treated as an error.  Record in *excess that excess
             characters were seen. */
          *excess = TRUE;
        } else {
          /* The initializer string is too long for the array being
             initialized. */
          err = TRUE;
        }  /* if */
        /* Truncate the string literal to fit the destination type. */
        string_con->type = string_literal_type(char_kind, array_length);
        string_con->variant.string.length = array_length*char_size;
      }  /* if */
    }  /* if */
  }  /* if */
  return !err;
}  /* check_string_constant_initializer_full */


static void copy_ctor_default_args_to_dynamic_init(a_dynamic_init_ptr  dip)
/*
dip points to a dik_constructor dynamic init entry.  Make a copy of the
default arguments from the routine type of the constructor, updating
the dynamic init entry.
*/
{
  a_routine_ptr           rp;
  a_param_type_ptr        ptp;
  an_object_lifetime_ptr  expr_temp_lifetime = NULL;

  rp = dip->variant.constructor.ptr;
  ptp = skip_typerefs(rp->type)->variant.routine.extra_info->param_type_list;
  if (dip->variant.constructor.is_copy_constructor_with_implied_source) {
    check_assertion(ptp != NULL);
    ptp = ptp->next;
  }  /* if */
  if (ptp != NULL) {
    an_object_lifetime_ptr saved_curr_object_lifetime = curr_object_lifetime;
    if (!long_lifetime_temps) {
      /* Push an object lifetime, in case the expression requires
         generating a temporary. */
      check_assertion(curr_object_lifetime != NULL);
      if (curr_il_region_number == file_scope_region_number &&
          !in_file_scope(curr_object_lifetime)) {
        /* Switch to under the static object lifetime if we're
           putting out IL in the file scope. */
        curr_object_lifetime = il_header.primary_scope->lifetime;
      }  /* if */
      /* Don't push one if we're already inside an expr temporary lifetime. */
      if (curr_object_lifetime->kind !=
                                 (an_object_lifetime_kind)olk_expr_temporary) {
        push_object_lifetime((an_il_entry_kind)iek_none, (char *)NULL,
                             (an_object_lifetime_kind)olk_expr_temporary);
        expr_temp_lifetime = curr_object_lifetime;
      }  /* if */
    }  /* if */
    /* If there is a default argument value, or several, use them. */
    /* Copy the default-arg list. */
    dip->variant.constructor.args =
      copy_default_arg_expr_list(rp, ptp,
                                 /*inside_conditional_expression=*/FALSE,
                                 /*potentially_evaluated=*/TRUE,
                                 /*evaluated=*/TRUE);
    if (expr_temp_lifetime != NULL) {
      /* Pop the object lifetime for the temp, binding the lifetime and
         dynamic init entry if appropriate. */
      if (!is_useless_object_lifetime(expr_temp_lifetime)) {
        bind_object_lifetime(expr_temp_lifetime,
                             (an_il_entry_kind)iek_dynamic_init,
                             (char *)dip);
      }  /* if */
      (void)pop_object_lifetime();
    }  /* if */
    curr_object_lifetime = saved_curr_object_lifetime;
  }  /* if */
}  /* copy_ctor_default_args_to_dynamic_init */


static a_dynamic_init_ptr alloc_ctor_dynamic_init(a_routine_ptr ctor_rp,
                                                  a_boolean     implied_source)
/*
Allocate a dik_constructor dynamic init entry that will call the
constructor given by ctor_rp.  If the constructor has default arguments,
add the expressions for those.  If implied_source is TRUE, the source
for the (copy) constructor call will be implicit.
*/
{
  a_dynamic_init_ptr dip;

  dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constructor);
  dip->variant.constructor.ptr = ctor_rp;
  dip->variant.constructor.is_copy_constructor_with_implied_source =
                                                                implied_source;
  /* A user defined default constructor may have default args that
     should be incorporated into the constructor call. */
  copy_ctor_default_args_to_dynamic_init(dip);
  return dip;
}  /* alloc_ctor_dynamic_init */


static void gen_dynamic_initialization(
                                  a_variable_ptr        vp,
                                  a_dynamic_init_ptr    dip,
                                  a_local_static_variable_init_ptr
                                                        *local_static_var_init,
                                  a_source_position     *source_pos,
                                  a_decl_pos_block_ptr  decl_pos_block,
                                  a_statement_ptr       *p_init_stmt)
/*
Generate a dynamic initialization of the variable vp based on the
dynamic init entry pointed to by dip.  Except for a dynamic
initialization at file scope (possible only in C++), also create an
stmk_init statement at the current point in the code.  If the
variable is a local static variable, a_local_variable_init entry
will be used to initialize it, and a pointer to the entry is returned
in *local_static_var_init.  *source_pos is the source position for an
error (dynamic initialization is in unreachable code).  If p_init_stmt
is non-NULL, *p_init_stmt is set to point to the stmk_init statement
created, or NULL it there is none.  decl_pos_block is non-NULL, if the
dynamic initialization corresponds to an actual initializer in the
source; in that case, it points to position information that should be
recorded in the stmk_init statement.
*/
{
  a_statement_ptr          init_stmt;
  a_boolean                static_lifetime = FALSE;
  a_boolean                at_file_scope;
  a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];

  db_enter(4, "gen_dynamic_initialization");
  *local_static_var_init = NULL;
  if (p_init_stmt != NULL) *p_init_stmt = NULL;
  at_file_scope = (depth_innermost_function_scope == NO_SCOPE_DEPTH &&
                   !inside_local_class);
  if (!at_file_scope) {
    check_assertion(ssep->kind == (a_scope_kind)sck_function ||
                    ssep->kind == (a_scope_kind)sck_block ||
                    ssep->kind == (a_scope_kind)sck_condition ||
                    ((a_symbol_ptr)vp->source_corresp.assoc_info)->is_error);
    check_assertion(!vp->source_corresp.is_class_member ||
                    ((a_symbol_ptr)vp->source_corresp.assoc_info)->is_error);
    /* We are in executable code (i.e., inside a function or block rather
       than at file scope). */
    if (dip->kind != (a_dynamic_init_kind)dik_none) {
      /* The initialization is not just a destruction. */
      /* If the block in which the dynamic initialization is executed is
         unreachable and if no other unreachability warnings have been
         issued on the block, put out a warning now. */
      warn_if_code_is_unreachable(ec_initialization_not_reachable, source_pos);
    }  /* if */
    /* If this dynamic init appears after some executable code
       in its block, set a flag to that effect in the dynamic
       init entry (it identifies the initialization as a C++ case). */
    check_assertion_str(depth_stmt_stack >= 0,
                        "gen_dynamic_initialization: bad stmt stack depth");
    if (ssep->kind == (a_scope_kind)sck_condition ||
        struct_stmt_stack[depth_stmt_stack].any_exec_statement_seen) {
      dip->follows_an_exec_statement = TRUE;
    }  /* if */
    /* Must be the initialization of a local variable. */
    static_lifetime = has_static_storage_duration(vp->storage_class);
    check_assertion(!in_file_scope(dip));
    check_assertion(in_file_scope(vp) == static_lifetime);
    if (static_lifetime) {
      /* Dynamic initialization of a local static variable.  Since the dynamic
         init entry is in the function scope memory region, the variable can't
         have a pointer to it.  Instead, create a local-static-variable-init
         entry to point to the initializer -- it is added to a list associated
         with the current function or block scope. */
      *local_static_var_init =
                   make_local_static_variable_init(vp, (a_scope_ptr)NULL,
                                                   (an_init_kind)initk_dynamic,
                                                   (a_constant_ptr)NULL, dip);
      if (inside_statement_expression() && !C_mode()) {
        /* Dynamically-initialized local statics are not allowed inside
           GNU statement expressions.  This is because in some modes the
           initialization guard variable must be cleared when an exception
           is thrown. */
        pos_error(ec_dyn_local_static_in_statement_expr, source_pos);
      }  /* if */
    } else {
      /* Make the variable point at the dynamic initialization. */
      vp->init_kind = (an_init_kind)initk_dynamic;
      vp->initializer.dynamic = dip;
    }  /* if */
  } else {
    /* An initialization of a file-scope variable or a static data member. */
    check_assertion(in_file_scope(vp));
    check_assertion(in_file_scope(dip));
    static_lifetime = TRUE;
    /* Make the variable point at the dynamic initialization. */
    vp->init_kind = (an_init_kind)initk_dynamic;
    vp->initializer.dynamic = dip;
    /* A dynamic file-scope initialization (possible only in C++) has
       no associated stmk_init statement, so attach the dynamic initialization
       entry to the scope list.  Be careful not to insert IL that depends on
       template parameters though (unless that is configured for). */
    if (prototype_instantiations_in_il ||
        !scope_stack[depth_scope_stack].in_prototype_instantiation) {
      add_to_dynamic_inits_list(dip);
    }  /* if */
  }  /* if */
  /* The dynamic init entry should point at the variable. */
  dip->variable = vp;
  /* If needed, record the dynamic init entry on the destructions list of the
     appropriate object-lifetime entry. */
  record_end_of_lifetime_destruction(dip, static_lifetime,
                                     /*block_lifetime=*/TRUE);
  if (!at_file_scope && ssep->kind != (a_scope_kind)sck_condition) {
    /* Build the initialization statement and add it to the statement block.
       This must be done after record_end_of_lifetime_destruction is called.
       If the statement is associated with an initializer appearing in the
       source code, record the position of that initializer as the statement
       position.  Otherwise, use the position of the variable declaration.
       (In the case of anonymous union variables, extra source position
       information may not be available.) */
    a_source_position  *stmt_pos;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    a_source_position  *stmt_end_pos;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    if (decl_pos_block != NULL) {
      stmt_pos = &decl_pos_block->var_init_range.start;
#if EXTRA_SOURCE_POSITIONS_IN_IL
      stmt_end_pos = &decl_pos_block->var_init_range.end;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    } else {
      stmt_pos = &vp->source_corresp.decl_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
      if (vp->source_corresp.decl_pos_info != NULL) {
        stmt_end_pos = &vp->source_corresp.decl_pos_info->identifier_range.end;
      } else {
        stmt_end_pos = NULL;
      }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    }  /* if */
    init_stmt = add_statement_at_stmt_pos((a_statement_kind)stmk_init,
                                          stmt_pos);
#if EXTRA_SOURCE_POSITIONS_IN_IL
    if (stmt_end_pos != NULL) {
      set_stmt_source_position(init_stmt->end_position, *stmt_end_pos);
    }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    if (p_init_stmt != NULL) {
      *p_init_stmt = init_stmt;
    }  /* if */
    init_stmt->variant.dynamic_init = dip;
    update_init_statement_control_flow(init_stmt);
  }  /* if */
  /* Mark all dynamically initialized variables as referenced.  (They are
     "referenced" in the sense that a variable assigned to, even if never
     used, is referenced.)  It is especially important not to leave the
     referenced flag unset when the initialization (e.g., by constructor)
     may have side effects. */
  vp->source_corresp.referenced = TRUE;
  db_exit();
}  /* gen_dynamic_initialization */


static void put_type_back_into_variable(a_variable_ptr     vp,
                                        a_symbol_ptr       symbol_ptr,
                                        a_source_position  *source_pos,
                                        an_id_linkage_kind linkage,
                                        a_type_ptr         vp_type)
/*
Put the type vp_type back into the variable vp.  symbol_ptr points to
the symbol associated with vp; *source_pos indicates the source position
of the declaration of the variable; linkage indicates the linkage of the
variable.  vp_type has been updated as a result of initialization, i.e.,
vp had an incomplete array type that has been completed by an initializer.
*/
{
  a_symbol_ptr         ext_sym;
  a_name_linkage_kind  name_linkage;
  a_symbol_locator     locator, ext_locator;
  a_boolean            is_array = is_array_type(vp_type);

  db_enter(5, "put_type_back_into_variable");
  check_assertion(is_array_type(vp->type) && is_incomplete_type(vp->type));
  /* See if the variable has linkage. */
  if (symbol_is(symbol_ptr, sk_variable) && linkage != idl_none &&
      !is_template_dependent_context()) {
    /* The type of a variable with linkage has been adjusted because it is an
       incomplete array that has been initialized.  Check that the new type is
       compatible with other declarations of the variable.  This is necessary
       for cases like
         main () {extern char a[5];}
         char a[] = "abc";  <-- Error; int [3] is incompatible with int [5].
       However, in prototype instantiations variables with linkage aren't
       really allocated and should therefore not be unified with other
       declarations. */
    make_locator_for_symbol(symbol_ptr, &locator);
    if (!is_error_locator(locator)) {
      name_linkage = symbol_ptr->
                           variant.variable.ptr->source_corresp.name_linkage;
      ext_sym = find_external_symbol(&locator, name_linkage,
                                     (a_type_ptr)NULL, &ext_locator);
      check_assertion(ext_sym != NULL);
      (void)reconcile_external_symbol_types(ext_sym, source_pos, vp_type,
                                            es_error);
    }  /* if */
  }  /* if */
  /* An empty aggregate initializer ({}) is not valid for an array variable
     with unspecified bound, except in GNU mode (where the type of the
     initializer is complete). */
  if (is_incomplete_type(vp_type) ||
      (!gnu_mode && is_array &&
       skip_typerefs(vp_type)->variant.array.bound_is_zero)) {
    if (is_array && is_or_contains_error_type(array_element_type(vp_type))) {
      /* If something went wrong with the element type, additional errors are
         unlikely to be helpful. */
      expect_error();
    } else {
      pos_error(ec_bad_initializer_for_array_with_unspecified_bound,
                source_pos);
    }  /* if */
    vp_type = error_type();
  }  /* if */
  /* Put the updated type into the variable. */
  vp->type = vp_type;
  db_exit();
}  /* put_type_back_into_variable */


static void pop_object_lifetime_for_local_static_init(
                        an_object_lifetime_ptr           local_static_lifetime,
                        a_local_static_variable_init_ptr local_static_var_init,
                        a_boolean                        err)
/*
An object lifetime was previously pushed to surround the initialization of
a local static variable; local_static_lifetime identifies it.  Bind it
to the local static variable initializer entry at local_static_var_init,
and pop it off the object lifetime stack.  err is TRUE if some error has been
detected in the initialization.  local_static_var_init is NULL if this
variable did not require dynamic initialization, if which case the lifetime
is not needed.
*/
{
  a_boolean  suppress_warning;

  check_assertion(local_static_lifetime == curr_object_lifetime);
  if (err) mark_object_lifetime_as_useless(local_static_lifetime);
  /* Note that the lifetime is for recovery if an exception is thrown during
     the initialization of the local static variable.  So the test here is
     not for the lifetime having anything in it, but rather for whether there's
     anything in the initialization that might throw, in which case the
     lifetime is needed. */
  if ((local_static_var_init == NULL ||
       local_static_var_init->init_kind != (an_init_kind)initk_dynamic ||
       !dynamic_init_has_side_effects(local_static_var_init->
                                               initializer.dynamic,
                                      /*for_unused_var=*/FALSE,
                                      &suppress_warning)) &&
      is_useless_object_lifetime(local_static_lifetime)) {
    /* Don't bind the object lifetime, allowing it to be deleted on the pop.
       (Being bound to a local static init is one of the things that makes
       a lifetime useful, so doing the binding would prevent it from being
       removed.)  The is_useless_object_lifetime test is there for error cases
       where something destructible has been recorded even though the final
       initializer ends up having no side effects.  We keep the extra
       object lifetime in that case, just to make things easier. */
  } else {
    bind_object_lifetime(local_static_lifetime,
                         (an_il_entry_kind)
                             iek_local_static_variable_init,
                         (char *)local_static_var_init);
  }  /* if */
  (void)pop_object_lifetime();
}  /* pop_object_lifetime_for_local_static_init */


static a_constant_ptr get_default_constructed_constant(
                                                a_dynamic_init_ptr  dip,
                                                a_type_ptr          tp,
                                                a_source_position   *diag_pos)
/*
dip is a dynamic init entry representing a call to the default constructor for
the given type.  If that default constructor is constexpr and a call to it
folds to a constant, return that constant.  Otherwise, issue an error at the
given position and return an error constant.
*/
{
  a_constant_ptr  result = alloc_constant((a_constant_repr_kind)ck_error);
  a_routine_ptr   ctor = dip->variant.constructor.ptr;

  if (ctor->is_constexpr) {
    if (!fold_constexpr_ctor(dip, /*record_backing_expr=*/TRUE, diag_pos,
                             result)) {
      /* The call to the default constructor could not be folded. */
      pos_ty_error(ec_default_ctor_call_not_constant, diag_pos, tp);
      set_error_constant(result);
    }  /* if */
  } else {
    pos_ty_error(ec_default_ctor_not_constexpr, diag_pos, tp);
    set_error_constant(result);
  }  /* if */
  return result;
}  /* get_default_constructed_constant */


static a_routine_ptr get_init_destructor(a_type_ptr         tp,
                                         an_init_state      *is,
                                         a_source_position  *diag_pos)
/*
Return the destructor for the given type.  The destructor is to be called for
cleanup of an initialization described by *is (which may be tentative).  If
diagnostics are issued, they should be issued at the given position.  If
diagnostics are not issued, errors are reflected in is->init_error.
*/
{
  a_routine_ptr  dtor_rp;
  a_boolean      err = FALSE, *p_err = NULL;

  if (is->no_diagnostics) p_err = &err;
  /* No access checking is done during tentative matching for overload
     resolution (indicated by is->check_validity_only). */
  dtor_rp = select_destructor_full(tp, tp, diag_pos, /*honor_virtual=*/FALSE,
                                   /*evaluated=*/TRUE, /*instantiate=*/TRUE,
                                   /*check_access=*/!is->check_validity_only,
                                   p_err);
  if (err) is->init_error = TRUE;
  return dtor_rp;
}  /* get_init_destructor */


/* Forward declaration. */
static void aggr_init_chained_designator(an_init_component_ptr  *p_icp,
                                         a_type_ptr             sub_type,
                                         an_init_state          *is,
                                         a_constant_ptr         *result);

static void aggr_init_element(an_init_component_ptr  *p_icp,
                              a_type_ptr             etype,
                              an_init_state          *is,
                              a_source_position      *diag_pos,
                              a_constant_ptr         *init_con);


static an_init_component_ptr skip_designators(an_init_component_ptr  icp)
/*
Return the first non-designator component on the list pointed to by icp (or
NULL if there is no such component).
*/
{
  while (icp != NULL && is_designator_component(icp)) {
    icp = icp->next;
  }  /* while */
  return icp;
}  /* skip_designators */


static a_boolean diagnose_empty_braced_component(an_init_component_ptr  icp)
/*
If the tree of components pointed to by icp contains empty braces ("{}"),
issue an error and return TRUE.  Otherwise, return FALSE.
*/
{
  a_boolean  result = FALSE;

  for (; icp != NULL && !result; icp = icp->next) {
    if (is_braced_init_component(icp)) {
      if (icp->variant.braced.list == NULL) {
        pos_error(ec_invalid_empty_initializer_list, init_component_pos(icp));
        result = TRUE;
      } else if (diagnose_empty_braced_component(icp->variant.braced.list)) {
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* for */
  return result;
}  /* diagnose_empty_braced_component */


static void aggr_init_simple_element(an_init_component_ptr  *p_icp,
                                     a_type_ptr             dest_type,
                                     an_init_state          *is,
                                     a_constant_ptr         *init_con)
/*
*p_icp is a single element in an aggregate initialization described by *is.
Ordinarily, *p_icp initializes an array element or a field of type dest_type,
but in error cases it could also be an invalid designator.
Return a constant in *init_con describing the element-level initialization, and
update *p_icp to the next component to be consumed.
The presence of a nonconstant initializer component is reflected in *is.
*/
{
  an_init_state          elem_is;
  an_init_component_ptr  orig_icp = *p_icp, icp = orig_icp;
  a_boolean              braced = is_braced_init_component(icp);

remove_any_extraneous_braces:
  if (braced) {
    if (is_reference_type(dest_type)) {
      /* Braces are not necessarily redundant: Initializing a reference amounts
         to initializing a temporary bound to that reference.  If the temporary
         has an aggregate type, the braces are not redundant.  Either way, this
         will be handled at the time the temporary initialization is processed.
         */
    } else if (is_class_struct_union_type(dest_type)) {
      /* If we get here with a class type, it must be a non-aggregate.  Braced
         initializers for non-aggregates are only permitted when list
         initialization is enabled. */
      check_assertion(!is_aggregate_type(dest_type));
      if (!list_init_enabled) {
        pos_ty_error(ec_brace_initialization_not_allowed,
                     init_component_pos(icp), dest_type);
      }  /* if */
    } else if (icp->variant.braced.list == NULL) {
      /* Empty braces: Pass the braces to convert_initializer below (which
         results in "value initialization"). */
      if (!list_init_enabled) {
        /* Empty braces initializing a scalar are a C++11 list initialization
           feature. */
        pos_error(ec_exp_primary_expr, &icp->variant.braced.end_pos);
      }  /* if */
    } else {
      /* A redundantly braced initializer: Strip off the "brace components". */
      an_error_severity      sev = es_none;
      a_source_position      *brace_pos = init_component_pos(icp);
      a_source_position      *excess_init_pos = NULL;
      an_init_component_ptr  next_icp;
      braced = TRUE;
      icp = icp->variant.braced.list;
      /* Check for excess initializers (designators don't count). */
      next_icp = skip_designators(icp);
      if (next_icp == NULL) {
        expect_error();
      } else if (next_icp->next != NULL) {
        excess_init_pos = init_component_pos(next_icp->next);
      }  /* if */
      if (!C_mode()) {
        /* A single level of braces is standard in C, but not in C++. */
        sev = strict_ansi_mode ? strict_ansi_error_severity : es_warning; 
      }  /* if */
      if (is_braced_init_component(icp) && icp->variant.braced.list != NULL) {
        /* Multiple levels of extra braces. */
        if (gcc_mode ||
            (microsoft_mode && (!C_mode() || microsoft_version < 1310))) {
          /* In GNU C mode and in some Microsoft modes, extraneous braces are
             ignored.  Issue a warning at least. */
          if (sev == es_none) sev = es_warning;
        } else {
          sev = es_error;
        }  /* if */
        /* Skip the levels of extra braces. */
        do {
          icp = icp->variant.braced.list;
          /* Check for excess initializers (designators don't count). */
          next_icp = skip_designators(icp);
          if (next_icp == NULL) {
            expect_error();
          } else if (next_icp->next != NULL) {
            excess_init_pos = init_component_pos(next_icp->next);
          }  /* if */
        } while (is_braced_init_component(icp) &&
                 icp->variant.braced.list != NULL);
      }  /* if */
      if (is->no_diagnostics) {
        is->init_error = is_effective_error(ec_nonstd_braces, sev);
      } else {
        pos_diagnostic(sev, ec_nonstd_braces, brace_pos);
      }  /* if */
      if (excess_init_pos != NULL) {
        if (gcc_mode) {
          /* GCC accepts excess initializers here with a warning, unless one
             of those initializers contains "{}". */
          if (!diagnose_empty_braced_component(icp->next)) {
            pos_warning(ec_excess_initializers_ignored, excess_init_pos);
          }  /* if */
        } else {
          if (is->no_diagnostics) {
            is->init_error = TRUE;
          } else {
            pos_error(ec_too_many_initializer_values, excess_init_pos);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (is_designator_component(icp)) {
    if (is->no_diagnostics) {
      is->init_error = TRUE;
    } else {
      pos_ty_error(ec_designator_requires_aggregate_type,
                   init_component_pos(icp), dest_type);
    }  /* if */
    dest_type = error_type();
    icp = skip_designators(icp);
    if (!is_expression_component(icp)) {
      goto remove_any_extraneous_braces;
    }  /* if */
  }  /* if */
  /* Convert the single value as appropriate. */
  /* Copy the initialization state for the top-level initialization, except
     that it should always indicate copy initialization (even if the top level
     initialization is direct).  The call to convert_initializer will update
     elem_is.init_con and elem_is.init_dip (possibly to NULL). */
  elem_is = *is;
  elem_is.direct_init = FALSE;
  convert_initializer(icp, dest_type, /*is_var_init=*/FALSE,
                      /*fill_in_dtor=*/exceptions_enabled, &elem_is);
  if (elem_is.init_error || is_error_component(icp)) {
    is->init_error = TRUE;
  }  /* if */
  if (elem_is.constant_expr_ruled_out) {
    is->constant_expr_ruled_out = TRUE;
  }  /* if */
  if (is->check_validity_only) {
    /* No return value. */
    *init_con = NULL;
  } else if (is->init_error) {
    *init_con = alloc_error_constant();
  } else if (elem_is.init_con != NULL) {
    /* A constant initializer: Return it. */
    *init_con = elem_is.init_con;
  } else if (elem_is.init_dip != NULL) {
    /* A nonconstant entry: Wrap it in a ck_dynamic_init entry, and record the
       fact that a nonconstant entry was seen. */
    a_dynamic_init_ptr  dip = elem_is.init_dip;
    check_assertion(!is->check_validity_only);
    *init_con = alloc_constant((a_constant_repr_kind)ck_dynamic_init);
    (*init_con)->variant.dynamic_init = dip;
    (*init_con)->type = dest_type;
    (*init_con)->source_corresp.decl_position = *init_component_pos(icp);
#if EXTRA_SOURCE_POSITIONS_IN_IL
    if (!is_designator_component(icp)) {
      (*init_con)->end_position = *init_component_end_pos(icp);
    }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    if (dip->kind == (a_dynamic_init_kind)dik_constant ||
        dip->kind == (a_dynamic_init_kind)dik_nonconstant_aggregate) {
      /* If this dynamic initialization embeds a designator, record it in the
         newly created constant. */
      (*init_con)->uses_designated_initializers =
                           dip->variant.constant->uses_designated_initializers;
    }  /* if */
    is->has_dynamic_init_component = TRUE;
    if (exceptions_enabled && dip->destructor != NULL) {
      record_partial_aggregate_cleanup_destruction(dip,
                                                   elem_is.evaluated);
    }  /* if */
  }  /* if */
  if (braced) {
    /* Proceed with the component after the braces. */
    *p_icp = orig_icp->next;
  } else {
    /* Proceed with the next component in the sequence. */
    *p_icp = icp->next;
  }  /* if */
}  /* aggr_init_simple_element */


static void aggr_init_generic_element(an_init_component_ptr  icp,
                                      a_type_ptr             gtype,
                                      an_init_state          *is,
                                      a_constant_ptr         *init_con)
/*
The given initialization component initializes an entity of unknown type gtype.
Create an aggregate constant whose structure matches that of *icp and return it
through *init_con (unless is->check_validity_only is TRUE).  Update the state
of the whole initialization (*is) as appropriate.
*/
{
  check_assertion(is_template_param_type(gtype) ||
                  (is_immediate_class_type(gtype) &&
                   gtype->variant.class_struct_union.is_nonreal_class) ||
                  (gpp_mode && is_prototype_instantiation_context()) ||
                  is_error_type(gtype));
  if (is->check_validity_only && !designators_allowed) {
    /* Except for designators, this routine always "succeeds" without
       diagnostics.  So if no IL should be produced, there is nothing to be
       done. */
    *init_con = NULL;
  } else if (is_expression_component(icp)) {
    /* A simple expression: No more recursion is needed. */
    aggr_init_simple_element(&icp, gtype, is, init_con);
  } else if (is_braced_init_component(icp)) {
    /* A braced list: Recursively treat every item in the list. */
    a_type_ptr  dest_type = type_of_unknown_templ_param_nontype;
    if (is_error_type(gtype)) dest_type = gtype;
    *init_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
    (*init_con)->type = gtype;
    (*init_con)->source_corresp.decl_position = *init_component_pos(icp);
#if EXTRA_SOURCE_POSITIONS_IN_IL
    if (!is_designator_component(icp)) {
      (*init_con)->end_position = *init_component_end_pos(icp);
    }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    (*init_con)->explicit_braces_on_aggregate = TRUE;
    for (icp = icp->variant.braced.list; icp != NULL; icp = icp->next) {
      a_constant_ptr  elem_con;
      aggr_init_generic_element(icp, dest_type, is, &elem_con);
      if (elem_con != NULL) {
        add_constant_to_aggregate(elem_con, *init_con);
      } else {
        check_assertion(is->init_error);
      }  /* if */
    }  /* if */
  } else if (is_designator_component(icp)) {
    /* We don't permit designators in templates because we cannot represent a
       field designator in the IL if we don't actually have a field entry to
       point to. */
    is->init_error = TRUE;
    *init_con = NULL;
    if (!is->check_validity_only && !is_error_type(gtype)) {
      pos_error(ec_designator_for_template_dependent_type,
                init_component_pos(icp));
    }  /* if */
  } else {
    unexpected_condition();
  }  /* if */
}  /* aggr_init_generic_element */

#if GNU_VECTOR_TYPES_ALLOWED

static a_boolean try_whole_vector_init(an_init_component_ptr  *p_icp,
                                       a_type_ptr             vtype,
                                       an_init_state          *is,
                                       a_constant_ptr         *result)
/*
p_icp represents an expression that might initialize vector of the given type
(the whole vector; not just an element of it).  If it does, return TRUE, set
*result to the a_constant entry representing the initializer, and update
*p_icp to the next component that hasn't been consumed.
*/
{
  a_boolean  success = FALSE;

  check_assertion(is_expression_component(*p_icp));
  if (whole_vector_init_possible(*p_icp, vtype)) {
    aggr_init_simple_element(p_icp, vtype, is, result);
    success = TRUE;
  }  /* if */
  return success;
}  /* try_whole_vector_init */


static void aggr_init_vector(an_init_component_ptr  *p_icp,
                             a_type_ptr             vtype,
                             an_init_state          *is,
                             a_source_position      *diag_pos,
                             a_constant_ptr         *init_con)
/*
Produce an aggregate constant (in *init_con) for the initialization of a GNU
vector type (vtype).  The initializer is described by *p_icp, and that value
is updated to the next initializer to be considered by the caller (if the
initializer is braced, just one initializer is "consumed", but otherwise
multiple components may be used for this initialization).  *is describes the
initialization as a whole.  diag_pos is the default position for
diagnostics.
*/
{
  an_init_component_ptr  icp = *p_icp;

  vtype = skip_typerefs(vtype);
  check_assertion(vtype->kind == (a_type_kind)tk_vector);
  if (is_expression_component(icp) &&
      try_whole_vector_init(p_icp, vtype, is, init_con)) {
    /* An expression of vector type (or convertible to a vector type) that
       initializes the whole vector. */
  } else {
    a_targ_size_t      ecount, icount = 0;
    a_boolean          no_bound = FALSE, saved_pack_expansion_handled;
    a_boolean          braced = is_braced_init_component(icp);
    a_type_ptr         etype;
    ecount = num_vector_elements(vtype);
    etype = vtype->variant.vector.element_type;
    check_assertion(!is_aggregate_or_union_type(etype));
    /* Create the result entry (unless we are only checking validity). */
    if (is->check_validity_only) {
      *init_con = NULL;
    } else {
      *init_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
      (*init_con)->type = vtype;
      (*init_con)->source_corresp.decl_position = *init_component_pos(icp);
#if EXTRA_SOURCE_POSITIONS_IN_IL
      if (!is_designator_component(icp)) {
        (*init_con)->end_position = *init_component_end_pos(icp);
      }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      (*init_con)->explicit_braces_on_aggregate = braced;
    }  /* if */
    if (braced) {
      /* The element values are enclosed in braces. */
      /* Diagnostics not associated with a particular element should be issued
         on the closing brace. */
      diag_pos = &icp->variant.braced.end_pos;
      /* Unwrap the braced list for the processing that follows. */
      icp = icp->variant.braced.list;
      /* Save the pack-expansion-handled state: Any expansions seen have an
         effect only within the braces. */
      saved_pack_expansion_handled = is->pack_expansion_handled;
    } else {
      if (!is->elided_braces_allowed) {
        /* Braces were elided at this level, but this is not a context that
           permits such elision.  (We don't issue an error if another error
           has already been issued for this initialization.) */
        if (!is->no_diagnostics && !is->init_error) {
          pos_error(ec_cannot_elide_braces, init_component_pos(icp));
        }  /* if */
        is->init_error = TRUE;
      }  /* if */
    }  /* if */
    while (icp != NULL && (no_bound || icount < ecount)) {
      a_constant_ptr  elem_con;
      aggr_init_element(&icp, etype, is, diag_pos, &elem_con);
      if (!is->check_validity_only) {
        add_constant_to_aggregate(elem_con, *init_con);
      }  /* if */
      if (is->pack_expansion_handled) {
        /* If a pack expansion was seen, don't try to track element counts. */
        no_bound = TRUE;
      } else {
        ++icount;
      }  /* if */
    }  /* if */
    if (no_bound) {
      /* The number of elements in the initializer isn't really known: Don't
         attempt related checks. */
    } else if (icp == NULL && icount < ecount) {
      /* No more initializers, but not all elements were initialized. */
      is->partial_initializer = TRUE;
      if (!is->check_validity_only) {
        (*init_con)->partial_aggr_value = TRUE;
        (*init_con)->is_partially_initialized = TRUE;
      }  /* if */
    }  /* if */
    if (braced) {
      /* The caller should move on to the component that follows the braced
         list (if any). */
      *p_icp = (*p_icp)->next;
      if (icp != NULL) {
        /* Extraneous elements: Issue a diagnostic (an error in GNU C++ mode; a
           warning otherwise). */
        if (!is->no_diagnostics) {
          pos_diagnostic(gpp_mode ? es_error : es_warning,
                         gpp_mode ? ec_too_many_initializer_values
                                  : ec_excess_initializers_ignored,
                         init_component_pos(icp));
        } else if (gpp_mode) {
          is->init_error = TRUE;
        }  /* if */
      }  /* if */
      is->pack_expansion_handled = saved_pack_expansion_handled;
    } else {
      /* Braces were omitted at this level of aggregate initialization: The
         the caller should continue associating the next component with any
         aggregate elements that follow this array. */
      check_assertion_or_expect_error(is->non_top_level_aggregate);
      *p_icp = icp;
    }  /* if */
  }  /* if */
}  /* aggr_init_vector */

#endif /* GNU_VECTOR_TYPES_ALLOWED */
#if C99_IL_EXTENSIONS_SUPPORTED

static void aggr_init_complex(an_init_component_ptr  *p_icp,
                              a_type_ptr             dtype,
                              an_init_state          *is,
                              a_constant_ptr         *init_con)
/*
Produce an aggregate constant (in *init_con) for the initialization of a GNU
complex type dtype.  *p_icp represents a braced initializer with at least two
elements.  *is tracks the current initialization.  The component following the
braced initializer (or NULL if there is none) is returned through *p_icp.
*/
{
  an_init_component_ptr  icp = *p_icp;
  a_source_position      *diag_pos;
  a_type_ptr             ftype;
  a_constant_ptr         elem_con;

  check_assertion(is_braced_init_component(icp));
  diag_pos = &icp->variant.braced.end_pos;
  /* Create the result entry (unless we are only checking validity). */
  if (is->check_validity_only) {
    *init_con = NULL;
  } else {
    *init_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
    (*init_con)->type = dtype;
    (*init_con)->source_corresp.decl_position = *init_component_pos(icp);
#if EXTRA_SOURCE_POSITIONS_IN_IL
    if (!is_designator_component(icp)) {
      (*init_con)->end_position = *init_component_end_pos(icp);
    }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    (*init_con)->explicit_braces_on_aggregate = TRUE;
  }  /* if */
  /* Determine the underlying floating-point type of dtype. */
  ftype = skip_typerefs(dtype);
  check_assertion(dtype->kind == (a_type_kind)tk_complex);
  ftype = float_type(ftype->variant.float_kind);
  /* Convert the real and complex parts in turn. */
  icp = icp->variant.braced.list;
  check_assertion(icp != NULL && icp->next != NULL);
  aggr_init_element(&icp, ftype, is, diag_pos, &elem_con);
  if (!is->check_validity_only) {
    add_constant_to_aggregate(elem_con, *init_con);
  }  /* if */
  aggr_init_element(&icp, ftype, is, diag_pos, &elem_con);
  if (!is->check_validity_only) {
    add_constant_to_aggregate(elem_con, *init_con);
  }  /* if */
  /* Issue an error if there are extraneous elements. */
  if (icp != NULL) {
    pos_error(ec_too_many_initializer_values, init_component_pos(icp));
  }  /* if */
  /* Move to the next element after the braces. */
  *p_icp = (*p_icp)->next;
}  /* aggr_init_complex */

#endif /* C99_IL_EXTENSIONS_SUPPORTED */

static a_constant_ptr default_nontrivial_init_constant_for_aggr_member(
                                                 a_type_ptr         tp,
                                                 an_init_state      *is,
                                                 a_source_position  *diag_pos)
/*
Return a constant representing the generated default initializer for an
aggregate member requiring nontrivial initialization (i.e., a default
constructor must be called, and/or a destructor must be recorded in case an
exception aborts the initialization).  tp is the type of the member.  *is
tracks the initialization as a whole and may be updated by this routine (e.g.,
to indicate that an error occurred).  Diagnostics should be issued at the
given position, unless is->no_diagnostics is TRUE.
*/
{
  a_constant_ptr      result = NULL;
  a_dynamic_init_ptr  dip;
  a_routine_ptr       ctor_rp, dtor_rp = NULL;
  a_boolean           err = FALSE, *p_err = NULL;

  /* Get the default constructor. */
  if (is->no_diagnostics) p_err = &err;
  /* No access checking is done during tentative matching for overload
     resolution (indicated by is->check_validity_only). */
  ctor_rp = select_default_constructor_full(
                                    tp, diag_pos, tp, /*evaluated=*/TRUE,
                                    /*check_access=*/!is->check_validity_only,
                                    p_err, (a_boolean *)NULL);
  if (err) is->init_error = TRUE;
  /* Determine if a constructor call will be involved. */
  if (exceptions_enabled && !is->initializer_must_be_constant) {
    a_class_symbol_supplement_ptr  cssp = symbol_supplement_for_class(tp);
    if (has_nontrivial_destructor(cssp)) {
      dtor_rp = get_init_destructor(tp, is, diag_pos);
    }  /* if */
  }  /* if */
  if (ctor_rp == NULL || is->init_error) {
    /* Trivial default constructor or error. */
    if (!is->check_validity_only) {
      dip = alloc_dynamic_init((a_dynamic_init_kind)dik_zero);
    }  /* if */
    is->has_dynamic_init_component = TRUE;
  } else  {
    if (!is->check_validity_only) {
      /* For a non-trivial constructor, create a dik_constructor dynamic init
         entry or, if a constant result is needed, a constant representing
         the folded constructor call. */
      dip = alloc_ctor_dynamic_init(ctor_rp, /*implied_source=*/FALSE);
      dip->variant.constructor.value_initialization = TRUE;
      if (is->initializer_must_be_constant) {
        result = get_default_constructed_constant(dip, tp, diag_pos);
      } else {
        a_constant  class_con;
        if (ctor_rp->is_constexpr &&
            fold_constexpr_ctor(dip, /*record_backing_expr=*/TRUE, diag_pos,
                                &class_con)) {
          if (class_con.is_partially_initialized) {
            is->partial_initializer = TRUE;
          }  /* if */
          result = alloc_unshared_constant(&class_con);
          if (dtor_rp != NULL) {
            /* Despite construction being folded into a constant, a nontrivial
               (and non-constexpr) destructor will still need to be called.
               Proceed with a dik_constant entry to which the destructor call
               can be added below. */
            dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constant);
            dip->variant.constant = result;
            if (result->is_partially_initialized) {
              dip->is_partially_initialized = TRUE;
            }  /* if */
            result = NULL;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    if (!ctor_rp->is_constexpr) {
      is->constant_expr_ruled_out = TRUE;
    }  /* if */
    /* If the default constructor is generated and some component of the class
       requires zeroing, initialization is not really done because the
       value-initialization rules require that the zeroing occurs. */
    if (ctor_rp->compiler_generated &&
        tp->variant.class_struct_union.has_zero_init_component) {
      is->partial_initializer = TRUE;
    }  /* if */
  }  /* if */
  /* If appropriate, add a destructor pointer to the dynamic init entry.
     This is for the case in which an exception is thrown by the
     constructor before the entire array has been initialized. */
  if (dtor_rp != NULL && !is->check_validity_only) {
    dip->destructor = dtor_rp;
    record_partial_aggregate_cleanup_destruction(dip, is->evaluated);
  }  /* if */
  /* Now create the constant entry (if needed). */
  if (!is->check_validity_only && result == NULL) {
    result = alloc_constant((a_constant_repr_kind)ck_dynamic_init);
    result->variant.dynamic_init = dip;
    result->type = tp;
    is->has_dynamic_init_component = TRUE;
  }  /* if */
  return result;
}  /* default_nontrivial_init_constant_for_aggr_member */


static a_constant_ptr implicit_init_anonymous_union_member(
                                                 a_type_ptr         tp,
                                                 an_init_state      *is,
                                                 a_source_position  *diag_pos)
/*
tp is an anonymous union type that is not explicitly initialized by an
aggregate initializer.  Return a constant representing the implicit
initialization of the anonymous union.  *is tracks the initialization as a
whole and may be updated by this routine (e.g., to indicate that an error
occurred).  Diagnostics should be issued at the given position, unless
is->no_diagnostics is TRUE.
*/
{
  a_constant_ptr                 result = NULL;
  a_class_symbol_supplement_ptr  cssp;

  check_assertion(is_immediate_class_type(tp) &&
                  class_type_supp(tp)->anonymous_union_kind ==
                                          (an_anonymous_union_kind)auk_field);
  cssp = class_symbol_supp(symbol_for(tp));
  if (cssp->variant_member_with_nontrivial_default_ctor ||
      cssp->variant_member_with_nontrivial_dtor) {
    /* A union with a variant member that has a nontrivial default constructor
       has a deleted default constructor itself, which prevents its default
       initialization.  Similarly with the destructor.  (For anonymous union
       types, we don't record special members in the symbol table, but from
       the standard's point of view the type of anonymous unions are ordinary
       unions.) */
    if (!is->no_diagnostics) {
      pos2_diagnostic(es_error, ec_cannot_default_initialize_anon_union,
                      diag_pos, &tp->source_corresp.decl_position);
    }  /* if */
    is->init_error = TRUE;
    result = alloc_error_constant();
  } else {
    a_field_ptr  fp, first_field;
    if (!is->check_validity_only) {
      result = alloc_constant((a_constant_repr_kind)ck_aggregate);
      result->type = tp;
      result->implicit_aggr_element = TRUE;
    }  /* if */
    first_field = tp->variant.class_struct_union.field_list;
    /* Search for a field with a field initializer. */
    for (fp = next_initializable_field(first_field);
         fp != NULL;
         fp = next_initializable_field(fp->next)) {
      if (fp->has_initializer) break;
    }  /* for */
    if (fp != NULL) {
      a_dynamic_init_ptr  dip;
      a_constant_ptr      des_con, elem_con;
      a_constant          folded_value;
      scan_field_initializer_if_needed(fp, tp);
      dip = fp->initializer;
      if (dip == NULL) {
        /* This can happen when a field initializer depends on a generated
           default constructor that depends itself on the field initializer. */
        is->init_error = TRUE;
        if (!is->no_diagnostics) {
          pos_ty_error(
                   ec_generated_default_constructor_used_in_field_initializer,
                   diag_pos, sym_parent_class(symbol_for(fp)));
        }  /* if */
        if (!is->check_validity_only) {
          dip = make_error_constant_dynamic_init();
        }  /* if */
      }  /* if */
      if (fp != first_field && !is->check_validity_only) {
        /* Add a designator to indicate the field to initialize. */
        des_con = alloc_constant((a_constant_repr_kind)ck_designator);
        des_con->variant.designator.field = fp;
        add_constant_to_aggregate(des_con, result);
      }  /* if */
      if (dip == NULL) {
        /* This can happen in error cases: Don't attempt operations on *dip. */
        check_assertion(is->init_error);
      } else if (fold_constexpr_dynamic_init(dip, fp->type, diag_pos,
                                             &folded_value)) {
        /* Append a copy of the constant. */
        if (!is->check_validity_only) {
          elem_con = alloc_unshared_constant(&folded_value);
        }  /* if */
      } else {
        if (!is->check_validity_only) {
          /* Copy the initializer and place the copy under a ck_dynamic_init
             constant. */
          dip = copy_dynamic_init(dip, CE_COPIED_CONSTANTS_MAY_BE_SHARED);
          elem_con = alloc_constant((a_constant_repr_kind)ck_dynamic_init);
          elem_con->variant.dynamic_init = dip;
          elem_con->type = fp->type;
        }  /* if */
        is->has_dynamic_init_component = TRUE;
      }  /* if */
      if (!is->check_validity_only) {
        elem_con->implicit_aggr_element = TRUE;
        add_constant_to_aggregate(elem_con, result);
      }  /* if */
    } else {
      /* A traditional (POD) union.  Just keep the empty aggregate constant. */
    }  /* if */
  }  /* if */
  return result;
}  /* implicit_init_anonymous_union_member */


static a_boolean implicit_init_involves_ref_init(a_type_ptr tp)
/*
Return TRUE if the given type is:
  - a reference type, or
  - an aggregate type (in the C++ sense) with a member of such a type.
*/
{
  a_boolean   result = FALSE;

  if (is_any_reference_type(tp)) {
    result = TRUE;
  } else {
    if (is_array_type(tp)) {
      tp = underlying_array_element_type(tp);
    }  /* if */
    tp = skip_typerefs(tp);
    if (is_immediate_class_type(tp)) {
      a_class_symbol_supplement_ptr  cssp = class_symbol_supp(symbol_for(tp));
      if (cssp->is_class_aggregate) {
        a_symbol_ptr  sym = cssp->symbols;
        for (; sym != NULL; sym = sym->next_in_scope) {
          if (symbol_is(sym, sk_field) &&
              implicit_init_involves_ref_init(sym->variant.field.ptr->type)) {
            result = TRUE;
            break;
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* implicit_init_involves_ref_init */


static a_constant_ptr add_repeat_con(a_constant_ptr  elem_con,
                                     a_targ_size_t   count)
/*
Return a ck_init_repeat for the given count on top of the given constant.
The count can be zero.
*/
{
  a_constant_ptr  result; 

  result = alloc_constant((a_constant_repr_kind)ck_init_repeat);
  result->variant.init_repeat.count = count;
  result->variant.init_repeat.constant = elem_con;
  return result;
}  /* add_repeat_con */


static a_constant_ptr repeat_constant_for_array_init(a_constant_ptr  cp,
                                                     a_type_ptr      atp)
/*
Return an aggregate constant for the initialization of the given array type
with every element initialized with the given constant.
*/
{
  a_constant_ptr  result;
  a_targ_size_t   count;

  check_assertion(is_array_type(atp));
  result = alloc_constant((a_constant_repr_kind)ck_aggregate);
  result->type = atp;
  count = num_array_elements(atp);
  if (count > 1) cp = add_repeat_con(cp, count);
  if (count > 0) add_constant_to_aggregate(cp, result);
  return result;
}  /* repeat_constant_for_array_init */


static a_boolean try_string_literal_init(an_init_component_ptr  icp,
                                         a_type_ptr             *p_array_type,
                                         an_init_state          *is,
                                         a_constant_ptr         *result)
/*
If the given initializer component is a valid string initializer for the given
array type, return TRUE and record the IL representation for that initializer
in *result (unless is->check_validity_only is TRUE).  Otherwise, return FALSE.
If TRUE is returned and *p_array_type represents an array with no specified
bound, replace *p_array_type with an array type corresponding to the string
size.
*/
{
  a_boolean  success = FALSE;

  if (may_be_string_type(*p_array_type)) {
    a_constant_ptr  string_constant;
    /* Permit an extra level of braces. */
    if (is_braced_init_component(icp)) icp = icp->variant.braced.list;
    if (icp != NULL && is_string_literal_component(icp, &string_constant)) {
      a_type_ptr  orig_string_type = string_constant->type;
      a_boolean   excess = FALSE, *p_excess = gcc_mode ? &excess : NULL;
      success = TRUE;
      if (check_string_constant_initializer_full(p_array_type, string_constant,
                                                 p_excess)) {
        if (!is->check_validity_only) {
          *result = alloc_unshared_constant(string_constant);
        }  /* if */
        if (!has_unknown_specified_bound(*p_array_type) &&
            num_array_elements(*p_array_type) >
                                  num_array_elements(string_constant->type)) {
          is->partial_initializer = TRUE;
        }  /* if */
        if (strict_ansi_mode && !list_init_enabled && !is->no_diagnostics &&
            is_parenthesized_component(icp)) {
          /* Strictly speaking, the standard doesn't allow parenthesized string
             literals for aggregate initialization.  However, with C++11-style
             list initialization that could make overload resolution depend on
             whether a string literal is parenthesized or not, which is not
             desirable.  So we impose this only in strict modes that don't
             permit generalized list initialization (typically, C++03 mode). */
          pos_diagnostic(strict_ansi_discretionary_severity,
                         ec_nonstandard_parenthesized_string_initializer,
                         init_component_pos(icp));
        } else if (excess && !is->no_diagnostics) {
          pos_warning(ec_excess_characters_in_literal_ignored,
                      init_component_pos(icp));
        }  /* if */
      } else {
        if (is->no_diagnostics) {
          is->init_error = TRUE;
        } else {
          /* Note: The call to check_string_constant_initializer truncates the
             string constant if needed.  So we must use the type of the string
             prior to that call. */
          pos_ty2_error(ec_bad_initializer_type, init_component_pos(icp),
                        orig_string_type, *p_array_type);
        }  /* if */
        if (!is->check_validity_only) {
          *result = alloc_error_constant();
        }  /* if */
        if (is_incomplete_array_type(*p_array_type) && !is->no_diagnostics) {
          /* An incomplete array initialized by an incompatible string literal.
             For better error recovery, replace the array type by an error
             type. */
          *p_array_type = error_type();
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return success;
}  /* try_string_literal_init */


static a_boolean try_whole_array_init(an_init_component_ptr  icp,
                                      a_type_ptr             array_type,
                                      a_constant_ptr         *result)
/*
icp represents an expression that might initialize an array of the given type
(the whole array; not just an element of it).  If it does, return TRUE, and
set *result to the a_constant entry representing the initializer.
*/
{
  a_boolean  success = FALSE;

  check_assertion(is_expression_component(icp));
  if (whole_array_init_possible(icp, array_type, result)) {
    success = TRUE;
  }  /* if */
  return success;
}  /* try_whole_array_init */


static void aggr_init_array_remainder_if_needed(a_constant_ptr     array_con,
                                                a_targ_size_t      count,
                                                a_type_ptr         etype,
                                                an_init_state      *is,
                                                a_source_position  *diag_pos)
/*
An array of elements of type etype has an initializer that doesn't initialize
all the array elements.  array_con points to a ck_aggregate constant that
represents the explicit initialization (unless is->check_validity_only flag is
FALSE).  Check that the remaining elements of the array can be initialized, and
if the flag is->check_validity_only is FALSE and the remaining elements need
dynamic initialization, append a constant representing that initialization to
the list embedded in array_con (plain zero initialization is done elsewhere if
needed).  *is describes the initialization as a whole, and diag_pos indicates
the position at which diagnostics should be issued.
*/
{
  a_boolean  partial_init_flag = TRUE;

  etype = skip_typerefs(etype);
  if (etype->kind == (a_type_kind)tk_array) {
    /* The element is a sub-array.  Create a single potentially-repeated
       initializer for all array levels. */
    count *= num_array_elements(etype);
    etype = underlying_array_element_type(etype);
    etype = skip_typerefs(etype);
  }  /* if */
  if (count == 0) {
    /* This can happen in GNU modes with arrays of zero-element arrays.  No
       further initializations are needed at this level. */
    check_assertion(gnu_mode);
    partial_init_flag = FALSE;
  } else if (is_real_class_type(etype)) {
    /* It is an array of class objects. */
    a_class_symbol_supplement_ptr  cssp = symbol_supplement_for_class(etype);
    if (has_trivial_default_constructor(cssp) &&
        (!exceptions_enabled || cssp->has_trivial_destructor)) {
      if (implicit_init_involves_ref_init(etype)) {
        /* An array element of class type with no constructor but with a ref
           member will end up uninitialized. */
        is->any_uninitialized_const_or_ref_member = TRUE;
      }  /* if */
      /* No initializers needed for the remaining array elements. */
    } else {
      /* Initialization must be represented in the IL since it is not
         trivial. */
      a_constant_ptr  remainder_con;
      partial_init_flag = FALSE;
      remainder_con = default_nontrivial_init_constant_for_aggr_member(
                                                         etype, is, diag_pos);
      if (!is->check_validity_only) {
        remainder_con->implicit_aggr_element = TRUE;
        /* Add the constant entry to the list of constants, but add a
           ck_repeat_init on top of it if needed. */
        check_assertion(array_con->type->kind == (a_type_kind)tk_array &&
                        array_con->kind == (a_constant_repr_kind)ck_aggregate);
        if (is->variable_size_array && !is->non_top_level_aggregate) {
          /* Something like "new T[x]{...}" where x is a run-time value.
             We don't know a priori how many default initializations are
             needed, but we have to represent it in some way so that lowering
             (or a back end) knows which routine to call.  We use a zero count,
             which somewhat matches the "incomplete array type" recorded in the
             new/delete supplement. */
          count = 0;
        }  /* if */
        /* Add a ck_init_repeat constant.  The case of a run-time count is
           represented using a "zero" ck_init_repeat.  A count of "one" is
           strictly speaking superfluous, but it makes it easier to pattern-
           match these implicit initializers. */
        remainder_con = add_repeat_con(remainder_con, count);
        remainder_con->implicit_aggr_element = TRUE;
        add_constant_to_aggregate(remainder_con, array_con);
      }  /* if */
    }  /* if */
  }  /* if */
  if (partial_init_flag) {
    /* The missing initializations are not explicit in the initializer.  Set
       the partial initializer flag in the initializer state. */
    is->partial_initializer = TRUE;
    if (array_con != NULL) {
      array_con->partial_aggr_value = TRUE;
      array_con->is_partially_initialized = TRUE;
    }  /* if */
  }  /* if */
}  /* aggr_init_array_remainder_if_needed */


static void set_aggr_tail_not_repeated_flag(a_constant_ptr  repeat_con)
/*
repeat_con is a ck_init_repeat constant generated for a GNU range designator.
The repeated constant is already recorded.  If the repeated constant
represents more than one element initializer value, the "repeat" should not
apply to any but the first value.  For example:
   int x[3][3] = { [0 ... 2][0] = 4, 5, 6 };
The constant struct for this is as follows:
   <ck_aggregate[3]>
     <ck_designator[0]><ck_init_repeat[3]>
                         <ck_aggregate[3]>
                            <ck_designator[0]> 4
                            5
                            6
Here, the ck_init_repeat should apply only to the first value (4), and not to
the "tail" of the ck_aggregate constant it points to.  This is indicated with
the multidimensional_aggr_tail_not_repeated flag.  This routine sets that flag
to TRUE if needed.
*/
{
  a_constant_ptr cp = repeat_con->variant.init_repeat.constant;

  /* Look through any chained designators to see if it was followed by more
     than one designated value. */
  while (cp != NULL) {
    if (cp->kind == (a_constant_repr_kind)ck_aggregate &&
        !cp->explicit_braces_on_aggregate) {
      /* An aggregate constant created by a designator. */
      a_constant_ptr  head = cp->variant.aggregate.first_constant;
      if (head != NULL && head->kind == (a_constant_repr_kind)ck_designator) {
        /* A chained designator: It must be followed by at least one element.
           If it is followed by more than one, then the repetition doesn't
           apply to the subsequent elements. */
        check_assertion(head->next != NULL);
        if (head->next->next != NULL) {
          repeat_con->variant.init_repeat
                             .multidimensional_aggr_tail_not_repeated = TRUE;
          break;
        } else {
          /* Examine the designated initializer. */
          cp = head->next;
        }  /* if */
      } else {
        cp = head;
      }  /* if */
    } else if (cp->kind == (a_constant_repr_kind)ck_init_repeat) {
      /* Another repetition, presumably from a chained array range
         designator. */
      cp = cp->variant.init_repeat.constant;
    } else {
      /* Not a constant created by chained designators. */
      break;
    }  /* if */
  }  /* while */
}  /* set_aggr_tail_not_repeated_flag */


static void aggr_init_array_designator(an_init_component_ptr  *p_icp,
                                       a_type_ptr             atype,
                                       an_init_state          *is,
                                       a_targ_size_t          *idx,
                                       a_constant_ptr         aggr_con,
                                       a_source_position      *diag_pos)
/*
*p_icp points to a designator component encountered while processing a braced
initializer for the given array type.  Check if the designator is valid, and,
if so, append a matching ck_designator constant to aggr_con.  This routine also
consumes initializer components up to and including a non-designator (and
*p_icp is updated to point to the component after that, or NULL if there is
none).  *is describes the initialization and *idx describes the index of the
next element to be initialized (which is updated by this routine).  diag_pos is
the position at which to issue diagnostics if no more specific position is
available.
*/
{
  a_boolean              okay, no_bound;
  an_init_component_ptr  icp = *p_icp;
  a_targ_size_t          repeat_count = 1;

  atype = skip_typerefs(atype);
  check_assertion(atype->kind == (a_type_kind)tk_array);
  no_bound = has_unknown_specified_bound(atype) ||
             (atype->variant.array.variant.number_of_elements == 0 &&
              !atype->variant.array.bound_is_zero);
  if (icp->variant.designator.field_name != NULL) {
    /* This is not an array designator, but we're in an array initializer.
       Issue an error. */
    okay = FALSE;
    pos_error(ec_invalid_designator_kind, init_component_pos(icp));
  } else if (no_bound ||
             (icp->variant.designator.element_index <
                            atype->variant.array.variant.number_of_elements &&
              icp->variant.designator.last_element_index <
                            atype->variant.array.variant.number_of_elements)) {
    okay = TRUE;
    *idx = icp->variant.designator.element_index;
    repeat_count = icp->variant.designator.last_element_index - *idx + 1;
  } else {
    /* The designator indicates a subscript outside the array bounds. */
    okay = FALSE;
    pos_error(ec_subscript_out_of_range, init_component_pos(icp));
  }  /* if */
  if (!C_mode() && okay) {
    a_type_ptr  etype = underlying_array_element_type(atype);
    etype = skip_typerefs(etype);
    if (is_immediate_class_type(etype) &&
        !etype->variant.class_struct_union.is_nonreal_class &&
        !symbol_supplement_for_class(etype)->is_POD) {
      /* Allowing designators in non-POD types would raise subtle questions
         about order of initialization and destruction.  For now, at least,
         we disallow such constructs.  (The error is only issued on the first
         designator if there is a sequence of consecutive designators.) */
      pos_error(ec_designator_for_non_POD, init_component_pos(icp));
    }  /* if */
  }  /* if */
  icp = icp->next;
  if (okay) {
    /* Designators complicate the determination of whether an aggregate
       initializer completely covers the target entity.  Assume partial
       initialization by default. */
    is->partial_initializer = TRUE;
    if (!is->check_validity_only) {
      /* Append the array designator constant to the end of the enclosing
         aggregate constant. */
      a_constant_ptr  des_con;
      des_con = alloc_constant((a_constant_repr_kind)ck_designator);
      des_con->variant.designator.array_element = *idx;
      des_con->source_corresp.decl_position = *init_component_pos(*p_icp);
      add_constant_to_aggregate(des_con, aggr_con);
      aggr_con->is_partially_initialized = TRUE;
    }  /* if */
    if (icp != NULL) {
      /* Process the component following this designator.  If it is another
         designator (i.e., a "chained" designator), special care must be taken
         to go down a level in the aggregate structure. */
      a_constant_ptr     next_con;
      a_boolean          saved_has_dynamic_init_component;
      a_source_position  *pos;
      if (repeat_count > 1) {
        /* Temporarily clear the has_dynamic_init_component flag so we can
           find out if the designated element has a dynamic component. */
        saved_has_dynamic_init_component = is->has_dynamic_init_component;
        is->has_dynamic_init_component = FALSE;
        pos = init_component_pos(icp);
      }  /* if */
      if (is_designator_component(icp)) {
        /* A chained designator follows (e.g., ".x.y =" or ".x[n] ="). */
        aggr_init_chained_designator(&icp, atype->variant.array.element_type,
                                     is, &next_con);
      } else {
        aggr_init_element(&icp, atype->variant.array.element_type, is,
                          diag_pos, &next_con);
      }  /* if */
      if (repeat_count > 1) {
        /* A repeated initializer cannot have a dynamic component. */
        if (is->has_dynamic_init_component) {
          if (is->no_diagnostics) {
            is->init_error = TRUE;
          } else {
            pos_error(ec_no_range_designator_with_dynamic_init, pos);
          }  /* if */
        }  /* if */
        if (saved_has_dynamic_init_component) {
          /* If a prior component was dynamic, merge that into the state. */
          is->has_dynamic_init_component = TRUE;
        }  /* if */
      }  /* if */
      *idx += repeat_count;
      if (next_con != NULL) {
        check_assertion(!is->check_validity_only);
        if (repeat_count > 1) {
          next_con = add_repeat_con(next_con, repeat_count);
          set_aggr_tail_not_repeated_flag(next_con);
        }  /* if */
        add_constant_to_aggregate(next_con, aggr_con);
      }  /* if */
    } else {
      expect_error();
    }  /* if */
  } else {
    /* The designator was invalid.  Subsequent initializer components are
       likely not going to match up with the destination type.  So for error
       recovery purposes, skip remaining initializer components. */
    icp = NULL;
    is->init_error = TRUE;
  }  /* if */
  *p_icp = icp;
}  /* aggr_init_array_designator */


static void aggr_init_array(an_init_component_ptr  *p_icp,
                            a_type_ptr             *p_array_type,
                            an_init_state          *is,
                            a_source_position      *diag_pos,
                            a_constant_ptr         *init_con)
/*
Produce an aggregate constant (in *init_con) for the initialization of an
object or subobject of the array type given by *p_array_type.  The initializer
is described by *p_icp, and that value is updated to the next initializer to be
considered by the caller (if the initializer is braced, just one initializer is
"consumed", but otherwise an arbitrary number may be used for this array
initialization).  *is describes the initialization as a whole.
*/
{
  an_init_component_ptr  icp = *p_icp;
  a_type_ptr             atype = skip_typerefs(*p_array_type);

  check_assertion(atype->kind == (a_type_kind)tk_array);
  if (try_string_literal_init(icp, p_array_type, is, init_con)) {
    /* A string literal initializer.  Nothing more to be done. */
    *p_icp = icp->next;
  } else if (is_expression_component(icp) &&
             try_whole_array_init(icp, atype, init_con)) {
    /* Whole-array initialization.  Currently, this is only possible in GNU C
       mode with compound literals.  For example:
         struct X { int i[3]; } x = { (int[3]){1, 2, 3} };
    */
    *p_icp = icp->next;
  } else {
    /* Ordinary element-by-element array initialization. */
    a_targ_size_t  ecount, idx = 0, icount = 0;
    a_type_ptr     etype = atype->variant.array.element_type;
    a_boolean      no_bound = FALSE, braced = is_braced_init_component(icp),
                   zero_sized_element = FALSE, incomplete_array = FALSE;
    a_boolean      saved_pack_expansion_handled;
    if (is->check_validity_only) {
      *init_con = NULL;
    } else {
      *init_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
      (*init_con)->type = atype;
      (*init_con)->source_corresp.decl_position = *init_component_pos(icp);
#if EXTRA_SOURCE_POSITIONS_IN_IL
      if (!is_designator_component(icp)) {
        (*init_con)->end_position = *init_component_end_pos(icp);
      }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      (*init_con)->explicit_braces_on_aggregate = braced;
    }  /* if */
    if (braced) {
      /* The element values are enclosed in braces. */
      /* Diagnostics not associated with a particular element should be issued
         on the closing brace. */
      diag_pos = &icp->variant.braced.end_pos;
      /* Unwrap the braced list for the processing that follows. */
      icp = icp->variant.braced.list;
      if (icp == NULL && C_mode() && !gcc_mode) {
        /* Empty initializer lists are not permitted in C mode (except GNU C
           mode). */
        pos_error(ec_exp_primary_expr, diag_pos);
      }  /* if */
      /* Save the pack-expansion-handled state: Any expansions seen have an
         effect only within the braces. */
      saved_pack_expansion_handled = is->pack_expansion_handled;
    } else {
      if (!is->elided_braces_allowed) {
        /* Braces were elided at this level, but this is not a context that
           permits such elision.  (We don't issue an error if another error
           has already been issued for this initialization.) */
        if (!is->no_diagnostics && !is->init_error) {
          pos_error(ec_cannot_elide_braces, init_component_pos(icp));
        }  /* if */
        is->init_error = TRUE;
      }  /* if */
    }  /* if */
    /* Determine the element count in the destination type if known. */
    if (has_any_unknown_specified_bound(atype)) {
      /* An array with a specified bound that cannot be evaluated (e.g., a
         template-dependent bound). */
      no_bound = TRUE;
    } else if (atype->variant.array.variant.number_of_elements == 0 &&
               !atype->variant.array.bound_is_zero) {
      /* An array whose number of elements is not a priori bound. */
      no_bound = TRUE;
      incomplete_array = TRUE;
    } else if (is_template_param_type(etype)) {
      /* For something like "T x[2] = { 1, 2, 3, 4 };" we cannot tell how the
         initializer elements should be allocated to the array elements, since
         T after instantiation can be an aggregate type that consumes any
         number of initializers.  To simplify processing, we therefore treat
         this as an unbounded array. */
      no_bound = TRUE;
    } else if (is->pack_expansion_handled) {
      /* If a pack expansion has been handled for an element at this level,
         don't attempt to match element counts with array bounds. */
      no_bound = TRUE;
    } else {
      ecount = atype->variant.array.variant.number_of_elements;
    }  /* if */
    if (is_array_type(etype) && has_any_zero_bound(etype)) {
      /* Some modes permit an array of zero-length arrays.  In that case, each
         element has size zero.  Note that etype may have a zero length
         dimension at a deeper level (e.g., int[3][0][1]); hence the use of
         num_array_elements (which counts elements across all dimensions). */
      zero_sized_element = TRUE;
    }  /* if */
    /* Loop through the initializer components and create individual constant
       entries for each of them. */
    while (icp != NULL) {
      if (is_designator_component(icp)) {
        /* One or more designators. */
        if (!braced && !is->chained_designator_okay) {
          /* The designator doesn't apply at this level.  Return to a previous
             level. */
          break;
        } else {
          is->chained_designator_okay = FALSE;
          aggr_init_array_designator(&icp, atype, is, &idx, *init_con,
                                     diag_pos);
          if (idx > icount) icount = idx;
        }  /* if */
      } else if (zero_sized_element) {
        /* Some modes allow zero-length arrays.  If the member type contains
           such an array, do not attempt to initialize it.  E.g.:
              int a[][0] = { 0 };  // Excess initializer.
        */
        break;
      } else if (no_bound || idx < ecount) {
        a_constant_ptr  elem_con;
        if (!is->non_top_level_aggregate &&
            is->arg_match != NULL &&
            ((is_aggregate_type(etype) && braced) || is_error_type(etype))) {
          /* We're evaluating a match for overload resolution (is->arg_match is
             non-NULL) and this is the top-level braced initializer for an
             array.  If the initialization for this element looks like an
             aggregate initialization, record it like a "user-defined
             conversion match".  Also do this for elements of error types since
             the match level might not be set elsewhere in such cases. */
          record_aggr_init_match(is->arg_match);
        }  /* if */
        aggr_init_element(&icp, etype, is, diag_pos, &elem_con);
        if (!is->check_validity_only) {
          add_constant_to_aggregate(elem_con, *init_con);
        }  /* if */
        ++idx;
        if (idx > icount) icount = idx;
        if (is->pack_expansion_handled) {
          /* If a pack expansion was seen, don't try to track element
             counts. */
          no_bound = TRUE;
        }  /* if */
      } else {
        /* No more elements to initialize. */
        break;
      }  /* if */
    }  /* while */
    if (!is->init_error &&
        ((!no_bound && icount < ecount) ||
         (is->variable_size_array && !is->non_top_level_aggregate &&
          !is_template_dependent_type(etype)))) {
      /* Not all array elements are explicitly initialized: Append an entry
         to initialize the remaining elements.  As special case occurs for
         expressions like "new T[x]{...}" where the number of uninitialized
         elements is not known, but lowering (or a back end) needs to know
         which default constructor to call: We arbitrarily pass a count of 1
         for that case (a count of zero would cause default initialization to
         be bypassed).  If T is template-dependent, we cannot do that reliably
         (and such cases do not go through lowering or a back end). */
      a_targ_size_t  rcount = 1;
      if (!no_bound) rcount = ecount - icount;
      aggr_init_array_remainder_if_needed(*init_con, rcount, etype, is,
                                          diag_pos);
    }  /* if */
    if (incomplete_array) {
      /* If appropriate, update the type of the constant and/or the type of
         the destination to reflect the actual number of initializer
         elements. */
      set_initialized_array_size(&atype, icount, /*unknown_dependent=*/FALSE);
      if (*init_con != NULL) (*init_con)->type = atype;
      if (!is->non_top_level_aggregate) {
        /* An aggregate initializer for a top-level incomplete array type.
           This is either an error, or the caller has requested to derive the
           dimension from the initializer (as, e.g., in "T x[] = { 1, 2 }").
           This also happens with variable-size array new-expressions such as
           "new T[n]{ 1, 2 }", where the destination type "T[]" is passed to
           this routine and is->initializer_can_dimension_array is TRUE (even
           though the type recorded in the associated new/delete supplement
           won't be updated).  (Note: The non-top-level case is only possible
           with flexible array initializers; the validity of that case is
           mode-dependent and checked elsewhere.) */
        if (is->initializer_can_dimension_array) {
          *p_array_type = atype;
        } else {
          expect_error();
        }  /* if */
      }  /* if */
    }  /* if */
    if (braced) {
      /* The caller should move on to the component that follows the braced
         list (if any). */
      *p_icp = (*p_icp)->next;
      if (icp != NULL && (!no_bound || zero_sized_element)) {
        /* Initializers remain at this level, but no elements. */
        an_error_severity  sev = gcc_mode ? es_warning : es_error;
        if (is->no_diagnostics) {
          is->init_error = sev == es_error;
        } else {
          pos_diagnostic(sev, 
                         (int)sev >= (int)es_error ?
                                               ec_too_many_initializer_values
                                             : ec_excess_initializers_ignored,
                         init_component_pos(icp));
        }  /* if */
      }  /* if */
      is->pack_expansion_handled = saved_pack_expansion_handled;
    } else {
      /* Braces were omitted at this level of aggregate initialization: The
         the caller should continue associating the next component with any
         aggregate elements that follow this array. */
      check_assertion_or_expect_error(is->non_top_level_aggregate || C_mode());
      *p_icp = icp;
    }  /* if */
  }  /* if */
}  /* aggr_init_array */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void set_cli_array_constant_dimensions(an_expr_node_ptr       dim_exprs,
                                              a_host_large_unsigned  rank,
                                              a_host_large_integer   dims[])
/*
For every constant in the list of CLI array dimension expressions pointed to by
dim_exprs, set the corresponding dims[] element (which is in reversed order) to
the value of that constant.  Set all other elements of dims[] to -1.  dim_exprs
can be NULL (in which case dims[1] ... dims[rank] will be set to -1).
*/
{
  an_expr_node_ptr  expr = dim_exprs;

  for (; rank != 0; --rank) {
    if (expr != NULL && is_constant_node(expr) &&
        expr->variant.constant->kind == (a_constant_repr_kind)ck_integer) {
      a_boolean  ovflo = FALSE;
      dims[rank] = value_of_integer_constant(expr->variant.constant, &ovflo);
    } else {
      dims[rank] = -1;
    }  /* if */
    if (expr != NULL) expr = expr->next;
  }  /* for */
}  /* set_cli_array_constant_dimensions */


static void aggr_init_cli_array_level(an_init_component_ptr  icp,
                                      a_type_ptr             etype,
                                      an_init_state          *is,
                                      a_host_large_unsigned  rank,
                                      a_host_large_integer   dims[],
                                      a_boolean              deduce_dims,
                                      a_constant_ptr         *result)
/*
icp is an initializer for a CLI array or a subarray thereof of the given rank.
etype is the element type.  is indicates the overall initialization state.
dims[rank] is the length of the array if known, or -1 otherwise.  If
deduce_dims is TRUE, dims[rank] is updated with the number of elements
initialized if that number is larger than the value already recorded in
dims[rank].  Produce an aggregate constant representing this initialization in
*result.
*/
{
  /* Traverse the rank-th dimension of the array. */
  if (is_braced_init_component(icp)) {
    a_host_large_integer  idx = 0;
    if (!is->check_validity_only) {
      a_symbol_ptr  array_type_sym = make_cli_array_type(etype, rank);
      *result = alloc_constant((a_constant_repr_kind)ck_aggregate);
      (*result)->type = make_handle_type(type_symbol_type(array_type_sym));
      (*result)->source_corresp.decl_position = *init_component_pos(icp);
#if EXTRA_SOURCE_POSITIONS_IN_IL
      if (!is_designator_component(icp)) {
        (*result)->end_position = *init_component_end_pos(icp);
      }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      (*result)->explicit_braces_on_aggregate = TRUE;
    }  /* if */
    icp = icp->variant.braced.list;
    while (icp != NULL) {
      a_constant_ptr  elem_con;
      ++idx;
      if (!deduce_dims && dims[rank] != -1) {
        /* The dimension of this array level is known.  Check for excess
           initializers. */
        if (idx > dims[rank]) {
          /* Too many initializers. */
          if (is->no_diagnostics) {
            is->init_error = TRUE;
          } else {
            pos_error(ec_too_many_initializer_values, init_component_pos(icp));
          }  /* if */
          break;
        }  /* if */
      }  /* if */
      if (rank > 1) {
        /* Another CLI array level: Recurse. */
        aggr_init_cli_array_level(icp, etype, is, rank-1, dims, deduce_dims,
                                  &elem_con);
        icp = icp->next;
      } else {
        /* Element-level initializers. */
        aggr_init_element(&icp, etype, is, init_component_pos(icp), &elem_con);
      }  /* if */
      if (!is->check_validity_only) {
        add_constant_to_aggregate(elem_con, *result);
      }  /* if */
    }  /* for */
    if (deduce_dims && idx > dims[rank]) {
      /* This is the largest dimension of the given rank we've encountered:
         Update the recorded dimension accordingly. */
      dims[rank] = idx;
    }  /* if */
  } else {
    if (!is->no_diagnostics && !is->init_error) {
      pos_error(ec_exp_lbrace, init_component_pos(icp));
    }  /* if */
    if (!is->check_validity_only) {
      *result = alloc_error_constant();
    }  /* if */
    is->init_error = TRUE;
  }  /* if */
}  /* aggr_init_cli_array_level */


void aggr_init_cli_array(an_init_component_ptr  icp,
                         a_type_ptr             hatype,
                         an_init_state          *is,
                         a_dynamic_init_ptr     *result,
                         an_expr_node_ptr       *dim_exprs)
/*
icp is a brace-enclosed initializer for a handle to CLI array type hatype.
*is describes the initialization as a whole.  Produce a dynamic init entry
(dik_constant or dik_nonconstant_aggregate) representing the initialization in
*result.
If *dim_exprs is NULL, deduce the dimensions of the array type from the
initializer, and set *dim_exprs to a list of dimension expressions suitable
for use in an enk_gcnew node.  (dim_exprs itself must be non-NULL.)
*/
{
  a_boolean       unknown_rank = FALSE, saved_elided_braces_allowed;
  a_type_ptr      atype;
  a_constant_ptr  aggr_con;

  check_assertion(dim_exprs != NULL);
  if (is_handle_type(hatype)) {
    /* The normal case: A handle type. */
    atype = type_pointed_to(hatype);
  } else {
    /* Error cases can get here. */
    expect_error();
    check_assertion(is_error_type(hatype));
    atype = hatype;
  }  /* if */
  /* Microsoft allows brace elision for initializers of CLI arrays with
     aggregate element types in all contexts. */
  saved_elided_braces_allowed = is->elided_braces_allowed;
  is->elided_braces_allowed = TRUE;
  /* If the array type is known, work through each level of the array,
     and deduce the dimension lengths if needed.  Otherwise, just scan a
     generic initializer. */
  if (is_cli_array_type(atype)) {
    a_type_ptr             etype = cli_array_element_type(atype);
    a_host_large_unsigned  rank = cli_array_rank(atype, &unknown_rank);
    a_host_large_integer   dims[33];
    check_assertion(rank >= 1 && rank < 33);
    set_cli_array_constant_dimensions(*dim_exprs, rank, dims);
    aggr_init_cli_array_level(icp, etype, is, rank, dims, *dim_exprs == NULL,
                              &aggr_con);
    if (*dim_exprs == NULL) {
      *dim_exprs = make_cli_array_length_nodes(rank, dims);
    }  /*if */
  } else {
    /* Presumably a handle to a generic type (that could end up being a CLI
       array type). */
    check_assertion(is_template_param_or_nonreal_class_type(atype) ||
                    is_error_type(atype));
    aggr_init_generic_element(icp, atype, is, &aggr_con);
  }  /* if */
  /* Restore the state wrt. brace elision. */
  is->elided_braces_allowed = saved_elided_braces_allowed;
  /* Wrap the aggregate constant in the appropriate kind of dynamic init
     entry. */
  *result = alloc_dynamic_init((a_dynamic_init_kind)
                   (is->has_dynamic_init_component ? dik_nonconstant_aggregate
                                                   : dik_constant));
  (*result)->variant.constant = aggr_con;
  (*result)->is_braced_initializer = TRUE;
}  /* aggr_init_cli_array */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static a_boolean try_whole_aggr_class_init(an_init_component_ptr  *p_icp,
                                           a_type_ptr             class_type,
                                           an_init_state          *is,
                                           a_constant_ptr         *result)
/*
p_icp represents an expression that might initialize an aggregate class of the
given type (the whole class object; not just a field of it).  If it does,
return TRUE, set *result to the a_constant entry representing the initializer,
and update *p_icp to the next component that hasn't been consumed.
For example:
         struct S { int i; } s, as[]{ 1, s };
Here the component 1 does not wholly initialize an element of as (so FALSE is
returned with no further action), but the component s does (so this function
returns TRUE, and sets *result to a constant representing the dynamic
initialization of as[1]).
*/
{
  a_boolean  success = FALSE;

  check_assertion(is_expression_component(*p_icp));
  if (whole_aggr_class_init_possible(*p_icp, class_type)) {
    aggr_init_simple_element(p_icp, class_type, is, result);
    success = TRUE;
  }  /* if */
  return success;
}  /* try_whole_aggr_class_init */


static a_boolean has_initializable_field(a_type_ptr  class_type)
/*
Return TRUE if the given class type has an initializable field.
*/
{
  a_field_ptr  fp = class_type->variant.class_struct_union.field_list;

  return next_initializable_field(fp) != NULL;
}  /* has_initializable_field */


static void aggr_init_class_remainder_if_needed(a_constant_ptr     aggr_con,
                                                a_type_ptr         aggr_type,
                                                a_field_ptr        next_field,
                                                an_init_state      *is,
                                                a_source_position  *diag_pos)
/*
We have processed an aggregate initializer for the given type, but it does not
explicitly initialize the given field nor any subsequent fields.  Append any
needed constants to the list embedded in aggr_con if the no_diagnostics flag
is FALSE (if it is TRUE, aggr_con will be NULL).
*is describes the initialization as a whole, and diag_pos indicates the
position for which diagnostics should be issued.
*/
{
  a_field_ptr  fp, last_dyn_field = NULL;

  next_field = next_initializable_field(next_field);
  /* Run a first pass through the remaining fields to see if any requires
     nontrivial default initialization.  Keep track of the last such field. */
  for (fp = next_field; fp != NULL; fp = next_initializable_field(fp->next)) {
    a_type_ptr  ftp = fp->type;
    if (implicit_init_involves_ref_init(ftp)) {
      /* An uninitialized reference will likely result in a diagnostic. */
      is->any_uninitialized_const_or_ref_member = TRUE;
    } else {
      if (is_array_type(ftp)) ftp = underlying_array_element_type(ftp);
      ftp = skip_typerefs(ftp);
      if (is_real_class_type(ftp)) {
        a_class_symbol_supplement  *cssp = symbol_supplement_for_class(ftp);
        if (!has_trivial_default_constructor(cssp) ||
            (exceptions_enabled && has_nontrivial_destructor(cssp)) ||
            (!cssp->is_class_aggregate &&
             class_type_supp(ftp)->anonymous_union_kind ==
                                         (an_anonymous_union_kind)auk_field)) {
          /* A default constructor and/or a destructor must be called to
             initialize this field. */
          last_dyn_field = fp;
        }  /* for */
      }  /* if */
    }  /* if */
  }  /* for */
  if (last_dyn_field != NULL) {
    a_field_ptr  end_fp = next_initializable_field(last_dyn_field->next);
    if (is_union_type(aggr_type)) {
      a_field_ptr  field2 = next_initializable_field(next_field->next);
      if (field2 != NULL) {
        /* A value-initialized union that has multiple fields, at least one of
           which requires nontrivial initialization: Issue an error since it
           isn't clear which should be the initialized field. */
        if (!is->no_diagnostics) {
          pos_error(ec_ambiguous_union_value_init, diag_pos);
        }  /* if */
        is->init_error = TRUE;
        /* For recovery purposes, just initialize the first field. */
        end_fp = field2;
      }  /* if */
    }  /* if */
    for (fp = next_field;
         fp != end_fp;
         fp = next_initializable_field(fp->next)) {
      a_type_ptr      ftp = skip_typerefs(fp->type), atp = NULL;
      a_constant_ptr  init_con = NULL;
      if (ftp->kind == (a_type_kind)tk_array) {
        atp = ftp;
        ftp = underlying_array_element_type(ftp);
        ftp = skip_typerefs(ftp);
      }  /* if */
      if (is_real_class_type(ftp)) {
        /* A field of class type (or array thereof): A constructor or
           destructor may be involved. */
        a_class_symbol_supplement_ptr  cssp = symbol_supplement_for_class(ftp);
        if (class_type_supp(ftp)->anonymous_union_kind ==
                                         (an_anonymous_union_kind)auk_field) {
          /* Anonymous union types don't have their own constructors or
             destructors, but if they include a field with an initializer,
             their initialization is not simply value initialization. */
          init_con = implicit_init_anonymous_union_member(ftp, is, diag_pos);
        } else if (!has_trivial_default_constructor(cssp) ||
                   (exceptions_enabled && !cssp->has_trivial_destructor)) {
          /* Nontrivial initialization/destruction. */
          init_con = default_nontrivial_init_constant_for_aggr_member(
                                                           ftp, is, diag_pos);
          if (atp != NULL && !is->check_validity_only) {
            /* The field is an array.  Wrap its initializer in an aggregate
               constant entry (but add an ck_init_repeat if needed). */
            init_con = repeat_constant_for_array_init(init_con, atp);
          }  /* if */
        }  /* if */
      }  /* if */
      if (!is->check_validity_only) {
        if (init_con == NULL) {
          /* Default initialization doesn't involve a constructor or destructor
             call.  Use a zero-valued constant for scalar types, and an empty
             aggregate for aggregate types. */
          init_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
          if (is_scalar_type(ftp) && atp == NULL) {
            make_zero_of_proper_type(ftp, init_con);
          } else {
            init_con->type = (atp == NULL) ? ftp : atp;
            if (!(is_immediate_class_type(ftp) &&
                  has_initializable_field(ftp)) &&
                !(atp != NULL && has_any_zero_bound(atp))) {
              /* Other than for empty classes and zero-length arrays, an empty
                 aggregate constant does not cover all the elements of the
                 destination type. */
              is->partial_initializer = TRUE;
              init_con->partial_aggr_value = TRUE;
              init_con->is_partially_initialized = TRUE;
            }  /* if */
          }  /* if */
        }  /* if */
        /* Add the constant entry to the list of constants. */
        init_con->implicit_aggr_element = TRUE;
        add_constant_to_aggregate(init_con, aggr_con);
      }  /* if */
    }  /* for */
  }  /* if */
  /* Check if there are any remaining fields not covered by the initializer. */
  if (last_dyn_field == NULL) {
    if (next_field != NULL) {
      is->partial_initializer = TRUE;
      if (aggr_con != NULL) {
        aggr_con->partial_aggr_value = TRUE;
        aggr_con->is_partially_initialized = TRUE;
      }  /* if */
    }  /* if */
  } else if (next_initializable_field(last_dyn_field->next) != NULL) {
    is->partial_initializer = TRUE;
    if (aggr_con != NULL) {
      aggr_con->partial_aggr_value = TRUE;
      aggr_con->is_partially_initialized = TRUE;
    }  /* if */
  }  /* if */
}  /* aggr_init_class_remainder_if_needed */


static void check_flexible_array_init(an_init_component_ptr  icp,
                                      a_field_ptr            fp,
                                      an_init_state          *is)
/*
icp is an aggregate initializer component for a flexible array member fp in an
initialization described by *is.  Check if that situation is valid; if not,
issue an error or set is->init_error to TRUE (depending on other flags in *is).
*/
{
  if (microsoft_mode || (gcc_mode && is->static_lifetime_init)) {
    a_type_ptr  etype = underlying_array_element_type(fp->type);
    etype = skip_typerefs(etype);
    if (!C_mode() && is_immediate_class_type(etype)) {
      a_class_symbol_supplement_ptr  cssp = symbol_supplement_for_class(etype);
      if (has_nontrivial_destructor(cssp)) {
        /* Microsoft C++ allows the aggregate initialization of flexible array
           members only if they do not have nontrivial destructors. */
        if (is->no_diagnostics) {
          is->init_error = TRUE;
        } else {
          pos_error(ec_cannot_initialize_destructible_flexible_array,
                    init_component_pos(icp));
        }  /* if */
      }  /* if */
    } else if (gcc_mode && is->non_top_level_aggregate &&
               !(is_braced_init_component(icp) &&
                 icp->variant.braced.list == NULL)) {
      /* GNU C does not allow flexible array member initializers that are not
         at the top level, except if the initializer is empty.  For example:
           struct F { int n; int a[]; };
           struct T { struct F f; };
           T x1 = { { 1, {} } };     // Okay: non-top-level but empty.
           T x2 = { { 1, { 2 } } };  // Error.
      */
      if (is->no_diagnostics) {
        is->init_error = TRUE;
      } else {
        pos_error(ec_cannot_initialize_indirect_flexible_array,
                  init_component_pos(icp));
      }  /* if */
    }  /* if */
  } else if (is->no_diagnostics) {
    is->init_error = TRUE;
  } else {
    pos_error(gcc_mode ? ec_cannot_init_auto_flexible_array_member
                       : ec_cannot_initialize_flexible_array_member,
              init_component_pos(icp));
  }  /* if */
}  /* check_flexible_array_init */


static void aggr_init_field(an_init_component_ptr  *p_icp,
                            a_field_ptr            *p_field,
                            an_init_state          *is,
                            a_constant_ptr         aggr_con,
                            a_source_position      *diag_pos)
/*
*p_icp is non-NULL and describes an initializer for *p_field (also non-NULL).
Append a constant representing this initializer to the given aggregate constant
(aggr_con).  *is tracks state information for the complete initializer and
diag_pos is the position to use for diagnostics by default (if no more specific
position is available).
*/
{
  a_field_ptr            fp = *p_field;
  a_type_ptr             class_type = parent_class_of(fp), dtype = fp->type;
  a_boolean              ms_enum_bit_field = FALSE;
  an_init_component_ptr  icp = *p_icp;
  a_constant_ptr         elem_con;

  if (is->pack_expansion_handled) {
    /* If a pack expansion has been seen, we cannot match up types anymore.
       Change the destination type to "the unknown type". */
    dtype = type_of_unknown_templ_param_nontype;
  } else if (fp->is_bit_field) {
    if (microsoft_mode && !C_mode() && is_enum_type(dtype)) {
      /* Microsoft's C++ compiler allow bit fields of enumeration types to be
         initialized by integer values.  We emulate this by converting to the
         underlying integer type, and then converting the result back to the
         enumeration type. */
      dtype = integer_type(skip_typerefs(dtype)->variant.integer.int_kind);
      ms_enum_bit_field = TRUE;
    }  /* if */
  } else if ((fp->next == NULL || class_type->kind == (a_type_kind)tk_union) &&
             is_incomplete_array_type(fp->type)) {
    /* A flexible array member. */
    check_flexible_array_init(icp, fp, is);
  }  /* if */
  aggr_init_element(p_icp, dtype, is, diag_pos, &elem_con);
  if (ms_enum_bit_field) {
    /* A Microsoft enum bit field being initialized with an integer.
       Implicitly cast the result back to the enumeration type.
       (We use type_change_constant_full for the constant and add_cast for the
       non-constant case.) */
    if (elem_con->kind == (a_constant_repr_kind)ck_dynamic_init) {
      a_dynamic_init_ptr  dip = elem_con->variant.dynamic_init;
      check_assertion(dip->kind == (a_dynamic_init_kind)dik_expression);
      dip->variant.expression = add_cast(dip->variant.expression, fp->type);
      elem_con->type = dip->variant.expression->type;
    } else {
      a_boolean  did_not_fold = FALSE;
      type_change_constant_full(elem_con, fp->type,
                                /*is_implicit_cast=*/TRUE,
                                /*constant_context=*/FALSE,
                                /*evaluated_context=*/TRUE,
                                /*fold_constant_addr_exprs=*/FALSE,
                                /*check_cast_access=*/FALSE,
                                /*check_ambiguity=*/FALSE,
                                /*is_reinterpret_cast=*/FALSE,
                                /*maintain_expression=*/TRUE,
                                &did_not_fold,
                                /*error_detected=*/(an_error_code *)NULL,
                                init_component_pos(icp));
    }  /* if */
  }  /* if */
  if (!is->check_validity_only) {
    add_constant_to_aggregate(elem_con, aggr_con);
  }  /* if */
  if (class_type->kind == (a_type_kind)tk_union) {
    /* In the case of a union, only one field is usually initialized.
       (Designated initializers can override that.) */
    *p_field = NULL;
  } else if (!is->pack_expansion_handled) {
    *p_field = next_initializable_field(fp->next);
  }  /* if */
}  /* aggr_init_field */


static void make_designators_for_nested_anonymous_union(
                                                a_symbol_ptr    field_sym,
                                                a_type_ptr      *p_class_type,
                                                a_constant_ptr  *p_aggr_con)
/*
field_sym represents a field in an anonymous union: That field was named with
a designator appearing in an initializer *p_aggr_con whose type *p_class_type
is not the anonymous union itself.  For example:

  struct S { int i; struct { union { int d; }; }; } s = { .d = 3 };

Here struct S is passed for *p_class_type.
Create anonymous designators representing the "navigation" to the given field.
In the example, let __S denote the anonymous struct and __U denote the
anonymous union, then the original *p_aggr_con is modified as follows:

  <original *p_aggr_con, ck_aggregate>
    <ck_designator for anonymous field of type __S>
    <ck_aggregate for anonymous field of type __S>
      <ck_designator for anonymous field of type __U>
      <ck_aggregate for anonymous field of type __U; new *p_aggr_con>

The aggregate constant representing the innermost anonymous union is returned
through *p_aggr_con (and its type through *p_class_type).
*/
{
  a_type_ptr      orig_class_type = skip_typerefs(*p_class_type);
  a_constant_ptr  orig_aggr_con = *p_aggr_con, des_con = NULL, au_con;

  /* Move up the anonymous union parent object chain and create a designator
     constant and corresponding aggregate constant for each one. */
  while (field_sym->variant.field.anonymous_parent_object != NULL) {
    a_field_ptr     field = field_sym->variant.field.ptr;
    a_type_ptr      anon_parent = parent_class_of(field);
    if (same_entities(anon_parent, orig_class_type)) break;
    au_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
    au_con->type = anon_parent;
    if (des_con == NULL) {
      /* We just created the bottom-most aggregate: Record it as the new
         aggregate in which to place the named designator. */
      *p_aggr_con = au_con;
    } else {
      /* We created a designator->anonymous-union-constant pair in a previous
         iteration.  Append it to the parent anonymous union. */
      add_constant_to_aggregate(des_con, au_con);
      if (anon_parent->kind != (a_type_kind)tk_union) {
        au_con->is_partially_initialized = TRUE;
      }  /* if */
      add_constant_to_aggregate(des_con->next, au_con);
    }  /* if */
    /* Create the designator for the next level out. */
    des_con = alloc_constant((a_constant_repr_kind)ck_designator);
    des_con->variant.designator.field =
          field_sym->variant.field.anonymous_parent_object->variant.field.ptr;
    des_con->next = au_con;
    field_sym = field_sym->variant.field.anonymous_parent_object;
  }  /* while */
  /* This routine is only called for fields in anonymous unions that are not
     the original class type.  So we must have iterated at least once. */
  check_assertion(des_con != NULL);
  add_constant_to_aggregate(des_con, orig_aggr_con);
  if (orig_class_type->kind != (a_type_kind)tk_union) {
    orig_aggr_con->is_partially_initialized = TRUE;
  }  /* if */
  add_constant_to_aggregate(des_con->next, orig_aggr_con);
}  /* make_designators_for_nested_anonymous_union */


static void aggr_init_field_designator(an_init_component_ptr  *p_icp,
                                       a_type_ptr             class_type,
                                       an_init_state          *is,
                                       a_field_ptr            *field,
                                       a_constant_ptr         aggr_con,
                                       a_source_position      *diag_pos)
/*
*p_icp points to a designator component encountered while processing a braced
initializer for class_type.  Check if the designator is valid, and, if so,
append a matching ck_designator constant to aggr_con.  This routine also
consumes initializer components up to and including a non-designator (and
*p_icp is updated to point to the component after that, or NULL if there is
none).  diag_pos is the position at which to issue diagnostics if no more
specific position is available.
*/
{
  a_boolean              okay;
  an_init_component_ptr  icp = *p_icp;
  a_type_ptr             class_to_look_in = class_type;
  a_symbol_locator       loc;
  a_symbol_ptr           sym;

  if (!C_mode()) {
    /* If we're in an anonymous union, look for the field in the enclosing
       class scope. */
    a_class_type_supplement_ptr  ctsp = class_type_supp(class_to_look_in);
    while (ctsp->anonymous_union_kind == (an_anonymous_union_kind)auk_field) {
      class_to_look_in = parent_class_of(class_to_look_in);
      ctsp = class_type_supp(class_to_look_in);
    }  /* while */
#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
  } else if (class_to_look_in
                 ->variant.class_struct_union.is_nonstd_anonymous_union_type) {
    /* Nonstandard anonymous-union-like constructs are possible in some C
       modes, but no meaningful "parent" structure is available in that case.
       *is therefore records the last traversed class that is not a nonstandard
       anonymous union type. */
    class_to_look_in = is->class_to_look_in;
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */
  }  /* if */
  if (icp->variant.designator.field_name == NULL) {
    /* This is not a field designator, but we're in a class initializer.
       Issue an error. */
    okay = FALSE;
    pos_error(ec_invalid_designator_kind, init_component_pos(icp));
  } else {
    clear_locator(&loc, init_component_pos(icp));
    loc.symbol_header = icp->variant.designator.field_name;
    sym = class_qualified_id_lookup(&loc, class_to_look_in, IDL_NO_OPTIONS);
    if (sym == NULL) {
      /* The name was not found. */
      okay = FALSE;
      pos_stsy_error(ec_not_a_field, init_component_pos(icp),
                     loc.symbol_header->identifier, symbol_for(class_type));
    } else if (!symbol_is(sym, sk_field)) {
      /* The name was found, but it's not a field. */
      okay = FALSE;
      pos_st_error(ec_not_a_field_name, init_component_pos(icp),
                   loc.symbol_header->identifier);
      check_assertion(!C_mode());
    } else {
      okay = TRUE;
      *field = sym->variant.field.ptr;
      if (sym->variant.field.anonymous_parent_object != NULL) {
        /* This field is a member of an anonymous union or (nonstandard)
           anonymous struct. */
        a_type_ptr  anon_parent = parent_class_of(*field);
        if (!same_entities(anon_parent, class_type)) {
          /* The anonymous union does not correspond to the current aggregate
             constant.  GNU C++ and earlier versions of GNU C do not permit
             this.
               struct S { struct { int i; float f; }; };
               struct S s1 = {{ .i = 1 }};  // Accepted by GCC
               struct S s2 = { .i = 1 };    // Sometimes an error.
             In modes where it is permitted, we must generate
             anonymous designators to navigate the aggregate structure. */
          if (!C_mode() || (gcc_mode && gnu_version < 40600)) {
            okay = FALSE;
            pos_error(ec_indirect_anon_union_designator,
                      init_component_pos(icp));
          } else {
            make_designators_for_nested_anonymous_union(sym, &class_type,
                                                        &aggr_con);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (!C_mode() && okay &&
      !class_type->variant.class_struct_union.is_nonreal_class &&
      !symbol_supplement_for_class(class_type)->is_POD) {
    /* Allowing designators in non-POD types would raise subtle questions about
       order of initialization and destruction.  For now, at least, we disallow
       such constructs.  (The error is only issued on the first designator if
       there is a sequence of consecutive designators.) */
    pos_error(ec_designator_for_non_POD, init_component_pos(icp));
  }  /* if */
  icp = icp->next;
  if (okay) {
    /* Designators complicate the determination of whether an aggregate
       initializer completely covers the target entity.  Assume partial
       initialization by default (in non-unions). */
    if (class_type->kind != (a_type_kind)tk_union) {
      is->partial_initializer = TRUE;
    }  /* if */
    if (!is->check_validity_only) {
      a_constant_ptr  des_con;
      des_con = alloc_constant((a_constant_repr_kind)ck_designator);
      des_con->variant.designator.field = *field;
      des_con->source_corresp.decl_position = *init_component_pos(*p_icp);
      add_constant_to_aggregate(des_con, aggr_con);
      if (class_type->kind != (a_type_kind)tk_union) {
        aggr_con->is_partially_initialized = TRUE;
      }  /* if */
    }  /* if */
    if (icp != NULL) {
      /* Process the component following this designator.  If it is another
         designator (i.e., a "chained" designator), special care must be taken
         to go down a level in the aggregate structure. */
      if (is_designator_component(icp)) {
        /* A chained designator follows (e.g., ".x.y =" or ".x[n] ="). */
        a_constant_ptr  next_con;
        if (((*field)->next == NULL ||
             class_type->kind == (a_type_kind)tk_union) &&
            is_incomplete_array_type((*field)->type)) {
          /* A flexible array member. */
          check_flexible_array_init(icp, *field, is);
        }  /* if */
        aggr_init_chained_designator(&icp, (*field)->type, is, &next_con);
        *field = (*field)->next;
        if (next_con == NULL) {
          check_assertion(is->init_error);
        } else if (!is->check_validity_only) {
          add_constant_to_aggregate(next_con, aggr_con);
        }  /* if */
      } else {
        aggr_init_field(&icp, field, is, aggr_con, diag_pos);
      }  /* if */
    }  /* if */
  } else {
    /* The designator was invalid.  Subsequent initializer components are
       likely not going to match up with the destination type.  So for error
       recovery purposes, skip remaining initializer components. */
    icp = NULL;
    is->init_error = TRUE;
  }  /* if */
  *p_icp = icp;
}  /* aggr_init_field_designator */


static void aggr_init_class(an_init_component_ptr  *p_icp,
                            a_type_ptr             class_type,
                            an_init_state          *is,
                            a_source_position      *diag_pos,
                            a_constant_ptr         *init_con)
/*
Produce an aggregate constant (in *init_con) for the initialization of an
object or subobject of the aggregate class type given by class_type.  The
initializer is described by *p_icp, and that value is updated to the next
initializer to be considered by the caller (if the initializer is braced, just
one initializer is "consumed", but otherwise several may be used depending on
the number of fields being initialized).  *is describes the initialization as
a whole, and diag_pos indicates the position at which diagnostics should be
issued if no more specific position is available.
*/
{
  an_init_component_ptr  icp = *p_icp;

  class_type = skip_typerefs(class_type);
  check_assertion(is_immediate_class_type(class_type));
  if (is_expression_component(icp) && (!C_mode() || c99_mode || gcc_mode) &&
      try_whole_aggr_class_init(p_icp, class_type, is, init_con)) {
    /* Even though class type is an aggregate class, it is completely
       initialized by the expression represented by icp.  E.g.:
         struct S { int i; } s, as[]{ 1, s };
       Here the use of s as a component wholly initializes as[1]. */
  } else {
    a_field_ptr  fp = class_type->variant.class_struct_union.field_list;
    a_boolean    braced = is_braced_init_component(icp),
                 saved_pack_expansion_handled;
#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
    a_type_ptr   saved_class_to_look_in = is->class_to_look_in;
    if (!class_type
                 ->variant.class_struct_union.is_nonstd_anonymous_union_type) {
      is->class_to_look_in = class_type;
    }  /* if */
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */
    /* Skip unnamed bit fields. */ 
    fp = next_initializable_field(fp);
    if (is->check_validity_only) {
      *init_con = NULL;
    } else {
      *init_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
      (*init_con)->type = class_type;
      (*init_con)->source_corresp.decl_position = *init_component_pos(icp);
#if EXTRA_SOURCE_POSITIONS_IN_IL
      if (!is_designator_component(icp)) {
        (*init_con)->end_position = *init_component_end_pos(icp);
      }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      (*init_con)->explicit_braces_on_aggregate = braced;
    }  /* if */
    if (braced) {
      /* The element values are enclosed in braces. */
      /* Diagnostics not associated with a particular element should be issued
         on the closing brace. */
      diag_pos = &icp->variant.braced.end_pos;
      /* Unwrap the braced list for the processing that follows. */
      icp = icp->variant.braced.list;
      if (icp == NULL && C_mode() && !gcc_mode) {
        /* Empty initializer lists are not permitted in C mode (except GNU C
           mode). */
        pos_error(ec_exp_primary_expr, diag_pos);
      }  /* if */
      /* Save the pack-expansion-handled state: Any expansions seen have an
         effect only within the braces. */
      saved_pack_expansion_handled = is->pack_expansion_handled;
    } else if (fp == NULL) {
      /* No braces and no fields, but another initializer: This is an error.
         E.g.:
           struct E {};
           struct A { E e; int x; } a{ 1 };  // Error: a.e uninitialized.
      */
      if (gcc_mode) {
        /* GCC accepts this with a warning about excess initializers. */
        pos_warning(ec_excess_initializers_ignored, init_component_pos(icp));
        icp = NULL;
      } else {
        /* In non-GCC modes, we treat it as an attempt to do a "whole aggregate
           class initialization".  It is essential to move forward in the list
           of initializers to avoid non-terminating loops.  */
        a_constant_ptr  empty_con = NULL;
        aggr_init_simple_element(&icp, class_type, is, &empty_con);
        /* The call to aggr_init_simple_element must have resulted in an error,
           since we previously determined that whole aggregate class
           initialization is not possible. */
        if (is->no_diagnostics) {
          check_assertion(is->init_error);
        } else {
          check_assertion(total_errors != 0);
        }  /* if */
      }  /* if */
    } else if (!is->elided_braces_allowed) {
      /* Braces were elided at this level, but this is not a context that
         permits such elision.  (We don't issue an error if another error has
         already been issued for this initialization.) */
      if (!is->no_diagnostics && !is->init_error) {
        pos_error(ec_cannot_elide_braces, init_component_pos(icp));
      }  /* if */
      is->init_error = TRUE;
    }  /* if */
    while (icp != NULL) {
      if (is_designator_component(icp)) {
        /* One or more designators. */
        if (!braced && !is->chained_designator_okay) {
          /* The designator doesn't apply at this level.  Return to a previous
             level. */
          break;
        } else {
          is->chained_designator_okay = FALSE;
          aggr_init_field_designator(&icp, class_type, is, &fp, *init_con,
                                     diag_pos);
        }  /* if */
      } else if (fp != NULL) {
        /* A field is available for the next initializer component. */
        aggr_init_field(&icp, &fp, is, *init_con, diag_pos);
      } else {
        /* No more fields to initialize. */
        break;
      }  /* if */
    }  /* while */
    if (fp != NULL && !is->pack_expansion_handled) {
      /* Not all class fields are explicitly initialized: Append entries to
         initialize remaining fields if appropriate. */
      aggr_init_class_remainder_if_needed(*init_con, class_type, fp, is,
                                          diag_pos);
    }  /* if */
    if (braced) {
      /* The caller should move on to the component that follows the braced
         list (if any). */
      *p_icp = (*p_icp)->next;
      if (icp != NULL) {
        /* Initializers remain at this level, but no fields. */
        check_assertion(fp == NULL);
        if (is->no_diagnostics) {
          is->init_error = !gcc_mode;
        } else if (gcc_mode) {
          /* GNU C (but not GNU C++) ignores extraneous initializers here. */
          pos_warning(ec_excess_initializers_ignored, init_component_pos(icp));
        } else {
          pos_error(ec_too_many_initializer_values, init_component_pos(icp));
          is->init_error = TRUE;
        }  /* if */
      }  /* if */
      is->pack_expansion_handled = saved_pack_expansion_handled;
    } else {
      /* Braces were omitted at this level of aggregate initialization: The
         the caller should continue associating the next component with any
         aggregate elements that follow those consumed here. */
      *p_icp = icp;
    }  /* if */
#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
    is->class_to_look_in = saved_class_to_look_in;
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */
  }  /* if */
}  /* aggr_init_class */


static void aggr_init_chained_designator(an_init_component_ptr  *p_icp,
                                         a_type_ptr             aggr_type,
                                         an_init_state          *is,
                                         a_constant_ptr         *result)
/*
*p_icp represents a "chained" designator; i.e., a designator immediately
following another designator.  For example, ".i" and "[3]" in ".x.i[3]".
aggr_type is the type into which the designator refers.  Call aggr_init_class
or aggr_init_array at the level that the designator applies to, or issue a
diagnostic if aggr_type is not a type in which designators can be used.  The
designated constant, if any, is produced in *result and *p_icp is updated to
point to the component following the designated component (or NULL if no more
components follow at the current level).
*/
{
  an_init_component_ptr  icp = *p_icp;
  a_boolean              saved_non_top_level_aggregate =
                                                  is->non_top_level_aggregate;

  check_assertion(icp != NULL && is_designator_component(icp));
  *result = NULL;
  is->non_top_level_aggregate = TRUE;
  if (is_class_struct_union_type(aggr_type)) {
    /* Call aggr_init_class but set a flag in the initialization state to
       accept the upcoming designator even though there are no braces around
       the subaggregate constant. */
    is->chained_designator_okay = TRUE;
    aggr_init_class(&icp, aggr_type, is, init_component_pos(icp),
                    result);
  } else if (is_array_type(aggr_type)) {
    /* Call aggr_init_array but set a flag in the initialization state to
       accept the upcoming designator even though there are no braces around
       the subaggregate constant. */
    is->chained_designator_okay = TRUE;
    aggr_init_array(&icp, &aggr_type, is, init_component_pos(icp), result);
  } else if (is_template_param_type(aggr_type)) {
    /* We cannot represent designators in nonreal types.  This was diagnosed
       earlier in a way that should prevent us from getting here. */
    unexpected_condition();
  } else {
    /* Not a type for which designators are valid. */
    is->init_error = TRUE;
    if (!is->no_diagnostics) {
      pos_ty_error(ec_designator_requires_aggregate_type,
                   init_component_pos(icp), aggr_type);
    }  /* if */
    icp = skip_designators(icp);
  }  /* if */
  *p_icp = icp;
  is->non_top_level_aggregate = saved_non_top_level_aggregate;
}  /* aggr_init_chained_designator */


static void aggr_init_element(an_init_component_ptr  *p_icp,
                              a_type_ptr             etype,
                              an_init_state          *is,
                              a_source_position      *diag_pos,
                              a_constant_ptr         *init_con)
/*
Handle the initialization of an element of type etype of an aggregate by the
component *p_icp (and potentially, the components that follow *p_icp); return
in *p_icp the next item not used for this initialization (NULL if there are no
more such items).  Return the result in *init_con.  *is describes the
initialization as a whole, and diag_pos indicates the position at which
diagnostics should be issued if no more specific position is available. 
If this element is itself an aggregate, then this routine recurses into
aggr_init_array or aggr_init_class, to produce a ck_aggregate constant.
*/
{
  an_init_component_ptr  icp = *p_icp;
  a_boolean              pack_expansion = FALSE;
  a_type_kind            etype_kind;
  a_boolean              saved_non_top_level_aggregate
                                                 = is->non_top_level_aggregate;
  struct an_arg_match_summary
                         *saved_arg_match = is->arg_match;

  check_assertion(init_con != NULL);
  if (is_pack_expansion_component(icp)) {
    /* If this component is a pack expansion, don't attempt to match up types
       since we don't know how many elements it should match. */
    etype = type_of_unknown_templ_param_nontype;
    pack_expansion = is->pack_expansion_handled = TRUE;
  } else if (gpp_mode && is_prototype_instantiation_context() &&
             is_class_struct_union_type(etype)) {
    /* GCC doesn't attempt to match the initializer to the class type in
       template definitions, even if the type is fully known (i.e.,
       nondependent). */
    etype = type_of_unknown_templ_param_nontype;
  }  /* if */
  etype_kind = skip_typerefs(etype)->kind;
  if (etype_kind == (a_type_kind)tk_array) {
    /* Array. */
    is->non_top_level_aggregate = TRUE;
    is->arg_match = NULL;
    aggr_init_array(p_icp, &etype, is, diag_pos, init_con);
  } else if (is_aggregate_type(etype)) {
    /* Aggregate class (since the array case was already tested for). */
    is->non_top_level_aggregate = TRUE;
    is->arg_match = NULL;
    aggr_init_class(p_icp, etype, is, diag_pos, init_con);
  } else if (etype_kind == (a_type_kind)tk_template_param ||
             etype_kind == (a_type_kind)tk_error) {
    /* Create a constant that matches the initializer structure (since the
       element structure is not a priori known). */
    is->non_top_level_aggregate = TRUE;
    is->arg_match = NULL;
    aggr_init_generic_element(icp, etype, is, init_con);
    *p_icp = icp->next;
#if GNU_VECTOR_TYPES_ALLOWED
  } else if (etype_kind == (a_type_kind)tk_vector &&
             (gpp_mode || (gcc_mode && gnu_version >= 40500) ||
              is_braced_init_component(icp))) {
    /* A braced component can initialize the elements of a GNU vector
       individually.  GCC also allows brace elision, except in early C
       compilers. */
    is->non_top_level_aggregate = TRUE;
    is->arg_match = NULL;
    aggr_init_vector(p_icp, etype, is, diag_pos, init_con);
#endif /* GNU_VECTOR_TYPES_ALLOWED */
#if C99_IL_EXTENSIONS_SUPPORTED
  } else if (gpp_mode && gnu_version >= 40700 &&
             etype_kind == (a_type_kind)tk_complex &&
             is_braced_init_component(icp) &&
             icp->variant.braced.list != NULL &&
             icp->variant.braced.list->next) {
    /* g++ 4.7 introduced the possibility of initializing the real and
       imaginary components of a built-in "complex" object with aggregate
       initialization syntax.  Cases with empty braces or singleton braces
       remain simple initializations and therefore fall through to the
       default case). */
    aggr_init_complex(p_icp, etype, is, init_con);
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (cppcli_enabled && is_braced_init_component(icp) &&
             is_handle_type(etype) &&
             (is_cli_array_type(type_pointed_to(etype)) ||
              is_template_param_or_nonreal_class_type(
                                                  type_pointed_to(etype)))) {
    a_dynamic_init_ptr  cli_array_dip;
    aggr_init_cli_array_with_alloc(icp, etype, is, &cli_array_dip);
    if (is->check_validity_only) {
      *init_con = NULL;
    } else {
      *init_con = alloc_constant((a_constant_repr_kind)ck_dynamic_init);
      (*init_con)->variant.dynamic_init = cli_array_dip;
      (*init_con)->type = etype;
      (*init_con)->source_corresp.decl_position = *init_component_pos(icp);
#if EXTRA_SOURCE_POSITIONS_IN_IL
      if (!is_designator_component(icp)) {
        (*init_con)->end_position = *init_component_end_pos(icp);
      }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    }  /* if */
    *p_icp = icp->next;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else {
    /* Use the single value in *p_icp to initialize one element. */
    aggr_init_simple_element(p_icp, etype, is, init_con);
  }  /* if */
  if (!is->check_validity_only) {
    check_assertion(*init_con != NULL);
    (*init_con)->is_pack_expansion = pack_expansion;
  }  /* if */
  is->non_top_level_aggregate = saved_non_top_level_aggregate;
  is->arg_match = saved_arg_match;
}  /* aggr_init_element */


static void prep_initializer_result(an_init_state  *is,
                                    a_routine_ptr  dtor_rp)
/*
The given init state describes an initializer that has been processed: It is
either an error state, or it points to a constant or dynamic init entry.
dtor_rp is the destructor needed to destroy the initializer value (or NULL if
none is needed).
Ensure that is->init_con or is_init_dip is non-NULL as appropriate given *is
and dtor_rp.  In particular, create a dynamic init entry for the initialization
if any of the flags is->has_dynamic_init_component or is->force_dynamic_init
are TRUE.
*/
{
  if (is->init_dip == NULL) {
    a_dynamic_init_kind  dik = (a_dynamic_init_kind)dik_constant;
    if (is->init_con == NULL) {
      check_assertion(is->init_error);
      is->init_con = alloc_error_constant();
    }  /* if */
    if (dtor_rp != NULL) is->has_dynamic_init_component = TRUE;
    if (is->has_dynamic_init_component &&
        is->init_con->kind == (a_constant_repr_kind)ck_aggregate) {
      dik = (a_dynamic_init_kind)dik_nonconstant_aggregate;
    }  /* if */
    if (is->has_dynamic_init_component || is->force_dynamic_init) {
      is->init_dip = alloc_dynamic_init(dik);
      is->init_dip->variant.constant = is->init_con;
      is->init_dip->destructor = dtor_rp;
      is->init_dip->is_braced_initializer = TRUE;
      is->init_dip->is_partially_initialized = is->partial_initializer;
      is->init_con = NULL;
    }  /* if */
  }  /* if */
}  /* prep_initializer_result */


void prep_aggr_initializer(an_init_component_ptr        icp,
                           a_type_ptr                   *p_type,
                           an_init_state                *is,
                           struct an_arg_match_summary  *arg_match,
                           a_boolean                    fill_in_dtor)
/*
Convert an initializer value represented by icp to the aggregate type *p_type
of the entity being initialized.  The result is returned through *is (in
particular, is->init_con and is->init_dip).  If fill_in_dtor is TRUE a
destructor will be added to the dynamic initialization if one is needed, but
the dynamic init will not be placed on any object lifetime list (the caller
must do that).  If the entity being initialized is an unknown-bound array,
*p_type will be updated to the complete array type matching the number of
elements initialized.  arg_match points to a structure used by expression
processing to track the worst argument match during overload resolution;
the type pointed to is opaque to declaration processing.
*/
{
  a_source_position_ptr  diag_pos = init_component_pos(icp);
  a_routine_ptr          dtor_rp = NULL;
  a_type_ptr             dtype = *p_type;
  a_boolean              saved_force_dynamic_init = is->force_dynamic_init;
  a_boolean              unknown_bound_array;
  struct an_arg_match_summary
                         *saved_arg_match = is->arg_match;

  is->arg_match = arg_match;
  check_assertion(!C_mode());
  is->init_con = NULL;
  is->init_dip = NULL;
  /* The force_dynamic_init flag only applies to the top-level result. */
  is->force_dynamic_init = FALSE;
  /* C++11 requires a diagnostic on narrowing in these cases, but in nonstrict
     modes we only make it a warning to permit the conversions traditionally
     allowed in C-style aggregate initializations.  That also matches the
     behavior of newer versions of GCC. */
  if (strict_ansi_mode || (gpp_mode && gnu_version < 40700)) {
    is->error_on_narrowing = TRUE;
  } else {
    is->warning_on_narrowing = TRUE;
  }  /* if */
  dtype = skip_typerefs(dtype);
  switch (dtype->kind) {
    case tk_error:
    case tk_template_param:
      /* Unknown destination type: Create an aggregate constant that follows
         the source form. */
      aggr_init_generic_element(icp, dtype, is, &is->init_con);
      break;
    case tk_array:
      /* Arrays are aggregates. */
      unknown_bound_array = is_incomplete_array_type(dtype);
      is->initializer_can_dimension_array = TRUE;
      aggr_init_array(&icp, &dtype, is, diag_pos, &is->init_con);
      if (is_error_type(dtype)) {
        is->init_error = TRUE;
        if (!is->no_diagnostics) expect_error();
      } else {
        a_type_ptr  etype = underlying_array_element_type(dtype);
        etype = skip_typerefs(etype);
        if (fill_in_dtor && is_immediate_class_type(etype)) {
          dtor_rp = get_init_destructor(etype, is, diag_pos);
        }  /* if */
        if (unknown_bound_array && is->init_con != NULL) {
          /* The destination type is incomplete, but the initializer constant
             should reflect the actual number of elements. */
          is->init_con->type = dtype;
          if (is->variable_size_array) {
            /* In a new-expression like "new T[n]{1, 2}", n could be larger
               than the number of elements in the initializer.  It is therefore
               incorrect to replace the incomplete array type *p_type by dtype.
               Instead, we assume that the initializer doesn't cover the whole
               array. */
            is->partial_initializer = TRUE;
          } else {
            /* Update the destination entity's type to reflect the number of
               elements in the initializer. */
            if (!gnu_mode && is_array_type(dtype) &&
                skip_typerefs(dtype)->variant.array.bound_is_zero) {
              /* Zero-length initializers are only allowed in GNU mode in this
                 context. */
              if (!is->no_diagnostics) {
                pos_error(ec_bad_initializer_for_array_with_unspecified_bound,
                          diag_pos);
              }  /* if */
              is->init_error = TRUE;
              *p_type = error_type();
            } else {
              *p_type = dtype;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
      break;
#if GNU_VECTOR_TYPES_ALLOWED
    case tk_vector:
      aggr_init_vector(&icp, dtype, is, diag_pos, &is->init_con);
      if (arg_match != NULL) record_aggr_init_match(arg_match);
      break;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
    case tk_class:
    case tk_struct:
    case tk_union:
      check_assertion(is_aggregate_type(dtype));
      if (fill_in_dtor) {
        dtor_rp = get_init_destructor(dtype, is, diag_pos);
      }  /* if */
      aggr_init_class(&icp, dtype, is, diag_pos, &is->init_con);
      if (arg_match != NULL) record_aggr_init_match(arg_match);
      break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_complex:
      if (gpp_mode && gnu_version >= 40700 &&
          icp->variant.braced.list != NULL &&
          icp->variant.braced.list->next) {
        /* g++ 4.7 introduced the possibility of initializing the real and
           imaginary components of a built-in "complex" object with aggregate
           initialization syntax.  Cases with empty braces or singleton
           braces remain simple initializations and therefore fall through
           to the default case). */
        aggr_init_complex(&icp, dtype, is, &is->init_con);
        break;
      }  /* if */
      /*FALLTHROUGH*/
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    default:
      unexpected_condition();
  }  /* switch */
  is->force_dynamic_init = saved_force_dynamic_init;
  if (!is->check_validity_only) {
    /* Ensure is->init_con and is->init_dip are set properly. */
    prep_initializer_result(is, dtor_rp);
  }  /* if */
  if (is->any_uninitialized_const_or_ref_member && !is->init_error) {
    /* A const or reference field was not initialized.  Issue a diagnostic,
       except in unions. */
    if (!is_union_type(dtype)) {
      if (is->no_diagnostics) {
        is->init_error = TRUE;
      } else {
        pos_error(ec_unnamed_object_with_uninitialized_field, diag_pos);
      }  /* if */
    }  /* if */
  }  /* if */
  is->arg_match = saved_arg_match;
}  /* prep_aggr_initializer */


static void process_simple_init_component(an_init_component_ptr  icp,
                                          a_type_ptr             dtype,
                                          an_init_state          *is,
                                          a_boolean              is_var_init)
/*
icp is an init component for a scalar type dtype (it may or may not be braced).
*is describes the associated initialization.  Process the component, returning
any results through *is.  is_var_init is TRUE if the initializer is for a
variable initialization.
*/
{
  an_init_component_ptr  icp2 = NULL, icp3;
  an_init_state          saved_is, is2;

  if (is_braced_init_component(icp)) {
    if (icp->variant.braced.list == NULL) {
      if (!list_init_enabled) {
        /* Empty braces initializing a scalar are a C++11 list initialization
           feature. */
        pos_error(ec_exp_primary_expr, &icp->variant.braced.end_pos);
      }  /* if */
    } else if (microsoft_bugs && microsoft_version < 1310 && is_var_init &&
               icp->variant.braced.list->next != NULL) {
      /* Earlier microsoft compilers accept e.g.  "int x = { f(), { 3 } }".
         The last value replaces previous ones (though side-effects take
         place), unless they're both constants and x is not automatic. */
      icp = icp->variant.braced.list;
      icp2 = icp->next;
      icp->next = NULL;
    }  /* if */
  }  /* if */
  /* The following initialization of saved_is is done unconditionally to avoid
     a spurious warning by Microsoft compilers. */
  saved_is = *is;
  convert_initializer(icp, dtype, is_var_init, /*fill_in_dtor=*/TRUE, is);
  while (icp2 != NULL) {
    /* If there are more components following the second one, detach them:
       They'll be handled in subsequent iterations. */
    icp3 = icp2->next;
    icp2->next = NULL;
    /* Handle the extra components recursively. */
    is2 = saved_is;
    process_simple_init_component(icp2, dtype, &is2, is_var_init);
    if (is->static_lifetime_init &&
        is->init_con != NULL && is2.init_con != NULL) {
      /* Approximately emulate the Microsoft behavior that if only true
         constants are involved, the first value is kept for variables with
         static lifetime (i.e., is2 is discarded).  The emulation is not
         perfect when more nesting is involved; for example in
         "int x = { f(), { 1, { 2 }}};". */
    } else if (!is->check_validity_only &&
               !is->init_error && !is2.init_error) {
      /* Combine the effects of the initializers, retaining the value of the
         second one. */
      combine_initializers(is->init_con, is->init_dip,
                           is2.init_con, is2.init_dip);
      *is = is2;
      /* If the combined initializer is non-constant, keep using the
         dynamic-initializer representation. */
      if (is->init_dip != NULL) {
        is->init_con = NULL;
      } else if (is->init_con->kind == (a_constant_repr_kind)ck_dynamic_init) {
        is->init_dip = is->init_con->variant.dynamic_init;
        is->init_con = NULL;
      } else {
        is->init_dip = NULL;
      }  /* if */
      check_assertion(is->init_con != NULL || is->init_dip != NULL);
    }  /* if */
    /* Restore the component chain so it can be freed by the caller. */
    icp->next = icp2;
    /* Move to the next component (if any). */
    icp = icp2;
    icp2 = icp3;
  }  /* while */
}  /* process_simple_init_component */


static void braced_initializer(a_type_ptr          dtype,
                               an_init_component   *rescan_aggr,
                               an_init_state       *is,
                               a_decl_parse_state  *dps,
                               an_init_component   **return_icp,
                               a_source_position   *diag_pos)
/*
Handle a braced-init-list following a declarator, a mem-initializer-id, or a
new-type-id, as well as the braced construct in a C99-style compound literal.
If the direct_init flag in the initialization state is TRUE, the initializer
uses direct initialization syntax (e.g., "T x{3};"); otherwise, it uses copy
initialization syntax (e.g., "T x = {3};").
dtype is the type of the entity being initialized.  *is describes the state of
initializer processing.  *dps describes the declaration that the initializer
is part of; it is NULL if the initialization is not (directly) part of a
declaration.  diag_pos is the position to be used by default for diagnostics.  
If return_icp is non-NULL, return the init-component entry for the
braced-init-list in *return_icp instead of freeing it as usual.  If
rescan_aggr is non-NULL, a rescan is being done during template
deduction; rescan_aggr provides a braced-init-list for the
initializer, already copied and substituted.
*/
{
  an_init_component_ptr  icp_tree, icp;
  a_boolean              is_aggregate = FALSE, is_var_init;
  a_boolean              saved_force_dynamic_init = is->force_dynamic_init;
  a_boolean              saved_no_diagnostics = is->no_diagnostics;
  a_type_ptr             atype;
  a_routine_ptr          dtor_rp = NULL;
  a_boolean              need_to_free_icp_tree = FALSE;

  check_assertion(rescan_aggr != NULL || curr_token == tok_lbrace ||
                  anything_cached(&dps->prescanned_initializer_cache));
  dtype = skip_typerefs(dtype);
  if (rescan_aggr != NULL) {
    /* Rescan.  The {...} is provided by the caller in init-component form. */
    is->no_diagnostics = TRUE;
    icp_tree = rescan_aggr;
  } else {
    /* Parse the list structure (which may be nested and therefore really a
       tree structure). */
    icp_tree = scan_braced_init_list(is->elements_are_full_expressions, dps);
    need_to_free_icp_tree = TRUE;
  }  /* if */
  icp = icp_tree;
  check_assertion(icp != NULL && is_braced_init_component(icp));
  is_var_init = dps != NULL && dps->sym != NULL &&
                var_for_symbol(dps->sym) != NULL;
  /* The force_dynamic_init flag only applies to the top-level result. */
  is->force_dynamic_init = FALSE;
  switch (dtype->kind) {
    case tk_error:
    case tk_template_param:
      /* Unknown destination type: Create an aggregate constant that follows
         the source form. */
      is_aggregate = TRUE;
      aggr_init_generic_element(icp, dtype, is, &is->init_con);
      break;
    case tk_array:
      /* Arrays are aggregates. */
      is_aggregate = TRUE;
      atype = dtype;
      aggr_init_array(&icp, &atype, is, diag_pos, &is->init_con);
      if (atype != dtype) {
        /* Presumably an incomplete array type whose length is now known.
           Update the recorded type. */
        check_assertion(dps != NULL);
        dps->type = atype;
      }  /* if */
      if (is_error_type(atype)) {
        is->init_error = TRUE;
        if (!is->no_diagnostics) expect_error();
      } else {
        a_type_ptr  etype = underlying_array_element_type(atype);
        etype = skip_typerefs(etype);
        if (is_immediate_class_type(etype)) {
          dtor_rp = get_init_destructor(etype, is, diag_pos);
        }  /* if */
      }  /* if */
      break;
#if GNU_VECTOR_TYPES_ALLOWED
    case tk_vector:
      /* A GNU vector type. */
      is_aggregate = TRUE;
      aggr_init_vector(&icp, dtype, is, diag_pos, &is->init_con);
      break;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
    case tk_class:
    case tk_struct:
    case tk_union:
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (cppcli_enabled && is_value_class_type(dtype) &&
          is_cli_generic_definition_argument_type(dtype)) {
        /* A constraint type can be a value class type, but should not be
           treated as an aggregate type since its subobject structure is not
           known.  E.g.:
             generic<class T> where T: value class
             void f(T x) { T y = { x }; }  // Treat as a simple initialization;
                                           // not as aggregate initialization.
        */
        process_simple_init_component(icp, dtype, is, is_var_init);
      } else 
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      /* Do not insert code here. */
      if (dtype->variant.class_struct_union.is_nonreal_class ||
          (gpp_mode && is_prototype_instantiation_context())) {
        /* For nonreal classes, don't attempt to track the class structure.
           GCC doesn't appear to match the class structure in any prototype
           instantiation context (even if the type being initialized is non-
           dependent). */
        is_aggregate = TRUE;
        aggr_init_generic_element(icp, dtype, is, &is->init_con);
      } else {
        a_class_symbol_supplement_ptr
              cssp = symbol_for(dtype)->variant.class_struct_union.extra_info;
        if (cssp->is_class_aggregate) {
          /* A class aggregate requires aggregate initialization. */
          is_aggregate = TRUE;
          dtor_rp = get_init_destructor(dtype, is, diag_pos);
          aggr_init_class(&icp, dtype, is, diag_pos, &is->init_con);
        } else {
          /* Non-aggregate class type. */
          convert_initializer(icp, dtype, is_var_init, /*fill_in_dtor=*/TRUE,
                              is);
        }  /* if */
      }  /* if */
      break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_complex:
      if (gpp_mode && gnu_version >= 40700 &&
          icp->variant.braced.list != NULL &&
          icp->variant.braced.list->next) {
        /* g++ 4.7 introduced the possibility of initializing the real and
           imaginary components of a built-in "complex" object with aggregate
           initialization syntax.  Cases with empty braces or singleton
           braces remain simple initializations and therefore fall through
           to the default case). */
        is_aggregate = TRUE;
        aggr_init_complex(&icp, dtype, is, &is->init_con);
        break;
      }  /* if */
      /*FALLTHROUGH*/
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    default:
      /* Non-class, non-aggregate initialization. */
      process_simple_init_component(icp, dtype, is, is_var_init);
      break;
  }  /* switch */
  if (return_icp != NULL) {
    /* Return the init-component tree to the caller, as requested. */
    *return_icp = icp_tree;
    need_to_free_icp_tree = FALSE;
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (rescan_aggr == NULL) {
    curr_construct_end_position = *init_component_end_pos(icp_tree);
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (need_to_free_icp_tree) free_init_component_list(icp_tree);
  is->force_dynamic_init = saved_force_dynamic_init;
  if ((is_aggregate && !is->init_error) || is->force_dynamic_init) {
    /* The routines for aggregate initialization produce a constant entry, but
       those entries may embed a dynamic initialization.  If so, return a
       dynamic initialization entry for a nonconstant aggregate to the caller.
       Also produce a dynamic init entry if the caller requested it through the
       force_dynamic_init state flag. */
    prep_initializer_result(is, dtor_rp);
  }  /* if */
  if (is->any_uninitialized_const_or_ref_member && !is->init_error) {
    /* A const or reference field was not initialized.  Issue a diagnostic,
       except in unions. */
    if (!is_union_type(dtype)) {
      if (C_mode()) {
        pos_sy_warning(ec_var_with_uninitialized_field, diag_pos, dps->sym);
      } else if (is->no_diagnostics) {
        is->init_error = TRUE;
      } else if (is_var_init) {
        pos_sy_error(ec_var_with_uninitialized_member, diag_pos, dps->sym);
      } else {
        pos_error(ec_unnamed_object_with_uninitialized_field, diag_pos);
      }  /* if */
    }  /* if */
  }  /* if */
  is->no_diagnostics = saved_no_diagnostics;
}  /* braced_initializer */


#if !EXTRA_SOURCE_POSITIONS_IN_IL
/*ARGSUSED*/ /* decl_pos_block is not used in some configurations. */
#endif /* !EXTRA_SOURCE_POSITIONS_IN_IL */
static void brace_init_variable(a_decl_parse_state  *dps,
                                a_boolean           direct,
                                an_id_linkage_kind  linkage,
                                a_source_position   *diag_pos,
                                a_decl_pos_block    *decl_pos_block)
/*
Handle a braced-initializer following the declarator for a variable or static
data member.  If direct is TRUE, the initializer uses direct initialization
syntax (e.g., "T x{3};"); otherwise, it uses copy initialization syntax (e.g.,
"T x = {3};").
dps, linkage, and decl_pos_block describe the declaration that the initializer
is part of.  diag_pos is the position to be used by default for diagnostics
(when no more specific position is available).
*/
{
  a_variable_ptr  vp;

  check_assertion(dps != NULL && dps->sym != NULL);
  vp = var_for_symbol(dps->sym);
  check_assertion_or_expect_error(vp != NULL);
  if (direct) {
    if (!list_init_enabled) {
      /* Direct list initializers are not explicitly enabled. */
      if (gpp_mode) {
        /* GCC accepts some of these cases in non-C++11 mode with a warning. */
        pos_warning(ec_list_initializer_nonstandard_in_current_mode,
                    &pos_curr_token);
      } else if (!is_or_contains_error_type(dps->type)) {
        pos_error(ec_exp_assign, &pos_curr_token);
      }  /* if */
      direct = FALSE;
    }  /* if */
    if (vp != NULL) vp->has_direct_braced_initializer = direct;
    dps->init_state.direct_init = direct;
  } else {
    /* Traditional aggregate initialization of the form "T x = { ... }".
       Brace elision is allowed in all modes. */
    dps->init_state.elided_braces_allowed = TRUE;
  }  /* if */
  if (list_init_enabled) {
    /* C++11 requires a diagnostic on narrowing in this case, but since it
       is a backward compatibility issue, we make it only a warning in non-
       strict modes.  (Strictly speaking, it is only a backward compatibility
       issue for non-direct initialization syntax, but since GCC only warns on
       the direct syntax too, we follow suit in nonstrict mode.) */
    if (strict_ansi_mode) {
      dps->init_state.error_on_narrowing = TRUE;
    } else {
      dps->init_state.warning_on_narrowing = TRUE;
    }  /* if */
  }  /* if */
  if (vp != NULL) vp->has_direct_braced_initializer = direct;
  braced_initializer(dps->type, (an_init_component *)NULL,
                     &dps->init_state, dps, (an_init_component **)NULL,
                     diag_pos);
  if (vp != NULL && is_incomplete_array_type(vp->type) &&
      is_array_type(dps->type)) {
    /* An array declarator of the form "X[]" followed by a braced initializer:
       Dimension it according to the initializer. */
    a_type_ptr  dim_type = dps->type;
    if (dps->init_state.init_error) {
      /* An error occurred while processing the initializer: Proceed with an
         error type. */
      dim_type = error_type();
    }  /* if */
    put_type_back_into_variable(vp, dps->sym, diag_pos, linkage, dim_type);
    dps->type = vp->type;
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_pos_block != NULL) {
    decl_pos_block->var_init_range.end = curr_construct_end_position;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* brace_init_variable */


static void expr_direct_init_object(a_decl_parse_state  *dps,
                                    an_id_linkage_kind  linkage,
                                    a_boolean           fill_in_dtor,
                                    a_source_position   *diag_pos)
/*
Scan and process the expression in a parenthesized variable or member
initializer (both are "direct" initializers) where the initialization is not
via a constructor (e.g., The expression "4" in "int x(4);" or in
"struct S { S(): x(4) {} int x; };").  dps is the declaration parsing state
associated with the initialization (a synthetic state in the case of member
initialization) and idl_linkage describes the linkage of the variable being
initialized (or idl_none for member initializers).  fill_in_dtor is TRUE if an
applicable destructor should be recorded in any top-level dynamic init entry
created for this initialization (but it is not put on a lifetime list at this
point).  diag_pos is the position to use for diagnostics when no more specific
position is available.
*/
{
  an_init_component_ptr  expr_icp;
  an_init_state          *is = &dps->init_state;
  a_boolean              is_var_init, is_pack_expansion;

  is_var_init = dps->sym != NULL && var_for_symbol(dps->sym) != NULL;
  is->direct_init = TRUE;
  /* Scan the expression if any. */
  if (curr_token == tok_rparen &&
      !anything_cached(&dps->prescanned_initializer_cache)) {
    /* Something like:
         X<T...> x(p...);
       where an empty parameter pack expansion during look-ahead turns "p..."
       into "nothing". */
    check_assertion(is_variadic_template_context() &&
                    !is_template_dependent_context());
    expr_icp = NULL;
  } else {
    expr_icp = scan_full_initializer_expr_as_component(
                 dps, /*parenthesized=*/TRUE, /*allow_empty_expansion=*/TRUE);
  }  /* if */
  if (expr_icp == NULL) {
    is_pack_expansion = TRUE;
    value_init_variable_or_member(dps->type, is, diag_pos);
  } else {
    a_boolean  saved_force_dynamic_init = is->force_dynamic_init;
    check_assertion(expr_icp->next == NULL);
    is_pack_expansion = expr_icp->pack_expansion_descr != NULL;
    if (is_error_component(expr_icp)) {
      /* An error occurred earlier.  Continue with an error constant. */
      is->init_con = alloc_error_constant();
      is->init_error = TRUE;
      if (is_incomplete_array_type(dps->type)) dps->type = error_type();
    } else if (may_be_string_type(dps->type) &&
               try_string_literal_init(expr_icp, &dps->type, is,
                                       &is->init_con)) {
      /* String initialization. */
    } else {
      /* Ordinary initialization. */
      is->elements_are_full_expressions = TRUE;
      convert_initializer(expr_icp, dps->type, is_var_init, fill_in_dtor, is);
    }  /* if */
    free_init_component_list(expr_icp);
    is->force_dynamic_init = saved_force_dynamic_init;
  }  /* if */
  if ((is_aggregate_type(dps->type) && !is->init_error) ||
      is->force_dynamic_init) {
    /* The routines for aggregate initialization produce a constant entry, but
       those entries may embed a dynamic initialization.  If so, return a
       dynamic initialization entry for a nonconstant aggregate to the caller.
       Also produce a dynamic init entry if the caller requested it through the
       force_dynamic_init state flag. */
    prep_initializer_result(is, /*dtor_rp=*/(a_routine_ptr)NULL);
  }  /* if */
  if (is_pack_expansion) {
    /* The given component is a pack expansion: Record that in the IL
       produced by the conversion. */
    if (is->init_con != NULL) {
      is->init_con->is_pack_expansion = TRUE;
    } else if (is->init_dip != NULL) {
      if (is->init_dip->kind == (a_dynamic_init_kind)dik_expression ||
          is->init_dip->kind ==
                     (a_dynamic_init_kind)dik_call_returning_class_via_cctor) {
        is->init_dip->variant.expression->is_pack_expansion = TRUE;
      } else if (is->init_dip->kind == (a_dynamic_init_kind)dik_constant ||
                 is->init_dip->kind ==
                              (a_dynamic_init_kind)dik_nonconstant_aggregate) {
        is->init_dip->variant.constant->is_pack_expansion = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (is_var_init) {
    a_variable_ptr  vp = var_for_symbol(dps->sym);
    if (is_incomplete_array_type(vp->type) &&
        (is_array_type(dps->type) || is_error_type(dps->type))) {
      put_type_back_into_variable(vp, dps->sym, diag_pos, linkage, dps->type);
      dps->type = vp->type;
    }  /* if */
  }  /* if */
}  /* expr_direct_init_object */


#if !EXTRA_SOURCE_POSITIONS_IN_IL
/*ARGSUSED*/ /* decl_pos_block is not used in some configurations. */
#endif /* !EXTRA_SOURCE_POSITIONS_IN_IL */
static void expr_init_aggr_variable(a_decl_parse_state  *dps,
                                    an_id_linkage_kind  linkage,
                                    a_source_position   *diag_pos,
                                    a_decl_pos_block    *decl_pos_block)
/*
dps, linkage,  decl_pos_block describe a variable of class or array type
initialized with what looks like an expression.  I.e., an initialization of
the form:

	T x = <expr>;

Check and record the initialization as appropriate.  diag_pos is the position
to use for diagnostics by default.
*/
{
  an_init_component_ptr  expr_icp, icp;
  an_init_state          *is = &dps->init_state;
  a_variable_ptr         vp;
  a_boolean              is_string_var, missing_braces_diagnosed = FALSE;
  a_boolean              make_error_result = FALSE;
  an_error_severity      severity = es_none;

  check_assertion(!dps->has_direct_initializer);
  check_assertion(dps != NULL && dps->sym != NULL);
  vp = var_for_symbol(dps->sym);
  check_assertion(vp != NULL);
  is->elided_braces_allowed = TRUE;
  is_string_var = may_be_string_type(dps->type);
  if (!is_string_var && !C_mode()) {
    /* In C++, the only valid case here is string initialization.  We cannot
       in general know whether this is a string initialization until we've
       parsed the expression, but if the destination type isn't a string type,
       we can issue the diagnostic early (which is nicer in cases where
       parsing the expression triggers severe syntax errors). */
    severity = es_error;
    pos_error(ec_missing_initializer_list, &pos_curr_token);
    missing_braces_diagnosed = TRUE;
  }  /* if */
  expr_icp = scan_full_initializer_expr_as_component(
                                         dps,
                                         /*parenthesized=*/FALSE,
                                         /*allow_empty_pack_expansion=*/FALSE);
  check_assertion(expr_icp->next == NULL);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_pos_block != NULL) {
    decl_pos_block->var_init_range.end = *init_component_end_pos(expr_icp);
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (is_error_component(expr_icp)) {
    /* An error occurred earlier.  Continue with an error constant. */
    make_error_result = TRUE;
  } else if (is_string_var &&
             try_string_literal_init(expr_icp, &dps->type, is,
                                     &is->init_con)) {
    /* String initialization. */
  } else {
    /* A braced initializer is normally required here.  However, pcc allows
       the braces to be omitted (e.g., "int a[2] = 1;" is treated as equivalent
       to "int a[2] = { 1 };"), and we also accept it with a warning in
       default C89 mode. */
    if (!missing_braces_diagnosed && C_dialect != C_dialect_pcc) {
      if (C_dialect == C_dialect_cplusplus) {
        severity = es_error;
      } else if (strict_ansi_mode || gcc_mode || microsoft_mode) {
        severity = strict_ansi_error_severity;
      } else {
        /* Issue a warning in non-ANSI C mode. */
        severity = es_warning;
      }  /* if */
      pos_diagnostic(severity, ec_missing_initializer_list,
                     init_component_pos(expr_icp));
    }  /* if */
    icp = expr_icp;
    if (severity == es_error) {
      /* We already issued an error and the expression is unlikely to be a
         valid initializer for the destination element.  Avoid further errors
         an just return an error constant. */
      make_error_result = TRUE;
    } else if (is_array_type(dps->type)) {
      is->initializer_can_dimension_array = TRUE;
      aggr_init_array(&icp, &dps->type, is, init_component_pos(icp),
                      &is->init_con);
    } else {
      check_assertion(is_class_struct_union_type(dps->type));
      aggr_init_class(&icp, dps->type, is, init_component_pos(icp),
                      &is->init_con);
    }  /* if */
  }  /* if */
  if (expr_icp != NULL) free_init_component_list(expr_icp);
  if (make_error_result) {
    is->init_con = alloc_error_constant();
    is->init_error = TRUE;
    if (is_incomplete_array_type(dps->type)) dps->type = error_type();
  } else {
    /* Ensure a dynamic initializer result is returned if needed.  (Since in
       C++ mode only the string literal initialization case is valid, no
       destructor needs to be passed in.) */
    prep_initializer_result(is, /*dtor_rp=*/NULL);
  }  /* if */
  if (is_incomplete_array_type(vp->type) &&
      (is_array_type(dps->type) || is_error_type(dps->type))) {
    put_type_back_into_variable(vp, dps->sym, diag_pos, linkage, dps->type);
    dps->type = vp->type;
  }  /* if */
}  /* expr_init_aggr_variable */


#if !EXTRA_SOURCE_POSITIONS_IN_IL
/*ARGSUSED*/ /* decl_pos_block is not used in some configurations. */
#endif /* !EXTRA_SOURCE_POSITIONS_IN_IL */
static void expr_init_scalar_variable(a_decl_parse_state  *dps,
                                      a_decl_pos_block    *decl_pos_block)
/*
dps (which must be non-NULL) and decl_pos_block describe a variable of scalar
type initialized with what looks like an expression.  I.e., an initialization
of the form:

	T x = <expr>

Check and record the initialization as appropriate.  diag_pos is the position
to use for diagnostics by default.
*/
{
  an_init_component_ptr  expr_icp;

  expr_icp = scan_full_initializer_expr_as_component(
                                         dps,
                                         /*parenthesized=*/FALSE,
                                         /*allow_empty_pack_expansion=*/FALSE);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_pos_block != NULL) {
    decl_pos_block->var_init_range.end = *init_component_end_pos(expr_icp);
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (dps->sym == NULL || var_for_symbol(dps->sym) == NULL) {
    /* In some error cases (e.g., an old-style C parameter with an initializer)
       dps->sym may not actually represent an initializable variable. */
    expect_error();
  } else {
    convert_initializer(expr_icp, dps->type, /*is_var_init=*/TRUE,
                        /*fill_in_dtor=*/TRUE, &dps->init_state);
  }  /* if */
  free_init_component_list(expr_icp);
}  /* expr_init_scalar_variable */


void scan_compound_literal_initializer(a_decl_parse_state  *dps,
                                       an_init_component  *rescan_aggr,
                                       an_init_component  **return_icp)
/*
Scan the brace-enclosed part of a compound literal.  Such literals are of the
form (type){initializer} or (type){initializer,}.  The type provided in
parentheses is passed to this function through dps->type; if this type is
incomplete, the complete type should be deduced from the initializer and
dps->type will be updated with that complete type.  The caller also sets the
dps->init_state.static_lifetime_init flag depending on the context of the
expression containing the compound literal.  A dynamic init entry is
created by this function and a pointer to it is returned through
dps->init_state.init_dip.  If return_icp is non-NULL, return the init-component
entry for the braced-init-list in *return_icp instead of freeing it as usual.
The caller is responsible for ensuring that the current token is a brace, and
the function braced_initializer does all the hard work.  If rescan_aggr is
non-NULL, a rescan is being done during template deduction; rescan_aggr
provides a braced-init-list for the initializer, already copied and
substituted.
*/
{
  a_source_position   start_pos;
  a_dynamic_init_ptr  dip;

  check_assertion((C_mode() || gpp_mode) &&
                  (rescan_aggr != NULL || curr_token == tok_lbrace));
  start_pos = pos_curr_token;
  /* Call braced_initializer to scan the brace-enclosed initializer part of
     the compound initializer.  Set up the "init state" to ensure a dynamic
     initializer entry is created. */
  dps->init_state.force_dynamic_init = TRUE;
  dps->init_state.init_error = is_error_type(dps->type);
  dps->init_state.elided_braces_allowed = TRUE;
  dps->init_state.initializer_can_dimension_array = TRUE;
  if (C_mode() && (dps->init_state.static_lifetime_init ||
                   !allow_nonconstant_auto_aggr_init_in_c_mode)) {
    dps->init_state.initializer_must_be_constant = TRUE;
    dps->init_state.traditional_const_expr_required = TRUE;
  }  /* if */
  braced_initializer(dps->type, rescan_aggr, &dps->init_state, dps,
                     return_icp, &start_pos);
  /* Adjust the dynamic initializer entry that was produced to reflect that it
     represents a compound initializer. */
  dip = dps->init_state.init_dip;
  if (dip != NULL) {
    dip->is_compound_literal = TRUE;
    if (dip->kind == (a_dynamic_init_kind)dik_constant ||
        dip->kind == (a_dynamic_init_kind)dik_nonconstant_aggregate) {
      dip->variant.constant->is_compound_literal = TRUE;
      if (!is_incomplete_array_type(dps->type)) {
        dip->variant.constant->type = dps->type;
      }  /* if */
    }  /* if */
  } else {
    check_assertion(dps->init_state.init_error);
  }  /* if */
}  /* scan_compound_literal_initializer */


void initializer(a_decl_parse_state  *dps,
                 a_source_position   *source_pos,
                 an_id_linkage_kind  linkage,
                 a_boolean           parenthesized_initializer,
                 a_boolean           *incomplete_type_error_reported,
                 a_decl_pos_block    *decl_pos_block)
/*
Scan an initializer for the declaration described by *dps (dps->sym points
to an sk_variable or sk_static_data_member symbol).  The linkage of the
initialized entity is given by linkage.  *source_pos is the declaration's
position for diagnostic purposes.
The C-mode syntax is:

3.5.7  initializer:
		assignment-expression
		{ initializer-list }
		{ initializer-list , }

       initializer-list:
		initializer-list
		initializer-list , initializer

In C mode parenthesized_initializer is always FALSE, but in C++ mode it can
be TRUE to indicate an alternate syntax (ARM 8.4):

       initializer:
                ( expression-list )

Note: when parenthesized_initializer is TRUE, the current token is the token
immediately following the left parenthesis; on return, the closing right
parenthesis will have been swallowed.  If the caller should suppress issuing
an error on an incomplete type, *incomplete_type_error_reported will be
returned set to TRUE.
*/
{
  a_symbol_ptr                      symbol_ptr = dps->sym;
  a_variable_ptr                    vp = NULL;
  a_type_ptr                        vp_type = NULL;
  a_boolean                         var_err, init_err;
  a_boolean                         static_lifetime;
  a_constant_ptr                    init_con = NULL;
  a_dynamic_init_ptr                init_dip = NULL;
  a_class_symbol_supplement_ptr     cssp = NULL;
  a_memory_region_number            region_to_switch_back_to;
  an_object_lifetime_ptr            local_static_lifetime = NULL;
  a_local_static_variable_init_ptr  local_static_var_init = NULL;
  a_token_kind                      first_token;
  a_source_position                 pos_first_token;
  a_boolean                         reactivation_pushed = FALSE;

  db_enter(3, "initializer");
  dps->has_initializer = TRUE;
  dps->init_state.decl_parse_state = dps;
  /* There are a number of tests to determine whether the variable can take
     an initializer.  If it cannot, set var_err; it will be checked later
     to decide whether to update the variable with information about the
     initialization. */
  var_err = FALSE;
  if (dps->is_old_style_param_decl) {
    /* Old-style C parameter declarations cannot contain an initializer.
       (C++ default arguments, which look a bit like a parameter with an
       initializer -- e.g., void f(int i = 1) -- are handled elsewhere.) */
    pos_error(ec_initializer_in_param, source_pos);
    var_err = TRUE;
    static_lifetime = FALSE;
  } else if (symbol_ptr->kind == (a_symbol_kind)sk_variable) {
    vp = symbol_ptr->variant.variable.ptr;
    static_lifetime = has_static_storage_duration(vp->storage_class);
  } else if (symbol_ptr->kind == (a_symbol_kind)sk_static_data_member) {
    vp = symbol_ptr->variant.static_data_member.variable;
    static_lifetime = TRUE;
  } else {
    /* Not a variable (for example, might be a typedef). */
    pos_sy_error(ec_cannot_initialize, source_pos, symbol_ptr);
    var_err = TRUE;
    /* Set static_lifetime to a fake value that will be consistent with where
       the declaration appears. */
    static_lifetime = (depth_innermost_function_scope == NO_SCOPE_DEPTH);
  }  /* if */
  dps->init_state.static_lifetime_init = static_lifetime;
  if (!var_err) {
    vp_type = vp->type;
    if (vla_enabled && is_vla_type(vp->type)) {
      /* VLAs cannot be initialized.  (This must be the first error case
         tested because we set vp_type to NULL to recover.  If it were a
         later case, and the declaration was also (e.g.) block extern,
         we'd diagnose that instead and not recover completely.) */
      pos_sy_error(ec_cannot_initialize, source_pos, symbol_ptr);
      var_err = TRUE;
      vp_type = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (vp->decl_modifiers & DM_DLLIMPORT) {
      /* A variable declared __declspec(dllimport) cannot be initialized. */
      pos_sy_error(ec_cannot_initialize, source_pos, symbol_ptr);
      var_err = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* Do not add code here. */
    } else if (symbol_ptr->kind == (a_symbol_kind)sk_variable &&
               linkage != idl_none &&
               depth_innermost_function_scope != NO_SCOPE_DEPTH) {
      /* "Block extern" variable with internal or external linkage --
         not allowed to be initialized.  (3.5.7 Constraints) */
      pos_sy_error(ec_cannot_initialize, source_pos, symbol_ptr);
      var_err = TRUE;
    } else if (vp->init_kind != (an_init_kind)initk_none) {
      /* Variable already initialized (presumably, it is being declared
         again, and we have the variable from the earlier declaration).
         Sun compilers mostly ignore (but do check for errors) an out-of-class
         initializer for a member constant of a class template instance. */
      if (vp->init_kind == (an_init_kind)initk_static &&
          vp->is_constexpr &&
          is_error_constant(vp->initializer.constant)) {
        /* The previous constant was generated for error recovery purposes.
           An additional diagnostic is unlikely to be helpful. */
        expect_error();
      } else if (sun_mode && vp->is_member_constant &&
                 vp->is_template_static_data_member) {
        pos_sy_warning(ec_out_of_class_initializer_ignored, source_pos,
                       symbol_ptr);
      } else {
        pos_sy_error(ec_already_initialized, source_pos, symbol_ptr);
      }  /* if */
      var_err = TRUE;
#if UPC_EXTENSIONS_ALLOWED
    } else if (is_underlying_shared_qualified_type(vp_type)) {
      /* Objects with shared types cannot have initializers. */
      pos_sy_error(ec_cannot_initialize, source_pos, symbol_ptr);
      var_err = TRUE;
      vp_type = NULL;
#endif /* UPC_EXTENSIONS_ALLOWED */
    } else {
      /* Only object types (except for VLAs), incomplete arrays, and reference
         types are allowed to be initialized. */
      if (is_complete_object_type(vp_type)) {
        /* Object type -- okay. */
      } else if (is_array_type(vp_type) &&
                 !is_incomplete_type(array_element_type(vp_type))) {
        /* Array type.  The is_incomplete_type test disallows arrays of
           incomplete struct/unions (which in C are possible as an
           extension). */
      } else if (is_any_reference_type(vp_type)) {
        /* Reference type -- okay. */
      } else {
        if (is_incomplete_type(vp_type)) {
          /* Incomplete type is an error. */
          pos_error(incomplete_type_err_code(vp_type), source_pos);
          *incomplete_type_error_reported = TRUE;
        } else {
          /* Catch-all error. */
          pos_sy_error(ec_cannot_initialize, source_pos, symbol_ptr);
        }  /* if */
        var_err = TRUE;
        vp_type = NULL;
      }  /* if */
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED
    if (gnu_mode && static_lifetime && var_is_gnu_named_register(vp) &&
        vp->asm_name_or_reg.reg != (a_named_register)anr_invalid) {
      /* A variable with static lifetime declared to map onto a specific
         register (using the GNU asm("register-name") construct) cannot have
         an initializer. */
      pos_error(ec_register_mapped_variable_cannot_have_initializer,
                source_pos);
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  }  /* if */
  /* Note that in the error cases just detected, we go ahead and scan the
     initializer, but then discard the value. */
  if (vp_type == NULL) {
    /* Use an error type to avoid additional errors. */
    vp_type = error_type();
  }  /* if */
  if (symbol_ptr->is_class_member) {
    /* The initializer of a static data member is scanned with the original
       class reactivated (if we're parsing a prototype instantiation, this was
       done elsewhere).  We may also end up here with an sk_variable in some
       error cases. */
    if (is_incomplete_type(sym_parent_class(symbol_ptr))) {
      /* We can end up here in error situations such as:
           { struct S; int S::i = 0; }
         or
           template <class T> struct A {
             static int x;
             template<> int A<double>::x = 37;
         We can also get here with a static data member of a C++/CLI managed
         class type when that member has an in-class initializer.  E.g.:
           ref class X { static int i = 3; };  // Definition of X::i
         There is no need to reactivate the class scope in that case. */
      check_assertion(symbol_ptr->is_error ||
                      !is_file_or_namespace_scope(
                                            &scope_stack[depth_scope_stack]));
    } else if (symbol_is(symbol_ptr, sk_static_data_member) &&
               is_local_scope_kind(scope_stack_top().kind)) {
      /* An attempt to define a static data member in a local scope.  Don't
         reactivate the class since it can cause problems with the lifetime
         management of temporaries. */
      check_assertion(symbol_ptr->is_error);
    } else if (!is_template_context() || sun_mode ||
               (microsoft_mode && microsoft_version <= 1300)) {
      /* For templates, the class was already reactivated when the
         instantiation scope was pushed.  In older Microsoft modes another
         scope is pushed because multiple sets of template parameter names
         are visible in the initializer. */
      push_class_reactivation_scope(sym_parent_class(symbol_ptr),
                                    /*extend_namespace=*/TRUE);
      reactivation_pushed = TRUE;
    }  /* if */
  } else {
    if (sym_is_namespace_member(symbol_ptr)) {
      push_namespace_reactivation_scope(sym_parent_namespace(symbol_ptr));
    }  /* if */
    if (exceptions_enabled && static_lifetime &&
        vp != NULL && vp->source_corresp.is_local_to_function) {
      /* This is the initialization of a local static variable.  Push
         a block lifetime around the entire initialization. */
      push_object_lifetime((an_il_entry_kind)iek_none, (char *)NULL,
                           (an_object_lifetime_kind)olk_block);
      local_static_lifetime = curr_object_lifetime;
    }  /* if */
  }  /* if */
  /* In variable initializations, the initializer elements should each be
     treated as full expressions.  E.g., in "T x = { f(), g() };" both "f()"
     and "g()" are full expressions. */
  dps->init_state.elements_are_full_expressions = TRUE;
  /* In this context, the dimension of an array might be determined by the
     initializer. */
  dps->init_state.initializer_can_dimension_array = TRUE;
  /* If the initialization is invalid in some way, init_err will be set to
     TRUE.  It will be used to assure that the initialization bound to the
     variable will be an error constant (or a dynamic initializer pointing
     to an error constant. */
  init_err = FALSE;
  if (!C_mode() && is_real_class_type(vp_type)) {
    cssp = symbol_supplement_for_class(vp_type);
    if (curr_token == tok_lbrace && !cssp->is_class_aggregate &&
        !dps->has_direct_initializer && !list_init_enabled) {
      /* This is an attempt to do C-style aggregate initialization on a class
         object for which there is a constructor, nonpublic members, base
         classes, or virtual functions.  In such cases a constructor must be
         used. */
      type_error(ec_brace_initialization_not_allowed, vp_type);
      init_err = TRUE;
      vp_type = error_type();
      cssp = NULL;
    }  /* if */
  }  /* if */
  /* Save the current token kind: curr_token will change if we prescan the
     initializer. */
  first_token = curr_token;
  pos_first_token = pos_curr_token;
  if (C_mode()) {
    /* In C mode, static lifetime variables require constant initializers.
       In addition, some C mode also require constant initializers for
       automatic variables of aggregate type initialized with a braced
       construct. */
    if (static_lifetime || (!allow_nonconstant_auto_aggr_init_in_c_mode &&
                            is_aggregate_or_union_type(vp_type) &&
                            first_token == tok_lbrace)) {
      dps->init_state.initializer_must_be_constant = TRUE;
      dps->init_state.traditional_const_expr_required = TRUE;
    }  /* if */
  } else {
    /* In C++ mode, constexpr variables require constant initializers. */
    dps->init_state.initializer_must_be_constant = vp != NULL &&
                                                   vp->is_constexpr;
    if (dps->auto_type_specifier_seen && !dps->has_trailing_return_type &&
        !is_error_type(vp_type)) {
      /* An initializer for a variable declared with the "auto" type
         specifier.*/
      if (first_token == tok_lbrace && !list_init_enabled) {
        error(ec_auto_brace_initialization_not_allowed);
        vp->type = vp_type = error_type();
        invalidate_type(dps);
        dps->auto_type_specifier_seen = FALSE;
        dps->auto_type = NULL;
      } else {
        prescan_initializer_for_auto_type_deduction(dps,
                                                    parenthesized_initializer);
        vp_type = dps->type;
      }  /* if */
      if (is_error_type(vp_type)) {
        cssp = NULL;
      }  /* if */
    }  /* if */
  }  /* if */
  dps->type = vp_type;
  /* Now process the initializer.  There are four syntactic cases:
       (1) parenthesized initializers (a C++ feature; e.g., "T x(3);"),
       (2) direct list initializers (a C++11 feature; e.g., "T x{3};"),
       (3) traditional list initializers (e.g., "T x = {3};"), and
       (4) simple initializers (e.g., "T x = 3;").
     These are handled in turn. */
  if (parenthesized_initializer) {
    /* Either this is an initialization of the form S x (arg [, ...]), where
       S is a class type name or an initialization of a scalar like int i(0).
       This form of initialization is allowed in C++ mode only.  Note that
       the opening parenthesis has already been scanned in the caller. */
    a_boolean  dependent_class_type = could_be_dependent_class_type(vp_type);
    if ((cssp != NULL && cssp->constructor != NULL) ||
        dependent_class_type) {
      /* It's a class type and there's a constructor or we're dealing with a
         dependent type that could be such a class. */
      /* Depending on the arguments present, a constructor, possibly the copy
         constructor, will be selected and returned. */
      a_source_position  pos;
      /* Use the source position of the first argument as the call position. */
      pos = pos_first_token;
      if (dependent_class_type) {
        scan_dependent_type_parenthesized_initializer(&dps->init_state);
      } else {
        scan_class_parenthesized_initializer(vp_type, vp_type, &pos,
                                             /*fill_in_dtor=*/TRUE,
                                             /*args_supplied=*/FALSE,
                                             (an_arg_list_elem_ptr)NULL,
                                             &dps->init_state);
      }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
      if (decl_pos_block != NULL) {
        decl_pos_block->var_init_range.end = curr_construct_end_position;
      }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      init_err = dps->init_state.init_error;
      init_con = dps->init_state.init_con;
      init_dip = dps->init_state.init_dip;
    } else {
      /* An entity with no constructor.  (If it's a C-style struct with no
         constructor, initialization with bitwise copy is allowed -- e.g.,
         S x, y(x) -- but typically it's an object of non-class type.) */
      add_stop_token(tok_rparen);
      /* Scan the initializer.  Either a constant pointer is returned or else
         a dynamic init entry representing an expression. */
      expr_direct_init_object(dps, linkage, /*fill_in_dtor=*/TRUE, source_pos);
      init_err = dps->init_state.init_error;
      init_con = dps->init_state.init_con;
      init_dip = dps->init_state.init_dip;
      /* The closing right paren will not have been consumed, as it is
         the arg list for a constructor call is scanned, so bypass it
         explicitly. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
      if (curr_token == tok_rparen && decl_pos_block != NULL) {
        decl_pos_block->var_init_range.end = end_pos_curr_token;
      }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      remove_stop_token(tok_rparen);
      check_closing_paren_after_expr_list();
      /* Although the entity has no constructor, it may have a destructor that
         needs to be recorded in the dynamic init entry (if any). */
      if (cssp != NULL && init_dip != NULL) {
        init_dip->destructor = select_destructor(vp_type, vp_type, source_pos);
      }  /* if */
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (cppcli_enabled && is_value_class_type(vp_type) &&
             is_cli_generic_definition_argument_type(vp_type)) {
    /* A constraint type can be a value class type, but should not be treated
       as an aggregate type since its subobject structure is not known.  E.g.:
         generic<class T> where T: value class
         void f(T x) { T y = { x }; }  // Treat as simple initialization and
                                       // not as aggregate initialization.
    */
    brace_init_variable(dps, dps->has_direct_initializer, linkage, source_pos,
                        decl_pos_block);
    init_err = dps->init_state.init_error;
    init_con = dps->init_state.init_con;
    init_dip = dps->init_state.init_dip;
  } else if (cppcli_enabled && first_token == tok_lbrace &&
             !dps->has_direct_initializer &&
             (is_handle_to_cli_array_type(vp_type) ||
              (is_handle_type(vp_type) &&
               is_template_param_or_nonreal_class_type(
                                                 type_pointed_to(vp_type))))) {
    /* A C++/CLI array initializer. */
    an_init_component_ptr  icp_tree;
    icp_tree = scan_braced_init_list(/*is_full_expr=*/TRUE, dps);
    aggr_init_cli_array_with_alloc(icp_tree, vp_type, &dps->init_state,
                                   &init_dip);
    free_init_component_list(icp_tree);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else if (first_token == tok_lbrace) {
    /* Braced initialization (e.g., "X x{1, 2};" or "X x = { y };"). */
    brace_init_variable(dps, dps->has_direct_initializer, linkage, source_pos,
                        decl_pos_block);
    init_err = dps->init_state.init_error;
    init_con = dps->init_state.init_con;
    init_dip = dps->init_state.init_dip;
  } else if (is_aggregate_or_union_type(vp_type)) {
    /* Class or array initialization without braces. */
    if (is_class_struct_union_type(vp_type) &&
        (C_dialect == C_dialect_cplusplus || !static_lifetime)) {
      /* Special C++ case:  a class aggregate may be initialized with an
         object of its class or a class derived from it.  E.g., if S is the
         name of a struct and x is an S, then S y = x is permitted.  In
         addition, x may be any expression of a type for which there is a
         type conversion to S. Thus S y = 1 is a legal initialization if
         S(int) exists to perform the conversion. */
      /* In ordinary C a struct or union variable may be initialized by an
         object of the same type as long as dynamic initialization is
         otherwise allowed. */
      scan_class_initializer_expression(dps);
      init_err = dps->init_state.init_error;
      init_con = dps->init_state.init_con;
      init_dip = dps->init_state.init_dip;
#if EXTRA_SOURCE_POSITIONS_IN_IL
      if (decl_pos_block != NULL) {
        decl_pos_block->var_init_range.end = curr_construct_end_position;
      }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    } else if (gnu_mode && static_lifetime) {
      /* In GNU modes, a compound literal is treated as a constant-expression
         that can initialize a variable with a static lifetime.  We may also
         arrive here when the initializer is a (possibly parenthesized) string
         literal. */
      a_constant  constant;
      scan_constant_initializer_expression(vp_type, dps, &constant);
      init_con = alloc_unshared_constant(&constant);
      if (!var_err && vp != NULL) {
        a_type_ptr     array_type = skip_typerefs(vp->type);
        if (is_incomplete_type(vp->type)) {
          /* An array of unspecified size is initialized with a constant that
             has a known number of elements: adjust the variable type. */
          a_targ_size_t  num_elems;
          check_assertion(is_array_type(array_type));
          if (!is_array_type(constant.type)) {
            /* An error occurred while scanning the initializer constant.
               Set the number of elements to "1" to avoid a second diagnostic
               about creating a variable of incomplete type. */
            check_assertion(is_or_contains_error_type(constant.type) &&
                            total_errors != 0);
            init_err = TRUE;
            num_elems = 1;
          } else {
            num_elems =
                       constant.type->variant.array.variant.number_of_elements;
          }  /* if */
          set_initialized_array_size(&array_type, num_elems,
                                     /*unknown_dependent=*/FALSE);
          vp->type = array_type;
        }  /* if */
      }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
      if (decl_pos_block != NULL) {
        decl_pos_block->var_init_range.end = curr_construct_end_position;
      }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    } else {
      /* Other initializations of array or class types using an expression
         not enclosed in braces.  That includes the special case of
         initializing a character array with a string literal. */
      expr_init_aggr_variable(dps, linkage, source_pos, decl_pos_block);
      init_err = dps->init_state.init_error;
      init_con = dps->init_state.init_con;
      init_dip = dps->init_state.init_dip;
    }  /* if */
  } else {
    /* A non-aggregate object is being initialized with an expression (the
       braced initializer case was handled above).  A constant or non-constant
       expression may be permitted as the initializer. */
    expr_init_scalar_variable(dps, decl_pos_block);
    init_err = dps->init_state.init_error;
    init_con = dps->init_state.init_con;
    init_dip = dps->init_state.init_dip;
  }  /* if */
  if (anything_cached(&dps->prescanned_initializer_cache)) {
    /* Normally, prescanned components should have been consumed by now.
       Only in error cases can it be otherwise. */
    expect_error();
    flush_initializer_cache(&dps->prescanned_initializer_cache);
  }  /* if */
  if (!var_err) {
    /* There was no error that precludes initialization, so update the
       variable entry with the initializer. */
    a_routine_ptr  dtor = NULL;
    /* Remember whether the initializer uses the "()" form or the "=" form. */
    vp->has_parenthesized_initializer = parenthesized_initializer;
    if (init_err) {
      /* There was an error in the initializer.  Put an error constant
         into the initializer field of the variable, if only to be sure
         another initialization will be prevented. */
      a_constant  constant;
      set_error_constant(&constant);
      init_con = alloc_unshared_constant(&constant);
      init_dip = NULL;
    }  /* if */
    if (init_dip == NULL) {
      check_assertion(init_con != NULL);
      /* There's no dynamic init entry because the need for one cannot be
         inferred from the initializer.  Nevertheless, create one if (1)
         there's a destructor associated with the type of the variable, or
         (2) it's an automatic variable. */
      if (!init_err && cssp != NULL) {
        /* Check for the existence of a destructor independently of checks
           for a constructor.  This is to catch the unusual case in which a
           user has defined a destructor but the object can be initialized
           without a constructor. */
        dtor = select_destructor(vp_type, vp_type, source_pos);
      }  /* if */
      if (dtor != NULL || !has_static_storage_duration(vp->storage_class)) {
        init_dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constant);
        init_dip->variant.constant = init_con;
        init_dip->is_braced_initializer = (first_token == tok_lbrace);
        if (dps->init_state.partial_initializer) {
          init_dip->is_partially_initialized = TRUE;
        }  /* if */
        init_con = NULL;
        /* If a destructor was found, add a pointer to it to the dynamic init
           entry. */
        init_dip->destructor = dtor;
      }  /* if */
    }  /* if */
    check_assertion((init_dip == NULL) != (init_con == NULL));
    if (init_dip != NULL) {
      /* Generate a dynamic initialization entry, attach it to the variable,
         and generate an stmk_init statement. */
      a_statement_ptr init_stmt;
      gen_dynamic_initialization(vp, init_dip, &local_static_var_init,
                                 source_pos, decl_pos_block, &init_stmt);
#if MICROSOFT_EXTENSIONS_ALLOWED && DO_IL_LOWERING
#if LOWER_MICROSOFT_NONCONSTANT_AGGREGATE
      /* Note that if microsoft_mode and C_mode() are TRUE, *vp may be an
         automatic variable with a nonconstant aggregate initializer.  The
         IL representation for this involves a dik_nonconstant_aggregate
         dynamic init entry.  Normally, such entries only appear in unlowered
         C++ IL.  Lower it to C if configured that way. */
      /* Note that the equivalent C99 and GNU C feature is not lowered here;
         that's done in the normal C99 lowering phase. */
      if (microsoft_mode && C_mode() &&
          /* Skip if C99 lowering will be done anyway. */
          !c99_il_lowering_needed() &&
          init_dip->kind == (a_dynamic_init_kind)dik_nonconstant_aggregate) {
        lower_microsoft_C_mode_nonconstant_aggregate_init(vp, init_stmt);
        /* Force re-determination of the last statement of the current
           sequence. */
        struct_stmt_stack[depth_stmt_stack].last_dep_statement = NULL;
      }  /* if */
#endif /* LOWER_MICROSOFT_NONCONSTANT_AGGREGATE */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED && DO_IL_LOWERING */
    } else if (has_static_storage_duration(vp->storage_class) &&
               vp->source_corresp.is_local_to_function) {
      /* This must be a non-dynamic initialization of a local static
         variable. */
      check_assertion(in_file_scope(vp));
      if (in_file_scope(init_con)) {
        /* Initializer constant is already in the file scope memory region.
           (This happens, for example, for an error constant case.) */
        vp->initializer.constant = init_con;
        vp->init_kind = (an_init_kind)initk_static;
      } else if (init_con->kind == (a_constant_repr_kind)ck_aggregate ||
                 has_non_file_scope_ref(init_con)) {
        /* Aggregate-constant initialization.  Since the aggregate constant
           is in the local memory region, the variable can't have a pointer
           to it.  Instead, create a local-static-variable-init entry to point
           to the initializer -- it is added to a list associated with the
           current function or block scope.  A similar problem exists when
           the constant points to something in the function scope memory
           region. */
        local_static_var_init =
              make_local_static_variable_init(vp, (a_scope_ptr)NULL,
                                              (an_init_kind)initk_static,
                                              init_con,
                                              (a_dynamic_init_ptr)NULL);
      } else {
        /* The initializer is a simple constant, so it can just be attached
           to the variable.  However, the variable is in file scope memory
           and init_con was allocated in function scope memory; therefore,
           copy the constant to the correct memory region. */
        switch_to_file_scope_region(&region_to_switch_back_to);
        vp->initializer.constant = copy_unshared_constant(init_con);
        switch_back_to_original_region(region_to_switch_back_to);
        vp->init_kind = (an_init_kind)initk_static;
      }  /* if */
    } else {
      /* Neither the variable nor the initializer require initialization to be
         dynamic. */
      vp->init_kind = (an_init_kind)initk_static;
      vp->initializer.constant = init_con;
    }  /* if */
    check_constant_valued_variable(dps);
#if EXTRA_SOURCE_POSITIONS_IN_IL
    if (decl_pos_block != NULL) {
      vp->initializer_range = decl_pos_block->var_init_range;
    }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  }  /* if */
  if (symbol_ptr->is_class_member) {
#if NEED_NAME_MANGLING
    if (symbol_is(symbol_ptr, sk_static_data_member)) {
      /* A static data member initializer.  If the initializer defines closure
         types (i.e., contains lambda expressions), assign unique numbers
         ("discriminators") to each one; this numbers will be used by name
         mangling.  Also record the data member as a "parent entity" for such
         closure types (this is also used in the mangled encoding). */
      compute_data_member_name_collision_discriminators(symbol_ptr);
      set_parent_entity_for_closure_types(
                vp->entities_defined_in_initializer, symbol_ptr,
                symbol_ptr->variant.static_data_member.instance_ptr != NULL);
    }  /* if */
#endif /* NEED_NAME_MANGLING */
    /* The initializer of a static data member was scanned with the original
       class reactivated (if we're parsing a prototype instantiation, this was
       done elsewhere).  Restore the scope to what it was before.  An exception
       can happen in managed class types where static data members can have
       in-class initializers even when the member is not a constant.  (We may
       also end up here with an sk_variable.) */
    /* Note that this call has to be after the select_destructor call in the
       preceding section of code. */
    if (reactivation_pushed) {
      check_assertion(!is_incomplete_type(sym_parent_class(symbol_ptr)));
      pop_class_reactivation_scope();
    }  /* if */
  } else {
    /* If an object lifetime was pushed to surround the initialization of
       a local static variable, pop it now. */
    if (local_static_lifetime != NULL) {
        pop_object_lifetime_for_local_static_init(local_static_lifetime,
                                                  local_static_var_init,
                                                  init_err || var_err);
    }  /* if */
    if (sym_is_namespace_member(symbol_ptr)) {
      pop_namespace_reactivation_scope();
    }  /* if */
  }  /* if */
#if CHECKING
  if (vp != NULL && vp->is_constexpr) {
    a_type_ptr  tp = skip_typerefs(vp->type);
    check_assertion_or_expect_error(
                                 initializer_constant(vp) != NULL ||
                                 is_template_dependent_type(tp) ||
                                 (is_immediate_class_type(tp) &&
                                  !class_symbol_supp(symbol_for(tp))
                                       ->has_nontrivial_default_constructor &&
                                  !tp->variant.class_struct_union
                                              .has_zero_init_component));
  }  /* if */
#endif /* CHECKING */
#if DEBUG
  if (debug_level >= 3 || db_flag_is_set("dump_init")) {
    if (!var_err) {
      fputs("initializer for ", f_debug);
      db_variable(vp);
      db_initializer(vp, 2);
#if EXTRA_SOURCE_POSITIONS_IN_IL
      fputs("  (initializer range: ", f_debug);
      db_source_range(&vp->initializer_range);
      fputs(")\n", f_debug);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* initializer */


static void expr_init_field(a_decl_parse_state  *dps,
                            a_type_ptr          dtype)
/*
Scan a field initialization of the form "T x = <expr>".  dps describes the
field declaration and dtype is the type of the field.
*/
{
  an_init_component_ptr  expr_icp = scan_full_initializer_expr_as_component(
                                         dps,
                                         /*parenthesized=*/FALSE,
                                         /*allow_empty_pack_expansion=*/FALSE);
  an_init_state          *is = &dps->init_state;

  if (may_be_string_type(dtype) &&
      try_string_literal_init(expr_icp, &dtype, is, &is->init_con)) {
    /* A string literal initializer for a string type. */
    prep_initializer_result(is, /*dtor_rp=*/NULL);
  } else {
    convert_initializer(expr_icp, dtype, /*is_var_init=*/FALSE,
                        /*fill_in_dtor=*/TRUE, is);
  }  /* if */
  free_init_component_list(expr_icp);
}  /* expr_init_field */


void field_initializer(a_decl_parse_state  *dps)
/*
Scan an initializer for the field described by dps->sym and record it in the
IL entry for that field.
*/
{
  an_init_state      *is = &dps->init_state;
  a_field_ptr        field;
  a_type_ptr         dtype, class_type;
  a_source_position  init_pos;
  a_boolean          saved_in_field_initializer = 
                                       scope_stack_top().in_field_initializer;
  an_object_lifetime_ptr
                     saved_curr_object_lifetime = curr_object_lifetime;
  a_class_symbol_supplement_ptr
                     parent_cssp;
  a_boolean          saved_scanning_field_initializer = FALSE;

  check_assertion(scope_is(&scope_stack_top(), sck_class_struct_union) ||
                  scope_is(&scope_stack_top(), sck_class_reactivation));
  class_type = scope_stack_top().assoc_type;
  /* Indicate in the scope stack that we are parsing a field initializer.
     (E.g., so that the expression routines know that the keyword "this" is
     meaningful.)  Also temporarily set the current object lifetime to file
     scope life time. */
  scope_stack_top().in_field_initializer = TRUE;
  /* Record that a field initializer is being scanned for this class.  This
     is needed to break an ordering issue when determining whether a class is
     literal.  (The literalness may depend on whether all field initializers
     are constants, but folding the field initializers may require knowing if
     this class is literal.  If this occurs, the type is considered not to be
     a literal type; see set_literal_type_flag.) */
  parent_cssp = class_symbol_supp(symbol_for(class_type));
  saved_scanning_field_initializer = parent_cssp->scanning_field_initializer;
  parent_cssp->scanning_field_initializer = TRUE;
  curr_object_lifetime = il_header.primary_scope->lifetime;
  is->force_dynamic_init = TRUE;
  if (symbol_is(dps->sym, sk_field)) {
    field = dps->sym->variant.field.ptr;
    dtype = field->type;
  } else {
    field = NULL;
    dtype = error_type();
  }  /* if */
  init_pos = pos_curr_token;
  if (curr_token == tok_assign) {
    /* A field initialization using copy-initialization syntax. */
    (void)get_token();
    if (curr_token == tok_lbrace) {
      /* An initialization of the form "T x = { ... }". */
      is->elements_are_full_expressions = TRUE;
      if (strict_ansi_mode) {
        is->error_on_narrowing = TRUE;
      } else {
        is->warning_on_narrowing = TRUE;
      }  /* if */
      /* Scan the initializer. */
      braced_initializer(dtype, (an_init_component*)NULL, is,
                         (a_decl_parse_state*)NULL, (an_init_component**)NULL,
                         &init_pos);
    } else {
      /* An initialization of the form "T x = <expr>". */
      expr_init_field(dps, dtype);
    }  /* if */
  } else if (curr_token == tok_lbrace) {
    /* A direct braced initializer. */
    is->direct_init = TRUE;
    is->elements_are_full_expressions = TRUE;
    if (strict_ansi_mode) {
      is->error_on_narrowing = TRUE;
    } else {
      is->warning_on_narrowing = TRUE;
    }  /* if */
    /* Scan the initializer. */
    braced_initializer(dtype, (an_init_component*)NULL, is,
                       (a_decl_parse_state*)NULL, (an_init_component**)NULL,
                       &init_pos);
  } else if (curr_token == tok_removed_expr) {
    /* We can get here with severe syntax errors.  Use an error constant to
       proceed. */
    expect_error();
    is->init_dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constant);
    is->init_dip->variant.constant = alloc_error_constant();
  } else {
    unexpected_condition();
  }  /* if */
  if (field != NULL && is->init_dip != NULL) {
    if (field->initializer != NULL) {
      /* If the field already has an initializer, that means that the
         evaluation of the initializer resulted in a recursive instantiation.
         The initializer will have been filled-in with an error node or error
         constant. */
      check_assertion(is_error_dynamic_init(field->initializer));
    } else {
      field->has_direct_braced_initializer = is->direct_init;
      field->initializer = is->init_dip;
      field->has_nonconstant_initializer = is->constant_expr_ruled_out;
#if EXTRA_SOURCE_POSITIONS_IN_IL
      field->initializer_range.start = init_pos;
      field->initializer_range.end = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if DEBUG
      if (db_flag_is_set("dump_init")) {
        fputs("initializer for field ", f_debug);
        db_name(&field->source_corresp);
        fputs(":\n", f_debug);
        db_dynamic_initializer(field->initializer, 2);
#if EXTRA_SOURCE_POSITIONS_IN_IL
        fputs("  (initializer range: ", f_debug);
        db_source_range(&field->initializer_range);
        fputs(")\n", f_debug);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      }  /* if */
#endif /* DEBUG */
    }  /* if */
  } else {
    expect_error();
    if (field != NULL) field->has_initializer = FALSE;
  }  /* if */
#if NEED_NAME_MANGLING
  /* If the initializer defines closure types (i.e., contains lambda
     expressions), assign unique numbers ("discriminators") to each one; these
     numbers will be used by name mangling.  Also record the data member as a
     "parent entity" for such closure types (this is also used in the mangled
     encoding). */
  if (field != NULL) {
    compute_data_member_name_collision_discriminators(dps->sym);
    set_parent_entity_for_closure_types(
                             field->entities_defined_in_initializer, dps->sym,
                             /*subject_to_trans_unit_corresp=*/TRUE);
  }  /* if */
#endif /* NEED_NAME_MANGLING */
  curr_object_lifetime = saved_curr_object_lifetime;
  scope_stack_top().in_field_initializer = saved_in_field_initializer;
  parent_cssp->scanning_field_initializer = saved_scanning_field_initializer;
  if (--parent_cssp->num_unparsed_field_initializers == 0) {
    /* This was the last unparsed field initializer.  Some actions and
       properties may have been delayed until now. */
    update_class_for_last_parsed_field_initializer(class_type);
  }  /* if */
}  /* field_initializer */


void repeat_nonconstant_init(a_dynamic_init_ptr  ctor_dip,
                             a_type_ptr          array_type,
                             a_type_ptr          elem_type,
                             a_dynamic_init_ptr  new_dip,
                             a_targ_size_t       count)
/*
Define a dynamic init entry for a nonconstant aggregate, which will always be
for an array (of type array_type) whose elements (of type elem_type) are to
be initialized by a series of constructor calls.  The dynamic entry to be
defined (new_dip) has already been allocated; the dynamic init entry that
represents the constructor call is ctor_dip.  count is the number of
elements in the array to be initialized.  Note that multi-dimensional
arrays are treated as one-dimensional arrays.
*/
{
  a_constant_ptr           aggr_con, dynamic_init_con;

  /* The IL structure is
       new dynamic init new_dip (dik_nonconstant_aggregate) ->
         constant (ck_aggregate) ->
           constant (ck_init_repeat) ->
             constant (ck_dynamic_init) ->
               original dynamic init ctor_dip (dik_constructor)
  */
  /* Create the new ck_dynamic_init constant representing the constructor
     call. */
  dynamic_init_con = alloc_constant((a_constant_repr_kind)ck_dynamic_init);
  dynamic_init_con->variant.dynamic_init = ctor_dip;
  dynamic_init_con->type = elem_type;
  /* Create a ck_aggregate constant and point *new_dip to it. */
  aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
  aggr_con->type = array_type;
  new_dip->variant.constant = aggr_con;
  /* Set it to point to a newly created ck_init_repeat constant. */
  aggr_con->variant.aggregate.first_constant =
    aggr_con->variant.aggregate.last_constant =
    add_repeat_con(dynamic_init_con, count);
}  /* repeat_nonconstant_init */


a_boolean def_initializer(a_symbol_ptr       sym,
                          a_source_position  *err_pos)
/*
Perform default initialization for variables and static data members of class
type if needed.  Such initialization is required whenever the class has a
constructor; the default constructor (if one exists) is called.  Return TRUE
if the default initialization of a class type object is performed
(conceptually; in some class type cases TRUE may be returned without an actual
call to a constructor, and in template cases TRUE may be returned when the
object is not known to be of class type).  No initialization is performed (and
FALSE is returned) for non-class objects.
*/
{
  a_boolean                         def_init_performed = FALSE;
  a_variable_ptr                    var = NULL;
  a_type_ptr                        var_type, tp;
  a_class_symbol_supplement_ptr     cssp = NULL;
  a_dynamic_init_ptr                init_dip = NULL, orig_init_dip;
  a_routine_ptr                     ctor = NULL, dtor = NULL;
  a_boolean                         static_lifetime;
  an_object_lifetime_ptr            local_static_lifetime = NULL;
  a_local_static_variable_init_ptr  local_static_var_init = NULL;
  a_boolean                         is_const;
  a_boolean                         is_nonreal_class = FALSE;

  db_enter(3, "def_initializer");
  /* Default initialization is done only in C++ and only for variables and
     static data members. */
  if (C_dialect == C_dialect_cplusplus) {
    if (sym->kind == (a_symbol_kind)sk_variable) {
      var = sym->variant.variable.ptr;
    } else if (sym->kind == (a_symbol_kind)sk_static_data_member) {
      var = sym->variant.static_data_member.variable;
    }  /* if */
  }  /* if */
  if (var != NULL) {
    static_lifetime = has_static_storage_duration(var->storage_class),
    is_const = is_const_qualified_type(var->type);
    tp = var_type = skip_typerefs(var->type);
    if (is_array_type(tp)) {
      tp = f_skip_typerefs(underlying_array_element_type(tp));
    }  /* if */
    if (is_class_struct_union_type(tp)) {
      is_nonreal_class = tp->variant.class_struct_union.is_nonreal_class;
      cssp = symbol_supplement_for_class(tp);
    }  /* if */
    /* Default initialization is done only for non-POD class objects that are
       defined in the current translation unit (i.e., storage class other than
       "extern").  It is also done for constexpr variables of POD class types
       (only possible for essentially empty POD classes). */
    /* We don't test just is_POD because we want to catch cases where there
       is a user-declared defaulted constructor or destructor that's not
       accessible. */
    if (cssp != NULL &&
        (!cssp->is_POD ||
         cssp->constructor != NULL || cssp->destructor != NULL ||
         (var->is_constexpr &&
          !tp->variant.class_struct_union.has_zero_init_component)) &&
        var->storage_class != (a_storage_class)sc_extern &&
        !is_incomplete_type(var_type)) {
      if (sym->kind == (a_symbol_kind)sk_static_data_member) {
        if (!is_template_dependent_context()) {
          /* Perform the default initialization of a static data member with
             its parent class reactivated. */
          push_class_reactivation_scope(sym_parent_class(sym),
                                        /*extend_namespace=*/TRUE);
        }  /* if */
      } else {
        if (exceptions_enabled && static_lifetime &&
            depth_innermost_function_scope != NO_SCOPE_DEPTH) {
          /* This is the initialization of a local static variable.  Push
             a block lifetime around the entire initialization. */
          push_object_lifetime((an_il_entry_kind)iek_none, (char *)NULL,
                               (an_object_lifetime_kind)olk_block);
          local_static_lifetime = curr_object_lifetime;
        }  /* if */
        if (sym_is_namespace_member(sym)) {
          push_namespace_reactivation_scope(sym_parent_namespace(sym));
        }  /* if */
      }  /* if */
      /* Find a default constructor. */
      if (cssp->constructor != NULL) {
        /* There are user-declared constructor(s) and/or implicitly-declared
           constructors represented in the normal cssp->constructor symbol.
           Look for a default constructor. */
        a_boolean err;
        ctor = select_default_constructor(tp, err_pos, tp, &err);
        if (err) {
          /* Some error, already diagnosed, e.g., no default constructor. */
        } else if (ctor == NULL || ctor->compiler_generated) {
          /* A default constructor was found, but it isn't a user-provided
             constructor. */
          if (cssp->trivial_default_constructor != NULL) {
            /* Ensure a trivial default constructor can be generated and that
               it is accessible. */
            (void)reference_to_trivial_default_constructor(
                       tp, tp, err_pos, /*check_access=*/TRUE,
                       (a_boolean *)NULL);
          }  /* if */
          if (is_const) {
            /* A user-provided default constructor is normally required for a
               const-qualified variable. */
            if (any_cfront_mode() || microsoft_mode) {
              /* In cfront and Microsoft modes silently use the generated
                 constructor. */
            } else {
              /* Except in -A mode just issue a warning. */
              pos_syty_diagnostic(strict_ansi_mode ?
                                    strict_ansi_discretionary_severity :
                                    es_warning,
                                  ec_missing_default_constructor_on_const,
                                  err_pos, sym, tp);
            }  /* if */
          }  /* if */
        }  /* if */
        /* Set def_init_performed, which is returned to the caller. */
        /* Even if ctor is NULL (as a result of failing to find a default
           constructor) we still set def_init_performed as though default
           initialization were done even though it wasn't -- this will
           prevent a redundant diagnostic from being issued. */
        def_init_performed = TRUE;
      } else if (is_nonreal_class) {
        /* In general we cannot refer to constructors of nonreal classes, but
           we should assume that they have them.  Proceed with ctor and dtor
           set to NULL, but do generate dynamic initializers in the IL. */
        def_init_performed = TRUE;
      } else {
        /* The class has no user-declared constructors. */
        if (is_const) {
          /* Since this is a non-POD class, a user-declared default
             constructor should have been provided.  Leave def_init_performed
             set to FALSE so that a diagnostic will be issued later. */
        } else {
          /* There is no user-declared or nontrivial implicitly declared
             default constructor.  However, the language definition says an
             object is "default initialized", which means the trivial default
             constructor will be called.  We apply the as-if rule and suppress
             the call (since it's a no-op), but the definition still needs to
             be generated, since it may have side-effects. */
          if (reference_to_trivial_default_constructor(tp, tp, err_pos,
                                                       /*check_access=*/TRUE,
                                                       (a_boolean *)NULL)) {
            def_init_performed = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
      dtor = select_destructor(tp, tp, err_pos);
      if (ctor == NULL && dtor == NULL && !is_nonreal_class && !var->is_vla) {
        /* No constructor for default initialization; no destructor either.
           Not a variable-length array (VLA). */
        if (innermost_function_scope != NULL) {
          /* Although no init statement is needed, we still need to track
             attempts to branch past the trivial initialization. */
          record_trivial_init_control_flow(var);
        }  /* if */
        if (var->is_constexpr) {
          /* A constexpr variable requires an initializer representation.  We
             use an empty aggregate in this case. */
          a_constant_ptr  cp;
          if (static_lifetime) {
            /* A static-lifetime variable: Use static initialization. */
            cp = fs_constant((a_constant_repr_kind)ck_aggregate);
            var->init_kind = (an_init_kind)initk_static;
            var->initializer.constant = cp;
          } else {
            /* A local variable with automatic storage duration; use a
               dynamic init entry. */
            cp = alloc_constant((a_constant_repr_kind)ck_aggregate);
            init_dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constant);
            init_dip->variant.constant = cp;
          }  /* if */
          if (!make_value_initialized_constant(var_type, cp)) {
            /* The constant couldn't be created, presumably because default
               initialization is not possible.  An error must have been issued
               earlier. */
            expect_error();
            set_error_constant(cp);
          }  /* if */
        }  /* if */
      } else if (var->is_constexpr) {
        check_assertion_or_expect_error(!has_nontrivial_destructor(cssp));
        if (ctor == NULL) {
          /* This should only be possible with nonreal classes or in some
             error cases. */
          check_assertion_or_expect_error(is_nonreal_class);
        } else {
          /* Fold the default constructor call to obtain a constant
             initializer. */
          a_constant_ptr  cp;
          init_dip = alloc_ctor_dynamic_init(ctor, /*implied_source=*/FALSE);
          cp = get_default_constructed_constant(init_dip, tp, err_pos);
          if (!same_entities(var_type, tp)) {
            /* The object has an array type.  We need to build an aggregate
               initialization on top of the constant. */
            cp = repeat_constant_for_array_init(cp, var_type);
          }  /* if */
          if (static_lifetime &&
              depth_innermost_function_scope == NO_SCOPE_DEPTH) {
            /* A nonlocal static-lifetime variable initialized with a constant
               value. */
            var->init_kind = (an_init_kind)initk_static;
            var->initializer.constant = cp;
            init_dip = NULL;
          } else {
            /* A local variable with automatic storage duration; use a
               dynamic init entry. */
            init_dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constant);
            init_dip->variant.constant = cp;
            init_dip->is_partially_initialized = cp->is_partially_initialized;
          }  /* if */
        }  /* if */
      } else {
        if (ctor != NULL) {
          /* Normal case -- there's a constructor to do the initialization. */
          init_dip = alloc_ctor_dynamic_init(ctor, /*implied_source=*/FALSE);
          if (!same_entities(var_type, tp)) {
            /* The object has an array type.  We need to build an aggregate
               initialization on top of the other dynamic init entry. */
            /* Save a pointer to init_dip, since it will be modified for
               an array initialization. */
            orig_init_dip = init_dip;
            /* Create a new one to represent a nonconstant aggregate
               initialization. */
            init_dip = alloc_dynamic_init(
                               (a_dynamic_init_kind)dik_nonconstant_aggregate);
            /* Build the repeat construct. */
            repeat_nonconstant_init(orig_init_dip, var_type, tp, init_dip,
                                    array_element_count(var_type, tp));
            if (exceptions_enabled && dtor != NULL) {
              /* Set up the representation to deal with the possibility of
                 an exception being thrown before the entire construction of
                 the array is complete. */
              orig_init_dip->destructor = dtor;
              record_partial_aggregate_cleanup_destruction(orig_init_dip,
                                                           /*evaluated=*/TRUE);
            }  /* if */
          }  /* if */
        } else if (is_nonreal_class) {
          /* Assume a dynamic initialization is needed. */
          init_dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constructor);
          init_dip->variant.constructor.ptr = ctor;
        } else {
          /* Default initialization of an object that has a destructor.  We
             generate a dik_none dynamic initialization entry for this object,
             even though it is not actually initialized, so that the existence
             of the destructor can be duly recorded.  VLA cases for non-POD
             classes with no constructor or destructor also get here. */
          init_dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
        }  /* if */
        /* A constructor (or at least a destructor or a VLA) was found and a
           dynamic init entry (local_di) was set to represent the
           initialization. */
        init_dip->destructor = dtor;
      }  /* if */
      if (init_dip != NULL) {
        /* Attach the dynamic init entry to the variable. */
        gen_dynamic_initialization(var, init_dip, &local_static_var_init,
                                   err_pos, (a_decl_pos_block_ptr)NULL,
                                   (a_statement_ptr *)NULL);
#if DEBUG
        if (debug_level >= 3 || db_flag_is_set("dump_init")) {
          db_variable(var);
          fputs(",\n", f_debug);
          db_initializer(var, 2);
        }  /* if */
#endif /* DEBUG */
      }  /* if */
      if (sym->kind == (a_symbol_kind)sk_static_data_member) {
        if (!is_template_dependent_context()) {
          pop_class_reactivation_scope();
        }  /* if */
      } else {
        /* If an object lifetime was pushed to surround the initialization of
           a local static variable, pop it now. */
        if (local_static_lifetime != NULL) {
          pop_object_lifetime_for_local_static_init(local_static_lifetime,
                                                    local_static_var_init,
                                                    /*err=*/FALSE);
        }  /* if */
        if (sym_is_namespace_member(sym)) {
          pop_namespace_reactivation_scope();
        }  /* if */
      }  /* if */
    } else if (var->is_vla) {
      /* The variable is a variable-length array (VLA) but not one with a
         constructor or destructor, e.g., an array of int or of a POD
         class type. */
      init_dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
      gen_dynamic_initialization(var, init_dip, &local_static_var_init,
                                 err_pos, (a_decl_pos_block_ptr)NULL,
                                 (a_statement_ptr *)NULL);
#if DEBUG
      if (debug_level >= 3 || db_flag_is_set("dump_init")) {
        fputs("Default-initialized VLA: ", f_debug);
        db_variable(var);
        fputs("\n", f_debug);
      }  /* if */
#endif /* DEBUG */
    } else if (could_be_dependent_class_type(tp)) {
      /* An unknown (i.e., template-dependent) type that might instantiate to
         a class type with default initializer.  Set def_init_performed to
         TRUE to avoid spurious diagnostics about uninitialized variables. */
      def_init_performed = TRUE;
    }  /* if */
#if CHECKING
    if (var->is_constexpr && !is_nonreal_class) {
      check_assertion_or_expect_error(initializer_constant(var) != NULL ||
                                      is_template_dependent_type(var->type));
    }  /* if */
#endif /* CHECKING */
  }  /* if */
  db_exit();
  return def_init_performed;
}  /* def_initializer */


static void detach_object_lifetime_for_dynamic_init(a_dynamic_init_ptr dip)
/*
If the indicated initialization has an associated object lifetime, detach
it from the object lifetime tree.  This is done for ctor-initializers
so that the object lifetime list can be re-constructed in the canonical
order rather than the order in which the initializers appear in the
source.  Also unlink any destruction for an associated temporary whose
lifetime was promoted to match a reference.
*/
{
  an_object_lifetime_ptr olp = init_expr_lifetime_of(dip);

  if (olp != NULL) {
    a_dynamic_init_ptr outer_dip = olp->parent_destruction_sublist;
    detach_from_object_lifetime_tree(olp);
    /* Restore the parent destruction list (cleared by the call above) so
       that when we re-insert the lifetime we can fix the related overlapping
       dynamic init also. */
    olp->parent_destruction_sublist = outer_dip;
    if (outer_dip != NULL &&
        outer_dip->overlaps_temps_in_inner_lifetime &&
        outer_dip->lifetime_of_overlapping_temps == olp) {
      remove_from_destruction_list(outer_dip);
    }  /* if */
  }  /* if */
}  /* detach_object_lifetime_for_dynamic_init */


static a_boolean disjoint_members_of_union(a_field_ptr field1,
                                           a_field_ptr field2)
/*
Return TRUE if the two indicated fields (ultimately, members of the
same class) are members of the same union, or members of members of
the same union.  Anonymous unions and structs are considered in the
determination.
*/
{
  a_boolean   disjoint_members = FALSE;
  a_type_ptr  class1 = parent_class_of(field1);

  /* Work up from each field looking at the parent classes.  Find the
     innermost class/struct/union that the fields have in common. */
  for (;;) {
    a_type_ptr class2 = parent_class_of(field2);
    for (;;) {
      if (same_entities(class2, class1)) {
        /* We've found the innermost class/struct/union that the
           fields have in common.  If it is a union, they conflict. */
        disjoint_members = (class1->kind == (a_type_kind)tk_union);
        goto end_of_routine;
      }  /* if */
      if (!class2->source_corresp.is_class_member) break;
      class2 = parent_class_of(class2);
    }  /* for */
    check_assertion(class1->source_corresp.is_class_member);
    class1 = parent_class_of(class1);
  }  /* for */
end_of_routine:
  return disjoint_members;
}  /* disjoint_members_of_union */


static a_symbol_ptr ctor_init_symbol(a_constructor_init_ptr  cip)
/*
Return a symbol associated with cip: A field symbol if cip represents the
initialization of a field, or a class type symbol if it represents the
initialization of a base class.
*/
{
  a_symbol_ptr  result = NULL;

  if (cip->kind == (a_constructor_init_kind)cik_field) {
    result = symbol_for(cip->variant.field);
  } else {
    result = symbol_for(cip->variant.base_class->type);
  }  /* if */
  check_assertion(result != NULL);
  return result;
}  /* ctor_init_symbol */


/*
Data structure describing the state of the constructor init list associated
with a constructor being defined.
*/
typedef struct a_ctor_init_block {
  a_constructor_init_ptr
		cip_list, end_of_cip_list;
			/* Pointers to the first and last constructor inits
			   recorded for a constructor.  (NULL if none.) */
  a_constructor_init_ptr
		direct_list, end_of_direct_list;
			/* Pointers to the first and last constructor inits
			   for direct base classes recorded for a constructor.
			   (NULL if there are no such base classes.) */
  a_constructor_init_ptr
		virtual_list, end_of_virtual_list;
		 	/* Pointer to the first constructor init for a virtual
			   base class. */
  a_pack_expansion_stack_entry_ptr
                pesep;
			/* If pack_expansion_context_started is TRUE, the pack
			   expansion stack entry produced by the associated
			   call to begin_potential_pack_expansion_context. */
  a_constructor_init_ptr
		last_order_checked_init;
		 	/* Pointer to last constructor init entry for a
			   mem-initializer whose order has been checked against
			   the declaration order of bases and members. */
  a_type_ptr    pending_decltype_initializer_type;
                        /* A decltype-specifier in a mem-initializer-id can
                           be either a delegating constructor or a base
                           class initializer, for example:
                               struct B {};
                               struct A : B {
                                 A() {}
                                 A(int) : decltype(A())() {}
                                 A(float) : decltype(B())() {}
                               };
                           decltype-specifiers in a mem-initializer-id are
                           consumed by delegating_ctor_initializer, and if
                           found not to be a delegating constructor, the
                           resulting type is stored here for later
                           consideration as a base class initializer. */
  a_source_position
                pending_decltype_pos;
                        /* When pending_decltype_initializer_type is non-NULL
                           contains the starting source position of the
                           decltype-specifier. */
  a_boolean	has_explicit_init;
			/* TRUE if mem-initializers appear explicitly in the
			   source code of this constructor, but possibly FALSE
			   if an empty pack expansion results in there not
			   being any actual explicit mem-initializers. */
  a_boolean	out_of_order_diag_issued;
			/* TRUE if a diagnostic has been issued about
			   mem-initializers not matching declaration order. */
  a_boolean	pack_expansion_context_started;
			/* TRUE if begin_potential_pack_expansion_context has
			   been called for a mem-initializer, and elements
			   remain to be processed for that call. */
} a_ctor_init_block;

/*
Macro that returns TRUE when a decltype-specifier is allowed in a
mem-initializer and the token stream indicates that such a decltype
is present at the current spot (either because curr_token == tok_decltype
or because a previously scanned decltype-specifier is pending).
*/
#define is_decltype_mem_initializer(cibp)                                     \
  (enable_decltype_in_base_specifier_and_mem_initializer &&                   \
   (curr_token == tok_decltype ||                                             \
    (cibp)->pending_decltype_initializer_type != NULL))                       \


static void check_out_of_order_init(a_constructor_init_ptr  new_cip,
                                    a_ctor_init_block       *cibp)
/*
new_cip represents a new constructor initializer and *cibp tracks the state of
constructor initializers for that same construct definition.  If it hasn't
been done yet, issue a remark if the new initializer will occur before the
previous initializer and update the state for the new entry.
*/
{
  if (!cibp->out_of_order_diag_issued) {
    a_boolean  is_out_of_order = FALSE;
    if (cibp->last_order_checked_init == NULL) {
      /* There was no previous initializer, so there cannot be an out-of-order
         item yet. */
    } else if (new_cip->kind < cibp->last_order_checked_init->kind) {
      /* The previous item was of a kind that is initialized after the current
         item. */
      is_out_of_order = TRUE;
    } else if (new_cip->kind == cibp->last_order_checked_init->kind) {
      /* The previous item was of the same kind as the current item, so they
         should both be on the same list.  See if the new item comes after it
         on the initialization-order list (which would mean the order of the
         two initializers matches the order in which the corresponding
         initializations are done). */
      a_constructor_init_ptr  cip = cibp->last_order_checked_init;
      for (; cip != NULL; cip = cip->next) {
        if (cip == new_cip) break;
      }  /* if */
      /* If new_cip was not found after the last checked entry, presumably it
         came before it, and an out-of-order diagnostic should be issued. */
      is_out_of_order = (cip == NULL);
    }  /* if */
    if (is_out_of_order) {
      pos_sy2_diagnostic(es_remark, ec_out_of_order_ctor_init, &error_position,
                         ctor_init_symbol(new_cip),
                         ctor_init_symbol(cibp->last_order_checked_init));
      cibp->out_of_order_diag_issued = TRUE;
    }  /* if */
  }  /* if */
  cibp->last_order_checked_init = new_cip;
}  /* check_out_of_order_init */


static a_symbol_ptr look_up_mem_initializer_id(void)
/*
The current token is a (generalized) identifier of a mem-initializer-id (which
could refer to a field or base class to be initialized, or, in the case of a
delegating constructor, to the parent class of the constructor itself).  Look
up this identifier and return the associated symbol (or NULL if none).
*/
{
  a_symbol_ptr               sym;
  an_identifier_options_set  gid_options = GID_NO_OPTIONS;
  an_identifier_lookup_mode  ilm = ilm_ctor_initializer_name;
  a_boolean                  gid_err;

  check_assertion(curr_token == tok_identifier);
  if (locator_for_curr_id.is_qualified_name) {
    /* A qualified name must name a class. */
    gid_options |= GID_IMPLICIT_TYPE_CONTEXT;
    ilm = ilm_qualified_ctor_initializer_name;
  }  /* if */
  /* Scan the class name or member name.  (The lookup modes used here skip the
     current function scope to ensure that a constructor parameter with the
     same name as a member or base class is not visible.) */
  sym = coalesce_and_lookup_generalized_identifier(gid_options, ilm, &gid_err);
  return sym;
}  /* look_up_mem_initializer_id */


static a_constructor_init_ptr scan_mem_initializer_id(
                                             a_type_ptr         class_type,
                                             a_ctor_init_block  *cibp,
                                             a_type_ptr         *p_init_type,
                                             a_type_ptr         *p_array_type)
/*
Scan a mem-initializer-id (i.e., the name of a field or base class or a
decltype that denotes a base class to be initialized by a constructor
definition) and return a constructor init entry corresponding to it.
class_type is the parent class of the constructor.  *cibp tracks the state of
the constructor init entries for the constructor currently being defined.
*p_init_type is the type to be initialized; in the case of an array, it is the
underlying element type and the array type itself is returned through
*p_array_type (in non-array cases, *p_array_type is left unchanged).
*/
{
  a_symbol_ptr               member_or_base_sym = NULL;
  a_type_ptr                 init_type, orig_type = NULL;
  a_boolean                  template_param_init = FALSE, is_decltype = FALSE;
  a_base_class_ptr           bcp;
  a_constructor_init_ptr     cip, new_cip = NULL;
  a_source_position          pos = pos_curr_token;

  if (is_decltype_mem_initializer(cibp)) {
    /* In C++11 mode, decltype may be used to denote a base class. */
    is_decltype = TRUE;
    if (cibp->pending_decltype_initializer_type != NULL) {
      /* The first mem-initializer-id in a list may already have been
         analyzed to see if it is a delegating constructor, and in cases
         where it's not, the pending decltype-specifier type has been stored
         for consideration as a base class initializer.  Use the pending
         type (which is not an error type). */
      orig_type = cibp->pending_decltype_initializer_type;
      pos = cibp->pending_decltype_pos;
      cibp->pending_decltype_initializer_type = NULL;
      check_assertion(!is_error_type(orig_type));
    } else {
      /* Delegating constructors are disabled or this is not the first
         mem-initializer-id in a list; scan the decltype-specifier. */
      orig_type = scan_decltype_operator((a_rescan_control_block *)NULL,
                                         (a_decl_pos_block *)NULL);
      if (is_error_type(orig_type)) {
        /* An error has been issued. */
        goto scan_paren;
      }  /* if */
    }  /* if */
    check_assertion(orig_type->kind == (a_type_kind)tk_typeref &&
                    typeref_is_type_operator(orig_type));
    if (orig_type->variant.typeref.is_dependent_type_operator) {
      template_param_init = TRUE;
    }  /* if */
    init_type = orig_type;
  } else {
    /* A field or base class name. */
    member_or_base_sym = look_up_mem_initializer_id();
    if (member_or_base_sym != NULL) {
      record_potential_pack_reference(member_or_base_sym, &pos_curr_token);
      /* Check if a template-dependent entity is being initialized: */
      if (symbol_is(member_or_base_sym, sk_field)) {
        if (!member_or_base_sym->is_class_member) {
          /* This can happen in error cases with anonymous unions:
               static union { int i; double j; };
               struct S { S(): i(j) {} };
             Avoid having to deal with non-member fields during error recovery
             by dropping the result of the lookup.  */
          member_or_base_sym = NULL;
        }  /* if */
      } else if (is_type_symbol(member_or_base_sym)) {
        /* This is presumably a mem-initializer for a base. */
        a_type_ptr  type = type_symbol_type(member_or_base_sym);
        type = skip_typerefs(type);
        template_param_init = (type->kind == (a_type_kind)tk_template_param);
      }  /* if */
    }  /* if */
    if ((!class_name_injection_enabled || microsoft_mode) &&
        !is_error_locator(locator_for_curr_id) &&
          !locator_for_curr_id.is_qualified_name) {
      /* If no symbol was returned from the lookup, or if the symbol returned
         was not a base class or member of the current class, see if the name
         (if it was unqualified) matches the name of a base class. This can be
         necessary in cases like this:
           namespace N {
             class A { A(int); ... };
           }
           class B : public N::A {
             B() : A(0) { }
           };
         The check that follows does not quite emulate the results of a lookup
         that supports class name injection (e.g., it doesn't deal properly
         with hiding within the inheritance hierarchy), but the differences
         will be manifested as slightly different diagnostics, and then only in
         rather obscure cases.
         This check is done in Microsoft mode even though class name injection
         is enabled because, in Microsoft mode, the injected name is ignored
         for most lookups. */
      a_boolean  check_base_classes;
      if (member_or_base_sym == NULL) {
        check_base_classes = TRUE;
      } else if (is_class_symbol(member_or_base_sym) &&
                 find_base_class_of(class_type,
                                    type_symbol_type(member_or_base_sym))) {
        /* A class that's on the base-classes list. */
        check_base_classes = FALSE;
      } else if (member_or_base_sym->is_class_member &&
                 same_entities(sym_parent_class(member_or_base_sym),
                               class_type)) {
        /* A member of the current class. */
        check_base_classes = FALSE;
      } else {
        check_base_classes = TRUE;
      }  /* if */
      if (check_base_classes) {
        for (bcp = base_classes_of(class_type); bcp != NULL; bcp = bcp->next) {
          if (bcp->direct || bcp->is_virtual || member_or_base_sym == NULL) {
            a_symbol_ptr  tmp_sym = symbol_for(bcp->type);
            if (locator_for_curr_id.symbol_header == tmp_sym->header) {
              member_or_base_sym = tmp_sym;
              break;
            }  /* if */
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
    if (member_or_base_sym == NULL ||
        symbol_is(member_or_base_sym, sk_undefined)) {
      /* No such name or qualified name in the symbol table. */
      if (is_error_locator(locator_for_curr_id)) {
        /* Some error will already have been issued on this name. */
      } else {
        pos_stty_error(ec_not_a_field_or_base_class, &error_position,
                       locator_for_curr_id.symbol_header->identifier,
                       class_type);
      }  /* if */
      init_type = error_type();
      goto scan_paren;
    }  /* if */
    /* Make sure the symbol found is accessible and not ambiguous. */
    check_ambiguity_and_verify_access(&locator_for_curr_id);
    record_symbol_reference(SRK_REFERENCE | SRK_INITIALIZATION,
                            member_or_base_sym, &error_position,
                            /*update_il_entry=*/FALSE);
  }  /* if */
  if (!is_decltype &&
      symbol_is(member_or_base_sym, sk_field) &&
      same_entities(sym_parent_class(member_or_base_sym), class_type)) {
    /* This is a field of the current class and may be mentioned in the
       constructor's initializer list.  But it's an error to refer to
       it by a qualified name. */
    a_field_ptr  field = member_or_base_sym->variant.field.ptr;
    if (locator_for_curr_id.is_qualified_name) {
      pos_error(ec_qualified_name_not_allowed,
                &locator_for_curr_id.source_position);
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (microsoft_mode && field_is_property_or_event(field)) {
      /* Property and event fields cannot be mentioned in a constructor
         initializer list. */
      pos_error(property_or_event_kind_is(field, pek_cli_event) ?
                  ec_event_name_not_allowed : ec_property_name_not_allowed,
                &locator_for_curr_id.source_position);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    }  /* if */
    init_type = field->type;
    if (is_array_type(init_type)) {
      *p_array_type = init_type;
      if (!is_string_type(init_type)) {
        /* Arrays can be default-initialized if the expression-list is
           omitted. */
        init_type = f_skip_typerefs(underlying_array_element_type(init_type));
      }  /* if */
    }  /* if */
    /* Only one member of a union or an anonymous union subobject is allowed
       to appear in the ctor-initializer list. */
    if (is_union_type(class_type) ||
        member_or_base_sym->variant.field.anonymous_parent_object != NULL) {
      /* Check through fields for which initializers have already been
         specified. */
      for (cip = cibp->cip_list; cip != NULL; cip = cip->next) {
        if (cip->initializer != NULL) {
          /* Note: at this point cip_list includes only fields, so we can
             assume cip->kind is cik_field. */
          if (cip->variant.field == field) {
            /* Error on duplicate initialization will be issued below. */
          } else if (!microsoft_mode &&
                     disjoint_members_of_union(cip->variant.field, field)) {
            /* The union (or the anonymous union subobject) has already been
               initialized. */
            error(ec_union_already_initialized);
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
    /* Check the list for a constructor init entry that refers to this member.
       If it's there we may have a reinitialization error. */
    for (new_cip = cibp->cip_list; new_cip != NULL; new_cip = new_cip->next) {
      /* Note: at this point cip_list includes only fields, so we can assume
         new_cip->kind is cik_field. */
      if (new_cip->variant.field == member_or_base_sym->variant.field.ptr) {
        if (new_cip->initializer != NULL) {
          sym_error(ec_member_already_initialized, member_or_base_sym);
          goto scan_paren;
        }  /* if */
        break;
      }  /* if */
    }  /* for */
    if (new_cip != NULL) {
      /* Already on the list and presumably marked as compiler-generated.
         Reset the flag, now that it's appeared explicitly in the
         ctor-initializer list. */
      new_cip->compiler_generated = FALSE;
    } else {
      /* No constructor init entry exists for this field.  Allocate one and
         add it to the list. */
      new_cip = alloc_ctor_init((a_constructor_init_kind)cik_field);
      new_cip->variant.field = member_or_base_sym->variant.field.ptr;
      new_cip->compiler_generated = FALSE;
      if (cibp->cip_list == NULL) {
        /* Easy case: Start a new list. */
        cibp->cip_list = cibp->end_of_cip_list = new_cip;
      } else {
        /* The order in which fields appear on the constructor init list must
           correspond exactly to the order in which they were declared.  This
           order is preserved in the symbol list for the class, so advance
           through the symbol list and through whatever is already on the
           constructor init list together. */
        a_constructor_init_ptr  prev_cip = NULL;
        a_symbol_ptr            sym;
        cip = cibp->cip_list;
        sym = symbol_supplement_for_class(class_type)->symbols;
        for (; sym != NULL; sym = sym->next_in_scope) {
          if (symbol_is(sym, sk_field)) {
            /* Found a nonstatic data member. */
            if (sym == member_or_base_sym) {
              /* Found the field.  Insert new_cip into cip_list immediately
                 following prev_cip.  If prev_cip is NULL this will be at the
                 head of the list. */
              if (prev_cip == NULL) {
                /* Insert at head of list. */
                new_cip->next = cibp->cip_list;
                cibp->cip_list = new_cip;
              } else {
                /* Insert into the list. */
                new_cip->next = prev_cip->next;
                prev_cip->next = new_cip;
              }  /* if */
              break;
            } else if (sym->variant.field.ptr == cip->variant.field) {
              /* We didn't find the field we're trying to insert, but we did
                 find the next item on the list. */
              if (cip == cibp->end_of_cip_list) {
                /* Since this is the end of the list, we know the new field
                   must appear after the current entry.  Cut short the
                   search. */
                cibp->end_of_cip_list->next = new_cip;
                cibp->end_of_cip_list = new_cip;
                break;
              }  /* if */
              /* Advance through the cip list, saving the current entry as a
                 possible insertion point. */
              prev_cip = cip;
              cip = cip->next;
            }  /* if */
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
    /* At this point new_cip should point to the field's constructor init
       entry to which the initializer should be attached.  It has been located
       in or inserted into the list of such entries at a spot corresponding to
       its declaration order. */
    check_out_of_order_init(new_cip, cibp);
  } else if (is_decltype ||
             is_class_symbol(member_or_base_sym) ||
             template_param_init) {
    /* It is a base class of the current class for which initialization is to
       be done.  (In a prototype instantiation, this could look like the
       initialization of a template parameter.) */
    a_boolean  indirect_nonvirtual_base_class_found = FALSE;
    if (!is_decltype) {
      if (locator_for_curr_id.is_semivisible_nested_type) {
        /* The symbol in the locator is a nested class that is not visible with
           the standard lookup rules but is returned in support of the nested
           class anachronism (ARM 18.3.5).  Issue an anachronism diagnostic. */
        sym_diagnostic(anachronism_error_severity, ec_nested_class_anachronism,
                       locator_for_curr_id.specific_symbol);
      }  /* if */
      orig_type = type_symbol_type(member_or_base_sym);
    }  /* if */
    init_type = skip_typerefs(orig_type);
    if (template_param_init &&
        init_type->kind == (a_type_kind)tk_template_param) {
      init_type = proxy_class_for_template_param(init_type);
    }  /* if */
    if (is_qualified_type(init_type)) {
      bcp = NULL;
    } else {
      a_base_class_ptr  found_bcp = NULL;
      /* Locate it in the base classes list for the current class.  Note
         that only direct and virtual base classes can be specified. */
      bcp = base_classes_of(class_type);
      for (; bcp != NULL; bcp = bcp->next) {
        if (same_entities(bcp->type, init_type)) {
          if (bcp->direct || bcp->is_virtual) {
            if (found_bcp == NULL) {
              found_bcp = bcp;
            } else {
              /* This condition occurs when there is a direct nonvirtual base
                 class with the same name as an indirect virtual base class. */
              pos_ty_error(ec_ambiguous_base_class, &pos, bcp->type);
              /* Go ahead and process the first one found. */
              break;
            }  /* if */
          } else {
            /* A base class of the required type was found, but it is
               neither direct nor virtual.  Unless another is found with
               the same name, this will be an error. */
            indirect_nonvirtual_base_class_found = TRUE;
          }  /* if */
        }  /* if */
      }  /* for */
      bcp = found_bcp;
    }  /* if */
    if (bcp == NULL) {
      if ((member_or_base_sym != NULL &&
           !member_or_base_sym->is_template_param &&
           template_param_init) ||
          (class_type->variant.class_struct_union.is_nonreal_class &&
           symbol_supplement_for_class(class_type)->
                                                  any_nonreal_base_classes)) {
        /* There are some cases where we cannot match up a base:
             - A dependent reference to a base, but not a template parameter
               itself (presumably, a dependent qualified name).
             - A reference to a class type that might be a dependent base or a
               virtual base class thereof in some instantiation.
           For these cases, we make up a nonvirtual base class node.  We also
           call complete_class_type_is_needed for that presumed base class so
           that it will be instantiated if necessary: base classes must be
           complete, and we will also need to know if it has a constructor. */
        complete_class_type_is_needed(init_type);
        new_cip = alloc_ctor_init(
                              (a_constructor_init_kind)cik_direct_base_class);
        new_cip->variant.base_class = alloc_base_class();
        new_cip->variant.base_class->type = init_type;
        new_cip->orig_type = orig_type;
        if (cibp->direct_list == NULL) {
          /* Start a new list. */
          cibp->direct_list = new_cip;
        } else {
          /* Add to end of list. */
          cibp->end_of_direct_list->next = new_cip;
        }  /* if */
        cibp->end_of_direct_list = new_cip;
      } else {
        /* No valid match found. */
        if (indirect_nonvirtual_base_class_found) {
          /* Actually, a match was found, but it was not a direct or
             virtual base class. */
          error(ec_indirect_nonvirtual_base_class_not_allowed);
        } else if (delegating_constructors_enabled &&
                   same_entities(init_type, class_type)) {
          /* This looks like the mem-initializer for a delegating constructor,
             but it followed an ordinary mem-initializer (which is invalid). */
          pos_error(ec_delegation_init_and_mem_init, &pos);
        } else if (is_decltype) {
          /* Decltype does not denote a base class of the type
             being defined. */
          pos_ty_error(ec_decltype_is_not_base_class, &pos, class_type);
        } else {
          /* Not a base class of the class for which a constructor is
             being defined. */
          pos_stty_error(ec_not_a_field_or_base_class, &pos,
                         member_or_base_sym->header->identifier, class_type);
        }  /* if */
        init_type = error_type();
      }  /* if */
    } else {
      /* The base class was found.  Now look on the appropriate list of
         constructor initializers. */
      new_cip = bcp->is_virtual ? cibp->virtual_list : cibp->direct_list;
      for (; new_cip != NULL; new_cip = new_cip->next) {
        if (new_cip->variant.base_class == bcp) break;
      }  /* for */
      check_assertion(new_cip != NULL);
      /* new_cip was initially marked as compiler-generated. Reset the
         flag now that it's appeared explicitly in the ctor-initializer
         list. */
      new_cip->compiler_generated = FALSE;
      new_cip->orig_type = orig_type;
      if (new_cip->initializer != NULL) {
        type_error(ec_base_class_already_initialized, bcp->type);
      } else {
        check_out_of_order_init(new_cip, cibp);
      }  /* if */
    }  /* if */
  } else {
    /* Not a base class, not a field.  Issue an error. */
    pos_stty_error(ec_not_a_field_or_base_class, &pos,
                   member_or_base_sym->header->identifier, class_type);
    init_type = error_type();
  }  /* if */
scan_paren:
  if (!is_decltype) {
    /* Advance past the identifier. */
    (void)get_token();
  }  /* if */
  *p_init_type = init_type;
  return new_cip;
}  /* scan_mem_initializer_id */


static void check_constexpr_ctor_init(a_routine_ptr      ctor,
                                      an_init_state      *is,
                                      a_source_position  *diag_pos)
/*
is describes the initialization state for a mem-initializer of the given
constructor.  If the constructor is "constexpr" and the initializer is not a
constant, either issue an error if the constructor is not a template instance,
or silently set the is_constexpr flag of the constructor to FALSE (except for
the prototype instantiation).
*/
{
  if (ctor->is_constexpr && is->constant_expr_ruled_out) {
    /* A constexpr constructor requires constant initialization.  In the
       template case, the "constexpr" property is silently dropped.  In other
       cases, an error is issued. */
    if (ctor->is_template_function && !ctor->is_specialized) {
      if (!ctor->is_prototype_instantiation) {
        ctor->is_constexpr = FALSE;
      }  /* if */
    } else {
      pos_error(ec_nonconstant_mem_init_for_constexpr_ctor, diag_pos);
    }  /* if */
  }  /* if */
}  /* check_constexpr_ctor_init */


static void braced_mem_initializer(a_routine_ptr           ctor,
                                   a_type_ptr              dtype,
                                   a_constructor_init_ptr  cip)
/*
Scan a braced mem-initializer (of the given constructor) for a member of the
given type, and record the initializer in *cip if cip is non-NULL.
*/
{
  an_init_state      is;
  a_source_position  lbrace_pos;

  lbrace_pos = pos_curr_token;
  clear_init_state(&is);
  is.direct_init = TRUE;
  is.force_dynamic_init = TRUE;
  is.elements_are_full_expressions = TRUE;
  if (strict_ansi_mode) {
    is.error_on_narrowing = TRUE;
  } else {
    is.warning_on_narrowing = TRUE;
  }  /* if */
  /* Scan the initializer. */
  braced_initializer(dtype, (an_init_component*)NULL, &is,
                     (a_decl_parse_state*)NULL, (an_init_component**)NULL,
                     &lbrace_pos);
  check_constexpr_ctor_init(ctor, &is, &lbrace_pos);
  if (cip != NULL) {
    /* A dynamic init entry has been produced: Record it in the
       constructor init entry. */
    check_assertion(is.init_dip != NULL);
    cip->initializer = is.init_dip;
    cip->initializer->is_constructor_init = TRUE;
    cip->is_braced = TRUE;
    /* If the initializer produced an object lifetime for the full expression,
       remove it temporarily from the object lifetime tree and restore it in
       the correct position later. */
    detach_object_lifetime_for_dynamic_init(cip->initializer);
  }  /* if */
}  /* braced_mem_initializer */


static void scan_parenthesized_mem_init_args(
                                           a_routine_ptr           ctor,
                                           a_constructor_init_ptr  cip,
                                           a_type_ptr              init_type,
                                           a_type_ptr              array_type)
/*
Scan the arguments for a mem-initializer enclosed in parentheses, and update
*cip accordingly.  ctor is the constructor being defined.
init_type is the type to be initialized; in the case of an array, it is the
underlying element type and the array type itself is array_type (in non-array
cases, array_type is NULL).
*/
{
  a_type_ptr                     class_type = parent_class_of(ctor);
  a_source_position              lparen_pos;
  a_class_symbol_supplement_ptr  cssp;
  a_boolean                      dependent_class_init;
  a_dynamic_init_ptr             dip;

  lparen_pos = pos_curr_token;
  /* Skip the left parenthesis. */
  check_assertion(curr_token == tok_lparen);
  (void)get_token();
  dependent_class_init = array_type == NULL &&
                         could_be_dependent_class_type(init_type);
  if (is_class_struct_union_type(init_type) &&
      (array_type == NULL || curr_token == tok_rparen)) {
    /* The type of the base or member is class or array-of-class -- the latter
       only if the expression-list is empty. */
    cssp = symbol_supplement_for_class(init_type);
  } else {
    cssp = NULL;
  }  /* if */
  if ((cssp != NULL && cssp->constructor != NULL) ||
      (dependent_class_init && !m_is_error_type(init_type))) {
    /* This is either a base class or a field of class type.  In
       either case, it will be initialized by a constructor call if
       a constructor exists.  Otherwise, it will be initialized
       like any scalar. */
    an_init_state  is;
    clear_init_state(&is);
    is.direct_init = TRUE;
    is.force_dynamic_init = TRUE;
    if (dependent_class_init) {
      scan_dependent_type_parenthesized_initializer(&is);
    } else {
      a_type_ptr  object_class_type;
      /* If it is a base class, the object being constructed is the whole
         class (and the base class is a subobject thereof).  If it is a field,
         the object being constructed is the field itself.  Set the object
         class type accordingly. */
      check_assertion(cip != NULL);
      if (cip->kind == (a_constructor_init_kind)cik_field) {
        object_class_type = init_type;
      } else {
        object_class_type = class_type;
      }  /* if */
      /* This is treated like an initialization of the form S x (arg [, ...]),
         where S is a class type name.  Depending on the arguments present, a
         constructor will be selected and returned.  The scan function returns
         is.init_error set to TRUE and is.init_dip set to NULL if it finds no
         constructor for which the arguments match. */
      scan_class_parenthesized_initializer(init_type, object_class_type,
                                           &lparen_pos,
                                           /*fill_in_dtor=*/exceptions_enabled,
                                           /*args_supplied=*/FALSE,
                                           (an_arg_list_elem_ptr)NULL,
                                           &is);
    }  /* if */
    dip = is.init_dip;
    if (dip == NULL) {
      /* An error occurred: Create a fake initializer to represent the
         error. */
      check_assertion(is.init_error);
      dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constant);
      dip->variant.constant = alloc_error_constant();
    } else {
      check_constexpr_ctor_init(ctor, &is, &lparen_pos);
      /* If the initializer produced an object lifetime for the full
         expression, remove it temporarily from the object lifetime tree and
         restore it in the correct position later. */
      detach_object_lifetime_for_dynamic_init(dip);
#if CHECKING
      /* If this is the initialization of an array, the dynamic init entry at
         this point represents the initialization of an element of the array,
         not of the array as a whole.  The remaining processing is done later,
         along with members of array type that are not explicitly specified in
         the mem-initializer list. */
      if (array_type != NULL) {
        check_assertion(dip->kind == (a_dynamic_init_kind)dik_constructor ||
                        (dip->kind == (a_dynamic_init_kind)dik_constant &&
                         dip->variant.constant->is_result_of_constexpr_call));
      }  /* if */
#endif /* CHECKING */
    }  /* if */
  } else if (curr_token == tok_rparen && cssp != NULL &&
             reference_to_trivial_default_constructor(init_type, class_type,
                                                      &error_position,
                                                      /*check_access=*/TRUE,
                                                      (a_boolean *)NULL)) {
    /* We fake a call to the trivial default constructor for the class.  No
       call is actually made, but the constructor definition is triggered (in
       case there are side-effects).  Note that this is a so-called
       "value-initialization" case and hence the object must be zeroed. */
    a_dynamic_init_kind  init_kind = (a_dynamic_init_kind)dik_zero;
    if (!value_initialization_enabled ||
        (gpp_mode && emulate_gnu_value_initialization_bugs)) {
      init_kind = (a_dynamic_init_kind)dik_none;
    }  /* if */
    dip = alloc_dynamic_init(init_kind);
#if EXTRA_SOURCE_POSITIONS_IN_IL
    curr_construct_end_position = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    /* Bypass the right paren. */
    (void)get_token();
  } else {
    /* A field whose initialization does not involve a constructor. */
    if (curr_token == tok_rparen) {
      /* An empty initializer, "()", indicating value initialization. */
      if (is_any_reference_type(init_type)) {
        /* Error.  A reference type may not be default-initialized. */
#if MICROSOFT_EXTENSIONS_ALLOWED
        /* Fields cannot be tracking references. */
        check_assertion(!cppcli_enabled ||
                        !is_tracking_reference_type(init_type));
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        error(ec_default_init_of_reference);
        dip = make_error_constant_dynamic_init();
      } else {
        /* Using "()" with the mem-initializer means, perform value
           initialization.  Note that the class and array-of-class cases have
           already been dealt with, so value initialization is tantamount to
           zero-initialization (8.5 [dcl.init]). */
        a_dynamic_init_kind  init_kind = (a_dynamic_init_kind)dik_zero;
        if ((microsoft_bugs && microsoft_version < 1310 &&
             emulate_msvc_value_initialization_bugs) ||
            (gpp_mode && emulate_gnu_value_initialization_bugs &&
             cip != NULL && cip->kind != (a_constructor_init_kind)cik_field) ||
            (array_type != NULL && is_incomplete_array_type(array_type))) {
          /* MSVC++ up to version 7.0 never initializes the entity in this
             case.  g++ up to 3.4 does not initialize base classes.  The
             flexible array member case cannot be initialized since the array
             has no known number of elements. */
          init_kind = (a_dynamic_init_kind)dik_none;
        }  /* if */
        dip = alloc_dynamic_init(init_kind);
      }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
      curr_construct_end_position = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      /* Bypass the right paren. */
      (void)get_token();
    } else {
      /* Not default-initialization. */
      add_stop_token(tok_rparen);
      if (list_init_enabled && gpp_mode && array_type != NULL &&
          curr_token == tok_lbrace) {
        /* Something like "S(): array({ 1, 2 }) {}".  A list initializer in a
           parenthesized initializer for an array member is not actually valid
           per the C++11 standard, but GCC accepts it. */
        pos_warning(ec_braced_init_in_paren_init, &pos_curr_token);
        braced_mem_initializer(ctor, array_type, cip);
        dip = cip->initializer;
      } else if (array_type != NULL && !is_string_type(array_type)) {
        /* Arrays can only be default- or value-initialized -- i.e., the
           expression-list must be omitted.  The exception is a character
           array, which can be initialized with a string literal.  GNU C++ is
           more permissive and allows initialization with an expression of the
           same array type if the elements of the array have a nontrivial copy
           constructor. */
        dip = scan_array_mem_initializer(cip);
      } else {
        a_decl_parse_state  dps;
        init_decl_parse_state(&dps);
        dps.type = init_type;
        dps.init_state.force_dynamic_init = TRUE;
        expr_direct_init_object(&dps, (an_id_linkage_kind)idl_none,
                                /*fill_in_dtor=*/FALSE, &lparen_pos);
        check_constexpr_ctor_init(ctor, &dps.init_state, &lparen_pos);
        dip = dps.init_state.init_dip;
        check_assertion(dip != NULL);
        /* If the initializer produced an object lifetime for the full
           expression, remove it temporarily from the object lifetime tree and
           restore it in the correct position later. */
        detach_object_lifetime_for_dynamic_init(dip);
      }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
      curr_construct_end_position = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      if (!required_token(tok_rparen, ec_exp_rparen)) {
        /* Avoid poor error recovery in cases where a comma-list appears
           between the parens in what is taken to be the initializer of a
           simple object -- e.g.,
               A::A(int i, int j) : x(i,j) { }
           If there is no constructor for x then it is interpreted as a simple
           object, only "i" is scanned, and an error is issued on the expected
           ")".  After that we want to bypass the rest of the comma-list before
           resuming scanning. */
        if (curr_token == tok_comma) {
          flush_to_end_of_arg_list();
          if (curr_token == tok_rparen) {
            /* We found the right parenthesis: Consume it. */
            (void)get_token();
          }  /* if */
        }  /* if */
      }  /* if */
      remove_stop_token(tok_rparen);
    }  /* if */
  }  /* if */
  check_assertion(dip != NULL);
  dip->is_constructor_init = TRUE;
  if (cip != NULL) cip->initializer = dip;
}  /* scan_parenthesized_mem_init_args */


#if !EXTRA_SOURCE_POSITIONS_IN_IL
/*ARGSUSED*/ /* pos is not used in some configurations. */
#endif /* !EXTRA_SOURCE_POSITIONS_IN_IL */
static void scan_mem_init_args(a_routine_ptr           ctor,
                               a_constructor_init_ptr  cip,
                               a_type_ptr              init_type,
                               a_type_ptr              array_type,
                               a_source_position       *pos)
/*
Scan the arguments of a mem-initializer (including the delimiting parentheses
or braces).  ctor is the constructor with which the mem-initializer is
associated.  cip describes this particular mem-initializer (it can be NULL in
error cases).  For non-array (sub)objects, init_type is the type being
initialized and array_type is NULL.
For array subobjects, init_type is the underlying element type being
initialized and array_type is the array type.  pos is the start position of
the mem-initializer.
*/
{
  if (curr_token == tok_lparen) {
    /* A classic (i.e., parenthesized) mem-initializer argument. */
    scan_parenthesized_mem_init_args(ctor, cip, init_type, array_type);
  } else if (list_init_enabled && curr_token == tok_lbrace) {
    /* A braced (i.e., C++11-style) mem-initializer argument. */
    a_type_ptr  dtype = (array_type != NULL) ? array_type : init_type;
    braced_mem_initializer(ctor, dtype, cip);
  } else {
    /* Neither brace nor parenthesis: A syntax error. */
    set_err_pos_to_curr_token();
    add_stop_token(tok_lparen);
    if (list_init_enabled) add_stop_token(tok_lbrace);
    syntax_error(list_init_enabled ? ec_exp_lparen_or_brace : ec_exp_lparen);
    if (list_init_enabled) remove_stop_token(tok_lbrace);
    remove_stop_token(tok_lparen);
    if (cip != NULL) cip->initializer = make_error_constant_dynamic_init();
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (cip != NULL) {
    cip->ctor_init_range.start = *pos;
    cip->ctor_init_range.end = curr_construct_end_position;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* scan_mem_init_args */


static a_constructor_init_ptr scan_mem_initializer(
                                                a_routine_ptr      ctor,
                                                a_type_ptr         class_type,
                                                a_ctor_init_block  *cibp)
/*
Scan a mem-initializer, i.e., the explicit initializer for one subobject in a
ctor-initializer list for the given constructor (which is being defined).
Return a pointer to the constructor-init entry for the member, or NULL in some
error cases.
The current token is the identifier naming the member (or an open parenthesis
for an old-style initializer case).  class_type identifies the class whose
constructor is being defined.  cibp records the state of the constructor init
list associated with the constructor.  That list starts off with entries
describing default initialization (in order of initialization) and those
entries are replaced as needed for each mem-initializer that is encountered.
*/
{
  a_type_ptr              init_type, array_type = NULL;
  a_constructor_init_ptr  new_cip = NULL;
  a_source_position       init_start_pos;

  init_start_pos = pos_curr_token;
  /* Unless this is an old style base class initializer, a base class
     name, member name, or (in C++11 mode) decltype is expected. */
  if (curr_token != tok_lparen &&
      !(is_decl_qualified_name_start() || is_decltype_mem_initializer(cibp))) {
    /* Either an identifier or "::" is expected here. */
    syntax_error(ec_exp_identifier);
  } else {
    if (curr_token == tok_lparen &&
        cibp->pending_decltype_initializer_type == NULL) {
      /* Old-style base class initializer.  It is assumed to apply to the
         direct base class (it's allowed only if there is exactly one direct
         base class). */
      a_boolean one_direct_base = (cibp->virtual_list != NULL &&
                                   cibp->virtual_list->next == NULL &&
                                   cibp->direct_list == NULL) ||
                                  (cibp->direct_list != NULL &&
                                   cibp->direct_list->next == NULL &&
                                   cibp->virtual_list == NULL);
      if (!allow_anachronisms || !one_direct_base) {
        /* Either no base classes or more than one. */
        pos_error(ec_missing_base_class_or_member_name, &init_start_pos);
        init_type = error_type();
      } else {
        /* The base class is probably on the direct_list, but if it was
           declared virtual it is on the virtual list. */
        a_base_class_ptr  bcp;
        new_cip = cibp->direct_list != NULL ? cibp->direct_list
                                            : cibp->virtual_list;
        new_cip->compiler_generated = FALSE;
        bcp = new_cip->variant.base_class;
        check_assertion(bcp->direct);
        init_type = bcp->type;
        pos_ty_diagnostic(anachronism_error_severity,
                          ec_base_class_init_anachronism,
                          &init_start_pos, init_type);
        if (new_cip->initializer != NULL) {
          pos_ty_error(ec_base_class_already_initialized,
                       &init_start_pos, init_type);
        } else {
          check_out_of_order_init(new_cip, cibp);
        }  /* if */
      }  /* if */
    } else {
      /* Standard case: A mem-initializer that starts with the name of a field,
         base class, or (in C++11 mode) a decltype that denotes a base
         class. */
      new_cip = scan_mem_initializer_id(class_type, cibp, &init_type,
                                        &array_type);
    }  /* if */
    scan_mem_init_args(ctor, new_cip, init_type, array_type, &init_start_pos);
  }  /* if */
  return new_cip;
}  /* scan_mem_initializer */


static void check_variant_has_initializer(
                               a_constructor_init_ptr  cip,
                               a_boolean               *variant_init,
                               a_boolean               *variant_explicit_init)
/*
cip points to the constructor init entry for the first variant member of an
anonymous union.  Look through all the entries for that anonymous union to see
if it contains an actual initialization (in which case *variant_init is set to
TRUE).  If it contains an explicit initialization (i.e., not one implied by a
field initializer), set *variant_explicit_init to TRUE.
*/
{

  for (; cip != NULL;) {
    a_dynamic_init_ptr  dip = cip->initializer;
    check_assertion(cip->kind == (a_constructor_init_kind)cik_field);
    if (dip != NULL && dip->kind != (a_dynamic_init_kind)dik_none) {
      /* An explicit initializer. */
      *variant_explicit_init = TRUE;
      *variant_init = TRUE;
      break;
    } else if (cip->variant.field->has_initializer) {
      /* No explicit mem-initializer, but the field has an associated
         in-class initializer. */
      *variant_init = TRUE;
      /* Continue in case an explicit initializer is present (which would
         supersede a field initializer). */
    }  /* if */
    if (symbol_for(cip->variant.field)->variant.field.extra_info
                                      ->is_last_variant_member) {
      /* Any subsequent members are not part of this variant.  End the search
         here. */
      break;
    }  /* if */
    cip = cip->next;
  }  /* for */
}  /* check_variant_has_initializer */


static a_dynamic_init_ptr repeat_mem_init_for_array(a_dynamic_init_ptr  dip,
                                                    a_type_ptr          atype)
/*
dip represents the initialization of a constructible element of an array of the
given type.  Return a corresponding dynamic initializer entry to initialize the
whole array.
*/
{
  a_dynamic_init_ptr  result;
  a_type_ptr          etype = underlying_array_element_type(atype);

  etype = skip_typerefs(etype);
  if (dip->kind == (a_dynamic_init_kind)dik_constant &&
      dip->destructor == NULL) {
    /* We get here with folded constexpr constructor calls. */
    a_constant_ptr  econ = dip->variant.constant, acon;
    check_assertion(econ->is_result_of_constexpr_call);
    acon = alloc_constant((a_constant_repr_kind)ck_aggregate);
    acon->type = atype;
    add_constant_to_aggregate(
                      add_repeat_con(econ, array_element_count(atype, etype)),
                      acon);
    dip->variant.constant = acon;
    result = dip;
  } else {
    result =
           alloc_dynamic_init((a_dynamic_init_kind)dik_nonconstant_aggregate);
    /* Build the looping constant entry. */
    repeat_nonconstant_init(dip, atype, etype, result,
                            array_element_count(atype, etype));
    if (dip->destructor != NULL) {
      /* A destructor is recorded in the array element dynamic-init entry.
         This is in case an exception is thrown in the midst of constructing
         the array, so that the already-constructed elements can be properly
         destroyed. */
      check_assertion(exceptions_enabled);
      dip->destruction_is_for_partially_constructed_aggregate = TRUE;
      /* The dynamic init for the array as a whole should also indicate
         destruction. */
      result->destructor = dip->destructor;
      record_end_of_lifetime_destruction(result, /*static_lifetime=*/FALSE,
                                         /*block_lifetime=*/TRUE);
    }  /* if */
  }  /* if */
  result->is_constructor_init = TRUE;
  return result;
}  /* repeat_mem_init_for_array */


static void restore_mem_init_object_lifetime(a_dynamic_init_ptr  dip)
/*
dip points to an entry describing a mem-initializer.  If the initializer had
produced an object lifetime for the full expression, it was temporarily
removed from the object lifetime tree.  Restore the object lifetime.  (This
restoration must respect the order of initialization, which is not necessarily
the order in which mem-initializers appear in the source.)
*/
{
  an_object_lifetime_ptr  olp = init_expr_lifetime_of(dip);

  if (olp != NULL) {
    if (!long_lifetime_temps) {
      /* Add the lifetime back in as a child of the current object
         lifetime.  This assures that the order of the child-lifetime
         list will reflect the actual order of construction. */
      add_as_child_of_curr_object_lifetime(olp);
    } else {
      /* Promote destructions associated with expression temps to the
         function scope lifetime. */
      promote_lifetime_contents_to_curr_object_lifetime(olp);
      if (dip->kind == (a_dynamic_init_kind)dik_expression) {
        an_expr_node_ptr  expr = dip->variant.expression;
        if (expr->kind == (an_expr_node_kind)enk_object_lifetime &&
            expr->variant.object_lifetime.ptr == olp) {
          /* Link around the enk_object_lifetime expression -- it's not needed
             any longer. */
          dip->variant.expression = expr->variant.object_lifetime.expr;
        }  /* if */
      }  /* if */
      /* Unbind the object lifetime and return it to an available list. */
      unbind_object_lifetime(olp);
      free_object_lifetime(olp);
    }  /* if */
  }  /* if */
}  /* restore_mem_init_object_lifetime */


/*
Pointer to a hash table tracking the targets of delegating constructors.
(We cannot use the IL because constructor definitions may be discarded early.)
Each entry in the table maps a routine entry for a delegating constructor to
a non-delegating constructor it (possibly indirectly) delegates construction
to.
*/
static a_hash_table_ptr
	ctor_delegation_map;

/*
Type of the data items pointed to by the delegation map.
*/
typedef struct a_void_pointer_pair {
  a_void_ptr
	key;
		/* The delegating constructor. */
  a_void_ptr
	value;
		/* The target construct. */
} a_void_pointer_pair;


a_hash_value hash_void_pointer(a_void_ptr  p)
/*
Return a hash value for the given pointer.
*/
{
  a_hash_value  h = (a_hash_value)(unsigned long)p;
  /* Jenkins integer hashing algorithm: */
  h -= (h<<6);
  h ^= (h>>17);
  h -= (h<<9);
  h ^= (h<<4);
  h -= (h<<3);
  h ^= (h<<10);
  h ^= (h>>15);
  return h;
}  /* hash_void_pointer */


a_boolean compare_for_pointer_pair_map(a_void_ptr  entry,
                                       a_void_ptr  key)
/*
Entry points to a void pointer pair.  Return TRUE if the pair's key equals key.
*/
{
  return ((a_void_pointer_pair*)entry)->key == key;
}  /* compare_for_pointer_pair_map */


static void record_nondelegating_target_ctor(a_routine_ptr  ctor,
                                             a_routine_ptr  target)
/*
Record in ctor_delegation_map the nondelegating target constructor (target) to
which ctor delegates initialization.
*/
{
  a_void_pointer_pair **p_pair;

  if (ctor_delegation_map == NULL) {
   ctor_delegation_map = alloc_hash_table(
                               FRONT_END_REGION_NUMBER,
                               (a_hash_table_size)1000,
                               fn_for_function(hash_void_pointer),
                               fn_for_function(compare_for_pointer_pair_map));
  }  /* if */
  p_pair = (a_void_pointer_pair**)hash_find(
                      ctor_delegation_map, (a_void_ptr)ctor, /*create=*/TRUE);
  check_assertion(*p_pair == NULL);
  *p_pair = alloc_fe_of_type(a_void_pointer_pair);
  (*p_pair)->key = ctor;
  (*p_pair)->value = target;
}  /* record_nondelegating_target_ctor */


static a_routine_ptr get_nondelegating_target_ctor(a_routine_ptr  ctor)
/*
The given constructor is the target of a delegating constructor.  If that
constructor is itself a delegating constructor, return the non-delegating
constructor it targets.  Otherwise, just return ctor.
*/
{
  a_routine_ptr       target;
  a_void_pointer_pair **p_pair;

  if (ctor->is_delegating_ctor && ctor_delegation_map != NULL) {
    /* Look up the nondelegating constructor in the delegation map. */
    p_pair = (a_void_pointer_pair**)hash_find(
                     ctor_delegation_map, (a_void_ptr)ctor, /*create=*/FALSE);
    if (p_pair != NULL) {
      target = (a_routine_ptr)(*p_pair)->value;
      if (target->is_delegating_ctor) {
        /* New entries in the delegation map may have lengthened the delegation
           chain since the entry for ctor was recorded.   Use recursion to find
           the current end of the chain. */
        target = get_nondelegating_target_ctor(target);
        /* Record the updated chain end. */
        (*p_pair)->value = (void*)target;
        check_assertion(target != ctor);
      }  /* if */
    } else {
      target = ctor;
    }  /* if */
  } else {
    target = ctor;
  }  /* if */
  return target;
}  /* get_nondelegating_target_ctor */


static a_boolean delegating_ctor_initializer(a_routine_ptr      ctor,
                                             a_ctor_init_block  *cibp)
/*
The current token is the one following a colon (":") presumably introducing
mem-initializers for the given constructor.  If what follows is a
mem-initializer for a delegating constructor, return TRUE and update *cibp to
reflect the initialization.  Otherwise, return FALSE.
In cases where a decltype-specifier is the next token in the stream, the
decltype-specifier is consumed here and if found not to be a delegating
constructor, the scanned type is stored for later use.
*/
{
  a_boolean  is_delegating_init = FALSE, is_decltype = FALSE;

  /* Start a pack expansion context, but skip empty expansions. */
  for (;;) {
    cibp->pack_expansion_context_started = 
                         begin_potential_pack_expansion_context(&cibp->pesep);
    if (!cibp->pack_expansion_context_started) {
      /* An empty pack expansion. */
      if (curr_token == tok_comma) {
        /* Additional mem-initializers follow: Iterate to see if the next one
           is a delegating initializer. */
        (void)get_token();
      } else {
        /* After variadic template expansion there are no mem-initializers. */
        cibp->has_explicit_init = FALSE;
        break;
      }  /* if */
    } else {
      /* An actual mem-initializer is presumably next. */
      break;
    }  /* if */
  }  /* for */
  check_assertion(cibp->pending_decltype_initializer_type == NULL);
  if (cibp->has_explicit_init &&
      (is_decl_qualified_name_start() || is_decltype_mem_initializer(cibp))) {
    a_type_ptr    tp, orig_type = NULL, decltype_type;
    a_symbol_ptr  sym;
    if (is_decltype_mem_initializer(cibp)) {
      /* decltype can be used to denote a delegating constructor in C++11
         modes; scan the decltype operator and see if the underlying type
         matches that of the constructor's class.  If not, the scanned
         type is saved (see below) and re-analyzed as a potential base
         class initializer. */
      is_decltype = TRUE;
      cibp->pending_decltype_pos = pos_curr_token;
      decltype_type = scan_decltype_operator((a_rescan_control_block *)NULL,
                                         (a_decl_pos_block *)NULL);
      if (is_error_type(decltype_type)) {
        /* An error has been issued.  Even though this isn't really a
           delegating constructor, return TRUE to prevent the caller from
           continuing to scan for mem-initializers (since this mem-initializer
           has been consumed). */
        is_delegating_init = TRUE;
        goto end_of_routine;
      }  /* if */
      check_assertion(decltype_type->kind == (a_type_kind)tk_typeref &&
                      typeref_is_type_operator(decltype_type));
      orig_type = decltype_type;
    } else {
      /* A name following the colon: Look it up. */
      sym = look_up_mem_initializer_id();
      if (sym != NULL && is_type_symbol(sym)) {
        /* The name refers to a type: Check if it's the constructor's class.
           (It could also be a base class type or, in error cases, another
           type.) */
        orig_type = type_symbol_type(sym);
      }  /* if */
    }  /* if */
    if (orig_type != NULL) {
      /* We have a candidate for a delegating constructor; see if it meets
         the criteria. */
      tp = skip_typerefs(orig_type);
      if (is_immediate_class_type(tp) &&
          ctor->source_corresp.is_class_member &&
          same_entities(tp, parent_class_of(ctor))) {
        /* This does look like a delegating mem-initializer.  Create a
           constructor init entry for it, and scan the initialization
           arguments (if any). */
        a_source_position       pos;
        a_constructor_init_ptr  cip;
        a_dynamic_init_ptr      dip;
        a_routine_ptr           target = NULL;
        is_delegating_init = TRUE;
        pos = pos_curr_token;
        cip = alloc_ctor_init((a_constructor_init_kind)cik_delegation);
        cip->compiler_generated = FALSE;
        cip->orig_type = orig_type;
        if (!is_decltype) {
          /* Record the reference to the mem-initializer-id. */
          record_potential_pack_reference(sym, &pos_curr_token);
          check_ambiguity_and_verify_access(&locator_for_curr_id);
          record_symbol_reference(SRK_REFERENCE | SRK_INITIALIZATION, sym,
                                  &pos_curr_token, /*update_il_entry=*/FALSE);
          /* Skip over the class name. */
          (void)get_token();
        }  /* if */
        scan_mem_init_args(ctor, cip, tp, (a_type_ptr)NULL, &pos);
        dip = cip->initializer;
        check_assertion(dip != NULL);
        if (dip->kind == (a_dynamic_init_kind)dik_constructor) {
          if (dip->variant.constructor.ptr != NULL) {
            /* Check that this delegation doesn't create a loop of
               delegations.  If it does, discard the constructor init entry. */
            target = get_nondelegating_target_ctor(
                                                 dip->variant.constructor.ptr);
            if (target == ctor) {
              pos_error(ec_delegation_loop, &pos);
              /* To avoid closing the loop in the delegation map (which could
                 lead to unbounded recursion), proceed with a NULL target. */
              target = NULL;
            }  /* if */
          } else {
            /* During prototype instantiations we may not be able to resolve
               the constructor. */
            check_assertion_or_expect_error(ctor->is_prototype_instantiation);
          }  /* if */
        } else if (dip->kind ==
                    (a_dynamic_init_kind)dik_call_returning_class_via_cctor) {
          /* This is fairly unusual.  For example:
                 extern struct X x;
                 struct X {
                   X(): X( []{return x;}() ) {}
                   X(X const&) {}
                 };
          */
        } else if (dip->kind == (a_dynamic_init_kind)dik_expression) {
          /* When forwarding to a trivial copy constructor, the dynamic init
             entry just represents the expression whose value should be
             copied. */
        } else if (dip->kind == (a_dynamic_init_kind)dik_zero) {
          /* This can happen when forwarding to a trivial default
             constructor. */
          check_assertion_or_expect_error(has_trivial_default_constructor(
                                           class_symbol_supp(symbol_for(tp))));
        } else if (dip->kind == (a_dynamic_init_kind)dik_constant ||
                   dip->kind ==
                              (a_dynamic_init_kind)dik_nonconstant_aggregate) {
          /* Template-based mem-initializers and constexpr constructors can
             make us end up with aggregate-like initialization here. */
          check_assertion_or_expect_error(ctor->is_prototype_instantiation ||
                                          dip->variant.constant
                                             ->is_result_of_constexpr_call);
        } else {
          /* Some error must have occurred. */
          expect_error();
        }  /* if */
        restore_mem_init_object_lifetime(dip);
        if (exceptions_enabled && dip->destructor != NULL) {
          /* If an exception is thrown in the delegating body, the destructor
             for the whole object is invoked. */
          record_end_of_lifetime_destruction(dip, /*static_lifetime=*/FALSE,
                                             /*block_lifetime=*/TRUE);
        }  /* if */
        if (end_potential_pack_expansion_context(
                              cibp->pesep, /*is_declarator=*/FALSE) != NULL) {
          /* A variadic pack expansion in a prototype instantiation. */
          cip->is_pack_expansion = TRUE;
        }  /* if */
        cibp->pack_expansion_context_started = 
                                    advance_to_next_pack_element(cibp->pesep);
        if (curr_token == tok_comma) {
          /* More mem-initializers are not permitted for a delegating
             constructor.  For recovery purposes ignore the delegation so
             that we'll scan the remaining mem-initializers. */
          pos_error(ec_delegation_init_and_mem_init, &pos_curr_token);
          (void)get_token();
          is_delegating_init = FALSE;
        }  /* if */
        if (is_delegating_init) {
          if (cip != NULL && target != NULL) {
            record_nondelegating_target_ctor(ctor, target);
          }  /* if */
          cibp->cip_list = cip;
          ctor->is_delegating_ctor = TRUE;
        }  /* if */
      } else if (is_decltype && !is_delegating_init) {
        /* A syntactically valid decltype-specifier has been found, but it's
           not a delegating constructor.  Save this type so that it can be
           processed as a potential base class initializer. */
        cibp->pending_decltype_initializer_type = decltype_type;
      }  /* if */
    }  /* if */
  }  /* if */
end_of_routine:
  return is_delegating_init;
}  /* delegating_ctor_initializer */


a_constructor_init_ptr ctor_initializer(a_routine_ptr  ctor_rout,
                                        a_boolean      user_defined)
/*
Process the explicit and implicit constructor initializations for constructor
routine ctor_rout.  If user_defined is TRUE, the context is that of an
explicit definition in the source code; otherwise, this routine is called
as part of the implicit definition of a compiler generated constructor.

If user_defined is TRUE, scan a comma-separated list of mem-initializers
for data members and/or base classes, if any.  If present, the current
token should be the colon introducing that list.  The special case of
a delegating constructor (a feature introduced in C++11) is also handled
here.

The implicit initializations are performed for base classes and class-type
data members for which no explicit initializers were specified and for which
constructor initialization is required; in such cases default constructors
are invoked.

In addition, when ctor_rout refers to a generated copy constructor, all
nonstatic data members are initialized (for bitwise copy at least) and all
implicitly invoked constructors for member and base class subobjects must
also be copy constructors.

There are rules governing order of initialization, virtual base classes, and
which subobjects require initialization and therefore must be implicitly
initialized.  These are addressed in the course of the processing.
*/
{
  a_boolean                     is_union;
  a_boolean                     has_field = FALSE, has_field_init = FALSE;
  a_boolean                     has_explicit_field_init = FALSE;
  a_boolean                     is_generated_cctor, is_generated_mctor;
  a_type_qualifier_set          required_qualifiers, object_qualifiers;
  a_type_ptr                    class_type, tp, array_type;
  a_symbol_ptr                  sym, class_sym;
  a_ctor_init_block             cib;
  a_constructor_init_ptr        cip, prev_cip, next_cip;
  a_base_class_ptr              bcp;
  a_class_type_supplement_ptr   ctsp;
  a_class_symbol_supplement_ptr cssp;
  a_routine_ptr                 rp;
  a_dynamic_init_ptr            dip;
  a_constructor_init_ptr        uninit_list = NULL, end_of_uninit_list = NULL;
  a_boolean                     any_ref_member_on_uninit_list = FALSE;
  a_boolean                     in_variant = FALSE, variant_complete = FALSE;
  a_boolean                     variant_init = FALSE;
  a_boolean                     variant_explicit_init = FALSE;
  a_boolean                     bad_call_for_constexpr_ctor_reported = FALSE;
  a_boolean                     clear_constexpr_flag = FALSE;

  db_enter(3, "ctor_initializer");
  cib.cip_list = cib.end_of_cip_list = NULL;
  cib.direct_list = cib.end_of_direct_list = NULL;
  cib.virtual_list = cib.end_of_virtual_list = NULL;
  cib.last_order_checked_init = NULL;
  cib.pending_decltype_initializer_type = NULL;
  cib.pending_decltype_pos = null_source_position;
  cib.has_explicit_init = FALSE;
  cib.out_of_order_diag_issued = FALSE;
  cib.pack_expansion_context_started = FALSE;
  if (user_defined && curr_token == tok_colon) {
    /* User-specified initializers are present. */
    cib.has_explicit_init = TRUE;
    /* Bypass the colon. */
    (void)get_token();
    /* Check for the case of a delegating constructor. */
    /* This requires starting a potential pack expansion context at this time.
       That context may then be used later on when scanning ordinary
       mem-initializers if this isn't a delegating constructor (or in some
       error cases that mix the delegating constructor initializer with
       ordinary subobject initializers). */
    /* Note that a decltype-specifier may be used to denote either a delegating
       constructor or a base class specifier; if a decltype-specifier is
       present it is scanned by delegating_ctor_initializer and if it is
       found not to be a delegating constructor, the resulting type is
       saved (in cib.pending_decltype_initializer_type) for later consideration
       by scan_mem_initializer_id. */
    if (delegating_constructors_enabled &&
        delegating_ctor_initializer(ctor_rout, &cib)) {
      goto done;
    }  /* if */
  }  /* if */
  if (ctor_rout->source_corresp.is_class_member) {
    class_type = parent_class_of(ctor_rout);
  } else {
    /* In severe error cases we may get here with a routine marked as a
       constructor, but with no recorded parent scope. */
    expect_error();
    goto done;
  }  /* if */
  is_union = class_type->kind == (a_type_kind)tk_union;
  check_assertion(class_type != NULL);
  ctsp = class_type_supp(class_type);
  /* Check if we are dealing with a generated move/copy constructor. */
  if (user_defined) {
    is_generated_cctor = FALSE;
    is_generated_mctor = FALSE;
    required_qualifiers = TQ_NONE;
  } else if (move_operations_can_be_defaulted() &&
             routine_is_move_constructor(ctor_rout)) {
    is_generated_cctor = FALSE;
    is_generated_mctor = TRUE;
    required_qualifiers = TQ_NONE;
  } else {
    is_generated_cctor = is_copy_constructor(ctor_rout, class_type,
                                             &required_qualifiers,
                                             rvalue_ctor_is_copy_ctor,
                                             /*is_declarative_context=*/TRUE);
    is_generated_mctor = FALSE;
  }  /* if */
  /* The first step is to construct three lists of constructor initializer
     entries, one for virtual base classes that have constructors, one for
     nonvirtual direct base classes that have constructors, and one for
     nonstatic data members that have constructors.  The entries on these
     lists identify all base classes and fields that *must* be initialized
     when the constructor is called; in addition, the third list may be
     supplemented by explicit initializers of fields without constructors.
     Eventually these three lists will be merged into one.  The order of
     items on the list is the order in which initializations are to be
     performed. */
  /* Handle the first two lists together. */
  /* Scan the list of base classes, which may include some that are
     ineligible for initialization. */
  for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
    if (bcp->is_virtual || bcp->direct) {
      cssp = symbol_supplement_for_class(bcp->type);
      /* If the virtual base class or direct base class has a constructor, a
         dynamic init entry will be required; otherwise it is optional.
         Create the constructor init entry now; the dynamic init will be added
         later. */
      cip = alloc_ctor_init((a_constructor_init_kind)(bcp->is_virtual ?
                                                      cik_virtual_base_class :
                                                      cik_direct_base_class));
      cip->variant.base_class = bcp;
      /* Mark the constructor initializer as compiler-generated (i.e., not
         representing an explicit entry in the ctor-initializer list); clear
         the flag later if appropriate. */
      cip->compiler_generated = TRUE;
      /* Add the constructor init to the end of the appropriate list. */
      if (bcp->is_virtual) {
        if (cib.virtual_list == NULL) {
          /* Start a new list. */
          cib.virtual_list = cip;
        } else {
          /* Add to end of list. */
          cib.end_of_virtual_list->next = cip;
        }  /* if */
        cib.end_of_virtual_list = cip;
      } else {
        if (cib.direct_list == NULL) {
          /* Start a new list. */
          cib.direct_list = cip;
        } else {
          /* Add to end of list. */
          cib.end_of_direct_list->next = cip;
        }  /* if */
        cib.end_of_direct_list = cip;
      }  /* if */
    }  /* if */
  }  /* for */
  /* Move on to the third list -- the list of nonstatic data members requiring
     initialization. */
  /* Loop through the symbol list for the class, not the field list, since
     the symbol list contains only user-defined fields whereas the field
     list may also include compiler-generated field entries. */
  class_sym = symbol_for(class_type);
  for (sym = class_sym->variant.class_struct_union.extra_info->symbols;
       sym != NULL;
       sym = sym->next_in_scope) {
    if (sym->kind == (a_symbol_kind)sk_field) {
      /* sym represents a field.  Determine whether constructor initialization
         is required. */
      a_field_ptr field = sym->variant.field.ptr;
      if (microsoft_mode && field_is_property_or_event(field)) {
        /* Property and event fields are not really data members and should
           not be explicitly initialized. */
        continue;
      }  /* if */
      has_field = TRUE;
      if (is_generated_cctor || is_generated_mctor) {
        /* All fields are explicitly listed for a generated copy or move
           constructor, since even if there is no constructor at least a
           bitwise copy is required. */
      } else if (sym->variant.field.extra_info->is_first_variant_member ||
                 sym->variant.field.extra_info->is_last_variant_member) {
        /* The first and last variant field of an anonymous union must be
           listed so we can detect the presence of the variant part below. */
      } else if (ctor_rout->is_constexpr) {
        /* All fields must be initialized in a constexpr constructor.  (For
           variant fields only one member of the union must be initialized.
           That is checked later.) */
      } else if (field->has_initializer) {
        /* Fields with an in-class initializer require corresponding
           constructor-init entries. */
      } else {
        /* This is not a copy constructor.  See if this is a field that
           requires an initializer. */
        tp = field->type;
        if (is_any_reference_type(tp) || is_const_qualified_type(tp)) {
          /* Reference-type fields and const and array-of-const fields require
             an initializer. */
        } else {
          tp = skip_typerefs(tp);
          if (is_array_type(tp)) {
            if (tp->size == 0) {
              /* Zero-length arrays (a GNU feature) and flexible array members
                 (a C++ extension in GNU and Microsoft modes) cannot be
                 initialized. */
              continue;
            }  /* if */
            tp = underlying_array_element_type(tp);
            tp = skip_typerefs(tp);
          }  /* if */
          if (is_real_class_type(tp)) {
            cssp = symbol_supplement_for_class(tp);
            if (!has_trivial_default_constructor(cssp)) {
              /* If the mem-initializer is omitted for this field, a
                 default constructor will have to be called. */
            } else if (cssp->trivial_default_constructor != NULL &&
                       !cssp->trivial_default_constructor
                            ->variant.routine.ptr->is_defaulted) {
              /* If the mem-initializer is omitted for this field, the
                 definition of the trivial default constructor will be
                 generated, though only in case there are diagnostics. */
            } else if (exceptions_enabled && has_nontrivial_destructor(cssp)) {
              /* When exception handling is enabled and there's a destructor,
                 we put out a constructor initializer entry anyway, just to
                 record the destructor. */
            } else if (cssp->is_POD &&
                       tp->variant.class_struct_union.any_const_member) {
              /* A POD with const members -- if the mem-initializer is
                 omitted a diagnostic will have to be issued. */
            } else {
              /* No action is required if the mem-initializer is omitted. */
              continue;
            }  /* if */
          } else {
            /* No initializer is needed. */
            continue;
          }  /* if */
        }  /* if */
      }  /* if */
      /* A constructor init entry is required for this field. */
      cip = alloc_ctor_init((a_constructor_init_kind)cik_field);
      cip->variant.field = sym->variant.field.ptr;
      /* Mark the constructor initializer as compiler-generated (i.e., not
         representing an explicit entry in the ctor-initializer list); clear
         the flag later if appropriate. */
      cip->compiler_generated = TRUE;
      if (cib.cip_list == NULL) {
        cib.cip_list = cip;
      } else {
        cib.end_of_cip_list->next = cip;
      }  /* if */
      cib.end_of_cip_list = cip;
    }  /* if */
  }  /* for */
  /* Three lists that have been created thus far were made to cover the
     default (or value) initialization required because base classes and
     fields need it.  It remains to scan the user specified initializers,
     if any, and to integrate them into the lists. */
  if (cib.has_explicit_init) {
    /* User-specified initializers are present. */
    add_stop_token(tok_lbrace);
    /* Loop through the comma-separated list of initializers. */
    do {
      a_boolean  any_more;
      add_stop_token(tok_comma);
      if (cib.pack_expansion_context_started) {
        /* A pack expansion context was started earlier (while checking for a
           delegating constructor). */
        any_more = TRUE;
      } else {
        any_more = begin_potential_pack_expansion_context(&cib.pesep);
      }  /* if */
      /* Extra loop is used if the mem-initializer is a variadic template
         pack expansion. */
      while (any_more) {
        a_pack_expansion_descr_ptr pedep;
        cip = scan_mem_initializer(ctor_rout, class_type, &cib);
        if (cip != NULL && cip->kind == (a_constructor_init_kind)cik_field) {
          has_field_init = TRUE;
          has_explicit_field_init = TRUE;
        }  /* if */
        pedep = end_potential_pack_expansion_context(cib.pesep,
                                                     /*is_declarator=*/FALSE);
        if (pedep != NULL && cip != NULL) {
          /* This mem-initializer is a variadic template pack expansion, i.e.,
             it's followed by "...".  Furthermore, we're in the prototype
             instantiation, so we mark the constructor init as a pack
             expansion. */
          cip->is_pack_expansion = TRUE;
        }  /* if */
        any_more = advance_to_next_pack_element(cib.pesep);
      }  /* while */
      cib.pack_expansion_context_started = FALSE;
      remove_stop_token(tok_comma);
    } while (loop_token(tok_comma));
    remove_stop_token(tok_lbrace);
  }  /* if */
  /* Merge the three lists into one:  virtual base classes followed by
     nonvirtual direct base classes followed by nonstatic data members. */
  if (cib.direct_list != NULL) {
    cib.end_of_direct_list->next = cib.cip_list;
    cib.cip_list = cib.direct_list;
  }  /* if */
  if (cib.virtual_list != NULL) {
    cib.end_of_virtual_list->next = cib.cip_list;
    cib.cip_list = cib.virtual_list;
  }  /* if */
  /* The following loop proceeds through the merged constructor-init list,
     which now reflects the canonical order of base-class and member
     initializations (i.e., as required by the language definition, not the
     order that appeared in the source).  The body of the loop does several
     things:
       (1) For explicit initializations, it restores to the IL an object
           lifetime entry that was generated for the initializer expression
           but then removed.
       (2) It does the processing for implicit initializations:
           (a) special handling for generated copy/move constructor; or
           (b) processing for user-defined constructor or generated default
               constructor.
           Note: unneeded ctor-init entries are removed from the list.
       (3) If exceptions are enabled, the destructor is recorded in the
           dynamic init entry (in case an exception is thrown during
           construction of the object).
       (4) If the member is an array, the dynamic init for the array as a
           whole is built.
  */
  prev_cip = NULL;
  for (cip = cib.cip_list; cip != NULL; cip = next_cip) {
    a_boolean          is_const_qualified, is_ref;
    a_source_position  err_pos;
    /* object_class_type is the type of the object being created.
       For base classes it will be different than the type associated
       with the constructor being called.  For fields it will be
       the same as the field type.  This is needed to check protected
       member access. */
    a_type_ptr         object_class_type;
    a_symbol_ptr       field_sym;
    next_cip = cip->next;
    if (cip->kind == (a_constructor_init_kind)cik_field) {
      field_sym = symbol_for(cip->variant.field);
      if (field_sym != NULL) {
        /* Check the initialization of anonymous unions (variants).  When we
           see the entry for the first variant field, we look ahead to check
           if it has an explicit initializer in this ctor-initializer, or if
           it is initialized via a field initializer.  When we see the last
           variant field, we set a flag to clear the variant state on the next
           iteration (we cannot do it at the end of the loop body because it
           is short-circuited in various ways). */
        if (variant_complete) {
          /* We saw the last field of a variant in the previous iteration.
             Reset the variant tracking variables for any other anonymous
             union that might follow. */
          in_variant = FALSE;
          variant_complete = FALSE;
          variant_init = FALSE;
          variant_explicit_init = FALSE;
        }  /* if */
        if (field_sym->variant.field.extra_info->is_first_variant_member) {
          /* The first field of an anonymous union: Look ahead through the
             entries for this union to check if any are initialized by an
             explicit mem-initializer or by a field initializer. */
          check_variant_has_initializer(cip, &variant_init,
                                        &variant_explicit_init);
          in_variant = TRUE;
          if (ctor_rout->is_constexpr && !variant_init) {
            /* If this is a constexpr constructor, each variant must have
               an initializer. */
            pos2_diagnostic(
                        es_error,
                        ec_constexpr_constructor_initializes_no_variant_field,
                        &error_position,
                        &field_sym->variant.field.anonymous_parent_object
                                  ->decl_position);
          }  /* if */
        }  /* if */
        if (field_sym->variant.field.extra_info->is_last_variant_member) {
          variant_complete = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
    dip = cip->initializer;
    /* If this was an explicit specialization, check whether an object
       lifetime needs to be restored to the IL. */
    if (dip != NULL && dip->kind != (a_dynamic_init_kind)dik_none) {
      /* Restore the object lifetime in the object lifetime tree now that we
         have the initializers in initialization order (rather than source
         order). */
      restore_mem_init_object_lifetime(dip);
      /* Unless this is an array type or exception processing is enabled, this
         is all that's required for explicit initializations. */
      if (exceptions_enabled ||
          (cip->kind == (a_constructor_init_kind)cik_field &&
           is_array_type(cip->variant.field->type))) {
        /* Further processing of explicit initializations is needed. */
      } else {
        /* Proceed on through the ctor-init list. */
        prev_cip = cip;
        continue;
      }  /* if */
    }  /* if */
    array_type = NULL;
    object_class_type = NULL;
    object_qualifiers = TQ_NONE;
    cssp = NULL;
    is_const_qualified = FALSE;
    is_ref = FALSE;
    if (user_defined) err_pos = pos_curr_token;
    if (cip->kind == (a_constructor_init_kind)cik_field) {
      /* Get the field type.  For arrays, we want the element type. */
      tp = cip->variant.field->type;
      object_qualifiers = get_type_qualifiers(tp);
      if (is_const_qualified_type(tp)) is_const_qualified = TRUE;
      tp = skip_typerefs(tp);
      if (is_array_type(tp)) {
        array_type = tp;
        tp = f_skip_typerefs(underlying_array_element_type(tp));
      } else if (is_any_reference_type(tp)) {
        is_ref = TRUE;
      }  /* if */
      object_class_type = tp;
      if (is_class_struct_union_type(tp)) {
        cssp = symbol_supplement_for_class(tp);
      }  /* if */
      if (!user_defined) {
        err_pos = cip->variant.field->source_corresp.decl_position;
      }
    } else {
      /* Get the type of the base class. */
      tp = cip->variant.base_class->type;
      cssp = symbol_supplement_for_class(tp);
      object_class_type = class_type;
      if (!user_defined) err_pos = cip->variant.base_class->decl_position;
    }  /* if */
    /* Do processing for implicit initializations. */
    if (dip == NULL || dip->kind == (a_dynamic_init_kind)dik_none) {
      if (is_generated_cctor || is_generated_mctor) {
        /* The constructor for the object as a whole is a generated copy/move
           constructor.  Any subobject constructors must also be copy/move
           constructors, and fields and base classes that have no constructor
           must be accounted for, too. */
        a_boolean  bitwise_copy = FALSE;
        if (cssp == NULL) {
          bitwise_copy = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
        } else if (cppcli_enabled &&
                   cli_class_type_kind_is(class_type, cctk_value) &&
                   cip->kind != (a_constructor_init_kind)cik_field) {
          /* Value class types are bit-copyable even though they derive from
             class types (e.g., System::Object) that are not marked as such
             (and which cannot go through the non-bitwise processing below
             because they have no copy constructor). */
          check_assertion(ctor_rout->is_trivial_copy_function);
          bitwise_copy = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        } else {
          /* "required_qualifiers" describes the qualifiers on an object that
             the top-level constructor can accept for copying; if it is
             non-zero, then all constructors called to copy subobjects must
             also accept such objects for copying (a conclusion based in part
             on ARM 12.8 -- this is clear for const and is applied by analogy
             to volatile and to other qualifiers, if any).  If construction
             by bitwise copy is allowed for this class, bitwise_copy will be
             returned TRUE. */
          a_type_qualifier_set  eff_qualifiers = required_qualifiers |
                                                 object_qualifiers;
          if (cip->kind == (a_constructor_init_kind)cik_field &&
              cip->variant.field->is_mutable) {
            /* Ignore constness of enclosing objects for mutable fields. */
            eff_qualifiers &= ~(a_type_qualifier_set)TQ_CONST;
          }  /* if */
          rp = select_copy_constructor(tp, eff_qualifiers, is_generated_mctor,
                                       &err_pos, object_class_type,
                                       &bitwise_copy,
                                       /*allow_suppressed_ctor=*/FALSE);
        }  /* if */
        if (bitwise_copy) {
          /* Construction by bitwise copy is allowed. */
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_bitwise_copy);
        } else if (rp == NULL) {
          /* The copy/move constructor was invalid in some way or other. */
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
        } else {
          /* A valid copy/move constructor does exist.  Generate the dynamic
             init entry. */
          dip = alloc_ctor_dynamic_init(rp, /*implied_source=*/TRUE);
        }  /* if */
      } else if (ctor_rout->is_inheriting_ctor &&
                 cip->kind != (a_constructor_init_kind)cik_field &&
                 identical_types(cip->variant.base_class->type,
                                 ctor_rout->generating_using_decl
                                          ->qualifier.class_type)) {
        /* This is the forwarding initializer for an inheriting constructor. */
        dip = forwarding_initializer_for_inheriting_constructor(ctor_rout);
      } else if (field_initializers_enabled &&
                 cip->kind == (a_constructor_init_kind)cik_field &&
                 cip->variant.field->has_initializer) {
        /* An implicit constructor-init entry for a field that has an
           initializer associated with it.  Set the flag indicating that the
           field initializer should be used, and move on to the next entry. */
        if ((is_union && has_explicit_field_init) ||
            (in_variant && variant_explicit_init)) {
          /* An explicit mem-initializer for a field supersedes variant field
             initializers.  Remove cip from the list. */
          if (prev_cip == NULL) {
            cib.cip_list = cip->next;
          } else {
            prev_cip->next = cip->next;
          }  /* if */
          cip->next = NULL;
          /* prev_cip remains unchanged. */
        } else {
          a_field_ptr   field = cip->variant.field;
          cip->use_field_initializer = TRUE;
          has_field_init = TRUE;
          /* Ensure the field initializer is scanned if necessary. */
          scan_field_initializer_if_needed(field, class_type);
          if (ctor_rout->is_constexpr) {
            if (field->initializer == NULL) {
              pos_sy_error(ec_unbounded_constexpr_ctor_init_recursion,
                           &err_pos, field_sym);
              field->initializer = make_error_constant_dynamic_init();
            } else if (field->has_nonconstant_initializer) {
              /* If the field initializer is known not to be a constant, it
                 cannot be used for constexpr construction. */
              if (!ctor_rout->is_template_function ||
                  ctor_rout->is_specialized) {
                pos_sy_error(
                          ec_nonconstant_field_initializer_in_mem_initializer,
                          &err_pos, field_sym);
                bad_call_for_constexpr_ctor_reported = TRUE;
              } else if (!ctor_rout->is_prototype_instantiation) {
                /* The is_constexpr flag must be cleared, but not until all
                   fields have been examined. */
                clear_constexpr_flag = TRUE;
              }  /* if */
            }  /* if */
          }  /* if */
          prev_cip = cip;
        }  /* if */
        continue;
      } else if (in_variant || is_union) {
        /* An entry for an uninitialized union member: Remove it (we only keep
           the entries for fields with initializers). */
        check_assertion_or_expect_error(
                             cip->kind == (a_constructor_init_kind)cik_field);
        if (prev_cip == NULL) {
          cib.cip_list = cip->next;
        } else {
          prev_cip->next = cip->next;
        }  /* if */
        cip->next = NULL;
        /* prev_cip remains unchanged. */
        continue;
      } else {
        /* No copy/move constructor is required.  If any constructor exists,
           the default constructor should be called. */
        if (cip->kind == (a_constructor_init_kind)cik_field &&
            (is_ref || is_const_qualified || ctor_rout->is_constexpr)) {
          /* An uninitialized field that probably requires initialization.
             That includes ref-type fields and const-qualified fields, as well
             as any non-variant field for a constexpr constructor. */
          if (is_union) {
            /* We don't issue diagnostics on initializing union members,
               partly because it's not well defined what should happen when
               const and non-const members are mixed, */
          } else if (!is_ref && 
                     ((cssp != NULL &&
                       !(ctor_rout->is_constexpr &&
                         tp->variant.class_struct_union
                                    .has_zero_init_component &&
                         has_trivial_default_constructor(cssp)) &&
                       (is_const_qualified ?
                            cssp->has_user_provided_default_constructor
                          : has_any_default_constructor(cssp))) ||
                      (ctor_rout->is_constexpr ?
                                               is_template_param_type(tp) :
                                               is_template_dependent_type(tp))
                      if_microsoft_extensions(|| is_value_class_type(tp)))) {
            /* A non-reference field may be initialized without an explicit
               initializer if it is of class type and there is a default
               constructor for the class (for a const-qualified field, it
               must be a user-provided default constructor).  For constexpr
               constructors, however, trivial member constructors are only
               valid if the member's class has no initializable members
               (otherwise the trivial constructor is not itself constexpr).
               Microsoft also treats value class types as initialized in this
               context.  Note that value class types that map to fundamental
               types -- like System::Int32 -- are treated like fundamental
               types (this matches Microsoft behavior). */
          } else {
             /* There may be more than one uninitialized const or ref field,
                so we wait to collect them all before issuing the error. */
            a_constructor_init_ptr  diag_cip;
            if (ctor_rout->is_constexpr && (!ctor_rout->is_template_function ||
                                            ctor_rout->is_specialized)) {
              /* For constexpr constructors that aren't template instances the
                 initializer entry should remain on the list so folding has
                 something to work with.  (For template instances, the
                 constructor will be treated as non-constexpr and so folding
                 will not be involved.)  Use a copy of the entry for
                 diagnostic purposes instead. */
              diag_cip = alloc_ctor_init(cip->kind);
              *diag_cip = *cip;
              cip->initializer = make_error_constant_dynamic_init();
            } else {
              /* Remove cip from the list. */
              if (prev_cip == NULL) {
                cib.cip_list = cip->next;
              } else {
                prev_cip->next = cip->next;
              }  /* if */
              diag_cip = cip;
            }  /* if */
            diag_cip->next = NULL;
            /* Add it to a list that identifies fields that need to be
               initialized but have no initializer. */
            if (uninit_list == NULL) {
              uninit_list = diag_cip;
            } else {
              end_of_uninit_list->next = diag_cip;
            }  /* if */
            end_of_uninit_list = diag_cip;
            if (is_any_reference_type(tp)) {
              any_ref_member_on_uninit_list = TRUE;
            }  /* if */
            continue;
          }  /* if */
        }  /* if */
        if (cssp != NULL) {
          if (cssp->is_POD) {
            if (tp->variant.class_struct_union.any_const_member) {
              if (cip->kind == (a_constructor_init_kind)cik_field) {
                pos_sy_error(ec_uninitialized_field_with_const_member,
                             &err_pos, field_sym);
              } else {
                pos_ty_error(ec_uninitialized_base_class_with_const_member,
                             &err_pos, tp);
              }  /* if */
            }  /* if */
          } else {
            /* If there is a trivial default constructor for this class,
               treat this as a reference to it. */
            (void)reference_to_trivial_default_constructor(
                               tp, class_type, &err_pos, /*check_access=*/TRUE,
                               (a_boolean *)NULL);
          }  /* if */
        }  /* if */
        /* Consider dropping the ctor-initializer entry if it isn't needed. */
        if (ctor_rout->is_constexpr && !is_union) {
          /* For classes and structs, every subobject must be initialized by a
             constexpr constructor, and we want that to be represented
             explicitly.  (For unions, exactly one field should be initialized;
             that is checked elsewhere.) */
        } else if (cssp == NULL ||
                   is_template_param_or_nonreal_class_type(tp) ||
                   (has_trivial_default_constructor(cssp) &&
                    (!exceptions_enabled || cssp->has_trivial_destructor))) {
          /* This constructor initializer entry is likely not really needed.
             It may be the result of an empty initializer on a field or it may
             be associated with a base class without a constructor. */
          if (cip->source_expr != NULL) {
            /* A special case: An explicit array initializer in a template (if
               it weren't in a template, we wouldn't be here since a nontrivial
               dynamic initialization entry would have been generated).  This
               can currently only happen in GNU C++ mode.  Don't drop the
               constructor initializer entry: It might be needed in the C++-
               generating back end, for example. */
            check_assertion(
                      gpp_mode && prototype_instantiations_in_il &&
                      cip->kind == (a_constructor_init_kind)cik_field &&
                      (is_template_dependent_type(cip->source_expr->type) ||
                       is_template_dependent_type(cip->variant.field->type)));
          } else {
            /* Unlink the constructor initializer entry from the list. */
            if (prev_cip == NULL) {
              cib.cip_list = cip->next;
            } else {
              prev_cip->next = cip->next;
            }  /* if */
          }  /* if */
          continue;
        }  /* if */
        if (is_immediate_class_type(tp)) {
          rp = select_default_constructor(tp, &err_pos, object_class_type,
                                          (a_boolean *)NULL);
        } else {
          rp = NULL;
        }  /* if */
        if (rp == NULL) {
          /* No constructor to call. */
          if (ctor_rout->is_constexpr &&
              is_immediate_class_type(tp) &&
              tp->variant.class_struct_union.has_zero_init_component) {
            /* The base has a component that requires initialization and the
               (trivial) default constructor won't do that initialization:
               That is not permitted in a constexpr constructor (which must
               fully initialize the object). */
            pos_ty_error(ec_constexpr_ctor_does_not_initialize_base,
                         &err_pos, tp);
            dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constant);
            dip->variant.constant = alloc_error_constant();
          } else {
            check_assertion(is_immediate_class_type(tp) ||
                            tp->kind == (a_type_kind)tk_template_param);
            dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
          }  /* if */
        } else {
          /* A default constructor does exist.  Generate the dynamic init
             entry. */
          dip = alloc_ctor_dynamic_init(rp, /*implied_source=*/FALSE);
          if (ctor_rout->is_constexpr && !rp->is_constexpr) {
            /* Check that a constexpr constructor doesn't call a non-
               constexpr constructor.  For compiler-generated constructors
               and for template instances failing this test isn't an error,
               but it makes the function effectively non-constexpr. */
            if (ctor_rout->is_declared_constexpr &&
                (!ctor_rout->is_template_function ||
                 ctor_rout->is_specialized)) {
              if (!bad_call_for_constexpr_ctor_reported) {
                pos_sy_error(ec_nonconstexpr_call_in_mem_initializer, &err_pos,
                             symbol_for(rp));
                bad_call_for_constexpr_ctor_reported = TRUE;
              }  /* if */
            } else if (!ctor_rout->is_prototype_instantiation) {
              ctor_rout->is_constexpr = FALSE;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
      /* Attach the new dynamic init entry to the constructor initializer. */
      dip->is_constructor_init = TRUE;
      cip->initializer = dip;
    }  /* if */
    /* Do processing for both implicitly and explicitly initialized members
       when exception handling is enabled. */
    if (exceptions_enabled) {
      if (cssp != NULL) {
        /* Since an exception could be thrown after this subobject is
           constructed but before construction of the entire object is
           complete, record the destructor in the dynamic-init entry. */
        if (dip->destructor != NULL) {
          /* This must be an entry for an explicit initialization, for which
             the destructor will already have been filled in. */
        } else {
          /* Implicit initialization -- the destructor has not yet been
             looked up. */
          dip->destructor = select_destructor(tp, object_class_type, &err_pos);
        }  /* if */
        /* Record the need for a destruction in the context of the current
           lifetime if dip->destructor != NULL.   Note: when the field is an
           array, it is the dynamic init entry for the array element that is
           being handled at this time; the array as a whole is dealt with
           below. */
        record_end_of_lifetime_destruction(dip, /*static_lifetime=*/FALSE,
                                           /*block_lifetime=*/TRUE);
      }  /* if */
    }  /* if */
    /* Do processing for both implicitly and explicitly initialized members
       when the field is an array of constructible elements. */
    if (array_type != NULL && dip->is_constructor_init &&
        (dip->kind == (a_dynamic_init_kind)dik_constructor ||
         (dip->kind == (a_dynamic_init_kind)dik_constant &&
                       dip->variant.constant->is_result_of_constexpr_call))) {
      /* We have an array whose elements are constructible.  dip is the
         dynamic init entry for the element.  Create a dynamic init entry to
         represent the initialization of the array as a whole. */
      cip->initializer = repeat_mem_init_for_array(dip, array_type);
    }  /* if */
    /* Continue through the ctor-init list. */
    prev_cip = cip;
  }  /* for */
  if (uninit_list != NULL) {
    /* Issue a diagnostic for uninitialized const and ref members. */
    an_error_severity  severity = es_error;
    if (ctor_rout->compiler_generated) {
      /* Error by 12.1 [class.ctor]. */
      pos_ty_start_diagnostic(severity, ec_cannot_initialize_fields,
                              &class_type->source_corresp.decl_position,
                              class_type);
    } else {
      /* This is a user-defined constructor, subject to restrictions in
         12.6.2 [class.base.init] para 4.  However, if only const members are
         involved, a discretionary error (or a warning, in early GNU C++ mode)
         is issued. */
      an_error_code  errcode = ec_missing_initializer_on_fields;
      if (ctor_rout->is_constexpr) {
        if (!ctor_rout->is_template_function || ctor_rout->is_specialized) {
          /* Use a slightly different wording for constexpr constructors. */
          errcode = ec_missing_initializer_on_fields_with_constexpr_ctor;
        } else {
          /* For template instantiations, this is not invalid: The constructor
             instance is, however, not treated as "constexpr". */
          severity = es_none;
          if (!ctor_rout->is_prototype_instantiation) {
            clear_constexpr_flag = TRUE;
          }  /* if */
        }  /* if */
      } else if (!any_ref_member_on_uninit_list) {
        severity = (gpp_mode && gnu_version < 30400) ? es_warning
                                                     : es_discretionary_error;
      }  /* if */
      if (severity != es_none) {
        pos_sy_start_diagnostic(severity, errcode, &pos_curr_token,
                                symbol_for(ctor_rout));
      }  /* if */
    }  /* if */
    if (severity != es_none) {
      for (cip = uninit_list; cip != NULL; cip = cip->next) {
        a_symbol_ptr field_sym = symbol_for(cip->variant.field);
        if (ctor_rout->is_constexpr) {
          /* The field being a reference or a const member is not relevant, so
             we use a diagnostic that doesn't emphasize that. */
          sym_add_diag_info(ec_specific_symbol, field_sym);
        } else if (is_any_reference_type(cip->variant.field->type)) {
#if MICROSOFT_EXTENSIONS_ALLOWED
          /* Fields cannot be tracking references. */
          check_assertion(!cppcli_enabled || !is_tracking_reference_type(tp));
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          sym_add_diag_info(ec_reference_member, field_sym);
        } else {
          /* Must be a const member. */
          sym_add_diag_info(ec_const_member, field_sym);
        }  /* if */
      }  /* for */
      end_error();
    }  /* if */
  } else if (is_union && ctor_rout->is_constexpr && has_field &&
             !has_field_init) {
    /* A constexpr constructor for a union must initialize a field
       explicitly. */
    pos_error(ec_union_constexpr_constructor_initializes_no_field,
              &error_position);
  }  /* if */
  if (clear_constexpr_flag) {
    ctor_rout->is_constexpr = FALSE;
  }  /* if */
#if NEW_CAN_BE_FOLDED_INTO_CTOR
  if (ctor_rout->is_trivial_default_constructor && !ctor_rout->is_defaulted) {
    /* IL will not be put out for an implicitly-generated trivial default
       constructor anyway, so there's no need to deal with default operator
       new. */
  } else {
    /* Determine and remember the default operator new() routine for the
       class.  This is done here because we are working out the "wrapper"
       code that will be required, and the "new" routine will be called from
       the wrapper. */
    a_routine_ptr new_routine;
    set_class_assoc_operator_new_routine(class_type);
    new_routine = ctsp->assoc_operator_new_routine;
    if (new_routine != NULL) {
      mark_routine_referenced(new_routine);
      new_routine->called = TRUE;
      if (exceptions_enabled) {
        a_routine_ptr delete_routine;
        /* When exceptions are enabled, the constructor has to be able to
           delete the storage allocated if an exception is thrown, so it
           needs the delete routine too. */
        set_class_assoc_operator_delete_routine(class_type);
        delete_routine = ctsp->assoc_operator_delete_routine;
        if (delete_routine != NULL) {
          mark_routine_referenced(delete_routine);
          delete_routine->called = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
#if DEBUG
  if (debug_level >= 3 || db_flag_is_set("dump_init")) {
    db_symbol((a_symbol_ptr)ctor_rout->source_corresp.assoc_info,
              "constructor: ", 2);
    for (cip = cib.cip_list; cip != NULL; cip = cip->next) {
      if (cip->kind == (a_constructor_init_kind)cik_field) {
        sym = symbol_for(cip->variant.field);
      } else {
        sym = symbol_for(cip->variant.base_class->type);
      }  /* if */
      fprintf(f_debug, "    initializer for %s %s%s: %s",
                       (cip->kind == (a_constructor_init_kind)cik_field) ?
                          "field" : "base class",
                       sym->header->identifier,
                       cip->compiler_generated ? " (compiler-generated)" : "",
                       (cip->initializer == NULL) ? " <none>\n" : "\n      ");
      if (cip->initializer != NULL) {
        db_dynamic_initializer(cip->initializer, 6);
      }  /* if */
    }  /* for */
  }  /* if */
#endif /* DEBUG */
done:
  db_exit();
  return cib.cip_list;
}  /* ctor_initializer */


a_constructor_init_ptr dtor_initializer(a_routine_ptr  dtor_rout)
/*
Return a list of constructor-init entries describing implicit destructor
calls required when the destructor dtor_rout is invoked.  (Constructor-init
entries are used because of the similarity to constructor processing, even
though neither constructors nor initialization is involved here.)
*/
{
  a_type_ptr                    class_type, tp;
  a_symbol_ptr                  sym, class_sym;
  a_constructor_init_ptr        cip;
  a_boolean                     is_virtual_pass;
  a_constructor_init_ptr        cip_list;
  a_routine_ptr                 rp;
  a_base_class_ptr              bcp;
  a_dynamic_init_ptr            dip;
  a_class_type_supplement_ptr   ctsp;
  a_source_position             source_pos;
  a_boolean                     in_variant = FALSE, variant_complete = FALSE;

  db_enter(3, "dtor_initializer");
  cip_list = NULL;
  class_type = parent_class_of(dtor_rout);
  check_assertion(class_type != NULL);
  ctsp = class_type_supp(class_type);
  source_pos = dtor_rout->source_corresp.decl_position;
  if (class_type->kind == (a_type_kind)tk_union) {
    /* Subobjects of variant members are not automatically destroyed.  So
       nothing must be done for unions.  (Anonymous union members are handled
       below.) */
    goto past_subobject_destructions;
  }  /* if */
  /* The order of destructor calls is exactly the reverse of the order of
     constructor calls.  In other words, destructors for virtual base classes
     are last, preceded by destructors for nonvirtual direct base classes,
     with destructors for members coming first (ARM 12.4).  Thus we follow the
     logic in ctor_initializer, except that the lists are built backwards and
     merged backwards.   First construct the lists for virtual base classes
     and nonvirtual direct base classes. */
  /* First loop through the base classes looking for virtual base classes. */
  is_virtual_pass = TRUE;
  for (;;) {
    for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
      /* On the first pass select out virtual base classes; on the second
         pass select out direct non-virtual base classes. */
      if (is_virtual_pass ? bcp->is_virtual :
                            (bcp->direct && !bcp->is_virtual)) {
        /* If the virtual base class or direct base class has a destructor, a
           dynamic init entry will be required. */
        rp = select_destructor(bcp->type, class_type, &source_pos);
        if (rp != NULL) {
          cip = alloc_ctor_init((a_constructor_init_kind)
                                                    (bcp->is_virtual ?
                                                     cik_virtual_base_class :
                                                     cik_direct_base_class));
          cip->variant.base_class = bcp;
          cip->compiler_generated = TRUE;
          /* Create a dynamic init entry. */
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
          dip->destructor = rp;
          dip->is_constructor_init = TRUE;
          if (exceptions_enabled) {
            /* Create a destruction entry and associate it with the
               appropriate object-lifetime entry. */
            record_end_of_lifetime_destruction(dip, /*static_lifetime=*/FALSE,
                                               /*block_lifetime=*/TRUE);
          }  /* if */
          /* Attach the new dynamic init entry to the constructor
             initializer. */
          cip->initializer = dip;
          /* Add the constructor init to the end of the appropriate list. */
          cip->next = cip_list;
          cip_list = cip;
        }  /* if */
      }  /* if */
    }  /* for */
    if (is_virtual_pass) {
      /* Repeat the loop through the base classes, this time picking up the
         direct base classes. */
      is_virtual_pass = FALSE;
    } else {
      /* Only go through twice. */
      break;
    }  /* if */
  }  /* for */
  /* Now add entries for destructors required by nonstatic data members.
     Loop through the symbol list for the class, not the field list, since
     the symbol list contains only user-defined fields whereas the field
     list may also include compiler-generated field entries. */
  class_sym = symbol_for(class_type);
  for (sym = class_sym->variant.class_struct_union.extra_info->symbols;
       sym != NULL;
       sym = sym->next_in_scope) {
    if (sym->kind == (a_symbol_kind)sk_field) {
      /* sym represents a field.  Determine whether a destructor exists. */
      a_field_ptr field = sym->variant.field.ptr;
      if (microsoft_mode && field_is_property_or_event(field)) {
        /* Property and event fields are not really data members and should
           not be destroyed. */
        continue;
      }  /* if */
      if (variant_complete) {
        /* We saw the last field of a variant in the previous iteration.
           Reset the variant tracking variables for any other anonymous
           union that might follow. */
        in_variant = FALSE;
        variant_complete = FALSE;
      }  /* if */
      if (sym->variant.field.extra_info->is_first_variant_member) {
        in_variant = TRUE;
      }  /* if */
      if (sym->variant.field.extra_info->is_last_variant_member) {
        variant_complete = TRUE;
      }  /* if */
      if (in_variant) {
        /* Variant subobjects are not automatically destroyed. */
        continue;
      }  /* if */
      tp = skip_typerefs(field->type);
      /* For arrays get the element type, allowing for multidimensional
         arrays.  Flexible array members (and zero-length array members)
         need no destruction (these are extensions in some modes). */
      if (is_array_type(tp)) {
        if (tp->size == 0) {
          /* A zero-length array or a flexible array member. */
          continue;
        }  /* if */
        tp = underlying_array_element_type(tp);
        tp = skip_typerefs(tp);
      }  /* if */
      if (is_immediate_class_type(tp)) {
        rp = select_destructor(tp, tp, &source_pos);
        if (rp != NULL) {
          /* Create the constructor init entry for a field. */
          cip = alloc_ctor_init((a_constructor_init_kind)cik_field);
          cip->variant.field = field;
          cip->compiler_generated = TRUE;
          /* Create a dynamic init entry. */
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
          dip->destructor = rp;
          dip->is_constructor_init = TRUE;
          if (exceptions_enabled) {
            /* Create a destruction entry and associate it with the
               appropriate object-lifetime entry. */
            record_end_of_lifetime_destruction(dip, /*static_lifetime=*/FALSE,
                                               /*block_lifetime=*/TRUE);
          }  /* if */
          /* Attach the new dynamic init entry to the constructor
             initializer. */
          cip->initializer = dip;
          /* Add the entry to the start of the list. */
          cip->next = cip_list;
          cip_list = cip;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
past_subobject_destructions:
  /* If the destructor is virtual, the class must have a visible
     default operator delete() (core issue 252). */
  if (dtor_rout->is_virtual) {
    a_boolean do_check = TRUE;
    if (microsoft_mode) do_check = FALSE;
#if DO_IL_LOWERING && IA64_ABI
    /* The IA-64 ABI requires this check, because the deleting destructor
       references the delete routine. */
    do_check = TRUE;
#endif /* DO_IL_LOWERING && IA64_ABI */
    if (do_check) {
      a_symbol_ptr  del_sym, fund_del_sym;
      a_boolean     ambiguous;
      del_sym = find_class_assoc_operator_delete_routine(class_type,
                                                         &ambiguous);
      if (ambiguous) {
        /* The operator delete is ambiguous by inheritance. */
        pos_sy2_error(ec_implicit_call_of_ambiguous_name,
                      &source_pos, del_sym, symbol_for(dtor_rout));
      } else if (del_sym == NULL) {
        /* There is no visible default operator delete. */
        pos_error(ec_no_default_delete_in_virtual_dtor,
                  &source_pos);
      } else {
        /* There is an unambiguous operator delete.  Make sure it is accessible
           and not "deleted". */
        fund_del_sym = fundamental_symbol_of(del_sym);
        check_assertion(is_simple_function_symbol(fund_del_sym));
        if (fund_del_sym->variant.routine.ptr->is_deleted) {
          pos_sy_error(ec_deleted_function, &source_pos, del_sym);
        } else if (del_sym->is_class_member) {
          /* Check access to a member operator delete (it might be in a base
             class).  Note that the access is also checked on every delete. */
          a_symbol_locator locator;
          make_locator_for_symbol(del_sym, &locator);
          check_ambiguity_and_verify_access(&locator);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
#if DELETE_CAN_BE_FOLDED_INTO_DTOR
  /* Determine and remember the default operator delete() routine for the
     class.  This is done here because we are working out the "wrapper"
     code that will be required, and the "delete" routine may be called from
     the wrapper.  This is only an optimization, so if the delete routine
     turns out not to exist it is simply not recorded, and no error is
     issued (here). */
  { a_routine_ptr delete_routine;
    set_class_assoc_operator_delete_routine(class_type);
    delete_routine = ctsp->assoc_operator_delete_routine;
    if (delete_routine != NULL) {
      mark_routine_referenced(delete_routine);
      delete_routine->called = TRUE;
    }  /* if */
  }
#endif /* DELETE_CAN_BE_FOLDED_INTO_DTOR */
#if DEBUG
  if (debug_level >= 3 || db_flag_is_set("dump_init")) {
    db_symbol(symbol_for(dtor_rout), "destructor: ", 2);
    for (cip = cip_list; cip != NULL; cip = cip->next) {
      if (cip->kind == (a_constructor_init_kind)cik_field) {
        sym = symbol_for(cip->variant.field);
      } else {
        sym = symbol_for(cip->variant.base_class->type);
      }  /* if */
      fprintf(f_debug, "    destructor for %s %s%s: %s",
                       (cip->kind == (a_constructor_init_kind)cik_field) ?
                          "field" : "base class",
                       sym->header->identifier,
                       cip->compiler_generated ? " (compiler-generated)" : "",
                       (cip->initializer == NULL) ? " <none>\n" : "\n      ");
      if (cip->initializer != NULL) {
        db_dynamic_initializer(cip->initializer, 6);
      }  /* if */
    }  /* for */
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return cip_list;
}  /* dtor_initializer */


void check_for_missing_initializer_full(a_symbol_ptr  sym,
                                        a_type_ptr    type,
                                        a_boolean     explicitly_internal)
/*
This routine is called when no explicit, value, or default initialization has
occurred.  It determines whether an initializer should have been provided
and issues a diagnostic if appropriate.  It is used both for variable
declarations (when sym represents the variable) and for unnamed objects that
are created by a new expression (in which case sym is NULL).  In both cases
"type" points to the type of the object.  explicitly_internal is TRUE in the
case of a variable declaration that has internal linkage because of the
explicit presence of a "static" storage class specifier. 
*/
{
  a_variable_ptr       vp;
  a_boolean            init_required;
  a_base_class_ptr     bcp;
  a_boolean            is_empty_POD_class = FALSE;
  an_error_severity    severity;
  a_boolean            is_incomplete_array = FALSE;

  db_enter(4, "check_for_missing_initializer_full");
  if (sym != NULL) {
    /* This must be a variable or static data member declaration. */
    check_assertion(sym->kind == (a_symbol_kind)sk_variable ||
                    sym->kind == (a_symbol_kind)sk_static_data_member);
    vp = (sym->kind == (a_symbol_kind)sk_variable) ? 
          sym->variant.variable.ptr : sym->variant.static_data_member.variable;
  } else {
    /* This must be a "new" expression. */
    vp = NULL;
  }  /* if */
  if (is_any_reference_type(type)) {
    /* Note that a reference type object cannot be produced by new. */
    /* coverity[var_deref_op] */
    if (vp->storage_class != (a_storage_class)sc_extern) {
      /* Non-extern reference variables must be initialized (ARM 8.4.3). */
      sym_error(ec_missing_initializer_on_reference, sym);
    }  /* if */
  } else if (is_const_qualified_type(type)) {
    if (is_array_type(type)) {
      if (is_incomplete_type(type)) is_incomplete_array = TRUE;
      type = underlying_array_element_type(type);
    }  /* if */
    if (C_dialect == C_dialect_cplusplus &&
        is_class_struct_union_type(type)) {
      a_class_symbol_supplement_ptr  cssp = symbol_supplement_for_class(type);
      if (!cssp->any_nonstatic_data_members &&
          (cssp->is_POD || any_cfront_mode() || microsoft_mode)) {
        /* Uninitialized const object that is an "empty" POD class (i.e.,
           one with no nonstatic data members).  The WP probably requires
           initialization of const objects even when they are empty.  Other
           C++ compilers don't enforce such a restriction, however. */
        /* In cfront and Microsoft modes treat a non-POD empty class as
           though it were a POD. */
        is_empty_POD_class = TRUE;
      }  /* if */
    }  /* if */
    if (vp != NULL) {
      /* Uninitialized const variable.  In C++ this is permitted only for
         externally linked variables (without an initializer, they are not
         definitions), but not for static data member definitions.
         In ordinary C we issue a warning for local variables (both static
         and automatic) here, but the warning for static file scope variables
         is given later. */
      a_name_linkage_kind  name_linkage =
                         (a_name_linkage_kind)vp->source_corresp.name_linkage;
      if (C_dialect == C_dialect_cplusplus) {
        if (name_linkage == (a_name_linkage_kind)nlk_none ||
            (name_linkage == (a_name_linkage_kind)nlk_internal &&
             decl_scope_level <= depth_innermost_namespace_scope) ||
#if MICROSOFT_EXTENSIONS_ALLOWED
            ((vp->decl_modifiers & DM_DLLEXPORT) != 0 &&
             vp->storage_class != (a_storage_class)sc_extern) ||
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
            sym->kind == (a_symbol_kind)sk_static_data_member) {
          /* In C++ const qualified variables that are internally linked
             must be initialized (ARM 7.1.6). */
          if (is_empty_POD_class && !strict_ansi_mode &&
              !is_incomplete_array) {
            /* Except in strict mode, don't bother issuing a diagnostic on
               something like "const struct S { } s;". */
          } else if (!could_be_dependent_class_type(type)) {
            /* If the type is dependent and could end up being a class type
               after substitution no diagnostic should be issued since the
               substituting class type may have a default constructor. */
            /* By default, the diagnostic is an error. */
            severity = es_error;
            if (is_empty_POD_class && !is_incomplete_array) {
              severity = strict_ansi_error_severity;
            } else if (microsoft_mode) {
              if (is_class_struct_union_type(type) || is_enum_type(type)) {
                /* MSVC++ does not require an initializer for a const class or
                   enum variable with no default constructor. */
                severity = es_warning;
              } else if (explicitly_internal) {
                /* It is probably a bug that MSVC++ has different behavior on
                   the following:
                     const int i;         // Error (no initializer)
                     static const int j;  // No diagnostic
                */
                severity = es_warning;
              }  /* if */
            }  /* if */
            if (gpp_mode &&
                sym->kind == (a_symbol_kind)sk_static_data_member &&
                is_prototype_instantiation_context()) {
              /* g++ fails to diagnose a missing initializer for a static
                 data member at template definition time.  An error is
                 issued if the template is instantiated. */
              severity = es_warning;
            }  /* if */
            if (is_class_struct_union_type(type) && !is_incomplete_array &&
                !any_cfront_mode() && !microsoft_mode) {
               /* Even if the class has an implicitly declared default
                  constructor, a user-declared default constructor must be
                  present (WP 7.1.5.1 [dcl.cv]). */
              check_assertion(
                      !type_has_user_provided_default_constructor(type));
              pos_syty_diagnostic(severity,
                                  ec_missing_default_constructor_on_const,
                                  &error_position, sym, skip_typerefs(type));
            } else {
              /* Issue an error or warning on omitting the initializer. */
              sym_diagnostic(severity, ec_missing_initializer_on_const, sym);
            }  /* if */
          }  /* if */
        }  /* if */
      } else {
        /* Ordinary C -- a warning, and only on local variables. */
        if (name_linkage == (a_name_linkage_kind)nlk_none) {
          sym_warning(ec_missing_initializer_on_const, sym);
        }  /* if */
      }  /* if */
    } else {
      /* Uninitialized const new-object. 
         The case of a const temporary, as in:
           typedef X const CX; CX().f();
         also comes here (when X has no explicit default constructor). */
      if (any_cfront_mode() || microsoft_mode ||
          (is_empty_POD_class && !strict_ansi_mode)) {
        /* No diagnostic required. (The "empty const object" case parallels
           the case where such an object is a named variable (see above).
           It is diagnosed in strict mode only.) */
      } else {
        /* Issue a discretionary error. */
        if (is_class_struct_union_type(type)) {
          /* Even if the class has an implicitly declared default constructor,
             a user-declared default constructor must be present (WP 5.3.4
             [expr.new]). */
          check_assertion(!type_has_user_provided_default_constructor(type));
          pos_ty_diagnostic(es_discretionary_error,
                            ec_missing_default_constructor_on_unnamed_const,
                            &error_position, skip_typerefs(type));
        } else {
          diagnostic(es_discretionary_error,
                     ec_missing_initializer_on_unnamed_const);
        }  /* if */
      }  /* if */
    }  /* if */
  } else {
    if (is_array_type(type)) type = underlying_array_element_type(type);
    type = skip_typerefs(type);
    if (C_mode() && is_union_type(type)) {
      /* In C, no diagnostic on unions with const members. */
    } else if (is_class_struct_union_type(type) &&
               (vp == NULL /* A new-expression */ ||
                vp->storage_class != (a_storage_class)sc_extern)) {
      /* The object is a class-struct-union type or an array whose element
         type is a class-struct-union type.  Issue a warning if there is a
         const qualified field or a field of reference type.  Note that this
         check is not explicitly mandated by the ARM (though it is implied in
         12.6.2:  "The argument list . . . is the only way to initialize
         nonstatic const and reference members").  Cfront issues an error on
         class declarations that contain nonstatic const or reference members
         and no constructor, but this seems to introduce an unnecessary
         incompatibility with C. */
      init_required = FALSE;
      if (type->variant.class_struct_union.any_const_member ||
          (C_dialect == C_dialect_cplusplus &&
           symbol_supplement_for_class(type)->any_ref_member)) {
        /* The class itself has a const or ref member that is not being
           initialized. */
        init_required = TRUE;
      } else if (C_dialect == C_dialect_cplusplus) {
        /* Check each of the base classes.  Note that we don't check whether
           there's a constructor in the base class, since if there were the
           derived class would have to have constructor, too. */
        for (bcp = base_classes_of(type); bcp != NULL; bcp = bcp->next) {
          type = bcp->type;
          if (type->variant.class_struct_union.any_const_member ||
              symbol_supplement_for_class(type)->any_ref_member) {
            /* One of the base classes has a const or ref member
               that is not being initialized. */
            init_required = TRUE;
            break;
          }  /* if */
        }  /* for */
      }  /* if */
      if (init_required) {
        if (sym != NULL) {
          /* Variable declaration -- display the symbol. */
          an_error_code code;
          if (C_dialect == C_dialect_cplusplus) {
            code = ec_var_with_uninitialized_member;
            severity = es_discretionary_error;
          } else {
            code = ec_var_with_uninitialized_field;
            severity = es_warning;
          }  /* if */
          pos_sy_diagnostic(severity, code, &sym->decl_position, sym);
        } else {
          /* New object -- there's no name to display. (C++ only.) */
          diagnostic(es_discretionary_error,
                     ec_unnamed_object_with_uninitialized_field);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
}  /* check_for_missing_initializer_full */


void decl_inits_one_time_init(void)
/*
Do one-time initialization of variables related to initialization processing.
(Variables that need to be reinitialized with each new translation unit are
handled in decl_inits_init.)
*/
{
  /* Save variables that are needed for precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(ctor_delegation_map),
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  /* Register variables (and arrays) that have distinct copies for distinct
     compilation units. */
  register_trans_unit_variable(ctor_delegation_map);
}  /* decl_inits_one_time_init */


void decl_inits_trans_unit_init(void)
/*
Initialize static variables related to initialization processing.  These are
variables that need initialization for every (primary and secondary)
translation unit.
*/
{
  ctor_delegation_map = NULL;
}  /* decl_inits_trans_unit_init */


void decl_inits_init(void)
/*
Initialize static variables related to initialization processing.  This is
done as a subroutine (rather than relying on static initialization) so that it
can be redone to compile more than one source file in a single invocation of
the front end.
*/
{
}  /* decl_inits_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2013 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
