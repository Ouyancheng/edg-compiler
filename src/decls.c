/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

decls.c -- Scanning of declarations.

*/

#include "basics.h"
#include "target.h"
#include "decls.h"
#include "decl_spec.h"
#include "declarator.h"
#include "def_arg.h"
#include "class_decl.h"
#include "cmd_line.h"
#include "decl_inits.h"
#include "error.h"
#include "func_def.h"
#include "il.h"
#include "lexical.h"
#include "types.h"
#include "lang_feat.h"
#include "mem_tables.h"
#include "mem_manage.h"
#include "pch.h"
#include "pragma.h"
#include "preproc.h"
#include "statements.h"
#include "symbol_tbl.h"
#include "symbol_ref.h"
#include "templates.h"
#if ASM_FUNCTION_ALLOWED
#include "asm_func.h"
#endif /* ASM_FUNCTION_ALLOWED */

/*
Macro that is TRUE if the current token is the start of a storage class
specifier (3.5.1).
*/
#define is_storage_class()                                            \
  (curr_token == tok_typedef  || curr_token == tok_extern   ||        \
   curr_token == tok_static   || curr_token == tok_auto     ||        \
   curr_token == tok_register)

/*
Macro that is TRUE if the current token is the start of a function
specifier.
*/
#define is_function_specifier()                                      \
  (curr_token == tok_inline   || curr_token == tok_virtual)


a_symbol_ptr curr_type_symbol(a_boolean is_new_type_name,
                              a_boolean in_prescan)
/*
The current token is an identifier or, in C++, the "::" at the start of a
global qualified name.  If it is the name of a type (a typedef name or,
in C++, the name of a class, struct, union, or enum), return a pointer to
the symbol.  Otherwise, return NULL.  Ambiguity and access control checking
is not done.  in_prescan is TRUE when we are called from the prescanning
routines used for disambiguation.  This flag suppresses errors that
might result from class template names that are missing argument lists.
*/
{
  a_symbol_ptr			assoc_symbol;
  a_boolean   			err;
  an_identifier_options_set	options;

  assoc_symbol = NULL;
  /* Set the options.  Since this call is a "probe" to determine if the
     current identifier is a type name, don't issue access errors yet, and
     don't complain if the name is that of a template but there are no
     template args (since it may actually be a different use of the name). */
  options = GID_DEFER_ACCESS_ERRORS;
  if (is_new_type_name) options |= GID_IS_NEW_TYPE_NAME;
  if (in_prescan) options |= GID_TEMPLATE_ARGS_OPTIONAL;
  if (is_generalized_identifier_start(options)) {
    if (locator_for_curr_id.is_operator_name ||
        locator_for_curr_id.is_conversion_name) {
      /* Cannot be a type name. */
    } else {
      /* Look up the current token identifier, which may be a qualified name.
         Since curr_type_symbol is often called as part of a test of the
         presence of a type name identifier, it is inappropriate to cause a
         projection symbol to be created in the current scope if in fact it
         projects something other than a type name.  It's easier to suppress
         the creation of such gratuitous projections here than to try to ignore
         them in symbol entry later.  Defer any access errors that may occur
         because we may actually be scanning something that is not a type
         (e.g., a declarator). */
      assoc_symbol =
          coalesce_and_lookup_generalized_identifier(options,
                                                     ilm_tentative_type, &err);
      if (assoc_symbol != NULL && !is_type_symbol(assoc_symbol)) {
        /* Symbol was found, but it is not a type name symbol.  Return NULL. */
        assoc_symbol = NULL;
      }  /* if */
      /* If a type symbol was found, issue any access errors that may have
         occurred. */
      if (assoc_symbol) {
        issue_qualifier_access_errors(&locator_for_curr_id.access_errors);
      }  /* if */
    }  /* if */
  }  /* if */
  return assoc_symbol;
}  /* curr_type_symbol */


/*
Macro that is TRUE if the current token is an identifier that represents
the name of a type (a typedef name or, in C++, the name of a class, struct,
union, or enum).  Also works if the current is the "::" at the start of
a global qualified name.
*/
#define is_type_name() (is_qualified_name_start() && curr_id_is_type_name())


a_boolean is_type_start(void)
/*
Return TRUE if the current token looks like the start of a type.  A type
starts with a type-specifier (including a typedef name) or a type-qualifier.
*/
{
  a_boolean    is_start = FALSE;

  if (is_type_specifier() || is_type_qualifier() ||
      is_function_specifier() || curr_token == tok_friend) {
    is_start = TRUE;
  } else if (is_type_name()) {
    /* Identifier that is a type name (a typedef name or, in C++,
       the name of a class, struct, or union). */
    is_start = TRUE;
  } else if (curr_token == tok_identifier && locator_for_curr_id.is_error &&
             locator_for_curr_id.is_template_id) {
    /* This is an error case -- presumably, an ill-formed template-id -- but
       it is treated as the start of a type anyway. */
    is_start = TRUE;
  }  /* if */
  return(is_start);
}  /* is_type_start */


a_boolean is_decl_start(a_boolean  expr_context,
                        a_boolean  real_declarator_allowed)
/*
Return TRUE if the current token looks like the start of a declaration,
i.e., it is the start of a type-specifier, a type-qualifier, or a
storage-class-specifier.  Note that this does not cover the start of
function-definitions, since they can start with the declarator.  If
expr_context is TRUE this test is done in a context in which an expression
is allowed.  If real_declarator_allowed is FALSE the error recovery
optimization is suppressed.
*/
{
  a_boolean     is_start = FALSE;
  a_token_kind  next_tok;

  if (is_storage_class()) {
    /* A storage-class-specifier. */
    is_start = TRUE;
  } else if (curr_token == tok_template) {
    /* Probably an error. */
    is_start = TRUE;
  } else if (is_type_start()) {
    /* Is start of type. */
    is_start = TRUE;
  } else if (curr_token == tok_identifier &&
             !is_error_locator(locator_for_curr_id)) {
    /* A special check to produce better error recovery in certain cases.
       If the lexical sequence suggests that this is a declaration even
       though the current identifier is not defined (and therefore not
       recognized as a type name), call it a declaration anyway. */
    if (!real_declarator_allowed) {
      /* With a sizeof or cast operation, real_declarator_allowed will come
         in as FALSE.  There's no point in looking ahead in such cases:
         "sizeof(x y)" isn't syntactically possible, so if x is not a
         type name, we'll assume it's an object name. */
    } else if (symbol_list_from_locator(locator_for_curr_id) == NULL) {
      /* If the current token is an identifier we'll proceed with the error
         recovery optimization only if we can be quite sure the name can't
         have another meaning.  The first indication of that is that there
         are no active symbols with this name in scope.  However, if the
         scope stack has a class reactivation entry on it or a class that
         itself has base classes, a deactivated symbol (one from the inactive
         list) may be visible.  Note that that this is an issue only if we
         are in a context that accepts an expression and there are inactive
         symbols associated with this name. */
      if (expr_context &&
          inactive_symbol_list_from_locator(locator_for_curr_id) != NULL &&
          scope_stack[depth_scope_stack].inactive_symbols_may_be_visible) {
        /* The scope stack contains either a class with base classes or a
           reactivated class.  In either case there may be a member among the
           inactive symbols, so we will suppress the optimization. */
      } else {
        /* An undefined identifier.  Check the next token -- we may have a
           token pattern that can be nothing but a declaration. */
        next_tok = next_token();
        if (next_tok == tok_identifier || next_tok == tok_operator) {
          /* Pattern "x y" or "x operator..." -- looks like a declaration in
             'most any context. */
          is_start = TRUE;
        } else if (!expr_context &&
                   (next_tok == tok_star || next_tok == tok_ampersand)) {
          /* Pattern "x *..." or x &..." -- looks like a declaration as long as
             the context rules out expressions. */
          is_start = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return(is_start);
}  /* is_decl_start */


a_boolean f_check_for_overload_anachronism(void)
/*
Issue a diagnostic, bypass the current token, which is "overload", and check
the tokens that follow.  If a declaration is of the format "overload f;" (or
"overload f, g, h;") just check for syntax errors and discard the entire
declaration; in such cases return TRUE.  Otherwise, return FALSE --
declaration processing will continue as though "overload" had not been seen.
(This function is only called from the macro check_for_overload_anachronism.)
*/
{
  a_boolean     discard_declaration = FALSE;
  a_token_kind  next_tok;

  db_enter(3, "f_check_for_overload_anachronism");
  check_assertion(curr_token == tok_overload);
  /* Issue an anachronism diagnostic indicating that "overload" is
     no longer allowed.  This can be either an error or a warning. */
  diagnostic(anachronism_error_severity, ec_overload_anachronism);
  /* Bypass "overload" */
  (void)get_token();
  if (curr_token == tok_identifier) {
    next_tok = next_token();
    if (next_tok == tok_semicolon || next_tok == tok_comma) {
      /* We have a single function name or a comma separated list of
         function names.  (We do not support a mixed list of function
         names and function declarations.) Throw away the identifier
         and advance to the ";" or ",". */
      (void)get_token();
      if (curr_token == tok_comma) {
        /* It is a list of names.  Loop through them just to flag syntax
           errors. */
        add_stop_token(tok_semicolon);
        /* Advance past the comma */
        (void)get_token();
        do {
          (void)required_token(tok_identifier, ec_exp_identifier);
        } while (loop_token(tok_comma));
        remove_stop_token(tok_semicolon);
      }  /* if */
      /* Check for final semicolon. */
      (void)required_token(tok_semicolon, ec_exp_semicolon);
      /* Tell the caller to do no more processing. */
      discard_declaration = TRUE;
    } else {
      /* Treat this as a function declaration.  Having bypassed the overload
         keyword we return to the caller. */
    }  /* if */
  }  /* if */
  db_exit();
  return discard_declaration;
}  /* f_check_for_overload_anachronism */


void adjust_parameter_type(a_type_ptr *type_ptr)
/*
*type_ptr points to the type of a parameter.  Modify the type if
necessary.  See 3.7.1:  A declaration of a parameter as "array of
type" shall be adjusted to "pointer to type", and the declaration of
a parameter as "function returning type" shall be adjusted to
"pointer to function returning type", as in 3.2.2.1.
*/
{
  db_enter(4, "adjust_parameter_type");
  /* Note that incomplete types are allowed.  One can't call a function
     that has an incomplete-type parameter, but one can complete it later. */
  if (is_array_type(*type_ptr)) {
    /* Array, adjust to pointer to element type. */
    *type_ptr = make_pointer_type(array_element_type(*type_ptr));
  } else if (is_function_type(*type_ptr)) {
    /* Function, adjust to pointer to function. */
    *type_ptr = make_pointer_type(*type_ptr);
  }  /* if */
  db_exit();
}  /* adjust_parameter_type */


static void check_type_qualifiers(a_type_ptr *type_ptr)
/*
A parameter or variable is about to be declared with the given type.
Check to see if any type qualifiers that are specified are meaningful.
*/
{
  db_enter(4, "check_type_qualifiers");
  if (is_qualified_type(*type_ptr)) {
    /* The type has type qualifiers. */
    if ((is_function_type(*type_ptr) && C_dialect != C_dialect_cplusplus) ||
        is_void_type(*type_ptr)) {
      /* Type qualifiers on void types are useless.  On function types they
         are undefined (3.5.3).  This can happen with something like
           typedef int F();
           const F g;
         -- we mark them as useless. */
      warning(ec_useless_type_qualifiers);
      /* The useless qualifiers could be removed by the statement
      *type_ptr = make_unqualified_type(*type_ptr);
	 but they are kept in case the back end assigns any meaning to them. */
    }  /* if */
  }  /* if */
  db_exit();
}  /* check_type_qualifiers */


void check_and_adjust_parameter_type(a_type_ptr         *type_ptr,
                                     a_source_position  *error_pos)
/*
This routine is called for all function parameter declarations.  It does
error checking and type adjustments as required.
*/
{
  /* Adjust the type if necessary (for example, "array of x" becomes
     "pointer to x"). */
  adjust_parameter_type(type_ptr);
  /* Disallow "void" as a parameter type. */
  if (is_void_type(*type_ptr)) {
    pos_error(ec_void_param_not_allowed, error_pos);
    *type_ptr = error_type();
  } else {
    /* See if any type qualifiers were specified, and if they are
       okay. */
    check_type_qualifiers(type_ptr);
    /* In C++ (except in cfront compatibility mode) disallow a parameter type
       that includes a pointer or reference to an array of unspecified size
       (WP 8.3.5 para 3). */
    if (!C_mode() && !any_cfront_mode()) {
      a_boolean  is_ref = FALSE;

#if 0
      /* WP 8.3.5 para 3 uses "includes" -- does this cover use in a template
         argument?  We currently assume "yes", but it the answer turns out to
         be "no", change the flags passed to traverse_type_tree by
         is_or_contains_ptr_or_ref_to_unknown_bound_array. */
#endif /* if 0 */
      if (is_or_contains_ptr_or_ref_to_unknown_bound_array(*type_ptr,
                                                           &is_ref)) {
        pos_error(is_ref ? ec_param_type_ref_array_of_unknown_bound :
                           ec_param_type_ptr_to_array_of_unknown_bound,
                  error_pos);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* check_and_adjust_parameter_type */


a_boolean check_operator_arrow_return_type(a_routine_ptr      rout_ptr,
                                           a_boolean          is_expr_use,
                                           a_source_position  *error_pos)
/*
rout_ptr points to the routine entry for an operator-> member function.
is_expr_use is TRUE if this is a call of the function (possible only if
it is a member of template class) and FALSE otherwise (i.e., if it is a
declaration or a fixup of a previous declaration).  *error_pos is the
source position for a diagnostic.
*/
{
  a_boolean                      err = FALSE;
  a_type_ptr                     rout_type, class_type, tp;
  a_class_symbol_supplement_ptr  cssp;

  rout_type = rout_ptr->type;
  class_type = rout_ptr->source_corresp.class_of_which_a_member;
  cssp = symbol_supplement_for_class(class_type);
  tp = skip_typerefs(rout_type->variant.routine.return_type);
  if (is_error_type(tp) || tp->kind == (a_type_kind)tk_template_param) {
    /* No action. */
  } else if (!is_expr_use && cssp->class_template != NULL &&
             !cssp->is_nonreal_class && !cssp->is_specific_template_def) {
    /* The operator-> function is being declared for a class that is a real
       instantiation of a template.  We can't issue a diagnostic on the
       return type, since it may be dependent on a template argument -- we
       wait till the call (if there is one). */
  } else if (is_pointer_type(tp)) {
    /* It's a pointer type -- be sure it's a pointer to a class. */
    tp = skip_typerefs(type_pointed_to(tp));
    if (is_class_struct_union_type(tp) ||
        tp->kind == (a_type_kind)tk_template_param) {
      /* Okay. */
    } else {
      /* Not a pointer to a class. */
      err = TRUE;
    }  /* if */
  } else {
    /* Not a pointer type.  Be sure the type is a class type for which
       operator-> is defined (unless the operator falls into one of the
       special categories). */
    if (is_reference_type(tp)) tp = skip_typerefs(type_pointed_to(tp));
    if (tp->kind == (a_type_kind)tk_template_param) {
      /* Okay.  We'll check again at the point of instantiation. */
    } else if (!is_class_struct_union_type(tp)) {
      /* Not a class type. */
      err = TRUE;
    } else if (tp == class_type) {
      /* X& X::operator->() would involve unbounded recursion at runtime,
         so we issued an error on such cases. */
      err = TRUE;
    } else if (symbol_supplement_for_class(tp)->is_nonreal_class) {
      /* The operator returns a (ref-to?) nonreal-class.  Ignore it for
         now, since any problems will be be handled whenever the class is
         instantiated. */
    } else if (is_incomplete_type(tp)) {
      if (cssp->is_prototype_instantiation) {
        /* Ignore an incomplete class return type during prototype
           instantiation. */
      } else {
        /* Postpone the check until the type is complete -- add the type to
           the class's dependent-type-fixup-list. */
        add_to_dependent_type_fixup_list(tp,
                                         (a_dependent_type_fixup_kind)
                                              dtfk_check_op_arrow_return_type,
                                         (char *)rout_ptr,
                                         (a_byte_il_entry_kind)iek_routine,
                                         error_pos);
      }  /* if */
    } else if (opname_member_function_symbol((an_opname_kind)onk_arrow,
                                             tp) == NULL) {
      /* tp is a class for which no operator-> has been defined. */
      err = TRUE;
    }  /* if */
  }  /* if */
  if (err) {
    if (cssp->is_prototype_instantiation) {
      /* Issue a warning instead of an error. */
      err = FALSE;
    }  /* if */
    pos_syty_diagnostic(err ? es_error : es_warning,
                        ec_bad_return_type_for_op_arrow, error_pos, 
                        (a_symbol_ptr)rout_ptr->source_corresp.assoc_info,
                        rout_type->variant.routine.return_type);
    if (err) {
      /* Change the return type to an error type.  In case the flag is
         set to cause the return to use a copy constructor, clear it. */
      rout_type->variant.routine.return_type = error_type();
      rout_type->variant.routine.extra_info->value_returned_by_cctor = FALSE;
    }  /* if */
  }  /* if */
  return !err;
}  /* check_operator_arrow_return_type */


void check_operator_function_params(a_type_ptr        rout_type,
                                    a_type_ptr        class_type,
                                    a_symbol_locator  *locator)
/*
Check the argument list on the declaration of a user-defined conversion
or overloaded operator function.  For conversion functions, no arguments
are allowed.  For operators there are different requirements for different
operator kinds.  Issue a diagnostic if an error is found.
If this routine is modified to use additional fields from the locator
make_template_function needs to be updated to make sure that the
new fields are set properly.
*/
{
  an_opname_kind                 opname;
  int                            param_count;
  a_param_type_ptr               ptp;
  a_boolean                      any_class_type_params = FALSE;
  a_type_ptr                     tp;
  a_boolean                      is_nonstatic_member_function;
  an_error_code                  error_code = ec_no_error;
  a_boolean                      err = FALSE;
  a_routine_type_supplement_ptr  rtsp;

  db_enter(4, "check_operator_function_params");
  rout_type = skip_typerefs(rout_type);
  rtsp = rout_type->variant.routine.extra_info;
  if (is_error_locator(*locator)) {
    /* Nothing to do. */
  } else if (locator->is_conversion_name) {
    check_assertion(class_type != NULL);
    /* Check the target type of the conversion -- which is the return type
       of rout_type. */
    if (!any_cfront_mode() &&
        is_void_type(rout_type->variant.routine.return_type)) {
      /* Conversion operators specifying conversion to void type are not
         allowed (Boston X3J16). */
      pos_ty2_error(ec_conversion_to_type_not_allowed,
                    &locator->source_position, class_type,
                    rout_type->variant.routine.return_type);
      err = TRUE;
    }  /* if */
    /* Any parameter is too many for a conversion function. */
    if (rtsp->param_type_list != NULL || rtsp->has_ellipsis) {
      pos_error(ec_too_many_args_for_conversion, &locator->source_position);
      err = TRUE;
    }  /* if */
  } else if (locator->is_operator_name) {
    /* It's an operator. */
    opname = locator->variant.opname;
    check_assertion(opname != (an_opname_kind)onk_none);
    is_nonstatic_member_function =
                routine_type_is_nonstatic_member_function(rout_type);
    /* Operator new/delete cannot be a nonstatic member function. */
    check_assertion(!(is_nonstatic_member_function &&
                      (opname == (an_opname_kind)onk_new ||
                       opname == (an_opname_kind)onk_delete)));
    /* Make a pass over the param types list to count the number of
       arguments to see if there are any parameters that are of class type
       or reference-to-class type.  Note that param_count is initialized to
       0 except in the case of nonstatic member functions, for which it is
       initialized to 1. This is because the implicit "this" parameter is
       counted in the latter case. */
    param_count = is_nonstatic_member_function ? 1 : 0;
    ptp = rout_type->variant.routine.extra_info->param_type_list;
    for (; ptp != NULL; ptp = ptp->next) {
      param_count++;
      tp = ptp->type;
      if (is_reference_type(tp)) tp = type_pointed_to(tp);
      if (is_class_struct_union_type(tp)) any_class_type_params = TRUE;
    }  /* if */
    if (opname == (an_opname_kind)onk_function_call ||
        opname == (an_opname_kind)onk_new) {
      /* Function call and new must have one or more arguments. */
      if (param_count == 0) {
        if (rtsp->has_ellipsis) {
          /* operator()(...) and operator new(...) are errors, but we do,
             with some trepidation, allow operator()(T, ...) and
             operator new(size_t, ...). */
          error_code = ec_ellipsis_on_operator_function;
        } else {
          error_code = ec_too_few_args_for_operator;
        }  /* if */
      } else if (opname == (an_opname_kind)onk_new) {
        ptp = rout_type->variant.routine.extra_info->param_type_list;
        tp = ptp->type;
        if (!is_error_type(tp) && !is_or_contains_template_param(tp)) {
          if (!is_integral_type(tp) ||
              skip_typerefs(tp)->variant.integer.int_kind !=
                                                      targ_size_t_int_kind) {
            error_code = ec_bad_arg_type_for_operator_new;
            ptp->type = error_type();
          }  /* if */
        }  /* if */
      }  /* if */
    } else if (rtsp->has_ellipsis) {
      /* All overloaded operators (except function call and new, handled
         above) require a specific number of arguments, so ellipsis is not
         allowed. */
      error_code = ec_ellipsis_on_operator_function;
    } else if (opname == (an_opname_kind)onk_compl ||
        opname == (an_opname_kind)onk_not ||
        opname == (an_opname_kind)onk_arrow) {
      /* Unary operator must have exactly one argument. */
      if (param_count > 1) {
        error_code = ec_too_many_args_for_operator;
      } else if (param_count < 1) {
        error_code = ec_too_few_args_for_operator;
      }  /* if */
    } else if (param_count == 1 &&
               (opname == (an_opname_kind)onk_plus ||
                opname == (an_opname_kind)onk_minus ||
                opname == (an_opname_kind)onk_star ||
                opname == (an_opname_kind)onk_ampersand ||
                opname == (an_opname_kind)onk_plus_plus ||
                opname == (an_opname_kind)onk_minus_minus)) {
       /* These operators can be either unary or binary.  It is legal for
          them to have exactly one argument. */
    } else if (param_count == 2 &&
               (opname == (an_opname_kind)onk_plus_plus ||
                opname == (an_opname_kind)onk_minus_minus)) {
      /* Extra argument on postfix operator must be of type "int"
         (ARM 13.4.7). */
      ptp = rout_type->variant.routine.extra_info->param_type_list;
      if (!is_nonstatic_member_function) ptp = ptp->next;
      tp = ptp->type;
      if (!is_error_type(tp) && !is_or_contains_template_param(tp)) {
        if (!is_integral_type(tp) ||
            skip_typerefs(tp)->variant.integer.int_kind !=
                                                  (an_integer_kind)ik_int) {
          pos_st_error(ec_bad_extra_arg_for_postfix_operator,
                       &locator->source_position,
                       opname == (an_opname_kind)onk_plus_plus ? "++" : "--");
          ptp->type = error_type();
          err = TRUE;
        }  /* if */
      }  /* if */
    } else if (opname == (an_opname_kind)onk_delete) {
      ptp = rout_type->variant.routine.extra_info->param_type_list;
      if (param_count == 0) {
        error_code = ec_too_few_args_for_operator;
      } else {
        tp = ptp->type;
        if (!is_error_type(tp) && !is_or_contains_template_param(tp)) {
          /* Check for "void *" -- notice that is_void_type is not called
             on the type-pointed-to: that is to catch "const void *". */
          if (!is_pointer_type(tp) ||
              skip_typedefs(type_pointed_to(tp))->kind !=
                                                   (a_type_kind)tk_void) {
            if (cfront_2_1_mode && is_pointer_type(tp) &&
                is_void_type(type_pointed_to(tp))) {
              /* Cfront 2.1 allows "const void *" parameter.  Issue a
                 warning and change the type to "void *". */
              pos_warning(ec_bad_first_arg_type_for_operator_delete,
                          &locator->source_position);
              ptp->type = make_pointer_type(void_type());
            } else {
              pos_error(ec_bad_first_arg_type_for_operator_delete,
                        &locator->source_position);
              ptp->type = error_type();
              err = TRUE;
            }  /* if */
          }  /* if */
        }  /* if */
        ptp = ptp->next;
        if (ptp != NULL) {
          /* There is a second argument.  This is permitted for class operator
             delete() but not for global operator delete() (ARM 12.5). */
          if (class_type == NULL) {
            error_code = ec_too_many_args_for_operator;
          } else {
            /* The second argument must be of type size_t (ARM 12.5). */
            tp = ptp->type;
            if (!is_error_type(tp) && !is_or_contains_template_param(tp)) {
              if (!is_integral_type(tp) ||
                  skip_typerefs(tp)->variant.integer.int_kind !=
                                                      targ_size_t_int_kind) {
                pos_error(ec_bad_second_arg_type_for_operator_delete,
                          &locator->source_position);
                ptp->type = error_type();
                err = TRUE;
              }  /* if */
            }  /* if */
            /* More than two arguments are not allowed. */
            if (ptp->next != NULL) error_code = ec_too_many_args_for_operator;
          }  /* if */
        }  /* if */
      }  /* if */
    } else {
      /* Binary operator must have exactly two arguments. */
      if (param_count > 2) {
        error_code = ec_too_many_args_for_operator;
      } else if (param_count < 2) {
        error_code = ec_too_few_args_for_operator;
      }  /* if */
    }  /* if */
    if (error_code != ec_no_error) {
      pos_error(error_code, &locator->source_position);
      err = TRUE;
    }  /* if */
    if (opname == (an_opname_kind)onk_new ||
        opname == (an_opname_kind)onk_delete) {
      /* Check return type. */
      tp = rout_type->variant.routine.return_type;
      if (!is_error_type(tp) && !is_or_contains_template_param(tp)) {
        if (opname == (an_opname_kind)onk_new) {
          if (!is_pointer_type(tp) || !is_void_type(type_pointed_to(tp))) {
            pos_error(ec_bad_return_type_for_op_new,
                      &locator->source_position);
            err = TRUE;
          }  /* if */
        } else {
          if (!is_void_type(tp)) {
            pos_error(ec_bad_return_type_for_op_delete,
                      &locator->source_position);
            err = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
    } else {
      /* If operator function is not a nonstatic member and does not have
         operands of class type or reference-to-class type, issue an error.
         This restriction does not apply to new and delete, however. */
      if (!is_nonstatic_member_function && !any_class_type_params) {
        pos_error(ec_no_args_with_class_type, &locator->source_position);
        err = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (err) set_to_error_locator(*locator);
  db_exit();
}  /* check_operator_function_params */


void add_exception_specification(a_func_info_block_ptr  func_info,
                                 a_routine_ptr          rp)
/*
Transfer the exception specification stored in func_info to the routine type
supplement for routine rp.
*/
{
  a_routine_type_supplement_ptr  rtsp;

  db_enter(4, "add_exception_specification");
  if (exceptions_enabled) {
    if (func_info->exception_specification != NULL &&
        func_info->is_main_function) {
      /* main() cannot have a throw specification, since there's no call stack
         to unwind from main. */
      pos_warning(ec_exception_specification_not_allowed,
                  &func_info->throw_position);
    }  /* if */
    if (rp->type->kind != (a_type_kind)tk_routine) {
      /* The routine was declared in terms of a typedef.  Don't add throw
         specifications. */
    } else {
      rtsp = rp->type->variant.routine.extra_info;
      check_assertion(rtsp->exception_specification == NULL);
      rtsp->exception_specification = func_info->exception_specification;
    }  /* if */
  }  /* if */
  db_exit();
}  /* add_exception_specification */


void check_exception_specification(a_func_info_block_ptr  func_info,
                               a_routine_ptr          rp)
/*
Check that the throw specification on the current declaration, if any, is
consistent with that of the previous declaration.
*/
{
  a_boolean                            match, any_difference_seen;
  an_exception_specification_ptr       new_tsp, old_tsp;
  an_exception_specification_type_ptr  new_est_list, old_est_list;
  an_exception_specification_type_ptr  estp, other_estp;
  a_symbol_ptr                         rout_sym;

  db_enter(4, "check_exception_specification");
  if (exceptions_enabled) {
    rout_sym = (a_symbol_ptr)rp->source_corresp.assoc_info;
    old_tsp = rp->type->variant.routine.extra_info->exception_specification;
    new_tsp = func_info->exception_specification;
    if (new_tsp != NULL && func_info->is_main_function) {
      /* main() cannot have a throw specification, since there's no call stack
         to unwind from main. */
      pos_warning(ec_exception_specification_not_allowed,
                  &func_info->throw_position);
    }  /* if */
    if (old_tsp == NULL) {
      /* Previous specification asserted that any exception may be thrown.
         It is compatible only with an identical specification on the current
         declaration. */
      if (new_tsp != NULL) {
        pos_stsy_error(ec_incompatible_exception_specification,
                       &func_info->throw_position, "", rout_sym);
      }  /* if */
    } else if (new_tsp == NULL) {
      /* Issue an error on the omission of a throw specification on the current
         declaration (it must have been present on the previous one). */
      pos_sy_error(ec_omitted_exception_specification,
                   &func_info->throw_position, rout_sym);
    } else if (old_tsp->exception_specification_type_list == NULL) {
      /* Previous specification asserted that no exceptions will be thrown.
         It is compatible only with an identical specification on the current
         declaration. */
      if (new_tsp->exception_specification_type_list != NULL) {
        pos_stsy_start_error(ec_incompatible_exception_specification,
                             &func_info->throw_position, ":", rout_sym);
        add_diag_info(ec_previous_exception_specification_was_empty);
        end_error();
      }  /* if */
    } else {
      /* Previous specification was a list of the types that will be thrown.
         Check for a mismatch between the previous list and the current one. */
      old_est_list = old_tsp->exception_specification_type_list;
      new_est_list = new_tsp->exception_specification_type_list;
      any_difference_seen = FALSE;
      /* First loop through the current list (if any) and issue a diagnostic
         on any type present in the current list but absent from the
         previous one. */
      for (estp = new_est_list; estp != NULL; estp = estp->next) {
        if (estp->redundant) {
          /* Don't bother looking for a match on redundant types.  It will
             already have been done. */
        } else {
          match = FALSE;
          other_estp = old_est_list;
          for (; other_estp != NULL; other_estp = other_estp->next) {
            if (!other_estp->redundant && other_estp->type != NULL &&
                identical_types(estp->type, other_estp->type)) {
              /* An entry of the same type was found on the previous list. */
              match = TRUE;
              break;
            }  /* if */
          }  /* for */
          if (!match) {
            /* No match was found, so the previous list does not have a
               type that is on the current list. */
            if (!any_difference_seen) {
              /* The diagnostics will be combined with a header message
                 followed by additional messages identifying the specific
                 discrepancy.  This is the first diagnostic, so put out
                 the header message first. */
              pos_stsy_start_error(ec_incompatible_exception_specification,
                                   &func_info->throw_position, ":",
                                   rout_sym);
              any_difference_seen = TRUE;
            }  /* if */
            ty_add_diag_info(ec_omitted_in_previous_exception_specification,
                             estp->type);
          }  /* if */
        }  /* if */
      }  /* for */
      /* Next loop through the previous list and issue a diagnostic on any
         type present in the previous list but absent from the current one
         (if any). */
      other_estp = old_est_list;
      for (; other_estp != NULL; other_estp = other_estp->next) {
        if (other_estp->redundant) {
          /* Don't bother looking for a match on redundant types.  It will
             already have been done. */
        } else {
          match = FALSE;
          for (estp = new_est_list; estp != NULL; estp = estp->next) {
            if (!estp->redundant &&
                other_estp->type != NULL && estp->type != NULL &&
                identical_types(estp->type, other_estp->type)) {
              match = TRUE;
              break;
            }  /* if */
          }  /* for */
          if (!match) {
            if (!any_difference_seen) {
              /* This is the first diagnostic, so put out the header
                 message first. */
              pos_stsy_start_error(ec_incompatible_exception_specification,
                                   &func_info->throw_position, ":",
                                   rout_sym);
               any_difference_seen = TRUE;
            }  /* if */
            ty_add_diag_info(ec_included_in_previous_exception_specification,
                             other_estp->type);
          }  /* if */
        }  /* if */
      }  /* for */
      if (any_difference_seen) end_error();
    }  /* if */
  }  /* if */
  db_exit();
}  /* check_exception_specification */

#if GENERATE_SOURCE_SEQUENCE_LISTS

a_source_sequence_entry_ptr init_param_source_sequence_sublist(void)
/*
Return a pointer to a source sequence entry that will be the predecessor of
any entries generated for a function prototype scope.
*/
{
  a_source_sequence_entry_ptr  ssep;

  if (!source_sequence_entries_disallowed) {
    /* Locate the last source sequence entry on the current list. */
    ssep = scope_stack[depth_innermost_ss_list_scope].
                                            last_source_sequence_entry;
    if (ssep != NULL && is_sublist_parent(ssep)) {
      /* It marks a sublist, so get the last entry on the sublist. */
      ssep = assoc_sublist_of(ssep)->last_source_sequence_entry;
    }  /* if */
  } else {
    ssep = NULL;
  }  /* if */
  return ssep;
}  /* init_param_source_sequence_sublist */


void terminate_param_source_sequence_sublist(
                                     a_func_info_block_ptr        func_info,
                                     a_source_sequence_entry_ptr  prev)
/*
Add pointers to *func_info identifying the starting and ending source sequence
entries of the function prototype scope declarations.  If it turns out that
this declaration is a function definition, the list segment marked by these
pointers may have to be moved from the file scope source sequence list to the
function scope source sequence list -- see scan_function_body.
*/
{
  a_source_sequence_entry_ptr  starting_ssep, ending_ssep;
  a_src_seq_sublist_ptr        sublist = NULL;

  if (!source_sequence_entries_disallowed) {
    /* The first entry in the function prototype list segment is prev's
       successor. */
    if (prev != NULL) {
      starting_ssep = prev->next;
    } else {
      /* Prev is NULL, so use the first entry on the current list. */
      starting_ssep = scope_stack[depth_innermost_ss_list_scope].
                                            il_scope->source_sequence_list;
    }  /* if */
    if (starting_ssep != NULL) {
      /* If starting_ssep is a sublist parent, it is the first entry on its
         sublist we're interested in. */
      if (is_sublist_parent(starting_ssep)) {
        sublist = assoc_sublist_of(starting_ssep);
        starting_ssep = sublist->source_sequence_list;
      }  /* if */
      /* Record the starting entry. */
      func_info->prototype_scope_ss_entry_start = starting_ssep;
      /* Find the ending entry. */
      if (depth_innermost_ss_list_scope != DEPTH_OF_FILE_SCOPE) {
        /* This function declaration appears inside a function scope, so the
           ending source sequence entry is the end of the sublist. */
#if CHECKING
        {
        a_source_sequence_entry_ptr  ssep;

        ssep = scope_stack[depth_innermost_ss_list_scope].
                                                 last_source_sequence_entry;
        check_assertion(is_sublist_parent(ssep));
        check_assertion(assoc_sublist_of(ssep) ==
                        (sublist != NULL ? sublist :
                                           sublist_header_of(starting_ssep)));
        }
#endif /* CHECKING */
        if (sublist != NULL) {
          /* The sublist header has already been determined. */
          ending_ssep = sublist->last_source_sequence_entry;
        } else {
          /* The sublist header is unknown, so just search to the end of the
             list. */
          ending_ssep = starting_ssep;
          while (ending_ssep->next != NULL) ending_ssep = ending_ssep->next;
        }  /* if */
      } else {
        /* The ending source sequence entry is simply the end of the file scope
           list. */
        ending_ssep =
                 scope_stack[DEPTH_OF_FILE_SCOPE].last_source_sequence_entry;
      }  /* if */
      /* Record the ending entry. */
      func_info->prototype_scope_ss_entry_end = ending_ssep;
#if DEBUG
      if (debug_level >= 4) {
        fputs("function prototype source sequence list:\n", f_debug);
        if (func_info->prototype_scope_ss_entry_start == NULL) {
          fputs("  <empty list>\n", f_debug);
        } else {
          a_source_sequence_entry_ptr  tmp_prev, tmp_next;
          tmp_prev = func_info->prototype_scope_ss_entry_start->prev;
          func_info->prototype_scope_ss_entry_start->prev = NULL;
          tmp_next = func_info->prototype_scope_ss_entry_end->next;
          func_info->prototype_scope_ss_entry_end->next = NULL;
          db_source_sequence_list(func_info->prototype_scope_ss_entry_start);
          func_info->prototype_scope_ss_entry_start->prev = tmp_prev;
          func_info->prototype_scope_ss_entry_end->next = tmp_next;
        }  /* if */
      }  /* if */
#endif /* DEBUG */
    }  /* if */
  }  /* if */
}  /* terminate_param_source_sequence_sublist */

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

a_routine_ptr make_routine(a_type_ptr      type_ptr,
                           a_storage_class storage_class,
                           a_boolean       at_file_scope,
                           a_boolean       add_to_list)
/*
Allocate an entry for a routine with function type type_ptr and storage class
storage_class, and return a pointer to it.  The entry is allocated at the
file scope.  type_ptr must be in the file scope.  If add_to_list is TRUE,
add the new routine entry to the routines list.
*/
{
  a_routine_ptr          rp;
  a_memory_region_number region_to_switch_back_to;

  /* Always allocate routines at the file scope. */
  switch_to_file_scope_region(&region_to_switch_back_to);
  rp = alloc_routine();
  rp->type = type_ptr;
  rp->storage_class = storage_class;
  if (add_to_list) add_to_routines_list(rp, at_file_scope);
  switch_back_to_original_region(region_to_switch_back_to);
  return rp;
}  /* make_routine */


a_variable_ptr make_variable(a_type_ptr      type_ptr,
                             a_storage_class storage_class,
                             a_boolean       at_file_scope)
/*
Allocate an entry for a variable with type type_ptr and storage class
storage_class, and return a pointer to it.  Add the variable to the
file scope if at_file_scope is TRUE (in that case, type_ptr must be
in the file scope).
*/
{
  a_variable_ptr          vp;

  /* Allocate the variable at the file scope if requested.  Variables
     receiving static storage, even if they are not to go on the file scope
     variables list, should also be allocated in the file scope memory
     region. */
  vp = alloc_variable(storage_class);
  vp->type = type_ptr;
  add_to_variables_list(vp, at_file_scope);
  return vp;
}  /* make_variable */


static void make_anonymous_union_variable(a_type_ptr      anon_union_type,
                                          a_storage_class storage_class)
/*
Create a variable to represent an anonymous union.  Issue an error if its
storage class is invalid.  Also promote the fields of the union to the
current scope.
*/
{
  a_variable_ptr vp;
  a_boolean      at_file_scope = (decl_scope_level == DEPTH_OF_FILE_SCOPE);
  a_symbol_ptr   assoc_object_sym;

  /* Check the storage class.  At file scope, only static is allowed. */
  if (at_file_scope) {
    switch (storage_class) {
      case sc_static:
        /* Okay. */
        break;
      case sc_extern:
      case sc_unspecified:
        /* Disallowed (ARM 9.5). */
        error(ec_anon_union_storage_class);
        storage_class = (a_storage_class)sc_static;
        break;
      default:
        /* Invalid for any variable at file scope. */
        error(ec_bad_file_scope_storage_class);
        storage_class = (a_storage_class)sc_static;
    }  /* switch */
  } else {
    /* Not at file scope. */
    switch (storage_class) {
      case sc_extern:
        /* Error, then default to automatic. */
        error(ec_anon_union_storage_class);
      case sc_unspecified:
        /* Default to automatic. */
        storage_class = (a_storage_class)sc_auto;
      case sc_static:
      case sc_auto:
      case sc_register:
        /* Okay. */
        break;
#if CHECKING
      default:
        internal_error("make_anonymous_union_variable: bad storage class");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */
  /* Allocate a variable to represent the anonymous union. */
  vp = make_variable(anon_union_type, storage_class, at_file_scope);
  vp->is_anonymous_parent_object = TRUE;
  /* Promote the fields of the anonymous union to the current scope, and do
     some error checking on the anonymous union's members. */
  assoc_object_sym = make_anonymous_parent_object_symbol(
                                         (a_symbol_kind)sk_variable,
                                         &pos_curr_token,
                                         scope_stack[decl_scope_level].number);
  assoc_object_sym->variant.variable.ptr = vp;
  check_anonymous_union_symbols(assoc_object_sym, (a_type_ptr)NULL,
                                /*is_nonstd=*/FALSE);
}  /* make_anonymous_union_variable */


a_variable_ptr make_param_variable(a_type_ptr       type_ptr,
                                   a_storage_class  storage_class)
/*
Allocate a variable entry with type type_ptr.  If type_ptr is NULL (as it
will be in trying to creating an implicit this parameter for static member
functions) simply return NULL.
*/
{
  a_variable_ptr vp;

  check_assertion(type_ptr != NULL);
  vp = alloc_variable(storage_class);
  vp->type = type_ptr;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  vp->declared_type = type_ptr;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  vp->is_parameter = TRUE;
  return(vp);
}  /* make_param_variable */


a_variable_ptr make_parameter(a_type_ptr       type,
                              a_storage_class  storage_class,
                              a_symbol_ptr     sym)
/*
Allocate a parameter variable with the specified type and storage class
and return a pointer to it.  The parameter is linked to/from its associated
symbol sym.
*/
{
  a_variable_ptr vp;

  vp = make_param_variable(type, storage_class);
  /* sym will be NULL when the parameter is unnamed. */
  if (sym != NULL) {
    sym->variant.variable.ptr = vp;
    set_source_corresp(&(vp->source_corresp), sym);
    mark_defined(sym, &sym->decl_position);
    mark_variable_value_set(sym);
  }  /* if */
  add_to_parameters_list(vp);
  return(vp);
}  /* make_parameter */


a_symbol_ptr enter_local_symbol(a_symbol_kind    kind,
                                a_symbol_locator *locator,
                                a_scope_depth    scope_level,
                                a_boolean        suppress_redecl_error)
/*

Enter a symbol declarative scope specified by scope_level.  kind indicates
the kind of symbol (e.g., a variable), and *locator is a locator for the
identifier.  Enter the symbol at the file scope if is_file_scope is TRUE.
If suppress_redecl_error is TRUE, suppress any error on a duplicate
declaration of this symbol.

*/
{
  a_symbol_ptr  sym;

  db_enter(4, "enter_local_symbol");
  if (scope_stack[scope_level].kind == (a_scope_kind)sck_func_prototype) {
    if (kind == (a_symbol_kind)sk_variable) {
      /* A variable declared in a function prototype scope is the result of
         an error in an old-style param list. */
    } else if (C_dialect == C_dialect_cplusplus) {
      /* We can't get here in C++ in a legal program.  If there was some
         sort of error, just go ahead and enter the symbol in the current
         scope. */
    } else {
      /* Any other declaration expected in a prototype scope is that of a
         type or an enumeration constant. */
      check_assertion(kind == (a_symbol_kind)sk_class_or_struct_tag ||
                      kind == (a_symbol_kind)sk_union_tag ||
                      kind == (a_symbol_kind)sk_enum_tag ||
                      kind == (a_symbol_kind)sk_type ||
                      kind == (a_symbol_kind)sk_constant);
      if (C_dialect == C_dialect_pcc) {
        /* In pcc a type declared in a parameter declaration belongs to the
           file scope.  For example:
               void f(a) struct s { int i; }; s a; { ... }
               struct s *aa;
           "s" refers to the same struct in both declarations. */
        scope_level = DEPTH_OF_FILE_SCOPE;
      } else {
        /* In C-mode a type declared in a parameter declaration is local to
           function.  Issue a warning on type declarations, since they will
           not be visible outside the function declaration.  For example:
               inf f(struct s a;);
               struct s {int b;};
           The first "struct s" is a different type than the second, which is
           probably not what was wanted. */
        if (kind != (a_symbol_kind)sk_constant &&
            !is_error_locator(*locator)) {
          pos_warning(ec_decl_in_prototype_scope, &locator->source_position);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  sym = enter_symbol(kind, locator, scope_level, suppress_redecl_error);
  db_exit();
  return(sym);
}  /* enter_local_symbol */


static a_boolean is_default_operator_new(a_symbol_locator *locator,
                                         a_type_ptr       type)
/*
Return TRUE if the locator is for an operator new() and the type indicates
that it is the default operator new() (i.e., if it has exactly one parameter,
which elsewhere is confirmed to have type size_t).
*/
{
  a_boolean         match = FALSE;
  a_param_type_ptr  ptp;

  if (locator->is_operator_name &&
      locator->variant.opname == (an_opname_kind)onk_new) {
    check_assertion(is_function_type(type));
    ptp = (skip_typerefs(type))->variant.routine.extra_info->param_type_list;
    if (ptp != NULL && ptp->next == NULL) {
      match = TRUE;
    }  /* if */
  }  /* if */
  return match;
}  /* is_default_operator_new */


static an_id_linkage_kind id_linkage(a_symbol_locator *locator,
                                     a_storage_class  *storage_class,
                                     a_type_ptr       type,
                                     a_boolean        is_main_function,
                                     a_symbol_ptr     *linked_symbol,
                                     a_symbol_ptr     *overload_symbol,
                                     a_scope_depth    *effective_decl_level)
/*
An identifier (specified by *locator) of type "type" and storage class
"storage class" is about to be declared at the current scope level.
Determine its linkage (see 3.1.2.2), and return it.  The linkage
determines how this declaration interacts with other (possibly tentative)
declarations of the same identifier.  Return in *linked_symbol a pointer
to any linked symbol (an identifier with the same name in the same scope,
if the new identifier has linkage).  This routine should not be called
for function parameters (they have no linkage); it will correctly handle
typedefs.  In C++, when the type is a function type and a previously
declared function in the same scope has a different type, we have a
candidate for function overloading; the *linked_symbol in this case will
be NULL, but we return in *overload_symbol a pointer to the symbol that
will be involved in overloading.
*/
{
  an_id_linkage_kind linkage;
  a_boolean          is_object, is_function, file_scope, decls_at_same_scope;
  a_boolean          is_list, is_friend_decl = FALSE;
  a_symbol_ptr       other_decl, sym, other_decl_saved;
  a_boolean          is_default_global_operator_new = FALSE;
  a_storage_class    local_storage_class = *storage_class;
  a_boolean          is_function_template_decl = FALSE;
  a_boolean          function_template_seen = FALSE;

  db_enter(3, "id_linkage");
  *linked_symbol = NULL;
  *overload_symbol = NULL;
  if (local_storage_class == (a_storage_class)sc_typedef) {
    /* A typedef is not an object or function, and has no linkage. */
    linkage = idl_none;
  } else if ((sym = locator->specific_symbol) != NULL &&
             sym->class_of_which_a_member != NULL) {
    /* Static data member. */
    check_assertion(sym->kind == (a_symbol_kind)sk_static_data_member);
    *linked_symbol = sym;
    linkage = idl_external;
  } else {
    /* Is this type an object or function, and is it going to be declared
       at file scope? */
    is_function = is_function_type(type);
    is_object = !is_function;
    /* In pcc mode, functions and extern variables are always effectively
       declared at the file scope level.  In addition, because we accept
       static function declarations inside functions as an extension, we
       must promote static function declarations to file scope. */
    if ((C_dialect == C_dialect_pcc &&
         (is_function || local_storage_class == (a_storage_class)sc_extern)) ||
        (C_dialect != C_dialect_cplusplus &&
         (is_function && local_storage_class == (a_storage_class)sc_static))) {
      *effective_decl_level = DEPTH_OF_FILE_SCOPE;
    } else if (scope_stack[decl_scope_level].kind ==
                                     (a_scope_kind)sck_template_declaration) {
      is_function_template_decl = TRUE;
      *effective_decl_level = DEPTH_OF_FILE_SCOPE;
    } else if (C_dialect == C_dialect_cplusplus &&
               is_default_operator_new(locator, type)) {
      /* Default global operator new must always be entered at file scope. */
      *effective_decl_level = DEPTH_OF_FILE_SCOPE;
      is_default_global_operator_new = TRUE;
    } else {
      *effective_decl_level = decl_scope_level;
      while (scope_stack[*effective_decl_level].kind ==
                                       (a_scope_kind)sck_class_struct_union) {
        /* This must be a friend function declaration.  Enter the name at the
           level of the scope containing the class.  We allow for nested
           classes by popping out till we find a non-class scope. */
        (*effective_decl_level)--;
        is_friend_decl = TRUE;
        if (scope_stack[*effective_decl_level].kind ==
                                 (a_scope_kind)sck_template_instantiation) {
          *effective_decl_level = DEPTH_OF_FILE_SCOPE;
          break;
        }  /* if */
      }  /* while */
    }  /* if */
    file_scope = (*effective_decl_level == DEPTH_OF_FILE_SCOPE);
    if (is_error_locator(*locator)) {
      /* Symbol is compiler-generated as a result of an error, so there are
         no other declarations of the same symbol. */
      other_decl = NULL;
    } else {
      other_decl = locator->symbol_header->symbol;
      /* Look for any visible declaration of the identifier. */
      for (; other_decl != NULL; other_decl = other_decl->next) {
        if (name_space_for_symbol_kind[(int)other_decl->kind] == nsk_other) {
          /* Found one.  If it's not a variable or routine (say, if it's
             a typedef), pretend that there is no visible declaration. */
          if (other_decl->class_of_which_a_member != NULL) {
            /* This is a member of a class scope.  Ignore it and keep looking
               till a match in an enclosing scope is found. */
          } else if (is_default_global_operator_new &&
                     other_decl->decl_scope != FILE_SCOPE_NUMBER) {
            /* This is a non-default global operator new that was not
               declared at file scope.  Skip over it and look for a file
               scope symbol. */
          } else {
            if (other_decl->kind != (a_symbol_kind)sk_variable &&
                other_decl->kind != (a_symbol_kind)sk_routine &&
                other_decl->kind != (a_symbol_kind)sk_function_template &&
                other_decl->kind != (a_symbol_kind)sk_overloaded_function) {
              other_decl = NULL;
            }  /* if */
            break;
          }  /* if */
        }  /* if */
      }  /* for */
      if (other_decl != NULL) {
        /* A match was found in searching the symbol table.  However, in
           C++ we have to allow for function overloading.  If what we found
           was an sk_overloaded_function symbol, we need to look for a type
           match amongst the instances of the name.  Even if it was an
           sk_routine symbol, we may want to overload the two functions. */
        decls_at_same_scope = (other_decl->decl_scope ==
                                    scope_stack[*effective_decl_level].number);
        if (C_dialect == C_dialect_cplusplus && is_function &&
            other_decl->kind != (a_symbol_kind)sk_variable &&
            !is_main_function) {
          /* C++ function -- type compatibility check is required. */
          if (decls_at_same_scope) {
            /* *overload_symbol is set for cases in which the current symbol
               may be added to an overload list.  Note that overloading across
               scopes is not allowed.  Also, *overload_symbol may end up being
               cleared latter. */
            *overload_symbol = other_decl;
          }  /* if */
          if (other_decl->kind == (a_symbol_kind)sk_overloaded_function) {
            other_decl = other_decl->variant.overloaded_function.symbols;
            is_list = TRUE;
          } else {
            is_list = FALSE;
          }  /* if */
          other_decl_saved = other_decl;
          /* Go through the list of functions and look for type compatibility.
             If types_are_compatible returns TRUE, this is a redeclaration.
             If no type match is found, this is a candidate for overloading. */
          for (; other_decl != NULL;
                 other_decl = is_list ? other_decl->next : NULL) {
            a_type_ptr  tp;

            if (other_decl->kind == (a_symbol_kind)sk_function_template) {
              a_template_symbol_supplement_ptr  tssp;
              tssp = other_decl->variant.template_info;
              if (is_function_template_decl) {
                tp = skip_typerefs(tssp->variant.function.routine->type);
                if (types_are_compatible(tp, skip_typerefs(type))) {
                  *linked_symbol = other_decl;
                  *overload_symbol = NULL;
                  goto determine_linkage;
                }
              } else {
                /* There may a match involving an instance of this function
                   template, but we delay searching its list of instantiations
                   until all normally declared functions have been seen. */
                function_template_seen = TRUE;
              }  /* if */
            } else {
              if (is_function_template_decl) {
                /* No match. */
              } else {
                tp = routine_symbol_type(other_decl);
                if (types_are_compatible(tp, skip_typerefs(type))) {
                  /* Other_decl matches the current declaration.  Null out
                     *overload_symbol in case it was set. */
                  *overload_symbol = NULL;
                  break;
                }  /* if */
              }  /* if */
            }  /* if */
          }  /* for */
          if (other_decl == NULL && function_template_seen) {
            /* We didn't find a match, but there was at least one function
               template.  See if it either provides a match with an
               existing instance of the template or if a new instance can
               be created based on the current type. */
            for (other_decl = other_decl_saved;
                 other_decl != NULL;
                 other_decl = is_list ? other_decl->next : NULL) {
              if (other_decl->kind == (a_symbol_kind)sk_function_template) {
                /* Look for a match on the list of instantiations. */
                a_symbol_ptr sym;
                sym = matching_template_function(other_decl, type,
                                                 &locator->source_position);
                if (sym != NULL) {
                  /* Found a match. */
                  *linked_symbol = other_decl = sym;
                  if (sym->variant.routine.instance_ptr->specific_decl) {
                    *overload_symbol = NULL;
                  }  /* if */
                  goto determine_linkage;
                }  /* if */
              }  /* if */
            }  /* for */
          }  /* if */
        }  /* if */
        if (decls_at_same_scope || is_friend_decl) {
          /* The function symbol was located in the current scope. If there
             there was an exact type match of C++ functions, and in general
             otherwise, this is a redeclaration, and if other_decl has
             linkage we can return in *linked_symbol a pointer to the function
             or variable it represents. */
          if (other_decl != NULL) {
            if (is_function_symbol(other_decl)) {
              /* Functions always have linkage. */
              *linked_symbol = other_decl;
            } else if (other_decl->decl_scope == FILE_SCOPE_NUMBER ||
                       other_decl->variant.variable.ptr->storage_class ==
                                           (a_storage_class)sc_extern ||
                       other_decl->variant.variable.ptr->storage_class ==
                                           (a_storage_class)sc_unspecified) {
              /* Variables at file scope always have linkage.  Automatic,
                 register, and static variables in local scopes do not. */
              *linked_symbol = other_decl;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
determine_linkage:
    /* Determine the linkage. */
    if (!file_scope && local_storage_class != (a_storage_class)sc_extern) {
      /* A non-file-scope object without extern storage class has no
         linkage.  In C++ a non-file-scope function may be declared --
         a friend function defined inline within a local class; it too
         is given no linkage. */
      check_assertion(!is_function ||
                      (is_friend_decl &&
                       local_storage_class == (a_storage_class)sc_static));
      linkage = idl_none;
    } else if (file_scope &&
               local_storage_class == (a_storage_class)sc_static) {
      /* An object or function at file scope with static storage class
         has internal linkage. */
      linkage = idl_internal;
      /* File scope objects with internal linkage must have static storage
         class.  Sometimes an adjustment must be made on the storage class
         passed in, e.g.,
           static int i; extern int i;
         or
           static void f(); void f() { }
         The variable and function acquire static storage class from the prior
         declarations. */
      *storage_class = (a_storage_class)sc_static;
    } else if (local_storage_class == (a_storage_class)sc_extern ||
               (is_function &&
                local_storage_class == (a_storage_class)sc_unspecified)) {
      /* An object or function with extern storage class, or a function
         with no storage class, has the same linkage as any visible
         declaration of this identifier with file scope.  If there is
         no visible declaration, the identifier has external linkage. */
      if (other_decl != NULL && other_decl->decl_scope == FILE_SCOPE_NUMBER) {
        /* There is a declaration with file scope that is visible from
           here.  Set the flags to describe this identifier, and go
           retry the determination of the linkage. */
        file_scope = TRUE;
        switch (other_decl->kind) {
          case sk_routine:
            local_storage_class = other_decl->variant.routine.ptr->
                                                                storage_class;
            is_function = TRUE;
            break;
          case sk_variable:
            local_storage_class = other_decl->variant.variable.ptr->
                                                                storage_class;
            is_function = FALSE;
            break;
          case sk_function_template:
            local_storage_class = other_decl->variant.template_info->
                                      variant.function.routine->storage_class;
            is_function = TRUE;
            break;
#if CHECKING
          default:
            internal_error("id_linkage: bad kind for other_decl");
#endif /* CHECKING */
        }  /* switch */
        is_object = !is_function;
        /* If we check again for visible identifiers, there can be no
           other visible identifier with the same name. */
        other_decl = NULL;
        goto determine_linkage;
      }  /* if */
      /* No visible declaration found, so the linkage is external. */
      linkage = idl_external;
    } else if (is_object && file_scope &&
               local_storage_class == (a_storage_class)sc_unspecified) {
      /* An object at file scope with no storage class has external linkage. */
      linkage = idl_external;
#if ASM_FUNCTION_ALLOWED
    } else if (local_storage_class == (a_storage_class)sc_asm) {
      /* An asm function has internal linkage. */
      linkage = idl_internal;
#endif /* ASM_FUNCTION_ALLOWED */
#if CHECKING
    } else {
      /* There should not be any other cases. */
      internal_error("id_linkage: could not determine identifier linkage");
#endif /* CHECKING */
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "Linkage for %s is ",
                     is_error_locator(*locator) ?
                        "<error>" : locator->symbol_header->identifier);
    switch (linkage) {
      case idl_none:     fputs("none",     f_debug); break;
      case idl_internal: fputs("internal", f_debug); break;
      case idl_external: fputs("external", f_debug); break;
#if CHECKING
      default:      internal_error("id_linkage: bad id linkage determination");
#endif /* CHECKING */
    }  /* switch */
    putc('\n', f_debug);
  }  /* if */
#endif /* DEBUG */
  if (linkage == idl_none) *linked_symbol = NULL;

  db_exit();
  return(linkage);
}  /* id_linkage */


a_boolean reconcile_external_symbol_types(
                             a_symbol_ptr          ext_sym,
                             a_source_position_ptr position,
                             a_type_ptr            type_ptr,
                             a_boolean             suppress_incompatible_error)
/*
Change the type of the external symbol ext_sym to type_ptr.  External
symbol entries are constructed for all variables and routines with
linkage (external or internal) as a place to keep the pointer to the
unique IL entry (needed because the normal symbol entries will not
necessarily stay in scope for the entire compilation).  *position
gives the source position to be used in case of error.
If suppress_incompatible_error is TRUE, do not issue an error about
a type incompatibility (presumably because the caller has already
issued a similar error).  Return FALSE if there is some error.
*/
{
  an_extern_symbol_descr_ptr esdp;
  a_type_ptr                 old_type;
  a_boolean                  okay = TRUE;

  esdp = ext_sym->variant.extern_symbol_descr;
  old_type = esdp->type;
  /* If the old and new types are the same, no checking or processing is
     required. */
  if (old_type != type_ptr) {
    if (!types_are_compatible(old_type, type_ptr)) {
      /* The old and new types are incompatible.  Error. */
      if (!suppress_incompatible_error) {
        pos_sy_error(ec_decl_incompatible_with_previous_use,
                     position, ext_sym);
      }  /* if */
      okay = FALSE;
      /* Record an error type as the external symbol's type, to avoid
         future errors. */
      esdp->type = error_type();
    } else {
      /* The old and new types are compatible.  Form the composite of
         those types, and save that as the type of the external symbol. */
      esdp->type = composite_type(old_type, type_ptr);
    }  /* if */
  }  /* if */
  return okay;
}  /* reconcile_external_symbol_types */


static a_symbol_ptr create_external_symbol_for_linked_entity(
                              a_symbol_locator     *locator,
                              a_boolean            is_function,
                              a_type_ptr           type_ptr,
                              a_name_linkage_kind  name_linkage,
                              a_boolean            redeclaration,
                              a_boolean            suppress_incompatible_error,
                              a_boolean            suppress_ext_sym_lookup,
                              a_variable_ptr       *variable_ptr,
                              a_routine_ptr        *routine_ptr)
/*
Find or create an external symbol entry for a variable or routine being
declared.  *locator gives the symbol locator for the identifier;
is_function is TRUE for a function, FALSE for a variable; type_ptr
gives the variable or routine type; linkage indicates the kind of
linkage the entity has (internal or external).  Aside from creating the
entry, this routine checks that the new declaration is compatible with
any previous linked declaration of the same name.  redeclaration is
TRUE if the present declaration is a redeclaration within the same scope.
suppress_incompatible_error is TRUE to suppress incompatibility errors
detected in this routine, presumably because the caller has already
issued a similar error.  If *variable_ptr and *routine_ptr are both NULL,
meaning no IL entity has been found for the entity, the appropriate one
of the two will be set to point to the IL entity attached to the external
symbol, if there is one.  Note that the pointer from the external symbol
entry to the IL variable or routine is not filled in if the entry is
created; the caller must set it.
*/
{
  a_symbol_ptr               ext_sym;
  an_extern_symbol_descr_ptr esdp;
  char                       *old_name, *new_name;
  a_symbol_kind              ext_sym_kind;
  a_symbol_locator           ext_locator;
  a_boolean                  err = FALSE;
  a_boolean                  use_existing_il_entry = FALSE;
  a_type_ptr                 preexisting_type;

  ext_sym_kind = is_function ? (a_symbol_kind)sk_extern_routine :
                               (a_symbol_kind)sk_extern_variable;
  if (suppress_ext_sym_lookup) {
    /* Ignore the presence of an external symbol with which the current
       symbol is compatible. */
    ext_sym = NULL;
    ext_locator = *locator;
    clear_specific_symbol(ext_locator);
  } else {
    /* Look up the external name of the identifier (i.e., the name after
       any truncation, etc.). */
    ext_sym = find_external_symbol(locator, name_linkage,
                                   is_function ? type_ptr : NULL,
                                   &ext_locator);
  }  /* if */
  if (ext_sym != NULL) {
    /* There is an existing external symbol for the name. */
    esdp = ext_sym->variant.extern_symbol_descr;
    if (!redeclaration) {
      /* Check if the old entity has a different name than the new entity,
         which would indicate an error (two different names ended up mapping
         to the same external name). */
      if (ext_sym->kind == (a_symbol_kind)sk_extern_variable) {
        old_name = esdp->variant.variable->source_corresp.name;
      } else {
        old_name = esdp->variant.routine->source_corresp.name;
      }  /* if */
      check_assertion(old_name != NULL);
      new_name = locator->symbol_header->identifier;
      if (old_name != new_name && strcmp(old_name, new_name) != 0) {
        /* Two different names ended up mapping to the same external name.
           In the error, reference the old name in its original form. */
        pos_st_error(ec_external_name_clash, &locator->source_position,
                     old_name);
        err = TRUE;
        /* Force creation of a new external symbol. */
        ext_sym = NULL;
      }  /* if */
    }  /* if */
    if (!err) {
      if (ext_sym_kind != ext_sym->kind) {
        /* The old entity is a variable and the new one is a routine, or
           vice-versa; error. */
        if (!suppress_incompatible_error) {
          pos_sy_error(ec_decl_incompatible_with_previous_use,
                       &locator->source_position, ext_sym);
        }  /* if */
        err = TRUE;
        /* Force creation of a new external symbol. */
        ext_sym = NULL;
      } else {
        /* Both are variables, or both are routines.  Compare the old and new
           types; they must be compatible. */
        err = !reconcile_external_symbol_types(ext_sym,
                                               &locator->source_position,
                                               type_ptr,
                                               suppress_incompatible_error);
      }  /* if */
    }  /* if */
  }  /* if */
  if (ext_sym == NULL) {
    /* There is no (compatible) external symbol entry for the identifier.
       Create one. */
    ext_sym = enter_symbol(ext_sym_kind, &ext_locator, DEPTH_OF_FILE_SCOPE,
                           /*suppress_error=*/TRUE);
    esdp = ext_sym->variant.extern_symbol_descr;
    esdp->type = type_ptr;
    /* The pointer to the variable or routine IL entry is filled in later,
       by the caller of this routine. */
  }  /* if */
  if (!err) {
    /* If we do not already have an IL entry, and the external symbol entry
       points to one, reuse it. */
    /* Note that since err == FALSE we know that the external symbol and
       the new entity are both variables or both routines. */
    if (!is_function) {
      /* The entity being declared is a variable. */
      if (*variable_ptr == NULL) {
        *variable_ptr = ext_sym->variant.extern_symbol_descr->variant.variable;
        if (*variable_ptr != NULL) {
          /* There is a variable entry we can reuse. */
          use_existing_il_entry = TRUE;
          preexisting_type = (*variable_ptr)->type;
          (*variable_ptr)->type = type_ptr;
        }  /* if */
      }  /* if */
    } else {
      /* The entity being declared is a routine. */
      if (*routine_ptr == NULL) {
        *routine_ptr = ext_sym->variant.extern_symbol_descr->variant.routine;
        if (*routine_ptr != NULL) {
          /* There is a routine entry we can reuse. */
          use_existing_il_entry = TRUE;
          preexisting_type = (*routine_ptr)->type;
          (*routine_ptr)->type = type_ptr;
        }  /* if */
      }  /* if */
    }  /* if */
    if (use_existing_il_entry) {
      /* An existing IL entry can be reused. */
      /* See if the entry's type has been changed.  Note that we're checking
         for pointer equality here, so an equivalent but distinct type
         will fail to match.  One of the issues that deals with is routine
         types with associated routine pointers -- the associated routine
         pointer needs to be preserved. */
      if (type_ptr != preexisting_type) {
        /* The type has been changed.  See if the pre-existing type will
           have to be restored at the end of the current scope.  If so,
           create a fixup entry that will be processed by pop_scope.  The
           type must be restored in a case like
             int a[];
             main () {
               extern int a[5];
               ... Type of "a" is now "int [5]".
             }
             ... Type of "a" must be restored to "int []" at the end of "main".
        */
        /* The fixup is only needed if the pre-existing definition is in
           a scope that surrounds the current one.  That's hard to determine,
           since it's hard to know the scope associated with the previous
           definition (the associated symbol gives only one scope, and
           not necessarily the outermost).  A simple way out is to build
           the fixup whenever the current scope is not the file scope.
           That builds more fixups than needed, but it always works. */
        if (C_dialect == C_dialect_pcc) {
          /* In pcc mode all symbols with linkage are entered at the
             file scope, so a fixup is never needed. */
        } else if (decl_scope_level != DEPTH_OF_FILE_SCOPE) {
          /* Create a fixup entry, which will be processed at the end
             of the current scope (see pop_scope).  Note that the
             allocation routine places the new entry on the fixup list
             for the current scope. */
          an_extern_type_fixup_ptr etfp;
          etfp = alloc_etype_fixup();
          etfp->type = preexisting_type;
          etfp->is_routine = is_function;
          if (!is_function) {
            etfp->variant.variable = *variable_ptr;
          } else {
            etfp->variant.routine = *routine_ptr;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return ext_sym;
}  /* create_external_symbol_for_linked_entity */


static void check_for_linkage_conflict(a_storage_class    *old_storage_class,
                                       an_id_linkage_kind *linkage,
                                       a_storage_class    *storage_class,
                                       a_source_position  *position,
                                       a_boolean          suppress_diagnostic)
/*
A variable or routine is being declared again.  The existing storage class
of the entity is *old_storage_class.  The linkage and storage class of the
new declaration are given by *linkage and *storage_class.  Issue a diagnostic
(at the indicated position) if the old and new linkages conflict, and update
*linkage, *storage_class, and *old_storage_class appropriately.
*/
{
  if ((*linkage == idl_internal) !=
                          (*old_storage_class == (a_storage_class)sc_static)) {
    /* External versus internal linkage conflict. */
    if (!suppress_diagnostic) {
      /* There is a conflict between a prior declaration and the current one.
         This is clearly an error in C++ (ARM 7.1.1, 7.1.2), but because of
         prevailing practice we only issue a remark.  The same is done in
         C mode, partly because it is common practice in pcc. */
      pos_diagnostic((strict_ansi_mode ?
                           strict_ansi_error_severity : es_remark),
                     ec_linkage_conflict, position);
    }  /* if */
    /* If either declaration has unspecified storage class (i.e., it's an
       external definition), that takes precedence, and the entity should
       have unspecified storage class.  Otherwise, because of the test above,
       one or the other of the declarations will have static storage class,
       and the entity should have static storage class.  extern storage
       class just defers to the other storage class, i.e., it's considered a
       reference to something else that is not necessarily external.  That
       means, for example, that "extern int f();" followed by
       "static int f();" yields a static routine (that's how pcc does it,
       and it's undefined according to the standard). */
    if (*old_storage_class == (a_storage_class)sc_unspecified ||
        *storage_class == (a_storage_class)sc_unspecified) {
      *storage_class = (a_storage_class)sc_unspecified;
      *linkage = idl_external;
    } else {
      *storage_class = (a_storage_class)sc_static;
      *linkage = idl_internal;
    }  /* if */
    *old_storage_class = *storage_class;
  }  /* if */
}  /* check_for_linkage_conflict */


static void check_default_args(a_type_ptr  type)
/*
Given a routine type based on a current declaration, where there is no
prior declaration with which to merge it, look for the case in which a
parameter with a default argument is followed in the parameter list by
one without a default argument, and report the error.
*/
{
  a_param_type_ptr  ptp;

  /* Loop through the single list. */
  ptp = skip_typerefs(type)->variant.routine.extra_info->param_type_list;
  for (; ptp != NULL; ptp = ptp->next) {
    if (ptp->has_default_arg && ptp->next != NULL &&
        !ptp->next->has_default_arg) {
      /* Current parameter has a default argument and its successor does
         not.  Report the error and break out of the loop. */
      error(ec_default_arg_not_at_end);
      break;
    }  /* if */
  }  /* for */
}  /* check_default_args */


static void check_default_arg_compatibility(a_type_ptr  orig_type,
                                            a_type_ptr  new_type)
/*
Given an existing routine type (orig_type) and the type based on a new
declaration (new_type), compare the default argument expressions on a
parameter-by-parameter basis and report any errors (see ARM 8.2.6).  The
merging of the default arguments occurs in composite_type.
*/
{
  a_boolean         not_at_end_of_list_error = FALSE;
  a_boolean         redecl_error = FALSE;
  a_boolean         default_arg_required = FALSE;
  a_param_type_ptr  ptp1, ptp2;

  /* Loop through the two lists in tandem.  We may assume that they are
     of equal length. */
  ptp1 = skip_typerefs(orig_type)->variant.routine.extra_info->param_type_list;
  ptp2 = skip_typerefs(new_type)->variant.routine.extra_info->param_type_list;
  for (; ptp1 != NULL; ptp1 = ptp1->next, ptp2 = ptp2->next) {
    if (ptp1->has_default_arg) {
      /* The parameter on the original type has a default arg. */
      if (ptp2->has_default_arg) {
        /* So does the parameter on the new type.  This is illegal. */
        redecl_error = TRUE;
      }  /* if */
      default_arg_required = TRUE;
    } else if (ptp2->has_default_arg) {
      default_arg_required = TRUE;
    } else if (default_arg_required) {
      not_at_end_of_list_error = TRUE;
    }  /* if */
  }  /* for */
  if (redecl_error) {
    error(ec_default_arg_already_defined);
  }  /* if */
  if (not_at_end_of_list_error) {
    error(ec_default_arg_not_at_end);
  }  /* if */
}  /* check_default_arg_compatibility */


void reconcile_routine_types(a_routine_ptr  routine_ptr,
                             a_type_ptr     type_ptr,
                             a_boolean      preserve_rout_type,
                             a_boolean      preserve_type_ptr)
/*
The routine routine_ptr has been given both the type it already has (i.e.,
routine_ptr->type) and the other type given by type_ptr; it may be assumed
that the two types are compatible.  Check the consistency of the default
argument specifications, if any, of the two types and set the routine type
to the composite of the two types.  If preserve_rout_type is TRUE,
routine_ptr->type must remain the same pointer; that type entry is
guaranteed to be unshared and is modified if necessary.  If
preserve_type_ptr is TRUE, routine_ptr->type must end up equal to type_ptr,
and type_ptr is guaranteed to be unshared and is modified if necessary.
Those flags are used when the associated type is part of a function
definition, and therefore already contains definition information like
assoc_routine which should not be overridden.  Obviously, both flags may
not be TRUE.
*/
{
  a_type_ptr        rout_type = routine_ptr->type;
  a_type_ptr        comp_type;
  a_param_type_ptr  rout_type_ptp, comp_type_ptp, next_rout_type_ptp;

  db_enter(4, "reconcile_routine_types");
  if (rout_type != type_ptr) {
    /* We only try to reconcile routine types that have already been
       determined to be compatible. */
    check_assertion(types_are_compatible(type_ptr, rout_type));
    /* We cannot be required to preserve the types from both sources. */
    check_assertion(!preserve_rout_type || !preserve_type_ptr);
    if (C_dialect == C_dialect_cplusplus) {
      /* If there are default arguments associated with the parameters, check
         them at this time.  They will be merged in composite types. */
      check_default_arg_compatibility(type_ptr, rout_type);
    }  /* if */
    /* The type of the routine should be the composite of the two types. */
    if (!preserve_rout_type && !preserve_type_ptr) {
      /* Simple case -- no required result type location. */
      routine_ptr->type = composite_type(type_ptr, rout_type);
    } else {
      /* Some requirement on where the result ends up.  Favor the type we'd
         like by passing it first to composite_type. */
      if (preserve_rout_type) {
        /* rout_type must be preserved. */
        comp_type = composite_type(rout_type, type_ptr);
      } else {
        /* type_ptr must be preserved. */
        comp_type = composite_type(type_ptr, rout_type);
#if GENERATE_SOURCE_SEQUENCE_LISTS
        /* This must be the defining declaration of the function, so record
           the type as it appears in the current declaration -- i.e., before
           it is merged with comp_type if comp_type is different. */
        check_assertion(routine_ptr->declared_type == NULL);
        if (comp_type == type_ptr) {
          /* type_ptr will not be modified. */
          routine_ptr->declared_type = type_ptr;
        } else {
           /* type_ptr will be modified, so copy it first. */
          routine_ptr->declared_type = alloc_type((a_type_kind)tk_routine);
          copy_routine_type_with_param_types(type_ptr,
                                             routine_ptr->declared_type);
        }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        routine_ptr->type = rout_type = type_ptr;
      }  /* if */
      /* If rout_type is not what was returned, copy the composite
         type on top of the existing rout_type (it's guaranteed to be
         unshared). */
      if (comp_type != rout_type) {
        comp_type = skip_typerefs(comp_type);
        /* Transfer the composite type to rout_type, which is unshared.
           We want to preserve fields like assoc_routine and arg_pragma in
           rout_type, so we can't just do a copy_type. */
        rout_type->variant.routine.return_type =
                            comp_type->variant.routine.return_type;
        rout_type->variant.routine.extra_info->prototyped =
                            comp_type->variant.routine.extra_info->prototyped;
        if (rout_type->variant.routine.extra_info->param_type_list == NULL) {
          /* The entire list may just be transferred over. */
          rout_type->variant.routine.extra_info->param_type_list =
                    comp_type->variant.routine.extra_info->param_type_list;
        } else {
          /* Copy the param type entries from the composite type onto the
             param type entries for the routine type.  This is done in case
             new param type entries were created.  The original ones must be
             preserved, however, since they may be pointed to by the parameter
             variables with which they are associated. */
          rout_type_ptp =
                     rout_type->variant.routine.extra_info->param_type_list;
          comp_type_ptp =
                     comp_type->variant.routine.extra_info->param_type_list;
          for (; rout_type_ptp != NULL; rout_type_ptp = next_rout_type_ptp,
                                        comp_type_ptp = comp_type_ptp->next) {
            if (rout_type_ptp == comp_type_ptp) {
              /* Whenever the corresponding param type entries on the two
                 lists are the same entry, all subsequent ones will also be
                 the same, so we can bail out at that point. */
              break;
            }  /* if */
            /* Save the original next pointer and restore it after the copy. */
            next_rout_type_ptp = rout_type_ptp->next;
            *rout_type_ptp = *comp_type_ptp;
            rout_type_ptp->next = next_rout_type_ptp;
          }  /* for */
        }  /* if */
        /* has_ellipsis need not be copied -- it will be the same in all of
           the types, since the original two types are compatible. */
        /* Likewise, the implicit_this_param_type pointers should be identical
           -- this will have been verified in types_are_compatible. */
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
}  /* reconcile_routine_types */

#if GENERATE_SOURCE_SEQUENCE_LISTS

void set_src_seq_secondary_decl_type(char        *il_entry_ptr,
                                     a_type_ptr  type)
/*
Set the declared_type field to "type" in the recently created secondary
source sequence entry created for the IL entry pointed to by il_entry_ptr.
*/
{
  a_source_sequence_entry_ptr  ssep;

  if (source_sequence_entries_disallowed) {
    /* We are in a context in which source sequence entries are not being
       created.  No further action is required. */
  } else {
    ssep = last_matching_source_sequence_entry(il_entry_ptr);
    if (ssep != NULL) {
      check_assertion(ss_entry_kind(ssep) == iek_src_seq_secondary_decl);
      ((a_src_seq_secondary_decl_ptr)ssep->entity.ptr)->declared_type = type;
    }  /* if */
  }  /* if */
}  /* set_src_seq_secondary_decl_type */

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */


static void mark_symbol_to_suppress_warnings(a_symbol_ptr  sym)
/*
The given symbol is involved in an error in some way or another.  Set flags
as appropriate to suppress warnings (e.g., in end_of_scope_symbol_check).
*/
{
  /* Suppress declared-but-not-referenced warnings. */
  sym->referenced = TRUE;
  if (sym->kind == (a_symbol_kind)sk_variable) {
    /* Suppress set-but-not-used warnings. */
    sym->variant.variable.used = TRUE;
  }  /* if */
}  /* mark_symbol_to_suppress_warnings */


void decl_var_or_routine(a_symbol_locator             *locator,
                         a_storage_class              storage_class,
                         a_type_ptr                   type_ptr,
                         a_func_info_block_ptr        func_info,
                         a_source_sequence_entry_ptr  declarator_ssep,
                         a_symbol_reference_kind      srk_flags,
                         a_symbol_ptr                 *symbol_ptr,
                         an_id_linkage_kind           *linkage_ptr,
                         a_type_ptr                   *old_type,
                         a_symbol_ptr                 *ext_sym)
/*
Enter the declaration of an identifier for a variable or routine.
*locator gives the symbol locator (and thus its name and its declaration
position).  storage_class and type_ptr give the storage class and type.
func_info will be non-NULL if and only if this is a function declaration.
If it is non-NULL then: if func_info->implicit_declaration is TRUE, this
declaration is for an implicit function declaration, and *symbol_ptr
already contains a pointer to the symbol entry, which is already in the
symbol table; if func_info->is_definition is TRUE, the identifier being
defined is part of a function definition (meaning there is a body in the
definition), in which case it is guaranteed that type_ptr points to an
unshared type entry, and that type entry will be preserved as the routine
type.  Create and enter a symbol entry, and return a pointer to it in
*symbol_ptr.  Also allocate any associated IL construct, and attach it to
the symbol.  If the identifier has linkage and there is an existing symbol
or IL entry, it will be re-used.  Return in *linkage_ptr the linkage of
the identifier.  Return in *old_type any previously-known type for this
identifier from a linked identifier in the same scope, or NULL if there
was no previously-known type.  If the identifier has linkage, return in
*ext_sym a pointer to the external symbol entry; otherwise, set *ext_sym
to NULL.  declarator_ssep (non-NULL only if source sequence entries are
being generated) is a pointer to the empty source sequence entry already
created for the declarator and added to the appropriate list; its kind
and entity pointer are updated.  srk_flags contain specific information
about the kind of declaration (whether it's a definition, a tentative
definition (C only), an implicit declaration (C only), a friend declaration
(C++ only), and so forth); this information is passed on for use in
generating cross-reference output describing this declaration.
*/
{
  a_symbol_ptr             sym = NULL;
  a_boolean                is_function;
  a_boolean                at_file_scope;
  a_symbol_ptr             linked_symbol, homonym_symbol;
  a_symbol_ptr             overload_symbol = NULL;
  a_boolean                redecl_error_already_issued = FALSE;
  a_boolean                linked_redecl_error = FALSE;
  a_boolean                old_decl_has_body = FALSE;
  a_boolean                redeclaration = FALSE;
  a_variable_ptr           variable_ptr = NULL;
  a_routine_ptr            routine_ptr = NULL;
  an_id_linkage_kind       linkage;
  a_source_correspondence  *source_corresp_ptr;
  a_scope_depth            effective_decl_level = decl_scope_level;
  a_boolean                template_function_specific_decl = FALSE;
  a_boolean                suppress_ext_sym_lookup = FALSE;
  a_boolean                is_main_function = FALSE;
  a_boolean                is_function_def = FALSE;
  a_boolean                changed_to_inline = FALSE;
  a_boolean                is_variable_def = FALSE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_type_ptr               declared_type = type_ptr;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

  db_enter(3, "decl_var_or_routine");
  *old_type = NULL;
  is_function = is_function_type(type_ptr);
  check_assertion(is_function == (func_info != NULL));
  check_assertion(storage_class != (a_storage_class)sc_typedef);
  check_assertion(srk_flags & SRK_DECLARATION);
  if (is_function) {
    if (func_info->is_main_function) is_main_function = TRUE;
    if (func_info->is_definition) {
      is_function_def = TRUE;
      check_assertion(srk_flags & SRK_DEFINITION);
    }  /* if */
    if (C_dialect == C_dialect_cplusplus) {
      if (func_info->is_inline) {
        check_assertion(storage_class == (a_storage_class)sc_unspecified ||
                        storage_class == (a_storage_class)sc_static);
        storage_class = (a_storage_class)sc_static;
      }  /* if */
      /* If this is an overloaded operator, check for errors in the
         argument list. */
      check_operator_function_params(type_ptr, /*class_type=*/(a_type_ptr)NULL,
                                     locator);
    }  /* if */
  } else {
    if (srk_flags & SRK_DEFINITION) is_variable_def = TRUE;
  }  /* if */
  if (is_function && func_info->is_implicit_declaration) {
    check_assertion(srk_flags & SRK_IMPLICIT);
    if (C_dialect != C_dialect_cplusplus) {
      /* For an implicit function, the identifier would not be in the process
         of being declared implicitly as a function if there were any visible
         declaration of it, and therefore it must have external linkage. */
      linkage = idl_external;
    } else {
      /* In C++ this is an error case.  Don't give this dummy routine any
         linkage. */
      linkage = idl_none;
    }  /* if */
    linked_symbol = NULL;
    homonym_symbol = NULL;
    sym = *symbol_ptr;
  } else {
    /* Determine the linkage of this symbol. */
    linkage = id_linkage(locator, &storage_class, type_ptr, is_main_function,
                         &linked_symbol, &homonym_symbol,
                         &effective_decl_level);
  }  /* if */
  /* at_file_scope will be TRUE if the IL variable or routine must be
     allocated in the file scope memory region.  This is always true
     of routines, and also true of variables with linkage. */
  at_file_scope = (is_function || linkage != idl_none);
  if (linkage != idl_none && linked_symbol != NULL) {
    /* There is a previous identifier of this name in the same scope,
       to which this declaration is linked. */
    if (is_function && linked_symbol->kind == (a_symbol_kind)sk_routine &&
        linked_symbol->variant.routine.instance_ptr != NULL) {
      /* This is not actually a redeclaration -- linked_symbol refers to a
         function template instantiation. */
      template_function_specific_decl = TRUE;
    } else {
      /* The new declaration must be compatible with the old. */
      redeclaration = TRUE;
    }  /* if */
  }  /* if */
  if (redeclaration) {
    if (linked_symbol->kind == (a_symbol_kind)sk_variable && !is_function) {
      if (C_mode() && linked_symbol->defined) {
        if (linked_symbol->variant.variable.ptr->init_kind !=
                                               (an_init_kind)initk_none) {
          /* The variable was initialized on a prior declaration, so this
             cannot be a definition or a tentative definition.  (The error
             will be reported by the caller if there is an initializer on
             this declaration, too.) */
          srk_flags &= ~(SRK_TENTATIVE_DEF | SRK_DEFINITION);
          check_assertion(srk_flags & SRK_DECLARATION);
          is_variable_def = FALSE;
        }  /* if */
      }  /* if */
      if (linked_symbol->defined && is_variable_def && !C_mode()) {
        /* Variable has already been defined.  Issue an error here and
           suppress an error when the symbol is entered. */
        pos_sy_error(ec_already_defined, &locator->source_position,
                     linked_symbol);
        redecl_error_already_issued = TRUE;
        linked_redecl_error = TRUE;
        /* Set a flag to suppress reuse of the existing external-variable
           symbol and of the variable already in use.  This is to avoid
           redundant errors in case both this and the previous definition
           involved initialization.  Also, to suppress a declared-but-not-used
           message, set the referenced flag in the linked symbol. */
        suppress_ext_sym_lookup = TRUE;
        mark_symbol_to_suppress_warnings(linked_symbol);
      } else {
        /* Linked symbol and new symbol are both variables.  See if they
           are compatible. */
        sym = linked_symbol;
        variable_ptr = linked_symbol->variant.variable.ptr;
        check_assertion(variable_ptr != NULL);
        *old_type = variable_ptr->type;
        if (!types_are_compatible(type_ptr, *old_type)) {
          pos_sy_error(ec_not_compatible_with_previous_decl,
                       &locator->source_position, linked_symbol);
          redecl_error_already_issued = TRUE;
          linked_redecl_error = TRUE;
        } else {
          /* The type of the variable should be the composite of the two
             types. */
          variable_ptr->type = type_ptr = composite_type(type_ptr, *old_type);
        }  /* if */
      }  /* if */
    } else if (linked_symbol->kind == (a_symbol_kind)sk_routine &&
               is_function) {
      /* Linked symbol and new symbol are both routines.  The new declaration
         must be compatible with the old. */
      sym = linked_symbol;
      routine_ptr = linked_symbol->variant.routine.ptr;
      check_assertion(routine_ptr != NULL);
      if (routine_ptr->assoc_scope != NULL_region_number
#if ASM_FUNCTION_ALLOWED
          || routine_ptr->storage_class == (a_storage_class)sc_asm
#endif /* ASM_FUNCTION_ALLOWED */
                                                        ) {
        old_decl_has_body = TRUE;
      } else if (sym->defined) {
        /* In C++ the defined flag may have been set without the body having
           been scanned and bound to the routine yet (e.g., inline friend
           function). */
        check_assertion(scope_stack[decl_scope_level].kind ==
                                        (a_scope_kind)sck_class_struct_union);
        old_decl_has_body = TRUE;
      }  /* if */
      if (is_function_def && old_decl_has_body) {
        /* Previous routine already has a body, and new one does (or will)
           too. */
        pos_sy_error(ec_already_defined, &locator->source_position, sym);
        redecl_error_already_issued = TRUE;
        linked_redecl_error = TRUE;
        /* Set a flag to suppress reuse of the existing external-routine
           symbol and of the routine already in use.  Also, to suppress a
           possible declared-but-not-used message, set the referenced flag
           in the linked symbol. */
        suppress_ext_sym_lookup = TRUE;
        mark_symbol_to_suppress_warnings(linked_symbol);
      } else {
        /* If this is not C++ mode (for which this check has already been
           done in id_linkage), be sure that the old and new types are
           compatible.  Then form the composite type.  Note that there is a
           second test for compatibility after old-style parameter
           declarations are scanned, if this declaration has a body (see
           function_definition). */
        if ((C_dialect != C_dialect_cplusplus || is_main_function) &&
            !types_are_compatible(routine_ptr->type, type_ptr)) {
          pos_sy_error(ec_not_compatible_with_previous_decl,
                       &locator->source_position, linked_symbol);
          redecl_error_already_issued = TRUE;
          if (!old_decl_has_body) {
            routine_ptr->type = type_ptr;
          } else {
            type_ptr = routine_ptr->type;
          }  /* if */
          linked_redecl_error = TRUE;
        } else {
          *old_type = routine_ptr->type;
          reconcile_routine_types(routine_ptr, type_ptr,
                                  /*preserve_rout_type=*/old_decl_has_body,
                                  /*preserve_type_ptr=*/is_function_def);
        }  /* if */
      }  /* if */
    } else {
      /* The linked symbol is a variable, while the new one is a routine,
         or vice-versa; error. */
      pos_sy_error(ec_not_compatible_with_previous_decl,
                   &locator->source_position, linked_symbol);
      redecl_error_already_issued = TRUE;
      linked_redecl_error = TRUE;
    }  /* if */
  } else {
    /* Not a redeclaration. */
    if (is_function && C_dialect == C_dialect_cplusplus) {
      /* Be sure the default arguments, if any, are at the end of the
         parameters list. */
      check_default_args(type_ptr);
    }  /* if */
    if (homonym_symbol != NULL) {
      /* homonym_symbol is a previously declared routine symbol with the
         same name but a different type signature from that of the current
         declaration.  We may have an instance of function overloading. */
      an_error_code  error_code;

      if (homonym_symbol->kind != (a_symbol_kind)sk_overloaded_function &&
          homonym_symbol->kind != (a_symbol_kind)sk_function_template) {
        a_routine_ptr  rp = homonym_symbol->variant.routine.ptr;
        if (rp->special_kind == (a_special_function_kind)sfk_operator &&
            rp->opname_kind == (an_opname_kind)onk_delete) {
          /* Overloading is not allowed for operator delete() (ARM 12.5). */
          pos_error(ec_delete_already_declared, &locator->source_position);
          redecl_error_already_issued = TRUE;
          goto skip_overloading;
        }  /* if */
      }  /* if */
      if (homonym_symbol->kind != (a_symbol_kind)sk_function_template &&
          !overload_distinguishable(homonym_symbol, type_ptr,
                                    /*new_is_template=*/FALSE, &error_code)) {
        /* The previous declaration and the current one are not "overload
           distinguishable" for a reason given by the error code returned. */
        pos_error(error_code, &locator->source_position);
        redecl_error_already_issued = TRUE;
        /* We can't add a symbol to the overload list, so change to locator
           to an error locator to prevent hiding the overload symbol when the
           new symbol is entered. */
        set_to_error_locator(*locator);
        /* Don't treat this as a template function specific declaration even
           if it was previously thought to be.  Do treat it as a redeclaration
           error. */
        template_function_specific_decl = FALSE;
        linked_redecl_error = TRUE;
        goto skip_overloading;
      }  /* if */
    }  /* if */
    if (template_function_specific_decl &&
        effective_decl_level == DEPTH_OF_FILE_SCOPE) {
      /* This is an explicit declaration of a template function.  Note that
         we are only interested in file-scope declarations -- declarations at
         local scope are handled separately. */
      sym = linked_symbol;
      routine_ptr = sym->variant.routine.ptr;
      old_decl_has_body = (routine_ptr->assoc_scope != NULL_region_number);
      if (is_function_def) {
        /* The current declaration is a definition. */
        if (!old_decl_has_body) {
          /* Okay. */
          routine_ptr->specific_def = TRUE;
          sym->variant.routine.instance_ptr->specific_def = TRUE;
        } else {
          /* There is already a definition.  This is some sort of error. */
          if (routine_ptr->specific_def) {
            /* Already defined. */
            pos_sy_error(ec_already_defined, &locator->source_position, sym);
          } else {
            /* It must be that this function has a body as a result of a
               prior instantiation. */
            check_assertion(routine_ptr->is_inline && routine_ptr->called);
              /* An inline function template has been declared, an instance
                 of it has been referenced and therefore instantiated on
                 the fly, and now a specializing declaration appears.
                 Issue an error (you can't reference an inline template
                 function that is specialized before the specialization is
                 declared) and treat it as a redeclaration error. */
            pos_error(ec_specialization_of_called_inline_template_function,
                      &locator->source_position);
          }  /* if */
          linked_redecl_error = TRUE;
          template_function_specific_decl = FALSE;
          redecl_error_already_issued = TRUE;
          set_to_named_error_locator(*locator);
          /* Set a flag to suppress reuse of the existing external-routine
             symbol and of the routine already in use.  Also, to suppress a
             possible declared-but-not-used message, set the referenced flag
             in the linked symbol. */
          suppress_ext_sym_lookup = TRUE;
          mark_symbol_to_suppress_warnings(linked_symbol);
        }  /* if */
      }  /* if */
      if (!linked_redecl_error) {
        if (!sym->variant.routine.instance_ptr->specific_decl) {
          check_assertion(homonym_symbol != NULL);
          /*  Its symbol is already on the template's function instantiation
              list, but it needs to be added to the overload list as well,
              to assure that it will be found by the ordinary overload
              resolution algorithm. */
          overload_symbol = add_symbol_to_overload_list(sym, homonym_symbol);
          sym->variant.routine.instance_ptr->specific_decl = TRUE;
        }  /* if */
        *old_type = routine_ptr->type;
        reconcile_routine_types(routine_ptr, type_ptr,
                                /*preserve_rout_type=*/old_decl_has_body,
                                /*preserve_type_ptr=*/is_function_def);
      }  /* if */
    } else if (homonym_symbol != NULL) {
      /* Overloaded function.  Create the new symbol, which will be on the
         list of functions connected to an sk_overloaded symbol. */
      sym = enter_overloaded_symbol((a_symbol_kind)sk_routine, locator,
                                    homonym_symbol, &overload_symbol);
    }  /* if */
skip_overloading:;
  }  /* if */
  if (linked_redecl_error) {
    /* There is a linked symbol, but it is not compatible with the new
       declaration.  Force a new symbol and a new IL entry. */
    sym = NULL;
    linked_symbol = NULL;
    variable_ptr = NULL;
    routine_ptr = NULL;
    *old_type = NULL;
    redeclaration = FALSE;
  }  /* if */
  if (sym == NULL) {
    /* There is no (compatible) symbol, so enter one now. */
    sym = enter_local_symbol
                  ((a_symbol_kind)(is_function ? sk_routine : sk_variable),
                   locator, effective_decl_level, redecl_error_already_issued);
  } else if (is_function && func_info->is_implicit_declaration) {
    /* This is an implicit declaration of a function.  The symbol has
       already been entered and marked as declared. */
  } else {
    if (C_dialect == C_dialect_cplusplus && is_function &&
        routine_ptr != NULL) {
      /* Do compatibility checking on the throw specification and, if this
         is a definition, bind the throw specification to the routine entry.
         Note that if it is a definition the checking must be done before
         the routine's decl position is modified, to assure that the
         "original declaration line number" is displayed accurately. */
      check_exception_specification(func_info, routine_ptr);
    }  /* if */
  }  /* if */
  if (C_dialect == C_dialect_cplusplus) {
    if (!is_function && decl_scope_level == DEPTH_OF_FILE_SCOPE &&
        storage_class == (a_storage_class)sc_unspecified &&
        is_const_qualified_type(type_ptr)) {
      /* In C++ all const qualified objects at file scope with no explicit
         storage class are internally linked unless previously declared to
         be extern (ARM 7.1.1).  The storage class has been left "unspecified"
         because till now we didn't know whether this was a redeclaration. */
      if (variable_ptr == NULL ||
          variable_ptr->storage_class != (a_storage_class)sc_extern) {
        storage_class = (a_storage_class)sc_static;
        linkage = idl_internal;
      }  /* if */
    }  /* if */
  }  /* if */
  *ext_sym = NULL;
  if (linkage != idl_none) {
    /* The symbol has external or internal linkage.  Find or create an
       external symbol entry for the identifier name, to check that the
       current declaration is compatible with any previous and future
       declarations of the same name.  Note that while other declarations
       are required to be compatible with the present one (because all
       declarations of a name with linkage refer to the same object or
       function), no composite type is formed; the type of the IL entity
       is only what is known in the current scope.  The external symbol
       keeps track of the full composite type behind the scenes.
       If we do not already have an IL entry, and the external symbol entry
       points to one, get a pointer to it and use it. */
    /* Determine the name linkage that should be used in looking up an
       existing external symbol entry. */
    a_name_linkage_kind  name_linkage;

    if (linkage == idl_internal) {
      name_linkage = (a_name_linkage_kind)nlk_internal;
    } else if (C_dialect != C_dialect_cplusplus || is_main_function) {
      name_linkage = (a_name_linkage_kind)nlk_external;
    } else {
      name_linkage = def_external_linkage.kind;
    }  /* if */
    *ext_sym = create_external_symbol_for_linked_entity(
                                                 locator, is_function,
                                                 type_ptr, name_linkage,
                                                 redeclaration,
                                                 linked_redecl_error,
                                                 suppress_ext_sym_lookup,
                                                 &variable_ptr, &routine_ptr);
  }  /* if */
  if (!is_function) {
    /* The entity being declared is a variable. */
    if (variable_ptr == NULL) {
      /* There is no IL entry, so create one now.  If the variable has
         internal or external linkage, it is entered at the file scope. */
      variable_ptr = make_variable(type_ptr, storage_class, at_file_scope);
      source_corresp_ptr = &variable_ptr->source_corresp;
    } else {
      /* There is an existing IL entry that we are reusing. */
      /* Check for internal linkage on the old but not the new, or
         vice-versa. */
      check_for_linkage_conflict(&variable_ptr->storage_class,
                                 &linkage, &storage_class,
                                 &locator->source_position,
                                 /*suppress_diagnostic=*/linked_redecl_error);
      /* Modify the storage class if necessary (an unspecified storage 
         class on the new declaration indicates a tentative definition --
         see 3.7.2).  Do not force anything but sc_unspecified on the
         preexisting variable entry -- we don't want to change the storage
         class in a case like this:  int i; extern int i; . */
      if (storage_class == (a_storage_class)sc_unspecified) {
        variable_ptr->storage_class = (a_storage_class)sc_unspecified;
      }  /* if */
      /* If the IL entry was previously referenced, the symbol should be
         marked as referenced too.  We may have a case like this:
           void f() { extern int i; i = 0; }
           int i;
         The IL entity associated with i is referenced in the function scope
         but the symbol at file scope is created later -- it should have its
         "referenced" flag set to prevent unwanted "defined but not referenced"
         warnings from being put out. */
      if (storage_class != (a_storage_class)sc_extern &&
          variable_ptr->source_corresp.referenced) {
        sym->referenced = TRUE;
      }  /* if */
      /* Similarly, it should have it's "used" flag set.  This is only needed
         for file-scope static variables, in cases like this:
           int f() { extern int i; return i; }
           static int i = 0;
         to avoid "set-but-never-used" diagnostics. */
      source_corresp_ptr = &variable_ptr->source_corresp;
      if (((a_symbol_ptr)source_corresp_ptr->assoc_info)->
                                                    variant.variable.used) {
        sym->variant.variable.used = TRUE;
      }  /* if */
      /* Move the variable entry to the end of the variables list if this is
         its definition. */
      if (srk_flags & SRK_DEFINITION) {
        /* This is definition of a variable that has previously been declared.
           In C++ only one declaration of a variable can be construed to be
           its definition, so if we are in C++ mode this is it. */
        if (C_mode() && sym->defined && (srk_flags & SRK_TENTATIVE_DEF)) {
          /* In C ignore a tentative definition (i.e., one for which no
             initializer is present) if the variable has already been defined
             in a previous tentative definition. */
        } else {
          /* This is a definition of a variable that was not previously
             defined, so unlink the variable entry and relink it at the end
             of the variables list, so that variables appear in the order in
             which they are defined. */
          /* This is only possible for file-scope variables, never for local
             variables, since it is only by means of a prior extern declaration
             or (in C mode only) a prior tentative definition that we can be
             defining a variable that has already been declared. */
          check_assertion(in_file_scope(variable_ptr));
          remove_from_variables_list(variable_ptr);
          add_to_variables_list(variable_ptr, /*at_file_scope=*/TRUE);
        }  /* if */
      }  /* if */
    }  /* if */
    /* Link the symbol to the IL variable entry. */
    sym->variant.variable.ptr = variable_ptr;
    if (*ext_sym != NULL) {
      /* Link the external symbol to the IL variable entry. */
      (*ext_sym)->variant.extern_symbol_descr->variant.variable = variable_ptr;
    }  /* if */
  } else {
    /* The entity being declared is a routine. */
    if (template_function_specific_decl && sym != linked_symbol) {
      /* This is a declaration of a template function at the local scope.
         A function instantiation entry with an associated symbol and routine
         entry already exist.  Be sure this local symbol is properly bound
         to the file-scope entities to which it corresponds. */
      check_assertion(linked_symbol != NULL &&
                      effective_decl_level != DEPTH_OF_FILE_SCOPE &&
                      (routine_ptr == NULL ||
                       routine_ptr == linked_symbol->variant.routine.ptr));
      sym->variant.routine.instance_ptr =
                                  linked_symbol->variant.routine.instance_ptr;
      routine_ptr = linked_symbol->variant.routine.ptr;
      sym->variant.routine.ptr = routine_ptr;
      *old_type = routine_ptr->type;
      reconcile_routine_types(routine_ptr, type_ptr,
                              /*preserve_rout_type=*/TRUE,
                              /*preserve_type_ptr=*/FALSE);
      /* Do compatibility checking for the throw specification. */
      check_exception_specification(func_info, routine_ptr);
    } else if (routine_ptr == NULL) {
      /* There is no IL entry, so create one now, and add it to the routine
         list of the file scope. */
      routine_ptr = make_routine(type_ptr, storage_class,
                                 /*at_file_scope=*/TRUE, /*add_to_list=*/TRUE);
      if (C_dialect == C_dialect_cplusplus) {
        /* Bind the throw specification to the routine entry. */
        add_exception_specification(func_info, routine_ptr);
        if (locator->is_operator_name) {
          routine_ptr->special_kind = (a_special_function_kind)sfk_operator;
          routine_ptr->opname_kind = locator->variant.opname;
        }  /* if */
      }  /* if */
    } else {
      /* There is an existing IL entry that we are reusing. */
      /* Check for internal linkage on the old but not the new, or
         vice-versa. */
      a_boolean suppress_diagnostic = linked_redecl_error;

      if (routine_ptr->compiler_generated) {
        /* This is an entry for a compiler generated ::operator new or
           ::operator delete.  It was created during initialization, but
           is overridden by the present declaration. */
        check_assertion(routine_ptr->special_kind ==
                                (a_special_function_kind)sfk_operator &&
                        (routine_ptr->opname_kind ==
                                                 (an_opname_kind)onk_new ||
                         routine_ptr->opname_kind ==
                                                 (an_opname_kind)onk_delete));
        routine_ptr->compiler_generated = FALSE;
        check_assertion(sym->decl_position.seq == 0);
        /* Record the new source position, both in the symbol and in the
           routine entry. */
        sym->decl_position = locator->source_position;
        routine_ptr->source_corresp.decl_position = sym->decl_position;
        suppress_diagnostic = TRUE;
      }  /* if */
#if ASM_FUNCTION_ALLOWED
      if (storage_class == (a_storage_class)sc_asm ||
          routine_ptr->storage_class == (a_storage_class)sc_asm) {
        /* asm functions have internal linkage but do not conflict
           with previous declarations that are either extern or static. */
          routine_ptr->storage_class = storage_class = (a_storage_class)sc_asm;
      } else {
#endif /* ASM_FUNCTION_ALLOWED */
        check_for_linkage_conflict(&routine_ptr->storage_class, &linkage,
                                   &storage_class, &locator->source_position,
                                   suppress_diagnostic);
#if ASM_FUNCTION_ALLOWED
      }  /* if */
#endif /* ASM_FUNCTION_ALLOWED */
      if (is_function_def) {
        a_boolean saved_referenced_flag;
        /* If this is a definition, unlink the routine entry and relink it
           at the end of the routines list, so that routines appear in the
           order that their bodies appear. */
        remove_from_routines_list(routine_ptr);
        add_to_routines_list(routine_ptr, /*at_file_scope=*/TRUE);
        /* Put in the storage class for the definition (static or 
           unspecified). */
        routine_ptr->storage_class = storage_class;
        /* If the IL entry was previously referenced, the symbol should
           be considered to have been referenced as well. */
        saved_referenced_flag = routine_ptr->source_corresp.referenced;
        if (saved_referenced_flag) sym->referenced = TRUE;
        /* Reset the source correspondence to the definition symbol. */
        set_source_corresp(&routine_ptr->source_corresp, sym);
        /* Keep an indication of any references so far (the referenced
           flag is reset by the set_source_corresp call). */
        routine_ptr->source_corresp.referenced = saved_referenced_flag;
      }  /* if */
      changed_to_inline = (func_info->is_inline && !routine_ptr->is_inline);
    }  /* if */
    if (func_info->is_inline) routine_ptr->is_inline = TRUE;
    source_corresp_ptr = &routine_ptr->source_corresp;
    /* Link the symbol to the IL routine entry. */
    sym->variant.routine.ptr = routine_ptr;
    if (*ext_sym != NULL) {
      /* Link the external symbol to the IL routine entry. */
      (*ext_sym)->variant.extern_symbol_descr->variant.routine = routine_ptr;
    }  /* if */
  }  /* if */
  /* Set the source correspondence, but leave it pointing at an outer-scope
     symbol if there is one. */
  if (source_corresp_ptr->assoc_info == NULL) {
    /* There is no symbol pointed to from the variable or routine, so
       update it with the current symbol. */
    set_source_corresp(source_corresp_ptr, sym);
  } else if (!redeclaration && !template_function_specific_decl) {
    /* Record a reference to the outer-scope symbol of the same name,
       but do not set the IL entity referenced flag. */
    record_symbol_reference(SRK_REFERENCE,
                            (a_symbol_ptr)source_corresp_ptr->assoc_info,
                            &locator->source_position,
                            /*update_il_entry=*/FALSE);
  }  /* if */
  if (changed_to_inline) {
    if (routine_ptr->called) {
      pos_sy_error(ec_called_function_redeclared_inline,
                   &locator->source_position, sym);
    }  /* if */
  }  /* if */
  if (linkage == idl_external) {
    /* Indicate in the IL entry that the name is externally visible by
       assigning the external linkage kind that is the default for the current
       context. */
    if (C_dialect != C_dialect_cplusplus || is_main_function) {
      /* Note that "main" is always given "C" linkage. */
      source_corresp_ptr->name_linkage = (a_name_linkage_kind)nlk_external;
      sym->explicit_linkage_specifier = FALSE;
    } else if (source_corresp_ptr->name_linkage ==
                                         (a_name_linkage_kind)nlk_none) {
      /* No prior declaration, so there's no conflict. */
      source_corresp_ptr->name_linkage = def_external_linkage.kind;
      sym->explicit_linkage_specifier = 
                              (*ext_sym)->explicit_linkage_specifier = 
                                         def_external_linkage.is_explicit;
      if (overload_symbol != NULL &&
          def_external_linkage.kind == (a_name_linkage_kind)nlk_external) {
        /* "At most one of a set of overloaded functions . . . can have
           C linkage" (ARM 7.4).  Search for conflicts. */
        a_symbol_ptr  sp;
        for (sp = overload_symbol->variant.overloaded_function.symbols;
             sp != NULL;
             sp = sp->next) {
          if (sp != sym &&
              sp->variant.routine.ptr->source_corresp.name_linkage ==
                                         (a_name_linkage_kind)nlk_external) {
            pos_sy_error(ec_overloaded_function_linkage,
                         &locator->source_position, overload_symbol);
            break;
          }  /* if */
        }  /* for */
      }  /* if */
    } else {
      /* Multiple specifications of external linkage must be the same
         (ARM 7.4).  But it's a little trickier than that.  We will not
         override the previous specification, but we need to be sure the
         current one is consistent with it. */
      a_boolean  err = FALSE;
      if (source_corresp_ptr->name_linkage == def_external_linkage.kind) {
        /* The linkage kinds (C or C++) are the same; however, the ARM states,
           "A function declaration without a linkage specification may not
           precede the first linkage specification for that function." */
        if (def_external_linkage.is_explicit) {
          err = (!sym->explicit_linkage_specifier &&
                 !(*ext_sym)->explicit_linkage_specifier);
          /* Mark the symbols as having an explicit linkage specifier to
             keep this error from occurring again later. */
          sym->explicit_linkage_specifier = 
                              (*ext_sym)->explicit_linkage_specifier = TRUE;
        }  /* if */
      } else {
        /* Linkage is not the same, but it's no error as long as the current
           specification is implicit. */
        err = def_external_linkage.is_explicit;
      }  /* if */
      if (err) {
        /* The ARM specifies that inconsistencies are errors for functions but
           not for variables.  Just issue a remark or, in strict ansi mode,
           a warning in the latter case. */
        pos_sy_diagnostic(is_function ?
                            (an_error_severity)es_error :
                            (strict_ansi_mode ?
                               (an_error_severity)es_warning :
                               (an_error_severity)es_remark),
                          ec_incompatible_linkage_specifier,
                          &locator->source_position, *ext_sym);
      }  /* if */
    }  /* if */
  } else if (linkage == idl_internal) {
    /* Internal linkage. */
    source_corresp_ptr->name_linkage = (a_name_linkage_kind)nlk_internal;
  } else {
    /* No linkage -- e.g., an automatic variable. */
    check_assertion(source_corresp_ptr->name_linkage ==
                                               (a_name_linkage_kind)nlk_none);
  }  /* if */
  if (C_dialect == C_dialect_cplusplus) {
    /* A variable or routine with linkage should not be declared in terms of
       a local type. */
    if (source_corresp_ptr->name_linkage != (a_name_linkage_kind)nlk_none) {
      if (is_or_contains_local_type(type_ptr)) {
        pos_warning(is_function ? ec_local_type_in_function :
                                  ec_local_type_in_nonlocal_var,
                    &locator->source_position);
      }  /* if */
    }  /* if */
  }  /* if */
  /* If cross-reference information is being issued, update the output.  If
     source sequence entries are being generated, update the declarator_ssep
     entry. */
  record_symbol_declaration(srk_flags, sym, &locator->source_position,
                            declarator_ssep);
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Do fixup on the source sequence entry that was just created to
     represent the current declaration.  Note that declaration_ssep is not
     used, since it may have been replaced (e.g., when a file scope entity
     is declared in a local scope and a sublist is generated). */
  if (is_function) {
    if (is_function_def) {
      /* The defining declaration of the function.  The type as it actually
         appeared in the current declaration may already have been set in
         reconcile_routine_types. */
      if (routine_ptr->declared_type == NULL) {
        check_assertion(type_ptr == declared_type);
        routine_ptr->declared_type = type_ptr;
      }  /* if */
    } else {
      /* A function declaration but not a definition.  Set the type in the
         secondary declaration entry. */
      set_src_seq_secondary_decl_type((char *)routine_ptr, declared_type);
    }  /* if */
  } else {
    if (!is_variable_def || (srk_flags & SRK_TENTATIVE_DEF)) {
      /* A function declaration but not a definition. */
      set_src_seq_secondary_decl_type((char *)variable_ptr, declared_type);
    } else {
      /* The defining declaration of the variable.  Record the type. */
      if (variable_ptr->declared_type == NULL) {
        variable_ptr->declared_type = declared_type;
      } else {
        check_assertion(C_mode());
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (is_function_def) {
    /* If a lint-style "argsused" or "varargs" comment appeared, record that in
       the function type.  That will suppress any warnings about unused
       parameters or variable arguments.  Note that this is done before calling
       process_curr_construct_pragmas; otherwise the pragmas we're interested
       in would have been disposed of. */
    record_lint_argsused_and_varargs_state(sym);
  }  /* if */
  /* Do processing required for the rest of the pragmas, if any, that are
     bound to the current declaration. */
  process_curr_construct_pragmas(sym, (a_statement_ptr)NULL);
  if (!is_function && is_volatile_qualified_type(type_ptr)) {
    /* A variable with a volatile type is considered to be used and modified
       from "elsewhere".  Note that this must be done after set_source_corresp
       because the latter clears the IL referenced flag. */
    source_corresp_ptr->referenced = TRUE;
    sym->referenced = TRUE;
    sym->variant.variable.used = TRUE;
    sym->variant.variable.value_has_been_set = TRUE;
  }  /* if */
  /* Return symbol and linkage pointers. */
  *symbol_ptr = sym;
  *linkage_ptr = linkage;

#if DEBUG
  if (debug_level >= 3) {
    db_symbol(sym, "", 4);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* decl_var_or_routine */


void decl_function_template(a_symbol_locator    *locator,
                            a_type_ptr          type_ptr,
                            a_func_info_block   *func_info,
                            a_symbol_ptr        *symbol_ptr,
                            a_storage_class     storage_class)
/*
Roughly speaking, this routine does for function templates what
decl_var_or_routine does for ordinary functions.  Lookup and reuse or else
create a function template symbol; for new symbols also create a routine
entry (though one that is not added to the IL).  *locator represents the
current identifier, type_ptr is the function type, storage_class is the
storage class, if any, specified in the declaration, and is_inline is TRUE
if "inline" was specified in the declaration.  The function template may
be part of an overload set, it may have been previously declared (but not
defined), and it may be an out-of-line definition of a member function of a
class template.
*/
{
  a_scope_depth                     effective_decl_level;
  a_symbol_ptr                      sym = NULL;
  a_symbol_ptr                      overload_symbol = NULL, homonym_symbol;
  a_template_symbol_supplement_ptr  tssp;
  a_routine_ptr                     rout_ptr;
  a_memory_region_number            region_to_switch_back_to;
  a_boolean                         changed_to_inline = FALSE;

  db_enter(3, "decl_function_template");
  if (func_info->is_inline) {
    storage_class = (a_storage_class)sc_static;
  } else if (storage_class == (a_storage_class)sc_unspecified) {
    /* Default. */
    storage_class = (a_storage_class)sc_extern;
  }  /* if */
  effective_decl_level = DEPTH_OF_FILE_SCOPE;
  if (locator->is_qualified_name && locator->specific_symbol != NULL) {
    /* Member function template. */
    sym = locator->specific_symbol;
    if (sym->kind == (a_symbol_kind)sk_projection) {
      /* A member of a base class. */
      pos_error(ec_inherited_member_not_allowed, &locator->source_position);
      sym = NULL;
      set_to_error_locator(*locator);
    } else if (sym->kind != (a_symbol_kind)sk_member_function &&
               sym->kind != (a_symbol_kind)sk_overloaded_function) {
      /* We must have nonfunction class member.  This is an error, so set sym
         to NULL to force the creation of a fake member function symbol. */
      pos_sy_error(ec_not_compatible_with_previous_decl,
                   &locator->source_position, locator->specific_symbol);
      sym = NULL;
      set_to_error_locator(*locator);
    } else {
      /* Look for a member function symbol of this type in the symbol table.
         It is an error if it is  not already there. */
      sym = member_function_redecl_sym(sym, type_ptr);
      if (sym == NULL) {
        /* No member function with a matching type was found.  Issue an
           error. */
        pos_sy_error(locator->specific_symbol->kind ==
                                     (a_symbol_kind)sk_overloaded_function ?
                        ec_overloaded_function_incompatible_type :
                        ec_not_compatible_with_previous_decl,
                   &locator->source_position, locator->specific_symbol);
        set_to_error_locator(*locator);
      } else {
        /* This is a member function symbol of a prototype instantiation.
           Get the associated function template. */
        sym = get_member_function_template_symbol(sym);
      }  /* if */
    }  /* if */
  } else if (!is_error_locator(*locator)) {
    a_symbol_header_ptr  hdr = locator->symbol_header;
    if (hdr->identifier != NULL &&
        (strcmp(hdr->identifier, "main") == 0)) {
      /* A function template named "main" is not allowed.  (This prohibition
         is not explicit in the ARM, but it makes sense, since a function
         named "main" cannot be called (ARM 3.4). */
      pos_error(ec_function_template_named_main, &locator->source_position);
      set_to_error_locator(*locator);
    } else if (locator->is_operator_name) {
      if (locator->variant.opname == (an_opname_kind)onk_delete) {
        /* A template definition of operator delete is not allowed.  This
           is inferred from the ARM prohibition against overloading
           operator delete. */
        pos_error(ec_template_operator_delete, &locator->source_position);
        set_to_error_locator(*locator);
      } else if (is_default_operator_new(locator, type_ptr)) {
        /* Overloading should not be allowed on the single-argument
           version of operator new(size_t), though it is not expressly
           prohibited.  At least one C++ test suite expects an error. */
        pos_error(ec_template_operator_new, &locator->source_position);
        set_to_error_locator(*locator);
      }  /* if */
    }  /* if */
  }  /* if */
  if (curr_token == tok_lbrace ||
      (curr_token == tok_colon && sym != NULL && is_constructor_symbol(sym))) {
    /* This is a defining declaration of the function template. */
    func_info->is_definition = TRUE;
    if (func_info->function_type_from_typedef) {
      /* Just as it is an error when a normal function is defined for the
         function type to come from a typedef, so too is that an error when
         a function template is being defined. */
      a_type_ptr  tp = alloc_type((a_type_kind)tk_routine);
      error(ec_function_type_must_come_from_declarator);
      /* Copy the type entry, since the typedef type may not be shared. */
      copy_routine_type_with_param_types(skip_typerefs(type_ptr), tp);
      type_ptr = tp;
    }  /* if */
  }  /* if */
  if (sym == NULL) {
    /* id_linkage will set sym to point to an existing symbol when we have
       a redeclaration of a function template. */
    (void)id_linkage(locator, &storage_class, type_ptr,
                     /*is_main_function=*/FALSE, &sym, &homonym_symbol,
                     &effective_decl_level);
    if (sym == NULL) {
      /* Not a redeclaration. */
      an_error_code error_code;

      check_default_args(type_ptr);
      if (homonym_symbol != NULL &&
          !overload_distinguishable(homonym_symbol, type_ptr,
                                    /*new_is_template=*/TRUE, &error_code)) {
        /* The previous declaration and the current one are not "overload
           distinguishable" for a reason given by the error code returned. */
        pos_error(error_code, &locator->source_position);
        set_to_error_locator(*locator);
        /* Avoid overloading. */
        homonym_symbol = NULL;
      }  /* if */
      if (homonym_symbol != NULL) {
        /* Another function with the same name has been declared already.  It
           may or may not be a function template.  In any case, create a new
           symbol and add it to an overload list. */
        sym = enter_overloaded_symbol((a_symbol_kind)sk_function_template,
                                      locator, homonym_symbol,
                                      &overload_symbol);
      } else {
        /* No overloading.  Simply create a new symbol. */
        sym = enter_local_symbol((a_symbol_kind)sk_function_template, locator,
                                 DEPTH_OF_FILE_SCOPE,
                                 /*suppress_redecl_error=*/FALSE);
      }  /* if */
    } else {
      check_assertion(sym->kind == (a_symbol_kind)sk_function_template);
      /* Merge type information from the two declarations. */
      reconcile_routine_types(sym->variant.template_info->
						variant.function.routine,
			       type_ptr,
                              /*preserve_rout_type=*/TRUE,
                              /*preserve_type_ptr=*/FALSE);
    }  /* if */
  }  /* if */
  tssp = template_supplement_for_symbol(sym);
  rout_ptr = tssp->variant.function.routine;
  /* A routine entry is created for the function template, but it is not
     entered in the IL.  It is a convenient place to keep track of prototype
     information: type, storage class, etc.  These values may be reused
     when the template is instantiated.  This routine entry will not, of
     course, have a body associated with it. */
  if (rout_ptr == NULL) {
    switch_to_file_scope_region(&region_to_switch_back_to);
    tssp->variant.function.routine = rout_ptr = alloc_routine();
    switch_back_to_original_region(region_to_switch_back_to);
    rout_ptr->type = type_ptr;
    rout_ptr->storage_class = storage_class;
    rout_ptr->is_inline = func_info->is_inline;
    if (locator->is_operator_name) {
      rout_ptr->special_kind = (a_special_function_kind)sfk_operator;
      rout_ptr->opname_kind = locator->variant.opname;
    }  /* if */
    check_assertion(!locator->is_conversion_name);
    set_source_corresp(&rout_ptr->source_corresp, sym);
    rout_ptr->source_corresp.name_linkage =
                          (storage_class == (a_storage_class)sc_extern) ?
                                (a_name_linkage_kind)nlk_cplusplus_external :
                                (a_name_linkage_kind)nlk_internal;
    /* Bind the throw specification to the routine entry's type. */
    add_exception_specification(func_info, rout_ptr);
  } else {
    if (func_info->is_inline) {
      if (!rout_ptr->is_inline) {
        rout_ptr->is_inline = TRUE;
        changed_to_inline = TRUE;
      }  /* if */
    }  /* if */
    /* Be sure the current throw specification is consistent with the one
       on the previous declaration. */
    check_exception_specification(func_info, rout_ptr);
  }  /* if */
  if (overload_symbol != NULL) {
    /* A new symbol was added to an overload list which may have included
       functions that were specific declarations of the current template.
       For instance,
         void f(int i) {  ... }
         template <class T> void f(T t) { ... }
       Here the first declaration of f turns out to be a specific declaration
       of the template named f, even though the template is declared after the
       instance.  We need to go back over the overload list and associate a
       function instantiation entry with each routine that can in retrospect
       be recognized as a specific declaration of the function template. */
    a_symbol_ptr  rout_sym;
    for (rout_sym = overload_symbol->variant.overloaded_function.symbols;
         rout_sym != NULL;
         rout_sym = rout_sym->next) {
      if (rout_sym->kind == (a_symbol_kind)sk_routine) {
        /* Determine whether rout_sym is a specialization of the function
           template represented by sym. */
        record_predeclared_template_function(sym, rout_sym);
      }  /* if */
    }  /* for */
  } else if (changed_to_inline) {
    /* An existing template function has been redeclared and this time it's
       inline.  Be sure that "inline" and storage class are propagated
       through the instances. */
    a_template_instance_ptr  tip = tssp->variant.function.instantiations;
    for (; tip != NULL; tip = tip->next) {
      a_routine_ptr  rp = tip->instance_sym->variant.routine.ptr;
      if (tip->specific_def) {
#if 0
        /* Must the inline setting of a specific definition of a function
           template be consistent with that of the template? */
#endif /* if 0 */
      } else {
        if (rp->storage_class != (a_storage_class)sc_static) {
          sym_warning(ec_template_and_instance_linkage_conflict,
                      tip->instance_sym);
          rp->storage_class = (a_storage_class)sc_static;
          rp->source_corresp.name_linkage = (a_name_linkage_kind)nlk_internal;
        }  /* if */
        /* Issue a diagnostic is the function has already been called. */
        if (!rp->is_inline && rp->called) {
          sym_error(ec_called_function_redeclared_inline,
                    tip->instance_sym);
        }  /* if */
        rp->is_inline = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  /* Return the function template symbol. */
  *symbol_ptr = sym;
#if DEBUG
  if (debug_level >= 3) {
    db_symbol(sym, "", 2);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* decl_function_template */


static void define_static_data_member(a_symbol_locator   *locator,
                                      a_storage_class	 storage_class,
				      a_type_ptr	 type_ptr,
                                      a_source_sequence_entry_ptr  ssep,
				      a_symbol_ptr       *symbol_ptr,
                                      an_id_linkage_kind *linkage_ptr)
/*
Enter the definition of a static data member.  *locator gives the symbol
locator (and thus its name and its declaration position).  storage_class and
type_ptr give the storage class and type of the current definition.
Note that static data members must already have been declared within the
class (or struct or union) of which they are members.  The type must be
compatible with the original declaration, and there must be no explicit
storage class on the current definition.  The storage class of defined
static members should be changed to sc_unspecified.  Return a pointer to
the symbol and its linkage (which is always "none").
*/
{
  a_variable_ptr	var;
  a_boolean		err = FALSE;
  a_symbol_ptr		sym;

  db_enter(3, "define_static_data_member");
  /* This routine is called after a qualified name has been seen, but be sure
     the object is a static data member.  (In invalid programs it could also
     be the name of a nonstatic data member or a member function.) */
  sym = locator->specific_symbol;
  /* A storage class of sc_unspecified means "no storage class explicitly
     specified" -- anything else is an error. */
  if (storage_class != (a_storage_class)sc_unspecified) {
    pos_error(ec_storage_class_not_allowed, &locator->source_position);
  }  /* if */
  if (sym->kind == (a_symbol_kind)sk_static_data_member) {
    var = sym->variant.static_data_member.variable;
    check_assertion(var->storage_class == (a_storage_class)sc_static);
    if (sym->defined) {
      pos_sy_error(ec_already_defined, &locator->source_position, sym);
      err = TRUE;
    } else if (!types_are_compatible(type_ptr, var->type)) {
      pos_sy_error(ec_not_compatible_with_previous_decl,
                   &locator->source_position, sym);
      err = TRUE;
    } else {
#if GENERATE_SOURCE_SEQUENCE_LISTS
      /* Since this is the defining declaration of the static data member,
         record the type.  Note that this has to be done before composite
         type is called -- in case there's some modification. */
      check_assertion(var->declared_type == NULL);
      var->declared_type = type_ptr;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      /* The type of the variable should be the composite of the two types. */
      var->type = composite_type(type_ptr, var->type);
      /* Set the IL referenced flag since, as an externally visible variable,
         it could be referenced from another translation unit. */
      var->source_corresp.referenced = TRUE;
      /* If this is a member of an instantiation of a class
         template, set the specific_def flag in the instance entry. */
      if (sym->variant.static_data_member.instance_ptr != NULL) {
        sym->variant.static_data_member.instance_ptr->specific_def = TRUE;
        sym->variant.static_data_member.variable->specific_def = TRUE;
      }  /* if */
      record_symbol_declaration(SRK_DECLARATION | SRK_DEFINITION, sym,
                                &locator->source_position, ssep);
    }  /* if */
  } else {
    /* Not a static data member (but a member of some sort, since it is a
       qualified name).  Issue the appropriate error. */
    if (sym->kind == (a_symbol_kind)sk_field) {
      /* Nonstatic data members (fields) cannot be defined. */
      pos_error(ec_nonstatic_member_def_not_allowed,
                &locator->source_position);
    } else if (is_member_function_symbol(sym)) {
      /* A member function -- this is treated as a type incompatibility. */
      pos_sy_error(ec_not_compatible_with_previous_decl,
                   &locator->source_position, sym);
    } else if (sym->kind == (a_symbol_kind)sk_projection) {
      /* A member of a base class. */
      pos_error(ec_inherited_member_not_allowed, &locator->source_position);
    } else if (sym->kind != (a_symbol_kind)sk_undefined &&
               !is_error_locator(*locator)) {
      pos_sy_error(ec_already_defined, &locator->source_position, sym);
    }  /* if */
    err = TRUE;
  }  /* if */
  if (err) {
    /* An error occurred which prevents using the object specified as
       target of any initialization that may follow.  Create a dummy
       variable with an error type (to suppress semantic errors on the
       initialization, if any). */
    a_type_ptr           tp = sym->class_of_which_a_member;
    a_symbol_header_ptr  hdr = locator->symbol_header;

    /* Record the symbol declaration, using the original symbol, even
       though there was an error.  This will make it show up on a cross
       reference listing. */
    record_symbol_declaration(SRK_DECLARATION, sym, &locator->source_position,
                              ssep);
    /* "Enter" the symbol using an error locator -- this means a symbol
       entry will be created but it will not be added to any lists.  Then
       we'll restore the header to the new symbol, so that the correct name
       will be available in diagnostics. */
    set_to_error_locator(*locator);
    sym = enter_symbol((a_symbol_kind)sk_static_data_member,
                       locator, DEPTH_OF_FILE_SCOPE,
                       /*suppress_redecl_error=*/TRUE);
    sym->header = hdr;
    sym->variant.static_data_member.variable =
               make_variable(error_type(), (a_storage_class)sc_static,
                             /*at_file_scope=*/TRUE);
    /* Make the error symbol have a class_of_which_a_member field, since
       it is expected on sk_static_data_member fields downstream. */
    sym->class_of_which_a_member = tp;
  }  /* if */
  /* Do processing required for any pragmas that are bound to the current
     declaration. */
  process_curr_construct_pragmas(sym, (a_statement_ptr)NULL);
  *linkage_ptr = idl_none;
  *symbol_ptr = sym;
#if DEBUG
  if (debug_level >= 3) {
    db_symbol(sym, "", 4);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* define_static_data_member */


void decl_typedef(a_symbol_locator             *locator,
                  a_type_ptr                   type_ptr,
                  a_symbol_ptr                 *symbol_ptr,
                  a_source_sequence_entry_ptr  declarator_ssep)
/*
Enter the declaration of an identifier for a typedef.  *locator gives
the symbol locator (and thus its name and its declaration position).
type_ptr gives the type.  Create and enter a symbol entry, and return
a pointer to it in *symbol_ptr.
*/
{
  a_type_ptr    tp;
  a_symbol_ptr  sym = NULL;
  a_boolean     suppress_redecl_error = FALSE;
  a_boolean     saved_referenced_flag;

  db_enter(3, "decl_typedef");
  if ((sym = curr_scope_id_lookup(locator, IDL_NO_OPTIONS)) != NULL) {
    if (sym->kind == (a_symbol_kind)sk_type ||
        (C_dialect == C_dialect_cplusplus && is_type_symbol(sym))) {
      /* Sym is a type name symbol from the current scope.  Issue an error
         if this is an illegal redefinition of the name; otherwise, reuse
         the existing symbol. */
      if (sym->kind == (a_symbol_kind)sk_type) {
        /* A typedef name -- strip off the typerefs until the base type
           is reached or a qualifier is found. */
        tp = skip_typedefs(sym->variant.type);
      } else if (sym->kind == (a_symbol_kind)sk_enum_tag) {
        /* C++ only. */
        tp = sym->variant.type;
      } else {
        /* C++ only -- sk_class_or_struct_tag or sk_union_tag: */
        tp = sym->variant.class_struct_union.type;
      }  /* if */
      if (identical_types(tp, type_ptr) || is_error_type(tp)) {
        /* The current declaration simply redefines the name to the same
           type, which is permitted in C++ (ARM 7.1.3) and warned about for
           ordinary C.  However, in C++ we may still need an sk_type symbol,
           since tags and typedefs do not occupy the same name space. */
        if (sym->kind == (a_symbol_kind)sk_type) {
          if (C_dialect != C_dialect_cplusplus) {
            /* Allowing a benign redeclaration is an extension in C, so issue
               a warning. */
            pos_diagnostic(strict_ansi_error_severity,
                           ec_duplicate_typedef, &locator->source_position);
          }  /* if */
          record_symbol_declaration(SRK_DECLARATION, sym,
                                    &locator->source_position,
                                    declarator_ssep);
#if GENERATE_SOURCE_SEQUENCE_LISTS
          set_src_seq_secondary_decl_type((char *)sym->variant.type, type_ptr);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
          goto return_point;
        } else {
          /* C++ only.  Must be a tag symbol. */
          suppress_redecl_error = TRUE;
        }  /* if */
      } else {
        /* This typedef statement redefines a type name to a different type.
           -- diagnostic is issued in enter_symbol processing. */
      }  /* if */
    } else {
      /* Either the symbol was not declared in the current scope, in which
         case a redefinition here is legal, or else it is not a type name
         symbol, in which case the error message will be issued by
         enter_symbol. */
    }  /* if */
  } else if (C_dialect == C_dialect_cplusplus) {
    /* No symbol by this name.  See if this is a tagless class, struct, or
       union type.  If so, the present name will serve as the tag (ARM 7.1.3).
       Note that we do NOT want to do a skip_typerefs on the type; only if
       *type_ptr itself lacks an associated tag symbol with a name do we want
       to create a new symbol. */
    if (!is_error_type(type_ptr) && !is_error_locator(*locator)) {
      sym = (a_symbol_ptr)type_ptr->source_corresp.assoc_info;
      if (sym != NULL && is_unnamed_class_symbol(sym)) {
        /* An unnamed tag symbol was created for the class and can be reused
           now that we have a name to assign to it.  We need to unlink it from
           the symbol table, give it the name, and relink it into the symbol
           table. */
        relink_unnamed_class_symbol(sym, locator);
        /* Call set_source_corresp, but preserve the current IL referenced
           setting, which set_source_corresp will clear. */
        saved_referenced_flag = type_ptr->source_corresp.referenced;
        set_source_corresp(&(type_ptr->source_corresp), sym);
        type_ptr->source_corresp.referenced = saved_referenced_flag;
        suppress_redecl_error = TRUE;
        /* Note that we do not look for conflicts between the class's new
           name and the names of its members.  This is an area where the
           wording of the ARM (7.1.3) has been clarified and/or amended by
           the X3J16 working paper, and so the restrictions specified in
           ARM 9.2 do not apply. */
      }  /* if */
    }  /* if */
  }  /* if */
  /* Call enter symbol to create a new symbol for this type.  It will
     also issue an error if the name is already declared in the current
     scope. */
  sym = enter_local_symbol((a_symbol_kind)sk_type, locator,
                           decl_scope_level, suppress_redecl_error);
  /* Create a new type entry and add it to the types list for the current
     scope. */
  sym->variant.type = tp = alloc_type((a_type_kind)tk_typeref);
  tp->variant.typeref.type = type_ptr;
  set_source_corresp(&(tp->source_corresp), sym);
  record_symbol_declaration(SRK_DECLARATION | SRK_DEFINITION, sym,
                            &locator->source_position, declarator_ssep);
  add_to_types_list(tp, decl_scope_level);

return_point:
  /* Do processing required for any pragmas that are bound to the current
     declaration. */
  process_curr_construct_pragmas(sym, (a_statement_ptr)NULL);
  /* Return the type name symbol to the caller. */
  *symbol_ptr = sym;
#if DEBUG
  if (debug_level >= 3) {
    db_symbol(*symbol_ptr, "", 4);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* decl_typedef */


void decl_default_function(a_symbol_ptr symbol_ptr)
/*
Declare the given symbol as a default function.  This routine is called
when a previously-unknown identifier appears in an expression followed by
a left parenthesis, indicating that it is an undeclared function.  The
symbol has already been entered as an undefined symbol.
*/
{
  an_id_linkage_kind     linkage;
  a_type_ptr             rout_type, old_type;
  a_symbol_ptr           ext_sym;
  a_symbol_locator       locator;
  a_memory_region_number region_to_switch_back_to;
  a_func_info_block      func_info;

  db_enter(4, "decl_default_function");
  /* Change the symbol kind to routine.  Note that the symbol has already
     been entered.  Fortunately, "undefined" and "routine" are in the
     same name space, so that proper checking for a duplicate definition
     will have already been done (there's a check in symbol_tbl_init
     that the name spaces are the same). */
  set_symbol_kind(symbol_ptr, (a_symbol_kind)sk_routine);
  /* In pcc mode, all routines are entered at file scope level.  Remove
     and re-enter the symbol (if necessary) so it will be there. */
  if (C_dialect == C_dialect_pcc) {
    if (symbol_ptr->decl_scope != FILE_SCOPE_NUMBER) {
      /* Take the symbol out of the symbol table. */
      remove_symbol(symbol_ptr);
      /* Put the symbol back into the symbol table at the file scope level. */
      reenter_symbol(symbol_ptr, DEPTH_OF_FILE_SCOPE, /*suppress_error=*/TRUE);
    }  /* if */
  }  /* if */
  /* All IL routines and their types must be at the file scope level, so switch
     to that memory region if necessary. */
  switch_to_file_scope_region(&region_to_switch_back_to);
  /* Generate the function type, with an old-style no-information parameter
     list, and in C a return type of "int".  See 3.3.2.2, semantics. In C++
     this must be an error, so give it a return type of tk_error. */
  rout_type = alloc_type((a_type_kind)tk_routine);
  rout_type->variant.routine.extra_info->param_type_list = NULL;
  rout_type->variant.routine.extra_info->prototyped = FALSE;
  if (C_dialect != C_dialect_cplusplus) {
    rout_type->variant.routine.return_type =
                                       integer_type((an_integer_kind)ik_int);
  } else {
    /* Making the return type an error type prevents cascading errors. */
    rout_type->variant.routine.return_type = error_type();
  }  /* if */
  make_locator_for_symbol(symbol_ptr, &locator);
  /* Declare the function identifier. */
  clear_func_info(&func_info);
  func_info.is_implicit_declaration = TRUE;
  if (exceptions_enabled) func_info.throw_position = locator.source_position;
  decl_var_or_routine(&locator, (a_storage_class)sc_extern, rout_type,
                      &func_info, (a_source_sequence_entry_ptr)NULL,
                      (SRK_DECLARATION | SRK_IMPLICIT),
                      &symbol_ptr, &linkage, &old_type, &ext_sym);
  done_with_func_info(func_info);
  /* Set the referenced flag on the routine entry.  The implicit declaration
     is also an immediate reference. */
  symbol_ptr->variant.routine.ptr->source_corresp.referenced = TRUE;
  switch_back_to_original_region(region_to_switch_back_to);
#if DEBUG
  if (debug_level >= 3) {
    db_symbol(symbol_ptr, "", 4);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* decl_default_function */


a_label_ptr scan_label(a_boolean is_definition)
/*
Scan a label as part of a statement label or goto statement.  Return a
pointer to the IL label.  The current token should be a label identifier.
is_definition is TRUE if the label is being scanned as part of a label.
*/
{
  a_symbol_ptr      label_sym;
  a_label_ptr       label;

  a_source_position start_pos;

  db_enter(3, "scan_label");

  copy_source_position(pos_curr_token, start_pos);
  if (curr_token != tok_identifier) {
    (void)required_token(tok_identifier, ec_exp_identifier);
    set_to_error_locator(locator_for_curr_id);
    label_sym = NULL;
  } else {
    /* See if the label identifier is already in the symbol table. */
    label_sym = symbol_list_from_locator(locator_for_curr_id);
    get_symbol_of_kind((a_symbol_kind)sk_label, label_sym);
    /* If the label is not from the current function, pretend it was
       not found.  This comes up in functions within local classes:
         void f() {
           label1:;
           class A {
             void g() { goto label1; }
           };
         }
    */
    if (label_sym != NULL &&
        label_sym->decl_scope !=
            scope_stack[depth_innermost_function_scope].il_scope->number) {
      /* This label is from an outer routine.  Pretend it's not found. */
      label_sym = NULL;
    }  /* if */  
  }  /* if */
  if (label_sym == NULL) {
    /* Enter the label identifier into the symbol table.  This is done
       at the function level even if we are inside some blocks.  Use
       a locator with an undefined source position; the decl_position will
       be handled explicitly shortly. */
    locator_for_curr_id.source_position = null_source_position;
    label_sym = enter_symbol((a_symbol_kind)sk_label, &locator_for_curr_id,
                             depth_innermost_function_scope,
                             /*suppress_error=*/TRUE);
    /* Allocate the IL label and attach it to the symbol. */
    label_sym->variant.label.ptr = label = alloc_label();
    add_to_labels_list(label);
    set_source_corresp(&label->source_corresp, label_sym);
    /* The exec_stmt field stays NULL to indicate that the declaration
       has not been (fully?) processed yet. */
  }  /* if */
  if (!is_error_locator(locator_for_curr_id)) {
    /* Record the right kind of reference to the label symbol. */
    if (is_definition) {
      /* Note that we want mark_defined is called even if the symbol
         was previously entered.  Labels are strange in that a reference
         can come up before a declaration. */
      mark_defined(label_sym, &pos_curr_token);
    } else {
      mark_referenced(label_sym, &pos_curr_token);
      /* Set the decl_position in case no declaration shows up, so we
         have the location of the use. */
      if (label_sym->decl_position.seq == 0 &&
          label_sym->decl_position.column == SP_COL_UNKNOWN) {
        copy_source_position(pos_curr_token, label_sym->decl_position);
      }  /* if */
    }  /* if */
    /* Advance past the identifier. */
    (void)get_token();
  }  /* if */

  copy_source_position(start_pos, error_position);
#if DEBUG
  if (debug_level >= 3) {
    db_symbol(label_sym, "", 4);
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return(label_sym->variant.label.ptr);
}  /* scan_label */


a_boolean simplify_curr_class_qualified_name(void)
/*
If the current token is the start of a qualified name in which the class
name component is the name of a class currently being defined, advance past
the class name and the "::" so that the current token is a non-qualified
name.  Return TRUE if such a modification is done and FALSE otherwise.
This routine is called in C++ only.

This functionality is provided to deal with declarations of class members
where a qualified name is used instead of a simple name, e.g., when a
constructor for class A is declared A::A() rather than A().  The ARM does
not specifically allow this syntax, but it is supported by cfront.
*/
{
  a_boolean                is_member_id = FALSE;
  a_scope_stack_entry_ptr  ssep = &scope_stack[decl_scope_level];

  db_enter(3, "simplify_curr_class_qualified_name");

  if (ssep->kind == (a_scope_kind)sck_class_struct_union &&
      is_generalized_identifier_start(GID_TEMPLATE_ARGS_OPTIONAL) &&
      locator_for_curr_id.is_qualified_name) {
    if (locator_for_curr_id.qualifier_class_type == ssep->assoc_type &&
        locator_for_curr_id.is_global_qualified_name == FALSE) {
      is_member_id = TRUE;
      /* Issue any access errors encountered while scanning the
         qualifier -- even though there shouldn't be any for this
         case. */
      issue_qualifier_access_errors(&locator_for_curr_id.access_errors);
      /* Reset the fields in the locator to make it appear as if the
         qualifier was not present. */
      locator_for_curr_id.is_qualified_name = FALSE;
      locator_for_curr_id.is_file_scope_qualified_name = FALSE;
      locator_for_curr_id.is_global_qualified_name = FALSE;
      locator_for_curr_id.qualifier_class_type = NULL;
      /* Accepting qualified member names is an extension so issue a
         diagnostic in strict ANSI mode. */
      if (strict_ansi_mode) {
        diagnostic(strict_ansi_error_severity,
                   ec_qualifier_in_member_declaration);
      }  /* if */ 
    }  /* if */
  }  /* if */
  db_exit();
  return is_member_id;
}  /* simplify_curr_class_qualified_name */


void type_name(a_type_ptr *type_ptr)
/*
Scan a type-name (see 3.5.5) and return a pointer to the type.  The syntax is:

3.5.5  type-name:
		specifier-qualifier-list abstract-declarator
							    opt
*/
{
  a_storage_class              storage_class;
  a_decl_flag_set              dso_flags, do_flags;
  a_type_ptr                   bottom_derived_type;
  a_source_position            start_pos;
  a_source_sequence_entry_ptr  declarator_ssep = NULL;

  db_enter(3, "type_name");
  set_err_pos_to_curr_token();
  copy_source_position(pos_curr_token, start_pos);
  (void)decl_specifiers(DSI_TYPE_SPECIFIER_ALLOWED, &dso_flags,
			&storage_class, type_ptr);
  if (C_dialect == C_dialect_cplusplus &&
      (dso_flags & DSO_DEFINES_SOMETHING)) {
    /* Definition of a class, struct, union, or enum type is not allowed. */
    pos_error(ec_type_definition_not_allowed, &start_pos);
  } else if (!(dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER)) {
    /* Missing type specifier. */
    warning(ec_missing_type_specifier);
  }  /* if */
  if (*type_ptr != NULL) {
    (skip_typerefs(*type_ptr))->source_corresp.referenced = TRUE;
  }  /* if */
  /* Note -- the check for dangling_type_specifier is not relevant here. */
  if (is_abstract_declarator_start()) {
    declarator(DI_ABSTRACT_DECLARATOR_ALLOWED | DI_QUALIFIED_NAME_ALLOWED,
               &do_flags, *type_ptr, /*member_parent_type=*/(a_type_ptr)NULL,
	       (a_symbol_locator *)NULL,
               type_ptr, &bottom_derived_type, &declarator_ssep,
               (a_func_info_block_ptr)NULL);
  }  /* if */
  copy_source_position(start_pos, error_position);
  db_exit();
}  /* type_name */


void new_type_name(a_boolean         is_parenthesized,
                   a_type_ptr        *type_ptr)
/*
Scan a C++ new-type-name or a parenthesized type-name that may appear in a
"new" expression (ARM 5.3.3), and return a pointer to the type.  The
syntax is:
   new-type-name:
              type-specifier-list new-declarator
                                                opt
   new-declarator:
              * cv-qualifier-list    new-declarator
                                 opt               opt
              class-name :: * cv-qualifier-list    new-declarator
                                               opt               opt
              new-declarator    [ expression ]
                            opt

   type-name:
              type-specifier-list abstract-declarator
                                                     opt
*/
{
  a_type_ptr            complete_type, new_type_ptr;
  a_type_ptr            derived_type, bottom_derived_type = NULL;
  a_decl_flag_set       dso_flags, do_flags;
  a_source_position     start_pos;
  a_storage_class       storage_class;
  a_source_sequence_entry_ptr
                        declarator_ssep = NULL;

  db_enter(3, "new_type_name");
  if (!is_parenthesized && curr_token == tok_lparen) {
    is_parenthesized = TRUE;
    (void)get_token();
  }  /* if */
  if (is_parenthesized) add_stop_token(tok_rparen);
  set_err_pos_to_curr_token();
  copy_source_position(pos_curr_token, start_pos);
  (void)decl_specifiers(DSI_TYPE_SPECIFIER_ALLOWED | DSI_IS_NEW_TYPE_NAME,
                        &dso_flags, &storage_class, type_ptr);
  if (dso_flags & DSO_DEFINES_SOMETHING) {
    /* Definition of a class, struct, union, or enum type is not allowed. */
    pos_error(ec_type_definition_not_allowed, &start_pos);
  } else if (!is_parenthesized && ((dso_flags & DSO_CONST_QUALIFIED) ||
                                   (dso_flags & DSO_VOLATILE_QUALIFIED))) {
    /* WP 5.3.4 states that the unparenthesized syntax (new-type-id) may
       not include "const" or "volatile".  (This doesn't seem right, since
       a qualified type can still be created with a typedef.  But in strict
       mode we issue the diagnostic anyway.) */
    if (strict_ansi_mode) {
      pos_diagnostic(strict_ansi_error_severity, ec_const_volatile_not_allowed,
                     &start_pos);
    }  /* if */
  } else if (!(dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER)) {
    /* Missing type specifier. */
    warning(ec_missing_type_specifier);
  }  /* if */
  if (*type_ptr != NULL) {
    (skip_typerefs(*type_ptr))->source_corresp.referenced = TRUE;
  }  /* if */
  /* Note -- the check for dangling_type_specifier is not relevant here. */
  bottom_derived_type = NULL;
  if (is_parenthesized) {
    if (is_abstract_declarator_start()) {
      declarator(DI_ABSTRACT_DECLARATOR_ALLOWED |
                    DI_QUALIFIED_NAME_ALLOWED |
                    DI_DIMENSION_EXPRESSION_ALLOWED,
                 &do_flags, *type_ptr,
                 /*member_parent_type=*/(a_type_ptr)NULL,
                 (a_symbol_locator *)NULL, type_ptr,
                 &bottom_derived_type, &declarator_ssep,
                 (a_func_info_block_ptr)NULL);
    }  /* if */
    (void)required_token(tok_rparen, ec_exp_rparen);
    remove_stop_token(tok_rparen);
  } else {
    complete_type = pointer_declarator(*type_ptr, &bottom_derived_type,
                                       /*reference_allowed=*/FALSE);

    derived_type = NULL;
    bottom_derived_type = NULL;
    add_stop_token(tok_lbracket);
    if (curr_token == tok_lbracket) {
      array_declarator(&new_type_ptr, /*nonconstant_allowed=*/TRUE);
      add_to_derived_type_list(new_type_ptr,
                               &derived_type, &bottom_derived_type);
      while (curr_token == tok_lbracket) {
        array_declarator(&new_type_ptr, /*nonconstant_allowed=*/FALSE);
        /* Add the new type to the bottom of the existing derived type list.
           Note that this involves error checking. */
        add_to_derived_type_list(new_type_ptr,
                                 &derived_type, &bottom_derived_type);
      }  /* while */
      if (derived_type != NULL) {
        if (complete_type != NULL) {
          if (!is_error_type(bottom_derived_type)) {
            /* Combine derived_type and complete_type. */
            add_to_derived_type_list(complete_type,
                                     &derived_type, &bottom_derived_type);
          }  /* if */
        }  /* if */
        complete_type = derived_type;
      }  /* if */
    }  /* if */
    remove_stop_token(tok_lbracket);
    *type_ptr = complete_type;
  }  /* if */
  db_exit();
}  /* new_type_name */


a_boolean scan_conversion_operator(a_source_position  *id_pos,
				   a_type_ptr	      class_type)
/*
The token "operator" has been seen and passed; we are now on the token
immediately following it.  If it marks the start of a type name we have
an identifier for a conversion operator -- scan the type name, update the
locator, and return TRUE.  If it doesn't, return FALSE.
If class_type is not NULL then push a class reactivation scope before
scanning type name in a type conversion operator.
*/
{
  a_storage_class           storage_class;
  a_decl_flag_set           dso_flags;
  a_type_ptr                specifiers_type, complete_type;
  a_type_ptr                bottom_derived_type = NULL;
  a_source_position         type_pos;
  a_boolean                 is_conversion_operator;
  a_boolean		    class_reactivated = FALSE;

  db_enter(3, "scan_conversion_operator");
  /* Push a class reactivation scope if class_type is not NULL.  This is
     used when scanning conversion operators such as "A::operator B" where
     B needs to be looked up within A.  This is not needed for overloaded
     operator routines, but we don't know what kind of operator we are
     scanning until we call is_type_start, and the class needs to
     be reactivated before is_type_start is called. */
  if (class_type != NULL && !is_incomplete_type(class_type)) {
    a_symbol_ptr	sym;
    sym = (a_symbol_ptr)class_type->source_corresp.assoc_info;
    /* In valid usage, the class type will always be either a complete
       real class type or a prototype instantiation.  In other cases,
       suppress the reactivation because incomplete and nonreal classes
       cannot be reactivated.  An error will be issued elsewhere for these
       cases. */
    if (sym != NULL && 
        (is_real_class_symbol(sym) ||
         is_prototype_instantiation_symbol(sym))) {
      push_class_reactivation_scope(class_type);
      class_reactivated = TRUE;
    }  /* if */
  }  /* if */
  if (is_type_start()) {
    /* It is the start of a type name. */
    is_conversion_operator = TRUE;
    set_err_pos_to_curr_token();
    copy_source_position(pos_curr_token, type_pos);
    (void)decl_specifiers(DSI_TYPE_SPECIFIER_ALLOWED, &dso_flags,
                          &storage_class, &specifiers_type);
    if (C_dialect == C_dialect_cplusplus &&
        (dso_flags & DSO_DEFINES_SOMETHING)) {
      /* Definition of a class, struct, union, or enum type is not allowed. */
      pos_error(ec_type_definition_not_allowed, &type_pos);
    } else if (!(dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER)) {
      /* Missing type specifier. */
      warning(ec_missing_type_specifier);
    }  /* if */
    complete_type = pointer_declarator(specifiers_type, &bottom_derived_type,
                                       /*reference_allowed=*/TRUE);
    unget_token();
    curr_token = tok_identifier;
    pos_curr_token = error_position = *id_pos;
    make_type_conversion_locator(complete_type, &locator_for_curr_id, id_pos);
  } else {
    is_conversion_operator = FALSE;
  }  /* if */
  /* Pop the class reactivation scope if one was pushed earlier. */
  if (class_reactivated) pop_class_reactivation_scope();
  db_exit();
  return is_conversion_operator;
}  /* scan_conversion_operator */


a_type_ptr type_keyword(void)
/*
If the current token is a type keyword (e.g., int, long); return the type
indicated by the keyword, otherwise, return NULL.  This is used in scanning
a simple-type-name for C++ functional-notation casts.  The current token is
not advanced.  Note that this routine does not deal with identifiers that
are defined as types, only keywords; it also does not accept multi-token
types, e.g., "unsigned int".  See ARM 7.1.6 and 5.2.3.
*/
{
  a_type_ptr type;

  switch (curr_token) {
    case tok_char:
      type = integer_type((an_integer_kind)ik_char);
      break;
    case tok_short:
      type = integer_type((an_integer_kind)ik_short);
      break;
    case tok_int:
    case tok_signed:
      type = integer_type((an_integer_kind)ik_int);
      break;
    case tok_long:
      type = integer_type((an_integer_kind)ik_long);
      break;
    case tok_unsigned:
      type = integer_type((an_integer_kind)ik_unsigned_int);
      break;
    case tok_float:
      type = float_type((a_float_kind)fk_float);
      break;
    case tok_double:
      type = float_type((a_float_kind)fk_double);
      break;
    case tok_void:
      type = void_type();
      break;
    default:
      type = NULL;
      break;
  }  /* switch */
  return type;
}  /* type_keyword */


void record_lint_argsused_and_varargs_state(a_symbol_ptr  rout_sym)
/*
Set fields in the routine type to reflect the current argsused and varargs
state, as indicated by a comment immediately preceding the current function
definition.
*/
{
  a_pending_pragma_ptr           ppp;
  a_routine_type_supplement_ptr  rtsp = NULL;
  
  rtsp = rout_sym->variant.routine.ptr->type->variant.routine.extra_info;
  /* Determine whether a lint argsused comment immediately preceded this
     function definition. */
  ppp = extract_specific_pragmas((a_pragma_kind)pk_lint_argsused, rout_sym,
                                 (a_statement_ptr)NULL,
                                 /*curr_scope_only=*/FALSE);
  if (ppp != NULL) {
    /* There is a currently active argsused comment. */
    rtsp->lint_argsused_flag = TRUE;
    /* The pending-pragma entry has been unlinked from the scope stack entry
       list, but it still must be returned to the available list. */
    free_pending_pragma_list(ppp);
  }  /* if */
  if (!rtsp->prototyped) {
    /* Determine whether a lint varargs count comment immediately preceded this
       function definition. */
    ppp = extract_specific_pragmas((a_pragma_kind)pk_lint_varargs_count,
                                   rout_sym, (a_statement_ptr)NULL,
                                   /*curr_scope_only=*/FALSE);
    if (ppp != NULL) {
      /* There is a currently active varargs comment. */
      rtsp->lint_varargs_count = ppp->variant.lint_varargs_count;
      /* The pending-pragma entry has been unlinked from the scope stack entry
         list, but it still must be returned to the available list. */
      free_pending_pragma_list(ppp);
    }  /* if */
  }  /* if */
}  /* record_lint_argsused_and_varargs_state */


/* ARGSUSED */ /* sp is required for pragma processing functions of type
                  a_next_construct_pragma_function. */
void record_arg_pragma(a_pending_pragma_ptr  ppp,
                       a_symbol_ptr          sym,
                       a_statement_ptr       sp)
/*
A pragma indicating special argument checking (e.g., for printf args) has
been specified immediately before the current declaration.  If the current
declaration declares a function, remember the pragma kind in the function
type, so that it can be referenced during argument processing.
*/
{
  if (sym->kind == (a_symbol_kind)sk_routine ||
      sym->kind == (a_symbol_kind)sk_member_function) {
    sym->variant.routine.ptr->type->
           variant.routine.extra_info->arg_pragma = ppp->descr_ptr->kind;
  } else {
    /* Diagnostic? */
  }  /* if */
}  /* record_arg_pragma */


#if C_ANACHRONISMS_ALLOWED

static a_boolean is_initializer_start(void)
/*
Return TRUE if the current token appears to be the start of an initializer.
This is used in pcc mode to decide whether or not an old-style initializer
is present when a "=" is not there.
*/
{
  a_boolean    is_init_start = FALSE;
  a_symbol_ptr assoc_symbol;

  if (curr_token == tok_semicolon ||
      curr_token == tok_comma     ||
      curr_token == tok_rbrace    ||
      is_decl_start(/*expr_context=*/FALSE,
                    /*real_declarator_allowed=*/TRUE)) {
    /* No initializer present. */
  } else if (curr_token == tok_identifier) {
    /* Identifier -- only consider as start of an initializer if defined
       as something that might be part of an expression. */
    assoc_symbol = normal_id_lookup(&locator_for_curr_id, IDL_NO_OPTIONS);
    if (assoc_symbol != NULL) {
      if (assoc_symbol->kind == (a_symbol_kind)sk_constant ||
          assoc_symbol->kind == (a_symbol_kind)sk_routine  ||
          assoc_symbol->kind == (a_symbol_kind)sk_variable) {
        /* This identifier is defined as an enumeration constant, a routine,
           or a variable, so it might be an initializer.  Note that a variable
           or routine is allowed in that it might get implicitly converted
           to a pointer to that entity. */
        is_init_start = TRUE;
      }  /* if */
    }  /* if */
  } else {
    /* Other tokens (for example, "("); assume this is an initializer. */
    is_init_start = TRUE;
  }  /* if */
  return is_init_start;
}  /* is_initializer_start */

#endif /* C_ANACHRONISMS_ALLOWED */

static void linkage_specification(a_boolean      function_definition_allowed,
                                  a_boolean      is_old_style_param_decl,
                                  a_param_id_ptr param_id_list)
/*
The caller has determined that we are at the start of a C++ linkage
specification -- that is, the current token is "extern" and it is followed
by a string literal.  The syntax (from ARM 7.4) is:

  linkage-specification:
      extern string-literal { declaration-list    }
                                              opt
      extern string-literal declaration

Since linkage specifications nest, the current linkage specifier is saved
in in a local variable, the new one is established by updating a global
variable, the declaration(s) are processed, and then the original linkage
specifier is restored.
*/
{
  an_extern_linkage  saved_linkage;
  char               *str;
  a_boolean          err = FALSE;

  db_enter(3, "linkage_specification");
  if (decl_scope_level != DEPTH_OF_FILE_SCOPE) {
    error(ec_linkage_specifier_not_allowed);
    err = TRUE;
  }  /* if */
  /* Advance to the string literal. */
  (void)get_token();
  str = const_for_curr_token.variant.string.value;
  /* ARM 7.4 specifies that the strings "C" and "C++" must be supported,
     but that implementations are permitted to add others, such as "Ada"
     or "FORTRAN".  If changes are made here to support other strings, be
     sure to update the name linkage kind enumeration. */
  /* Save the current default linkage. */
  saved_linkage = def_external_linkage;
  if (strcmp(str, "C") == 0) {
    if (!err) {
      def_external_linkage.kind = (a_name_linkage_kind)nlk_external;
      def_external_linkage.is_explicit = TRUE;
    }  /* if */
  } else if (strcmp(str, "C++") == 0) {
    if (!err) {
      def_external_linkage.kind =
			     (a_name_linkage_kind)nlk_cplusplus_external;
      def_external_linkage.is_explicit = TRUE;
    }  /* if */
  } else {
    /* Leave def_external_linkage unmodified. */
    error(ec_bad_linkage_specifier);
  }  /* if */
  (void)get_token();
  /* If a brace enclosed declaration list follows, call declaration
     repeatedly.  If no brace follows, call declaration just once to pick
     up the rest of the current declaration. */
  if (curr_token == tok_lbrace) {
    /* Issue diagnostics on pragmas that are trying to bind to the
       extern "C" (or whatever) construct. */
    cannot_bind_to_curr_construct();
    /* Advance past the left brace. */
    (void)get_token();
    add_stop_token(tok_rbrace);
    /* Go through the declarations. */
    while (curr_token != tok_rbrace && curr_token != tok_end_of_source) {
      declaration(function_definition_allowed, /*extern_implied=*/FALSE,
                  is_old_style_param_decl, param_id_list);
    }  /* while */
    remove_stop_token(tok_rbrace);
    (void)required_token(tok_rbrace, ec_exp_rbrace);
  } else {
    /* Just one declaration is governed by this linkage specifier.  If no
       storage class is specified it is as though "extern" were specified --
       this is an interpretation of the sentence in ARM 7.4 asserting, "An
       object defined withing an `extern "C" {...}' construct is still defined
       and not just declared," and of the example following it, where without
       the braces the variable is not defined. */
    declaration(function_definition_allowed, /*extern_implied=*/TRUE,
                is_old_style_param_decl, param_id_list);
  }  /* if */
  /* Restore the default linkage to the value it had before the declaration
     (or declaration list) was processed. */
  def_external_linkage = saved_linkage;

  db_exit();
}  /* linkage_specification */


void handler_declaration(a_statement_ptr     try_block_stmt,
                         a_source_position*  catch_pos)
/*
Process a handler declaration:

  "catch" "(" exception-declaration ")" compound-statement

try_block_stmt is a pointer to the try-block statement to which the catch
clause is to be attached.  catch_pos is the source position of "catch".
*/
{
  a_handler_ptr      handler, prev_handler;
  a_type_ptr         type_ptr = NULL, bottom_derived_type;
  a_storage_class    storage_class;
  a_decl_flag_set    dso_flags, do_flags;
  a_symbol_ptr       sym;
  a_symbol_locator   locator;
  a_source_position  decl_pos;
  a_routine_ptr      cctor, dtor;
  a_param_type_ptr   ptp;
  a_dynamic_init_ptr dip;
  a_source_sequence_entry_ptr
                     declarator_ssep = NULL;

  db_enter(3, "handler_declaration");
  /* Push the scope for the handler before processing the exception
     declaration to assure that the scope of the handler's parameter is the
     same as that of the handler's compound statement block. */
  (void)push_scope((a_scope_kind)sck_block, NO_SCOPE_NUMBER,
                   (a_type_ptr)NULL, (a_routine_ptr)NULL,
                   (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                   (a_template_arg_ptr)NULL);
  /* Allocate the handler. */
  handler = alloc_handler();
  /* Set the assoc_handler field of the IL scope entry. */
  set_block_scope_handler(handler);
  set_stmt_source_position(handler->catch_position, *catch_pos);
  if (required_token(tok_lparen, ec_exp_lparen)) {
    decl_pos = pos_curr_token;
    if (curr_token == tok_ellipsis) {
      /* NULL parameter. */
      (void)get_token();
    } else {
      if (curr_token != tok_identifier &&
          !is_decl_start(/*expr_context=*/FALSE,
                         /*real_declarator_allowed=*/TRUE)) {
        add_stop_token(tok_rparen);
        syntax_error(ec_missing_exception_declaration);
        type_ptr = error_type();
        set_to_error_locator(locator);
        remove_stop_token(tok_rparen);
      } else {
        (void)decl_specifiers((DSI_TYPE_SPECIFIER_ALLOWED |
                               DSI_EMPTY_DECL_SPECIFIERS_ALLOWED),
                              &dso_flags, &storage_class, &type_ptr);
        if (dso_flags & DSO_DEFINES_SOMETHING) {
          /* Definition of a class, struct, union, or enum type is not
             allowed. */
          pos_error(ec_type_definition_not_allowed, &decl_pos);
        } else if (dso_flags & DSO_NO_DECL_SPECIFIERS) {
          /* Missing type specifier. */
          pos_error(ec_missing_exception_declaration, &decl_pos);
          type_ptr = error_type();
        } else if (!(dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER)) {
          /* Implicit int. */
          pos_warning(ec_missing_type_specifier, &decl_pos);
        }  /* if */
        sym = NULL;
        if (is_abstract_or_real_declarator_start()) {
          declarator(DI_REAL_DECLARATOR_ALLOWED |
                       DI_ABSTRACT_DECLARATOR_ALLOWED,
                     &do_flags, type_ptr,
                     /*member_parent_type=*/(a_type_ptr)NULL, &locator,
                     &type_ptr, &bottom_derived_type, &declarator_ssep,
                     (a_func_info_block_ptr)NULL);
          if (do_flags & DO_REAL_DECLARATOR_SCANNED) {
            sym = enter_symbol((a_symbol_kind)sk_variable, &locator,
                               decl_scope_level,
                               /*suppress_redecl_error=*/FALSE);
          }  /* if */
        }  /* if */
        if (!exceptions_enabled) {
          /* Don't bother with the semantic checks on the handler type.  Set
             type to error type to avoid inappropriate errors downstream. */
          type_ptr = error_type();
        } else if (!is_error_type(type_ptr)) {
          /* Force instantiation of template class. */
          check_for_uninstantiated_template_class(type_ptr);
          /* Adjust the type if necessary (for example, "array of x"
             becomes "pointer to x"). */
          adjust_parameter_type(&type_ptr);
          if (is_incomplete_type(type_ptr)) {
            /* Incomplete type is not allowed. */
            pos_error(ec_incomplete_type_not_allowed, &decl_pos);
            type_ptr = error_type();
          } else {
            /* Mark the type as having been used in an exception.  (Also,
               if it "contains" any classes, they are marked as requiring
               external linkage.) */
            set_used_in_exception_flag(type_ptr);
            if (is_or_contains_local_type(type_ptr)) {
              /* Exception types, if they have linkage at all, must have
                 external linkage; however, it is possible to write a useful
                 program in which a local type is thrown and caught -- e.g.,
                   void f() {
                     class A { ... };
                     try { ... throw A ... }
                     catch (A) { ... }
                   }
                 We still issue a diagnostic, since there are other cases
                 (not necessarily detectable by the compiler) in which local
                 types would be problematic. */
              pos_remark(ec_local_type_used_in_exception, &decl_pos);
            }  /* if */
          }  /* if */
        }  /* if */
        /* Create a variable for the handler parameter, even if there's no
           explicit name. */
        handler->parameter = make_handler_parameter(type_ptr);
        /* Update the symbol, if there is one. */
        if (sym != NULL) {
          sym->variant.variable.ptr = handler->parameter;
          set_source_corresp(&(handler->parameter->source_corresp), sym);
          record_symbol_declaration(SRK_DECLARATION | SRK_DEFINITION, sym,
                                    &sym->decl_position, declarator_ssep);
#if GENERATE_SOURCE_SEQUENCE_LISTS
          sym->variant.variable.ptr->declared_type = type_ptr;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
          mark_variable_value_set(sym);
        }  /* if */
        /* A handler parameter is initialized by the run-time when the
           handler is invoked.  Create the dynamic init entry to represent
           the initialization. */
        if (is_class_struct_union_type(type_ptr)) {
          /* Classes may require the use of a copy constructor. */
          a_boolean          bitwise_copy;
          a_source_position  pos;

          if (sym != NULL) {
            pos = sym->decl_position;
          } else {
            pos = pos_curr_token;
          }  /* if */
          cctor = select_copy_constructor(type_ptr,
                                          /*const_object_required=*/FALSE,
                                          /*volatile_object_okay=*/FALSE,
                                          &pos, type_ptr, &bitwise_copy,
                                          /*evaluated=*/TRUE,
                                          /*suppress_access_check=*/TRUE);
          check_assertion((cctor == NULL) == bitwise_copy); 
          dtor = select_destructor(type_ptr, type_ptr, &pos,
                                   /*honor_virtual=*/FALSE, /*evaluated=*/TRUE,
                                   /*suppress_access_check=*/TRUE);
        } else {
          /* Non classes require only bitwise copying. */
          cctor = dtor = NULL;
        }  /* if */
        if (cctor != NULL) {
          /* A copy constructor was located.  Create a dynamic-init entry
             to point to it. */
          ptp = (skip_typerefs(cctor->type))->
                                   variant.routine.extra_info->param_type_list;

          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constructor);
          dip->variant.constructor.ptr = cctor;
          /* We need to copy the default arg expressions of the second and
             subsequent parameters (if any) of the copy constructor.  The
             first param is ignored even if it is declared to have a default
             arg. */
          ptp = ptp->next;
          dip->variant.constructor.args = copy_default_arg_expr_list(ptp);
          /* Only at runtime is the source known. */
          dip->variant.constructor.
                             is_copy_constructor_with_implied_source = TRUE;
        } else {
          /* A bitwise copy is all that is required. */
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_bitwise_copy);
        }  /* if */
        dip->variable = handler->parameter;
        dip->destructor = dtor;
        handler->dynamic_init = dip;
      }  /* if */
    }  /* if */
    prev_handler = try_block_stmt->variant.try_block.handlers;
    if (prev_handler == NULL) {
      /* This is the first handler declared for this try block. */
      try_block_stmt->variant.try_block.handlers = handler;
    } else {
      a_boolean  masked = FALSE;
      /* Make a pass over the previously declared handlers in this try block
         to do error checking and locate the end of the list, where the new
         handler will be added. */
      for (;;) {
        if (!exceptions_enabled) {
          /* Don't bother with semantic checks. */
        } else if (masked) {
          /* One "masking" diagnostic has already been issued -- there's no
             point in putting out another. */
        } else if (type_ptr == error_type()) {
          /* No need to check for masking in this case. */
        } else if (prev_handler->parameter == NULL) {
          /* Anything following a default handler is masked by it. */
          pos_error(ec_masked_by_default_handler, &decl_pos);
          masked = TRUE;
        } else if (handler->parameter == NULL) {
          /* Current handler is a default handler -- it can only be masked by
             another default handler. */
        } else if (prev_handler->parameter->type == error_type()) {
          /* No need to check for masking in this case. */
        } else if (type_masks_handler_param_type(prev_handler->parameter->type,
                                                 type_ptr)) {
          /* The type of prev_handler assures that handler will never be
             called, because it masks current handler's type.  See ARM 15.4. */
          pos_ty_error(ec_masked_by_handler, &decl_pos,
                       prev_handler->parameter->type);
          masked = TRUE;
        }  /* if */
        if (prev_handler->next == NULL) break;
        prev_handler = prev_handler->next;
      }  /* for */
      prev_handler->next = handler;
    }  /* if */
    (void)required_token(tok_rparen, ec_exp_rparen);
  }  /* if */
  /* Parse the body of the handler. */
  handler->statement = compound_statement(/*at_function_level=*/FALSE,
                                          /*explicit_return_type=*/FALSE,
                                          /*is_catch_clause=*/TRUE);
  /* pop_scope is called from compound_statement processing. */
  db_exit();
}  /* handler_declaration */


#if !GENERATE_SOURCE_SEQUENCE_LISTS
/* ARGSUSED */ /* is_asm_statement is only used with source sequence lists. */
#endif /* !GENERATE_SOURCE_SEQUENCE_LISTS */
an_asm_entry_ptr asm_declaration(a_boolean  asm_decl_allowed,
                                 a_boolean  is_asm_statement)
/*
Scan an asm declaration, create an entry to represent it in the IL, and
return a pointer to the asm entry.  An asm declaration is specified as
follows in the ARM:

  asm ( string-literal ) ;

We infer that it can appear as a declaration at file scope, function scope,
and block scope.  It can also appear as a block of executable code, so
in C mode, where declarations and executable statements may not be mingled,
an asm "declaration" is actually treated as an executable statement.
*/
{
  a_constant        asm_string;
  an_asm_entry_ptr  ap = NULL;
  a_source_position asm_pos;

  db_enter(3, "asm_declaration");
  check_assertion(curr_token == tok_asm);
  if (!asm_decl_allowed) {
    /* An asm declaration is not allowed in the current scope. */
    error(ec_asm_not_allowed);
    discard_curr_construct_pragmas();
  } else {
    /* Issue diagnostics on pragmas that are trying to bind to an asm
       declaration. */
    cannot_bind_to_curr_construct();
  }  /* if */
  copy_source_position(pos_curr_token, asm_pos);
  /* Skip past the "asm". */
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
  /* Check for and skip the closing parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_rparen);
  /* Check for and skip the semicolon. */
  (void)required_token(tok_semicolon, ec_exp_semicolon);
  /* Update the IL. */
  if (asm_decl_allowed) {
    ap = alloc_asm_entry();
    ap->asm_string = alloc_unshared_constant(&asm_string);
    copy_source_position(asm_pos, ap->source_corresp.decl_position);
    /* Add the asm entry to the list for the current scope. */
    add_to_asm_entries_list(ap);
#if GENERATE_SOURCE_SEQUENCE_LISTS
    if (!is_asm_statement) {
      /* There's no name or symbol for the asm declaration, so call
         update_source_sequence_list directly. */
      update_source_sequence_list((char *)ap, (an_il_entry_kind)iek_asm_entry,
                                  (a_source_sequence_entry_ptr)NULL);
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  }  /* if */

  db_exit();
  return ap;
}  /* asm_declaration */


/*
Local macro for the routine "declaration".  Does any remove_stop_token
calls that have not yet been done.  Useful in ensuring that all the stop
tokens get removed, especially when the internal flow is complicated by
error cases.
*/
#define remove_all_local_stop_tokens()                                \
{ if (need_semicolon_remove_stop_token) {                             \
    remove_stop_token(tok_semicolon);                                 \
    need_semicolon_remove_stop_token = FALSE;                         \
  }  /* if */                                                         \
  if (need_comma_remove_stop_token) {                                 \
    remove_stop_token(tok_comma);                                     \
    need_comma_remove_stop_token = FALSE;                             \
  }  /* if */                                                         \
  if (need_assign_remove_stop_token) {                                \
    remove_stop_token(tok_assign);                                    \
    need_assign_remove_stop_token = FALSE;                            \
  }  /* if */                                                         \
  if (need_lbrace_remove_stop_token) {                                \
    remove_stop_token(tok_lbrace);                                    \
    need_lbrace_remove_stop_token = FALSE;                            \
  }  /* if */                                                         \
}  /* remove_all_local_stop_tokens */


void declaration(a_boolean      function_definition_allowed,
                 a_boolean      extern_implied,
                 a_boolean      is_old_style_param_decl,
                 a_param_id_ptr param_id_list)
/*
Scan a declaration (standard, 3.5).  If function_definition_allowed is TRUE,
alternatively scan a function-definition (3.7.1).  With that flag TRUE, this
routine also corresponds to an external-declaration (3.7).  param_id_list
is non-NULL if this declaration is for an old-style function parameter; in 
that case, the identifier declared must be on the list.

Syntax:

3.7    external-declaration:
		function-definition
		declaration
3.7.1  function-definition:
		declaration-specifiers    declarator declaration-list
                                      opt                            opt
		    compound-statement
3.5    declaration:
		declaration-specifiers init-declarator-list    ;
                                                           opt
3.5    init-declarator-list:
		init-declarator
		init-declarator-list , init-declarator
3.5    init-declarator:
		declarator
		declarator = initializer

This routine is used in scanning file-scope declarations (of types,
variables, and functions), old-style parameter declarations, and declarations
of local variables (and types, etc.) of functions and in blocks.
*/
{
  a_boolean         local_is_old_style_param_decl;
  a_storage_class   storage_class, local_storage_class;
  a_type_ptr        type_ptr, old_type;
  a_type_ptr	    local_type_ptr;
  a_boolean         has_explicit_type_specifier;
  a_boolean	    declares_something;
  a_boolean	    defines_something;
  a_decl_flag_set   dso_flags, do_flags;
  a_decl_flag_set   dsi_flags, di_flags;
  a_symbol_ptr      symbol_ptr, ext_sym;
  a_boolean	    decl_specifiers_omitted = FALSE;
  a_boolean         is_function, is_main_function;
  a_boolean         is_constructor_or_destructor;
  a_boolean         is_static_data_member;
  a_symbol_locator  locator;
  a_param_id_ptr    param_id;
  a_type_ptr        bottom_derived_type;
  a_func_info_block func_info;
  a_boolean         top_declarator_type_is_function;
  an_id_linkage_kind
                    linkage;
  a_boolean         has_initializer;
  a_boolean         has_parenthesized_initializer;
  a_boolean         err = FALSE;
  a_boolean         decl_start;
  a_boolean         dangling_type_specifier = FALSE;
  a_boolean         inline_specified;
  a_source_position decl_start_pos, declarator_pos;
  a_boolean         need_semicolon_remove_stop_token = FALSE;
  a_boolean         need_comma_remove_stop_token     = FALSE;
  a_boolean         need_assign_remove_stop_token    = FALSE;
  a_boolean         need_lbrace_remove_stop_token    = FALSE;
  a_boolean         is_variable_def, incomplete_type_error_reported;
  a_boolean         is_tentative_definition;
  a_variable_ptr    var_ptr;
  a_source_sequence_entry_ptr
                    declarator_ssep = NULL;
  a_boolean         first_declarator = TRUE;
#if ASM_FUNCTION_ALLOWED
  a_boolean         is_asm_function = FALSE;
#endif /* ASM_FUNCTION_ALLOWED */

  db_enter(3, "declaration");

  set_err_pos_to_curr_token();
  copy_source_position(pos_curr_token, decl_start_pos);
  if (extern_implied) {
    /* Called in the midst of an ``extern "C"'' declaration, so
       select_curr_construct_pragmas has already been called. */
  } else if (!function_definition_allowed) {
    /* Called while processing a routine -- select_curr_construct_pragmas
       will already have been called. */
  } else {
    /* Move cached #pragma declarations (if any) to the current scope stack
       entry so they can be examined and acted upon in subsequent
       processing. */
    (void)select_curr_construct_pragmas(/*add_to_list=*/FALSE);
  }  /* if */
  if (C_dialect == C_dialect_cplusplus) {
    if (curr_token == tok_extern && next_token() == tok_string_literal) {
      /* This looks like a C++ linkage specification, which is "extern"
         followed by a string literal (e.g., "C++" or "C"). */
      linkage_specification(function_definition_allowed,
                            is_old_style_param_decl, param_id_list);
      goto return_point;
    } else if (curr_token == tok_template) {
      /* Do the processing required for a template declaration.  */
      symbol_ptr = template_declaration(&defines_something);
      if (symbol_ptr != NULL && defines_something &&
          (symbol_ptr->kind == (a_symbol_kind)sk_function_template ||
           symbol_ptr->kind == (a_symbol_kind)sk_member_function)) {
        /* No trailing semicolon expected for a function template. */
      } else {
        /* This should be a class template -- check for final semicolon. */
        (void)required_token(tok_semicolon, ec_exp_semicolon);
      }  /* if */
      goto return_point;
    }  /* if */
  }  /* if */
  add_stop_token(tok_semicolon);
  need_semicolon_remove_stop_token = TRUE;
  if (curr_token == tok_asm) {
#if ASM_FUNCTION_ALLOWED
    if (function_definition_allowed && next_token() != tok_lparen) {
      is_asm_function = TRUE;
      /* Skip over the "asm". */
      (void)get_token();
    } else {
#endif /* ASM_FUNCTION_ALLOWED */
      /* Scan the asm declaration. */
      (void)asm_declaration(/*asm_decl_allowed=*/!is_old_style_param_decl,
                            /*is_asm_statement=*/FALSE);
      goto return_point;
#if ASM_FUNCTION_ALLOWED
    }  /* if */
#endif /* ASM_FUNCTION_ALLOWED */
  }  /* if */

  if (C_dialect == C_dialect_cplusplus) {
    /* Check for and discard declarations of the form "overload f;". */
    if (check_for_overload_anachronism()) {
      /* Issue diagnostics on pragmas that are trying to bind to an overload
         declaration. */
      cannot_bind_to_curr_construct();
      goto return_point;
    }  /* if */
  }  /* if */
  /* Set the flags for calling decl_specifiers. */
  decl_start = is_decl_start(/*expr_context=*/FALSE,
                             /*real_declarator_allowed=*/TRUE);
  dsi_flags = DSI_TYPE_SPECIFIER_ALLOWED;
  /* Within a non-block linkage specification no storage class is allowed
     (inferred from ARM 7.4). */
  if (!extern_implied) dsi_flags |= DSI_STORAGE_CLASS_SPECIFIER_ALLOWED;
  dsi_flags |= DSI_CHECK_FOR_DANGLING_TYPE_SPECIFIER;
  if (is_old_style_param_decl) {
    dsi_flags |= DSI_IS_PARAMETER;
    dsi_flags |= DSI_IS_OLD_STYLE_PARAM_DECL;
  } else {
    /* A "vacuous declaration" of a class, struct, or union is allowed, but
       only has an effect when not at file scope. */
    dsi_flags |= DSI_VACUOUS_TAG_DECL_ALLOWED;
    if (function_definition_allowed) {
      dsi_flags |= DSI_EMPTY_DECL_SPECIFIERS_ALLOWED;
      /* "inline" is allowed only on function declarations at file scope. */
      if (!extern_implied) dsi_flags |= DSI_INLINE_ALLOWED;
    }  /* if */
  }  /* if */
  /* Scan the initial declaration specifiers (including storage class,
     type specifiers, and type qualifiers).  For a function definition,
     the specifiers can be omitted entirely. */
  if (!decl_start) {
    if (function_definition_allowed && is_declarator_start()) {
      /* Function definition with omitted specifiers. */
    } else {
      /* Look for some cases that are obviously not the start of a declaration,
         and give a more specific "Expected a declaration" message. */
      if (curr_token == tok_semicolon) {
        /* An empty declaration is ignored (as an extension in ANSI mode). */
        if (strict_ansi_mode) {
          diagnostic(strict_ansi_discretionary_severity, ec_extra_semicolon);
        } else {
          remark(ec_extra_semicolon);
        }  /* if */
      } else {
        if (curr_token == tok_lbrace) {
          /* Special error recovery on encountering an open brace: it
             may be the start of a routine. */
          error(ec_exp_declaration);
          flush_until_matching_token();
          if (curr_token == tok_rbrace) (void)get_token();
          if (is_decl_start(/*expr_context=*/FALSE,
                            /*real_declarator_allowed=*/TRUE)) {
            goto continue_with_declaration;
          }  /* if */
        } else {
          syntax_error(ec_exp_declaration);
        }  /* if */
      }  /* if */
      /* Give up on scanning a declaration (assume we're at the end of one). */
      if (curr_token == tok_semicolon) (void)get_token();
      discard_curr_construct_pragmas();
      goto return_point;
    }  /* if */
  }  /* if */
continue_with_declaration:
  /* Scan the specifiers. */
  err = decl_specifiers(dsi_flags, &dso_flags, &storage_class, &type_ptr);
  has_explicit_type_specifier = dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER;
  declares_something = dso_flags & DSO_DECLARES_SOMETHING;
  defines_something = dso_flags & DSO_DEFINES_SOMETHING;
  dangling_type_specifier = dso_flags & DSO_DANGLING_TYPE_SPECIFIER;
  decl_specifiers_omitted = dso_flags & DSO_NO_DECL_SPECIFIERS;
  is_constructor_or_destructor =
                            dso_flags & (DSO_CONSTRUCTOR | DSO_DESTRUCTOR);
  inline_specified = dso_flags & DSO_INLINE;
  /* The declaration can end at this point (";" is next). */
  if (curr_token == tok_semicolon && !decl_specifiers_omitted) {
    if (err) {
      /* There was a previous error, so do not check further. */
    } else if (is_old_style_param_decl &&
               (declares_something || defines_something)) {
      /* ANSI C does not allow freestanding declarations (as of structs)
         within an old-style parameter list.  pcc, on the other hand,
         will allow something like
            int f(a)
            struct s {int b;};
            struct s a;
            { ... }
      */
      if (C_dialect != C_dialect_pcc) {
        diagnostic(strict_ansi_mode ? strict_ansi_error_severity : es_warning,
                   ec_decl_should_be_of_param);
      }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
      set_autonomous_tag_decl_flag(type_ptr, defines_something);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    } else if (!declares_something && C_dialect == C_dialect_cplusplus &&
               defines_something && type_ptr->kind == (a_type_kind)tk_union &&
               storage_class != (a_storage_class)sc_typedef) {
      /* Special C++ case:  the declaration of an anonymous union.   Do the
         required error checking and special processing, including creation
         of a variable which will represent the anonymous union and with
         which its fields will be aliased. */
      check_assertion(is_unnamed_class_symbol(
                        (a_symbol_ptr)(type_ptr->source_corresp.assoc_info)));
      make_anonymous_union_variable(type_ptr, storage_class);
      /* The anonymous union variable is marked as referenced, as are all
         unnamed entities.  So its type is also marked referenced. */
      type_ptr->source_corresp.referenced = TRUE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
      set_autonomous_tag_decl_flag(type_ptr, /*is_definition=*/TRUE);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    } else if (extern_implied && is_enum_type(type_ptr)) {
      /* This is a declaration like
                      extern "C" enum E { e1, e2, e3 };
         which is not allowed (inference from ARM 7.4). */
      pos_error(ec_enum_not_allowed, &decl_start_pos);
    } else {
      if (storage_class == (a_storage_class)sc_typedef) {
        /* Typedef declaration with no declarator. */
        an_error_severity  severity = es_warning;

        if (declares_something ||
            (defines_something && is_enum_type(type_ptr))) {
          /* No error on a case like "typedef struct S { int i; };" or
             "typedef enum { red, green, blue };" -- see first constraint,
             Section 3.5 of the ANSI C standard.  However, a warning should
             be issued, since the "typedef" is superfluous. */
        } else {
          /* A case like "typedef int;" or "typedef struct { int i; };" --
             gets a warning by default but may get an error in strict ANSI
             mode. */
          if (strict_ansi_mode) severity = strict_ansi_error_severity;
        }  /* if */
        set_err_pos_to_curr_token();
        diagnostic(severity, ec_missing_typedef_name);
      } else {
        if (!declares_something) {
          /* The specifiers should have declared something or this declaration
             is pointless.  Examples would be
                int ;
                struct { int i; };
             whereas, despite the missing declarator,
                struct x {int a;};
             is not useless since it declares something (namely x). */
          /* ANSI probably thinks of this as an error, but that seems a bit
             extreme, especially since pcc allows it.  Normally we issue a
             warning, unless the -A option is selected. */
          diagnostic(strict_ansi_mode ?
                       strict_ansi_error_severity : es_warning,
                     ec_useless_decl);
        }  /* if */
        /* A storage class can only be specified for an object or a function
           (ARM 7.1.1). */
        if (storage_class != (a_storage_class)sc_unspecified) {
          diagnostic((C_mode() || any_cfront_mode()) ? es_warning : es_error,
                     ec_storage_class_not_allowed);
        }  /* if */
        /* ARM 7.1.6 implies that the absence of a object in this declaration
           makes it ill-formed.  Is the implication strong enough to justify
           an error here? */
        if (is_qualified_type(type_ptr)) {
          diagnostic(C_dialect == C_dialect_cplusplus && strict_ansi_mode ?
                       strict_ansi_error_severity : es_warning,
                     ec_const_volatile_not_allowed);
        }  /* if */
        /* Inline can only be specified for a function (ARM 7.1.2). */
        if (inline_specified) {
          error(ec_inline_and_nonfunction);
        }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
        if (defines_something || declares_something) {
          /* This is a class/struct/union or enum declaration. */
          set_autonomous_tag_decl_flag(type_ptr, defines_something);
        }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      }  /* if */
    }  /* if */
    discard_curr_construct_pragmas();
  } else if (dangling_type_specifier ||
             (!decl_specifiers_omitted && !C_mode() &&
              identifier_is_template_id())) {
    /* A class, struct, union, or enum definition was followed by a type
       specifier keyword.  Issue a missing-semicolon error, since the type
       specifier can be taken as introducing a new declaration. */
    /* A similar case is the identifier-but-not-declarator-id case -- which
       occurs when a template-id appears where a declarator was expected. */
    set_err_pos_to_curr_token();
    if (is_old_style_param_decl && declares_something) {
      /* An old style param declaration that introduces a named struct or
         enum type but has no declarator for the parameter. */
      pos_error(ec_decl_should_be_of_param, &decl_start_pos);
    } else if (!declares_something) {
      /* A declaration that introduces an unnamed struct or enum type but has
         no declarator.  May or may not be in an old-style param list. */
      error(ec_exp_identifier);
    }  /* if */
    error(ec_exp_semicolon);
    discard_curr_construct_pragmas();
    goto return_point;
  } else if (curr_token == tok_void && C_dialect == C_dialect_pcc && 
             storage_class == (a_storage_class)sc_typedef &&
             next_token() == tok_semicolon) {
    /* "typedef <something> void;" in pcc mode.  Usually "typedef int void;".
       Shows up in old pre-void-keyword code.  Ignored in pcc mode. */
    set_err_pos_to_curr_token();
    warning(ec_decl_of_void_ignored);
    discard_curr_construct_pragmas();
    (void)get_token();
  } else {
    /* Set the various flags for declarator processing. */
    di_flags = DI_REAL_DECLARATOR_ALLOWED;
    if (C_dialect == C_dialect_cplusplus) {
      di_flags |= DI_PARENTHESIZED_INITIALIZER_ALLOWED;
      di_flags |= DI_OPERATOR_NAME_ALLOWED;
      if (storage_class != (a_storage_class)sc_typedef &&
          decl_scope_level == DEPTH_OF_FILE_SCOPE) {
        di_flags |= DI_QUALIFIED_NAME_ALLOWED;
      }  /* if */
    }  /* if */
    if (storage_class == (a_storage_class)sc_typedef) {
      di_flags |= DI_IS_TYPEDEF_DECLARATION;
    }  /* if */
    /* Scan the declarator list. */
    do {
      add_stop_token(tok_comma);
      need_comma_remove_stop_token = TRUE;
      add_stop_token(tok_assign);
      need_assign_remove_stop_token = TRUE;
      if (function_definition_allowed) {
        add_stop_token(tok_lbrace);
        need_lbrace_remove_stop_token = TRUE;
      }  /* if */
      if (curr_token == tok_identifier &&
          (locator_for_curr_id.is_operator_name ||
           locator_for_curr_id.is_conversion_name)) {
        copy_source_position(locator_for_curr_id.source_position,
                             declarator_pos);
      } else {
        copy_source_position(pos_curr_token, declarator_pos);
      }  /* if */
      clear_func_info(&func_info);
#if GENERATE_SOURCE_SEQUENCE_LISTS
      if (!first_declarator && depth_scope_stack == DEPTH_OF_FILE_SCOPE) {
        /* This is a declaration at file scope, and not the first declarator
           in the declarator list.  As for the start of the declaration,
           set the source-sequence insert point for instantiations to NULL. */
        scope_stack[DEPTH_OF_FILE_SCOPE].
                                ss_list_instantiation_insert_point = NULL;
      }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      declarator(di_flags, &do_flags, type_ptr, 
                 /*member_parent_type=*/(a_type_ptr)NULL, &locator,
                 &local_type_ptr, &bottom_derived_type, &declarator_ssep,
                 &func_info);
      is_function = (storage_class != (a_storage_class)sc_typedef &&
                     is_function_type(local_type_ptr));
      is_main_function = FALSE;
      if (is_function && !is_error_locator(locator) &&
          locator.symbol_header->identifier != NULL &&
          (strcmp(locator.symbol_header->identifier, "main") == 0)) {
        /* Recognizing a declaration of function "main" is more than checking
           the identifier. */
        if (C_dialect == C_dialect_cplusplus) {
          if (locator.specific_symbol == NULL ||
              locator.specific_symbol->class_of_which_a_member == NULL) {
            /* Not a member function named "main". */
            func_info.is_main_function = is_main_function = TRUE;
            /* Perform some error checking that is specific to C++. */
            if (def_external_linkage.is_explicit) {
              pos_warning(ec_linkage_specifier_not_allowed, &declarator_pos);
            }  /* if */
            /* "inline" and "static" are not allowed (ARM 3.4). */
            if (storage_class == (a_storage_class)sc_static) {
              pos_error(ec_static_not_allowed, &declarator_pos);
              storage_class =(a_storage_class)sc_unspecified;
            }  /* if */
            if (inline_specified) {
              pos_error(ec_inline_main, &declarator_pos);
              inline_specified = FALSE;
            }  /* if */
          }  /* if */
        } else {
          if (storage_class == (a_storage_class)sc_unspecified ||
              storage_class == (a_storage_class)sc_extern) {
            /* Not a static function named "main".  This is not an option
               in C++ (ARM 3.4). */
            func_info.is_main_function = is_main_function = TRUE;
          }  /* if */
        }  /* if */
      } else if (storage_class == (a_storage_class)sc_typedef &&
                 (do_flags & DO_CFRONT_MEMBER_FUNCTION_TYPEDEF)) {
        /* This looked like a cfront-style member function typedef.  Be sure
           the type was a function type. */
        if (is_function_type(local_type_ptr)) {
          /* Issue a warning on the extension. */
          pos_warning(ec_ptr_to_member_typedef, &locator.source_position);
        } else {
          /* No function type, so what looked like a qualified name really
             was -- but they aren't allowed. */
          pos_error(ec_qualified_name_not_allowed, &locator.source_position);
          set_to_error_locator(locator);
        }  /* if */
      }  /* if */
      has_parenthesized_initializer = do_flags & DO_PARENTHESIZED_INITIALIZER;
      /* top_declarator_type_is_function is TRUE if the fact that this is a
         function is derived from the declarator and not from a typedef.  It
         is sufficient that the result type is a function type and a
         declarator was scanned. */
      top_declarator_type_is_function = (is_function &&
				         local_type_ptr != type_ptr);
      if (is_function && !top_declarator_type_is_function &&
          any_cfront_mode()) {
        a_type_ptr                     tp = skip_typerefs(local_type_ptr);
        a_routine_type_supplement_ptr  rtsp = tp->variant.routine.extra_info;

        /* Check for the declaration of a function with a typedef type that
           is supposed to be used only for pointer-to-member declarations
           (and only in cfront compatibility mode). */
        if (rtsp->implicit_this_param_type != NULL) {
          /* It must be one of these special member function typedefs.  Issue
             an error, since this appears to be a function declaration,
             not a pointer to member declaration. */
          pos_sy_error(ec_bad_use_of_ptr_to_member_typedef, &decl_start_pos,
                       (a_symbol_ptr)(make_unqualified_type(local_type_ptr)->
                                                   source_corresp.assoc_info));
          /* Replace the type with one that does not have an implicit
             this param. */
          local_type_ptr = alloc_type((a_type_kind)tk_routine);
          copy_routine_type_with_param_types(tp, local_type_ptr);
          local_type_ptr->variant.routine.extra_info->
                                       implicit_this_param_type = NULL;
        }  /* if */
      }  /* if */
      if (C_dialect == C_dialect_cplusplus && defines_something) {
        /* The ARM (8.2.5) explicitly prohibits defining a type in a
           function return type.  This is taken to apply to pointer-to-function
           type declarations as well to the function declarations. */
        a_boolean  is_function_type_decl = is_function;
        if (!is_function_type_decl) {
          a_type_ptr  tp = local_type_ptr;
          for (;;) {
            if (is_ptr_or_ref_type(tp)) {
              /* Get type pointed to and continue. */
              tp = type_pointed_to(tp);
            } else if (is_ptr_to_member_type(tp)) {
              /* Get member type and continue. */
              tp = pm_member_type(tp);
            } else {
              /* No function type can be involved.  Stop looping. */
              break;
            }  /* if */
          }  /* for */
          is_function_type_decl = is_function_type(tp);
        }  /* if */
        if (is_function_type_decl) {
          pos_error(ec_type_def_not_allowed_in_func_type_decl,
                    &decl_start_pos);
        }  /* if */
      }  /* if */
      local_storage_class = storage_class;
      local_is_old_style_param_decl = is_old_style_param_decl;
      /* If this is a parameter (old-style), make sure it appears on
         the param_id_list.  Also adjust the type if necessary
         (for example, "array of x" becomes "pointer to x"). */
      if (local_is_old_style_param_decl) {
        if (local_storage_class == (a_storage_class)sc_typedef) {
          if (strict_ansi_mode && strict_ansi_error_severity == es_error) {
            pos_error(ec_decl_should_be_of_param, &decl_start_pos);
            set_to_error_locator(locator);
          } else {
            pos_warning(ec_decl_should_be_of_param, &decl_start_pos);
          }  /* if */
          local_is_old_style_param_decl = FALSE;
        } else {
          param_id = param_id_on_list(&locator, param_id_list);
          if (param_id == NULL) {
            /* The identifier was not found on the list. */
            error(ec_decl_should_be_of_param);
            /* Enter the declared object as a variable rather than as a
               parameter.  */
            local_is_old_style_param_decl = FALSE;
          } else if (param_id->type != NULL) {
            /* Parameter has already been declared. */
            str_error(ec_id_already_declared,
                      locator.symbol_header->identifier);
          } else {
            /* When the parameter name was listed (but not yet actually
               declared) the sk_parameter symbol was created but not entered
               in the symbol table.  Now that it is explicitly declared, add
               it to the function prototype scope; it will later be moved
               to the function scope. */
            reenter_symbol(param_id->symbol, decl_scope_level,
                           /*suppress_error=*/FALSE);
#if GENERATE_SOURCE_SEQUENCE_LISTS
            param_id->source_sequence_entry = declarator_ssep;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
          }  /* if */
          /* Check that the type is legal, and do required adjustments. */
          check_and_adjust_parameter_type(&local_type_ptr, &decl_start_pos);
          is_function = top_declarator_type_is_function = FALSE;
          /* For pcc compatibility, promote float parameters to double. */
          if (C_dialect == C_dialect_pcc) {
            promote_float_to_double(local_type_ptr);
          }  /* if */
        }  /* if */
      } else if (local_storage_class != (a_storage_class)sc_typedef) {
        /* See if any type qualifiers were specified, and if they are okay. */
        check_type_qualifiers(&local_type_ptr);
      }  /* if */
      if (need_lbrace_remove_stop_token) {
        remove_stop_token(tok_lbrace);
        need_lbrace_remove_stop_token = FALSE;
      }  /* if */
#if ASM_FUNCTION_ALLOWED
      if (is_asm_function) {
        /* This is an asm function.  Go process it. */
        remove_all_local_stop_tokens();
        asm_function_definition(&locator, local_type_ptr, 
                                top_declarator_type_is_function,
                                &func_info, local_storage_class);
        done_with_func_info(func_info);
        goto return_point;
      }  /* if */
#endif /* ASM_FUNCTION_ALLOWED */
      if (is_function && local_storage_class != (a_storage_class)sc_typedef) {
        if (local_storage_class != (a_storage_class)sc_unspecified &&
            local_storage_class != (a_storage_class)sc_extern &&
            local_storage_class != (a_storage_class)sc_static) {
          /* The storage class of a function must be extern or static. */
          pos_error(ec_bad_function_storage_class, &declarator_pos);
          local_storage_class = (a_storage_class)sc_unspecified;
        }  /* if */
        if (locator.specific_symbol != NULL &&
            locator.specific_symbol->class_of_which_a_member != NULL) {
          /* This is the definition of a static member function.  No storage
             class specifier (not even "static") is permitted. */
          if (local_storage_class != (a_storage_class)sc_unspecified) {
            if (local_storage_class == (a_storage_class)sc_static &&
                inline_specified) {
              /* Just give a warning on this.  The storage class designation
                 is taken to be redundant, since all "inline" member functions
                 (both static and nonstatic, in the sense applied to member
                 functions) are "static" (in the sense of having internal
                 linkage). */
              pos_warning(ec_storage_class_not_allowed, &decl_start_pos);
            } else {
              pos_error(ec_storage_class_not_allowed, &decl_start_pos);
            }  /* if */
          }  /* if */
          /* Set the storage class to sc_static. */
          local_storage_class = (a_storage_class)sc_static;
        }  /* if */
      }  /* if */
      /* Check for restrictions on use of the "inline" specifier. */
      if (inline_specified) {
        if (!is_function) {
          /* Not a function declaration. */
          pos_error(ec_inline_and_nonfunction, &declarator_pos);
        } else {
          /* Set the storage class to sc_static. */
          local_storage_class = (a_storage_class)sc_static;
          func_info.is_inline = TRUE;
        }  /* if */
      }  /* if */
      /* Indicate whether the function type is based on a typedef. */
      func_info.function_type_from_typedef = !top_declarator_type_is_function;
      /* If the thing declared is a function, and if the token following looks
         like it could be part of a function-definition, go scan that. */
      if (function_definition_allowed && is_function &&
          local_storage_class != (a_storage_class)sc_typedef &&
          curr_token != tok_semicolon && curr_token != tok_comma &&
          curr_token != tok_assign && curr_token != tok_end_of_source) {
        if (!has_explicit_type_specifier && !is_main_function) {
          /* Function with no explicitly specified return type.  Issue a
             remark (except in pcc mode and except for C++ constructors,
             destructors, and conversion operators). */
          if (C_dialect != C_dialect_pcc) {
            if (C_dialect != C_dialect_cplusplus ||
                (!is_constructor_or_destructor &&
                 !locator.is_conversion_name)) {
              pos_remark(ec_missing_type_specifier, &declarator_pos);
            }  /* if */
          }  /* if */
        }  /* if */
        remove_all_local_stop_tokens();
        func_info.is_definition = TRUE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
        func_info.declarator_ssep = declarator_ssep;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        function_definition(&locator, local_type_ptr, &func_info,
                            local_storage_class, has_explicit_type_specifier);
        done_with_func_info(func_info);
        goto return_point;
      }  /* if */
      /* Not a function definition, must be a declaration. */
      /* After a declaration has been scanned, it is no longer possible
         that the next thing is a function definition. */
      function_definition_allowed = FALSE;
      is_static_data_member = FALSE;
      if (locator.specific_symbol != NULL &&
          locator.specific_symbol->class_of_which_a_member != NULL) {
        if (is_function) {
          /* A qualified name that identifies a function is allowed only when
             the function body is present. */
          if (is_member_function_symbol(locator.specific_symbol)) {
            pos_error(ec_member_function_redecl_outside_class,
                      &declarator_pos);
          } else {
            pos_sy_error(ec_not_compatible_with_previous_decl,
                         &declarator_pos, locator.specific_symbol);
          }  /* if */
          set_to_error_locator(locator);
        } else {
          /* Assume that qualified names that are not functions refer to static
             data members. */
          is_static_data_member = TRUE;
        }  /* if */
      }  /* if */
      /* Issue diagnostics on missing type specifiers, etc. */
      if (!is_main_function) {
        if (decl_specifiers_omitted &&
            (C_dialect != C_dialect_cplusplus || !is_function)) {
          /* In ANSI C declaration specifiers can only be entirely omitted in
             a function definition.  This is possibly an undefined typedef
             name at the start of a declaration, so enter an error symbol
             instead of the name given.  In pcc mode the declaration is taken
             as a declaration of an int variable.  (In C++ the decl specifiers
             may be omitted on function declarations and definitions.) */
          if (C_dialect == C_dialect_pcc) {
            pos_warning(ec_missing_decl_specifiers, &declarator_pos);
          } else {
            pos_error(ec_missing_decl_specifiers, &declarator_pos);
          }  /* if */
        } else if (!has_explicit_type_specifier) {
          if (is_function) {
            /* Function with no explicitly specified return type.  Issue a
               remark (except in pcc mode and except for C++ constructors,
               destructors, and conversion operators). */
            if (C_dialect != C_dialect_pcc) {
              if (C_dialect != C_dialect_cplusplus ||
                  (!is_constructor_or_destructor &&
                   !locator.is_conversion_name)) {
                pos_remark(ec_missing_type_specifier, &declarator_pos);
              }  /* if */
            }  /* if */
          } else {
            /* For implicitly typed nonfunction declarations (variables,
               typedefs, etc.) issue a warning in all modes. */
            pos_warning(ec_missing_type_specifier, &declarator_pos);
          }  /* if */
        }  /* if */
      }  /* if */
      if (top_declarator_type_is_function) {
        a_param_id_ptr  pid = func_info.param_id_list;
#if GENERATE_SOURCE_SEQUENCE_LISTS
        a_src_seq_sublist_ptr  sublist = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        if (pid != NULL) {
          /* If the function has a non-empty old-style identifier list of
             parameters, a body should have been present. */
          if (!local_type_ptr->variant.routine.extra_info->prototyped) {
            error(ec_param_id_list_needs_function_def);
          }  /* if */
          /* After updating xref information on each symbol, free the list
             of parameter identifiers -- they're not needed if there's no
             definition. */
          for (; pid != NULL; pid = pid->next) {
            if (pid->symbol != NULL) {
              mark_declared(pid->symbol, &pid->symbol->decl_position);
            }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
            if (pid->source_sequence_entry != NULL) {
              check_assertion(ss_entry_kind(pid->source_sequence_entry) ==
                                                 (an_il_entry_kind)iek_none);
              remove_from_source_sequence_list(pid->source_sequence_entry,
                                               &sublist);
              pid->source_sequence_entry = NULL;
            }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
          }  /* for */
        }  /* if */
      }  /* if */
      /* Do some checking of storage classes, but not for typedefs. */
      if (local_storage_class != (a_storage_class)sc_typedef) {
        /* auto and register may not appear in a file-scope level
           declaration (3.7, constraints). */
        if (decl_scope_level == DEPTH_OF_FILE_SCOPE &&
            (local_storage_class == (a_storage_class)sc_auto ||
             local_storage_class == (a_storage_class)sc_register)) {
          pos_error(ec_bad_file_scope_storage_class, &decl_start_pos);
          local_storage_class = (a_storage_class)sc_unspecified;
        }  /* if */
        if (is_function) {
          /* A function with block scope (i.e., within an sck_function or
             sck_block scope) can only have an explicit storage class of
             extern (3.5.1). */
          if ((scope_stack[decl_scope_level].kind ==
                                           (a_scope_kind)sck_function ||
               scope_stack[decl_scope_level].kind ==
                                           (a_scope_kind)sck_block) &&
              local_storage_class != (a_storage_class)sc_unspecified &&
              local_storage_class != (a_storage_class)sc_extern) {
            /* Allow "static" in all C modes except strict ANSI. The function
               will be entered at the file scope as static.  This is an
               extension to ANSI C.  Do not allow at all in C++ mode. */
            if (local_storage_class == (a_storage_class)sc_static) {
              if (C_dialect == C_dialect_cplusplus) {
                error(ec_block_scope_function_must_be_extern);
                /* id_linkage doesn't expect block level statics in
                   C++ mode. */
                local_storage_class = (a_storage_class)sc_extern;
              } else {  /* a C dialect */
                /* This is an extension to ANSI C so produce a diagnostic
                   in strict ANSI C mode. */
                if (strict_ansi_mode) {
                  diagnostic(strict_ansi_error_severity,
                             ec_block_scope_function_must_be_extern);
                }  /* if */
              }  /* if */
            }  /* if */
          }  /* if */
          /* Make a function with no body (yet?) have a storage class of
             extern (for external linkage) or static (for internal linkage). */
          if (local_storage_class != (a_storage_class)sc_static) {
            local_storage_class = (a_storage_class)sc_extern;
          }  /* if */
        } else {
          if (is_static_data_member) {
            /* A static data member (or, illegally, a qualified name referring
               to another kind of member).  Leave the storage class set to
               sc_unspecified even if we are not at file scope. */
          } else {
            /* Not a function, not a typedef, therefore a variable or 
               parameter.  If the storage class is unspecified, and
               we are not at file scope, use a storage class of auto. */
            if (local_storage_class == (a_storage_class)sc_unspecified) {
              if (decl_scope_level != DEPTH_OF_FILE_SCOPE) {
                /* We are not at file scope, so an unspecified storage class
                   means auto. */
                local_storage_class = (a_storage_class)sc_auto;
              } else if (extern_implied) {
                /* This must be part of an linkage specification declaration.
                   An "extern" storage class is implied (ARM 7.4, comment on
                   p. 118). */
                local_storage_class = (a_storage_class)sc_extern;
              }  /* if */
            }  /* if */
          }  /* if */
          if (local_is_old_style_param_decl) {
            /* The check is made elsewhere for parameters. */
          } else if (is_abstract_class_type(local_type_ptr)) {
            /* Abstract class objects are prohibited (ARM 10.3). */
            error(ec_abstract_class_object_not_allowed);
          }  /* if */
        }  /* if */
      }  /* if */
      /* Enter the symbol with the proper type. */
      linkage = idl_none;
      var_ptr = NULL;
      /* Look for optional initializer. */
      remove_stop_token(tok_assign);
      need_assign_remove_stop_token = FALSE;
      if (has_parenthesized_initializer) {
        has_initializer = TRUE;
      } else if (curr_token == tok_assign) {
        has_initializer = TRUE;
#if C_ANACHRONISMS_ALLOWED
      } else if (C_dialect == C_dialect_pcc && is_initializer_start()) {
        /* In pcc mode, the "=" may be omitted (K&R first edition, Appendix A,
           section 17 (Anachronisms)). */
        has_initializer = TRUE;
        warning(ec_old_fashioned_initializer);
#endif /* C_ANACHRONISMS_ALLOWED */
      } else {
        has_initializer = FALSE;
      }  /* if */
      is_variable_def = FALSE;
      is_tentative_definition = FALSE;
      if (local_is_old_style_param_decl) {
        symbol_ptr = param_id->symbol;
        copy_source_position(locator.source_position,
                             symbol_ptr->decl_position);
        param_id->type = local_type_ptr;
        copy_source_position(decl_start_pos, param_id->type_pos);
        param_id->storage_class = local_storage_class;
        /* Note that the creation of the parameter variable, etc., is done
           in decl_parameter, called when the function body is scanned. */
      } else if (local_storage_class == (a_storage_class)sc_typedef) {
        /* A typedef declaration. */
        decl_typedef(&locator, local_type_ptr, &symbol_ptr, declarator_ssep);
      } else if (is_static_data_member) {
        /* A static data member definition. */
        define_static_data_member(&locator, local_storage_class,
                                  local_type_ptr, declarator_ssep,
                                  &symbol_ptr, &linkage);
        var_ptr = symbol_ptr->variant.static_data_member.variable;
        /* Fetch the type of the symbol again, since it might have been
           changed when reconciled with the original declaration. */
        local_type_ptr = var_ptr->type;
        /* All static data member declarations that that pass though this
           code are definitions. */
        is_variable_def = TRUE;
      } else if (is_function) {
        /* A function declaration with no body. */
        decl_var_or_routine(&locator, local_storage_class, local_type_ptr,
                            &func_info, declarator_ssep, SRK_DECLARATION,
                            &symbol_ptr, &linkage, &old_type, &ext_sym);
      } else {
        /* A variable declaration. */
        a_symbol_reference_kind  srk_flags = SRK_DECLARATION;

        /* Set a flag marking this as a defining declaration, if that's
           appropriate. */
        if (is_old_style_param_decl) {
          /* This flag is TRUE when local_is_old_style_param_decl is FALSE
             in the error case where a name appears in an old-style param
             declaration but for which no corresponding param-id was created.
               void f(i,j) int i, j, k; { }      // Error on "k"
             Treat this as a definition. */
          is_variable_def = TRUE;
        } else if (has_initializer) {
          /* A variable declaration involving an initializer is always
             considered to be a definition. */
          is_variable_def = TRUE;
        } else if (C_dialect == C_dialect_cplusplus) {
          /* In C++ all other variable declarations are definitions, except
             those with a storage class of extern. */
          is_variable_def =
                       (local_storage_class != (a_storage_class)sc_extern);
        } else {
          /* C mode. */
          if (decl_scope_level == DEPTH_OF_FILE_SCOPE) {
            if (local_storage_class == (a_storage_class)sc_unspecified ||
                local_storage_class == (a_storage_class)sc_static) {
              /* In C a file scope variable declaration with no storage class
                 or static storage class is called a tentative definition. */
              is_tentative_definition = TRUE;
              srk_flags |= SRK_TENTATIVE_DEF | SRK_DEFINITION;
            }  /* if */
          } else {
            /* In C all local variable declarations are definitions. */
            if (local_storage_class != (a_storage_class)sc_extern) {
              is_variable_def = TRUE;
            }  /* if */
          }  /* if */
        }  /* if */
        if (is_variable_def) srk_flags |= SRK_DEFINITION;
        decl_var_or_routine(&locator, local_storage_class, local_type_ptr,
                            (a_func_info_block *)NULL, declarator_ssep,
                            srk_flags, &symbol_ptr, &linkage, &old_type,
                            &ext_sym);
        var_ptr = symbol_ptr->variant.variable.ptr;
        /* Fetch the type of the symbol again, since it might have been
           changed when reconciled with the original declaration. */
        local_type_ptr = var_ptr->type;
        if (is_old_style_param_decl) {
          /* Error case (described above).  Mark the symbol referenced, to
             suppress subsequent "declared and not referenced" warnings. */
          mark_symbol_to_suppress_warnings(symbol_ptr);
        }  /* if */
      }  /* if */
      if (is_variable_def && C_dialect == C_dialect_cplusplus) {
        /* At the point at which an object of incomplete template class is
           defined, its class needs to be instantiated.  When its type is
           ref-template-class, the instantiation is also required.  Note that
           in this respect a reference does not behave like a pointer -- in
           the latter case, the instantiation is not required until the pointer
           is dereferenced. */
        a_type_ptr  tp = local_type_ptr;
        if (is_reference_type(tp)) tp = type_pointed_to(tp);
        check_for_uninstantiated_template_class(tp);
      }  /* if */
      incomplete_type_error_reported = FALSE;
      /* Set the error position to the start of the initializer (that is, to
         the "=" if there is one) or to where the initializer should be in
         case there ought to be one. */
      set_err_pos_to_curr_token();
      if (has_initializer) {
        /* Advance past the "=". */
        if (curr_token == tok_assign) (void)get_token();
        /* Now scan the initializer. */
        if (symbol_ptr->kind == (a_symbol_kind)sk_variable &&
            !is_old_style_param_decl) {
          /* Set the storage class of a file-scope initialized variable to
             unspecified (meaning external) or static (meaning internal).
             See 3.7.2. */
          if (decl_scope_level == DEPTH_OF_FILE_SCOPE) {
            if (var_ptr->storage_class == (a_storage_class)sc_extern) {
              var_ptr->storage_class = (a_storage_class)sc_unspecified;
            }  /* if */
          }  /* if */
          /* All initialized variables are considered defined.  This flag
             may have already been set based on storage class and scope
             level. */
          mark_variable_value_set(symbol_ptr);
        }  /* if */
        /* If the symbol is a parameter, the subroutine will generate the
           error.  This is done rather than flagging the error here because
           the subroutine can scan over the initializer expression neatly. */
        initializer(symbol_ptr, &locator.source_position, linkage,
                    has_parenthesized_initializer, is_old_style_param_decl,
                    &incomplete_type_error_reported);
        /* Fetch the type of the symbol again, since it might have been
           changed if it was an incomplete array and was initialized. */
        if (var_ptr != NULL) local_type_ptr = var_ptr->type;
      } else if (is_old_style_param_decl) {
        /* Don't worry about missing initializer. */
      } else if (is_variable_def && !is_error_locator(locator) &&
                 var_ptr->init_kind == (an_init_kind)initk_none) {
        /* Uninitialized variable or static data member is being defined, but
           no explicit initializer was provided.  Do default initialization
           if appropriate (e.g., if a default constructor exists). */
        if (def_initializer(symbol_ptr, &locator.source_position)) {
          /* Default initialization was successful. */
          if (symbol_ptr->kind == (a_symbol_kind)sk_variable) {
            mark_variable_value_set(symbol_ptr);
          }  /* if */
        } else if (symbol_ptr->kind == (a_symbol_kind)sk_variable) {
          /* No default initialization, so do some additional checking. */
          check_for_missing_initializer(symbol_ptr, local_type_ptr);
          if (!var_ptr->source_corresp.is_local_to_function ||
              var_ptr->storage_class == (a_storage_class)sc_static) {
            mark_variable_value_set(symbol_ptr);
          }  /* if */
        }  /* if */
      } else if (symbol_ptr->kind == (a_symbol_kind)sk_variable &&
                 (local_storage_class == (a_storage_class)sc_extern ||
                  is_tentative_definition)) {
        /* Either:  This is not a definition of a variable but rather an extern
           declaration.  Such a variable may be assumed to be initialized
           at the point of definition, so flag it as "set" (even if it is not
           actually set at the current declaration). */
        /* Or else:  This is a tentative definition (C mode only), which should
           be treated as though it were a definition. */
        mark_variable_value_set(symbol_ptr);
      }  /* if */
      copy_source_position(locator.source_position, error_position);
      if (var_ptr != NULL && is_incomplete_type(local_type_ptr)) {
        /* Issue an error on a variable for which this is the defining
           declaration but whose type is incomplete.  Also, in C mode, issue
           an error on a static variable with incomplete type (6.7.2 para 3)
           or a externally linked variable with a tentative definition but an
           uncompletable type (a case like "void i;" at file scope).  And in
           C++ mode, since no object may be of void type, issue the error for
           cases like "extern void i;" even though it is not a defining
           declaration. */
        if (is_variable_def ||
            (!C_mode() && is_void_type(local_type_ptr)) ||
            (is_tentative_definition &&
             (local_storage_class == (a_storage_class)sc_static ||
              is_void_type(local_type_ptr)))) {
          if (!incomplete_type_error_reported) {
            pos_error(ec_incomplete_type_not_allowed,
                      &locator.source_position);
          }  /* if */
          var_ptr->type = error_type();
        }  /* if */
      }  /* if */
      done_with_func_info(func_info);
      remove_stop_token(tok_comma);
      need_comma_remove_stop_token = FALSE;
      first_declarator = FALSE;
      /* Keep scanning the list of declarators. */
    } while (loop_token(tok_comma));
  }  /* if */
  /* Check for final semicolon. */
  (void)required_token(tok_semicolon, ec_exp_semicolon);

return_point:
  /* Do necessary remove_stop_tokens.  Even when there is no error, this
     does the remove_stop_token for tok_semicolon. */
  remove_all_local_stop_tokens();
  db_exit();
  return;
}  /* declaration */


void local_declaration(void)
/*
Scan a block-level declaration.
*/
{
  declaration(/*function_definition_allowed=*/FALSE,
              /*extern_implied=*/FALSE, /*is_old_style_param_decl=*/FALSE,
              (a_param_id_ptr)NULL);
}  /* local_declaration */


void translation_unit(void)
/*
Scan a translation-unit (3.7).  This is the topmost syntactic entity in
a compilation.  The syntax is

3.7    translation-unit:
		external-declaration
		translation-unit external-declaration

In C++, however, the declaration list is optional (3.4):

       translation-union:
                declaration-seq
                               opt
*/
{
  a_boolean  pch_check_pending = precompiled_header_processing_required;

  if (get_token() == tok_end_of_source) {
    /* Empty translation unit -- okay in C++ mode. */
    if (C_mode()) {
      /* A translation unit cannot be empty.  Note that this can happen not
         only for an empty file, but also for a file containing only
         preprocessing directives.  pcc allows an empty source file.
         In ANSI mode, it's allowed as an extension. */
      if (strict_ansi_mode) {
        diagnostic(strict_ansi_error_severity, ec_empty_translation_unit);
      }  /* if */
    }  /* if */
  } else {
    do {
      if (pch_check_pending && !curr_ise->is_include_file) {
        if (curr_ise->actual_line ==
              (a_line_number)header_stop_source_position.seq &&
            pos_curr_token.column == header_stop_source_position.column) {
          /* This should be the first declaration in the primary source file
             (i.e., excluding preprocessor directives).  If there were any
             include files and if the current state otherwise qualifies, write
             out the IL, symbol table, etc. to a precompiled header file. */
          generate_precompiled_header();
        }  /* if */
        pch_check_pending = FALSE;
      }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
      /* For each declaration at file scope, reset the source-sequence insert
         point for instantiations to NULL -- it will be set to point to the
         first source sequence entry that add_to_source_sequence_list sees,
         which should be the first entry associated with the current
         declaration. */
      scope_stack[DEPTH_OF_FILE_SCOPE].
                       ss_list_instantiation_insert_point = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      declaration(/*function_definition_allowed=*/TRUE,
                  /*extern_implied=*/FALSE, /*is_old_style_param_decl=*/FALSE,
                  (a_param_id_ptr)NULL);
    } while (curr_token != tok_end_of_source);
  }  /* if */
  /* Do any end-of-translation unit pragma processing that may be required. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* First reset the point for instantiations to NULL. */
  scope_stack[DEPTH_OF_FILE_SCOPE].
                       ss_list_instantiation_insert_point = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  process_pragmas_at_end_of_source();
}  /* translation_unit */


#if INSTANTIATION_BY_IMPLICIT_INCLUSION
void scan_implicitly_included_template_definition_file(void)
/*
Scan an implicitly included template definition file.  It is just like
scanning a translation-unit, except there's no diagnostic on the empty file.
*/
{
  (void)get_token();
  while (curr_token != tok_end_of_source) {
    declaration(/*function_definition_allowed=*/TRUE,
                /*extern_implied=*/FALSE, /*is_old_style_param_decl=*/FALSE,
                (a_param_id_ptr)NULL);
  }  /* if */
}  /* scan_implicitly_included_template_definition_file */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
