/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2010 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

overload.c -- Expression processing overload resolution.

*/

/* Header files common to all files. */
#include "fe_common.h"
/* Header files used by files involved in expression processing. */
#include "expr_hdrs.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

#include "trans_corresp.h"
#include "func_def.h"

/* Forward declarations required because of out-of-order references. */
static void try_conversion_function_match(
                            an_operand               *source_operand,
                            a_type_ptr               dest_type,
                            a_type_ptr               requested_type,
                            a_builtin_type_kind_set  builtin_types_allowed,
                            a_boolean                need_lvalue_result,
                            a_boolean                is_copy_initialization,
                            a_boolean                is_reference_binding,
                            a_candidate_function_ptr *candidate_functions);
static void prep_conversion_operand(
                                 an_operand        *source_operand,
                                 a_type_ptr        dest_type,
                                 a_boolean         *is_transparent,
                                 a_conv_descr      *conversion,
                                 a_boolean         initializing_return_value,
                                 a_boolean         is_copy_initialization,
                                 a_boolean         orig_is_copy_initialization,
                                 a_boolean         nontype_template_arg,
                                 an_error_code     incompatible_err,
                                 a_source_position *err_pos);
static a_boolean type_matches_type_code(a_type_ptr type,
                                        char       type_code);
static a_boolean microsoft_can_bind_ref_to_rvalue(an_operand *operand);
static a_boolean adjust_deduction_pair(
                                    a_type_ptr           *p_param_type,
                                    a_type_ptr           *p_arg_type,
                                    an_operand           *arg_operand,
                                    a_template_param_ptr templ_params,
                                    a_template_arg_ptr   template_arg_list,
                                    a_type_ptr           *qc_param_type,
                                    a_type_ptr           *qc_arg_type,
                                    a_boolean            *consider_nondeduced);


#if DEBUG
static unsigned long
		overload_level;
			/* Number of levels of overload resolution
			   underway. */ 
#endif /* DEBUG */


/*
Return TRUE if the indicated symbol is invisible because it was
declared in a friend declaration and not confirmed with an explicit
declaration, when friend injection is turned off.
*/
#define symbol_is_invisible_friend(sym) \
  ((sym)->is_invisible && !(sym)->is_class_member)


void clear_conv_descr(a_conv_descr_ptr conv)
/*
Clear a conversion description.
*/
{
  conv->routine                        = NULL;
  conv->routine_symbol                 = NULL;
  conv->class_identity_or_bitwise_copy = FALSE;
  conv->result_is_an_lvalue            = FALSE;
  conv->unusable                       = FALSE;
  conv->class_object_adjustment_required = FALSE;
  conv->conversion_for_direct_reference_binding = FALSE;
  conv->copy_initialization_done_as_direct = FALSE;
  conv->user_conversion_for_class_copy_must_be_determined = FALSE;
  conv->unknown_dependent_conversion   = FALSE;
  conv->is_explicit_cast               = FALSE;
  clear_std_conv_descr(&conv->std);
}  /* clear_conv_descr */


static a_boolean symbol_is_member_of_nonreal_class(a_symbol_ptr sym)
/*
Return TRUE if the given symbol is a member of a nonreal class.
*/
{
  a_boolean is_nonreal = FALSE;

  reduce_projection_symbol_to_fundamental_symbol(sym);
  if (sym->is_class_member) {
    a_type_ptr parent = sym_parent_class(sym);
    check_assertion(is_immediate_class_type(parent));
    if (parent->variant.class_struct_union.is_nonreal_class) {
      is_nonreal = TRUE;
    }  /* if */
  }  /* if */
  return is_nonreal;
}  /* symbol_is_member_of_nonreal_class */


a_symbol_ptr find_addr_of_overloaded_function_match(
                                a_symbol_ptr       ovl_sym,
                                a_boolean          is_template_id,
                                a_template_arg_ptr template_arg_list,
                                a_boolean          source_is_lvalue,
                                a_type_ptr         dest_type,
                                a_boolean          is_cast,
                                a_boolean          is_static_cast,
                                an_arg_match_level *match_level,
                                a_std_conv_descr   *std_conv,
                                a_boolean          *reinterpret_semantics,
                                a_boolean          *unknown_dependent_function,
                                a_boolean          *ambiguous)
/*
ovl_sym is the symbol from an indefinite function operand representing
the address of an overloaded function.  (For completeness, ovl_sym
can be a simple function or a template).  is_template_id is TRUE if ovl_sym
is followed by an explicit template argument list, in which case
template_arg_list gives the argument list.  The source is an lvalue
(function designator) if source_is_lvalue is TRUE, an rvalue (pointer
or pointer to member) otherwise.  The indefinite function is being
converted to a destination type dest_type.  If dest_type is a
pointer, reference, or pointer-to-member type that could be a
pointer/reference to one of the overloaded functions, return a pointer
to that function's symbol (possibly a projection symbol); otherwise,
return NULL.  Also set *match_level to indicate whether or not any
conversion is needed after the coercion to a specific function pointer
and set *std_conv to indicate any such conversion.  If std_conv == NULL,
do not attempt any conversions.  If the function cannot be determined
because some template-dependent types are involved (in a prototype
instantiation), return NULL and *unknown_dependent_function TRUE.
If more than one function matches, return NULL and *ambiguous TRUE.
See WP [over.over], and ARM 13.3, "Address of Overloaded Function".  If
is_cast is TRUE, this disambiguation is being done via an explicit
cast; if so, then is_static_cast indicates whether the cast is a
static_cast (TRUE) or an old-style or functional-notation cast (FALSE).
If reinterpret_semantics is non-NULL, *reinterpret_semantics is returned
TRUE if a reinterpret_cast is needed to convert the function to the
destination type (this comes up in a Microsoft-mode extension).
*/
{
  a_boolean        is_ptr = FALSE, is_ref = FALSE, is_ptr_to_member = FALSE;
  a_boolean        is_ref_to_const = FALSE, is_rvalue_ref = FALSE;
  a_boolean        sym_is_list, need_templates_pass;
  a_boolean        dest_type_has_type_qualifiers = FALSE;
  a_type_ptr       routine_type, dest_class, ptr_routine_type;
  a_type_ptr       dest_underlying_type;
  a_symbol_ptr     sym, proj_sym, match_sym = NULL;
  unsigned long    number_of_matches = 0;
  a_std_conv_descr std_conversion;
  a_boolean        exception_spec_checked = FALSE;
  a_boolean        is_new_template_instance;

  db_enter(4, "find_addr_of_overloaded_function_match");
  if (std_conv != NULL) clear_std_conv_descr(std_conv);
  *ambiguous = FALSE;
  *unknown_dependent_function = FALSE;
  if (reinterpret_semantics != NULL) *reinterpret_semantics = FALSE;
  if (is_template_dependent_context() &&
      (is_template_dependent_type(dest_type) ||
       (is_template_id &&
        template_arg_list_is_dependent(template_arg_list)) ||
       symbol_is_member_of_nonreal_class(ovl_sym))) {
    /* The destination type is not fully known, or the template argument
       list contains template-dependent types or the function is a member
       of a nonreal class. */
    *unknown_dependent_function = TRUE;
  } else if (is_pointer_type(dest_type)) {
    dest_class = NULL;
    is_ptr = TRUE;
    dest_underlying_type = type_pointed_to(dest_type);
  } else if (is_reference_type(dest_type)) {
    dest_class = NULL;
    is_ref = TRUE;
    is_rvalue_ref = is_rvalue_reference_type(dest_type);
    dest_underlying_type = type_pointed_to(dest_type);
    is_ref_to_const = is_const_qualified_type(dest_underlying_type);
  } else if (is_ptr_to_member_type(dest_type)) {
    dest_class = pm_class_type(dest_type);
    is_ptr_to_member = TRUE;
    dest_underlying_type = pm_member_type(dest_type);
  }  /* if */
  if (is_ptr || is_ref || is_ptr_to_member) {
    /* dest_type is a pointer, reference, or pointer-to-member type.
       The underlying type is not necessarily a function type.  (For
       one thing, a pointer to void can be made to match any function
       type.  Of course, that case is always ambiguous.) */
    dest_type_has_type_qualifiers = is_qualified_type(dest_underlying_type);
    dest_underlying_type = skip_typerefs(dest_underlying_type);
    reduce_projection_symbol_to_fundamental_symbol(ovl_sym);
    if (ovl_sym->kind == (a_symbol_kind)sk_function_template) {
      /* A single function template represents multiple instantiations of
         that template. */
      sym_is_list = FALSE;
    } else if (ovl_sym->kind == (a_symbol_kind)sk_member_function ||
               ovl_sym->kind == (a_symbol_kind)sk_routine) {
      /* A single non-overloaded function. */
      sym_is_list = FALSE;
    } else {
#if CHECKING
      if (ovl_sym->kind != (a_symbol_kind)sk_overloaded_function) {
        internal_error("find_addr_of_overloaded_function_match: not function");
      }  /* if */
#endif /* CHECKING */
      /* A list of overloaded functions. */
      sym_is_list = TRUE;
      ovl_sym = ovl_sym->variant.overloaded_function.symbols;
    }  /* if */
    /* Check each function in the overload set to see if its type matches
       the one desired.  The algorithm is as follows:
         (1)  Look for an exact match on a non-template.
         (2)  Look for a function template that can yield a function with
              exactly the right type.
         (3)  Look for a match involving a conversion (this is possible only
              for pointers-to-members, because there are no implicit
              conversions defined on pointers).
       If there is more than one match at any level, the operation is
       ambiguous.  That's probably possible only when function templates
       are involved. */
    need_templates_pass = FALSE;
    if (is_ref && !is_rvalue_ref && !source_is_lvalue) {
      /* The source has already been converted to a pointer (e.g., &f) or
         pointer to member (e.g., &A::f), so an lvalue reference can't bind
         directly to it.  need_templates_pass is left FALSE to suppress the
         template loop as well.  Some match may still be possible via a
         conversion, for a reference to const.  That's checked below. */
    } else if (is_rvalue_ref && source_is_lvalue && !is_cast) {
      /* Similar case for rvalue references -- they can't bind to an lvalue
         (but a static_cast to an rvalue reference type can). */
    } else if (is_template_id) {
      /* There is an explicit template argument list, so do not look
         for exact matches on non-templates. */
      need_templates_pass = TRUE;
    } else {
      /* Check for an exact match on a non-template. */
      for (proj_sym = ovl_sym;
           proj_sym != NULL;
           proj_sym = (sym_is_list ? proj_sym->next : NULL)) {
        /* Remove projections added for namespaces, if any. */
        sym = fundamental_symbol_of(proj_sym);
        if (symbol_is_invisible_friend(proj_sym)) {
          /* Ignore invisible symbols from friend declarations. */
        } else if (sym->kind == (a_symbol_kind)sk_function_template) {
          /* Function template.  Ignore on this pass, but enable a second pass
             to try matching it. */
          need_templates_pass = TRUE;
        } else {
          /* Not a function template (i.e., a normal function). */
          routine_type = routine_symbol_type(sym);
          /* Note that the type qualifiers on both types have already been
             dropped. */
          if (identical_types(routine_type, dest_underlying_type)) {
            a_type_ptr	parent_class_if_nonstatic_member;
            parent_class_if_nonstatic_member =
                    routine_type_is_nonstatic_member_function(routine_type) ?
                                                 sym_parent_class(sym) : NULL;
            if (f_same_entities(dest_class,
                                parent_class_if_nonstatic_member)) {
              /* Exact match. */
              match_sym = proj_sym;
              *match_level = aml_exact;
              number_of_matches++;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
    if (number_of_matches == 0 && need_templates_pass &&
        is_function_type(dest_underlying_type)) {
      a_partial_order_candidate_ptr candidate_list = NULL;
      /* Try matching function templates. */
      for (proj_sym = ovl_sym;
           proj_sym != NULL;
           proj_sym = (sym_is_list ? proj_sym->next : NULL)) {
        /* Remove projections added for namespaces, if any. */
        sym = fundamental_symbol_of(proj_sym);
        if (symbol_is_invisible_friend(proj_sym)) {
          /* Ignore invisible symbols from friend declarations. */
        } else if (sym->kind == (a_symbol_kind)sk_function_template) {
          /* Function template. */
          if (has_matching_template_function(sym, dest_underlying_type,
                                             template_arg_list,
                                             /*is_decl_context=*/FALSE)) {
            /* This template can generate an instance of the appropriate
               type.  Add the matching template to a list of matching
               candidates. */
            add_to_partial_order_candidates_list(&candidate_list, sym,
                                                 (a_template_arg_ptr)NULL);
          }  /* if */
        }  /* if */
      }  /* for */
      if (candidate_list != NULL) {
        /* If any of the templates matched, select the best one using
           the partial ordering rules. */
        a_boolean	   templ_ambiguous;
        a_template_arg_ptr templ_arg_list;

        select_best_partial_order_candidate(
                           candidate_list, (a_symbol_ptr)NULL, &sym,
                           &templ_arg_list, &templ_ambiguous);
        if (templ_ambiguous) {
          number_of_matches = 2;
        } else {
          /* Generate a partial instantiation of the matching instance. */
          match_sym = matching_template_function(
                                            sym, dest_underlying_type,
                                            template_arg_list,
                                            is_template_id,
                                            /*is_decl_context=*/FALSE,
                                            /*in_class_specialization=*/FALSE,
                                            &is_new_template_instance);
          *match_level = aml_exact;
          number_of_matches = 1;
        }  /* if */
      }  /* if */
    }  /* if */
    if (number_of_matches == 0 && std_conv != NULL &&
        (!is_ref ||
         (is_rvalue_ref ? (!source_is_lvalue || is_cast) : is_ref_to_const))) {
      /* Try matches involving an implicit conversion.  This is here
         primarily for the pointer-to-member case, but it makes sense to
         handle the normal pointer case too in case the implicit conversion
         rules change (also, it makes the error message clearer in the
         case where dest_type is "void *").  In addition, for reference-to-
         const cases the function-to-pointer or function-to-pointer-to-
         member decay can be done to get a match.  For rvalue reference
         cases, the source must already be an rvalue, i.e., be in "&f"
         form. */
      a_type_ptr match_routine_type = NULL;
      a_type_ptr eff_dest_type = dest_type;
      if (is_ref) eff_dest_type = type_pointed_to(eff_dest_type);
      for (proj_sym = ovl_sym;
           proj_sym != NULL;
           proj_sym = (sym_is_list ? proj_sym->next : NULL)) {
        /* Remove projections added for namespaces, if any. */
        sym = fundamental_symbol_of(proj_sym);
        routine_type = NULL;
        if (symbol_is_invisible_friend(proj_sym)) {
          /* Ignore invisible symbols from friend declarations. */
        } else if (sym->kind == (a_symbol_kind)sk_function_template) {
          /* Template.  If we have an explicit template argument list that
             selects a unique instance, use that. */
          if (is_template_id) {
            a_template_arg_ptr new_template_arg_list;
            routine_type =
                 explicit_arg_list_identifies_specialization(
                                                       sym,
                                                       template_arg_list,
                                                       &new_template_arg_list);
            /* If there is a match, the original template_arg_list is used
               later to create the template instance, not the new list returned
               above.  This is necessary because matching_template_function
               (called below) requires an untransformed template argument
               list (i.e., one in which any arg_operands have not yet been
               converted to constants). */
            if (new_template_arg_list != NULL) {
              /* Discard the new list if one was returned. */
              free_template_arg_list(new_template_arg_list);
              new_template_arg_list = NULL;
            }  /* if */
          }  /* if */
          if (routine_type == NULL) {
            /* The template could be converted to "void *", but that would
               always be ambiguous. */
            if (is_ptr && is_void_type(dest_underlying_type)) {
              goto is_ambiguous;
            }  /* if */
          }  /* if */
        } else if (is_template_id) {
          /* There is an explicit template argument list, so do not consider
             non-templates. */
        } else {
          /* Not a function template (i.e., a normal function). */
          routine_type = routine_symbol_type(sym);
        }  /* if */
        if (routine_type != NULL) {
          if (routine_type_is_nonstatic_member_function(routine_type)) {
            /* The class of the pointer to member is always the class in
               which the function is defined, not any derived class
               indicated in the projection symbol. */
            ptr_routine_type = ptr_to_member_type(routine_type,
                                                  sym_parent_class(sym));
          } else {
            ptr_routine_type = make_pointer_type(routine_type);
          }  /* if */
          /* See if the type of the function can be converted to the required
             destination type.  For the explicit cast case, use
             static_cast_conversion_possible to find cases of a
             pointer-to-member of a derived class cast to a pointer-to-member
             of a base class.  expl_conversion_possible would be too broad
             because it would also allow changing the member type. */
          if (ptr_routine_type != NULL) {
            a_boolean is_match = FALSE;
            clear_std_conv_descr(&std_conversion);
            if (!is_cast) {
              if (impl_conversion_possible(ptr_routine_type,
                                           /*source_is_constant=*/FALSE,
                                           /*source_is_string_literal=*/FALSE,
                                           (a_constant_ptr)NULL,
                                           eff_dest_type,
                                      /*allow_qualifier_or_eh_mismatch=*/FALSE,
                                           /*suppress_extensions=*/FALSE,
                                           ec_no_error,
                                           &std_conversion)) {
                is_match = TRUE;
              }  /* if */
            } else if (static_cast_conversion_possible(
                                     ptr_routine_type,
                                     /*source_is_constant=*/FALSE,
                                     /*source_is_string_literal=*/FALSE,
                                     (a_constant_ptr)NULL,
                                     eff_dest_type,
                                     /*allow_qualifier_or_eh_mismatch=*/FALSE,
                                     ec_no_error,
                                     &std_conversion.warning_suggested)) {
              is_match = TRUE;
            } else if (microsoft_mode && !is_static_cast) {
              /* Microsoft allows a conversion that changes the implicit this
                 class type but not the parameter types of a pointer-to-member
                 function if the cast used is old-style or
                 functional-notation. */
              a_boolean qualifiers_added;
              if (is_ptr_to_member_type(ptr_routine_type) &&
                  is_ptr_to_member_type(eff_dest_type) &&
                  member_types_correspond(pm_member_type(eff_dest_type),
                                          pm_member_type(ptr_routine_type),
                                      /*allow_qualifier_or_eh_mismatch=*/FALSE,
                                          &qualifiers_added) &&
                  !qualifiers_added) {
                is_match = TRUE;
                std_conversion.nontrivial_conversion = TRUE;
                if (reinterpret_semantics != NULL) {
                  *reinterpret_semantics = TRUE;
                }  /* if */
              }  /* if */
            }  /* if */
            if (is_match) {
              /* A match. */
              match_sym = proj_sym;
              match_routine_type = routine_type;
              *match_level = aml_std_conversion;
              *std_conv = std_conversion;
              number_of_matches++;
              exception_spec_checked = TRUE;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* for */
      if (number_of_matches == 1) {
        if (is_template_id) {
          /* Make the template instance for the best match. */
          match_sym = matching_template_function(
                                            match_sym, match_routine_type,
                                            template_arg_list,
                                            is_template_id,
                                            /*is_decl_context=*/FALSE,
                                            /*in_class_specialization=*/FALSE,
                                            &is_new_template_instance);
        }  /* if */
      }  /* if */
    }  /* if */
    if (number_of_matches > 1) {
is_ambiguous:
      /* Ambiguous case. */
      *ambiguous = TRUE;
      match_sym = NULL;
    }  /* if */
  }  /* if */
  if (match_sym != NULL && std_conv != NULL) {
    /* If the pointer type we converted to has extra type qualifiers,
       set the tie-breaker flag in the standard conversion description.
       The flag might also have been set by the call of
       impl_conversion_possible. */
    if (dest_type_has_type_qualifiers) std_conv->type_qualifiers_added = TRUE;
    if (!exception_spec_checked) {
      sym = fundamental_symbol_of(match_sym);
      routine_type = routine_symbol_type(sym);
      if (!exception_spec_conversion_possible(routine_type,
                                              dest_underlying_type)) {
        /* The exception specifications can't be converted. */
        std_conv->exception_spec_incompatibility = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 4 || db_flag_is_set("overload")) {
    if (match_sym == NULL) {
      fprintf(f_debug, "find_addr_of_overloaded_function_match: %s\n",
              *ambiguous ? "ambiguous" : "no match");
    } else {
      db_symbol(match_sym, "find_addr_of_overloaded_function_match: ", 2);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return match_sym;
}  /* find_addr_of_overloaded_function_match */


void choose_function_and_make_address_constant(
                                             a_symbol_ptr   sym,
                                             a_boolean      is_template_id,
                                             a_template_arg *template_arg_list,
                                             a_type_ptr     guide_type,
                                             a_constant_ptr constant,
                                             a_boolean      *err)
/*
As part of template argument substitution, select the instance of sym
that will produce a pointer or pointer to member that matches guide_type.
If that can be done, return a constant for the address of that
function in *constant.  Otherwise, return *err TRUE.  If is_template_id
is TRUE, template_arg_list is a set of explicit template arguments for sym.
*/
{
  a_symbol_ptr       func_sym;
  an_arg_match_level match_level;
  a_std_conv_descr   std_conversion;
  a_boolean          unknown_dependent_function, ambiguous;

  func_sym = find_addr_of_overloaded_function_match(
                                                   sym,
                                                   is_template_id,
                                                   template_arg_list,
                                                   /*source_is_lvalue=*/TRUE,
                                                   guide_type,
                                                   /*is_cast=*/FALSE,
                                                   /*is_static_cast=*/FALSE,
                                                   &match_level,
                                                   &std_conversion,
                                                   (a_boolean *)NULL,
                                                   &unknown_dependent_function,
                                                   &ambiguous);
  if (func_sym == NULL ||
      !conversion_allowed_for_nontype_template_argument(&std_conversion) ||
      std_conversion.exception_spec_incompatibility) {
    *err = TRUE;
  } else {
    a_routine_ptr routine;
    /* Remove projections for namespaces, if any. */
    func_sym = fundamental_symbol_of(func_sym);
    check_assertion(func_sym->kind == (a_symbol_kind)sk_routine ||
                    func_sym->kind == (a_symbol_kind)sk_member_function);
    /* Build a pointer-to-function or pointer-to-member constant. */
    routine = func_sym->variant.routine.ptr;
    if (routine_type_is_nonstatic_member_function(routine->type)) {
      /* Nonstatic member function: pointer to member. */
      set_ptr_to_member_function_constant(routine, constant);
    } else {
      /* Static member function: simple pointer to function. */
      set_routine_address_constant(routine, constant,
                                   /*set_address_taken_flag=*/TRUE);
    }  /* if */
  }  /* if */
}  /* choose_function_and_make_address_constant */


static a_type_ptr arg_type_for_unique_specialization(a_type_ptr arg_type,
                                                     an_operand *operand)
/*
Helper routine used when calling explicit_arg_list_identifies_specialization.
Converts the function type returned by that function (arg_type) into a
pointer or pointer-to-member type as appropriate.  operand, if non-NULL,
is the source indefinite function operand.
*/
{
  if (operand == NULL || !is_a_function_designator(operand)) {
    /* The operand is not a function designator, so make a pointer or
       pointer to member as the argument type. */
    if (routine_type_is_nonstatic_member_function(arg_type)) {
      arg_type = ptr_to_member_type(
                          arg_type,
                          skip_typerefs(
                            arg_type->variant.routine.extra_info->this_class));
    } else {
      arg_type = make_pointer_type(arg_type);
    }  /* if */
  }  /* if */
  return arg_type;
}  /* arg_type_for_unique_specialization */


static
a_boolean indefinite_function_can_be_template_arg(
                                   an_operand           *operand,
                                   a_type_ptr           param_type,
                                   a_type_ptr           *arg_type,
                                   a_template_param_ptr templ_params,
                                   a_template_arg_ptr   template_arg_list)
/*
operand is an indefinite function operand.  See if it can be matched against
a parameter of type param_type from a function template.  If so, return TRUE
and set *arg_type to the argument type to use.  *arg_type is not changed if
this function returns FALSE.  templ_params describes the template parameter
list of the function template associated with param_type, or it has a single
entry representing the "auto" type when handling an "auto" type specifier.
template_arg_list is used in some nonstandard modes to introduce knowledge
from previous arguments; in the standard case, it is always NULL.
*/
{
  a_boolean    can_be_arg = FALSE;
  a_symbol_ptr sym = operand->symbol, proj_sym;
  a_type_ptr   matching_arg_type = NULL;

  reduce_projection_symbol_to_fundamental_symbol(sym);
  if (sym->kind == (a_symbol_kind)sk_function_template) {
    /* A template with explicit arguments can be made to match if there's
       only one possible instance. */
    if (operand->is_template_id) {
      a_template_arg_ptr new_arg_list;
      matching_arg_type =
        explicit_arg_list_identifies_specialization(operand->symbol,
                                                    operand->template_arg_list,
                                                    &new_arg_list);
      if (matching_arg_type != NULL) {
        can_be_arg = TRUE;
        matching_arg_type = arg_type_for_unique_specialization(
                                                   matching_arg_type, operand);
      }  /* if */
    }  /* if */
  } else {
    check_assertion(sym->kind == (a_symbol_kind)sk_overloaded_function);
    for (proj_sym = sym->variant.overloaded_function.symbols;
         proj_sym != NULL;
         proj_sym = proj_sym->next) {
      a_type_ptr routine_type, ptr_routine_type;
      a_boolean  matches = FALSE;
      /* Remove projections for namespaces, if any. */
      sym = fundamental_symbol_of(proj_sym);
      if (sym->kind == (a_symbol_kind)sk_function_template) {
        /* A template with explicit arguments can be made to match if there's
           only one possible instance. */
        if (operand->is_template_id) {
          a_template_arg_ptr new_arg_list;
          routine_type = explicit_arg_list_identifies_specialization(
                                                    sym,
                                                    operand->template_arg_list,
                                                    &new_arg_list);
          if (routine_type != NULL) {
            matches = TRUE;
            ptr_routine_type = arg_type_for_unique_specialization(
                                                           routine_type,
                                                           (an_operand *)NULL);
          }  /* if */
        } else {
          /* If an overload set contains any function templates (and the
             operand is not a template-id), the parameter is considered a
             nondeduced context. */
          can_be_arg = FALSE;
          break;
        }  /* if */
      } else {
        /* Not a function template. */
        /* This can't match if there are explicit template arguments. */
        if (!operand->is_template_id) {
          /* See if this function can be made to match the parameter type. */
          a_type_ptr         local_arg_type, local_param_type;
          a_template_arg_ptr eff_template_arg_list = NULL;
          if (gpp_mode || microsoft_mode) {
            /* Microsoft and GNU allow information to leak in from deduction
               on previous arguments. */
            eff_template_arg_list = template_arg_list;
          }  /* if */
          routine_type = routine_symbol_type(sym);
          local_arg_type = routine_type;
          local_param_type = param_type;
          if (adjust_deduction_pair(&local_param_type, &local_arg_type,
                                    (an_operand *)NULL, templ_params,
                                    (a_template_arg *)NULL,
                                    (a_type_ptr*)NULL, (a_type_ptr*)NULL,
                                    (a_boolean *)NULL) &&
              tentatively_matches_template_type(local_arg_type,
                                                local_param_type,
                                                templ_params,
                                                eff_template_arg_list)) {
            matches = TRUE;
            ptr_routine_type = arg_type_for_unique_specialization(
                                                           routine_type,
                                                           (an_operand *)NULL);
          }  /* if */
        }  /* if */
      }  /* if */
      if (matches) {
        /* This function matches.  Only one is allowed to match, so if
           a previous one matched, the overall match fails. */ 
        if (can_be_arg) {
          can_be_arg = FALSE;
          break;
        } else {
          can_be_arg = TRUE;
          /* For the argument type, use a pointer or the function type itself
             according to what the original operand is. */
          matching_arg_type = is_a_function_designator(operand) ?
                                           routine_type : ptr_routine_type;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  if (can_be_arg) *arg_type = matching_arg_type;
  return can_be_arg;
}  /* indefinite_function_can_be_template_arg */


static void clear_arg_match_summary(an_arg_match_summary_ptr amsp)
/*
Clear the fields of the indicated argument match summary entry to default
values.
*/
{
  amsp->next                       = NULL;
  amsp->match_level                = aml_none;
  amsp->anachronism_used           = FALSE;
  amsp->tiebreaker_anachronism_used= FALSE;
  amsp->const_anachronism          = FALSE;
  amsp->is_match_for_this_param    = FALSE;
  amsp->arg_is_constant            = FALSE;
  amsp->lvalue_to_rvalue_conversion_used = FALSE;
  amsp->param_type                 = NULL;
  amsp->guide_type                 = NULL;
  clear_conv_descr(&amsp->conversion);
  amsp->template_symbol            = NULL;
}  /* clear_arg_match_summary */


static an_arg_match_summary_ptr alloc_arg_match_summary(void)
/*
Allocate an argument match summary entry and return a pointer to it.
Such entries are used in resolving calls to overloaded functions.
*/
{
  an_arg_match_summary_ptr amsp;

  if (avail_arg_match_summaries != NULL) {
    /* Reuse a previously-freed entry. */
    amsp = avail_arg_match_summaries;
    avail_arg_match_summaries = amsp->next;
  } else {
    /* Allocate a new entry. */
    amsp = (an_arg_match_summary_ptr)alloc_fe(sizeof(an_arg_match_summary));
#if DEBUG
    num_arg_match_summaries_allocated++;
#endif /* DEBUG */
  }  /* if */
  clear_arg_match_summary(amsp);
  return amsp;
}  /* alloc_arg_match_summary */


void free_arg_match_summary_list(an_arg_match_summary_ptr amsp)
/*
Free the list of argument match summary entries pointed to by amsp.
*/
{
  an_arg_match_summary_ptr amsp_next;

  for (; amsp != NULL; amsp = amsp_next) {
    amsp_next = amsp->next;
    /* Add the entry to the available list. */
    amsp->next = avail_arg_match_summaries;
    avail_arg_match_summaries = amsp;
  }  /* for */
}  /* free_arg_match_summary_list */

#if DEBUG

static void db_arg_match_summary(an_arg_match_summary_ptr amsp)
/*
Print an argument match summary for debug purposes.
*/
{
  char             *str;
  a_base_class_ptr bcp;

  switch (amsp->match_level) {
    case aml_exact:             str = "exact";               break;
    case aml_promotion:         str = "promotion";           break;
    case aml_std_conversion:    str = "std conversion";      break;
    case aml_user_conversion:   str = "user conversion";     break;
    case aml_ellipsis:          str = "ellipsis";            break;
    case aml_error:             str = "error";               break;
    case aml_none:              str = "none";                break;
    default:                    str = "**BAD MATCH LEVEL**";
  }  /* if */
  fprintf(f_debug, "match level = %s", str);
  if (amsp->const_anachronism) {
    fprintf(f_debug, " (const anachronism)");
  } else if (amsp->anachronism_used) {
    fprintf(f_debug, " (anachronism used)");
  } else if (amsp->tiebreaker_anachronism_used) {
    fprintf(f_debug, " (tiebreaker anachronism used)");
  }  /* if */
  if (amsp->match_level == aml_user_conversion &&
      amsp->conversion.std.nontrivial_conversion) {
    if (amsp->conversion.std.promotion) {
      fprintf(f_debug, " (plus promotion)");
    } else {
      fprintf(f_debug, " (plus conversion)");
    }  /* if */
  }  /* if */
  if (amsp->lvalue_to_rvalue_conversion_used) {
    fprintf(f_debug, " (lvalue-to-rvalue conv)");
  }  /* if */
  if (amsp->conversion.std.type_qualifiers_added) {
    fprintf(f_debug, " (type qualifiers added)");
  }  /* if */
  if (amsp->conversion.std.conv_of_string_literal_to_ptr_to_nonconst) {
    fprintf(f_debug, " (const string conv anachronism)");
  }  /* if */
  bcp = amsp->conversion.std.cast_base_class;
  if (bcp != NULL) {
    fprintf(f_debug, ", base class ");
    db_abbreviated_base_class(bcp);
  }  /* if */
  fprintf(f_debug, "\n");
}  /* db_arg_match_summary */

#endif /* DEBUG */

static a_candidate_function_ptr alloc_candidate_function(void)
/*
Allocate a candidate function entry and return a pointer to it.  Such entries
are used in resolving calls to overloaded functions.
*/
{
  a_candidate_function_ptr cfp;

  if (avail_candidate_functions != NULL) {
    /* Reuse a previously-freed entry. */
    cfp = avail_candidate_functions;
    avail_candidate_functions = cfp->next;
  } else {
    /* Allocate a new entry. */
    cfp = (a_candidate_function_ptr)alloc_fe(sizeof(a_candidate_function));
#if DEBUG
    num_candidate_functions_allocated++;
#endif /* DEBUG */
  }  /* if */
  cfp->next = NULL;
  cfp->function_symbol = NULL;
  cfp->is_function_template = FALSE;
  cfp->expl_template_arg_list_used = FALSE;
  cfp->template_arg_list = NULL;
  cfp->operand_type_pattern = NULL;
  cfp->surrogate_function_conv_sym = NULL;
  cfp->uses_microsoft_explicit_anachronism = FALSE;
  cfp->is_user_conversion = FALSE;
  clear_conv_descr(&cfp->conversion);
  cfp->specific_type = NULL;
  cfp->arg_matches = NULL;
  cfp->current_arg_match = NULL;
  cfp->next_in_arg_best_match_set = NULL;
  cfp->in_best_match_set = FALSE;
  cfp->in_best_match_set_for_some_argument = FALSE;
  cfp->in_best_match_set_for_curr_argument = FALSE;
#if BACK_END_IS_CP_GEN_BE
  cfp->found_through_adl = FALSE;
#endif /* BACK_END_IS_CP_GEN_BE */
  return cfp;
}  /* alloc_candidate_function */


static void free_candidate_function_list(a_candidate_function_ptr cfp)
/*
Free the list of candidate function entries pointed to by cfp.
*/
{
  a_candidate_function_ptr cfp_next;

  for (; cfp != NULL; cfp = cfp_next) {
    cfp_next = cfp->next;
    /* Free the template argument list if any. */
    free_template_arg_list(cfp->template_arg_list);
    /* Free the argument match entries if any. */
    free_arg_match_summary_list(cfp->arg_matches);
    /* arg_operand_list is deliberately not freed, because it is shared
       with any other candidate function entries for templates. */
    /* Add the entry to the available list. */
    cfp->next = avail_candidate_functions;
    avail_candidate_functions = cfp;
  }  /* for */
}  /* free_candidate_function */

#if DEBUG

static void db_candidate_function(a_candidate_function_ptr cfp)
/*
Print a candidate function entry for debugging purposes.
*/
{
  unsigned long            narg;
  an_arg_match_summary_ptr amsp;

  if (cfp->function_symbol != NULL) {
    /* Normal function case. */
    db_symbol(cfp->function_symbol, "", 2);
  } else if (cfp->surrogate_function_conv_sym != NULL) {
    fprintf(f_debug, "surrogate function, conv = ");
    db_symbol(cfp->surrogate_function_conv_sym, "", 2);
  } else {
    /* Built-in operator case. */
    fprintf(f_debug, "Built-in %s", cfp->operand_type_pattern);
    if (cfp->specific_type != NULL) {
      fprintf(f_debug, ", specific_type = ");
      db_abbreviated_type(cfp->specific_type);
    }  /* if */
    fprintf(f_debug, "\n");
  }  /* if */
  if (cfp->is_function_template) {
    fprintf(f_debug, "(function template)\n");
  }  /* if */
  /* Display the arg match list. */
  narg = 0;
  for (amsp = cfp->arg_matches; amsp != NULL; amsp = amsp->next) {
    if (amsp->is_match_for_this_param) {
      fprintf(f_debug, "  this:  ");
    } else {
      fprintf(f_debug, "  arg %lu: ", ++narg);
    }  /* if */
    db_arg_match_summary(amsp);
  }  /* for */
}  /* db_candidate_function */

#endif /* DEBUG */
#if DEBUG

static void db_candidate_function_list(a_candidate_function_ptr cfp_list)
/*
Print a candidate function entry list for debugging purposes.
*/
{
  a_candidate_function_ptr cfp;

  fprintf(f_debug, "Candidate functions list:");
  if (cfp_list == NULL) {
    fprintf(f_debug, " NULL\n");
  } else {
    fprintf(f_debug, "\n");
    for (cfp = cfp_list; cfp != NULL; cfp = cfp->next) {
      db_candidate_function(cfp);
    }  /* for */
  }  /* if */
}  /* db_candidate_function_list */

#endif /* DEBUG */

static void add_function_to_candidate_functions_list(
                                 a_symbol_ptr             function_symbol,
                                 an_arg_match_summary_ptr arg_matches,
                                 a_candidate_function_ptr *candidate_functions)
/*
Add the function identified by function_symbol to the front of the
candidate_functions list.  arg_matches gives information about how well
the actual arguments we have match the function's formal parameters.
*/
{
  a_candidate_function_ptr candidate;

  candidate = alloc_candidate_function();
  candidate->function_symbol = function_symbol;
  candidate->arg_matches = arg_matches;
  candidate->next = *candidate_functions;
  *candidate_functions = candidate;
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "add_function_to_candidate_functions_list: added\n"); 
    db_candidate_function(candidate);
  }  /* if */
#endif /* DEBUG */
}  /* add_function_to_candidate_functions_list */


static void add_surrogate_function_to_candidate_functions_list(
                                 a_symbol_ptr             conv_function_symbol,
                                 an_arg_match_summary_ptr arg_matches,
                                 a_candidate_function_ptr *candidate_functions)
/*
Add the surrogate function produced by the conversion indicated by
conv_function_symbol to the front of the candidate_functions list.
arg_matches gives information about how well the actual arguments we
have match the function's formal parameters.
*/
{
  a_candidate_function_ptr candidate;

  candidate = alloc_candidate_function();
  candidate->function_symbol = NULL;
  candidate->surrogate_function_conv_sym = conv_function_symbol;
  candidate->arg_matches = arg_matches;
  candidate->next = *candidate_functions;
  *candidate_functions = candidate;
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug,
            "add_surrogate_function_to_candidate_functions_list: added\n"); 
    db_candidate_function(candidate);
  }  /* if */
#endif /* DEBUG */
}  /* add_surrogate_function_to_candidate_functions_list */


static void add_builtin_operator_to_candidate_functions_list(
                                char                     *operand_type_pattern,
                                a_type_ptr               specific_type,
                                an_arg_match_summary_ptr arg_matches,
                                a_candidate_function_ptr *candidate_functions)
/*
Add the built-in operator identified by operand_type_pattern and specific_type
to the candidate_functions list.  arg_matches gives information about how well
the operands we have match the operator's required operand types.
*/
{
  a_candidate_function_ptr candidate;

  candidate = alloc_candidate_function();
  candidate->operand_type_pattern = operand_type_pattern;
  candidate->specific_type = specific_type;
  candidate->arg_matches = arg_matches;
  candidate->next = *candidate_functions;
  *candidate_functions = candidate;
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug,
            "add_builtin_operator_to_candidate_functions_list: added\n"); 
    db_candidate_function(candidate);
  }  /* if */
#endif /* DEBUG */
}  /* add_builtin_operator_to_candidate_functions_list */


static void add_function_template_to_candidate_functions_list(
                          a_symbol_ptr             function_symbol,
                          a_boolean                expl_template_arg_list_used,
                          a_template_arg_ptr       template_arg_list,
                          an_arg_match_summary_ptr arg_matches,
                          a_candidate_function_ptr *candidate_functions)
/*
Add the function template identified by function_symbol to the front of the
candidate_functions list.  If template_arg_list is non-NULL, it gives a
list of explicit template arguments.  Some part of the argument list was
explicitly specified if expl_template_arg_list_used is TRUE.
arg_matches gives information about how well the actual arguments we
have match the function's formal parameters.
*/
{
  a_candidate_function_ptr candidate;

  candidate = alloc_candidate_function();
  candidate->function_symbol = function_symbol;
  candidate->is_function_template = TRUE;
  candidate->expl_template_arg_list_used = expl_template_arg_list_used;
  candidate->template_arg_list = template_arg_list;
  candidate->arg_matches = arg_matches;
  candidate->next = *candidate_functions;
  *candidate_functions = candidate;
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug,
            "add_function_template_to_candidate_functions_list: added\n"); 
    db_candidate_function(candidate);
  }  /* if */
#endif /* DEBUG */
}  /* add_function_template_to_candidate_functions_list */


/*
Output control block used with the il_to_str routines to output argument
types for diagnostic messages.
*/
static an_il_to_str_output_control_block
		octl;


static void set_up_for_argument_type_formatting(void)
/*
Initialize the control block for the il_to_str routines before using them
to format argument types.  This setup arranges for the output to go into
temp_text_buffer.
*/
{
  clear_il_to_str_output_control_block(&octl);
  octl.output_str = put_str_to_temp_text_buffer_octl;
  octl.remove_template_typedefs = !display_template_typedefs_in_diagnostics;
  pos_in_temp_text_buffer = 0;
}  /* set_up_for_argument_type_formatting */


static void format_argument_type_for_display(a_type_ptr type)
/*
Add the indicated type to the string being built up in temp_text_buffer,
as part of producing a diagnostic for an overload resolution problem.
*/
{
  form_type(type, &octl);
}  /* format_argument_type_for_display */


void display_object_type(a_type_ptr object_type)
/*
Output a diagnostic line that displays the indicated type as the
object type, as part of producing a diagnostic for an overload
resolution problem.  The object type can be a class type or a pointer
to class type (or an error type).
*/
{
  if (is_pointer_type(object_type)) {
    object_type = type_pointed_to(object_type);
  }  /* if */
  set_up_for_argument_type_formatting();
  format_argument_type_for_display(object_type);
  put_ch_to_temp_text_buffer('\0');
  str_add_diag_info(ec_object_type_add_on, temp_text_buffer);
}  /* display_object_type */


static void display_argument_list_types(
                                   a_type_ptr         object_type,
                                   an_arg_operand_ptr arg_operand_list)
/*
Output a diagnostic line that displays the types of the arguments in
arg_operand_list, as part of producing a diagnostic for an overload
resolution problem.  object_type is the selector object type,
if there is one, or NULL otherwise; output a line giving the type if
it is provided.  The start_error or equivalent has already been done.
This routine does not call end_error.
*/
{
  an_arg_operand_ptr arg_operand;

  check_assertion(expr_stack != NULL &&
                  !expr_stack->suppress_diagnostics);
  /* Display nothing if the argument list is empty. */
  if (arg_operand_list != NULL) {
    set_up_for_argument_type_formatting();
    for (arg_operand = arg_operand_list;
         arg_operand != NULL;
         arg_operand = arg_operand->next) {
      format_argument_type_for_display(arg_operand->operand.type);
      if (arg_operand->next != NULL) {
        /* This is not the last argument, so put a comma after it. */
        put_str_to_temp_text_buffer(", ");
      }  /* if */
    }  /* for */
    put_ch_to_temp_text_buffer('\0');
    str_add_diag_info(ec_argument_list_types_add_on, temp_text_buffer);
  }  /* if */
  if (object_type != NULL) {
    display_object_type(object_type);
  }  /* if */
}  /* display_argument_list_types */


static void display_operand_types(an_arg_operand_ptr arg_operand_list,
                                  an_opname_kind     kind)
/*
Put the types of the operands (given by arg_operand_list) of an operator
(given by kind) into temp_text_buffer so they can be used in a diagnostic.
The start_error or equivalent has already been done.  This routine does not
call end_error.
*/
{
  an_arg_operand_ptr arg_operand;
  a_boolean          unary_operator;
  a_boolean          list_form;
  char               *opname = opname_names[(int)kind];
  unsigned long      num;

  check_assertion(expr_stack != NULL &&
                  !expr_stack->suppress_diagnostics);
  set_up_for_argument_type_formatting();
  /* Normal cases:
        unop type1
        type1 binop type2
     Special cases:
        type1 [ type2 ]
        type1 : type2     (used for "?")
        type1 ++          (implicit second argument of "int" not displayed)
        type1 --          (ditto)
     List form used for (), -> new, new[], delete, and delete[]:
        type1, type2, ...
  */
  list_form = (kind == (an_opname_kind)onk_function_call ||
               kind == (an_opname_kind)onk_new ||
               kind == (an_opname_kind)onk_array_new ||
               kind == (an_opname_kind)onk_arrow ||
               kind == (an_opname_kind)onk_delete ||
               kind == (an_opname_kind)onk_array_delete);
  unary_operator = (!list_form && arg_operand_list->next == NULL);
  if (unary_operator) {
    /* Unary operator precedes the operand. */
    put_str_to_temp_text_buffer(opname);
    put_ch_to_temp_text_buffer(' ');
  }  /* if */
  for (arg_operand = arg_operand_list, num = 1;
       arg_operand != NULL;
       arg_operand = arg_operand->next, num++) {
    format_argument_type_for_display(arg_operand->operand.type);
    if (list_form) {
      /* List form.  Comma after each operand except the last. */
      if (arg_operand->next != NULL) {
        put_str_to_temp_text_buffer(", ");
      }  /* if */
    } else if (num == 1) {
      /* After the first operand. */
      if (kind == (an_opname_kind)onk_subscript) {
        put_str_to_temp_text_buffer(" [ ");
      } else if (kind == (an_opname_kind)onk_question) {
        put_str_to_temp_text_buffer(" : ");
      } else if (!unary_operator) {
        /* Binary operators go between the first and second operands. */
        put_ch_to_temp_text_buffer(' ');
        put_str_to_temp_text_buffer(opname);
        /* On postfix ++, do not display the implicit second argument. */
        if (kind == (an_opname_kind)onk_plus_plus ||
            kind == (an_opname_kind)onk_minus_minus) break;
        put_ch_to_temp_text_buffer(' ');
      }  /* if */
    } else if (num == 2) {
      /* After the second operand. */
      if (kind == (an_opname_kind)onk_subscript) {
        put_str_to_temp_text_buffer(" ]");
      }  /* if */
    }  /* if */
  }  /* for */
  put_ch_to_temp_text_buffer('\0');
  str_add_diag_info(ec_operand_types_add_on, temp_text_buffer);
}  /* display_operand_types */


/*
Type codes used in type patterns that describe built-in operators
for overload resolution.
*/
#define LVALUE_FIRST_OPERAND_TYPE_CODE 'L'
			/* First operand must be a non-const lvalue. */
#define CORRESP_TYPE_CODE '='
			/* Used before two operand type codes to indicate that
			   the two operands must correspond, e.g., two pointer
			   operands must have the same pointer type. */
#define PROMOTED_INTEGRAL_TYPE_CODE 'I'
			/* Promoted integral type. */
#define INTEGRAL_TYPE_CODE 'i'
			/* Integral type. */
#define PTRDIFF_T_TYPE_CODE 'D'
			/* ptrdiff_t. */
#define ENUM_TYPE_CODE 'E'
			/* Enumerated type. */
#define PROMOTED_ARITH_TYPE_CODE 'A'
			/* Promoted arithmetic type. */
#define ARITH_TYPE_CODE 'a'
			/* Arithmetic type. */
#define POINTER_TYPE_CODE 'P'
			/* Any pointer type. */
#define POINTER_TO_OBJECT_TYPE_CODE 'O'
			/* Pointer to object type. */
#define POINTER_TO_FUNCTION_TYPE_CODE 'F'
			/* Pointer to function. */
#define PTR_TO_MEMBER_TYPE_CODE 'M'
			/* Pointer to member. */
#define BOOL_TYPE_CODE 'B'
			/* bool. */
#define BOOL_EQUIVALENT_TYPE_CODE 'b'
			/* Used instead of BOOL_TYPE_CODE when bool_is_keyword
			   is FALSE.  Specifies types that can be used in a
			   boolean controlling expression, i.e., arithmetic,
			   enum, pointer, pointer to member. */
#define CLASS_TYPE_CODE 'C'
			/* Class, struct, or union.  Note that this is used
			   for the "?" operator, and the operands are dealt
			   with in a special way, to preserve the original
			   class object if possible. */


static char *name_for_type_code(char type_code)
/*
Return a printable string describing a type code.
*/
{
  char *str;

  switch (type_code) {
    case INTEGRAL_TYPE_CODE:
    case PROMOTED_INTEGRAL_TYPE_CODE:
    case PTRDIFF_T_TYPE_CODE:
      str = "integer";
      break;
    case ENUM_TYPE_CODE:
      str = "enum";
      break;
    case ARITH_TYPE_CODE:
    case PROMOTED_ARITH_TYPE_CODE:
      str = "arithmetic";
      break;
    case POINTER_TYPE_CODE:
      str = "pointer";
      break;
    case POINTER_TO_OBJECT_TYPE_CODE:
      str = "pointer-to-object";
      break;
    case POINTER_TO_FUNCTION_TYPE_CODE:
      str = "pointer-to-function";
      break;
    case PTR_TO_MEMBER_TYPE_CODE:
      str = "pointer-to-member";
      break;
    case BOOL_TYPE_CODE:
      str = "bool";
      break;
    case BOOL_EQUIVALENT_TYPE_CODE:
      str = "bool-equivalent";
      break;
    case CLASS_TYPE_CODE:
      str = "class";
      break;
    default:
      str = "?";
      unexpected_condition_str("name_for_type_code: bad type code");
  }  /* switch */
  return str;
}  /* name_for_type_code */


/*
Return TRUE if the given symbol is ambiguous by inheritance.
This applies to projection and namespace projection symbols.
*/
#define is_ambiguous_by_inheritance(symbol) ((symbol)->ambiguous)


static a_boolean same_candidate_function(a_candidate_function_ptr cfp1,
                                         a_candidate_function_ptr cfp2);


static void diagnose_overload_ambiguity(
                             a_candidate_function_ptr candidate_functions,
                             an_operand               *bound_function_selector,
                             an_arg_operand_ptr       arg_operand_list,
                             an_opname_kind           kind)
/*
Issue the add-on diagnostics to describe an overloading ambiguity.
candidate_functions gives the list of functions in the best-match set.
arg_operand_list gives the operand list, but is NULL if the operand types
should not be listed.  bound_function_selector is the selector object,
if there is one, or NULL otherwise.  kind gives the operator associated
with any entries in the set for built-in operators.  The start_error
or equivalent has already been done, and this routine does the end_error
call.
*/
{
  a_candidate_function_ptr cfp;
  a_symbol_ptr             function_sym;
  an_error_code            err_code;

  check_assertion(expr_stack != NULL &&
                  !expr_stack->suppress_diagnostics);
  for (cfp = candidate_functions; cfp != NULL; cfp = cfp->next) {
    /* Print each candidate function. */
    function_sym = cfp->function_symbol;
    if (function_sym != NULL) {
      /* Normal function case. */
      a_candidate_function_ptr temp_cfp;
      /* Ignore a function that has appeared earlier on the list, so as
         not to put it out twice. */
      for (temp_cfp = candidate_functions;
           temp_cfp != cfp;
           temp_cfp = temp_cfp->next) {
        if (same_candidate_function(temp_cfp, cfp)) goto next_function;
      }  /* for */
      if (is_ambiguous_by_inheritance(function_sym)) {
        /* Function symbol is ambiguous by inheritance.  Use a special
           message.  This happens for conversion functions inherited
           into a derived class. */
        err_code = ec_ambiguous_by_inheritance_add_on;
      } else {
        /* Normal case. */
        err_code = ec_ambiguous_function_add_on;
        reduce_projection_symbol_to_fundamental_symbol(function_sym);
      }  /* if */
      sym_add_diag_info(err_code, function_sym);
    } else if (cfp->surrogate_function_conv_sym != NULL) {
      /* Surrogate function. */
      sym_add_diag_info(ec_surrogate_func_add_on,
                        cfp->surrogate_function_conv_sym);
    } else {
      /* Built-in operator case. */
      /* Put out something like
           built-in operator "pointer + integer"
      */
      char buf[100]; /* Big enough for
                        "pointer-to-member == pointer-to-member". */
      char *pattern = cfp->operand_type_pattern;
      char *opname = opname_names[(int)kind];
      if (pattern[1] == '\0' || pattern[1] == ';') {
        /* Unary operator. */
        (void)sprintf(buf, "%s %s", opname, name_for_type_code(pattern[0]));
      } else if (kind == (an_opname_kind)onk_subscript) {
        /* Subscript -- funny because the operator surrounds the second
           operand. */
        (void)sprintf(buf, "%s[%s]", name_for_type_code(pattern[0]),
                                     name_for_type_code(pattern[1]));
      } else if (kind == (an_opname_kind)onk_question) {
        /* ?: -- funny because the operands considered are the second and
           third. */
        (void)sprintf(buf, "expression ? %s : %s",
                                       name_for_type_code(pattern[0]),
                                       name_for_type_code(pattern[1]));
      } else {
        /* Binary operator. */
        (void)sprintf(buf, "%s %s %s", name_for_type_code(pattern[0]), opname,
                                       name_for_type_code(pattern[1]));
      }  /* if */
      str_add_diag_info(ec_builtin_operator_add_on, buf);
    }  /* if */
next_function:;
  }  /* for */
  if (arg_operand_list != NULL) {
    /* Display the operand types. */
    if (kind == (an_opname_kind)onk_none) {
      a_type_ptr object_type = NULL;
      if (bound_function_selector != NULL) {
        object_type = bound_function_selector->type;
      }  /* if */
      display_argument_list_types(object_type, arg_operand_list);
    } else {
      check_assertion(bound_function_selector == NULL);
      display_operand_types(arg_operand_list, kind);
    }  /* if */
  }  /* if */
  end_error();
}  /* diagnose_overload_ambiguity */


static void set_arg_summary_for_user_conversion(
                                       an_arg_match_summary *arg_summary,
                                       a_conv_descr         *conversion,
                                       a_type_ptr           param_type,
                                       a_boolean            param_is_reference)
/*
Set *arg_summary to indicate an argument match involving a user-defined
conversion using a constructor or conversion function.  The conversion
is described by *conversion.  param_type is the parameter type; it's a
reference type if param_is_reference is TRUE.
*/
{
  a_type_ptr conversion_type;

  arg_summary->match_level = aml_user_conversion;
  arg_summary->conversion = *conversion;
  if (param_is_reference && !conversion->unusable &&
      !conversion->unknown_dependent_conversion) {
    /* For reference parameters, see if any type qualifiers were added under
       the reference relative to the output type of the conversion function.
       That serves as a tie-breaker in overload resolution. */
    param_type = type_pointed_to(param_type);
    check_assertion(arg_summary->conversion.routine != NULL);
    /* Get the return type of the conversion routine. */
    conversion_type = return_type_of(arg_summary->conversion.routine->type);
    if (!arg_summary->conversion.result_is_an_lvalue) {
      /* The lvalue gets converted to an rvalue, so the type qualifiers
         are dropped. */
      conversion_type = rvalue_type(conversion_type);
    }  /* if */
    if (any_qualifier_missing(conversion_type, param_type)) {
      /* Some type qualifiers are being added.  Remember that for use as a
         tie-breaker later. */
      arg_summary->conversion.std.type_qualifiers_added = TRUE;
    }  /* if */
  }  /* if */
}  /* set_arg_summary_for_user_conversion */


static a_boolean array_transformation_needed_on_reference_init(
                                                       a_type_ptr *arg_type,
                                                       a_type_ptr param_type,
                                                       an_operand *arg_operand)
/*
Return TRUE if when initializing a parameter of type param_type (a reference
type) from an argument of type *arg_type (an array type), the array -->
pointer transformation should be done.  arg_operand, if non-NULL, is
the argument.  arg_type is passed with an extra level of indirection
so that it can be updated if arg_operand is changed (e.g., because
it is a reference to a template static data member which gets
instantiated and changes from an unknown-bound array to a known-size
array).
*/
{
  a_boolean transform_needed = TRUE, dropping_qualifiers;
  a_boolean ref_to_const, ref_to_const_volatile, binding_to_rvalue_allowed;
  a_boolean template_case;

  /* The array --> pointer transformation is wanted except when initializing
     a reference to the right array type, e.g.,
       char (&r)[4] = "abc";
  */
  if (direct_reference_binding_possible(arg_operand,
                                        *arg_type,
                                        param_type,
                                        /*is_cast=*/FALSE,
                                        &ref_to_const,
                                        &ref_to_const_volatile,
                                        &binding_to_rvalue_allowed,
                                        &dropping_qualifiers,
                                        &template_case,
                                        (a_symbol **)NULL)) {
    transform_needed = FALSE;
    if (arg_operand != NULL) *arg_type = arg_operand->type;
  }  /* if */
  return transform_needed;
}  /* array_transformation_needed_on_reference_init */


static a_boolean function_transformation_needed_on_reference_init(
                                                         a_type_ptr arg_type,
                                                         a_type_ptr param_type)
/*
Return TRUE if when initializing a parameter of type param_type (a reference
type) from an argument of type arg_type (a function type), the function -->
pointer transformation should be done.
*/
{
  a_boolean transform_needed = TRUE, dropping_qualifiers;
  a_boolean ref_to_const, ref_to_const_volatile, binding_to_rvalue_allowed;
  a_boolean template_case;

  /* The function --> pointer transformation is wanted except when
     initializing a reference to the right function type, as in
       void f();
       void (&r)() = f;
  */
  if (direct_reference_binding_possible((an_operand *)NULL,
                                        arg_type,
                                        param_type,
                                        /*is_cast=*/FALSE,
                                        &ref_to_const,
                                        &ref_to_const_volatile,
                                        &binding_to_rvalue_allowed,
                                        &dropping_qualifiers,
                                        &template_case,
                                        (a_symbol **)NULL)) {
    transform_needed = FALSE;
  }  /* if */
  return transform_needed;
}  /* function_transformation_needed_on_reference_init */


a_boolean conversion_for_direct_reference_binding_possible(
                                      an_operand               *source_operand,
                                      a_type_ptr               dest_type,
                                      a_boolean                question_conv,
                                      a_conv_descr             *conversion,
                                      a_boolean                *ambiguous,
                                      a_candidate_function_ptr *ambiguity_list)
/*
See if it is possible to convert source_operand (of class type) to an lvalue
to which a reference of type dest_type can be directly bound.  If so, set
*conversion to describe the conversion and return TRUE; otherwise, return
FALSE.  If more than one function matches, set *ambiguous to TRUE and
return FALSE.  If ambiguity_list is non-NULL in that case, it is set to
point to a list describing the set of ambiguous functions; the caller
must free that list.  If question_conv is TRUE, this is being checked as
part of determining the conversions on the operands of a "?" operator.
*/
{
  a_boolean  okay;
  a_type_ptr base_dest_type;

  *ambiguous = FALSE;
  check_assertion(is_reference_type(dest_type));
  base_dest_type = type_pointed_to(dest_type);
  if (is_rvalue_reference_type(dest_type)) {
    /* This conversion is not applicable to rvalue references (if you convert
       to an lvalue you won't be able to bind the rvalue reference to it). */
    okay = FALSE;
  } else if (microsoft_bugs && microsoft_version < 1310 &&
             (!is_an_lvalue(source_operand) ||
              operand_is_temp_init(source_operand))) {
    /* The Microsoft compiler (VC++ 6.0, 7.0, fixed in 7.1) implements
       an older rule in the Working Paper that does not allow a conversion
       function to be used for a direct reference binding unless the original
       expression is an lvalue.  Note that because there is another
       Microsoft change that makes the result of a function call that
       returns a class into an lvalue, we have to test for temp init
       expressions specially. */
    okay = FALSE;
  } else {
    okay = conversion_from_class_possible(source_operand,
                                          base_dest_type,
                                          (a_builtin_type_kind_set)BTK_NONE,
                                          /*need_lvalue_result=*/TRUE,
                                          /*is_copy_initialization=*/FALSE,
                                          /*is_reference_binding=*/TRUE,
                                          conversion,
                                          ambiguous,
                                          ambiguity_list);
    if (okay && microsoft_bugs && microsoft_version >= 1310 &&
        !question_conv && is_const_qualified_type(base_dest_type)) {
      /* MSVC++ (up to version 8.0, at least) has some confusion on
         doing a conversion to bind a reference.  Instead of doing one
         overload resolution for the direct binding case and one later
         for the bind-to-converted-temp-rvalue case, it always does both
         and if they're both okay looks to see if they got a different
         result and if so concludes that the case is ambiguous. */
      a_boolean    ms_ambiguous = FALSE, local_ambiguous;
      a_conv_descr local_conversion;
      if (conversion_from_class_possible(source_operand,
                                         base_dest_type,
                                         (a_builtin_type_kind_set)BTK_NONE,
                                         /*need_lvalue_result=*/FALSE,
                                         /*is_copy_initialization=*/TRUE,
                                         /*is_reference_binding=*/FALSE,
                                         &local_conversion,
                                         &local_ambiguous,
                                         (a_candidate_function_ptr *)NULL)) {
        if (local_conversion.routine != conversion->routine &&
            !(local_conversion.routine != NULL &&
              conversion->routine != NULL &&
              local_conversion.routine->assoc_template != NULL &&
              local_conversion.routine->assoc_template ==
                                        conversion->routine->assoc_template)) {
          /* The second overload resolution would get a different conversion
             function.  We rule out cases where both routines are instances of
             the same template because MSVC++ seems to do something like
             that. */
          ms_ambiguous = TRUE;
        }  /* if */
      }  /* if */
      if (ms_ambiguous) {
        /* MSVC++ would consider the conversion ambiguous. */
        okay = FALSE;
        *ambiguous = TRUE;
        if (ambiguity_list != NULL) {
          /* Try the conversion again to get the candidate functions set
             for the ambiguity list (containing all the functions, not just
             the one that might have won on the second overload resolution). */
          try_conversion_function_match(source_operand,
                                        base_dest_type,
                                        base_dest_type,
                                        (a_builtin_type_kind_set)BTK_NONE,
                                        /*need_lvalue_result=*/FALSE,
                                        /*is_copy_initialization=*/FALSE,
                                        /*is_reference_binding=*/FALSE,
                                        ambiguity_list);
          check_assertion(*ambiguity_list != NULL);
        }  /* if */
      }  /* if */
    }  /* if */
    /* The flag here is deliberately not set when *ambiguous is TRUE. */
    if (okay) conversion->conversion_for_direct_reference_binding = TRUE;
  }  /* if */
  return okay;
}  /* conversion_for_direct_reference_binding_possible */


static a_boolean arg_copy_can_be_done_via_constructor(an_operand *arg_operand,
                                                      a_type_ptr param_type)
/*
The argument described by arg_operand is being passed to a parameter of
type param_type.  param_type is not a reference and, ignoring cv-qualifiers,
it is the same type as that of the argument, or a base type thereof.
In this context, no user-defined conversion is allowed, so verify that
the argument can be passed via a constructor (not necessarily a "copy
constructor", e.g., a template is allowed) rather than requiring some
auto_ptr-like trick involving an auxiliary class, which would count as
a user-defined conversion.  Return TRUE if the copy can be done via a
constructor.
*/
{
  a_boolean    copy_can_be_done = FALSE;
  a_symbol_ptr cctor_sym;
  a_boolean    class_bitwise_copy, ambiguous;

  check_assertion(arg_operand != NULL &&
                  is_class_struct_union_type(param_type));
  cctor_sym = select_overloaded_copy_constructor(
                                        param_type,
                                        get_type_qualifiers(arg_operand->type),
                                        is_an_rvalue(arg_operand),
                                        &arg_operand->position,
                                        &ambiguous,
                                        /*uncallable=*/(a_boolean *)NULL,
                                        &class_bitwise_copy);
  if (class_bitwise_copy || ambiguous || cctor_sym != NULL) {
    /* A copy constructor can be used. */
    copy_can_be_done = TRUE;
  }  /* if */
  return copy_can_be_done;
}  /* arg_copy_can_be_done_via_constructor */


#if DEBUG

static void db_display_overload_level(void)
/*
Display the current overload resolution nesting level at the start
of a line of debug output.
*/
{
  fprintf(f_debug, "[%lu] ", overload_level);
}  /* db_display_overload_level */

#endif /* DEBUG */

/*
Return TRUE if the given constant is a possible template-dependent
null pointer constant but not a known null pointer constant.
*/
#define is_possible_dependent_null_pointer_constant(con) \
  ((con)->kind == (a_constant_repr_kind)ck_template_param && \
   is_or_might_be_null_pointer_constant(con))


void determine_arg_match_level(an_operand           *arg_operand,
                               a_type_ptr           arg_type,
                               a_type_ptr           param_type,
                               a_boolean            param_type_is_deduced,
                               a_boolean            try_user_conversions,
                               an_arg_match_summary *arg_summary)
/*
Determine how well an actual argument matches a formal parameter with type
param_type.  The actual argument is usually given by arg_operand, but
if arg_type is non-NULL, it provides the argument type for an argument
about which nothing else is known (and arg_operand is ignored; this can
only be used for operands that don't require user-defined conversions,
e.g., those being matched up with a "this" parameter).  arg_summary is
set to indicate the level of match.  This is used in resolving overloaded
function calls.  See ARM 13.2.  param_type_is_deduced is TRUE if
the parameter type involved template parameters and was deduced.
User-defined conversions will be attempted only if try_user_conversions
is TRUE; it must be FALSE if arg_type is non-NULL.
*/
{
  an_operand        *orig_arg_operand;
  a_boolean         param_is_reference, param_is_rvalue_reference;
  a_boolean         source_can_be_rvalue = TRUE;
  a_boolean         param_is_class_type, arg_is_class_type;
  a_boolean         ref_type_qualifiers_dropped, ref_type_qualifiers_added;
  a_boolean         ref_qualifiers_dropped_related_type;
  a_boolean         uses_type_qualifiers_dropped_anachronism = FALSE;
  a_std_conv_descr  std_conversion;
  a_base_class_ptr  bcp;
  a_boolean         ambiguous;
  a_boolean         arg_operand_is_constant, arg_converted_to_rvalue = FALSE;
  a_boolean         arg_originally_an_lvalue = FALSE;
  a_boolean         arg_operand_is_simple_string_literal;
  a_constant_ptr    arg_operand_constant;
  an_operand        implicit_arg_operand;
  a_type_ptr        orig_param_type = param_type;
  a_type_ptr        unqual_arg_type, unqual_param_type;

  db_enter(4, "determine_arg_match_level");
#if DEBUG
  if (debug_level >= 4 || db_flag_is_set("overload")) {
    db_display_overload_level();
    fprintf(f_debug, "Entering determine_arg_match_level, param_type = ");
    db_abbreviated_type(param_type);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  clear_arg_match_summary(arg_summary);
  arg_summary->param_type = param_type;
  if (arg_type == NULL) {
    /* Get the actual argument type from arg_operand. */
    arg_type = arg_operand->type;
  } else {
    /* arg_type is supplied, so arg_operand should be ignored. */
    arg_operand = NULL;
#if CHECKING
    if (try_user_conversions) {
      /* Cannot try user conversions without a full operand. */
      internal_error(
        "determine_arg_match_level: arg_type != NULL && try_user_conversions");
    }  /* if */
#endif /* CHECKING */
  }  /* if */
  orig_arg_operand = arg_operand;
  arg_operand_is_simple_string_literal = FALSE;
  if (arg_operand != NULL) {
    /* Remember whether the argument is a simple string literal.  This is
       done early so that it is set before the lvalue-->rvalue conversion
       is done on the string literal. */
    arg_operand_is_simple_string_literal =
                                         arg_operand->is_simple_string_literal;
    arg_originally_an_lvalue = (is_an_lvalue(arg_operand) ||
                                is_a_function_designator(arg_operand));
  }  /* if */
  param_is_reference = is_reference_type(param_type);
  param_is_rvalue_reference = is_rvalue_reference_type(param_type);
  /* See if the array --> pointer and function --> pointer transformations
     should be done. */
  if (is_array_type(arg_type) &&
      (!param_is_reference ||
       array_transformation_needed_on_reference_init(&arg_type, param_type,
                                                     arg_operand))) {
    /* Simulate the array --> pointer transformation.  After the transformation
       we have only a type for the argument, and no arg_operand. */
    arg_type = type_after_array_to_pointer_transformation(arg_type);
    arg_operand = NULL;
    arg_converted_to_rvalue = TRUE;
    /* arg_operand_is_simple_string_literal is left alone on purpose. */
  } else if ((arg_operand != NULL ?
                              (is_a_function_designator(arg_operand) &&
                               !is_indefinite_function_operand(arg_operand)) :
                              is_function_type(arg_type)) &&
             (!param_is_reference ||
              function_transformation_needed_on_reference_init(arg_type,
                                                               param_type))) {
    /* Simulate the function --> pointer transformation.  After the
       transformation we have only a type for the argument, and no
       arg_operand. */
    /* Note that indefinite function designators are left alone. */
    arg_type = type_after_function_to_pointer_transformation(arg_type,
                                                             arg_operand);
    arg_operand = NULL;
    arg_converted_to_rvalue = TRUE;
  }  /* if */
  /* Remove parts of the param type that could be added by trivial
     conversions, hoping thereby to end up with the arg type. */
  ref_type_qualifiers_dropped = ref_type_qualifiers_added = FALSE;
  ref_qualifiers_dropped_related_type = FALSE;
  if (param_is_reference) {
    a_type_qualifier_set param_type_qualifiers, arg_type_qualifiers;
    /* The parameter type is a reference.  Drop the reference and remember
       we have one.  This is the "T --> T&" case.  Note that we're dropping any
       qualifiers above the reference type, but that's okay; they don't really
       mean anything ("int &const a" is meaningless, and, in recent WPs,
       invalid). */
    param_type = type_pointed_to(param_type);
    param_type_qualifiers = get_type_qualifiers(param_type);
    arg_type_qualifiers   = get_type_qualifiers(arg_type);
    /* See whether the reference can bind to an rvalue. */
    if (param_is_rvalue_reference) {
      /* An rvalue reference can bind (only) to an rvalue. */
      source_can_be_rvalue = TRUE;
    } else if (any_cfront_mode() || allow_anachronisms) {
      /* A reference to non-const can bind to an rvalue in cfront mode
         or anachronisms mode. */
      source_can_be_rvalue = TRUE;
    } else {
      /* Normal case.  An lvalue reference can bind to an rvalue only if it's
         a reference to const. */
      /* Note that the C++ Standard does not require the test for
         const volatile here.  (A core working group discussed it in
         Austin in March 1995 and decided it didn't care to bring it
         up in full committee.) */
      source_can_be_rvalue = ((param_type_qualifiers & TQ_CONST) != 0);
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* __unaligned and __restrict can be dropped. */
    if (microsoft_mode) {
      arg_type_qualifiers &= ~(TQ_UNALIGNED | TQ_RESTRICT);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* Check the type qualifiers to see if they can be reconciled by
       trivial conversions. */
    if (param_type_qualifiers == arg_type_qualifiers) {
      /* The qualifiers are the same: okay. */
    } else if (any_qualifier_in_set_missing(param_type_qualifiers,
                                            arg_type_qualifiers)) {
      /* There are some type qualifiers on the argument type that do not
         appear on the parameter type, so some type qualifiers are being
         dropped. */
      /* cfront allows dropping of qualifiers on nonclass arguments.
         Note that all cases handled here are arguments. */
      if (any_cfront_mode() && !is_class_struct_union_type(param_type)) {
        /* Okay to drop qualifiers. */
        uses_type_qualifiers_dropped_anachronism = TRUE;
      } else {
        ref_type_qualifiers_dropped = TRUE;
        /* If the types are not reference-related, any dropping of
           cv-qualifiers is not relevant as a qualification issue. */
        if (identical_types_ignoring_qualifiers(arg_type, param_type) ||
            (is_class_struct_union_type(arg_type) &&
             is_class_struct_union_type(param_type) &&
             find_base_class_of(arg_type, param_type) != NULL)) {
          ref_qualifiers_dropped_related_type = TRUE;
        }  /* if */
      }  /* if */
    } else {
      /* Some type qualifiers are being added.  That's okay, but it may
         be a tie-breaker later. */
      ref_type_qualifiers_added = TRUE;
    }  /* if */
  } else {
    /* The parameter type is not a reference, which means the argument is
       passed by value (i.e., a copy is made).  The cv-qualifiers on the
       source do not matter: you can copy a const T to a T without problem. */
    arg_type = skip_typerefs(arg_type);
    /* Qualifiers on the parameter type are also not significant.
       One cannot distinguish f(int) and f(const int).  In default mode
       the declaration processing removes the qualifiers, but there might
       be some in cfront mode. */
    param_type = skip_typerefs(param_type);
    /* See if the operand is an lvalue for a constant-valued variable.
       If so, an lvalue --> rvalue transformation might be useful.
       Don't do this in cfront mode. */
    if (!any_cfront_mode() &&
        arg_operand != NULL && is_an_lvalue(arg_operand)) {
      a_constant_ptr con_var_value =
                             value_of_constant_var_lvalue_operand(arg_operand);
      if (con_var_value != NULL) {
        /* The operand is an lvalue for a constant-valued variable.
           Make an operand for the constant value, because it might be
           that a pointer conversion can convert 0 to a pointer type. */
        make_constant_operand(con_var_value, &implicit_arg_operand);
        arg_operand = &implicit_arg_operand;
      }  /* if */
    }  /* if */
  }  /* if */
  /* We've now done transformations for all the trivial conversions except
     those that involve adding type qualifiers. */
  unqual_arg_type = skip_typerefs(arg_type);
  unqual_param_type = skip_typerefs(param_type);
  if (is_immediate_error_type(unqual_arg_type) ||
      is_immediate_error_type(unqual_param_type)) {
    /* An error type matches anything, but not very well. */
    arg_summary->match_level = aml_error;
    goto have_level;
  }  /* if */
  param_is_class_type = is_immediate_class_type(unqual_param_type);
  arg_is_class_type = is_immediate_class_type(unqual_arg_type);
  /* Determine whether the argument is constant. */
  arg_operand_is_constant = FALSE;
  arg_operand_constant = NULL;
  if (arg_operand != NULL && is_an_rvalue(arg_operand)) {
    /* For a constant argument, get the constant value. */
    arg_operand_is_constant = is_constant_operand(arg_operand);
    if (arg_operand_is_constant) {
      arg_operand_constant = &arg_operand->variant.constant;
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (microsoft_mode) {
      /* Microsoft mode allows some expressions as null pointer constants. */
      adjust_constant_operand_info_for_microsoft_null_pointer_test(
                                                      arg_operand,
                                                      &arg_operand_is_constant,
                                                      &arg_operand_constant,
                                                      (an_expr_node **)NULL);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    }  /* if */
  }  /* if */
  arg_summary->arg_is_constant = arg_operand_is_constant;
  /* If the type qualifiers are not okay, do not check for the simple
     matches; go directly to user-defined conversions (which do their own
     variety of checking of type qualifiers). */
  if (!ref_qualifiers_dropped_related_type) {
    /* Check for an exact match.  This is case [1] in the ARM. */
    /* The "_ignoring_qualifiers" version is called here to deal with
       array types with qualifiers on the element type. */
    if (types_are_compatible_ignoring_qualifiers(unqual_arg_type,
                                                 unqual_param_type)) {
      /* There is an exact match, possibly involving trivial conversions. */
      arg_summary->match_level = aml_exact;
      if (!param_is_reference && param_is_class_type) {
        /* The argument and parameter are the same class type, so this
           qualifies as a class copy. */
        if (!try_user_conversions) {
          /* User-defined conversions are not allowed.  Copying a class to
             its own type doesn't count as a conversion if a copy
             constructor is used, but does if some trick of using an
             auxiliary class is required to do the copy.  So check that the
             copy can be done via a constructor. */
          if (!arg_copy_can_be_done_via_constructor(arg_operand,
                                                    param_type)) {
            /* No simple copy constructor can be used to do this copy,
               so fail. */
            arg_summary->match_level = aml_none;
            clear_conv_descr(&arg_summary->conversion);
            goto have_level;
          }  /* if */
        }  /* if */
        arg_summary->conversion.
                      user_conversion_for_class_copy_must_be_determined = TRUE;
      }  /* if */
      goto have_level;
    }  /* if */
    /* Check for another exact match case, for pointers involving addition
       of type qualifiers on the type pointed to (the "T* --> qualified T *"
       case). */
    if ((!param_is_reference ||
         (microsoft_bugs && !ref_type_qualifiers_dropped)) &&
        is_pointer_type(param_type) &&
        is_pointer_type(arg_type)
#ifdef pointer_types_have_same_repr
        /* Checking that the pointer representation is the same here forces
           a conversion that changes pointer representation to be a
           conversion, not an exact match.  That is a language choice on
           an extension.  If that's not right, remove this. */
        && pointer_types_have_same_repr(param_type, arg_type)
#endif /* ifdef pointer_types_have_same_repr */
                                                             ) {
      a_type_ptr    arg_type_pointed_to = type_pointed_to(arg_type);
      a_type_ptr    param_type_pointed_to = type_pointed_to(param_type);
      an_error_code warning_suggested;
      if (qualification_conversion_possible(arg_type_pointed_to,
                                            param_type_pointed_to,
                                            (a_boolean *)NULL,
                                            &warning_suggested,
                                            /*ignore_underlying_type=*/FALSE)){
        /* Some qualifiers are being added.  This is the
           "T* --> qualified T *" case, which should be remembered
           as a possible tie-breaker later.  Note that the case where
           the types are the same would have been handled earlier and
           would not come here. */
        arg_summary->match_level = aml_exact;
        arg_summary->conversion.std.type_qualifiers_added = TRUE;
        arg_summary->conversion.std.warning_suggested = warning_suggested;
        if (param_is_reference) {
          /* This is a Microsoft bug extension.  Mark it as less desirable. */
          arg_summary->tiebreaker_anachronism_used = TRUE;
        }  /* if */
        goto have_level;
      }  /* if */
    }  /* if */
    if (arg_operand != NULL && is_indefinite_function_operand(arg_operand) &&
        (source_can_be_rvalue || is_a_function_designator(arg_operand)) &&
        (!param_is_rvalue_reference || is_an_rvalue(arg_operand))) {
      /* The source is an indefinite function, i.e., the address of an
         overloaded function.  It can be converted to an appropriate
         pointer, reference, or pointer-to-member type.  For the
         pointer and pointer-to-member cases, the operand can be a function
         designator or pointer to function; for the (non-const) reference
         case it must be a function designator.  For an rvalue reference
         parameter, only an rvalue pointer to function will do. */
      a_symbol_ptr chosen_function;
      a_boolean    unknown_dependent_function;
      if ((chosen_function =
           find_addr_of_overloaded_function_match(arg_operand->symbol,
                                                  (a_boolean)arg_operand->
                                                                is_template_id,
                                                  arg_operand->
                                                             template_arg_list,
                                                  is_a_function_designator(
                                                                  arg_operand),
                                                  orig_param_type,
                                                  /*is_cast=*/FALSE,
                                                  /*is_static_cast=*/FALSE,
                                                  &arg_summary->match_level,
                                                  &std_conversion,
                                                  (a_boolean *)NULL,
                                                  &unknown_dependent_function,
                                                  &ambiguous)) != NULL ||
          unknown_dependent_function ||
          ambiguous) {
        /* There is a suitable indefinite function, or more than one.
           arg_summary->match_level has been set appropriately. */
        arg_summary->conversion.std = std_conversion;
        /* The unknown dependent case should have been caught higher up. */
        check_assertion(!unknown_dependent_function);
        if (ambiguous) arg_summary->conversion.unusable = TRUE;
        if (chosen_function != NULL &&
            (chosen_function->kind == (a_symbol_kind)sk_routine ||
             chosen_function->kind == (a_symbol_kind)sk_member_function) &&
            chosen_function->variant.routine.ptr->is_template_function) {
          /* Remember that the argument is a template. */
          arg_summary->template_symbol = chosen_function;
        }  /* if */
        goto have_level;
      }  /* if */
    }  /* if */
    /* Try a match involving standard conversions.  This is case [3] in
       the ARM.  As a subcase, some standard conversions are considered
       promotions (case [2] in the ARM). */
    if (impl_conversion_possible(arg_type,
                                 arg_operand_is_constant,
                                 arg_operand_is_simple_string_literal,
                                 arg_operand_constant,
                                 param_type,
                                 /*allow_qualifier_or_eh_mismatch=*/FALSE,
                                 /*suppress_extensions=*/TRUE,
                                 ec_simple_incompatible_param,
                                 &std_conversion) &&
        /* cfront requires that a null pointer constant be spelled "0"
           for it to be convertible to a pointer in overload resolution. */
        !(any_cfront_mode() && arg_operand_is_constant &&
          is_null_pointer_constant(arg_operand_constant) &&
          (is_pointer_type(param_type) || is_ptr_to_member_type(param_type)) &&
          !arg_operand->is_cfront_null_pointer_constant) &&
        /* In prototype instantiation calls, assume that a value-dependent
           integral value can't be treated as a null pointer constant even
           if it might have the value zero in some instantiations.
           g++ does this differently; we treat the call as dependent in g++
           mode. */
        !(arg_operand_is_constant &&
          is_possible_dependent_null_pointer_constant(arg_operand_constant) &&
          is_pointer_type(param_type) &&
          !gpp_mode)) {
      /* Match with standard conversions. */
      arg_summary->match_level = aml_std_conversion;
      arg_summary->conversion.std = std_conversion;
      arg_converted_to_rvalue = arg_originally_an_lvalue;
      if (std_conversion.promotion) {
        /* This standard conversion is a promotion. */
        arg_summary->match_level = aml_promotion;
      } else if (std_conversion.conv_of_string_literal_to_ptr_to_nonconst) {
        /* The deprecated conversion from a string literal to a pointer to
           nonconst counts as an exact match (it's worse than other exact
           matches that do not use that deprecated conversion). */
        arg_summary->match_level = aml_exact;
        if (std_conversion.pointer_normalization_needed) {
          /* In Microsoft mode, a conversion from a const string literal to
             void * is allowed as an anachronism.  Keep that case as an
             anachronistic standard conversion rather than an anachronistic
             exact match. */
          arg_summary->match_level = aml_std_conversion;
          arg_summary->tiebreaker_anachronism_used = TRUE;
          arg_summary->conversion.std.
                             conv_of_string_literal_to_ptr_to_nonconst = FALSE;
        }  /* if */
      } else if (cfront_2_1_mode && param_is_reference &&
                 std_conversion.cast_base_class == NULL) {
        /* cfront 2.1 has a bug: when a reference parameter is initialized
           with something that requires a standard conversion that isn't
           class-related, the cost is considered to be a user-defined
           conversion. */
        /* Note that this case is strange in that the level is
           aml_user_conversion but arg_summary->conversion does not indicate a
           user-defined conversion. */
        arg_summary->match_level = aml_user_conversion;
      }  /* if */
      goto have_level;
    }  /* if */
    if (param_is_class_type && arg_is_class_type &&
        (bcp = find_base_class_of(arg_type, param_type)) != NULL) {
      /* The argument is a derived class and the parameter is a base class,
         so the conversion can be done. */
      arg_summary->match_level = aml_std_conversion;
      arg_summary->conversion.std.cast_base_class = bcp;
      arg_summary->conversion.std.nontrivial_conversion = TRUE;
      if (param_is_reference) {
        /* This case falls under the reference standard conversions
           (ARM 4.7). */
        /* The operand need not be forced to an rvalue. */
        check_assertion(arg_operand != NULL);
        arg_summary->conversion.result_is_an_lvalue= is_an_lvalue(arg_operand);
      } else {
        /* This case falls under the aggregate initialization rules
           (ARM 8.4.1) or the copy constructor rules (ARM 12.8).  Note
           that this case counts as a standard conversion even if a copy
           constructor is called. */
        if (!try_user_conversions) {
          /* User-defined conversions are not allowed.  Copying a class to
             a base class type doesn't count as a conversion if a copy
             constructor is used, but does if some trick of using an
             auxiliary class is required to do the copy.  So check that the
             copy can be done via a constructor. */
          if (!arg_copy_can_be_done_via_constructor(arg_operand,
                                                    param_type)) {
            /* No simple copy constructor can be used to do this copy,
               so fail. */
            arg_summary->match_level = aml_none;
            clear_conv_descr(&arg_summary->conversion);
            goto have_level;
          }  /* if */
        }  /* if */
        arg_summary->conversion.
                      user_conversion_for_class_copy_must_be_determined = TRUE;
      }  /* if */
      goto have_level;
    }  /* if */
  }  /* if */
  if (try_user_conversions) {
    a_conv_descr conversion;
    /* Try a match involving user-defined conversions.  This is case [4]
       in the ARM.  Note that we use orig_arg_operand, i.e., the argument
       before any implicit transformations (like array --> pointer) for
       these tests, because the user-defined conversion routines may or
       may not want the transformations we've done. */
    check_assertion(orig_arg_operand != NULL);
    if (param_is_rvalue_reference && !is_an_rvalue(orig_arg_operand)) {
      /* An rvalue reference can only bind to an rvalue. */
    } else if (param_is_reference && arg_is_class_type &&
               (conversion_for_direct_reference_binding_possible(
                                           orig_arg_operand,
                                           orig_param_type,
                                           /*question_conv=*/FALSE,
                                           &conversion,
                                           &ambiguous,
                                           (a_candidate_function_ptr *)NULL) ||
         ambiguous)) {
      /* The parameter is a reference, and there exists a conversion function
         that can convert the argument to an lvalue that the reference can
         bind to directly. */
      if (microsoft_bugs && ambiguous) {
        /* On cases that are ambiguous, MSVC++ considers the function
           non-viable. */
        arg_summary->match_level = aml_none;
      } else {
        set_arg_summary_for_user_conversion(arg_summary, &conversion,
                                            orig_param_type,
                                            param_is_reference);
      }  /* if */
      goto have_level;
    } else if (ref_qualifiers_dropped_related_type) {
      /* This is a case where cv-qualifiers are dropped in a reference
         binding, and the underlying types are reference-related (see
         [dcl.init.ref] in the C++ standard).  This cannot be made to match.
         For example:
           volatile int vi;
           const int &r = vi;
         Note that this test is necessary to avoid infinite recursion
         when trying to analyze an argument to a copy constructor that
         is more cv-qualified than the constructor can handle. */
    } else if (param_is_class_type && source_can_be_rvalue &&
               /* Bitwise copies are not tried here because they were
                  considered above, and bitwise copies that drop type
                  qualifiers under a reference would be allowed by here
                  after we've gone to the trouble of rejecting them above. */
               (conversion_to_class_possible(orig_arg_operand, param_type,
                                             /*try_bitwise_copy=*/FALSE,
                                           /*initializing_return_value=*/FALSE,
                                             /*is_copy_initialization=*/TRUE,
                                             /*orig_is_copy_initialization=*/
                                                                          TRUE,
                                             /* Following FALSE is correct:
                                                reference binding here is to
                                                a temp, not direct. */
                                             /*is_reference_binding=*/FALSE,
                                             &conversion, (a_conv_descr *)NULL,
                                             &ambiguous,
                                           (a_candidate_function_ptr *)NULL) ||
                (ambiguous && !microsoft_bugs))) {
      /* There is a constructor or conversion function (or several) that
         will convert the argument type to the parameter class type.
         The Microsoft compiler (VC++ 6.0) considers the match impossible
         if it is ambiguous. */
      /* We don't try the conversions to classes if we need an lvalue result,
         since constructors don't yield lvalues. */
      set_arg_summary_for_user_conversion(arg_summary, &conversion,
                                          orig_param_type, param_is_reference);
      goto have_level;
    } else if (arg_is_class_type && source_can_be_rvalue &&
               (conversion_from_class_possible(orig_arg_operand, param_type,
                                               (a_builtin_type_kind_set)
                                                                      BTK_NONE,
                                               /*need_lvalue_result=*/FALSE,
                                               /*is_copy_initialization=*/TRUE,
                                               /* Following FALSE is correct:
                                                  reference binding here is to
                                                  a temp, not direct. */
                                               /*is_reference_binding=*/FALSE,
                                               &conversion,
                                               &ambiguous,
                                           (a_candidate_function_ptr *)NULL) ||
                (ambiguous && !microsoft_bugs))) {
      /* There is a conversion function (or several) that will convert the
         argument class type into the parameter type or to some type that
         can be converted to the parameter type via a standard conversion.
         The Microsoft compiler (VC++ 6.0) considers the match impossible
         if it is ambiguous. */
      set_arg_summary_for_user_conversion(arg_summary, &conversion,
                                          orig_param_type, param_is_reference);
      goto have_level;
    }  /* if */
  }  /* if */
  /* Case [5] in the ARM, match with ellipsis, is handled by the caller. */
  /* No match is possible. */
  arg_summary->match_level = aml_none;
have_level:;
  arg_summary->lvalue_to_rvalue_conversion_used = arg_converted_to_rvalue;
  /* Do some special processing for simple matches.  User-defined conversions
     do their own processing for these issues. */
  if (arg_summary->match_level != aml_none &&
      (int)arg_summary->match_level < (int)aml_user_conversion) {
    if (ref_type_qualifiers_added) {
      /* Some type qualifiers were added under a reference.  This can serve as
         a tie-breaker later. */
      arg_summary->conversion.std.type_qualifiers_added = TRUE;
    } else if (uses_type_qualifiers_dropped_anachronism) {
      /* Some type qualifiers were dropped on a reference binding.  That's
         an anachronism and can serve as a tie-breaker. */
      arg_summary->tiebreaker_anachronism_used = TRUE;
    }  /* if */
    /* In some cases, reference parameters can't be bound to certain kinds of
       arguments, based on their lvalueness. */
    if (param_is_rvalue_reference) {
      /* An rvalue reference can only be bound to an rvalue. */
      if (arg_originally_an_lvalue) {
        arg_summary->match_level = aml_none;
      }  /* if */
    } else if (!source_can_be_rvalue &&
               (arg_converted_to_rvalue ||
                (arg_operand != NULL && is_an_rvalue(arg_operand)))) {
      /* You can't bind an lvalue reference to non-const to an rvalue.
         This was a post-ARM change (in the ARM, the binding would be okay
         in overload resolution and would get an error later if chosen). */
      if (allow_nonconst_ref_anachronism && param_is_class_type) {
        /* The anachronism of binding a reference to nonconst to a class
           rvalue is enabled.  Leave this alone.  A warning will be issued
           if this binding is actually used. */
        if (microsoft_mode && microsoft_version >= 1310) {
          /* MSVC++ allows binding a reference to nonconst to a class.
             From version 7.1 on, the binding is worse than other matches. */
          arg_summary->anachronism_used = TRUE;
        }  /* if */
      } else if (microsoft_bugs && param_type_is_deduced &&
                 arg_operand != NULL &&
                 ((microsoft_version < 1300 &&
                   is_constant_operand(arg_operand)) ||
                  is_this_parameter_operand(arg_operand,
                                            (a_variable_ptr *)NULL))) {
        /* A reference to non-const that's deduced can bind to an rvalue in
           Microsoft bugs mode (VC++ 6.0, 7.0 beta) if the operand
           is a constant (fixed as of the real 7.0).  Binding to "this"
           is allowed in all versions (at least up to 7.1). */
      } else if (microsoft_bugs &&
                 arg_operand != NULL &&
                 microsoft_can_bind_ref_to_rvalue(arg_operand)) {
        /* A reference to non-const can bind to an rvalue in Microsoft bugs
           mode in certain cases. */
      } else {
        arg_summary->match_level = aml_none;
      }  /* if */
    }  /* if */
    if (arg_summary->match_level == aml_none) {
      /* We decided the binding can't be done, so clear any conversion
         information we might have set previously. */
      clear_conv_descr(&arg_summary->conversion);
    }  /* if */
    if (arg_operand != NULL &&
        arg_summary->template_symbol == NULL) {
      if (is_constant_operand(arg_operand) &&
          is_ptr_to_member_type(arg_operand->type)) {
        /* Remember if the argument is a pointer-to-member for a member
           function of a template.  This is needed to resolve some nonstandard
           cases with unevaluated default arguments. */
        a_constant_ptr pm_con = &arg_operand->variant.constant;
        if (pm_con->kind == (a_constant_repr_kind)ck_ptr_to_member &&
            pm_con->variant.ptr_to_member.is_function_ptr) {
          a_routine_ptr pm_rout =pm_con->variant.ptr_to_member.variant.routine;
          if (pm_rout != NULL && pm_rout->is_template_function) {
            arg_summary->template_symbol = symbol_for(pm_rout);
          }  /* if */
        }  /* if */
      } else if (is_pointer_type(arg_operand->type)) {
        /* See comment above about pointers to members.  Similar processing
           for addresses of functions (e.g., non-template static member
           functions of class templates). */
        a_constant_ptr conptr = NULL;
        a_constant     con;
        if (is_constant_operand(arg_operand)) {
          conptr = &arg_operand->variant.constant;
        } else if (is_expression_operand(arg_operand) &&
                   is_an_rvalue(arg_operand) &&
                   constant_rvalue_pointer(arg_operand->variant.expression,
                                           &con, /*address_escapes=*/FALSE,
                                           (a_boolean *)NULL)) {
          conptr = &con;
        }  /* if */
        if (conptr != NULL &&
            con_is_exact_addr_of_routine(conptr)) {
          a_routine_ptr rout = conptr->variant.address.variant.routine;
          if (rout->is_template_function) {
            arg_summary->template_symbol = symbol_for(rout);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 4 || db_flag_is_set("overload")) {
    db_display_overload_level();
    if (arg_summary->match_level == aml_none) {
      fprintf(f_debug, "determine_arg_match_level: no match\n");
    } else {
      fprintf(f_debug, "determine_arg_match_level: ");
      db_arg_match_summary(arg_summary);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* determine_arg_match_level */


static void determine_selector_match_level(
                               a_type_ptr           arg_type,
                               a_boolean            selector_is_object_pointer,
                               a_type_ptr           param_type,
                               an_arg_match_summary *match_summary)
/*
Determine how well a selector argument of type arg_type matches a
"this" parameter with type param_type.  The selector type is a pointer
if selector_is_object_pointer is TRUE, a class type otherwise.
match_summary is set to indicate the level of match.  If the
anachronism of allowing a call of a non-const function with a const
selector is enabled, allow that kind of mismatch here.
*/
{
  /* Do the matching in terms of pointers even if arg_type is a class type.
     This allows differentiating matches on the basis of added qualifiers.
     The C++ standard [over.match.funcs] actually defines this matching in
     terms of references, but pointers give the same result. */
  if (!selector_is_object_pointer) {
    arg_type = make_pointer_type(arg_type);
  } else if (is_class_struct_union_type(arg_type) &&
             could_be_dependent_class_type(arg_type)) {
    /* A nonreal class could have an operator-> function, so try matching
       against a pointer to unknown type. */
    arg_type = make_pointer_type(type_of_unknown_templ_param_nontype);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode) {
    /* Drop __unaligned as a type qualifier on the argument type.
       MSVC++ allows a member function to be called on an __unaligned
       object with no warning. */
    if (is_pointer_type(arg_type)) {
      a_type_ptr           underlying_type = type_pointed_to(arg_type);
      a_type_qualifier_set quals = get_type_qualifiers(underlying_type);
      if (quals & TQ_UNALIGNED) {
        quals &= ~TQ_UNALIGNED;
        arg_type = make_unqualified_type(underlying_type);
        arg_type = make_qualified_type(arg_type, quals);
        arg_type = make_pointer_type(arg_type);
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  determine_arg_match_level((an_operand *)NULL, arg_type, param_type,
                            /*param_type_is_deduced=*/FALSE,
                            /*try_user_conversions=*/FALSE, match_summary);
  match_summary->is_match_for_this_param = TRUE;
  if (match_summary->match_level == aml_none &&
      allow_nonconst_call_anachronism) {
    /* No match.  Try the anachronism of calling a function that
       does not require a const "this" with a const selector.  See also
       set_up_for_conversion_function_call. */
    /* Make the type that the "this" parameter would have if the routine
       were const, and try again. */
    a_type_ptr this_param_base_type = type_pointed_to(param_type);
    a_type_ptr const_this_param_base_type =
                                      make_qualified_type(this_param_base_type,
                                                          TQ_CONST);
    a_type_ptr const_this_param_type =
                                 make_pointer_type(const_this_param_base_type);
    const_this_param_type = make_qualified_type(const_this_param_type,
                                                TQ_CONST);
    determine_arg_match_level((an_operand *)NULL, arg_type,
                              const_this_param_type,
                              /*param_type_is_deduced=*/FALSE,
                              /*try_user_conversions=*/FALSE,
                              match_summary);
    match_summary->is_match_for_this_param = TRUE;
    if (match_summary->match_level != aml_none) {
      /* Anachronism -- calling non-const function with const object. */
      match_summary->const_anachronism = TRUE;
      match_summary->tiebreaker_anachronism_used = TRUE;
    }  /* if */
  }  /* if */
}  /* determine_selector_match_level */


void selector_match_with_this_param(
                               an_operand           *bound_function_selector,
                               a_routine_ptr        rout,
                               a_type_ptr           this_param_type,
                               an_arg_match_summary *this_match_summary)
/*
Determine how well the selector object indicated by *bound_function_selector
matches the "this" parameter (of type this_param_type) of a member function.
If bound_function_selector->selector_is_object_pointer is TRUE,
*bound_function_selector is a pointer to a class object; otherwise,
it's a class object (lvalue or rvalue).  Return the match summary in
*this_match_summary.  If the specific routine being called is known,
rout points to the routine entry; otherwise, rout is NULL.  rout must
be non-NULL when calling a constructor or destructor, so that those
can be treated as a special case: constructors and destructors can be
called for const- and volatile-qualified objects even though they
themselves are not (and cannot be) const- or volatile-qualified.
bound_function_selector is not used in that case, and can be NULL.
*/
{
  db_enter(4, "selector_match_with_this_param");
  if (rout != NULL &&
      (rout->special_kind == (a_special_function_kind)sfk_constructor ||
       rout->special_kind == (a_special_function_kind)sfk_destructor)) {
    /* The routine is a constructor or destructor, so the check is
       suppressed. */
    clear_arg_match_summary(this_match_summary);
    this_match_summary->match_level = aml_exact;
    this_match_summary->is_match_for_this_param = TRUE;
  } else {
#if CHECKING
    if (this_param_type == NULL) {
      internal_error("selector_match_with_this_param: this_param_type NULL");
    }  /* if */
#endif /* CHECKING */
    /* See how well the selector type and the "this" parameter type
       match up. */
    determine_selector_match_level(bound_function_selector->type,
                                   (a_boolean)bound_function_selector->
                                                    selector_is_object_pointer,
                                   this_param_type,
                                   this_match_summary);
  }  /* if */
  db_exit();
}  /* selector_match_with_this_param */


static a_type_ptr this_param_type_for_overload_res(
                                             a_type_ptr   routine_type,
                                             a_symbol_ptr proj_function_symbol,
                                             a_boolean    is_conv_func)
/*
Return the effective "this" parameter type that should be used in
overload resolution for the function with the indicated type and
symbol (possibly a projection symbol).  For conversion functions and
functions imported via a using-declaration, the effective "this"
parameter type is based on the derived class indicated by the
projection symbol.  is_conv_func is TRUE if the function is a
conversion function.
*/
{
  a_type_ptr this_param_type;

  if (proj_function_symbol->kind == (a_symbol_kind)sk_projection &&
      (proj_function_symbol->variant.projection.is_using_decl ||
       is_conv_func)) {
    /* Make a pointer to the class of the projection, qualified like the
       actual "this" parameter type. */
    a_routine_type_supplement_ptr  rtsp =
                                     routine_type->variant.routine.extra_info;
    a_type_ptr                     saved_this_class = rtsp->this_class;

    /* Temporarily replace the class of "*this" by the parent type of the
       projection symbol.  This allows us to use implicit_this_param_type_of
       to synthesize the appropriately qualified type. */
    rtsp->this_class = sym_parent_class(proj_function_symbol);
    this_param_type = implicit_this_param_type_of(routine_type);
    /* Restore the correct class for "*this". */
    rtsp->this_class = saved_this_class;
  } else {
    this_param_type = implicit_this_param_type_of(routine_type);
  }  /* if */
  return this_param_type;
}  /* this_param_type_for_overload_res */


static void check_template_arg_type_qualifiers(a_type_ptr *arg_type,
                                               a_type_ptr *param_type)
/*
Check and process the type qualifiers on an argument type *arg_type and a
parameter type *param_type as part of trying to match a function template
to an argument list.  Adjust the types to remove qualifiers that need
not be considered further.
*/
{
  /* All combinations of type qualifiers are allowed in one way or
     another.  For each qualifier (e.g., const, volatile,...):
       (a)  If both the argument and the parameter are so-qualified
            or not so-qualified, that's okay.
       (b)  If the parameter is so-qualified but the argument is not,
            that's okay.  For example:
              template <class T> void f(const T &p) {}
              void m() {int i; f(i);}
            The "const" need not be considered further in matching
            the template.
       (c)  If the argument is so-qualified but the parameter is not,
            that can possibly be accommodated by choosing a template 
            parameter type that is so-qualified, so we call it okay
            here and let matches_template_type figure it out.  For
            example:
              template <class T> void f(T &p) {}
              void m() {const int i = 1; f(i);}
            "T" can be chosen to be "const int".
     Only the qualifiers of case (c) are preserved for the call to
     matches_template_type. */
  /* The first step is to drop any qualifiers that the argument and
     parameter have in common -- case (a). */
  skip_common_type_qualifiers(arg_type, param_type);
  if (any_qualifier_missing(*arg_type, *param_type)) {
    /* Some type qualifiers are being added -- case (b). */
    /* All the qualifiers on the parameter type are case (b) and can be
       removed from further consideration. */
    *param_type = skip_typerefs(*param_type);
  }  /* if */
}  /* check_template_arg_type_qualifiers */


static a_boolean adjust_deduction_pair(
                                     a_type_ptr           *p_param_type,
                                     a_type_ptr           *p_arg_type,
                                     an_operand           *arg_operand,
                                     a_template_param_ptr templ_params,
                                     a_template_arg_ptr   template_arg_list,
                                     a_type_ptr           *qc_param_type,
                                     a_type_ptr           *qc_arg_type,
                                     a_boolean            *consider_nondeduced)
/*
Adjust the types *p_param_type (a parameter type of a function template or a
type involving the "auto" type specifier) and *p_arg_type (the type of the
corresponding argument or initializer) for template argument deduction.  If
deduction is driven by an actual expression, that expression is described by
*arg_operand; otherwise, arg_operand is NULL.  templ_params points to the list
of template parameters (when deducing the "auto" type specifier the list of
template parameters has a single entry representing the "auto" type).
Adjustments may include dropping the "reference" layer on the parameter type,
array-to-pointer or function-to-pointer transformation, dropping cv-qualifiers,
and/or dropping matching "pointer" layers (e.g., T* and int* can be replaced
by T and int).  Returns TRUE if the adjustment is successful (which may mean
the types were left untouched), and FALSE otherwise (in which case the
deduction fails).  When TRUE is returned, qc_param_type and qc_arg_type
(if non-NULL) are updated to point to versions of the adjusted types that can
be used to determine whether deduction should succeed on the basis of a
qualification conversion.  The "qc_" types are similar to the adjusted types
except that matching top-level pointer types and qualifiers thereunder
are not removed.  If consider_nondeduced is non-NULL and the reason for
failure is that an indefinite function matches several ways, return
*consider_nondeduced TRUE.  template_arg_list is used in some nonstandard
modes to introduce knowledge from previous arguments; in the standard case,
it is always NULL.
*/
{
  a_boolean   adjustment_okay = FALSE;
  a_type_ptr  param_type = *p_param_type;
  a_type_ptr  arg_type = *p_arg_type;

  /* Certain top-level parts of the parameter type (e.g., references) are
     processed here before going to the type deduction routine.  The code here
     must match determine_arg_match_level and overload_distinguishable. */
  if (consider_nondeduced != NULL) *consider_nondeduced = FALSE;
  if (arg_operand != NULL && is_indefinite_function_operand(arg_operand)) {
    /* For an overloaded function, each possibility must be tried.
       Only one is allowed to match. */
    if (!indefinite_function_can_be_template_arg(arg_operand,
                                                 param_type,
                                                 &arg_type,
                                                 templ_params,
                                                 template_arg_list)) {
      /* The operand cannot be used to deduce template arguments.  This could
         be because there are multiple matches, or because the argument names
         a function template.  As a result the caller should be instructed
         to consider this a nondeduced context. */
      if (consider_nondeduced != NULL) *consider_nondeduced = TRUE;
      goto done;
    }  /* if */
    arg_operand = NULL;
  }  /* if */
  if (is_function_type(arg_type) &&
      routine_type_is_nonstatic_member_function(arg_type)) {
    /* The argument is a member function, so convert it to a pointer to
       member function.  This comes up when the function name is overloaded
       and a particular function has just been chosen (either just above,
       or in the caller).  It comes up both for pointers to members
       in the standard &A::f form and for nonstandard forms; for the latter,
       an error will be issued later if appropriate.  Note that the decay
       here is done unconditionally.  When the parameter type is not a
       reference, that obviously makes sense.  When it is a reference,
       consider that there is no such type as a "reference to member function",
       so there's no need to allow the possibility that the decay should
       not be done because the reference might bind directly. */
    arg_operand = NULL;
    arg_type = ptr_to_member_type(
                             arg_type,
                             arg_type->variant.routine.extra_info->this_class);
  }  /* if */
  if (is_reference_type(param_type)) {
    /* The parameter has a reference type. */
    a_boolean is_rvalue_ref = is_rvalue_reference_type(param_type);
    /* Drop the reference type. */
    param_type = type_pointed_to(param_type);
    if (is_rvalue_ref &&
        is_template_param_type(param_type) &&
        !is_qualified_type(param_type) &&
        arg_operand != NULL &&
        (is_an_lvalue(arg_operand) ||
         is_a_function_designator(arg_operand))) {
      /* A special case ([temp.deduct.call] paragraph 3): If the parameter
         type is an rvalue reference to a template parameter (with no
         cv-qualifiers), and the argument is an lvalue, use
         reference-to-arg-type for the argument type, which will eventually
         produce a parameter type that is an lvalue reference. */
      arg_operand = NULL;
      arg_type = make_reference_type(arg_type);
    } else if (gpp_mode &&
               arg_operand != NULL &&
               is_expression_operand(arg_operand) &&
               is_variable_node(arg_operand->variant.expression) &&
               arg_operand->variant.expression->variant.variable->
                                                           is_this_parameter &&
               !is_rvalue_ref &&
               is_template_param_type(param_type) &&
               !is_qualified_type(param_type)) {
      /* g++ allows "T &" to match "this".  The T is deduced to a const type
         to allow the binding.  Checked in g++ 3.2 and 4.4. */
      arg_type = make_qualified_type(arg_type, TQ_CONST);
    } else {
      /* Check and adjust the top-level type qualifiers. */
      check_template_arg_type_qualifiers(&arg_type, &param_type);
    }  /* if */
  } else {
    /* Not a reference. */
    /* See if any implicit transformations (e.g., array --> pointer) should
       be done. */
    arg_type = do_implicit_type_transformations(arg_type, arg_operand);
    /* The argument will be passed by copying it, so its cv-qualifiers
       are not significant. */
    arg_type = skip_typerefs(arg_type);
    /* Top-level type qualifiers on the parameter type are also not
       significant. */
    param_type = skip_typerefs_not_dependent_decltypes(param_type);
    /* An incomplete type operand cannot be made to match anything.
       This comes up for something like
         struct A *p;
         template<class T> void f(T);
         void m() { f(*p); }
    */
    complete_type_is_needed(arg_type);
    if (is_incomplete_type(arg_type) && !is_managed_nullptr_type(arg_type)) {
      /* Although the managed (C++/CLI) nullptr type is incomplete and
         cannot be used as the type of an object, for example, the Microsoft
         compiler allows it as a template argument. */
      goto done;
    }  /* if */
  }  /* if */
  /* Return the adjusted types at this point as the types that can be used to
     check for deduction via a qualification conversion. */
  if (qc_param_type != NULL) *qc_param_type = param_type;
  if (qc_arg_type != NULL) *qc_arg_type = arg_type;
  if (is_pointer_type(arg_type) && is_pointer_type(param_type)
#ifdef pointer_types_have_same_repr
      && pointer_types_have_same_repr(arg_type, param_type)
#endif /* ifdef pointer_types_have_same_repr */
                                                           ) {
    /* Check for cases where type qualifiers are being added down one
       level (or deeper) in a pointer case, e.g., int * --> const int *.
       This is another trivial conversion.
       In general, remove one level of matching pointer types. */
    arg_type = type_pointed_to(arg_type);
    param_type = type_pointed_to(param_type);
    /* Function types are not allowed to have type qualifiers, so
       do not allow deduction that puts type qualifiers over them. */
    if (!is_function_type(arg_type)) {
      /* Check and adjust the top-level type qualifiers. */
      check_template_arg_type_qualifiers(&arg_type, &param_type);
    }  /* if */
  }  /* if */
  adjustment_okay = TRUE;
done:
  *p_param_type = param_type;
  *p_arg_type = arg_type;
  return adjustment_okay;
}  /* adjust_deduction_pair */


static a_boolean deduce_from_one_pair(a_type_ptr            param_type,
                                      a_type_ptr            arg_type,
                                      a_type_ptr            qc_param_type,
                                      a_type_ptr            qc_arg_type,
                                      a_template_arg_ptr    *template_arg_list,
                                      a_template_param_ptr  template_params)
/*
This routine is used to implement template argument deduction: Trying to
develop a list of template arguments that will produce an instance type
that matches the argument list the arguments to a function call, or the
initializer of an "auto type" object.
This routine updates *template_arg_list with bindings for the template
argument based on one P/A pair, where P is the generic type *param_type and
A is the type *arg_type of the call argument or initializer.  The *param_type
and *arg_type types used for deduction are actually adjusted from the original 
types (see adjust_deduction_pair).  qc_param_type and qc_arg_type are
versions of the adjusted types (see also adjust_deduction_pair) that can
be used to determine if deduction should succeed based on a qualification
conversion.  template_params lists the template parameters (or the "auto"
specifier) for which bindings are sought.
*/
{
  a_boolean  deduction_okay = FALSE;

  /* As the matching is attempted, template_arg_list is filled in with
     the bindings for the template arguments.  This is needed during the
     matching process to ensure that each argument is used consistently
     and also later routine to build the instantiation.  The
     MTT_ALLOW_INEXACT_DEDUCTION option is used to allow an argument requiring
     a conversion from Derived<T> to Base<T>, and to allow qualifiers to be
     added under an array type. */
  if (matches_template_type(arg_type, param_type, template_arg_list,
                            template_params, MTT_ALLOW_INEXACT_DEDUCTION)) {
    deduction_okay = TRUE;
  } else if ((is_pointer_type(qc_arg_type) ||
              is_ptr_to_member_type(qc_arg_type)) &&
             (is_pointer_type(qc_param_type) ||
              is_ptr_to_member_type(qc_param_type))) {
    /* Normal deduction failed.  For pointer types, see if a qualification
       conversion can be used. */
    if (matches_template_type_with_qualification_conversion(
                            qc_arg_type, qc_param_type, template_arg_list,
                            template_params, MTT_NO_FLAGS)) {
      deduction_okay = TRUE;
    }  /* if */
  }  /* if */
  return deduction_okay;
}  /* deduce_from_one_pair */


static a_boolean deduce_one_parameter(a_param_type_ptr   ptp,
                                      an_arg_operand     **p_arg_operand,
                                      a_type_ptr         arg_type,
                                      a_symbol_ptr       template_sym,
                                      a_template_arg_ptr *template_arg_list)
/*
Do template argument deduction on one parameter of a function
template.  ptp identifies the parameter (which requires deduction).
**p_arg_operand gives the argument; p_arg_operand can be NULL, in
which case arg_type gives the argument type.  If it isn't NULL,
*p_arg_operand is advanced past the right number of arguments on
return (usually one, more than one for a parameter pack; note that
the zero-length parameter pack case won't get here because this routine
gets called only when there's an argument to match to the parameter).
template_sym is the symbol for the function_template (not a projection
symbol).  *template_arg_list points to the template argument list so
far; anything deduced is added to that.  Return TRUE if the deduction
succeeds, FALSE if it fails.
*/
{
  a_boolean            deduction_okay = TRUE;
  a_type_ptr           param_type = ptp->type;
  an_arg_operand       *arg_operand = NULL;
  an_operand           *operand = NULL;
  a_template_param_ptr templ_params;
  a_boolean            consider_nondeduced;
  a_type_ptr           qc_param_type;
  a_type_ptr           qc_arg_type;
  a_pack_expansion_stack_entry_ptr
                       pesep = NULL;

  if (p_arg_operand != NULL) arg_operand = *p_arg_operand;
  templ_params = template_supplement_for_symbol(template_sym)
                          ->variant.function.decl_cache.decl_info->parameters;
  if (ptp->is_parameter_pack) {
    /* For a parameter pack, we'll be iterating over several arguments
       that match the same parameter. */
    begin_pack_deduction_context(ptp->pack_expansion_descr,
                                 templ_params,
                                 template_arg_list,
                                 &pesep);
  }  /* if */
  /* Loop through the arguments that match a parameter pack, or just once
     through in other cases. */
  for (;;) {
    if (arg_operand != NULL) {
      operand = &arg_operand->operand;
      arg_type = operand->type;
    }  /* if */
    /* Adjust the types (e.g., for references) to prepare for the
       deduction. */
    if (!adjust_deduction_pair(&param_type, &arg_type, operand,
                               templ_params, *template_arg_list,
                               &qc_param_type, &qc_arg_type,
                               &consider_nondeduced)) {
      if (consider_nondeduced) {
        /* The argument is an indefinite function that can match in more than
           one way.  Keep going without adding anything to the template
           argument list and see if we can resolve this later as a nondeduced
           context. */
        goto next_iteration;
      }  /* if */
      /* Other cases are outright deduction failures. */
      deduction_okay = FALSE;
      break;
    }  /* if */
    /* Do the deduction. */
    deduction_okay = deduce_from_one_pair(
                         param_type, arg_type, qc_param_type, qc_arg_type,
                         template_arg_list,
                         template_sym->variant.template_info
                                     ->variant.function.decl_cache.decl_info
                                     ->parameters);
    if (!deduction_okay) break;
next_iteration:
    if (arg_operand != NULL) arg_operand = arg_operand->next;
    /* Only once through the loop for non-parameter-pack cases. */
    if (pesep == NULL) break;
    /* Parameter pack cases. */
    if (arg_operand == NULL) break;
    advance_to_next_deduced_element(pesep);
  }  /* for */
  if (pesep != NULL) {
    end_pack_deduction_context(pesep);
  }  /* if */
  if (p_arg_operand != NULL) *p_arg_operand = arg_operand;
  return deduction_okay;
}  /* deduce_one_parameter */


static a_type_ptr function_template_call_argument_deduction(
                                         a_symbol_ptr       template_sym,
                                         a_type_ptr         routine_type,
                                         an_arg_operand_ptr arg_operand_list,
                                         a_template_arg_ptr *template_arg_list)
/*
Do template type deduction on a call of the function template specified by
template_sym (not a projection symbol).  routine_type is the type of the
function, with explicitly-specified template arguments (if any) already
substituted in.  arg_operand_list is the list of arguments to the call.
*template_arg_list points to the template argument list so far; anything
deduced is added to that.  This routine returns a pointer to a routine
type for the function as it appears after substitution with the deduced
template arguments, or NULL if deduction failed.
*/
{
  a_type_ptr         updated_routine_type = NULL;
  a_routine_type_supplement_ptr
                     rtsp;
  a_param_type_ptr   ptp;
  an_arg_operand_ptr arg_operand;
  a_template_symbol_supplement_ptr
                     tssp;

  db_enter(4, "function_template_call_argument_deduction");
  check_assertion(template_sym->kind == (a_symbol_kind)sk_function_template);
  check_assertion(routine_type->kind == (a_type_kind)tk_routine);
  tssp = template_supplement_for_symbol(template_sym);
  rtsp = routine_type->variant.routine.extra_info;
  /* The pending deduction count is incremented during the deduction
     process to detect recursion in the substitution process.  If recursion
     is detected, we consider deduction to have failed.  The test is ">"
     instead of ">=" so that the recursion will normally be detected during
     instantiation instead of deduction, because that produces a better
     diagnostic. */
  if (tssp->variant.function.pending_deductions >
                                         max_pending_instantiations) goto skip;
  ++(tssp->variant.function.pending_deductions);
  /* Look through the arguments/parameters to do template argument
     deduction. */
  for (ptp = rtsp->param_type_list, arg_operand = arg_operand_list;
       ptp != NULL && arg_operand != NULL;
       ptp = ptp->next) {
    if (ptp->type_involves_deduced_template_param) {
      /* A parameter that requires type deduction.  Do the deduction. */
      if (!deduce_one_parameter(ptp, &arg_operand,
                                (a_type_ptr)NULL,
                                template_sym, template_arg_list)) {
        /* Deduction failed. */
        goto done;
      }  /* if */
    } else {
      /* Not a deduced parameter, so just advance to the next one. */
      arg_operand = arg_operand->next;
    }  /* if */
  }  /* for */
#if CHECKING
  if (arg_operand != NULL) {
    /* We ran out of parameters, but we still have arguments.  There should
       be an ellipsis. */
    check_assertion_str(rtsp->has_ellipsis,
                "function_template_call_argument_deduction: missing ellipsis");
  } else if (ptp != NULL) {
    /* We ran out of arguments, but we still have parameters.  The parameter
       should have a default argument expression or a parameter pack. */
    check_assertion_str(ptp->has_default_arg || ptp->is_parameter_pack,
        "function_template_call_argument_deduction: missing default arg expr");
  }  /* if */
#endif /* CHECKING */
  /* Make sure that the types of nontype template parameters that depend
     on other template parameters agree with the types of the deduced
     values.  Also check for the case where not all template parameters
     have been deduced.  Create a routine type with all the substitution
     done. */
  updated_routine_type = wrapup_function_template_argument_deduction(
                                           template_arg_list,
                                           template_sym,
                                           (a_template_param_ptr)NULL,
                                           /*is_partial_order_check=*/FALSE);
  if (updated_routine_type != NULL) {
    a_routine_ptr routine = template_sym->variant.template_info->
                                                      variant.function.routine;
    if (routine->special_kind == (a_special_function_kind)sfk_constructor) {
      rtsp = updated_routine_type->variant.routine.extra_info;
      ptp = rtsp->param_type_list;
      if (ptp != NULL &&
          identical_types_ignoring_qualifiers(ptp->type,
                                              parent_class_of(routine))) {
        /* After deduction, this is a constructor that looks like
             X(X);
           That is, it takes its own type as its first parameter.
           This should be treated as a deduction failure. */
        if (ptp->next == NULL) {
          /* Just one parameter.  Fail. */
          updated_routine_type = NULL;
        } else {
          /* More than one parameter.  Okay, except if we're defaulting all
             the parameters after the first. */
          if (arg_operand_list != NULL &&
              arg_operand_list->next == NULL) {
            /* Exactly one argument, and more than one parameter, so we're
               defaulting those after the first.  Fail. */
            updated_routine_type = NULL;
          }  /* if */
        } /* if */
      }  /* if */
    }  /* if */
  }  /* if */
done:
  /* Decrement the pending deduction count used to detect recursion. */
  --(tssp->variant.function.pending_deductions);
skip:;
  db_exit();
  return updated_routine_type;
}  /* function_template_call_argument_deduction */


#if !MICROSOFT_EXTENSIONS_ALLOWED
/*ARGSUSED*/  /* <-- arg_dep_lookup_done is not used in that case. */
#endif /* !MICROSOFT_EXTENSIONS_ALLOWED */
static a_boolean candidate_function_is_visible(
                                    a_symbol_ptr function_symbol,
                                    a_boolean    is_template_id,
                                    a_boolean    effects_copy_initialization,
                                    a_boolean    arg_dep_lookup_done,
                                    a_boolean    from_arg_dep_lookup,
                                    a_boolean    dependent_call,
                                    a_boolean    is_overloaded_operator,
                                    a_boolean    allow_post_declared_functions,
                                    a_boolean    *invisible_because_explicit,
                                    a_boolean    *invisible_because_post_decl)
/*
Return TRUE if the indicated candidate function (possibly a projection
symbol, but not an overloaded function) is visible.  That is, return
FALSE if it should be considered invisible for some reason.  For example,
functions injected by friend declarations are generally invisible.
is_template_id is TRUE if the function symbol has an associated
explicit template argument list.  effects_copy_initialization is
TRUE if this call is the user-defined conversion in a copy-initialization
(constructors marked "explicit" are considered invisible).
arg_dep_lookup_done is TRUE if argument-dependent lookup is enabled for
this call.  from_arg_dep_lookup is TRUE if the function was found by
argument-dependent lookup.  dependent_call is TRUE if the call is a
template-dependent call.  is_overloaded_operator is TRUE if the call
is written in operator form, e.g., a+b rather than operator+(a, b).
allow_post_declared_functions is TRUE if functions declared after the
point of reference in a dependent call should be visible to the normal
lookup (in violation of the requirements of the standard).  If
invisible_because_explicit is non-NULL, it is returned TRUE if the
routine is invisible because it is an explicit constructor, FALSE
otherwise.  If invisible_because_post_decl is non-NULL, it is returned
TRUE if the function is not visible because it is declared after the
point of call, FALSE otherwise.
*/
{
  a_boolean              visible = TRUE, function_template_case;
  a_routine_ptr          routine;
  a_decl_sequence_number effective_decl_seq;

  if (invisible_because_explicit != NULL) *invisible_because_explicit = FALSE;
  if (invisible_because_post_decl != NULL) *invisible_because_post_decl=FALSE;
  /* Ignore friend functions that aren't visible.  Note that this
     test is done on the projection symbol, if any, and not on the
     underlying fundamental symbol. */
  if (symbol_is_invisible_friend(function_symbol)) {
    visible = FALSE;
    goto end_of_function;
  }  /* if */
  /* Remove projection, if any. */
  function_symbol = fundamental_symbol_of(function_symbol);
#if MICROSOFT_EXTENSIONS_ALLOWED
  /* Note that here we must test the fundamental symbol. */
  if (microsoft_mode && microsoft_version >= 1310 &&
      (is_overloaded_operator || !arg_dep_lookup_done) &&
      function_symbol->is_microsoft_invisible_operator &&
      !from_arg_dep_lookup) {
    /* As of MSVC++ 7.1, certain operators defined as friends are
       not visible.  This is an approximation to eliminating friend
       injection, in some limited cases.  This is used on conjunction
       with special processing in argument_dependent_lookup that
       suppresses argument-dependent lookup in namespaces already searched
       by the normal lookup (a Microsoft quirk). */
    visible = FALSE;
    goto end_of_function;
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  function_template_case = (function_symbol->kind ==
                                          (a_symbol_kind)sk_function_template);
  if (do_dependent_name_processing &&
      (!from_arg_dep_lookup ||
       (defer_function_prototype_instantiations &&
        is_prototype_instantiation_context())) &&
      is_nonspecialized_instantiation_context() &&
      !function_symbol->is_class_member &&
      !is_local_symbol(function_symbol) &&
      (function_symbol->decl_seq >
                             (effective_decl_seq = get_effective_decl_seq()) &&
       effective_decl_seq != NO_DECL_SEQUENCE_NUMBER)) {
 
    /* This symbol is not visible in this template instantiation (it
       was declared after the template definition).  The test done when
       deferring prototype instantiations is used to apply the decl_seq
       check to functions found by argument dependent lookup during the
       prototype instantiation. */
    if (dependent_call && gpp_mode &&
        (gnu_version >= 30400 && gnu_version < 40100)) {
      /* g++ 3.4 has a bug and considers such symbols visible. */
    } else if (gpp_mode && is_overloaded_operator) {
      /* g++ treats symbols in overloaded operator calls as always visible.
         This has been verified in versions 3.2 through 4.2. */
    } else if (allow_post_declared_functions) {
      /* The caller says we should accept this case. */
    } else {
      visible = FALSE;
      if (invisible_because_post_decl != NULL) {
        *invisible_because_post_decl = TRUE;
      }  /* if */
      goto end_of_function;
    }  /* if */
  }  /* if */
  if (!function_template_case) {
    /* The symbol is not a function template (i.e., it's a normal function). */
    routine = function_symbol->variant.routine.ptr;
    if (is_template_id) {
      /* An explicit list of template arguments (e.g., f<int>) rules out
         non-templates. */
      visible = FALSE;
      goto end_of_function;
    }  /* if */
  } else {
    /* The symbol is a function template. */
    routine = function_symbol->variant.template_info->variant.function.routine;
  }  /* if */
  if (dependent_call &&
      routine->source_corresp.name_linkage ==
                                           (a_name_linkage_kind)nlk_internal) {
    /* Functions with internal linkage are invisible in the template-
       dependent name lookup. */
    if (gpp_mode && gnu_version >= 30400) {
      /* g++ 3.4 does not ignore static functions. */
    } else {
      visible = FALSE;
      goto end_of_function;
    }  /* if */
  }  /* if */
  if (effects_copy_initialization && routine->is_explicit_constructor) {
    /* Constructors marked "explicit" are to be ignored. */
    visible = FALSE;
    if (invisible_because_explicit != NULL) *invisible_because_explicit = TRUE;
    goto end_of_function;
  }  /* if */
end_of_function:
  return visible;
}  /* candidate_function_is_visible */


static void determine_function_viability(
                 a_symbol_ptr             proj_function_symbol,
                 a_boolean                is_template_id,
                 a_template_arg_ptr       template_arg_list,
                 a_symbol_ptr             surrogate_function_conv_sym,
                 a_type_ptr               routine_type,
                 an_arg_operand_ptr       arg_operand_list,
                 a_boolean                have_selector,
                 an_operand               *bound_function_selector,
                 a_type_ptr               implicit_selector_type,
                 a_boolean                ctor_conversion_case,
                 a_boolean                initializing_return_value,
                 a_boolean                effects_copy_initialization,
                 a_boolean                allow_udc_on_arguments,
                 a_boolean                arg_dep_lookup_done,
                 a_boolean                from_arg_dep_lookup,
                 a_boolean                dependent_call,
                 a_boolean                known_to_be_visible,
                 a_boolean                is_overloaded_operator,
                 a_boolean                allow_post_declared_functions,
                 a_candidate_function_ptr *candidate_functions,
                 a_boolean                *matched_except_for_missing_selector,
                 a_boolean                *matched_except_for_selector,
                 a_boolean                *discarded_because_post_decl)
/*
Determine whether a function is viable in overload resolution, which
means whether it has the right number of parameters of the right types.
proj_function_symbol indicates the function; it may be a projection
symbol, but it is not an overloaded function.  is_template_id is TRUE if
the symbol has an associated explicit template argument list; if so,
template_arg_list gives the argument list.  If
surrogate_function_conv_sym is non-NULL, we are evaluating a surrogate
function call (see [over.call.object] in the C++ standard);
proj_function_symbol is NULL and routine_type gives the function type.
The argument list for the call is given by arg_operand_list, and the
selector is given by bound_function_selector (if have_selector is
TRUE).  have_selector can be TRUE and bound_function_selector NULL
when calling constructors.  bound_function_selector is an object
pointer if bound_function_selector->selector_is_object_pointer is
TRUE, an object otherwise.  implicit_selector_type indicates the type
of an implicit "this->" selector, if applicable, or is NULL otherwise.
Any viable functions are added to the candidate_functions list along
with information on the level of argument matches.  If a match would
have been found except for the absence of a selector, set
*matched_except_for_missing_selector TRUE, and if a match would have
been found except for a mismatch on the selector, set
*matched_except_for_selector TRUE; those allow different error
messages.  If ctor_conversion_case is TRUE, this analysis is being
done as part of resolving an implicit or explicit conversion to a
class type: the functions are constructors, have_selector is FALSE
(sic; the "this" parameter is not matched up); the "conversion" field
is set in any candidate function entries created.
initializing_return_value is TRUE if the initialization is being done
to return a value in a return statement.  effects_copy_initialization
is TRUE if this call is the user-defined conversion in a
copy-initialization; constructors that are marked "explicit" are
ignored.  allow_udc_on_arguments is TRUE if user-defined conversions
should be allowed on the argument matches.  arg_dep_lookup_done is
TRUE if argument-dependent lookup is enabled for this call.
from_arg_dep_lookup is TRUE if the function was found by
argument-dependent lookup.  dependent_call is TRUE if the call is a
template-dependent call.  known_to_be_visible is TRUE if the function
is known to be visible and the visibility check should be suppressed.
is_overloaded_operator is TRUE if the call is written in operator
form, e.g., a+b rather than operator+(a, b).
allow_post_declared_functions is TRUE if functions declared after the
point of reference in a dependent call should be visible to the normal
lookup (in violation of the requirements of the standard).
*discarded_because_post_decl goes along with that: it is returned TRUE
if the function was not viable (at least) because it is declared after
the point of call.
*/
{
  a_symbol_ptr             function_symbol;
  a_routine_ptr            routine;
  a_routine_type_supplement_ptr
                           rtsp;
  an_arg_operand_ptr       arg_operand;
  a_param_type_ptr         param, param_before_deduction = NULL;
  a_param_type_ptr         first_param_before_deduction;
#if DEBUG
  unsigned long            narg;
#endif /* DEBUG */
  a_boolean                reached_ellipsis, first_pass;
  an_arg_match_summary_ptr this_match, this_match_next;
  an_arg_match_summary_ptr arg_match = NULL, saved_arg_match_next;
  an_arg_match_summary_ptr arg_match_list = NULL;
  an_arg_match_summary_ptr end_arg_match_list = NULL;
  a_boolean                function_template_case = FALSE;
  a_template_arg_ptr       local_template_arg_list = NULL;
  a_boolean                microsoft_explicit_constructor_case = FALSE;

  *discarded_because_post_decl = FALSE;
  if (proj_function_symbol != NULL) {
    /* Normal case: a known function. */
    a_boolean invisible_because_explicit;
    a_boolean invisible_because_post_decl;
    if (!known_to_be_visible &&
        !candidate_function_is_visible(proj_function_symbol,
                                       is_template_id,
                                       effects_copy_initialization,
                                       arg_dep_lookup_done,
                                       from_arg_dep_lookup,
                                       dependent_call,
                                       is_overloaded_operator,
                                       allow_post_declared_functions,
                                       &invisible_because_explicit,
                                       &invisible_because_post_decl)) {
      /* The function is not visible. */
      if (microsoft_bugs && microsoft_version == 1200 &&
          invisible_because_explicit && initializing_return_value) {
        /* MSVC++ 6.0 sees invisible explicit constructors, but considers
           them worse than others.  It seems to allow this on return
           statements only (not variable initializations). */
        microsoft_explicit_constructor_case = TRUE;
      } else {
        /* The function is not visible, so ignore it. */
        if (invisible_because_post_decl) *discarded_because_post_decl = TRUE;
        goto reject_function;
      }  /* if */
    }  /* if */
    function_symbol = fundamental_symbol_of(proj_function_symbol);
    function_template_case = (function_symbol->kind ==
                                          (a_symbol_kind)sk_function_template);
    if (is_ambiguous_by_inheritance(proj_function_symbol)) {
      /* The symbol is ambiguous, and as such is an arbitrary
         representative of a set of functions that collided due
         to inheritance.  There's no point in seeing whether the function
         indicated matches up, since there might be another function
         that isn't represented that would match better.  Just put the
         symbol in the candidate functions set.  It will stay in there
         and cause the overload resolution to be ambiguous. */
      goto accept_function;
    }  /* if */
    if (!function_template_case) {
      /* The symbol is not a function template (i.e., it's a normal
         function). */
      routine = function_symbol->variant.routine.ptr;
      routine_type = routine->type;
    } else {
      /* The symbol is a function template. */
      routine=function_symbol->variant.template_info->variant.function.routine;
      routine_type = routine->type;
      if (template_arg_list != NULL) {
        /* Substitute the explicitly-specified template arguments into the
           template and get the updated routine type.  This also creates
           an updated template argument list (template arguments are cast to
           the types of the template parameters), which may be different for
           each template considered. */
        a_template_symbol_supplement_ptr tssp =
                               template_supplement_for_symbol(function_symbol);
        /* Avoid infinite recursion. */
        if (tssp->variant.function.pending_deductions >
                              max_pending_instantiations) goto reject_function;
        ++(tssp->variant.function.pending_deductions);
        routine_type = substitute_template_arguments(
                         function_symbol, template_arg_list,
                         &local_template_arg_list, (a_template_param_ptr)NULL,
                         /*is_partial_order_check=*/FALSE);
        --(tssp->variant.function.pending_deductions);
        /* Bail out if there is a mismatch. */
        if (routine_type == NULL) goto reject_function;
      }  /* if */
    }  /* if */
  } else {
    /* Surrogate function call case.  We have routine_type but not
       proj_function_symbol. */
    check_assertion(routine_type != NULL);
  }  /* if */
  routine_type = skip_typerefs(routine_type);
  rtsp = routine_type->variant.routine.extra_info;
  /* Do a quick pass through the lists to eliminate a function with an
     obviously wrong number of parameters quickly.  This is not just a
     speed optimization; it avoids recursion loops on constructors
     that look like
       struct A { A(A, xxx, yyy); }
     which look viable as copy constructors on the first argument. */
  param = rtsp->param_type_list;
  /* Save the pointer to the first parameter in the template version
     (i.e., before deduction) for later use.  Note that this is after
     substitution of explicitly-specified template arguments. */
  first_param_before_deduction = param;
  for (arg_operand = arg_operand_list;
       arg_operand != NULL;
       arg_operand = arg_operand->next) {
    /* See if the parameter list is exhausted. */
    if (param == NULL) {
      /* More arguments than required.  No match unless there is an
         ellipsis. */
      if (rtsp->has_ellipsis) break;
      goto reject_function;
    } else if (param->is_parameter_pack) {
      /* A parameter pack can match all the remaining arguments. */
      param = NULL;
      arg_operand = NULL;
      break;
    }  /* if */
    param = param->next;
  }  /* for */
  /* Check that the argument and parameter lists ended at the same place. */
  if (param != NULL) {
    /* Fewer arguments than required.  No match unless there are default
       argument values.  Note that has_default_arg is not used here,
       because there are cases where has_default_arg is set and
       default_arg_expr is not set yet.  A default argument with a NULL
       default_arg_expr is accepted if it has an unevaluated template
       value, because we know this value can be produced when the call
       is generated. */
    /* A parameter pack can also make the call okay, because it can be
       matched with zero arguments. */
    if (!param->has_unevaluated_template_default &&
        param->default_arg_expr == NULL &&
        !param->is_parameter_pack) goto reject_function;
#if DEBUG
    if (debug_level >= 4 || db_flag_is_set("overload")) {
      db_display_overload_level();
      fprintf(f_debug, "determine_function_viability: default arg match\n");
    }  /* if */
#endif /* DEBUG */
  }  /* if */
  /* The function looks okay from the standpoint of argument count. */
  /* Look at each argument and see whether or not it can match the formal
     parameter, and if so, how well.  For a template, we match the
     nondependent parameters on the first pass, and only if they all
     match do we do template deduction and then a second pass for the
     dependent parameters.  That may save time and prevent us from
     instantiating some things we don't really need. */
  first_pass = TRUE;
  for (;;) {
    param = rtsp->param_type_list;
    reached_ellipsis = FALSE;
    param_before_deduction = first_param_before_deduction;
#if DEBUG
    narg = 0;
#endif /* DEBUG */
    for (arg_operand = arg_operand_list;
         arg_operand != NULL;
         arg_operand = arg_operand->next) {
#if DEBUG
      narg++;
      if (debug_level >= 4 || db_flag_is_set("overload")) {
        db_display_overload_level();
        fprintf(f_debug, "determine_function_viability: arg %lu", narg);
        if (!first_pass) fprintf(f_debug, " (pass 2)");
        fprintf(f_debug, "\n");
      }  /* if */
#endif /* DEBUG */
      if (first_pass) {
        /* Add an entry to the end of the arg_match_list to record whether or
           not this argument matches.  On the second pass, we just step
           through the already-allocated entries. */
        arg_match = alloc_arg_match_summary();
        if (arg_match_list == NULL) {
          arg_match_list = arg_match;
        } else {
          end_arg_match_list->next = arg_match;
        }  /* if */
        end_arg_match_list = arg_match;
      }  /* if */
      /* See if the parameter list is exhausted. */
      if (param == NULL) {
        /* More arguments than required.  Since the function was not rejected
           in the initial argument-count check, it must have an ellipsis. */
        check_assertion_str(rtsp->has_ellipsis,
                          "determine_function_viability: no arg, no ellipsis");
        reached_ellipsis = TRUE;
        /* There is an ellipsis, so there is a match, but with a low
           desirability. */
        arg_match->match_level = aml_ellipsis;
#if DEBUG
        if (debug_level >= 4 || db_flag_is_set("overload")) {
          db_display_overload_level();
          fprintf(f_debug, "determine_function_viability: ellipsis match\n");
        }  /* if */
#endif /* DEBUG */
      } else if (param->is_parameter_pack) {
        /* For a parameter pack in the first pass, continue advancing through
           the arguments, keeping the parameter the same, so the match entry
           for each argument gets added.  On the second pass, we should no
           longer see the parameter pack; the deduction should get rid of
           it. */
        check_assertion(first_pass);
        goto next_argument;
      } else {
        a_boolean param_type_is_deduced = FALSE;
        /* Both the actual argument and formal parameter are available.
           See how well they match. */
        if (function_template_case) {
          /* Note that we test param_before_deduction rather than param because
             we want to get the same result on the first and second passes. */
          if (param_before_deduction->type_involves_template_param) {
            /* A template-dependent parameter.  On the first pass, skip it. */
            if (first_pass) goto next_parameter;
          } else {
            /* A non-dependent parameter.  On the second pass, skip it (it
               was processed on the first pass). */
            if (!first_pass) goto next_parameter;
          }  /* if */
          if (param_before_deduction != NULL &&
              param_before_deduction->type_involves_deduced_template_param) {
            param_type_is_deduced = TRUE;
          }  /* if */
        }  /* if */
        /* On the second pass, preserve the "next" pointer in arg_match,
           so we keep the already-allocated match list intact. */
        if (!first_pass) saved_arg_match_next = arg_match->next;
        /* See how well the argument matches the parameter. */
        determine_arg_match_level(&arg_operand->operand, (a_type_ptr)NULL,
                                  param->type,
                                  param_type_is_deduced,
                                  /*try_user_conversions=*/
                                                        allow_udc_on_arguments,
                                  arg_match);
        if (!first_pass) arg_match->next = saved_arg_match_next;
        /* If no match is possible, go on to the next function. */
        if (arg_match->match_level == aml_none) goto reject_function;
      }  /* if */
next_parameter:
      /* Go on to the next parameter. */
      if (!reached_ellipsis) {
        param = param->next;
        if (function_template_case) {
          check_assertion(param_before_deduction != NULL);
          if (!param_before_deduction->is_parameter_pack) {
            param_before_deduction = param_before_deduction->next;
          }  /* if */
          if (!first_pass) arg_match = arg_match->next;
        }  /* if */
      }  /* if */
next_argument:;
    }  /* for */
    if (!first_pass || !function_template_case) break;
    /* For a template, do a second pass to match the dependent parameters. */
    first_pass = FALSE;
    arg_match = arg_match_list;
    /* Do template argument deduction on the parameter types. */
    routine_type = function_template_call_argument_deduction(
                                                   function_symbol,
                                                   routine_type,
                                                   arg_operand_list,
                                                   &local_template_arg_list);
    if (routine_type == NULL) {
      /* Deduction failed. */
      goto reject_function;
    }  /* if */
    routine_type = skip_typerefs(routine_type);
    rtsp = routine_type->variant.routine.extra_info;
  }  /* for */
  /* If param != NULL here, there are default arguments or a parameter pack
     (because we got past the argument-count check above). */
  check_assertion_str(param == NULL || param->has_default_arg ||
                      param->is_parameter_pack,
                     "determine_function_viability: no param, no default arg");
  /* All the arguments can be made to match the parameters. */
  /* See if the "this" parameter, if any, matches. */
  /* Do not process the "this" parameter for constructors in a conversion
     case. */
  if (!ctor_conversion_case) {
    a_boolean function_is_nonstatic_member_function =
                       routine_type_is_nonstatic_member_function(routine_type);
    if (have_selector) {
      /* We have a selector. */
      /* Put a match entry for it on the front of the match list. */
      this_match = alloc_arg_match_summary();
      this_match->next = this_match_next = arg_match_list;
      arg_match_list = this_match;
      if (!function_is_nonstatic_member_function) {
        /* The function has no "this" parameter, so it does not need a
           selector.  We would discard it if this function is chosen,
           but we still need a match entry for it. */
        if (surrogate_function_conv_sym != NULL) {
          /* For a surrogate function, the match on the "this" parameter
             includes the conversion. */
          a_symbol_ptr  base_conv_sym;
          a_routine_ptr conv_rout;
          a_type_ptr    conv_rout_type;
          this_match->match_level = aml_user_conversion;
          base_conv_sym = fundamental_symbol_of(surrogate_function_conv_sym);
          conv_rout = base_conv_sym->variant.routine.ptr;
          this_match->conversion.routine = conv_rout;
          this_match->conversion.routine_symbol = surrogate_function_conv_sym;
          conv_rout_type = conv_rout->type;
          conv_rout_type = skip_typerefs(conv_rout_type);
          if (is_lvalue_reference_type(
                                conv_rout_type->variant.routine.return_type)) {
            /* A conversion function returning an lvalue reference type creates
               an lvalue. */
            this_match->conversion.result_is_an_lvalue = TRUE;
          }  /* if */
        } else {
          /* Normal case (not surrogate function).  This is a match that
             can't be compared to other matches (it's neither better nor
             worse) so leave it as "none". */
          this_match->match_level = aml_none;
        }  /* if */
        this_match->is_match_for_this_param = TRUE;
      } else {
        /* The function requires a selector, and we have one. */
        /* Determine the effective "this" parameter type.  When namespaces
           are involved in classes, the parameter type is taken to be the
           class in which the "using" occurs. */
        a_type_ptr this_param_type;
        check_assertion(proj_function_symbol != NULL); /* For Coverity */
        this_param_type =
                      this_param_type_for_overload_res(routine_type,
                                                       proj_function_symbol,
                                                       /*is_conv_func=*/FALSE);
        if (implicit_selector_type != NULL) {
          /* The selector is an implicit "this->".  See how well it
             matches.  It might not match at all. */
          determine_selector_match_level(implicit_selector_type,
                                         /*selector_is_object_pointer=*/TRUE,
                                         this_param_type,
                                         this_match);
          /* Set the "next" pointer again, because it is cleared by
             determine_selector_match_level. */
          this_match->next = this_match_next;
          if (this_match->match_level == aml_none) {
            /* Mismatch. */
            a_type_ptr this_class = type_pointed_to(this_param_type);
            a_type_ptr sel_class  = type_pointed_to(implicit_selector_type);
            if (!is_same_class_or_base_class_thereof(sel_class, this_class)){
              /* "this" and the member function are in unrelated classes,
                 so it's as if no selector appears. */
              *matched_except_for_missing_selector = TRUE;
            } else {
              *matched_except_for_selector = TRUE;
            }  /* if */
            goto reject_function;
          }  /* if */
        } else {
          /* See how the selector expression matches the "this" parameter
             type. */
          selector_match_with_this_param(bound_function_selector,
                                         routine, this_param_type, this_match);
          /* Set the "next" pointer again, because it is cleared by
             selector_match_with_this_param. */
          this_match->next = this_match_next;
          if (this_match->match_level == aml_none) {
            *matched_except_for_selector = TRUE;
            goto reject_function;
          }  /* if */
        }  /* if */
      }  /* if */
    } else {
      /* We have no selector.  For functions other than a nonstatic
         member function, this is fine.  For a nonstatic member function,
         it doesn't count against the function (see core issue 364),
         but the function is not callable if selected. */
    }  /* if */
  }  /* if */
accept_function:
  /* The function is a viable candidate.  Add it to the candidates list. */
  if (function_template_case) {
    /* The symbol is a function template. */
    add_function_template_to_candidate_functions_list(
                                             proj_function_symbol,
                                             is_template_id,
                                             local_template_arg_list,
                                             arg_match_list,
                                             candidate_functions);
  } else if (surrogate_function_conv_sym != NULL) {
    /* A surrogate function is being called. */
    add_surrogate_function_to_candidate_functions_list(
                                             surrogate_function_conv_sym,
                                             arg_match_list,
                                             candidate_functions);
  } else {
    /* The symbol is a normal function. */
    add_function_to_candidate_functions_list(proj_function_symbol,
                                             arg_match_list,
                                             candidate_functions);
    if (microsoft_explicit_constructor_case) {
      (*candidate_functions)->uses_microsoft_explicit_anachronism = TRUE;
    }  /* if */
  }  /* if */
  if (ctor_conversion_case) {
    /* If we are analyzing a constructor to resolve an implicit or
       explicit conversion, set "conversion" appropriately.
       Note that this can happen for the template case also, with a
       member template, but "routine" in that case is still the
       prototype instantiation version of the routine.  The true
       routine is not known until later, when the template is chosen
       and instantiated. */
    a_candidate_function_ptr candidate = *candidate_functions;
    candidate->is_user_conversion = TRUE;
    /* coverity[uninit_use] */
    if (!function_template_case) candidate->conversion.routine = routine;
  }  /* if */
#if BACK_END_IS_CP_GEN_BE
  if (from_arg_dep_lookup) {
    /* The C++-generating back end will need to know if the call was
       resolved only because of argument-dependent lookup. */
    (*candidate_functions)->found_through_adl = TRUE;
  }  /* if */
#endif /* BACK_END_IS_CP_GEN_BE */
  goto end_of_routine;
reject_function:
  /* The function is not suitable. */
  /* Free any argument match summary entries built for it. */
  free_arg_match_summary_list(arg_match_list);
  /* Free any template argument list built for it. */
  free_template_arg_list(local_template_arg_list);
end_of_routine:;
}  /* determine_function_viability */


static a_type_ptr make_implicit_selector_type(void)
/*
If an implicit "this" is available in the current context, return its type.
Otherwise, return NULL.
*/
{
  a_type_ptr     implicit_selector_type = NULL;
  a_variable_ptr this_var;

  if (variable_this_exists(&this_var)) {
    /* An implicit selector can be generated. */
    implicit_selector_type= make_pointer_type(type_pointed_to(this_var->type));
  }  /* if */
  return implicit_selector_type;
}  /* make_implicit_selector_type */


static void try_overloaded_function_match(
                 a_symbol_ptr             overloaded_function_symbol,
                 a_boolean                is_template_id,
                 a_template_arg_ptr       template_arg_list,
                 an_arg_operand_ptr       arg_operand_list,
                 a_boolean                have_selector,
                 an_operand               *bound_function_selector,
                 a_boolean                ctor_conversion_case,
                 a_boolean                initializing_return_value,
                 a_boolean                effects_copy_initialization,
                 a_boolean                allow_udc_on_arguments,
                 a_boolean                arg_dep_lookup_done,
                 a_boolean                from_arg_dep_lookup,
                 a_boolean                dependent_call,
                 a_boolean                forced_dependent,
                 a_boolean                known_to_be_visible,
                 a_boolean                is_overloaded_operator,
                 a_candidate_function_ptr *candidate_functions,
                 a_boolean                *matched_except_for_missing_selector,
                 a_boolean                *matched_except_for_selector)
/*
Find out how well the functions described by overloaded_function_symbol
match the argument list given by arg_operand_list and the selector given
(if have_selector is TRUE) by bound_function_selector.  have_selector
can be TRUE and bound_function_selector NULL when calling constructors.
overloaded_function_symbol may be an overloaded function, a simple
function, or a projection symbol for one of those.  is_template_id
is TRUE if the symbol has an associated explicit template argument list;
if so, template_arg_list gives the argument list.
bound_function_selector is an object pointer if
bound_function_selector->selector_is_object_pointer is TRUE, an object
otherwise.  Any viable functions are added to the candidate_functions
list along with information on the level of argument matches.  If a
match would have been found except for the absence of a selector, set
*matched_except_for_missing_selector TRUE, and if a match would have
been found except for a mismatch on the selector, set
*matched_except_for_selector TRUE; those allow different error
messages.  If ctor_conversion_case is TRUE, this analysis is being
done as part of resolving an implicit or explicit conversion to a
class type: the functions are constructors, have_selector is FALSE
(sic; the "this" parameter is not matched up); the "conversion" field
is set in any candidate function entries created.
initializing_return_value is TRUE if the initialization is being done
to return a value in a return statement.  effects_copy_initialization
is TRUE if this call is the user-defined conversion in a
copy-initialization; constructors that are marked "explicit" are
ignored.  allow_udc_on_arguments is TRUE if user-defined conversions
should be allowed on the argument matches.  arg_dep_lookup_done is
TRUE if argument-dependent lookup is enabled for this call.
from_arg_dep_lookup is TRUE if the function was found by
argument-dependent lookup.  dependent_call is TRUE if the call is a
template-dependent call.  forced_dependent is TRUE if dependent_call
was forced to TRUE for reasons of g++ emulation.  known_to_be_visible
is TRUE if the function is known to be visible and the visibility
check should be suppressed.  is_overloaded_operator is TRUE if the
call is written in operator form, e.g., a+b rather than operator+(a,
b).
*/
{
  a_boolean     overloaded_function_case;
  a_symbol_ptr  function_symbol, proj_function_symbol;
  a_symbol_ptr  saved_proj_function_symbol;
  a_type_ptr    implicit_selector_type = NULL;
  a_boolean     allow_post_declared_functions = FALSE;
  a_boolean     any_discarded_because_post_decl;
  a_boolean     any_not_discarded_because_post_decl;
  a_candidate_function_ptr
                saved_candidate_functions = *candidate_functions;

  function_symbol = fundamental_symbol_of(overloaded_function_symbol);
  /* Determine whether or not the symbol is an overloaded function. */
  overloaded_function_case = (function_symbol->kind ==
                                        (a_symbol_kind)sk_overloaded_function);
  if (overloaded_function_case) {
    /* Overloaded functions. */
    overloaded_function_symbol = function_symbol;
    proj_function_symbol =
               overloaded_function_symbol->variant.overloaded_function.symbols;
  } else {
    /* Non-overloaded function. */
    proj_function_symbol = overloaded_function_symbol;
  }  /* if */
  /* Remove namespace projections, if any. */
  function_symbol = fundamental_symbol_of(proj_function_symbol);
  /* If we have no selector, see if any one of the functions requires one.
     If so, we will look to see if an implicit "this->" can be generated.
     Don't do this for the constructor case (the "this" parameter of the
     constructor is not used in the match). */
  if (!ctor_conversion_case && !have_selector) {
    a_routine_ptr routine;
    a_type_ptr    routine_type;
    a_boolean     some_function_needs_selector = FALSE;
    /* Check the first or only function to see whether or not it requires
       a selector. */
    if (function_symbol->kind == (a_symbol_kind)sk_function_template) {
      /* Template -- might be a member function template. */
      routine=function_symbol->variant.template_info->variant.function.routine;
    } else {
      check_assertion(function_symbol->kind == (a_symbol_kind)sk_routine ||
                      function_symbol->kind ==
                                            (a_symbol_kind)sk_member_function);
      routine = function_symbol->variant.routine.ptr;
    }  /* if */
    routine_type = skip_typerefs(routine->type);
    if (routine_type_is_nonstatic_member_function(routine_type)) {
      some_function_needs_selector = TRUE;
    }  /* if */
    /* If the first function of a list of functions does not need a selector,
       and the mixed_static_nonstatic says the list contains both static
       and nonstatic functions, then we know at least one function needs
       a selector. */
    if (overloaded_function_case && !some_function_needs_selector &&
        overloaded_function_symbol->variant.overloaded_function.
                                                      mixed_static_nonstatic) {
      some_function_needs_selector = TRUE;
    }  /* if */
    if (some_function_needs_selector) {
      /* We need a selector and we don't have one.  See if a selector
         can be generated from the "this" pointer of the current function. */
      if ((implicit_selector_type = make_implicit_selector_type()) != NULL) {
        have_selector = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (gpp_mode && gnu_version >= 40100 && forced_dependent) {
    /* Weird g++ emulation case, which comes up for the "new" operator:
       Make functions declared after the call visible. */
    allow_post_declared_functions = TRUE;
  }  /* if */
  saved_proj_function_symbol = proj_function_symbol;
retry:
  any_discarded_because_post_decl = FALSE;
  any_not_discarded_because_post_decl = FALSE;
  /* Look at each instance of the overloaded function and see whether or
     not it can match the actual arguments, and if so, how well. */
  for (; proj_function_symbol != NULL;
       proj_function_symbol = overloaded_function_case ? 
                                                   proj_function_symbol->next :
                                                   NULL) {
    a_boolean discarded_because_post_decl;
#if DEBUG
    if (debug_level >= 4 || db_flag_is_set("overload")) {
      db_display_overload_level();
      db_symbol(proj_function_symbol,
                "try_overloaded_function_match: considering ", 4);
    }  /* if */
#endif /* DEBUG */
    /* Determine whether the function is viable by looking at the arguments.
       Add the function to the candidates list if it is viable. */
    determine_function_viability(proj_function_symbol,
                                 is_template_id,
                                 template_arg_list,
                                 (a_symbol_ptr)NULL,
                                 (a_type_ptr)NULL,
                                 arg_operand_list,
                                 have_selector,
                                 bound_function_selector,
                                 implicit_selector_type,
                                 ctor_conversion_case,
                                 initializing_return_value,
                                 effects_copy_initialization,
                                 allow_udc_on_arguments,
                                 arg_dep_lookup_done,
                                 from_arg_dep_lookup,
                                 dependent_call,
                                 known_to_be_visible,
                                 is_overloaded_operator,
                                 allow_post_declared_functions,
                                 candidate_functions,
                                 matched_except_for_missing_selector,
                                 matched_except_for_selector,
                                 &discarded_because_post_decl);
    if (discarded_because_post_decl) {
      any_discarded_because_post_decl = TRUE;
    } else {
      any_not_discarded_because_post_decl = TRUE;
    }  /* if */
  }  /* for */
  if (gpp_mode && gnu_version >= 40100 &&
      *candidate_functions == saved_candidate_functions &&
      any_discarded_because_post_decl &&
      !any_not_discarded_because_post_decl &&
      !allow_post_declared_functions &&
      (dependent_call || is_overloaded_operator)) {
    /* g++ 4.1 continues to allow functions declared after the point of a
       dependent call to be visible, but only if nothing from before the
       call is visible. */
    allow_post_declared_functions = TRUE;
    proj_function_symbol = saved_proj_function_symbol;
    goto retry;
  }  /* if */
}  /* try_overloaded_function_match */

#if MICROSOFT_EXTENSIONS_ALLOWED

a_boolean overloaded_function_match_possible(
                                a_symbol_ptr       overloaded_function_symbol,
                                a_boolean          is_template_id,
                                a_template_arg_ptr template_arg_list,
                                an_arg_operand_ptr arg_operand_list,
                                a_boolean          have_selector,
                                an_operand         *bound_function_selector)
/*
Similar to try_overloaded_function_match, but just returns TRUE if there
are viable functions, FALSE if not.  Issues no errors.
*/
{
  a_boolean                possible;
  a_candidate_function_ptr candidate_functions = NULL;
  a_boolean                matched_except_for_missing_selector = FALSE;
  a_boolean                matched_except_for_selector = FALSE;

  try_overloaded_function_match(overloaded_function_symbol,
                                is_template_id,
                                template_arg_list,
                                arg_operand_list,
                                have_selector,
                                bound_function_selector,
                                /*ctor_conversion_case=*/FALSE,
                                /*initializing_return_value=*/FALSE,
                                /*effects_copy_initialization=*/FALSE,
                                /*allow_udc_on_arguments=*/TRUE,
                                /*arg_dep_lookup_done=*/FALSE,
                                /*from_arg_dep_lookup=*/FALSE,
                                /*dependent_call=*/FALSE,
                                /*forced_dependent=*/FALSE,
                                /*known_to_be_visible=*/FALSE,
                                /*is_overloaded_operator=*/FALSE,
                                &candidate_functions,
                                &matched_except_for_missing_selector,
                                &matched_except_for_selector);
  possible = (candidate_functions != NULL);
  free_candidate_function_list(candidate_functions);
  return possible;
}  /* overloaded_function_match_possible */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void try_surrogate_function_match(
                             an_operand               *class_object,
                             an_arg_operand_ptr       arg_operand_list,
                             a_candidate_function_ptr *candidate_functions)
/*
Find any candidate surrogate functions and add them to the candidate_functions
list.  Look for conversion functions that convert the class object indicated
by class_object to pointer to function.  Each function pointed to
is considered a surrogate function, and its parameters are compared to the
arguments of the call (given by arg_operand_list).
*/
{
  a_symbol_list_entry_ptr slep;
  a_symbol_ptr            surrogate_function_conv_sym;
  a_symbol_ptr            base_surrogate_function_conv_sym;
  a_type_ptr              class_type, conversion_type, routine_type;
  a_boolean               matched_except_for_missing_selector = FALSE;
  a_boolean               matched_except_for_selector = FALSE;

  class_type = skip_typerefs(class_object->type);
  check_assertion(is_immediate_class_type(class_type));
  /* Look at all conversion functions.  (The standard calls for conversion
     functions in accessible base classes, but that seems wrong.) */
  for (slep = symbol_supplement_for_class(class_type)->conversion_list;
       slep != NULL;
       slep = slep->next) {
    surrogate_function_conv_sym = slep->symbol;

#if DEBUG
    if (debug_level >= 4 || db_flag_is_set("overload")) {
      db_display_overload_level();
      db_symbol(surrogate_function_conv_sym,
                "try_surrogate_function_match: considering ", 4);
    }  /* if */
#endif /* DEBUG */
    base_surrogate_function_conv_sym =
                            fundamental_symbol_of(surrogate_function_conv_sym);
    /* Consider only conversion functions to pointer to function type or
       reference to function type or reference to pointer to function type. */
    routine_type = base_surrogate_function_conv_sym->variant.routine.ptr->type;
    routine_type = skip_typerefs(routine_type);
    conversion_type = routine_type->variant.routine.return_type;
    if (is_ptr_or_ref_type(conversion_type)) {
      a_type_ptr underlying_type = type_pointed_to(conversion_type);
      if (is_reference_type(conversion_type) &&
          is_pointer_type(underlying_type)) {
        /* Deal with the reference-to-pointer case. */
        underlying_type = type_pointed_to(underlying_type);
      }  /* if */
      underlying_type = skip_typerefs(underlying_type);
      if (is_function_type(underlying_type)) {
        /* The conversion function returns an appropriate pointer or reference
           to function. */
        /* Find out whether the conversion function is callable for the
           object we have (i.e., how do the cv-qualifiers match up). */
        an_arg_match_summary
                 match;
        a_type_ptr this_param_type =
                 this_param_type_for_overload_res(routine_type,
                                                  surrogate_function_conv_sym,
                                                  /*is_conv_func=*/TRUE);
        determine_selector_match_level(class_object->type,
                                       /*selector_is_object_pointer=*/FALSE,
                                       this_param_type,
                                       &match);
        /* coverity[uninit_use] */ /* Coverity bug */
        if (match.match_level != aml_none) {
          /* See how the arguments match up against the surrogate function
             parameters. */
          a_boolean discarded_because_post_decl;
          determine_function_viability((a_symbol_ptr)NULL,
                                       /*is_template_id=*/FALSE,
                                       (a_template_arg_ptr)NULL,
                                       surrogate_function_conv_sym,
                                       underlying_type,
                                       arg_operand_list,
                                       /*have_selector=*/TRUE,
                                       class_object,
                                       (a_type_ptr)NULL,
                                       /*ctor_conversion_case=*/FALSE,
                                       /*initializing_return_value=*/FALSE,
                                       /*effects_copy_initialization=*/FALSE,
                                       /*allow_udc_on_arguments=*/TRUE,
                                       /*arg_dep_lookup_done=*/FALSE,
                                       /*from_arg_dep_lookup=*/FALSE,
                                       /*dependent_call=*/FALSE,
                                       /*known_to_be_visible=*/FALSE,
                                       /*is_overloaded_operator=*/FALSE,
                                       /*allow_post_declared_functions=*/FALSE,
                                       candidate_functions,
                                       &matched_except_for_missing_selector,
                                       &matched_except_for_selector,
                                       &discarded_because_post_decl);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
}  /* try_surrogate_function_match */


static int compare_standard_conversions(a_std_conv_descr *conv1,
                                        a_std_conv_descr *conv2,
                                        a_boolean        skip_rank_comparisons,
                                        a_boolean        init_conv_after_udc,
                                        a_type_ptr       source_type1,
                                        a_type_ptr       source_type2)
/*
Compare two standard conversions using the ordering criteria of
overload resolution [over.ics.rank], and return 

  +1 if conv1 is a better conversion than conv,
   0 if the two conversions are equal, or
  -1 if conv1 is a worse conversion than conv.

The comparisons that involve rank ordering (e.g., promotion versus
conversion) are skipped if skip_rank_comparisons is TRUE.  This is used
as a speed optimization if those have already been handled.

init_conv_after_udc is TRUE if the conversions are the standard conversions
that follow user-defined conversions in the context of an initialization.
In such cases, the source types of the standard conversions are different
and the destination types are the same, and base class subsequence testing
must be done backwards from the usual way.  source_type1 and source_type2
indicate the source types in that case.  They are NULL when the conversion
uses a template conversion function (the standard conversion after that
is an exact match, by definition, so the source type is not needed).

Note that this routine does not deal with the qualification-conversion
ordering issues (pointer, pointer-to-member, and reference) that appear
in [over.ics.rank].
*/
{
  int              cmp = 0;
  a_base_class_ptr bcp_1, bcp_2;

  if (!skip_rank_comparisons) {
    /* A trivial conversion (i.e., an "exact match") is better than another
       conversion that is nontrivial. */
    if (!conv1->nontrivial_conversion || !conv2->nontrivial_conversion) {
      /* conv1 or conv2 is a trivial conversion (or both are). */
      if (conv2->nontrivial_conversion) {
        /* conv1 is a trivial conversion and conv2 is not, so conv1 is
           better. */
        cmp = 1;
      } else if (conv1->nontrivial_conversion) {
        /* conv2 is a trivial conversion and conv1 is not, so conv2 is
           better. */
        cmp = -1;
      }  /* if */
      goto have_cmp;
    }  /* if */
    /* A promotion is better than a conversion. */
    if (conv1->promotion || conv2->promotion) {
      /* conv1 or conv2 is a promotion (or both are). */
      if (!conv2->promotion) {
        /* conv1 is a promotion and conv2 is not, so conv1 is better. */
        cmp = 1;
      } else if (!conv1->promotion) {
        /* conv2 is a promotion and conv1 is not, so conv2 is better. */
        cmp = -1;
      }  /* if */
      goto have_cmp;
    }  /* if */
  }  /* if */
  if (bool_is_keyword) {
    /* A cast of a pointer or pointer-to-member to bool is worse than
       another conversion that isn't such a cast. */
    if (conv1->ptr_or_pm_to_bool != conv2->ptr_or_pm_to_bool) {
      if (conv1->ptr_or_pm_to_bool) {
        /* conv1 converts a ptr or pointer to member to bool and conv2
           does not, so conv2 is better. */
        cmp = -1;
      } else {
        /* conv2 converts a ptr or pointer to member to bool and conv1
           does not, so conv2 is better. */
        cmp = 1;
      }  /* if */
      goto have_cmp;
    }  /* if */
  }  /* if */
  /* A cast to a base class is better than a cast to further along the
     same base class derivation (see rule [3] in ARM 13.2):
       struct A {};
       struct B : public A {};
       struct C : public B {};
       void f(A*);
       void f(B*);
       int main () {
         C c;
         f(&c);  // C* -> B* is better than C* -> B* -> A*
       }
     Similar processing applies for casts to derived classes, and for
     pointers-to-members.  Also, a cast to "void *" is considered worse
     that any cast to a base class.
  */
  bcp_1 = conv1->cast_base_class;
  bcp_2 = conv2->cast_base_class;
  if (bcp_1 != NULL && bcp_2 != NULL &&
      conv1->reversed_cast == conv2->reversed_cast) {
    /* Both entries have related-class casts, so they can be compared.
       If one is a subsequence of the other, the shorter derivation is
       preferable. */
    if (bcp_1 == bcp_2) {
      /* The same cast in both cases, so the two are equally good. */
    } else if (conv1->reversed_cast != !init_conv_after_udc) {
      /* Normal case: derived --> base pointer cast.  Here, with a hierarchy
         A is-base-of B is-base-of C, we are looking for C* to B* is better
         than C* to A*, or base class B of C is better than base class
         A of C. */
      /* Or, a pointer-to-member conversion for initialization, where the
         destination types are the same.  Here, we are looking for
         B::* to C::* is better than A::* to C::*, which requires the
         same base class relationship test. */
      /* This test formerly used "is_on_any_derivation_of(bcp_2, bcp_1)"
         which makes more sense in the presence of ambiguous base classes
         but is not what the standard requires (see [over.ics.rank]
         paragraph 4 bullet 3).  Likewise the second test below was
         "is_on_any_derivation_of(bcp_1, bcp_2)". */
      if (find_base_class_of(bcp_1->type, bcp_2->type) != NULL) {
        /* bcp_2's type is a base class of bcp_1's type, so bcp_1 is
           preferable. */
        cmp = 1;
      } else if (find_base_class_of(bcp_2->type, bcp_1->type) != NULL) {
        /* bcp_1's type is a base class of bcp_2's type, so bcp_1 is
           preferable. */
        cmp = -1;
      }  /* if */
    } else {
      /* Other case: base --> derived pointer to member cast.  Here, with
         a hierarchy A is-base-of B is-base-of C, we are looking for A::* to
         B::* is better than A::* to C::*, or base class A of B is better
         than base class A of C. */
      /* Or, a pointer conversion for initialization, where the
         destination types are the same.  Here, we are looking for
         B* to A* is better than C* to A*, which requires the
         same base class relationship test. */
      if (find_base_class_of(bcp_2->derived_class,
                             bcp_1->derived_class) != NULL) {
        /* bcp_1's type is a base class of bcp_2's type, so bcp_1 is
           preferable. */
        cmp = 1;
      } else if (find_base_class_of(bcp_1->derived_class,
                                    bcp_2->derived_class) != NULL) {
        /* bcp_2's type is a base class of bcp_1's type, so bcp_2 is
           preferable. */
        cmp = -1;
      }  /* if */
    }  /* if */
  } else {
    if (!init_conv_after_udc) {
      /* Normal case: one source type, two destination types. */
      if (bcp_1 != NULL) {
        /* bcp_1 != NULL, bcp_2 == NULL.  We know that the source type must
           be a class pointer, and not a constant zero, which means the
           conv2 destination type must be "void *" (since an implicit
           conversion is possible).  A base class cast is preferable to a
           cast to "void *", so conv1 is better. */
        cmp = 1;
      } else if (bcp_2 != NULL) {
        /* bcp_1 != NULL, bcp_2 != NULL.  We know that the source type must
           be a class pointer, and not a constant zero, which means the
           conv1 destination type must be "void *" (since an implicit
           conversion is possible).  A base class cast is preferable to a
           cast to "void *", so conv2 is better. */
        cmp = -1;
      }  /* if */
    } else {
      /* Initialization case: two source types, one destination type. */
      if (conv1->pointer_normalization_needed &&
          conv2->pointer_normalization_needed &&
          source_type1 != NULL && source_type2 != NULL) {
        /* The destination type is "void *".  With hierarchy A is-base-of B,
           A* to void* is better than B* to void*. */
        if (is_pointer_type(source_type1) && is_pointer_type(source_type2)) {
          a_type_ptr under_type1 = type_pointed_to(source_type1);
          a_type_ptr under_type2 = type_pointed_to(source_type2);
          if (is_class_struct_union_type(under_type1) &&
              is_class_struct_union_type(under_type2)) {
            /* Both source types are pointers to classes.  See if one class
               is a base class of the other. */
            if (find_base_class_of(under_type2, under_type1)) {
              /* The source for conv1 is a base class of the source for
                 conv2, so conv1 is better. */
              cmp = 1;
            } else if (find_base_class_of(under_type1, under_type2)) {
              /* The source for conv2 is a base class of the source for
                 conv1, so conv2 is better. */
              cmp = -1;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
have_cmp:;
  return cmp;
}  /* compare_standard_conversions */

/*
Return TRUE if the indicated standard conversion is an identity conversion
(i.e., no conversion at all, ignoring lvalue-to-rvalue conversions).
is_ref is TRUE if the parameter has a reference type.  For a reference
binding, type_qualifiers_added does not indicate a qualification
conversion.  Lvalue-to-rvalue conversions are ignored in this test
because of [over.ics.rank] paragraph 3 first bullet first sub-bullet
"excluding any Lvalue Transformation" in the subsequence check.
*/
#define is_identity_conversion(is_ref, conv) \
  (!(conv)->nontrivial_conversion && \
   ((is_ref) || !(conv)->type_qualifiers_added))

/*
Return TRUE if the indicated standard conversion is a qualification
conversion.  is_ref is TRUE if the parameter has a reference type.
For a reference binding, type_qualifiers_added does not indicate a
qualification conversion.
*/
#define is_qualification_conversion(is_ref, conv) \
  (!(conv)->nontrivial_conversion && \
   !(is_ref) && (conv)->type_qualifiers_added)

/*
Return TRUE if the indicated parameter type is a reference type, or if
it is a pointer type for the implicit "this" parameter, which is treated
by the standard as a reference-equivalent in overload resolution.
*/
#define is_ref_or_ref_equivalent(param_type, arg_match) \
  (is_reference_type(param_type) || \
   ((arg_match)->is_match_for_this_param && is_pointer_type(param_type)))


static int compare_argument_tiebreakers(an_arg_match_summary_ptr arg_match1,
                                        an_arg_match_summary_ptr arg_match2)
/*
arg_match1 and arg_match2 are the argument match summaries for the
corresponding parameters of two different functions.  See if any tiebreakers
apply that would make one better than the other, and return 

  +1 if arg_match1 is better than arg_match2,
   0 if arg_match1 and arg_match2 are equally good, or
  -1 if arg_match1 is worse than arg_match2.

*/
{
  int cmp = 0;

  /* We're looking for cases like
       void f(const int *);
       void f(      int *);
       int *p;
       int main () {
         f(p);  // Picks f(int *); f(const int *) is worse because it adds
                // type qualifiers under a pointer or reference.
       }
  */
  check_assertion(arg_match1 != NULL && arg_match2 != NULL);
  /* Use of the deprecated conversion of a string literal to a pointer to
     nonconst can break a tie. */
  if (arg_match1->conversion.std.conv_of_string_literal_to_ptr_to_nonconst !=
      arg_match2->conversion.std.conv_of_string_literal_to_ptr_to_nonconst) {
    if (arg_match1->conversion.std.conv_of_string_literal_to_ptr_to_nonconst) {
      /* Argument 1 uses the deprecated conversion and argument 2 does not,
         so argument 2 is better. */
      cmp = -1;
    } else {
      /* Argument 2 uses the deprecated conversion and argument 1 does not,
         so argument 1 is better. */
      cmp = 1;
    }  /* if */
  }  /* if */
  if (cmp == 0 &&
      (arg_match1->conversion.std.type_qualifiers_added ||
       arg_match2->conversion.std.type_qualifiers_added)) {
    /* There is the possibility of a tie-breaker because of a difference
       in adding cv-qualifiers. */
    /* Get the corresponding parameter types. */
    a_type_ptr param_type1 = arg_match1->param_type;
    a_type_ptr param_type2 = arg_match2->param_type;

    /* Some arguments have no parameter type (e.g., an ellipsis match). */
    if (param_type1 != NULL && param_type2 != NULL) {
      if (any_cfront_mode()) {
        /* cfront has a very simple tiebreaker test for adding cv-qualifiers.
           It applies only when both are pointers or references (except in
           cfront 2.1 mode), and either the match is exact, or it's a
           derived-to-base conversion on matching a "this" parameter. */
        if ((cfront_2_1_mode ||
             (is_ptr_or_ref_type(param_type1) &&
              is_ptr_or_ref_type(param_type2))) &&
            (arg_match1->match_level == (an_arg_match_level)aml_exact ||
             (arg_match1->is_match_for_this_param &&
              arg_match2->is_match_for_this_param))) {
          if (arg_match1->conversion.std.type_qualifiers_added &&
              !arg_match2->conversion.std.type_qualifiers_added) {
            /* Qualifiers are added for param_type1 and not for param_type2,
               so param_type2 is better. */
            cmp = -1;
          } else if (arg_match2->conversion.std.type_qualifiers_added &&
                     !arg_match1->conversion.std.type_qualifiers_added) {
            /* Qualifiers are added for param_type2 and not for param_type1,
               so param_type1 is better. */
            cmp = 1;
          }  /* if */
        }  /* if */
      } else {
        /* Not cfront mode. */
        a_boolean do_subsequence_test =
                           (!microsoft_bugs ||
                            (!arg_match1->lvalue_to_rvalue_conversion_used &&
                             !arg_match2->lvalue_to_rvalue_conversion_used &&
                             (microsoft_version >= 1300 ||
                              (is_ptr_or_ref_type(param_type1) &&
                               is_ptr_or_ref_type(param_type2)))));
        a_boolean param1_is_ref =
                             is_ref_or_ref_equivalent(param_type1, arg_match1);
        a_boolean param2_is_ref =
                             is_ref_or_ref_equivalent(param_type2, arg_match2);
        /* If one conversion sequence is an identity conversion (i.e.,
           no change at all) and the other has a qualification conversion,
           the identity conversion is a subsequence of the other and is
           better. */
        if (do_subsequence_test &&
            is_identity_conversion(param1_is_ref,
                                   &arg_match1->conversion.std) &&
            is_qualification_conversion(param2_is_ref,
                                        &arg_match2->conversion.std)) {
          cmp = 1;
        } else if (do_subsequence_test &&
                   is_identity_conversion(param2_is_ref,
                                          &arg_match2->conversion.std) &&
                   is_qualification_conversion(param1_is_ref,
                                               &arg_match1->conversion.std)) {
          cmp = -1;
        } else {
          /* Test for adding cv-qualifiers immediately below a reference. */
          a_type_ptr           base_param_type1 = param_type1,
                               base_param_type2 = param_type2;
          a_type_qualifier_set qualifiers1 = TQ_NONE,
                               qualifiers2 = TQ_NONE;
          if (param1_is_ref) {
            base_param_type1 = type_pointed_to(param_type1);
            qualifiers1 =
                     simple_qualifiers(get_type_qualifiers(base_param_type1));
          }  /* if */
          if (param2_is_ref) {
            base_param_type2 = type_pointed_to(param_type2);
            qualifiers2 =
                     simple_qualifiers(get_type_qualifiers(base_param_type2));
          }  /* if */
          /* The tiebreaker for adding cv-qualifiers under a reference is
             applied only when both parameters are references, according to the
             standard.  However, in a compatibility mode it is applied when
             at least one parameter is a reference. */
          if ((param1_is_ref && param2_is_ref &&
               (is_rvalue_reference_type(param_type1) ==
                is_rvalue_reference_type(param_type2))) ||
              (single_ref_qual_ovl_res_tiebreaker &&
               (param1_is_ref || param2_is_ref) &&
               /* In Microsoft bugs mode, the tie-breaker applies only when
                  the argument is not a constant, if one parameter is
                  not a pointer or reference. */
               (!microsoft_bugs ||
                (is_ptr_or_ref_type(param_type1) &&
                 is_ptr_or_ref_type(param_type2)) ||
                !arg_match1->arg_is_constant))) {
            /* The tiebreaker applies only when the qualifiers under the
               references are different. */
            if (qualifiers1 != qualifiers2) {
              /* The tiebreaker applies only if the underlying types are the
                 same. */
              if (types_are_compatible_ignoring_qualifiers(base_param_type1,
                                                           base_param_type2)) {
                a_boolean any_extra_in_2 =
                        any_qualifier_in_set_missing(qualifiers1, qualifiers2);
                a_boolean any_extra_in_1 =
                        any_qualifier_in_set_missing(qualifiers2, qualifiers1);
                if (any_extra_in_1 && !any_extra_in_2) {
                  /* param_type1 has more qualifiers than param_type2, so
                     fewer qualifiers are added to get to param_type2, and
                     argument 2 is better. */
                  cmp = -1;
                } else if (any_extra_in_2 && !any_extra_in_1) {
                  /* param_type2 has more qualifiers than param_type1, so
                     fewer qualifiers are added to get to param_type1, and
                     argument 1 is better. */
                  cmp = 1;
                }  /* if */
              }  /* if */
            }  /* if */
          }  /* if */
          if (cmp != 0) {
            /* There was a reference cv-qualifier tiebreaker, so no further
               testing is necessary. */
          } else if (is_pointer_type(param_type1) &&
                     is_pointer_type(param_type2)) {
            /* Check for adding cv-qualifiers under a pointer.  For multi-level
               pointers, the cv-qualifiers can be added at several levels. */
            a_boolean qualifiers_added;
            if (arg_match1->conversion.std.type_qualifiers_added &&
                same_type_with_added_qualifiers(param_type2,
                                                param_type1,
                                                /*ignore_qualifiers=*/FALSE,
                                                &qualifiers_added) &&
                qualifiers_added) {
              /* param_type1 has more qualifiers than param_type2, and the
                 types are otherwise compatible.  Therefore fewer qualifiers
                 are added to get to param_type2, and argument 2 is better. */
              cmp = -1;
            } else if (arg_match2->conversion.std.type_qualifiers_added &&
                       same_type_with_added_qualifiers(
                                                   param_type1,
                                                   param_type2,
                                                   /*ignore_qualifiers=*/FALSE,
                                                   &qualifiers_added) &&
                       qualifiers_added) {
              /* param_type2 has more qualifiers than param_type1, and the
                 types are otherwise compatible.  Therefore fewer qualifiers
                 are added to get to param_type1, and argument 1 is better. */
              cmp = 1;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  /* Use of an anachronism (e.g., calling a const function for a
     non-const object) can break a tie. */
  if (cmp == 0 &&
      arg_match1->tiebreaker_anachronism_used !=
                                     arg_match2->tiebreaker_anachronism_used) {
    if (arg_match1->tiebreaker_anachronism_used) {
      /* Argument 1 uses an anachronism and argument 2 does not, so
         argument 2 is better. */
      cmp = -1;
    } else {
      /* Argument 2 uses an anachronism and argument 1 does not, so cfp1
         is better. */
      cmp = 1;
    }  /* if */
  }  /* if */
  return cmp;
}  /* compare_argument_tiebreakers */


static int compare_reference_matches(an_arg_match_summary *arg_match1,
                                     an_arg_match_summary *arg_match2)
/*
Compare two argument match summary entries and see if one is an rvalue
reference match and the other is an lvalue reference match, and return

  +1 if arg_match1 is a better match than arg_match2,
   0 if the two matches are equal (or this comparison is not applicable), or
  -1 if arg_match1 is a worse match than arg_match2.

Binding an rvalue reference to an argument is better than binding an
lvalue reference to that argument.
*/
{
  int        cmp = 0;
  a_type_ptr arg_type1 = arg_match1->param_type;
  a_type_ptr arg_type2 = arg_match2->param_type;

  if (arg_type1 != NULL && arg_type2 != NULL &&
      is_reference_type(arg_type1) &&
      is_reference_type(arg_type2) &&
      /* This comparison does not apply if either binding is for the
         "this" parameter. */
      !arg_match1->is_match_for_this_param &&
      !arg_match2->is_match_for_this_param &&
      (is_rvalue_reference_type(arg_type1) !=
                                        is_rvalue_reference_type(arg_type2))) {
    if (is_rvalue_reference_type(arg_type1)) {
      /* arg_match1 is an rvalue reference binding and arg_match2 is an lvalue
         reference binding, so arg_match1 is better. */
      cmp = 1;
    } else {
      /* arg_match1 is an lvalue reference binding and arg_match2 is an rvalue
         reference binding, so arg_match2 is better. */
      cmp = -1;
    }  /* if */
  }  /* if */
  return cmp;
}  /* compare_reference_matches */


static int compare_arg_match_levels(an_arg_match_summary *arg_match1,
                                    an_arg_match_summary *arg_match2,
                                    a_boolean            suppress_tiebreakers)
/*
Compare two argument match summary entries and return

  +1 if arg_match1 is a better match than arg_match2,
   0 if the two matches are equal, or
  -1 if arg_match1 is a worse match than arg_match2.

If suppress_tiebreakers is TRUE, ignore tiebreakers (this is used
for a Microsoft bug).
*/
{
  int cmp = 0;

  /* Compare the gross match levels. */
  if (arg_match1->match_level == aml_none ||
      arg_match2->match_level == aml_none) {
    /* The match for the "this parameter" of a static member function
       has a "none" match level.  It's no better and no worse than any
       other match. */
  } else if (arg_match1->anachronism_used != arg_match2->anachronism_used) {
    /* Use of an anachronism (e.g., calling a const function for a
       non-const object) makes a match worse. */
    if (arg_match1->anachronism_used) {
      /* arg_match1 uses an anachronism and arg_match2 does not, so
         arg_match2 is better. */
      cmp = -1;
    } else {
      /* arg_match2 uses an anachronism and arg_match1 does not, so
         arg_match1 is better. */
      cmp = 1;
    }  /* if */
  } else if ((int)arg_match1->match_level < (int)arg_match2->match_level) {
    /* arg_match1 is better. */
    cmp = 1;
  } else if ((int)arg_match1->match_level > (int)arg_match2->match_level) {
    /* arg_match2 is better. */
    cmp = -1;
  } else if (!do_late_ovl_res_tiebreaker &&
             !suppress_tiebreakers &&
             (cmp=compare_argument_tiebreakers(arg_match1, arg_match2)) != 0) {
    /* The argument tiebreakers (applied early, which is the standard-
       conforming way) prefer one match over the other. */
  } else if (rvalue_references_enabled &&
             (cmp = compare_reference_matches(arg_match1, arg_match2)) != 0) {
    /* Binding an rvalue reference to an argument is better than binding an
       lvalue reference to that argument. */
  } else {
    /* The matches are equal in terms of match level.  One can still be
       better than the other in some cases. */
    /* They can only be compared if the user-defined part of the conversion
       (if any) is the same in both conversions. */
    a_routine_ptr arg_routine1 = arg_match1->conversion.routine;
    a_routine_ptr arg_routine2 = arg_match2->conversion.routine;
    if (arg_match1->match_level != (an_arg_match_level)aml_user_conversion) {
      /* Ignore routines indicated for copy constructors on class copies. */
      arg_routine1 = arg_routine2 = NULL;
    }  /* if */
    if (arg_routine1 == arg_routine2 ||
        (microsoft_bugs && arg_routine1 != NULL && arg_routine2 != NULL)) {
      /* The conversions have the same user-defined conversion (or both
         have no user-defined conversion).  (The MSVC++ 6.0 compiler
         doesn't care that the user-defined conversions are different.) */
      /* Compare the standard conversions.  The comparisons that are related
         to rank (e.g., promotion versus conversion) need not be done if
         there is no user-defined conversion (because they have been handled
         already by the match_level test above). */
      cmp = compare_standard_conversions(&arg_match1->conversion.std,
                                         &arg_match2->conversion.std,
                                         /*skip_rank_comparisons=*/
                                                       (arg_routine1 == NULL),
                                         /*init_conv_after_udc=*/FALSE,
                                         (a_type_ptr)NULL,
                                         (a_type_ptr)NULL);
    }  /* if */
  }  /* if */
  return cmp;
}  /* compare_arg_match_levels */


/*
Set the current argument pointer in the indicated candidate function entry
to the first argument match.
*/
#define set_first_arg_match(cfp)                                      \
  ((cfp)->current_arg_match = (cfp)->arg_matches)

/*
Advance the current argument pointer in the indicated candidate function
entry to the next argument match.
*/
#define advance_arg_match(cfp)                                        \
  ((cfp)->current_arg_match = (cfp)->current_arg_match->next)


static a_boolean suppress_microsoft_tiebreakers(
                                    a_candidate_function_ptr cfp1,
                                    a_candidate_function_ptr cfp2)
/*
MSVC++ has certain quirks regarding tiebreakers, copy constructors,
and templates.  Examine the two candidate functions cfp1 and cfp2,
and return TRUE if the tiebreakers should be suppressed for this case.
*/
{
  a_boolean suppress = FALSE;

  check_assertion(microsoft_mode);
  /* The quirks come up when one function is a template and the other
      is not. */
  if (cfp1->is_function_template != cfp2->is_function_template) {
    a_symbol_ptr non_template_sym, template_sym;
    if (cfp1->is_function_template) {
      template_sym = cfp1->function_symbol;
      non_template_sym = cfp2->function_symbol;
    } else {
      template_sym = cfp2->function_symbol;
      non_template_sym = cfp1->function_symbol;
    }  /* if */
    if (non_template_sym != NULL && template_sym != NULL) {
      reduce_projection_symbol_to_fundamental_symbol(non_template_sym);
      reduce_projection_symbol_to_fundamental_symbol(template_sym);
      /* See whether the non-template function is a copy constructor. */
      if (non_template_sym->kind == (a_symbol_kind)sk_member_function) {
        a_routine_ptr rout = non_template_sym->variant.routine.ptr;
        if (rout->special_kind == (a_special_function_kind)sfk_constructor &&
            is_copy_constructor(rout, (a_type_ptr)NULL,
                                (a_type_qualifier_set *)NULL,
                                /*include_move_ctors=*/FALSE,
                                /*is_declarative_context=*/FALSE)) {
          /* A non-template copy constructor against a template.  Suppress
              the tiebreakers. */
          suppress = TRUE;
          /* In MSVC++ 6, certain combinations of generated copy constructors
             and templates with reference parameters get special treatment. */
          if (microsoft_version == 1200 &&
              template_sym->kind == (a_symbol_kind)sk_function_template) {
            a_template_symbol_supplement_ptr tssp;
            a_routine_ptr                    trout;
            a_type_ptr                       trout_type;
            a_routine_type_supplement_ptr    rtsp;
            a_boolean                        ref_param;
            tssp = template_supplement_for_symbol(template_sym);
            trout = tssp->variant.function.routine;
            trout_type = skip_typerefs(trout->type);
            rtsp = trout_type->variant.routine.extra_info;
            ref_param = (rtsp->param_type_list != NULL &&
                         is_lvalue_reference_type(
                                                 rtsp->param_type_list->type));
            suppress = (rout->compiler_generated == ref_param);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return suppress;
}  /* suppress_microsoft_tiebreakers */


static int compare_late_tiebreakers(a_candidate_function_ptr cfp1,
                                    a_candidate_function_ptr cfp2)
/*
Compare the argument matches of the indicated candidate function calls,
which are equally good overall so far, and look for tie-breakers.
Return 

  +1 if cfp1 is better than cfp2,
   0 if cfp1 and cfp2 are equally good, or
  -1 if cfp1 is worse than cfp2.

This routine is used only when checking the tiebreakers "late", i.e.,
after all the argument matches for two functions have been found to be
otherwise equivalent.  This is nonstandard, but it's what some compilers
(e.g., cfront) do.
*/
{
  int                      result_cmp = 0;
  an_arg_match_summary_ptr arg1, arg2;

  if (any_cfront_mode() &&
      (cfp1->is_function_template || cfp2->is_function_template)) {
    /* Cfront doesn't consider tiebreakers for templates. */
  } else if (microsoft_bugs &&
             suppress_microsoft_tiebreakers(cfp1, cfp2)) {
    /* MSVC++ has some quirks with tiebreakers, copy constructors, and
       templates. */
  } else {
    /* Compare each argument. */
    for (arg1 = cfp1->arg_matches, arg2 = cfp2->arg_matches;
         arg1 != NULL;
         arg1 = arg1->next, arg2 = arg2->next) {
      int cmp = compare_argument_tiebreakers(arg1, arg2);
      if (cmp != 0) {
        /* This tie-breaker applies only if no other arguments contradict
           it. */
        if (result_cmp == 0) {
          /* No previous argument had a tiebreaker.  Remember this one and
             keep going to see if any later argument contradicts it. */
          result_cmp = cmp;
        } else if (result_cmp != cmp) {
          /* This contradicts a previous argument, so the tie-breaker does
             not apply. */
          result_cmp = 0;
          break;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  return result_cmp;
}  /* compare_late_tiebreakers */


static int compare_copy_constructors_for_microsoft(
                                                 a_candidate_function_ptr cfp1,
                                                 a_candidate_function_ptr cfp2)
/*
Compare two candidate functions in Microsoft bugs mode and return

  +1 if cfp1 is better than cfp2,
   0 if cfp1 and cfp2 are equally good, or
  -1 if cfp1 is worse than cfp2

on the basis that MSVC++ prefers copy constructors over other functions.
*/
{
  int          cmp = 0;
  a_symbol_ptr sym1 = cfp1->function_symbol;
  a_symbol_ptr sym2 = cfp2->function_symbol;

  if (sym1 != NULL && sym2 != NULL &&
      !cfp1->is_function_template && !cfp2->is_function_template) {
    a_routine_ptr rout1, rout2;
    a_boolean     is_cctor1, is_cctor2;
    reduce_projection_symbol_to_fundamental_symbol(sym1);
    reduce_projection_symbol_to_fundamental_symbol(sym2);
    check_assertion(sym1->kind == (a_symbol_kind)sk_routine ||
                    sym1->kind == (a_symbol_kind)sk_member_function);
    check_assertion(sym2->kind == (a_symbol_kind)sk_routine ||
                    sym2->kind == (a_symbol_kind)sk_member_function);
    rout1 = sym1->variant.routine.ptr;
    rout2 = sym2->variant.routine.ptr;
    is_cctor1 = (rout1->special_kind ==
                                    (a_special_function_kind)sfk_constructor &&
                 is_copy_constructor(rout1, (a_type_ptr)NULL,
                                     (a_type_qualifier_set *)NULL,
                                     /*include_move_ctors=*/FALSE,
                                     /*is_declarative_context=*/FALSE));
    is_cctor2 = (rout2->special_kind ==
                                    (a_special_function_kind)sfk_constructor &&
                 is_copy_constructor(rout2, (a_type_ptr)NULL,
                                     (a_type_qualifier_set *)NULL,
                                     /*include_move_ctors=*/FALSE,
                                     /*is_declarative_context=*/FALSE));
    if (is_cctor1 && !is_cctor2) {
      cmp = 1;
    } else if (is_cctor2 && !is_cctor1) {
      cmp = -1;
    }  /* if */
  }  /* if */
  return cmp;
}  /* compare_copy_constructors_for_microsoft */


static a_type_ptr candidate_return_type(a_candidate_function_ptr cfp)
/*
Return the return type of the indicated candidate function.  Return NULL
if the return type is not available (e.g., for a template).
*/
{
  a_symbol_ptr sym = cfp->function_symbol;
  a_type_ptr   type = NULL;

  if (sym != NULL) {
    sym = fundamental_symbol_of(sym);
    /* Don't process templates. */
    if (sym->kind != (a_symbol_kind)sk_function_template) {
      type = routine_symbol_type(sym);
      type = return_type_of(type);
    }  /* if */
  } /* if */
  return type;
}  /* candidate_return_type */


static a_boolean candidate_return_type_same_with_added_qualifiers(
                                                 a_candidate_function_ptr cfp1,
                                                 a_candidate_function_ptr cfp2)
/*
Return TRUE if the return type of the function indicated in the candidate
function entry cfp1 is the same as the return type for cfp2 except that
the former has additional type qualifiers.
*/
{
  a_boolean    same_with_added_qualifiers = FALSE, qualifiers_added;
  a_symbol_ptr sym1 = cfp1->function_symbol;
  a_symbol_ptr sym2 = cfp2->function_symbol;

  if (sym1 != NULL && sym2 != NULL) {
    a_type_ptr type1 = candidate_return_type(cfp1);
    a_type_ptr type2 = candidate_return_type(cfp2);
    if (type1 != NULL && type2 != NULL &&
        same_type_with_added_qualifiers(type2, type1,
					/*ignore_qualifiers=*/FALSE,
                                        &qualifiers_added) &&
        qualifiers_added) {
      same_with_added_qualifiers = TRUE;
    }  /* if */
  }  /* if */
  return same_with_added_qualifiers;
}  /* candidate_return_type_same_with_added_qualifiers */


static int compare_template_candidate_functions(a_candidate_function_ptr cfp1,
                                                a_candidate_function_ptr cfp2)
/*
Compare two candidate functions.  If they can be distinguished on the
basis of the template/non-template comparison described in [over.match.best] of
the standard, return cmp set accordingly:

  +1 if cfp1 is better than cfp2,
   0 if cfp1 and cfp2 are equally good, or
  -1 if cfp1 is worse than cfp2.

*/
{
  int cmp = 0;

  if (cfp1->is_function_template != cfp2->is_function_template) {
    if (cfp1->is_function_template) {
      /* cfp1 is a function template and cfp2 is not, so cfp2 is better. */
      cmp = -1;
    } else {
      /* cfp2 is a function template and cfp1 is not, so cfp1 is better. */
      cmp = 1;
    }  /* if */
  }  /* if */
  return cmp;
}  /* compare_template_candidate_functions */


static int compare_candidate_functions(a_candidate_function_ptr cfp1,
                                       a_candidate_function_ptr cfp2)
/*
Compare two candidate functions for which the argument matches have been
determined to be same to see if there is anything about the functions
themselves or the overall call that makes one function preferable to the
other.  Return

  +1 if cfp1 is better than cfp2,
   0 if cfp1 and cfp2 are equally good, or
  -1 if cfp1 is worse than cfp2.

*/
{
  int                  cmp = 0;
  a_type_qualifier_set cfp1_type_qualifiers_added = FALSE,
                       cfp2_type_qualifiers_added = FALSE;

  if (cfp1->is_user_conversion) {
    cfp1_type_qualifiers_added = cfp1->conversion.std.type_qualifiers_added;
    cfp2_type_qualifiers_added = cfp2->conversion.std.type_qualifiers_added;
  }  /* if */
  /* Note that the tests here must be ordered from most significant
     to least significant. */
  if (do_late_ovl_res_tiebreaker &&
      (cmp = compare_late_tiebreakers(cfp1, cfp2)) != 0) {
    /* There is something about one argument list that makes it better
       than the other. */
  } else if (!late_template_ovl_res_tiebreaker &&
             (cmp = compare_template_candidate_functions(cfp1, cfp2)) != 0) {
    /* The fact that one function is a function template and the other
       is not can serve as a tie-breaker. */
  } else if (cfp1->is_user_conversion &&
             (cmp = compare_standard_conversions(&cfp1->conversion.std,
                                                 &cfp2->conversion.std,
                                                 /*skip_rank_comparisons=*/
                                                                         FALSE,
                                                 /*init_conv_after_udc=*/TRUE,
                                                 candidate_return_type(cfp1),
                                                 candidate_return_type(cfp2)))
                                                                        != 0) {
    /* The conversions are user-defined conversions followed by standard
       conversions, and the standard conversion in one case is better than the
       standard conversion in the other case. */
  } else if (cfp1_type_qualifiers_added &&
            candidate_return_type_same_with_added_qualifiers(cfp2, cfp1)) {
    /* The fact that type qualifiers were added after a conversion
       function can serve as a tie-breaker. */
    /* More type qualifiers were added on cfp1, so cfp2 is better. */
    cmp = -1;
  } else if (cfp2_type_qualifiers_added &&
             candidate_return_type_same_with_added_qualifiers(cfp1, cfp2)) {
    /* The fact that type qualifiers were added after a conversion
       function can serve as a tie-breaker. */
    /* More type qualifiers were added on cfp2, so cfp1 is better. */
    cmp = 1;
  } else if ((microsoft_bugs || sun_mode) &&
             (cmp = compare_copy_constructors_for_microsoft(cfp1, cfp2)) != 0){
    /* MSVC++ favors copy constructors over other functions, as a way
       of making their funny "copy-initialization is direct-initialization"
       rules work. */
  } else if (cfp1->uses_microsoft_explicit_anachronism !=
             cfp2->uses_microsoft_explicit_anachronism) {
    /* One of the functions is an explicit constructor let by as an
       anachronism, and the other isn't. */
    if (cfp1->uses_microsoft_explicit_anachronism) {
      /* cfp1 uses the anachronism and cfp2 doesn't, so cfp2 is better. */
      cmp = -1;
    } else {
      /* cfp2 uses the anachronism and cfp1 doesn't, so cfp1 is better. */
      cmp = 1;
    }  /* if */
  } else if (late_template_ovl_res_tiebreaker &&
             (cmp = compare_template_candidate_functions(cfp1, cfp2)) != 0) {
    /* The fact that one function is a function template and the other
       is not can serve as a tie-breaker. */
  } else if (cfp1->is_function_template && cfp2->is_function_template) {
    /* cfp1 and cfp2 are function templates.  Determine whether either of
       the templates is more specialized than the other. */
    cmp = compare_function_templates(cfp1->function_symbol,
                                     cfp2->function_symbol,
                                     /*entire_type=*/FALSE);
  }  /* if */
  return cmp;
}  /* compare_candidate_functions */


static a_boolean same_candidate_function(a_candidate_function_ptr cfp1,
                                         a_candidate_function_ptr cfp2)
/*
Return TRUE if the candidate functions cfp1 and cfp2 are the same function.
*/
{
  a_boolean    same = FALSE;
  a_symbol_ptr sym1 = cfp1->function_symbol;
  a_symbol_ptr sym2 = cfp2->function_symbol;

  if (sym1 != NULL && sym2 != NULL) {
    if (sym1 == sym2) {
      same = TRUE;
    } else if (sym1->kind == (a_symbol_kind)sk_projection &&
               sym2->kind == (a_symbol_kind)sk_projection &&
               !same_base_classes(sym1->variant.projection.extra_info->
                                                     fundamental_base_class,
                                  sym2->variant.projection.extra_info->
                                                     fundamental_base_class)) {
      /* When dealing with class member projections, if the subobjects
         involved are different (e.g., because of an ambiguous base class)
         the functions are different because they deal with different base
         class subobjects. */
      /* same = FALSE; -- already set. */
    } else {
      sym1 = fundamental_symbol_of(sym1);
      sym2 = fundamental_symbol_of(sym2);
      if (sym1 == sym2) {
        same = TRUE;
      } else if (sym1->kind == sym2->kind) {
        if (sym1->kind == (a_symbol_kind)sk_routine ||
            sym1->kind == (a_symbol_kind)sk_member_function) {
           /* Compare IL entry pointers to deal with block extern symbols. */
          a_routine_ptr rout1 = sym1->variant.routine.ptr;
          a_routine_ptr rout2 = sym2->variant.routine.ptr;
          same = corresponding_routines(rout1, rout2);
          if (!same && gpp_mode) {
            /* In g++ mode we create distinct routine entries for extern "C"
               functions in different namespaces.  g++ treats such routines
               as identical if they have the same type. */
            a_routine_ptr	rp1 = sym1->variant.routine.ptr;
            a_routine_ptr	rp2 = sym2->variant.routine.ptr;
            if (rp1->source_corresp.name_linkage ==
                                          (a_name_linkage_kind)nlk_external &&
                rp2->source_corresp.name_linkage ==
                                          (a_name_linkage_kind)nlk_external &&
                identical_types(rp1->type, rp2->type)) {
              same = TRUE;
            }  /* if */
          }  /* if */
        } else {
          /* A function template.  Compare the canonical a_template entries. */
          a_template_ptr temp1, temp2;
          check_assertion(sym1->kind == (a_symbol_kind)sk_function_template);
          temp1 = sym1->variant.template_info->il_template_entry->
                                                            canonical_template;
          temp2 = sym2->variant.template_info->il_template_entry->
                                                            canonical_template;
          same = corresponding_templates(temp1, temp2);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return same;
}  /* same_candidate_function */


static a_boolean match_is_better_on_at_least_one_arg(
                                           a_candidate_function_ptr best_cfp,
                                           a_candidate_function_ptr candidates)
/*
Compare the candidate function best_cfp against all the other candidate
functions in candidates.  Return TRUE if best_cfp's arguments matches are
a strictly better match for at least one argument for each of the other
functions (not necessarily the same argument for each function).  This is
the final test required for overload resolution (see ARM 13.2).
Note that the candidate function can also be ruled better on the basis
of something based strictly on the function itself or the call context
(as judged by compare_candidate_functions).
*/
{
  a_candidate_function_ptr cfp;
  a_boolean                match_is_better = TRUE;
  an_arg_match_summary_ptr curr_arg, best_curr_arg;

  /* Note that we start with the assumption that the function we're checking
     is at least as good on every argument as all of the other functions.
     That's established by the first part of overload resolution.  Here, we
     only have to look for some argument on which the chosen function is
     better than each of the other functions. */
  /* Loop through all the other functions. */
  for (cfp = candidates; cfp != NULL; cfp = cfp->next) {
    if (cfp == best_cfp) {
      /* Skip the chosen function itself. */
    } else if (same_candidate_function(cfp, best_cfp)) {
      /* Skip other symbols that are the same function, which can appear
         in synthesized overload sets. */
    } else {
      /* Compare the match level of each argument of the chosen function
         with the same argument of another function. */
      set_first_arg_match(best_cfp);
      set_first_arg_match(cfp);
      for (;;) {
        best_curr_arg = best_cfp->current_arg_match;
        if (best_curr_arg == NULL) break;
        curr_arg = cfp->current_arg_match;
        /* As soon as we find an argument for which the chosen
           function is better than the other function, we can stop
           checking.  Consider an error match to be (possibly) better
           than another match. */
        if (compare_arg_match_levels(best_curr_arg, curr_arg,
                                     /*suppress_tiebreakers=*/FALSE) > 0 ||
            best_curr_arg->match_level == aml_error) {
          goto check_next_function;
        }  /* if */
        advance_arg_match(best_cfp);
        advance_arg_match(cfp);
      }  /* for */
      /* All the argument matches have the same level. */
      /* See if the "best" function is better than the other for some reason
         related to the function instead of the arguments. */
      if (compare_candidate_functions(best_cfp, cfp) > 0) {
        goto check_next_function;
      }  /* if */
      /* The chosen function is not any better than this other function. */
      match_is_better = FALSE;
      /* Make sure the other function is listed in the ambiguity error
         message. */
      cfp->in_best_match_set_for_some_argument = TRUE;
      break;
    }  /* if */
check_next_function:;
  }  /* for */
  return match_is_better;
}  /* match_is_better_on_at_least_one_arg */


static an_arg_match_level worst_arg_match_level_for_candidate_arg(
                                            a_candidate_function_ptr candidate)
/*
Return the argument match level for the worst argument match of the
given candidate function.
*/
{
  an_arg_match_level       worst_match = (an_arg_match_level)aml_exact;
  an_arg_match_summary_ptr amsp;

  for (amsp = candidate->arg_matches; amsp != NULL; amsp = amsp->next) {
    if ((int)amsp->match_level > (int)worst_match) {
      worst_match = amsp->match_level;
    }  /* if */
  }  /* for */
  return worst_match;
}  /* worst_arg_match_level_for_candidate_arg */

#if GNU_EXTENSIONS_ALLOWED

static a_candidate_function_ptr select_best_gpp_candidate(
                                           a_candidate_function_ptr candidates)
/*
g++ has an "extension" that chooses one function match over
another if the worst conversion for its arguments is not as bad
as the worst conversion for another function's arguments.
candidates gives the set of candidate functions.
If there is a function that is least-worst, return a pointer to
its candidate function entry.  Otherwise, return NULL.
*/
{
  a_candidate_function_ptr cfp, best_cfp = NULL;
  an_arg_match_level       worst_match, best_worst_match = aml_none;

  for (cfp = candidates; cfp != NULL; cfp = cfp->next) {
    if (best_cfp != NULL &&
        same_candidate_function(best_cfp, cfp)) {
      /* This function is the same as the best one, so skip it. */
    } else {
      worst_match = worst_arg_match_level_for_candidate_arg(cfp);
      if ((int)worst_match < (int)best_worst_match) {
        /* A new best function. */
        best_cfp = cfp;
        best_worst_match = worst_match;
      } else if ((int)worst_match > (int)best_worst_match) {
        /* The worst match for this candidate is worse than the best
           worst match we've seen previously, so ignore it. */
      } else {
        /* A tie between the best so far and this one, so neither
           one is best.  Note that best_worst_match remains set so
           that only better functions will be considered in the
           rest of the list. */
        best_cfp = NULL;
      }  /* if */
    }  /* if */
  }  /* for */
  if (best_cfp != NULL) {
    /* When the candidate selected is not a built-in operator, g++ (3.2
       through 3.4, at least) gives an error, which is equivalent to
       considering the case ambiguous.  Built-in operators do take
       advantage of this extension. */
    if (best_cfp->operand_type_pattern == NULL) best_cfp = NULL;
  }  /* if */
  return best_cfp;
}  /* select_best_gpp_candidate */

#endif /* GNU_EXTENSIONS_ALLOWED */

static a_boolean function_candidate_with_same_sig_as_builtin_present(
                                  a_candidate_function_ptr builtin_cfp,
                                  a_candidate_function_ptr candidate_functions)
/*
Return TRUE if on the list of candidate_functions there is a function
with the same signature as the builtin operator indicated by builtin_cfp.
*/
{
  a_boolean present = FALSE;

  /* See 13.3.1.2 [over.match.oper] paragraph 2 bullet 3 sub-bullet 4
     in the C++98 standard:

     -- do not have the same parameter type list as any non-template
        non-member candidate.

     Note that because there aren't any built-in operators that
     take operands of class types, the real issue is only
     builtin operators that take enum types.
  */
  if (builtin_cfp->operand_type_pattern[0] == 'E' &&
      builtin_cfp->operand_type_pattern[1] == 'E') {
    a_candidate_function_ptr cfp;
    for (cfp = candidate_functions; cfp != NULL; cfp = cfp->next) {
      if (builtin_cfp != cfp &&
          cfp->function_symbol != NULL &&
          !cfp->is_function_template &&
          !cfp->function_symbol->is_class_member) {
        /* Compare the function parameter types to the builtin operator
           operand types. */
        an_arg_match_summary_ptr arg = cfp->arg_matches;
        char *type_code = builtin_cfp->operand_type_pattern;
        for (; arg != NULL; arg = arg->next, type_code++) {
          check_assertion(arg->param_type != NULL);
          if (!type_matches_type_code(arg->param_type, *type_code)) {
            /* This function does not match. */
            goto next_function;
          }  /* if */
        }  /* for */
        /* This function matches. */
        present = TRUE;
        break;
      }  /* if */
next_function:;
    }  /* for */
  }  /* if */
  return present;
}  /* function_candidate_with_same_sig_as_builtin_present */


static a_boolean has_anachronism_match(a_candidate_function_ptr cfp)
/*
Return TRUE if any of the argument matches for the given candidate
function required use of anachronisms.
*/
{
  a_boolean                any_anachronism_match = FALSE;
  an_arg_match_summary_ptr amsp;  

  for (amsp = cfp->arg_matches; amsp != NULL; amsp = amsp->next) {
    if (amsp->anachronism_used) {
      any_anachronism_match = TRUE;
      break;
    }  /* if */
  }  /* for */
  return any_anachronism_match;
}  /* has_anachronism_match */


static a_param_type_ptr orig_unevaluated_param_type(a_param_type_ptr ptp)
/*
Chase the orig_param_type_for_unevaluated_default_arg_expr link for the
indicated parameter type entry, and return the ultimate entry pointed to.
*/
{
  a_param_type_ptr res_ptp = 
                         ptp->orig_param_type_for_unevaluated_default_arg_expr;

  if (res_ptp != NULL) {
    for (;;) {
      a_param_type_ptr next_ptp = 
                     res_ptp->orig_param_type_for_unevaluated_default_arg_expr;
      if (next_ptp == NULL || next_ptp == res_ptp) break;
      res_ptp = next_ptp;
    }  /* for */
  }  /* if */
  return res_ptp;
}  /* orig_unevaluated_param_type */


static a_boolean equiv_template_routine_types(a_type_ptr type1,
                                              a_type_ptr type2)
/*
Return TRUE if the two given routine types are different copies of the
routine type for the same template instance.  This check is done by
looking for equivalent unevaluated default arguments, so this is not
a general-purpose routine.
*/
{
  a_boolean                     equiv = FALSE;
  a_routine_type_supplement_ptr rtsp1, rtsp2;
  a_param_type_ptr              ptp1, ptp2;

  if (type1->kind == (a_type_kind)tk_routine &&
      type2->kind == (a_type_kind)tk_routine) {
    rtsp1 = type1->variant.routine.extra_info;
    rtsp2 = type2->variant.routine.extra_info;
    ptp1 = rtsp1->param_type_list;
    ptp2 = rtsp2->param_type_list;
    for (; ptp1 != NULL && ptp2 != NULL;
         ptp1 = ptp1->next, ptp2 = ptp2->next) {
      if (ptp1->orig_param_type_for_unevaluated_default_arg_expr != NULL &&
          orig_unevaluated_param_type(ptp1) ==
          orig_unevaluated_param_type(ptp2)) {
        equiv = TRUE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return equiv;
}  /* equiv_template_routine_types */


static void instantiate_default_arguments_of_template_matching(
                                       a_type_ptr               type,
                                       a_candidate_function_ptr best_candidate,
                                       a_type_ptr               *updated_type)
/*
Instantiate the unevaluated default arguments of whatever template on the
argument list of the candidate function indicated by best_candidate
matches the function type "type".  There must be one.  *updated_type is
set to an updated version of the original type (to be put back into the
template argument list), or to the original type if no update is required.
*/
{
  an_arg_match_summary_ptr arg_match;

  *updated_type = type;
  /* If any argument was a template, its symbol was recorded in the
     argument match.  Look for any of those and see which one matches the
     type we have. */
  for (arg_match = best_candidate->arg_matches;
       arg_match != NULL;
       arg_match = arg_match->next) {
    a_symbol_ptr sym = arg_match->template_symbol;
    if (sym != NULL) {
      a_type_ptr rout_type;
      a_boolean  equiv_type = FALSE;
      reduce_projection_symbol_to_fundamental_symbol(sym);
      check_assertion(sym->kind == (a_symbol_kind)sk_routine ||
                      sym->kind == (a_symbol_kind)sk_member_function);
      rout_type = routine_symbol_type(sym);
      if (rout_type != type) {
        /* Check for a type that came from the same template but is not
           the same copy. */
        if (equiv_template_routine_types(rout_type, type)) {
          equiv_type = TRUE;
          *updated_type = rout_type;
        }  /* if */
      }  /* if */
      if (rout_type == type || equiv_type) {
        /* Found a match.  Instantiate its unevaluated default arguments. */
        a_routine_type_supplement_ptr rtsp =
                                         rout_type->variant.routine.extra_info;
#if CHECKING
        a_boolean processed_any = FALSE;
#endif /* CHECKING */
        a_param_type_ptr ptp;
        for (ptp = rtsp->param_type_list; ptp != NULL; ptp = ptp->next) {
          if (ptp->has_unevaluated_template_default) {
            instantiate_default_argument(sym, ptp);
#if CHECKING
            processed_any = TRUE;
          } else if (ptp->orig_param_type_for_unevaluated_default_arg_expr !=
                                                                        NULL) {
            /* There was an unevaluated default argument here, but it got
               evaluated for some other reason since we recorded it. */
            processed_any = TRUE;
#endif /* CHECKING */
          }  /* if */
        }  /* for */
#if CHECKING
        check_assertion(processed_any || total_errors != 0);
#endif /* CHECKING */
        goto done;
      }  /* if */
    }  /* if */
  }  /* for */
  check_assertion_str(total_errors != 0,
     "instantiate_default_arguments_of_template_matching: template not found");
done:;
}  /* instantiate_default_arguments_of_template_matching */


static void instantiate_template_default_arguments(
                                       a_candidate_function_ptr best_candidate)
/*
The candidate function identified by best_candidate is a function template
and is the candidate selected by overload resolution.  The corresponding
template function is about to be instantiated.  If the template arguments
contain any function types with uninstantiated default arguments,
instantiate those default argument expressions now because the template
and its function type are about to be separated as the function type is
used to instantiate a function.  This can only happen in a nonstandard
mode indicated by nonstandard_default_arg_deduction; usually, the
default arguments are removed from any function types as they enter
type deduction.
*/
{
  a_template_arg_ptr tap;

  check_assertion(best_candidate->is_function_template &&
                  nonstandard_default_arg_deduction);
  begin_template_arg_list_traversal_simple(best_candidate->template_arg_list,
                                           &tap);
  for (; tap != NULL; advance_to_next_template_arg_simple(&tap)) {
    if (tap->kind == (a_templ_arg_kind)tak_type) {
      a_type_ptr type = tap->variant.type;
      a_type_ptr orig_type = type;
      /* Look for function types, pointer-to-function types, and pointer-to-
         member-function types.  Uninstantiated default arguments can only
         appear at that kind of "top level." */
      if (is_pointer_type(type)) {
        type = type_pointed_to(type);
      } else if (is_ptr_to_member_type(type)) {
        type = pm_member_type(type);
      }  /* if */
      type = skip_typerefs(type);
      if (is_function_type(type)) {
        a_routine_type_supplement_ptr rtsp = type->variant.routine.extra_info;
        a_param_type_ptr ptp = rtsp->param_type_list;
        /* Go through the parameters of the function type. */
        for (; ptp != NULL; ptp = ptp->next) {
          if (ptp->has_unevaluated_template_default) {
            /* This parameter type (from something in the template argument
               list) has an uninstantiated default argument.  Instantiate it
               by finding the corresponding template on the call arguments
               list. */
            a_type_ptr updated_type;
            instantiate_default_arguments_of_template_matching(type,
                                                               best_candidate,
                                                               &updated_type);
            if (updated_type != type) {
              /* Update the type in the template argument list (It was a copy
                 of the type in the template, and we replace it by the
                 original type in the template, which now has the instantiated
                 default arguments). */
              if (is_pointer_type(orig_type)) {
                updated_type = make_pointer_type(updated_type);
              } else if (is_ptr_to_member_type(orig_type)) {
                updated_type = ptr_to_member_type(updated_type,
                                                  pm_class_type(orig_type));
              }  /* if */
              tap->variant.type = updated_type;
            }  /* if */
            break;
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
  }  /* for */
}  /* instantiate_template_default_arguments */


static void select_best_candidate_functions(
                        a_candidate_function_ptr *candidate_functions,
                        a_source_position        *source_pos,
                        a_boolean                *undecidable_because_of_error,
                        a_boolean                *ambiguous)
/*
*candidate_functions is the list of viable functions for a particular
overloaded function call.  From that set, select the best functions
and set *candidate_functions to that set.  Other candidate functions
that do not make the "best" set are freed.  On return from this function,
the *candidate_functions list has no members if there are no viable
functions, has more than one member if the call is ambiguous, and
has exactly one member if the call is valid.  *source_pos is the source
position of the reference.  If the call is ambiguous, *ambiguous
is set to TRUE.  If the best functions could not be selected because
there were error arguments in the matches, *undecidable_because_of_error
is returned TRUE, *candidate_functions is set to NULL, and *ambiguous
is set to TRUE.
*/
{
  a_candidate_function_ptr candidates = *candidate_functions;
  a_candidate_function_ptr cfp, best_cfp, end_candidate_functions, cfp_next;
  a_candidate_function_ptr prev_cfp;
  unsigned long            number_in_best_match_set;
  an_arg_match_summary_ptr curr_arg;
  int                      cmp;
  a_boolean                overall_ambiguity = FALSE, any_error_match = FALSE;
  a_boolean                have_candidates_without_anachronisms = FALSE;

  db_enter(4, "select_best_candidate_functions");
#if DEBUG
  if (debug_level >= 4 || db_flag_is_set("overload")) {
    db_display_overload_level();
    fprintf(f_debug, "Entry to select_best_candidate_functions: ");
    db_candidate_function_list(candidates);
  }  /* if */
#endif /* DEBUG */
  *undecidable_because_of_error = FALSE;
  *ambiguous = FALSE;
  candidates = *candidate_functions;
  /* If there are no functions or there is exactly one function, the
     list is already correct. */
  if (candidates != NULL && candidates->next != NULL) {
    /* More than one, so we will have to choose a "best" function. */
    /* The algorithm here is the one described in ARM 13.2: "The best-matching
       function is the intersection of sets of functions that best match on
       each argument.  Unless this intersection has exactly one member, the
       call is illegal.  The function thus selected must be a strictly better
       match for at least one argument than every other possible function
       (but not necessarily the same argument for each function).  Otherwise,
       the call is illegal." */
    /* We form the intersection of best-match sets by putting all functions
       in the best-match set and then doing an intersection after each argument
       best-match set is determined. */
    /* Put all functions in the best-match set, and set the current argument
       for each function to the first one. */
    number_in_best_match_set = 0;
    prev_cfp = NULL;
    for (cfp = candidates; cfp != NULL; cfp = cfp_next) {
      a_boolean uses_anachronism = has_anachronism_match(cfp);
      cfp_next = cfp->next;
      /* Look for a candidate that is a builtin operator that has the
         same signature as a function in the candidate set, and eliminate
         the builtin operator. */
      /* Also eliminate a candidate that uses anachronism matches, if
         we've already seen one that used no such matches. */
      if ((cfp->operand_type_pattern != NULL &&
           function_candidate_with_same_sig_as_builtin_present(cfp,
                                                              candidates)) ||
          (uses_anachronism && have_candidates_without_anachronisms)) {
        /* Remove the candidate. */
        if (prev_cfp == NULL) {
          candidates = *candidate_functions = cfp_next;
        } else {
          prev_cfp->next = cfp_next;
        }  /* if */
        cfp->next = NULL;
        free_candidate_function_list(cfp);
      } else {
        /* The entry stays in the candidate functions set. */
        if (!uses_anachronism) {
          /* This candidate uses no anachronism matches. */
          if (!have_candidates_without_anachronisms) {
            /* Any previously-considered candidates must require anachronism
               matches.  Remove them. */
            if (prev_cfp != NULL) {
              prev_cfp->next = NULL;
              free_candidate_function_list(candidates);
              number_in_best_match_set = 0;
              candidates = *candidate_functions = cfp;
            }  /* if */
            have_candidates_without_anachronisms = TRUE;
          }  /* if */
        }  /* if */
        cfp->in_best_match_set = TRUE;
        cfp->in_best_match_set_for_some_argument = FALSE;
        number_in_best_match_set++;
        set_first_arg_match(cfp);
        if (cfp->function_symbol != NULL &&
            is_ambiguous_by_inheritance(cfp->function_symbol)) {
          /* A candidate is an ambiguous symbol, which is an arbitrary
             representative of a set of functions that collided due
             to inheritance.  The overload resolution is ambiguous. */
          overall_ambiguity = TRUE;
        }  /* if */
        prev_cfp = cfp;
      }  /* if */
    }  /* for */
    if (overall_ambiguity) goto create_final_list;
    /* If we have only one candidate left after removing builtin operators
       above, skip the rest of the processing. */
    if (number_in_best_match_set == 1) goto create_final_list;
    /* Loop for each argument. */
    while (candidates->current_arg_match != NULL) {
      /* Find the best-match set for this argument. */
      a_candidate_function_ptr best_match_set_for_curr_arg = NULL;
      /* Loop for each candidate function.  Look at the current argument
         under each function to find the best matches. */
      for (cfp = candidates; cfp != NULL; cfp = cfp->next) {
        a_boolean new_match_worse_than_some_match_in_set = FALSE;
        curr_arg = cfp->current_arg_match;
        if (curr_arg->match_level == aml_error) {
          /* Always add error matches to the best-match set. */
          any_error_match = TRUE;
        } else {
          a_candidate_function_ptr func_in_set, prev_func_in_set = NULL;
          for (func_in_set = best_match_set_for_curr_arg;
               func_in_set != NULL;
               func_in_set = func_in_set->next_in_arg_best_match_set) {
            /* Compare the current argument match level against one match
               in the set of best matches so far on this argument. */
            a_boolean suppress_tiebreakers = FALSE;
            if (microsoft_bugs &&
                suppress_microsoft_tiebreakers(cfp, func_in_set)) {
              /* The Microsoft compiler suppresses argument tiebreakers
                 in some strange cases. */
              suppress_tiebreakers = TRUE;
            }  /* if */
            cmp = compare_arg_match_levels(curr_arg,
                                           func_in_set->current_arg_match,
                                           suppress_tiebreakers);
            if (cmp < 0) {
              /* The current argument match is not as good as this match in
                 the set, so it will not be added to the set.  We do have
                 to keep going to compare the current argument match against
                 the other matches in the set. */
              new_match_worse_than_some_match_in_set = TRUE;
              prev_func_in_set = func_in_set;
            } else if (cmp > 0) {
              /* The current argument match is better than this match in
                 the set, so remove the match from the set. */
              func_in_set->in_best_match_set_for_curr_argument = FALSE;
              if (prev_func_in_set == NULL) {
                best_match_set_for_curr_arg =
                                       func_in_set->next_in_arg_best_match_set;
              } else {
                prev_func_in_set->next_in_arg_best_match_set =
                                       func_in_set->next_in_arg_best_match_set;
              }  /* if */
            } else {
              /* The current argument match is no better/no worse than this
                 match in the set, so keep the match in the set. */
              prev_func_in_set = func_in_set;
            }  /* if */
          }  /* for */
        }  /* if */
        /* Add the new match to the set if it is no worse than everything
           in the set. */
        if (!new_match_worse_than_some_match_in_set) {
          cfp->next_in_arg_best_match_set = best_match_set_for_curr_arg;
          best_match_set_for_curr_arg = cfp;
          cfp->in_best_match_set_for_curr_argument = TRUE;
        } else {
          cfp->in_best_match_set_for_curr_argument = FALSE;
        }  /* if */
      }  /* for */
      /* Loop through the functions and form the intersection of the
         best-match set for this argument and the overall best-match
         set to date. */
      for (cfp = candidates; cfp != NULL; cfp = cfp->next) {
        /* Also keep functions with error matches in the best-match set. */
        curr_arg = cfp->current_arg_match;
        if (cfp->in_best_match_set_for_curr_argument) {
          /* This function is in the best-match set for the current
             argument. */
          cfp->in_best_match_set_for_some_argument = TRUE;
        } else {
          /* This function is not in the best-match set for the current
             argument. */
          if (cfp->in_best_match_set) {
            /* Remove the function from the overall best-match set. */
            cfp->in_best_match_set = FALSE;
            number_in_best_match_set--;
            /* Note that we continue here even if number_in_best_match_set is
               zero so that in_best_match_set_for_some_argument will be
               set so that we will get the best error message. */
          }  /* if */
        }  /* if */
        /* Advance the current argument for this function to the next one,
           in preparation for the next iteration of the outer loop. */
        advance_arg_match(cfp);
      }  /* for */
      /* Loop to consider the next argument. */
    }  /* while */
    /* Here, the intersection of the best-match sets has been made, and
       the candidates with in_best_match_set TRUE are in that set.
       number_in_best_match_set indicates the number of members of that
       set. */
    if (number_in_best_match_set > 1) {
      /* There are two or more functions that are in the best-match set
         for all arguments.  See if any of those are better than the others
         for some other reason (for example: one is a function template
         and the other is not). */
      best_cfp = NULL;
      /*lint --e{850} cfp modified in loop */
      for (cfp = candidates; cfp != NULL; cfp = cfp->next) {
        /* Look at each function in the best-match set, and see if any
           of those are better than the others. */
        if (cfp->in_best_match_set) {
          if (best_cfp == NULL) {
            /* First function.  Take it as the best so far by definition. */
            best_cfp = cfp;
          } else if (same_candidate_function(cfp, best_cfp)) {
            /* The same function appears twice in the overload set,
               probably because the set is synthesized for a member lookup
               or because of a using directive.  Ignore the second function. */
            cfp->in_best_match_set = FALSE;
            number_in_best_match_set--;
            if (number_in_best_match_set == 1) goto end_func_winnow;
          } else {
            /* Compare the current function against the best so far. */
            cmp = compare_candidate_functions(cfp, best_cfp);
            if (cmp < 0) {
              /* The new function is not as good as the best so far.
                 Take it out of the best-match set. */
              cfp->in_best_match_set = FALSE;
              number_in_best_match_set--;
              if (number_in_best_match_set == 1) goto end_func_winnow;
            } else if (cmp > 0) {
              /* The new function is better than the best so far.  Take
                 all previous functions out of the best-match set. */
              best_cfp = cfp;
              /*lint --e{445} reuse of for loop variable cfp*/
              for (cfp = candidates; cfp != best_cfp; cfp = cfp->next) {
                if (cfp->in_best_match_set) {
                  cfp->in_best_match_set = FALSE;
                  number_in_best_match_set--;
                  if (number_in_best_match_set == 1) goto end_func_winnow;
                }  /* if */
              }  /* for */
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* for */
      /* Here, we have eliminated any functions that are not as good as the
         best function. */
end_func_winnow:;
    }  /* if */
    /* If there is only one function in the overall best-match set, perform
       the additional check that it must be strictly better than every other
       function on at least one argument. */
    if (number_in_best_match_set == 1) {
      /* Find the chosen function. */
      for (best_cfp = candidates;
           !best_cfp->in_best_match_set;
           best_cfp = best_cfp->next) {}
      if (!match_is_better_on_at_least_one_arg(best_cfp, candidates)) {
        /* The chosen function is not better than some other function.
           That means the call is ambiguous. */
        overall_ambiguity = TRUE;
        goto create_final_list;
      }  /* if */
    } else if (any_error_match && number_in_best_match_set > 1) {
      /* There are some error matches, and we have more than one "best"
         function, so the problem is undecidable. */
      *undecidable_because_of_error = TRUE;
#if GNU_EXTENSIONS_ALLOWED
    } else if (gpp_mode && gnu_version < 40000 &&
               number_in_best_match_set == 0) {
      /* g++ has an "extension" that chooses one function match over
         another if the worst conversion for its arguments is not as bad
         as the worst conversion for another function's arguments.
         This is tested after we've determined that we would get an
         error by the standard rules, so no standard-conforming
         program is affected.  This extension is still present in g++ 3.4
         but it's gone in g++ 4.0 (except with -fpermissive). */
      best_cfp = select_best_gpp_candidate(candidates);
      if (best_cfp != NULL) {
        /* There's a single best function under the g++ extension.
           Take the others out of the best-match set. */
        for (cfp = candidates; cfp != NULL; cfp = cfp->next) {
          cfp->in_best_match_set = (cfp == best_cfp);
        }  /* for */
        number_in_best_match_set = 1;
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    }  /* if */
create_final_list:
    /* Make the final list.  If overall_ambiguity is TRUE, use the
       functions that were likely contenders, i.e., those that have
       in_best_match_set_for_some_argument TRUE. */
    /* If there are no functions left in the best-match set, there is
       overall ambiguity. */
    if (number_in_best_match_set == 0) overall_ambiguity = TRUE;
    /* Here, the best candidate functions have in_best_match_set TRUE.
       Make *candidate_functions a list of just those.  (Or, when
       overall_ambiguity is TRUE, make a list of the candidate functions
       with in_best_match_set_for_some_argument TRUE.)  Discard the
       other functions.  If *undecidable_because_of_error is TRUE,
       throw away all entries regardless of the in_best_match_set flag. */
    *candidate_functions = end_candidate_functions = NULL;
    for (cfp = candidates; cfp != NULL; cfp = cfp_next) {
      cfp_next = cfp->next;
      cfp->next = NULL;
      if (!*undecidable_because_of_error &&
          (overall_ambiguity ? cfp->in_best_match_set_for_some_argument :
                               cfp->in_best_match_set)) {
        /* Keep an entry that made the final list. */
        if (end_candidate_functions == NULL) {
          *candidate_functions = cfp;
        } else {
          end_candidate_functions->next = cfp;
        }  /* if */
        end_candidate_functions = cfp;
      } else {
        /* Free an entry that didn't make the final list. */
        /* Note that this call just frees one entry because we've already
           cleared the next pointer. */
        free_candidate_function_list(cfp);
      }  /* if */
    }  /* for */
    candidates = *candidate_functions;
  }  /* if */
  if (*undecidable_because_of_error) {
    *ambiguous = TRUE;
  } else if (candidates != NULL) {
    if (candidates->next != NULL ||
        (candidates->function_symbol != NULL &&
         is_ambiguous_by_inheritance(candidates->function_symbol))) {
      /* There is more than one function in the "best" set, or only one
         function but its name is ambiguous, so the overload resolution
         is ambiguous. */
      *ambiguous = TRUE;
    } else if (candidates->is_function_template) {
      /* A single candidate function template was unambiguously selected.
         Create the template function instance. */
      a_symbol_ptr sym = candidates->function_symbol;
      reduce_projection_symbol_to_fundamental_symbol(sym);
      if (nonstandard_default_arg_deduction) {
        /* If the template arguments include function types with uninstantiated
           default arguments, instantiate them now because the template and
           its function type are getting separated. */
        instantiate_template_default_arguments(candidates);
      }  /* if */
      candidates->function_symbol = sym =
                         find_template_function(sym,
                                                &candidates->template_arg_list,
                            (a_boolean)candidates->expl_template_arg_list_used,
                                                source_pos);
      candidates->is_function_template = FALSE;
      if (candidates->is_user_conversion) {
        candidates->conversion.routine = sym->variant.routine.ptr;
        candidates->conversion.routine_symbol = sym;
      } /* if */
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 4 || db_flag_is_set("overload")) {
    db_display_overload_level();
    fprintf(f_debug, "Return from select_best_candidate_functions: ");
    db_candidate_function_list(candidates);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* select_best_candidate_functions */


static void add_operand_to_arg_dependent_lookup_list(
                                              an_operand            *operand,
                                              a_type_list_entry_ptr *type_list)
/*
operand is an argument of a call.  Add its type to the type list pointed
to by type_list, which is being accumulated to do argument-dependent
lookup.
*/
{
  if (is_indefinite_function_operand(operand)) {
    /* The operand is an indefinite function.  Loop through the symbols
       and add each function type to the list. */
    a_symbol_ptr ovl_sym = operand->symbol, sym;
    reduce_projection_symbol_to_fundamental_symbol(ovl_sym);
    /* Ignore templates. */
    if (ovl_sym->kind != (a_symbol_kind)sk_function_template) {
      check_assertion(ovl_sym->kind == (a_symbol_kind)sk_overloaded_function);
      sym = ovl_sym->variant.overloaded_function.symbols;
      for (; sym != NULL; sym = sym->next) {
        a_type_ptr   func_type;
        a_symbol_ptr fund_sym = fundamental_symbol_of(sym);
        /* Ignore templates. */
        if (fund_sym->kind != (a_symbol_kind)sk_function_template) {
          check_assertion(fund_sym->kind == (a_symbol_kind)sk_routine ||
                          fund_sym->kind == (a_symbol_kind)sk_member_function);
          func_type = routine_symbol_type(fund_sym);
          add_to_arg_dependent_lookup_list(func_type, type_list);
        }  /* if */
      }  /* for */
    }  /* if */
  } else {
    /* Normal case. */
    add_to_arg_dependent_lookup_list(operand->type, type_list);
  }  /* if */
}  /* add_operand_to_arg_dependent_lookup_list */


static a_boolean any_function_has_dependent_param_or_default_arg(
                                                              a_symbol_ptr sym)
/*
sym is a function or set of overloaded functions.  Return TRUE if
any member of the set has a dependent parameter or dependent default
argument expression.
*/
{
  a_boolean any_dep = FALSE;

  /* Only block extern functions and members of prototype instantiations
     can have dependent parameters. */
  if (is_block_extern_symbol(sym) ||
      (sym->is_class_member &&
       sym_parent_class(sym)->variant.class_struct_union.
                                                 is_prototype_instantiation)) {
    a_boolean is_overloaded_function;
    sym = fundamental_symbol_of(sym);
    if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
      is_overloaded_function = TRUE;
      sym = sym->variant.overloaded_function.symbols;
    } else {
      is_overloaded_function = FALSE;
    }  /* if */
    /* Loop through the symbols in the overload set. */
    for (; sym != NULL; sym = (is_overloaded_function ? sym->next : NULL)) {
      a_symbol_ptr                  fund_sym = fundamental_symbol_of(sym);
      a_type_ptr                    rout_type;
      a_routine_type_supplement_ptr rtsp;
      a_param_type_ptr              ptp;
      if (fund_sym->kind == (a_symbol_kind)sk_function_template ||
          fund_sym->kind == (a_symbol_kind)sk_constant) {
        /* Function templates are always going to have template parameters
           in their parameter lists.  Also, using-declarations for names
           in dependent base classes show up as sk_constant symbols, and
           we can't tell much about them, so we assume they will be
           dependent. */
        any_dep = TRUE;
        goto end_of_function;
      }  /* if */
      check_assertion(fund_sym->kind == (a_symbol_kind)sk_routine ||
                      fund_sym->kind == (a_symbol_kind)sk_member_function);
      rout_type = routine_symbol_type(fund_sym);
      check_assertion(rout_type->kind == (a_type_kind)tk_routine);
      rtsp = rout_type->variant.routine.extra_info;
      /* Loop through the parameter list looking for dependent types. */
      for (ptp = rtsp->param_type_list; ptp != NULL; ptp = ptp->next) {
        if (ptp->type_involves_template_param) {
          any_dep = TRUE;
          goto end_of_function;
        }  /* if */
        if (ptp->has_default_arg) {
          /* We don't have a good way of examining the default argument
             expression to see whether it is dependent.  Consider
               void f(int i = 1 + sizeof(T));
             which is going to look a lot like a normal addition expression.
             So just assume that any default argument is dependent. */
          any_dep = TRUE;
          goto end_of_function;
        }  /* if */
      }  /* for */
    }  /* for */
  }  /* if */
end_of_function:
  return any_dep;
}  /* any_function_has_dependent_param_or_default_arg */


a_boolean is_template_dependent_indefinite_function(an_operand *operand)
/*
Return TRUE if the indicated operand is a template-dependent indefinite
function.
*/
{
  a_boolean dep = FALSE;

  if (is_indefinite_function_operand(operand) &&
      ((operand->symbol->is_class_member &&
        sym_parent_class(operand->symbol)->variant.class_struct_union.
                                                           is_nonreal_class) ||
       (operand->is_template_id &&
        template_arg_list_is_dependent(operand->template_arg_list)))) {
    /* An indefinite function in a nonreal class, or one with a
       template-dependent explicit template argument list, is
       template-dependent. */
    dep = TRUE;
  }  /* if */
  return dep;
}  /* is_template_dependent_indefinite_function */


a_boolean operand_is_dependent(an_operand *operand)
/*
Return TRUE if the indicated operand is dependent.  Specifically, this
means type-dependent and not value-dependent.
*/
{
  a_boolean is_dependent = (is_template_dependent_type(operand->type) ||
                           is_template_dependent_indefinite_function(operand));
  return is_dependent;
}  /* operand_is_dependent */


static a_boolean is_symbol_for_which_overload_resolution_should_be_deferred(
                                                              a_symbol_ptr sym)
/*
Return TRUE if the indicated symbol (potentially an overload set) is one for
which overload resolution cannot be done at present.  We are in a template
dependent context.
*/
{
  a_boolean defer = FALSE;

  check_assertion(is_template_dependent_context());
  if (any_function_has_dependent_param_or_default_arg(sym)) {
    /* If any function in the set has a dependent parameter type we cannot
       do overload resolution.  If any function has a dependent default
       argument expression, we might be able to determine the function
       to call but we couldn't assemble the actual call arguments, so
       delay all processing until a real instantiation. */
    defer = TRUE;
  } else if (sym->potentially_overloaded) {
    /* The function coexists with a using-declaration that might or
       might not cause it to be overloaded. */
    defer = TRUE;
  }  /* if */
  return defer;
}  /* is_symbol_for_which_overload_resolution_should_be_deferred */


static a_boolean is_symbol_for_which_arg_dependent_lookup_should_be_suppressed(
                                                              a_symbol_ptr sym)
/*
Return TRUE if the indicated symbol is one for which argument-dependent
lookup should be suppressed.
*/
{
  a_boolean suppress = FALSE;

  if (sym->is_class_member) {
    /* Argument-dependent lookup is suppressed for a member function. */
    suppress = TRUE;
  } else if (is_block_extern_symbol(sym)) {
    /* Argument-dependent lookup is suppressed for a block extern.
       This is core issue 239.  Note that local using-declarations
       are excluded. */
    if (gpp_mode || sun_mode ||
        (microsoft_mode && microsoft_version == 1310)) {
      /* Sun, g++, and Microsoft 7.1 do not suppress the argument-dependent
         lookup for a block extern.  Microsoft 6.0 and 7.0 appear to suppress
         this, but that's actually because they do not do argument-dependent
         lookup at all. */
    } else {
      suppress = TRUE;
    }  /* if */
  } else if (gpp_mode && gnu_version < 30400 && is_local_symbol(sym)) {
    /* Versions of g++ prior to 3.4 suppress the argument-dependent lookup
       for a using-declaration. */
    suppress = TRUE;
  }  /* if */
  return suppress;
}  /* is_symbol_for_which_arg_dependent_lookup_should_be_suppressed */


static a_boolean is_gpp_falsely_dependent_argument(an_operand *operand)
/*
Return TRUE if the given operand is to be treated as dependent when it's
an argument of a call in gpp mode even though the standard says it's not.
*/
{
  a_boolean result = FALSE;

  /* The cases we care about are "this->x" and "*(this->x)".  g++ sees
     those as dependent even if the type of x is known.  Also a call
     of a member function of the current class even if the return type
     is known. */
  if (is_expression_operand(operand)) {
    an_expr_node_ptr expr = skip_parens(operand->variant.expression);
    if (is_operation_node(expr) &&
        (node_operator_is(expr, eok_indirect) ||
         node_operator_is(expr, eok_ref_indirect))) {
      /* Drop "*" or the reference equivalent. */
      expr = skip_parens(expr->variant.operation.operands);
    }  /* if */
    if (is_operation_node(expr) &&
        !expr->variant.operation.compiler_generated) {
      an_expr_node_ptr potential_this = NULL;
      an_expr_node_ptr op1 = expr->variant.operation.operands;
      an_expr_node_ptr op2 = op1->next;
      op1 = skip_parens(op1);
      if (node_operator_is(expr, eok_points_to_field)) {
        potential_this = op1;
      } else if (node_operator_is(expr, eok_points_to_member_call)) {
        potential_this = skip_parens(op2);
      } else if (node_operator_is(expr, eok_call)) {
        /* Look for a call of a static member function of the current class.
           Note that conv_expr_function_designator_to_ptr_to_function forces
           such functions to be (value-)dependent. */
        if (is_constant_node(op1) &&
            op1->variant.constant->kind ==
                                     (a_constant_repr_kind)ck_template_param) {
          result = TRUE;
        }  /* if */
      }  /* if */
      if (potential_this != NULL &&
          is_variable_node(potential_this) &&
          potential_this->variant.variable->is_this_parameter) {
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* is_gpp_falsely_dependent_argument */


#if !BACK_END_IS_CP_GEN_BE
/*ARGSUSED*/  /* found_through_adl is only used with the C++-generating
                 back end. */
#endif /* !BACK_END_IS_CP_GEN_BE */
a_symbol_ptr select_overloaded_function(
                         a_symbol_ptr             overloaded_function_symbol,
                         a_boolean                is_template_id,
                         a_template_arg_ptr       template_arg_list,
                         a_boolean                have_selector,
                         an_operand               *bound_function_selector,
                         an_arg_operand_ptr       arg_operand_list,
                         a_boolean                do_arg_dep_lookup,
                         a_boolean                force_dependent,
                         an_error_code            err_none_applies,
                         an_error_code            err_ambiguous,
                         a_source_position        *call_position,
                         a_token_sequence_number  paren_tok_seq_number,
                         a_boolean                *single_function,
                         a_boolean                *unknown_dependent_function,
                         a_boolean                *found_through_adl,
                         a_symbol_ptr             *surrogate_function_conv_sym,
                         an_arg_match_summary_ptr *arg_match_list)
/*
Determine which of the functions under overloaded_function_symbol
should be called given an argument list arg_operand_list.  The symbol
may be an overloaded function, a simple member or nonmember function,
or a projection symbol for one of those.  is_template_id is TRUE if
the symbol has an associated explicit template argument list; if so,
template_arg_list gives the list of arguments.  If have_selector is
TRUE, *bound_function_selector is a selector object.  Note that, for
constructor calls, bound_function_selector can be NULL when
have_selector is TRUE; we have a selector, but it's not available.
That's okay for constructors, because they cannot be const- or
volatile-qualified, and the selector expression is only needed for
that discrimination.
bound_function_selector->selector_is_object_pointer is TRUE if the
selector is an object pointer, FALSE if it is an object.
do_arg_dep_lookup is TRUE if argument-dependent lookup should be done;
if it is TRUE, overloaded_function_symbol may be an sk_undefined
symbol, indicating that nothing was found on a normal id lookup of the
function name.  force_dependent is TRUE if the call should be treated
as dependent even when argument-dependent lookup is not done (that
would usually force the call to be treated as nondependent).
call_position is the source position of the call.
paren_tok_seq_number is the token sequence number of the opening
parenthesis of the argument list, but it's required only when
do_arg_dep_lookup is TRUE; it can be zero otherwise.  If an error of
some sort is detected, issue an error at that position and return
NULL.  err_none_applies is the error code to use when no function
applies, and err_ambiguous is the error code to use when more than one
function applies.  If there is no error, an argument match list is
returned in *arg_match_list (the caller must free this) and the symbol
selected is returned.  If single_function is non-NULL and the set of
functions to be considered (the symbol passed in, if not undefined,
plus any symbols added by argument-dependent lookup) contains exactly
one function, set *single_function to TRUE and return the function,
without checking whether the function matches the argument list
provided (this allows the caller to revert to the simpler processing
used for non-overloaded functions, which can produce clearer error
messages).  If the call is dependent, and the function to be called
cannot be determined, return *unknown_dependent_function set to TRUE
(unknown_dependent_function can be NULL if the call cannot be
dependent).  If found_through_adl is non-NULL and the callee was found
only through ADL, *found_through_adl is returned TRUE.  If
surrogate_function_conv_sym is non-NULL, look for surrogate functions
also.  overloaded_function_symbol may be NULL in that case.  If a
surrogate function is the best match, return in
*surrogate_function_conv_sym a pointer to the symbol for the
conversion function that yields the pointer to the surrogate function,
and return NULL.  This routine is called only in C++ mode.
*/
{
  a_candidate_function_ptr candidate_functions;
  a_symbol_ptr             function_symbol;
  a_boolean                matched_except_for_missing_selector = FALSE;
  a_boolean                matched_except_for_selector = FALSE;
  a_boolean                undecidable_because_of_error, ambiguous;
  a_boolean                sym_is_undefined = FALSE;
  a_boolean                some_function_tried = FALSE;
  a_boolean                dependent_call = FALSE;
  a_boolean                known_to_be_visible = FALSE;
  an_arg_operand_ptr       arg_operand;

  db_enter(4, "select_overloaded_function");
#if DEBUG
  overload_level++;
  if (debug_level >= 4 || db_flag_is_set("overload")) {
    db_display_overload_level();
    db_symbol(overloaded_function_symbol,
              "Entering select_overloaded_function with ", 4);
  }  /* if */
#endif /* DEBUG */
  if (!have_selector) bound_function_selector = NULL;
  /* candidate_functions will contain the list of viable functions. */
  candidate_functions = NULL;
  if (found_through_adl != NULL) *found_through_adl = FALSE;
  if (single_function != NULL) *single_function = FALSE;
  /* The "single function" processing is not compatible with trying
     surrogate functions. */
  if (surrogate_function_conv_sym != NULL) single_function = NULL;
  if (overloaded_function_symbol != NULL) {
    sym_is_undefined = (overloaded_function_symbol->kind ==
                                                  (a_symbol_kind)sk_undefined);
    if (do_arg_dep_lookup && !sym_is_undefined) {
      /* For certain symbols, e.g., member functions, suppress argument-
         dependent lookup. */
      if (is_symbol_for_which_arg_dependent_lookup_should_be_suppressed(
                                                 overloaded_function_symbol)) {
        do_arg_dep_lookup = FALSE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (is_template_dependent_context()) {
    a_boolean defer_overload_resolution = FALSE;
    /* In a prototype instantiation.  See whether the call is dependent
       (i.e., has arguments of dependent types). */
    for (arg_operand = arg_operand_list;
         arg_operand != NULL;
         arg_operand = arg_operand->next) {
      an_operand *arg = &arg_operand->operand;
      if (operand_is_dependent(arg)) {
        dependent_call = TRUE;
        break;
      } else if (gpp_mode && is_constant_operand(arg) &&
                 is_possible_dependent_null_pointer_constant(
                                                     &arg->variant.constant)) {
        /* g++ seems to make a call dependent if one of the arguments might
           be a null pointer constant given the right choice of template
           arguments.  In other modes, we disallow use of such constants
           as null pointer constants in argument matching. */
        dependent_call = TRUE;
        break;
      } else if (gpp_mode &&
                 is_gpp_falsely_dependent_argument(arg)) {
        /* g++ incorrectly treats something like "this->x" in a member
           function of a template as dependent even if the type of x is
           not dependent. */
        dependent_call = TRUE;
        break;
      }  /* if */
    }  /* for */
    if (!dependent_call && is_template_id &&
        template_arg_list_is_dependent(template_arg_list)) {
      /* A call like f<T>(1), where the explicit template argument
         list includes dependent arguments. */
      dependent_call = TRUE;
    }  /* if */
    if (dependent_call) {
      /* No need to take time to check for deferring overload resolution
         if we've already decided the call is dependent. */
    } else if (overloaded_function_symbol != NULL &&
               is_symbol_for_which_overload_resolution_should_be_deferred(
                                                 overloaded_function_symbol)) {
      /* A function for which we can't do overload resolution at this
         time. */
      defer_overload_resolution = TRUE;
    }  /* if */
    if (dependent_call || defer_overload_resolution) {
      /* We can't do overload resolution (e.g., because some of the
         arguments have template-dependent types).  Return a flag
         indicating that. */
      check_assertion(unknown_dependent_function != NULL);
      *unknown_dependent_function = TRUE;
      function_symbol = NULL;
      *arg_match_list = NULL;
      goto have_function;
    }  /* if */
    if (scope_stack[depth_scope_stack].in_nonreal_instantiation &&
        do_dependent_name_processing) {
      /* The current context is a nonreal instantiation (probably of
         a default argument expression).  We needed to do the above
         code so we wouldn't get confused on a call that is
         dependent on template parameters, but now we should go to
         the normal processing because this is, after all, an
         instantiation. */
      goto in_instantiation;
    }  /* if */
  } else if (do_dependent_name_processing &&
             is_nonspecialized_instantiation_context()) {
    /* In a real (not prototype) instantiation, and doing dependent
       name processing.  Look up this call to see whether it was a
       dependent call in the prototype instantiation.  If it was a
       nondependent call, it was recorded, along with (usually) the
       symbol chosen by overload resolution.  In Microsoft and Sun mode, an
       instantiation scope may be pushed for a nonstandard specialization
       scope.  Don't treat such specializations as instantiations. */
in_instantiation:
    if (!do_arg_dep_lookup) {
      /* Calls where argument-dependent lookup is turned off are
         not recorded, but they're always considered non-dependent. */
      dependent_call = FALSE;
      if (force_dependent) {
        /* The caller asked that the call be treated as dependent anyway.
           This is used to emulate a g++ bug that treats certain nondependent
           operator "new" calls as dependent. */
        dependent_call = TRUE;
      }  /* if */
    } else {
      a_nondependent_call_info_ptr ndcall_info;
      ndcall_info = get_nondependent_call_info(paren_tok_seq_number,
                                               (a_nondependent_call_depth)0);
      dependent_call = (ndcall_info == NULL);
      if (!dependent_call && ndcall_info->symbol != NULL) {
        /* We know the function selected for this nondependent call during
           the prototype instantiation.  Use that without going through
           overload resolution. */
        overloaded_function_symbol = ndcall_info->symbol;
        do_arg_dep_lookup = FALSE;
        known_to_be_visible = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (overloaded_function_symbol != NULL) {
    if (!do_arg_dep_lookup) {
      /* No argument-dependent lookup.  Use only the function symbol
         provided. */
      /* coverity[dead_error_line] */  /* Coverity bug: tool thinks
                                          single_function is NULL here. */
      if (single_function != NULL && !have_selector) {
        /* If the function is a single non-overloaded function, overload
           resolution is not required. */
        function_symbol = fundamental_symbol_of(overloaded_function_symbol);
        if ((function_symbol->kind == (a_symbol_kind)sk_routine ||
             function_symbol->kind == (a_symbol_kind)sk_member_function) &&
            (known_to_be_visible ||
             candidate_function_is_visible(
                                       overloaded_function_symbol,
                                       is_template_id,
                                       /*effects_copy_initialization=*/FALSE,
                                       /*arg_dep_lookup_done=*/FALSE,
                                       /*from_arg_dep_lookup=*/FALSE,
                                       dependent_call,
                                       /*is_overloaded_operator=*/FALSE,
                                       /*allow_post_declared_functions=*/FALSE,
                                       (a_boolean *)NULL,
                                       (a_boolean *)NULL))) {
          *single_function = TRUE;
          function_symbol = overloaded_function_symbol;
          goto have_function;
        }  /* if */
      }  /* if */
      /* Evaluate all matches in the function set. */
      try_overloaded_function_match(overloaded_function_symbol,
                                    is_template_id,
                                    template_arg_list,
                                    arg_operand_list,
                                    have_selector,
                                    bound_function_selector,
                                    /*ctor_conversion_case=*/FALSE,
                                    /*initializing_return_value=*/FALSE,
                                    /*effects_copy_initialization=*/FALSE,
                                    /*allow_udc_on_arguments=*/TRUE,
                                    /*arg_dep_lookup_done=*/FALSE,
                                    /*from_arg_dep_lookup=*/FALSE,
                                    dependent_call,
                                    force_dependent,
                                    known_to_be_visible,
                                    /*is_overloaded_operator=*/FALSE,
                                    &candidate_functions,
                                    &matched_except_for_missing_selector,
                                    &matched_except_for_selector);
      some_function_tried = TRUE;
    } else {
      /* Do argument-dependent lookup, which may add additional functions
         from the classes and namespaces associated with the argument
         types. */
      a_type_list_entry_ptr   type_list = NULL;
      a_symbol_locator        locator;
      a_symbol_list_entry_ptr symbol_list, slep;
      a_symbol_ptr            normal_lookup_function_symbol;

      /* Accumulate the types used in the arguments. */
      for (arg_operand = arg_operand_list;
           arg_operand != NULL;
           arg_operand = arg_operand->next) {
        add_operand_to_arg_dependent_lookup_list(&arg_operand->operand,
                                                 &type_list);
      }  /* for */
      /* Do argument-dependent lookup, producing a list of symbols to
         be considered as candidate functions. */
      make_locator_for_symbol(overloaded_function_symbol, &locator);
      normal_lookup_function_symbol = sym_is_undefined ?
                                                    NULL :
                                                    overloaded_function_symbol;
      symbol_list = argument_dependent_lookup(normal_lookup_function_symbol,
                                              &locator,
                                              &type_list);
      if (single_function != NULL && symbol_list != NULL) {
        /* If the function is a single non-overloaded function, overload
           resolution is not required. */
        function_symbol = fundamental_symbol_of(symbol_list->symbol);
        if ((function_symbol->kind == (a_symbol_kind)sk_routine ||
             function_symbol->kind == (a_symbol_kind)sk_member_function) &&
            candidate_function_is_visible(symbol_list->symbol,
                                          is_template_id,
                                         /*effects_copy_initialization=*/FALSE,
                                          do_arg_dep_lookup,
                                          /*from_arg_dep_lookup=*/
                                               (symbol_list->symbol !=
                                                normal_lookup_function_symbol),
                                          dependent_call,
                                          /*is_overloaded_operator=*/FALSE,
                                          /*allow_post_declared_functions=*/
                                                                         FALSE,
                                          (a_boolean *)NULL,
                                          (a_boolean *)NULL)) {
          /* This must be either the only entry on the list, or all other
             entries on the list must be the same symbol. */
          for (slep = symbol_list->next; slep != NULL; slep = slep->next) {
            if (slep->symbol != symbol_list->symbol) break;
          }  /* for */
          if (slep == NULL) {
            free_list_of_symbol_list_entries(symbol_list);
            *single_function = TRUE;
            function_symbol = symbol_list->symbol;
#if BACK_END_IS_CP_GEN_BE
            if (found_through_adl != NULL) {
              *found_through_adl = (symbol_list->symbol !=
                                                normal_lookup_function_symbol);
            }  /* if */
#endif /* BACK_END_IS_CP_GEN_BE */
            goto have_function;
          }  /* if */
        }  /* if */
      }  /* if */
      /* Try each function symbol on the symbol list. */
      for (slep = symbol_list; slep != NULL; slep = slep->next) {
        function_symbol = slep->symbol;
        try_overloaded_function_match(function_symbol,
                                      is_template_id,
                                      template_arg_list,
                                      arg_operand_list,
                                      have_selector,
                                      bound_function_selector,
                                      /*ctor_conversion_case=*/FALSE,
                                      /*initializing_return_value=*/FALSE,
                                      /*effects_copy_initialization=*/FALSE,
                                      /*allow_udc_on_arguments=*/TRUE,
                                      do_arg_dep_lookup,
                                      /*from_arg_dep_lookup=*/
                                               (slep != symbol_list ||
                                                function_symbol !=
                                                normal_lookup_function_symbol),
                                      dependent_call,
                                      force_dependent,
                                      /*known_to_be_visible=*/FALSE,
                                      /*is_overloaded_operator=*/FALSE,
                                      &candidate_functions,
                                      &matched_except_for_missing_selector,
                                      &matched_except_for_selector);
        some_function_tried = TRUE;
      }  /* for */
      free_list_of_symbol_list_entries(symbol_list);
    }  /* if */
  }  /* if */
  if (surrogate_function_conv_sym != NULL) {
    /* Try surrogate functions also.  The "called function" is a class
       object, and we look for conversion functions that convert that
       class to pointers to functions.  The functions found in that
       way are surrogate functions. */
    check_assertion(have_selector);
    try_surrogate_function_match(bound_function_selector,
                                 arg_operand_list,
                                 &candidate_functions);
  }  /* if */
  /* The candidate_functions list now contains all the viable functions.
     Find the best one(s). */
  select_best_candidate_functions(&candidate_functions, call_position,
                                  &undecidable_because_of_error, &ambiguous);
  function_symbol = NULL;
  *arg_match_list = NULL;
  if (undecidable_because_of_error) {
    /* There was some previous error, so do not put out an error message. */
  } else if (candidate_functions == NULL) {
    /* None of the functions applies. */
    if (matched_except_for_missing_selector) {
      /* At least one of the functions would have matched if we had had a
         selector expression, so issue a different error message. */
      /* A nonstatic member function is used someplace where there is no
         "this" available, e.g., outside of a member function. */
      expr_pos_error(ec_member_ref_requires_object, call_position);
    } else if (sym_is_undefined && !some_function_tried) {
      /* The function symbol is not defined, and no functions were added
         by argument-dependent lookup, so the best diagnostic is one
         that says the name is undefined. */
      /* On a rescan, do not enter the name into the symbol table. */
      if (!expr_stack->template_deduction_context) {
        enter_undefined_symbol(overloaded_function_symbol);
      }  /* if */
      if (expr_error_should_be_issued()) {
        pos_st_error(ec_undefined_identifier, call_position,
                     overloaded_function_symbol->header->identifier);
      }  /* if */
    } else if (overloaded_function_symbol == NULL) {
      /* Call of class object, no operator() or appropriate conversion
         functions to pointer to function type. */
      check_assertion(surrogate_function_conv_sym != NULL);
      expr_pos_error(ec_bad_call_of_class_object, call_position);
    } else {
      /* Normal case. */
      a_type_ptr object_type = NULL;
      if (bound_function_selector != NULL) {
        object_type = bound_function_selector->type;
      }  /* if */
      if (matched_except_for_selector) {
        /* At least one of the functions would have matched except for the
           selector expression, so issue a different error message.
           This is likely due to a cv-qualifier difference. */
        err_none_applies = ec_no_matching_function_due_to_selector;
        if (object_type == NULL) object_type = make_implicit_selector_type();
      } else if (overloaded_function_symbol->kind ==
                                           (a_symbol_kind)sk_routine ||
                 overloaded_function_symbol->kind ==
                                           (a_symbol_kind)sk_member_function) {
        /* Use a simpler message for a non-overloaded function.  This
           is particularly useful when the called function is the operator()
           of a lambda. */
        if (err_none_applies == ec_no_matching_function ||
            err_none_applies == ec_no_matching_new_function) {
          err_none_applies = ec_function_does_not_match_arguments;
        }  /* if */
      }  /* if */
      if (expr_error_should_be_issued()) {
        pos_sy_start_error(err_none_applies, call_position,
                           overloaded_function_symbol);
        display_argument_list_types(object_type, arg_operand_list);
        end_error();
      }  /* if */
    }  /* if */
  } else if (ambiguous) {
    /* More than one function applies and is a best match -- ambiguity. */
#if DEBUG
    if (debug_level >= 4) {
      db_candidate_function_list(candidate_functions);
    }  /* if */
#endif /* DEBUG */
    if (candidate_functions->next == NULL &&
        candidate_functions->function_symbol != NULL &&
        is_ambiguous_by_inheritance(candidate_functions->function_symbol)) {
      /* For a case involving a single function name that's ambiguous
         by inheritance, use a simpler message. */
      if (expr_error_should_be_issued()) {
        pos_sy_error(ec_ambiguous_name, call_position,
                     overloaded_function_symbol);
      }  /* if */
    } else {
      /* Use a special diagnostic for a call that includes surrogate
         functions. */
      a_boolean                use_class_call_message = FALSE;
      a_candidate_function_ptr cfp;
      for (cfp = candidate_functions; cfp != NULL; cfp = cfp->next) {
        if (cfp->surrogate_function_conv_sym != NULL) {
          use_class_call_message = TRUE;
          break;
        }  /* if */
      }  /* for */
      if (use_class_call_message) {
        /* Candidate set includes at least one surrogate function. */
        a_type_ptr object_class_type;
        check_assertion(bound_function_selector != NULL);
        object_class_type = bound_function_selector->type;
        if (bound_function_selector->selector_is_object_pointer) {
          object_class_type = type_pointed_to(object_class_type);
        }  /* if */
        if (expr_error_should_be_issued()) {
          pos_ty_start_error(ec_ambiguous_class_call, call_position,
                             object_class_type);
        }  /* if */
      } else {
        /* Normal case (not a class call). */
        check_assertion(overloaded_function_symbol != NULL);
        if (expr_error_should_be_issued()) {
          pos_sy_start_error(err_ambiguous, call_position,
                             overloaded_function_symbol);
        }  /* if */
      }  /* if */
      if (expr_error_should_be_issued()) {
        diagnose_overload_ambiguity(candidate_functions,
                                    bound_function_selector,
                                    arg_operand_list,
                                    (an_opname_kind)onk_none);
      }  /* if */
    }  /* if */
  } else {
    /* Exactly one function applies and is best. */
    function_symbol = candidate_functions->function_symbol;
#if BACK_END_IS_CP_GEN_BE
    if (found_through_adl != NULL) {
      *found_through_adl =  candidate_functions->found_through_adl;
    }  /* if */
#endif /* BACK_END_IS_CP_GEN_BE */
    *arg_match_list = candidate_functions->arg_matches;
    /* Prevent freeing of the arg_match_list when the candidate_functions
       list is freed. */
    candidate_functions->arg_matches = NULL;
    if (candidate_functions->surrogate_function_conv_sym != NULL) {
      /* The best function is a surrogate function. */
      check_assertion(surrogate_function_conv_sym != NULL);  /* For Coverity */
      *surrogate_function_conv_sym =
                              candidate_functions->surrogate_function_conv_sym;
#if DEBUG
      if (debug_level >= 4 || db_flag_is_set("overload")) {
        db_display_overload_level();
        db_symbol(*surrogate_function_conv_sym,
                  "select_overloaded_function: selected surrogate ", 4);
      }  /* if */
#endif /* DEBUG */
    } else {
#if DEBUG
      if (debug_level >= 4 || db_flag_is_set("overload")) {
        db_display_overload_level();
        db_symbol(function_symbol,
                  "select_overloaded_function: selected ", 4);
      }  /* if */
#endif /* DEBUG */
    }  /* if */
  }  /* if */
  /* Free the candidate functions list. */
  free_candidate_function_list(candidate_functions);
have_function:
  if (do_dependent_name_processing && is_prototype_instantiation_context() &&
      !dependent_call && do_arg_dep_lookup &&
      !(function_symbol != NULL && is_block_extern_symbol(function_symbol))) {
    /* Record the outcome of overload resolution for a nondependent call
       in a prototype instantiation.  Dependent calls in such a context
       don't get here.  Calls where argument-dependent lookup is turned
       off are not recorded; they're considered non-dependent.  Also
       block externs, which usually do not require special handling
       because do_arg_dep_lookup is FALSE for them in standard mode,
       but might come up in other modes, and should not be recorded
       because they might have dependent return types or might depend
       on (nondependent) typedefs in the prototype instantiation. */
    /* Note that function_symbol can be NULL here, e.g., for a call of
       a (possibly dependent) block extern declaration, which must be
       resolved in the real instantiation. */
    check_assertion(paren_tok_seq_number != 0);
    record_nondependent_call(function_symbol, paren_tok_seq_number,
                             (a_nondependent_call_depth)0);
  }  /* if */
#if DEBUG
  if (debug_level >= 4 || db_flag_is_set("overload")) {
    db_display_overload_level();
    db_symbol(function_symbol,
              "Leaving select_overloaded_function, function_symbol = ", 4);
  }  /* if */
  overload_level--;
#endif /* DEBUG */
  db_exit();
  return function_symbol;
}  /* select_overloaded_function */


void issue_warning_from_arg_match_summary(an_arg_match_summary_ptr amsp,
                                          a_source_position        *err_pos)
/*
Issue any suggested warning recorded in an argument match summary.
*err_pos is the source position to be used.
*/
{
  if (amsp->conversion.std.warning_suggested != ec_no_error) {
    expr_pos_warning(amsp->conversion.std.warning_suggested, err_pos);
  } else if (amsp->const_anachronism) {
    /* This call depends on the anachronism that allows a non-const function
       to be called with a const object. */
    expr_pos_warning(ec_const_function_anachronism, err_pos);
  }  /* if */
}  /* issue_warning_from_arg_match_summary */


a_type_ptr operand_complete_object_type(an_operand *operand,
                                        a_boolean  call_case)
/*
Return the type of the complete object that contains the object indicated
by operand (an lvalue or rvalue), or NULL if no complete object type can be
determined.  call_case is TRUE if the answer will be used to optimize a
virtual function call.  NULL is always a safe answer; non-NULL values may
permit optimizations.  Note that "complete object" means an object that is
not a base class of another object, not necessarily a top-level object.
This is used only in C++ mode; it is useful to know what the complete object
type is to optimize base class casts and virtual function calls.
*/
{
  a_type_ptr complete_object_type = NULL;

  if (is_expression_operand(operand)) {
    complete_object_type = 
                         expr_complete_object_type(operand->variant.expression,
                                                   call_case);
  } else if (is_an_lvalue(operand)) {
    if (is_constant_operand(operand)) {
      /* The only lvalue case that is a constant is a string literal. */
      if (operand_is_string_literal(operand)) {
        complete_object_type = operand->type;
      }  /* if */
    }  /* if */
  } else if (is_error_operand(operand)) {
    complete_object_type = NULL;
  } else if (is_an_rvalue(operand)) {
    /* For other rvalues (not in expression form), the complete object type
       is the operand type. */
    complete_object_type = operand->type;
  }  /* if */
  return complete_object_type;
}  /* operand_complete_object_type */

#if OPTIMIZE_VIRTUAL_FUNCTION_CALLS

static a_type_ptr pointer_operand_complete_object_type(an_operand *operand,
                                                       a_boolean  call_case)
/*
Return the type of the complete object that contains the location pointed to
by operand (an rvalue), or NULL if no complete object type can be determined.
call_case is TRUE if the answer will be used to optimize a virtual function
call.  NULL is always a safe answer; non-NULL values may permit optimizations.
Note that "complete object" means an object that is not a base class of
another object, not necessarily a top-level object.  This is used only in
C++ mode; it is useful to know what the complete object type is to optimize
base class casts and virtual function calls.  In C++/CLI mode, the operand
can be a handle.
*/
{
  a_type_ptr complete_object_type = NULL;

  check_assertion((is_an_rvalue(operand) &&
                   (is_pointer_or_handle_type(operand->type) ||
                    is_template_param_type(operand->type) ||
                    is_error_type(operand->type))) ||
                  is_error_operand(operand));
  if (is_constant_operand(operand)) {
    complete_object_type =
                  pointer_con_complete_object_type(&operand->variant.constant);
  } else if (is_expression_operand(operand)) {
    complete_object_type = 
                 pointer_expr_complete_object_type(operand->variant.expression,
                                                   call_case);
  }  /* if */
  return complete_object_type;
}  /* pointer_operand_complete_object_type */

#endif /* OPTIMIZE_VIRTUAL_FUNCTION_CALLS */

#if !OPTIMIZE_VIRTUAL_FUNCTION_CALLS
/*ARGSUSED*/  /* <-- bound_function_selector is not used in that case. */
#endif /* !OPTIMIZE_VIRTUAL_FUNCTION_CALLS */
void bind_member_function_operand_to_selector(
                                         an_operand *bound_function_selector,
                                         a_boolean  selector_is_object_pointer,
                                         an_operand *function_operand)
/*
Bind the operand for a function to an associated selector object.  If the
complete object type can be determined, convert a virtual function call
into a direct call if possible.  The selector object can be a class lvalue
or rvalue ("." or ".*" case, selector_is_object_pointer FALSE) or a pointer
or handle to a class ("->" or "->*" case, selector_is_object_pointer TRUE).
*/
{
  function_operand->bound_function = TRUE;
  check_assertion((selector_is_object_pointer ==
                   is_pointer_or_handle_type(bound_function_selector->type)) ||
                  (is_template_dependent_context() &&
                   is_template_dependent_type(bound_function_selector->type))||
                  is_error_operand(bound_function_selector));
  bound_function_selector->selector_is_object_pointer =
                                                    selector_is_object_pointer;
#if OPTIMIZE_VIRTUAL_FUNCTION_CALLS
  if (function_operand->virtual_function &&
      is_expression_operand(function_operand)) {
    /* Virtual function call */
    a_routine_ptr    function;
    an_expr_node_ptr function_expr;
    a_type_ptr       complete_object_type = NULL;

    if (selector_is_object_pointer) {
      complete_object_type =
                  pointer_operand_complete_object_type(bound_function_selector,
                                                       /*call_case=*/TRUE);
    } else {
      complete_object_type =
                          operand_complete_object_type(bound_function_selector,
                                                       /*call_case=*/TRUE);
    }  /* if */
    if (complete_object_type != NULL) {
      /* We know the type of the complete object: we may be able to
         determine the specific function to call and suppress the
         virtual function mechanism. */
      a_type_ptr class_of_orig_function;

      complete_object_type = skip_typerefs(complete_object_type);
      /* Extract the routine being called.  We don't use
         routine_from_function_operand here because we only need to handle
         one case, and furthermore we want to know the structure of the
         function operand so we can alter it below without rebuilding it. */
      function_expr = function_operand->variant.expression;
      check_assertion(is_routine_node(function_expr));
      function = function_expr->variant.routine;
      class_of_orig_function = parent_class_of(function);

      if (identical_types(complete_object_type, class_of_orig_function)) {
        /* The function is a direct member of the class of the complete
           object.  Everything is already set up to call it directly, so
           we just need to flag it as a non-virtual call. */
        function_operand->virtual_function = FALSE;
      } else {
        /* The routine is not a direct member of the class of the complete
           object, so we have to check if it is overridden there.  (This
           case can come up because of casts in the function selector,
           e.g., ((base*) derived_p)->f().) */
        an_expr_node_ptr implicit_this_arg;
        a_routine_ptr    overrider;

        /* The routine is a member of a base class of the complete object,
           and the implicit "this" argument should already have been
           adjusted to the corresponding type before we get here -- which
           means that the bound_function_selector should be sitting on top
           of at least one eok_base_class_cast expression node. */
        check_assertion(is_expression_operand(bound_function_selector));
        implicit_this_arg = bound_function_selector->variant.expression;
        overrider = final_overrider(function, implicit_this_arg,
                                    complete_object_type);
        if (same_entities(function, overrider)) {
          /* Again, everything is already set up for a direct call. */
          function_operand->virtual_function = FALSE;
        } else {
          /* The function is overridden in the complete type.  We need to
             make a cast to the appropriate class type for the "this"
             argument and to update the function operand to refer to the
             actual function being called. */
          an_expr_node_ptr new_top_of_tree;
          a_type_ptr       class_of_overrider = parent_class_of(overrider);
          a_type_ptr       qualifiers_model = implicit_this_arg->type;
          an_expr_node_ptr new_parent;
          if (is_pointer_type(qualifiers_model)) {
            qualifiers_model = type_pointed_to(qualifiers_model);
          }  /* if */
          new_parent= retrace_base_casts(implicit_this_arg, class_of_overrider,
                                         qualifiers_model, &new_top_of_tree);
          if (new_parent != NULL) {
            /* The cast to the derived class was successful (i.e., the
               chain of base class casts was in the correct form to be
               inverted and there were no virtual base classes).  Mark the
               call as nonvirtual and splice the new chain of derived
               class casts between the selector operand and the previous
               implicit "this" argument expression. */
            new_parent->variant.operation.operands = implicit_this_arg;
            bound_function_selector->variant.expression = new_top_of_tree;
            bound_function_selector->type = new_top_of_tree->type;
            /* Change the function operand to refer to the actual function to
               be called. */
            { a_ref_entry_ptr rep = function_operand->ref_entries_list;
              if (rep != NULL) rep->symbol = symbol_for(overrider);
              function_expr->variant.routine = overrider;
              function_operand->orig_routine_type = function->type;
              function_expr->type = overrider->type;
              if (!function_expr->is_lvalue) {
                function_expr->type = make_pointer_type(function_expr->type);
              }  /* if */
              function_operand->type = function_expr->type;
              function_operand->virtual_function = FALSE;
            }
            function = overrider;
          }  /* if */
        }  /* if */
      }  /* if */
      if (!function_operand->virtual_function) {
        /* We optimized away the virtual function call: set the IL
           referenced flag for the function.  It wasn't set when the call
           was thought to be virtual, since a virtual call does not
           necessarily end up at the indicated routine. */
        if_evaluating_mark_routine_referenced(function);
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* OPTIMIZE_VIRTUAL_FUNCTION_CALLS */
}  /* bind_member_function_operand_to_selector */


void overloaded_function_catch_up(a_symbol_ptr      function_symbol,
                                  a_symbol_ptr      overloaded_function_symbol,
                                  an_operand        *orig_function_operand,
                                  a_source_position *call_position,
                                  a_boolean         elided_reference,
                                  a_boolean         result_is_lvalue,
                                  a_boolean         address_taken,
                                  an_operand        *operand,
                                  a_boolean         *access_error_reported)
/*
We've just determined which specific function within a set of overloaded
functions is being referenced, i.e., function_symbol was chosen from the
set given by overloaded_function_symbol.  (function_symbol is not an
overloaded function, but it might be a projection symbol.)  Do whatever
would have been done with the function along the way if we had known all
along which specific function was intended.  That is, "catch up" with the
processing that would have been done up to this point for a non-overloaded
function, e.g., access checking and recording of references.  While this
is usually used for overloaded functions, it is also used for a few cases
where the function is not overloaded but it is not convenient to note that
fact before scanning the arguments (e.g., operator overloading).
Therefore, while overloaded_function_symbol is typically an
sk_overloaded_function containing function_symbol, it may be the same as
function_symbol, or it may be a projection symbol for one of those, or it
may be an sk_function_template symbol.  Generate an operand for the
specific function in *operand (more on this below).  orig_function_operand,
if non-NULL, gives the operand that has been used to hold the overloaded
function name reference so far, which is used to provide some additional
information (e.g., source positions, whether the name is qualified).
orig_function_operand must not be the same operand as the result operand.
If orig_function_operand is NULL (e.g., for operator overloading),
call_position provides a position for the call.  Access control and
ambiguity checking are always done, even if the
overloaded_function_symbol is a non-overloaded function.  On return,
*access_error_reported is TRUE if an access control checking error was
detected and reported.

This routine handles several kinds of uses, indicated by result_is_lvalue
and address_taken.  When result_is_lvalue is TRUE (address_taken must be
FALSE), the operand created is an lvalue for the function; this is used,
for example, when a reference is bound to an overloaded function.  When
result_is_lvalue is FALSE and address_taken is FALSE, the operand created
is an rvalue address of the function suitable for use in calling the
function; the address is assumed not to escape, and the operand for a
nonstatic member function is a pointer, not a pointer to member (weird,
but that's what calls require).  When result_is_lvalue is FALSE and
address_taken is TRUE, the operand created is an rvalue for the address of
the function as would be created by the "&" operator; the address is
assumed to escape (i.e., address_taken is set), and the operand for a
nonstatic member function is a pointer to member.  operand can be NULL
if it is not necessary to generate the function designator operand;
address_taken still has an effect in that it controls the type of
reference recorded.  elided_reference is TRUE if the routine was
referenced in the program but the reference is being elided in the
intermediate language (operand should be NULL in that case).
*/
{
  a_symbol_ptr      base_function_symbol =
                                        fundamental_symbol_of(function_symbol);
  a_symbol_ptr      base_overloaded_function_symbol =
                             fundamental_symbol_of(overloaded_function_symbol);
  a_symbol_locator  function_symbol_locator;
  a_ref_entry_ptr   rep;
  a_boolean         eff_address_taken = address_taken;
  a_boolean         is_qualified_name = FALSE;
  a_boolean         has_required_ptr_to_member_form = FALSE;
  a_source_position *ampersand_position = NULL;
  a_source_position *function_position;
  a_source_position *function_end_position;
  a_source_position *id_position;

#if CHECKING
  if (base_function_symbol->kind != (a_symbol_kind)sk_routine &&
      base_function_symbol->kind != (a_symbol_kind)sk_member_function) {
    internal_error("overloaded_function_catch_up: bad function_symbol");
  }  /* if */
#endif /* CHECKING */
  check_assertion(!(result_is_lvalue && address_taken));
  if (orig_function_operand != NULL) {
    check_assertion(orig_function_operand != operand);
    is_qualified_name = orig_function_operand->is_qualified_name;
    if (orig_function_operand->is_operand_of_address_of) {
      ampersand_position = &orig_function_operand->ampersand_position;
      has_required_ptr_to_member_form =
                        orig_function_operand->has_required_ptr_to_member_form;
    }  /* if */
    function_position = &orig_function_operand->position;
    function_end_position =
                    end_position_or_null(&orig_function_operand->end_position);
    if (is_indefinite_function_operand(orig_function_operand) ||
        is_undefined_symbol_operand(orig_function_operand)) {
      id_position = &orig_function_operand->id_position;
    } else {
      id_position = function_position;
    }  /* if */
  } else {
    check_assertion(call_position != NULL);
    function_position = function_end_position = id_position = call_position;
  }  /* if */
  /* The address of the function is not really taken if the current expression
     is not evaluated. */
  if (!curr_expr_is_potentially_evaluated()) eff_address_taken = FALSE;
  /* Check ambiguity and access. */
  if (base_overloaded_function_symbol->kind ==
                                       (a_symbol_kind)sk_overloaded_function ||
      base_overloaded_function_symbol->kind ==
                                       (a_symbol_kind)sk_function_template) {
    /* Use a special routine for overloaded functions because (a) overloaded
       functions are considered always accessible when checked through the
       normal routine and (b) the projection symbol here may point to the
       overloaded function symbol rather than to the specific function
       symbol.  Templates are also treated like overloaded functions. */
    make_locator_for_symbol(function_symbol, &function_symbol_locator);
    function_symbol_locator.source_position = *id_position;
    expr_overload_check_ambiguity_and_verify_access(&function_symbol_locator,
                                                   overloaded_function_symbol);
  } else {
    /* Non-overloaded function; use the normal routine.
       overloaded_function_symbol is either the same as function_symbol
       or is a projection symbol for it. */
    make_locator_for_symbol(overloaded_function_symbol,
                            &function_symbol_locator);
    function_symbol_locator.source_position = *id_position;
    expr_check_ambiguity_and_verify_access(&function_symbol_locator);
  }  /* if */
  *access_error_reported =
                         function_symbol_locator.access_control_error_reported;
  if (is_error_locator(function_symbol_locator)) {
    /* An error locator is returned for an ambiguous case. */
    if (operand != NULL) {
      make_error_operand(operand);
      operand->position = *function_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
      operand->end_position = *function_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    }  /* if */
  } else {
    if (elided_reference) {
      /* The reference to the routine was elided (e.g., in a "new" where the
         "new" call can be folded into a constructor call).  Mark the symbol
         as referenced, but not the IL entry. */
      check_assertion(operand == NULL);
      /* Note that address_taken does not affect the kind of reference.
         That's intentional, since this is not a "real" reference. */
      record_symbol_reference(SRK_REFERENCE, base_function_symbol,
                              id_position, /*update_il_entry=*/FALSE);
    } else {
      /* The reference is not elided. */
      if (operand == NULL) {
        /* We don't want an operand, presumably because the function is
           being used in some unusual way and the caller will build the
           operand.  Mark the function as referenced.  Note that we are
           ignoring whether or not the function is virtual; we are assuming
           that the reference is to exactly that function. */
        record_symbol_reference(SRK_REFERENCE |
                                  (eff_address_taken ? SRK_ADDRESS_TAKEN : 0),
                                base_function_symbol, id_position,
                                /*update_il_entry=*/FALSE);
        if_evaluating_mark_routine_referenced(base_function_symbol->
                                                         variant.routine.ptr);
      } else {
        /* Normal case: build an operand for the function. */
        a_boolean ptr_to_member_case = FALSE;
        if (address_taken &&
            base_function_symbol->kind == (a_symbol_kind)sk_member_function) {
          a_type_ptr routine_type = routine_symbol_type(base_function_symbol);
          if (routine_type_is_nonstatic_member_function(routine_type)) {
            ptr_to_member_case = TRUE;
          }  /* if */
        }  /* if */
        /* Record that the function was referenced, for cross-reference (etc.)
           purposes. */
        rep = ref_entry(base_function_symbol, id_position);
        if (ptr_to_member_case) {
          /* We're taking the address of a nonstatic member function.
             Make an operand for a pointer to member. */
          make_ptr_to_member_constant_operand(function_symbol,
                                              overloaded_function_symbol,
                                              function_position,
                                              function_end_position,
                                              !*access_error_reported,
                                              is_qualified_name,
                                              has_required_ptr_to_member_form,
                                              operand);
          operand->ref_entries_list = rep;
          change_ref_kinds(operand->ref_entries_list, SRK_ADDRESS_TAKEN);
        } else {
          /* All cases other than the pointer-to-member case. */
          make_function_designator_operand(function_symbol,
                                           is_qualified_name,
                                           function_position,
                                           function_end_position,
                                           rep, operand);
          if (!result_is_lvalue) {
            /* Convert the operand to a function pointer.  Note the use of
               a special routine that will convert a nonstatic member
               function designator to a pointer rather than a pointer to
               member.  Also, it doesn't change the reference kind to
               SRK_ADDRESS_TAKEN. */
            conv_expr_function_designator_to_ptr_to_function(
                                                           operand,
                                                           !address_taken,
                                                           ampersand_position);
            if (eff_address_taken) {
              change_some_ref_kinds(operand->ref_entries_list,
                                    SRK_REFERENCE, SRK_ADDRESS_TAKEN);
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* overloaded_function_catch_up */
  

static a_boolean is_dependent_selection_first_operand(
                                            a_boolean        is_arrow_operator,
                                            an_expr_node_ptr left_operand)
/*
left_operand is the first operand of a static selection operation.  The
operation is a "->" if is_arrow_operator is TRUE, or a "." otherwise.
Return TRUE if the selection is dependent, meaning among other things
that we can't tell what the selected member is, or even whether it is
static or nonstatic.
*/
{
  a_boolean is_dependent = FALSE;

  if (is_template_dependent_context()) {
    a_type_ptr left_type = left_operand->type;
    if (is_arrow_operator && is_pointer_type(left_type)) {
      left_type = type_pointed_to(left_type);
    }  /* if */
    if (could_be_dependent_class_type(left_type) ||
        is_error_type(left_type)) {
      /* The left operand type is unknown, so we don't know what class we
         would look up the member in. */
      is_dependent = TRUE;
    }  /* if */
  }  /* if */
  return is_dependent;
}  /* is_dependent_selection_first_operand */


a_boolean is_dependent_static_selection(an_expr_node_ptr sel_expr)
/*
Return TRUE if the indicated expression is a static selection operation
and it is dependent, meaning we can't tell what the selected member is
or whether it is static or nonstatic.
*/
{
  a_boolean        is_dependent = FALSE, is_selection = FALSE;
  a_boolean        is_arrow_operator;
  an_expr_node_ptr op1;

  if (is_operation_node(sel_expr)) {
    if (node_operator_is(sel_expr, eok_dot_static)) {
      is_arrow_operator = FALSE;
      is_selection = TRUE;
    } else if (node_operator_is(sel_expr, eok_points_to_static)) {
      is_arrow_operator = TRUE;
      is_selection = TRUE;
    }  /* if */
    if (is_selection) {
      op1 = sel_expr->variant.operation.operands;
      is_dependent = is_dependent_selection_first_operand(is_arrow_operator,
                                                          op1);
    }  /* if */
  }  /* if */
  return is_dependent;
}  /* is_dependent_static_selection */
  

void combine_unneeded_selector_with_operand(
                                           an_operand *bound_function_selector,
                                           a_boolean  is_arrow_operator,
                                           an_operand *operand)
/*
*operand is a reference to a static class member, and *bound_function_selector
is an unneeded selector for that reference.  Save it by attaching it to
*operand (it must be evaluated, even though its type only -- and not its
value -- is used to select the member referenced).  is_arrow_operator is
TRUE if the operator is "->", FALSE if it is ".".
*/
{
  an_operand            orig_operand;
  an_expr_node_ptr      selector_expr, expr, orig_expr;
  an_expr_node_ptr      stripped_selector_expr, stripped_orig_expr;
  an_operand_state      saved_operand_state = operand->state;
  an_expr_operator_kind op;
  a_boolean             need_expr = FALSE, need_expr_for_constant = FALSE;
  a_boolean             discard_selector_in_expr = FALSE;
  a_boolean             template_constant = FALSE;

  orig_operand = *operand;
  selector_expr = make_node_from_operand(bound_function_selector);
  orig_expr = make_node_from_operand(operand);
  stripped_selector_expr = skip_parens(selector_expr);
  stripped_orig_expr = skip_parens(orig_expr);
  if (is_template_dependent_context() &&
      is_constant_node(stripped_selector_expr) &&
      is_constant_node(stripped_orig_expr) &&
      !stripped_orig_expr->is_lvalue &&
      (stripped_selector_expr->variant.constant->kind ==
                                   (a_constant_repr_kind)ck_template_param ||
       stripped_orig_expr->variant.constant->kind ==
                                   (a_constant_repr_kind)ck_template_param)) {
    /* The operation is based on constants, and at least one of those
       is a ck_template_param, so build an expression and later put that
       under a tpck_expression constant, thus producing a constant result. */
    need_expr = TRUE;
    template_constant = TRUE;
  } else if (!stripped_orig_expr->is_lvalue &&
             is_constant_node(stripped_orig_expr) &&
             current_mode_allows_dot_static_folding(stripped_selector_expr) &&
             !is_dependent_selection_first_operand(is_arrow_operator,
                                                   selector_expr)) {
    /* In certain modes, produce a constant result for an rvalue.
       Note that only things like enumerator values are handled here.  Most
       others stay as lvalues at this point and are converted to the constant
       when lvalue-to-rvalue conversion is done. */
    /* Don't do this simplification for template-dependent cases, because
       we're going to need the left operand later to do substitution
       to find out what we really have.  We might find we have a nonstatic
       selection. */
    /* The is_constant_node test below may seem redundant, but is needed for
       template-dependent constants in prototype instantiations,
       because such constants are considered to have side effects. */
    check_assertion(is_constant_node(stripped_selector_expr) ||
                    !node_has_side_effects(stripped_selector_expr,
                                           (a_boolean *)NULL) ||
                    is_error_node(stripped_selector_expr));
    make_constant_operand(stripped_orig_expr->variant.constant, operand);
    need_expr = curr_expr_kind_is_one_in_which_const_exprs_are_recorded();
    need_expr_for_constant = need_expr;
    if (curr_il_region_number == file_scope_region_number &&
        innermost_function_scope != NULL) {
      /* For an expression scanned within a function body whose constant will
         be at file scope, like an array bound, discard the selector expression
         because it might reference "this" or local variables in the first
         operand. */
      discard_selector_in_expr = TRUE;
    }  /* if */
  } else {
    /* In all other cases, the result will be an expression. */
    need_expr = TRUE;
  }  /* if */
  if (need_expr) {
    /* Make an expression for the selection. */
    if (discard_selector_in_expr) {
      /* For the reasons given above, discard the selector and use just the
         second operand as the expression. */
      expr = orig_expr;
    } else {
      /* Make an expression for a static selection. */
      selector_expr->next = orig_expr;
      /* Determine the operator to use. */
      if (is_arrow_operator) {
        op = (an_expr_operator_kind)eok_points_to_static;
      } else {
        op = (an_expr_operator_kind)eok_dot_static;
      }  /* if */
      /* Make a node for the selector and the operand. */
      expr = make_operator_node(op, orig_expr->type, selector_expr);
      if (orig_expr->is_lvalue) expr->is_lvalue = TRUE;
    }  /* if */
    /* Save the expression as either the overall result or as the backing
       expression for a constant result. */
    if (need_expr_for_constant) {
      operand->variant.constant.expr = expr;
    } else {
      make_expression_operand(expr, operand);
      operand->state = saved_operand_state;
      if (template_constant) {
        make_template_param_expr_constant_operand(operand);
      }  /* if */
      /* Restore the reference entries list too so that we can get
         address_taken set on the function. */
      restore_operand_details_incl_ref(operand, &orig_operand);
    }  /* if */
  }  /* if */
  rule_out_expr_kinds(ROEK_CONSTANT, operand);
}  /* combine_unneeded_selector_with_operand */


void cast_pointer_for_field_selection(
                               an_operand        *operand_1,
                               a_boolean         is_arrow_operator,
                               a_symbol_ptr      member_sym,
                               a_symbol_ptr      projection_member_sym,
                               a_boolean         access_control_error_reported,
                               a_boolean         do_protected_member_check,
                               a_source_position *member_pos)
/*
Adjust the left operand of a "->" or "." operation, if necessary, to make
it point to a class/struct/union of the type containing the member.
This is significant in C++, where the member may be in a base class of the
left-operand class, and baseward casts are needed.  operand_1 is the left
operand.  is_arrow_operator is TRUE for "->", FALSE for ".".
member_sym is the referenced member (possibly a projection symbol, but the
presence or absence of a projection is largely ignored).
projection_member_sym is either the same as member_sym or a projection
thereof, or, for a reference to an overloaded function, the symbol for
the overload set or a projection thereof -- it identifies the symbol
that was actually named in the member reference, and a projection
symbol on it is significant.  access_control_error_reported is TRUE if
an access control error has already been reported.
do_protected_member_check is TRUE if the protected member access check
of 11.5 in the C++ standard should be done.  *member_pos gives the
source position of the member name reference.  This routine also handles
the case where the left operand is a C++/CLI handle.
*/
{
  a_type_ptr       desired_class = sym_parent_class(projection_member_sym);
  a_type_ptr       class_struct_union_type;
  a_base_class_ptr bcp;

  /* Leave an error operand alone. */
  if (!is_error_operand(operand_1)) {
    class_struct_union_type = operand_1->type;
    if (is_arrow_operator) {
      if (is_template_param_or_nonreal_class_type(class_struct_union_type)) {
        /* Pointer type is unknown, in a prototype instantiation.  Or, the
           selector has a nonreal class type, which might have an operator->
           function. */
        class_struct_union_type = type_of_unknown_templ_param_nontype;
      } else {
        /* Normal case.  Go from the pointer type to the underlying class
           type. */
        class_struct_union_type = type_pointed_to(class_struct_union_type);
      }  /* if */
    }  /* if */
    /* Drop any typedefs on the class type. */
    class_struct_union_type = skip_typerefs(class_struct_union_type);
    check_assertion(is_immediate_class_type(class_struct_union_type) ||
                    class_struct_union_type->kind ==
                                               (a_type_kind)tk_template_param);
    if (is_template_dependent_context() &&
        (is_template_param_type(class_struct_union_type) ||
         class_struct_union_type->variant.class_struct_union.is_nonreal_class||
         desired_class->variant.class_struct_union.is_nonreal_class) &&
        (projection_member_sym->kind == (a_symbol_kind)sk_projection ||
         (!same_entities(class_struct_union_type, desired_class) &&
          (is_template_param_type(class_struct_union_type) ||
           find_base_class_of(class_struct_union_type,
                              desired_class) == NULL)))) {
      /* Don't do any checking on nonreal classes in prototype
         instantiations, unless it does happen that there is a relationship. */
      prep_generic_operand(operand_1);
    } else {
      /* If the member is protected, it can only be accessed through an object
         or pointer of a type to which we have member access (ARM 11.5). */
      if (do_protected_member_check && !access_control_error_reported &&
          expr_access_checking_should_be_done()) {
        a_boolean error_detected = FALSE;
        a_boolean *p_error_detected = NULL;
        /* If errors are suppressed, get a returned variable instead of issuing
           any error. */
        if (expr_stack->suppress_diagnostics) {
          p_error_detected = &error_detected;
        }  /* if */
        (void)check_protected_member_access(member_sym, projection_member_sym,
                                            member_pos,
                                            class_struct_union_type,
                                            p_error_detected);
        if (error_detected) record_suppressed_error();
      }  /* if */
      /* Do nothing if the type is already okay (which it almost always
         will be; only in cases involving qualified names can it be
         different). */
      if (!same_entities(class_struct_union_type, desired_class)) {
        /* Some adjustment is required.  Find out how the classes are
           related to one another. */
        bcp = find_base_class_of(class_struct_union_type, desired_class);
        if (bcp == NULL) {
          check_assertion(total_errors != 0);
        } else {
          /* Cast the left operand to the proper type. */
          base_class_cast_operand(operand_1, bcp, (a_type_ptr)NULL,
                                  /*check_cast_access=*/
                                                !access_control_error_reported,
                                  /*is_implicit_cast=*/TRUE,
                                  /*implicit_in_naming=*/FALSE,
                                  /*is_object_pointer=*/TRUE);
        }  /* if */
        class_struct_union_type = desired_class;
      }  /* if */
      /* If the member symbol is a projection symbol (i.e., it's inherited
         into the class where it is being referenced), cast the left operand
         down to the base class in which the fundamental symbol is defined.
         There's no access check on this part of the cast because the access
         to the fundamental base class was checked as part of determining
         access to the symbol. */
      if (projection_member_sym->kind == (a_symbol_kind)sk_projection) {
        bcp = projection_member_sym->variant.projection.extra_info->
                                                        fundamental_base_class;
        /* Normally, when a projection symbol is used it means the name was
           specified as a simple name.  This is not the case for a projection
           symbol created for a Microsoft __super lookup. */
        base_class_cast_operand(operand_1, bcp, (a_type_ptr)NULL,
                                /*check_cast_access=*/FALSE,
                                /*is_implicit_cast=*/TRUE,
                                /*implicit_in_naming=*/
                                    if_microsoft_extensions_else(
                                       !projection_member_sym->
                                                           is_super_reference,
                                       TRUE),
                                /*is_object_pointer=*/TRUE);
        class_struct_union_type = bcp->type;
      }  /* if */
      if (projection_member_sym != member_sym) {
        if (!same_entities(sym_parent_class(member_sym),
                           class_struct_union_type)) {
          /* In some cases, the member_sym and the projection_member_sym
             don't quite meet up -- there's a gap in the base class
             sequence.  This happens, for example, when a template
             instance is generated; it is generated in the fundamental
             class and no projection symbol exists for it.  Look for the
             member of the overload set of projection_member_sym that is
             the appropriate projection symbol. */
          a_symbol_ptr fund_sym = fundamental_symbol_of(projection_member_sym);
          a_symbol_ptr fund_member_sym = fundamental_symbol_of(member_sym);
          a_symbol_ptr sym;
          check_assertion(fund_sym->kind ==
                                        (a_symbol_kind)sk_overloaded_function);
          for (sym = fund_sym->variant.overloaded_function.symbols;
               ;
               sym = sym->next) {
            check_assertion(sym != NULL);
            fund_sym = fundamental_symbol_of(sym);
            /* If the symbol is a member function template, see if the
               function_symbol is an instance of the template.  Otherwise,
               just compare the pointers. */
            if (fund_sym->kind == (a_symbol_kind)sk_function_template &&
                fund_member_sym->variant.routine.instance_ptr != NULL &&
                fund_member_sym->variant.routine.instance_ptr->
                                                    template_sym == fund_sym) {
              member_sym = sym;
              break;
            }  /* if */
          }  /* for */
          /* Remove any namespace projection symbols. */
          while (member_sym->kind == (a_symbol_kind)sk_namespace_projection) {
            member_sym = namespace_projection_fundamental_symbol(sym);
          }  /* while */
        }  /* if */
        if (member_sym->kind == (a_symbol_kind)sk_projection) {
          /* This comes up with overload sets that contain using-declarations.
             Cast from the using-declaration class to the class of the
             member. */
          bcp = member_sym->variant.projection.extra_info->
                                                        fundamental_base_class;
          /* Normally, when a projection symbol is used it means the name was
             specified as a simple name.  This is not the case for a projection
             symbol created for a Microsoft __super lookup. */
          base_class_cast_operand(operand_1, bcp, (a_type_ptr)NULL,
                                  /*check_cast_access=*/FALSE,
                                  /*is_implicit_cast=*/TRUE,
                                  /*implicit_in_naming=*/
                                    if_microsoft_extensions_else(
                                       !member_sym->is_super_reference,
                                       TRUE),
                                  /*is_object_pointer=*/TRUE);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* cast_pointer_for_field_selection */


static a_boolean variable_this_exists_full(a_variable_ptr *this_var,
                                           a_boolean      allow_lambda_this)
/*
Return TRUE if there is a currently-visible "this" variable.  If there is,
also set *this_var to point to the variable entry for it.  The captured
"this" in a lambda body (which is the "this" from the function enclosing the
lambda, not the "this" that points to the closure class object) is considered
visible only if allow_lambda_this is TRUE.  This routine is called only
in C++ mode.  Note that there is no implication yet that the "this" is
actually being used; we may simply be testing that an implicit "this" is
available, e.g., during overload resolution.
*/
{
  a_boolean this_exists = FALSE;

  *this_var = NULL;
  if (innermost_function_scope != NULL) {
    a_routine_ptr curr_rout = current_routine_entry();
    if (curr_rout->is_lambda_body && allow_lambda_this) {
      /* We're inside the body of a lambda.  "this" exists only if it's
         captured from the surrounding context.  The lambda body is the
         operator() function of the lambda closure class, but the "this"
         of the operator() function isn't available as an implicit "this" --
         it's used only to access the fields of the closure class to fetch
         captured values. */
      a_type_ptr    closure_class = parent_class_of(curr_rout);
      a_routine_ptr encl_rout= closure_class->source_corresp.enclosing_routine;
      while (encl_rout != NULL && encl_rout->is_lambda_body) {
        /* We can reach out past intermediate lambdas. */
        closure_class = parent_class_of(encl_rout);
        encl_rout = closure_class->source_corresp.enclosing_routine;
      }  /* if */
      if (encl_rout != NULL) {
        /* There is a routine that encloses the lambda.  See if it is a
           nonstatic member function. */
        if (routine_type_is_nonstatic_member_function(encl_rout->type)) {
          /* It is, so it has a "this".  We delay until later checking whether
             the "this" is or can be captured. */
          a_scope_ptr scope = scope_for_routine(encl_rout);
          *this_var = scope->variant.routine.this_param_variable;
          check_assertion(*this_var != NULL);
          this_exists = TRUE;
        }  /* if */
      }  /* if */
    } else {
      /* Normal case, not inside a lambda. */
      *this_var= innermost_function_scope->variant.routine.this_param_variable;
      this_exists = (*this_var != NULL);
    }  /* if */
  }  /* if */
  return this_exists;
}  /* variable_this_exists_full */


a_boolean variable_this_exists(a_variable_ptr *this_var)
/*
Return TRUE if there is a currently-visible "this" variable.  If there is,
also set *this_var to point to the variable entry for it.  The captured
"this" in a lambda body (which is the "this" from the function enclosing the
lambda, not the "this" that points to the closure class object) is considered
visible.  This routine is called only in C++ mode.  It can be called from
outside the expression-processing routines.
*/
{
  a_boolean this_exists;

  this_exists = variable_this_exists_full(this_var,
                                          /*allow_lambda_this=*/TRUE);
  return this_exists;
}  /* variable_this_exists */


static a_variable_ptr this_variable_for_lambda_closure(void)
/*
We're currently inside a lambda body.  Return a pointer to the "this" variable
for the lambda closure class, which is used among other things to access the
fields that contain the captures of local variables.
*/
{
  a_variable_ptr this_var;

  check_assertion(innermost_function_scope != NULL &&
                  innermost_function_scope->variant.routine.ptr->
                                                               is_lambda_body);
  this_var = innermost_function_scope->variant.routine.this_param_variable;
  check_assertion(this_var != NULL && this_var->is_this_parameter);
  return this_var;
}  /* this_variable_for_lambda_closure */


an_expr_node_ptr make_selection_for_captured_variable(
                                              a_lambda_capture *lambda_capture,
                                              a_boolean        is_lvalue)
/*
Make a field selection expression for a captured variable in a lambda.
lambda_capture describes the variable.  The selection is an lvalue selection
if is_lvalue is TRUE.
*/
{
  a_variable_ptr   this_var = this_variable_for_lambda_closure();
  an_expr_node_ptr lambda_this = var_rvalue_expr(this_var);
  a_field_ptr      closure_field = lambda_capture->closure_field;
  an_expr_node_ptr sel_expr;

  sel_expr = field_lvalue_selection_expr(lambda_this, closure_field);
  if (!is_lvalue) sel_expr = rvalue_expr_for_lvalue(sel_expr);
  return sel_expr;
}  /* make_selection_for_captured_variable */


#if !EXTRA_SOURCE_POSITIONS_IN_IL
/*ARGSUSED*/ /* <-- is_implicit and end_position are not used in that case. */
#endif /* !EXTRA_SOURCE_POSITIONS_IN_IL */
void make_this_variable_operand(a_variable_ptr    this_var,
                                a_boolean         is_implicit,
                                a_source_position *position,
                                a_source_position *end_position,
                                an_operand        *result)
/*
Make an operand for the value of the "this" variable this_var.  The reference
is implicit if is_implicit is TRUE.  The source position of the operand is
set to *position and its end position (if present) to *end_position.
The position in the expression, which exists when EXTRA_SOURCE_POSITIONS_IN_IL
is TRUE, is set only when is_implicit is FALSE.  The operand is an rvalue.
*/
{
  an_expr_node_ptr node;

  if (in_lambda_body()) {
    /* We're inside a lambda body, so the "this" must be the one from
       the function enclosing the lambda.  It needs to be captured to be
       used. */
    a_lambda_capture *lambda_capture =
                               lambda_capture_for_variable(this_var, position);
    if (lambda_capture != NULL) {
      node = make_selection_for_captured_variable(lambda_capture,
                                                  /*is_lvalue=*/FALSE);
      make_expression_operand(node, result);
    } else {
      /* "this" cannot be captured. */
      expr_pos_error(ec_not_captured_this_in_lambda, position);
      make_error_operand(result);
    }  /* if */
  } else {
    /* Normal case, not in a lambda body. */
    /* Make a variable value node for the variable. */
    node = var_rvalue_expr(this_var);
    /* Make an operand for the node. */
    make_expression_operand(node, result);
  }  /* if */
  result->position = *position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  result->end_position = *end_position;
  if (!is_implicit) {
    /* Set the position in the expression too when the reference is
       explicit. */
    set_operand_expr_position_if_expr(result, (a_source_position *)NULL);
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  rule_out_expr_kinds(ROEK_CONSTANT, result);
}  /* make_this_variable_operand */


a_boolean make_this_pointer_operand(
                               a_symbol_ptr      member_sym,
                               a_symbol_ptr      projection_member_sym,
                               a_source_position *member_pos,
                               a_boolean         access_control_error_reported,
                               an_operand        *result)
/*
Make an operand for the "this" pointer of a C++ nonstatic member function.
The operand made is an rvalue for the value of the pointer.  If we are not
currently in a nonstatic member function, issue an error and return an
error operand.  member_sym is the referenced member (possibly a projection
symbol, but the presence or absence of a projection is ignored);
projection_member_sym is either the same as member_sym or a projection
thereof, or, for a reference to an overloaded function, the symbol for
the overload set or a projection thereof -- it identifies the symbol
that was actually named in the member reference, and a projection symbol
on it is significant.  The "this" pointer is cast (if necessary)
to the base class in which that member is defined.  Access checking is
done on that cast unless access_control_error_reported is TRUE.  If the
symbol is a member of an unrelated class, issue an error and return an
error operand.  member_pos is the position of the member, for use in
errors and as the source position of the result operand.  Return TRUE
if the "this" operand was built without error.  This routine is called
only in C++ mode.  Note that this routine is called only for an implicit
"this->", not for the explicit case.  Also note that this routine is
called when the "this" is actually being used, not when we're just
wondering if it's available.
*/
{
  a_variable_ptr   this_var;
  a_type_ptr       member_class, this_class;
  a_boolean        okay, template_case = FALSE;
  a_base_class_ptr bcp;

  /* This routine is similar to cast_pointer_for_field_selection. */
  if (curr_expr_kind_is_const()) {
    /* Nonstatic members are not allowed in constant expressions. */
    expr_pos_error(ec_expr_not_constant, member_pos);
    make_error_operand(result);
    okay = FALSE;
  } else {
    /* See if a "this" pointer exists and can be used. */
    if (!variable_this_exists(&this_var)) {
      /* We're not inside a function, or the function does not have a "this"
         variable. */
      okay = FALSE;
    } else {
      /* There is a "this" variable. */
      /* Find the relationship between the "this" variable and the class
         of the member. */
      this_class = type_pointed_to(this_var->type);
      this_class = skip_typerefs(this_class);
      check_assertion(projection_member_sym->is_class_member);
      member_class = sym_parent_class(projection_member_sym);
      if (same_entities(this_class, member_class)) {
        /* The class is right already.  This is the usual case. */
        bcp = NULL;
        okay = TRUE;
      } else {
        /* Look for a base class cast.  This comes up when qualified names
           are used.  Also watch out for error cases where the classes
           are unrelated. */
        bcp = find_base_class_of(this_class, member_class);
        okay = (bcp != NULL);
        if (!okay && is_template_dependent_context()) {
          /* In a prototype instantiation, there might be some unknown
             relationship between the classes. */
          okay = TRUE;
          template_case = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
    if (!okay) {
      /* The "this" pointer cannot be used (it doesn't exist or it has
         no relationship to the member). */
      expr_pos_error(ec_member_ref_requires_object, member_pos);
      make_error_operand(result);
    } else {
      /* The "this" pointer can be used to access the member. */
      /* Make an operand for the value of the "this" pointer. */
      make_this_variable_operand(this_var, /*is_implicit=*/TRUE, member_pos,
                                 member_pos, result);
      if (template_case) {
        /* For the template case, just do a direct cast. */
        a_symbol_ptr fund_sym = fundamental_symbol_of(member_sym);
        a_type_ptr   member_ptr, underlying_this_type;
        check_assertion(fund_sym->is_class_member);
        /* The pointer type has to be qualified the same as the "this"
           pointer type (e.g., if the function is "const", the pointer type
           must be pointer to const). */
        underlying_this_type = type_pointed_to(this_var->type);
        member_ptr = make_identically_qualified_type(
                                                   sym_parent_class(fund_sym),
                                                   underlying_this_type);
        member_ptr = make_pointer_type(member_ptr);
        cast_operand_full(member_ptr, result, (a_source_position *)NULL,
                          /*check_cast_access=*/FALSE,
                          /*check_ambiguity=*/FALSE,
                          /*is_implicit_cast=*/TRUE,
                          /*is_reinterpret_cast=*/FALSE,
                          /*reinterpret_semantics=*/FALSE);
      } else {
        /* Cast the "this" value to the class of the member. */
        /* Note that no ARM 11.5 protected member access check is needed,
           because an access through "this" is always acceptable under the
           rules in that section. */
        cast_pointer_for_field_selection(result,
                                         /*is_arrow_operator=*/TRUE,
                                         member_sym,
                                         projection_member_sym,
                                         access_control_error_reported,
                                         /*do_protected_member_check=*/FALSE,
                                         member_pos);
      }  /* if */
      /* Check for errors on the casts. */
      if (is_error_operand(result)) okay = FALSE;
    }  /* if */
  }  /* if */
  result->position = *member_pos;
  rule_out_expr_kinds(ROEK_CONSTANT, result);
  return okay;
}  /* make_this_pointer_operand */


a_boolean is_this_parameter_operand(an_operand     *operand,
                                    a_variable_ptr *p_this_var)
/*
Return TRUE if the given operand is for the "this" parameter of the
current function.  If so, and if p_this_var is non-NULL, also set
*p_this_var to the "this" variable.  The captured "this" of a lambda
is not considered to match.
*/
{
  a_boolean        is_this = FALSE;
  a_variable_ptr   this_var = NULL, operand_var;
  an_expr_node_ptr operand_expr;

  if (p_this_var != NULL) *p_this_var = NULL;
  if (is_an_rvalue(operand) && is_expression_operand(operand)) {
    operand_expr = skip_parens(operand->variant.expression);
    if (is_variable_node(operand_expr)) {
      /* The operand is an rvalue that is the value of a simple variable. */
      operand_var = operand_expr->variant.variable;
      if (variable_this_exists_full(&this_var, /*allow_lambda_this=*/FALSE)) {
        /* There is a current "this" parameter.  See if it matches the
           variable in the operand. */
        if (this_var == operand_var) {
          /* Yes. */
          is_this = TRUE;
          if (p_this_var != NULL) *p_this_var = this_var;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return is_this;
}  /* is_this_parameter_operand */


static void make_resolved_overloaded_function_operand(
                                 a_symbol_ptr       function_symbol,
                                 a_symbol_ptr       overloaded_function_symbol,
                                 an_operand         *orig_function_operand,
                                 a_source_position  *call_position,
                                 a_boolean          *have_selector,
                                 an_operand         *bound_function_selector,
                                 a_boolean          is_property,
                                 an_operand         *function_operand)
/*
Overload resolution has been done, and it has been decided that, of
the functions in overloaded_function_symbol (which may be a projection
symbol and/or just a simple function), function_symbol is the specific
function to be called (also possibly a projection symbol).  Create a
function designator operand for the function in *function_operand.
orig_function_operand, if non-NULL, gives the operand that has been
used to hold the overloaded function name reference so far, which is
used to provide some additional information (e.g., source positions,
whether the name is qualified).  orig_function_operand must not be the
same operand as function_operand.  If orig_function_operand is NULL
(e.g., for operator overloading), call_position provides a position
for the call.  If both orig_function_operand and call_position are
supplied, call_position is assumed to be a better position for the
call.  The call is the result of the expansion of a Microsoft
property reference if is_property is TRUE.  The reference has an
associated selector object if *have_selector is TRUE; in that case,
bound_function_selector gives the object, and function_operand is
bound to that object.  Even when *have_selector is FALSE going in,
bound_function_selector must point at an operand that can be filled in
if an implicit selector is generated (*have_selector is set to TRUE
for that case).  bound_function_selector->selector_is_object_pointer
is TRUE if the selector is an object pointer, FALSE if it is an
object.
*/
{
  a_symbol_ptr base_function_symbol = fundamental_symbol_of(function_symbol);
  a_boolean    access_error_reported;
  a_type_ptr   routine_type;
  a_boolean    selector_is_object_pointer = FALSE;

  check_assertion((orig_function_operand != NULL) ?
                                  (orig_function_operand != function_operand) :
                                  (call_position != NULL));
  /* Do whatever would have been done to the function had we known
     originally which specific function was intended. */
  overloaded_function_catch_up(function_symbol,
                               overloaded_function_symbol,
                               orig_function_operand,
                               call_position,
                               /*elided_reference=*/FALSE,
                               /*result_is_lvalue=*/FALSE,
                               /*address_taken=*/FALSE,
                               function_operand,
                               &access_error_reported);
  if (call_position == NULL) {
    /* Make sure call_position always has a usable position from this point
       forward in this routine. */
    call_position = &orig_function_operand->position;
  }  /* if */
  if (*have_selector) {
    selector_is_object_pointer =
                           bound_function_selector->selector_is_object_pointer;
  }  /* if */
  /* Check whether or not a selector is needed. */
  routine_type = routine_symbol_type(base_function_symbol);
  if (routine_type_is_nonstatic_member_function(routine_type)) {
    /* The function needs a selector. */
    if (!*have_selector) {
      /* We don't have a selector.  Try to generate one. */
      if (make_this_pointer_operand(function_symbol,
                                    overloaded_function_symbol,
                                    call_position,
                                    (a_boolean)function_operand->
                                         access_control_error_reported,
                                    bound_function_selector)) {
        /* The selector was generated without problem. */
      } else {
        /* There was some problem in generating the selector. */
        conv_to_error_operand(function_operand);
      }  /* if */
      *have_selector = TRUE;
      selector_is_object_pointer = TRUE;
    } else {
      a_source_position selector_position;
      /* We have a selector. */
      /* Cast the selector to the class of the member symbol. */
      /* Also do the ARM 11.5 access checking for the type of selector used
         to access a protected member. */
      selector_position = bound_function_selector->position;
      cast_pointer_for_field_selection(bound_function_selector,
                                       selector_is_object_pointer,
                                       function_symbol,
                                       overloaded_function_symbol,
                                       access_error_reported,
                                       /*do_protected_member_check=*/
                                                                  !is_property,
                                       &selector_position);
    }  /* if */
    /* Bind the function to the selector. */
    bind_member_function_operand_to_selector(bound_function_selector,
                                             selector_is_object_pointer,
                                             function_operand);
  } else {
    /* The routine does not need a selector. */
    if (*have_selector) {
      /* Attach the unneeded selector provided to the function operand. */
      combine_unneeded_selector_with_operand(bound_function_selector,
                                             selector_is_object_pointer,
                                             function_operand);
      *have_selector = FALSE;
    }  /* if */
  }  /* if */
}  /* make_resolved_overloaded_function_operand */

/*
We currently check a maximum of 99 positional arguments.  The following macro
must be kept consistent with that limit.  It must be at least a factor of 10
less than INT_MAX-9 (to avoid overflow).
*/
#define CHECKED_PRINTF_SCANF_ARG_POS_LIMIT  100

static int printf_scanf_arg_pos(char  **fmt_string_ptr)
/*
*fmt_string_ptr points to the first character following a '%' character of a
conversion specifier in a format string (specified as a literal) for a call
to a printf- or scanf-like function.  For conversion specifiers of the form
"%ddd$..." (where the 'd's stand for decimal digits), scan the positional
digits and return the associated position.  If the position is larger than 99,
return -1, and if it is zero, return -2: The caller is responsible for giving
up on checking the format string specifier in those cases.  *fmt_string_ptr
is updated to point after the string of digits and the "$" character (when
present).
*/
{
  int   result = 0, k = 0;

  if (check_printf_scanf_positional_args) {
    char  *pc = *fmt_string_ptr;
    while (isdigit((unsigned char)*pc)) {
      if (result < CHECKED_PRINTF_SCANF_ARG_POS_LIMIT) {
        result = result*10 + (int)(*pc - '0');
      }  /* if */
      ++pc; ++k;
    }  /* while */
    if (k > 0 && *pc == '$') {
      /* A positional field. */
      if (result >= CHECKED_PRINTF_SCANF_ARG_POS_LIMIT) {
        /* The position is larger than what we are willing to check.*/
        result = -1;
      } else if (result == 0) {
        /* The position cannot be zero. */
        result = -2;
      }  /* if */
      ++pc;
      *fmt_string_ptr = pc;
    } else {
      /* This wasn't a positional field.  Discard the value. */
      result = 0;
    }  /* if */
  }  /* if */
  return result;
}  /* printf_scanf_arg_pos */


static a_type_ptr next_printf_scanf_arg_type(
                                 a_boolean           is_scanf,
                                 char                **fmt_string_ptr,
                                 a_printf_scan_state *pss_ptr,
                                 a_boolean           *indirect,
                                 a_boolean           *weakly_typed,
                                 a_boolean           *weak_pointer_to_integral,
                                 a_type_ptr          *alt_type,
                                 int                 *value_pos,
                                 int                 *init_value_pos)
/*
Return the type that the next argument to a printf or scanf call should have,
by finding the next thing in the format string that consumes an argument.
Return NULL if no further arguments are needed.  is_scanf is TRUE for
scanf/FALSE for printf; *fmt_string_ptr points to the current position 
in the format string (it will be updated); and *pss_ptr is maintained
to handle resuming the scan after a "*" field width or precision.
If there is an error in the format string, set *fmt_string_ptr to NULL.
*indirect is returned TRUE if the type returned has an added pointer level
relative to the type indicated in the formatting string, e.g., for scanf.
*weakly_typed is returned TRUE if the formatting specifier is one that is
weakly typed, e.g. "%x".  *weak_pointer_to_integral is returned TRUE if the
required type is a pointer to an integral type, and any other pointer to
an integral type that differs only in signedness or cv-qualification
is also acceptable.  *alt_type is usually returned NULL, but if some
alternate type is also valid for the next argument (i.e., in addition
to the type returned), *alt_type is set to the alternate type.

See 4.9.6.1 in the standard for printf, 4.9.6.2 for scanf.

If check_printf_scanf_positional_args is TRUE, positional arguments are
recognized and returned through *value_pos (a value of zero indicates that
no positional argument indicator was seen, -1 indicates that the position
was too large to check, and -2 indicates that the position was zero).
A single format specifier may contain up to three positional indicators:
One for the value to format, one for the field width value, and one for the
precision.  In such cases, *value_pos will indicate the argument number
for the argument to be checked on this iteration, and *init_value_pos
is used to save the initial position (the one indicating the argument to
be formatted) when the current return is to check one of the others,
so it can be retrieved and returned on a subsequent call.  The caller
just needs to provide a variable for that, but doesn't need to do anything
to manage that variable.
*/
{
  a_type_ptr          required_type;
  char                *fmt_string = *fmt_string_ptr;
  a_printf_scan_state pss = *pss_ptr;
  a_boolean           l_size, L_size, h_size, add_pointer;
  a_boolean           hh_size, j_size, z_size, t_size;
#if LONG_LONG_ALLOWED
  a_boolean           ll_size;
#endif /* LONG_LONG_ALLOWED */
#if FIXED_POINT_ALLOWED
  char                type_char;
  a_boolean           is_fract_type = FALSE;
#endif /* FIXED_POINT_ALLOWED */
  a_boolean           suppress_assignment = FALSE;

  *weakly_typed = FALSE;
  *weak_pointer_to_integral = FALSE;
  *indirect = FALSE;
  *alt_type = NULL;
  /* Pick up in the middle if the previous call returned a field width
     or precision. */
  if (pss == pss_after_field_width) {
    *value_pos = *init_value_pos;
    *init_value_pos = 0;
    goto after_field_width;
  }  /* if */
  if (pss == pss_after_precision) {
    *value_pos = *init_value_pos;
    *init_value_pos = 0;
    goto after_precision;
  }  /* if */

  /* Look for the next "%" in the string, or the null that terminates it. */
another_specifier:;
  *value_pos = *init_value_pos = 0;
  suppress_assignment = FALSE;
  while (*fmt_string != '%' && *fmt_string != '\0') fmt_string++;
  /* If the null was found, there is no next argument. */
  if (*fmt_string == '\0') {
    required_type = NULL;
  } else {
    /* "%" was found. */
    fmt_string++;
    /* On "%%", go back and look for another specifier. */
    if (*fmt_string == '%') {
      fmt_string++;
      goto another_specifier;
    }  /* if */
    *value_pos = printf_scanf_arg_pos(&fmt_string);
    /* For printf, ignore a sequence of flags (-, +, space, #, or 0).
       For scanf, ignore the assignment-suppressing character "*". */
    if (is_scanf) {
      if (*fmt_string == '*') {
        fmt_string++;
        suppress_assignment = TRUE;
      }  /* if */
    } else {
      while (*fmt_string == '-' || *fmt_string == '+' || *fmt_string == ' ' ||
             *fmt_string == '#' || *fmt_string == '0') fmt_string++;
    }  /* if */
    /* An optional field width is next.  For printf, it can be a "*". */
    if (isdigit((unsigned char)*fmt_string)) {
      /* Decimal integer field width.  Skip over it. */
      do {} while (isdigit((unsigned char)*++fmt_string));
    } else if (!is_scanf && *fmt_string == '*') {
      /* "*" as field width.  The corresponding argument should be an 
         int.  Return that, and pick up next time after the field width. */
      required_type = integer_type((an_integer_kind)ik_int);
      fmt_string++;
      *init_value_pos = *value_pos;
      *value_pos = printf_scanf_arg_pos(&fmt_string);
      pss = pss_after_field_width;
      goto end_of_scan;
    }  /* if */
after_field_width:;
    /* For printf, an optional precision is next ("." followed by a
       decimal integer or "*"). */
    if (!is_scanf && *fmt_string == '.') {
      fmt_string++;
      if (isdigit((unsigned char)*fmt_string)) {
        /* Decimal integer precision.  Skip over it. */
        do {} while (isdigit((unsigned char)*++fmt_string));
      } else if (*fmt_string == '*') {
        /* "*" as precision.  The corresponding argument should be an 
           int.  Return that, and pick up next time after the precision. */
        required_type = integer_type((an_integer_kind)ik_int);
        fmt_string++;
        *init_value_pos = *value_pos;
        *value_pos = printf_scanf_arg_pos(&fmt_string);
        pss = pss_after_precision;
        goto end_of_scan;
      }  /* if */
    }  /* if */
after_precision:;
    /* The optional size character is next.  l indicates long integer
       (sometimes double), L long double, and h short integer.
       C99 adds hh for char, ll for long long, j for intmax_t,
       z for size_t, and t for ptrdiff_t. */
    l_size = L_size = h_size = FALSE;
    hh_size = j_size = z_size = t_size = FALSE;
#if LONG_LONG_ALLOWED
    ll_size = FALSE;
#endif /* LONG_LONG_ALLOWED */
    if (*fmt_string == 'l') {
#if LONG_LONG_ALLOWED
      if (fmt_string[1] == 'l') {
        /* "ll" for long long.  This is nonstandard. */
        ll_size = TRUE;
        fmt_string += 2;
      } else
#endif /* LONG_LONG_ALLOWED */
      {
        l_size = TRUE;
        fmt_string++;
      }
    } else if (*fmt_string == 'L') {
      L_size = TRUE;
      fmt_string++;
    } else if (*fmt_string == 'h') {
      if (fmt_string[1] == 'h') {
        hh_size = TRUE;
        fmt_string += 2;
      } else {
        h_size = TRUE;
        fmt_string++;
      }  /* if */
    } else if (*fmt_string == 'j') {
      j_size = TRUE;
      fmt_string++;
    } else if (*fmt_string == 'z') {
      z_size = TRUE;
      fmt_string++;
    } else if (*fmt_string == 't') {
      t_size = TRUE;
      fmt_string++;
    }  /* if */
    /* The next character indicates the conversion type, e.g., "d" for
       decimal.  Determine the required type.  For most (but not all)
       scanf cases, "pointer to" will be added afterwards. */
    *indirect = add_pointer = is_scanf;
#if FIXED_POINT_ALLOWED
    type_char = *fmt_string;
#endif /* FIXED_POINT_ALLOWED */
    switch (*fmt_string++) {
      case 'd':
      case 'i':
        /* int conversion.  If "l" was specified, long conversion;
           if "h" was specified, short conversion.
           C99 adds "hh" for signed char, "ll" for long long,
           "j" for intmax_t, "z" for the signed type corresponding to
           size_t, and "t" for ptrdiff_t. */
        if (l_size) {
          required_type = integer_type((an_integer_kind)ik_long);
        } else if (h_size) {
          required_type = integer_type((an_integer_kind)ik_short);
        } else if (hh_size) {
          required_type = integer_type((an_integer_kind)ik_signed_char);
#if LONG_LONG_ALLOWED
        } else if (ll_size) {
          required_type = integer_type((an_integer_kind)ik_long_long);
#endif /* LONG_LONG_ALLOWED */
        } else if (j_size) {
          required_type = integer_type(targ_intmax_kind);
        } else if (z_size) {
          /* Use the signed integral type that's the same size as size_t. */
          required_type = other_signedness_integer_type(targ_size_t_int_kind);
        } else if (t_size) {
          required_type = integer_type(targ_ptrdiff_t_int_kind);
        } else {
          required_type = integer_type((an_integer_kind)ik_int);
        }  /* if */
        break;
      case 'x':
      case 'X':
      case 'o':
        *weakly_typed = TRUE;
        /*FALLTHROUGH*/
      case 'u':
        /* Unsigned int conversion.  If "l" was specified, unsigned long
           conversion; if "h" was specified, unsigned short conversion.
           C99 adds "hh" for unsigned char, "ll" for unsigned long long,
           "j" for uintmax_t, "z" for size_t, and "t" for the unsigned
           type corresponding to ptrdiff_t. */
        if (l_size) {
          required_type = integer_type((an_integer_kind)ik_unsigned_long);
        } else if (h_size) {
          required_type = integer_type((an_integer_kind)ik_unsigned_short);
        } else if (hh_size) {
          required_type = integer_type((an_integer_kind)ik_unsigned_char);
#if LONG_LONG_ALLOWED
        } else if (ll_size) {
          required_type = integer_type((an_integer_kind)ik_unsigned_long_long);
#endif /* LONG_LONG_ALLOWED */
        } else if (j_size) {
          required_type = integer_type(targ_uintmax_kind);
        } else if (z_size) {
          required_type = integer_type(targ_size_t_int_kind);
        } else if (t_size) {
          /* Use the unsigned integral type that's the same size as
             ptrdiff_t. */
          required_type = other_signedness_integer_type(
                                                      targ_ptrdiff_t_int_kind);
        } else {
          required_type = integer_type((an_integer_kind)ik_unsigned_int);
        }  /* if */
        break;
#if FIXED_POINT_ALLOWED
      case 'r':
      case 'R':
        is_fract_type = TRUE;
        /*FALLTHROUGH*/
      case 'k':
      case 'K':
        /* Fixed-point conversions.  Note that the saturation behavior does
           not matter, so we use alt_type to also allow the nonsaturating
           type.  (Note: upper-case letters indicate unsigned fixed-point
           types.) */
        { a_fixed_point_precision
                     precision = l_size ? (a_fixed_point_precision)fpp_long :
                                 h_size ? (a_fixed_point_precision)fpp_short :
                                          (a_fixed_point_precision)fpp_default;
          a_boolean  is_unsigned = (type_char == 'R' || type_char == 'K');
          required_type = fixed_point_type(
                                make_fixed_point_type_descr(
                                          precision, is_unsigned,
                                          is_fract_type, /*saturating=*/TRUE));
          *alt_type = fixed_point_type(
                                make_fixed_point_type_descr(
                                       precision, is_unsigned, is_fract_type,
                                       /*saturating=*/FALSE));
        }
        break;
#endif /* FIXED_POINT_ALLOWED */
      case 'a':  /* Added in C99. */
      case 'A':  /* Added in C99. */
      case 'f':
      case 'F':  /* Added in C99. */
      case 'e':
      case 'E':
      case 'g':
      case 'G':
        /* Floating-point conversions.
           For printf: double usually, long double if "L" was specified.
           For scanf:  float usually, double if "l" was specified, long
                       double if "L" was specified. */
        if (L_size) {
          required_type = float_type((a_float_kind)fk_long_double);
        } else if (!is_scanf || l_size) {
          required_type = float_type((a_float_kind)fk_double);
        } else {
          required_type = float_type((a_float_kind)fk_float);
        }  /* if */
        break;
      case 'c':
        /* Character conversion. */
        if (l_size) {
          if (is_scanf) {
            required_type = eff_wchar_t_type();
          } else {
            required_type = integer_type(targ_wint_t_int_kind);
          }  /* if */
        } else {
          required_type = integer_type(plain_char_int_kind);
        }  /* if */
        break;
      case 's':
        /* String conversion.  "pointer to" will be added to make
           "char" into "char *", for both printf and scanf. */
        if (l_size) {
          required_type = eff_wchar_t_type();
        } else {
          required_type = integer_type(plain_char_int_kind);
        }  /* if */
        add_pointer = TRUE;
        *weak_pointer_to_integral = TRUE;
        /* *indirect is not set on purpose (it's not wanted for printf, and
           it's set by default for scanf). */
        break;
      case 'p':
        /* Pointer conversion.  Basic type is "void *". */
        required_type = make_pointer_type(void_type());
        *weakly_typed = TRUE;
        break;
      case 'n':
        /* Return number of characters read or written so far.
           Argument is "int *" for both printf and scanf, or "short *"
           if "h" was specified, "long *" if "l" was specified.
           C99 adds "hh" for signed char, "ll" for long long,
           "j" for intmax_t, "z" for the signed type corresponding
           to size_t, and "t" for ptrdiff_t. */
        if (l_size) {
          required_type = integer_type((an_integer_kind)ik_long);
        } else if (h_size) {
          required_type = integer_type((an_integer_kind)ik_short);
        } else if (hh_size) {
          required_type = integer_type((an_integer_kind)ik_signed_char);
#if LONG_LONG_ALLOWED
        } else if (ll_size) {
          required_type = integer_type((an_integer_kind)ik_long_long);
#endif /* LONG_LONG_ALLOWED */
        } else if (j_size) {
          required_type = integer_type(targ_intmax_kind);
        } else if (z_size) {
          /* Use the signed integral type that's the same size as size_t. */
          required_type = other_signedness_integer_type(targ_size_t_int_kind);
        } else if (t_size) {
          required_type = integer_type(targ_ptrdiff_t_int_kind);
        } else {
          required_type = integer_type((an_integer_kind)ik_int);
        }  /* if */
        *indirect = add_pointer = TRUE;
        break;
      case '[':
        /* For scanf only, a scanset.  Skip to the corresponding "]".
           Watch out for the special constructions "[]..." and "[^]...".  The
           input item is a pointer to character. */
        if (!is_scanf) goto default_case;
        if (*fmt_string == ']') {
          fmt_string++;
        } else if (*fmt_string == '^' && fmt_string[1] == ']') {
          fmt_string += 2;
        }  /* if */
        while (*fmt_string != ']' && *fmt_string != '\0') fmt_string++;
        if (l_size) {
          required_type = eff_wchar_t_type();
        } else {
          required_type = integer_type(plain_char_int_kind);
        }  /* if */
        break;
      default:
default_case:;
        /* Unknown formatting character.  Abandon checking and set fmt_string
           to NULL to notify the caller. */
        required_type = NULL;
        fmt_string = NULL;
        *value_pos = 0;
        goto end_of_scan;
    }  /* switch */
    /* Add a "pointer to" to the required type if necessary. */
    if (add_pointer) {
      required_type = make_pointer_type(required_type);
      if (*alt_type != NULL) *alt_type = make_pointer_type(*alt_type);
    }  /* if */
  }  /* if */
  /* Next time around, look for a new specifier. */
  pss = pss_new_specifier;
  /* If there was an assignment-suppressing character "*" in a scanf, go
     get the next specifier. */
  if (suppress_assignment) goto another_specifier;
  if (!is_scanf && required_type != NULL) {
    /* For printf, promote any types to the type that will actually be
       passed. */
    required_type = type_after_integral_promotion(required_type);
  }  /* if */
end_of_scan:;
  *fmt_string_ptr = fmt_string;
  *pss_ptr = pss;
  return required_type;
}  /* next_printf_scanf_arg_type */


static void check_printf_scanf_arg(an_operand  *argument_operand,
                                   a_type_ptr  required_type,
                                   a_type_ptr  alt_type,
                                   a_boolean   indirect,
                                   a_boolean   weakly_typed,
                                   a_boolean   weak_pointer_to_integral)
/*
Check an argument of a printf- or scanf-type function call to see if its
type matches the corresponding formatting specifier in the format string.
argument_operand points to the argument to check.  required_type is the
type expected (based on the formatting specifier) for the argument; in
some cases an alternative type alt_type may also be valid (otherwise,
alt_type is NULL).  indirect is TRUE if an extra level of indirection was
applied to the required type (so a value can be returned).  weakly_typed
is TRUE if the format specifier does not fully constrain the type (e.g.,
"%x", "%o", and "%p").  weak_pointer_to_integral is TRUE if the required
type is a pointer to an integral type that can match any pointer to
an integral type that differs only in signedness or cv-qualification
(this is used, for example, to allow a pointer to any variety of char
for the "%s" specifier).
*/
{
  a_type_ptr eff_required_type, eff_argument_type;

  /* Check that the argument type matches the specifier type.  Note
     the use of "interchangeable" rather than "compatible", because
     we want to allow things like "printf("%lx", (long)i);". */
  eff_required_type = required_type;
  eff_argument_type = argument_operand->type;
  if (indirect ||
      (weak_pointer_to_integral && is_pointer_type(eff_required_type))) {
    /* In cases where an extra indirection is added to the required
       type so that a value can be returned from the routine, remove
       the extra level of pointer type.  That allows matching things
       like "int *" and "unsigned int *".  This is slightly looser
       matching than is allowed without warning for normal function
       calls, but here we know what the runtime routine is doing. */
    if (!is_pointer_type(eff_argument_type)) goto mismatch;
    eff_argument_type = type_pointed_to(eff_argument_type);
    eff_required_type = type_pointed_to(eff_required_type);
    if (alt_type != NULL) alt_type = type_pointed_to(alt_type);
    /* Don't allow use of a pointer to const if we're going to store
       into it. */
    if (indirect &&
        is_const_qualified_type(eff_argument_type) &&
        !is_const_qualified_type(eff_required_type)) goto mismatch;
  }  /* if */
  /* Drop type qualifiers. */
  eff_argument_type = skip_typerefs(eff_argument_type);
  eff_required_type = skip_typerefs(eff_required_type);
  if (alt_type != NULL) alt_type = skip_typerefs(alt_type);
  if (types_are_compatible(eff_required_type, eff_argument_type)) {
    /* The types are exactly the same. */
  } else if (alt_type != NULL &&
             types_are_compatible(alt_type, eff_argument_type)) {
    /* The type matches the alternate acceptable type. */
  } else if (is_template_param_type(eff_argument_type) ||
             (is_pointer_type(eff_argument_type) &&
              is_template_param_type(type_pointed_to(eff_argument_type)) &&
              is_pointer_type(eff_required_type))) {
    /* A template parameter type could match anything. */
  } else if ((weakly_typed || weak_pointer_to_integral) &&
             is_integral_or_enum_type(eff_required_type) &&
             is_integral_or_enum_type(eff_argument_type) &&
             integral_types_the_same_except_for_signedness(
                                   eff_required_type, eff_argument_type)) {
    /* For a weakly-typed specifier like "%x", allow an integral type
       even if its signedness is different.  Ditto for the underlying type
       for "%s", to allow any version of (pointer-to-)char. */
  } else if (weakly_typed &&
             is_pointer_type(eff_required_type) &&
             is_pointer_type(eff_argument_type)) {
    /* Allow any pointer type for %p. */
  } else if (!strict_ansi_mode &&
             is_integral_or_enum_type(eff_required_type) &&
             is_pointer_type(eff_argument_type) &&
             eff_required_type->size == eff_argument_type->size &&
             eff_required_type->alignment == eff_argument_type->alignment){
    /* Allow a pointer to be passed where an integral type is expected
       as long as the integral type is the right size.   This accommodates
       lots of code that prints pointers using %lx. */
  } else if (interchangeable_types(eff_required_type, eff_argument_type)) {
    /* The types are not exactly the same, but they are interchangeable. */
    if (expr_diagnostic_should_be_issued(es_remark, ec_printf_arg_mismatch)) {
      pos_remark(ec_printf_arg_mismatch, &argument_operand->position);
    }  /* if */
  } else {
    /* The argument type does not match the required type. */
mismatch:
    if (!is_error_type(eff_argument_type)) {
      expr_pos_warning(ec_printf_arg_mismatch, &argument_operand->position);
    }  /* if */
  }  /* if */
}  /* check_printf_scanf_arg */


void start_call_argument_processing(a_type_ptr         function_type,
                                    a_routine_ptr      routine,
                                    an_arg_check_block *arg_block)
/*
Initialize for the process of checking a sequence of argument expressions
against the corresponding function parameters.  *arg_block is a status
block, which is initialized to appropriate values.  function_type is the
type of the function being called, or NULL if the type is not known (e.g.,
for an overloaded function).  routine is the routine being called, if known,
or NULL otherwise (e.g., for a call through a pointer to function).
*/
{
  /* Initialize the control block. */
  arg_block->routine = routine;
  arg_block->unknown_dependent_function = FALSE;
  arg_block->have_param_info = FALSE;
  arg_block->curr_param_type = NULL;
  arg_block->prototyped = FALSE;
  arg_block->has_ellipsis = FALSE;
  arg_block->arg_list_kind = (a_pragma_kind)pk_none;
  arg_block->varargs_count = NOT_LINT_VARARGS;
  arg_block->arg_ctr = 0;
  arg_block->argument_head = NULL;
  arg_block->argument_tail = NULL;
#if GNU_EXTENSIONS_ALLOWED
  arg_block->fmt_arg = 0;
  arg_block->sentinel_pos = 0;
#endif /* GNU_EXTENSIONS_ALLOWED */
  arg_block->printf_scanf_args = NULL;
  arg_block->fmt_string = NULL;
  arg_block->closing_paren_position = null_source_position;
  if (function_type != NULL) {
    /* The function type is known, so set the block to match it. */
    a_routine_type_supplement_ptr extra_info;

    /* Get information on the parameters of the function. */
    function_type = skip_typerefs(function_type);
#if CHECKING
    if (function_type->kind != (a_type_kind)tk_routine) {
      internal_error("start_call_argument_processing: bad function type");
    }  /* if */
#endif /* CHECKING */
    extra_info = function_type->variant.routine.extra_info;
    arg_block->curr_param_type = extra_info->param_type_list;
    arg_block->prototyped = extra_info->prototyped;
    arg_block->has_ellipsis = extra_info->has_ellipsis;
    arg_block->have_param_info =
                  (arg_block->prototyped || extra_info->assoc_routine != NULL);
    arg_block->arg_list_kind = extra_info->arg_pragma;
    arg_block->varargs_count = extra_info->lint_varargs_count;
#if GNU_EXTENSIONS_ALLOWED
    arg_block->fmt_arg = extra_info->fmt_arg;
    if (extra_info->this_class != NULL) {
      /* For nonstatic member functions, the "this" parameter is number one.
         Since the corresponding argument is not counted, we must adjust
         the numbering of the format argument here. */
      --arg_block->fmt_arg;
    }  /* if */
    arg_block->sentinel_pos = extra_info->sentinel_pos;
#endif /* GNU_EXTENSIONS_ALLOWED */
  }  /* if */
}  /* start_call_argument_processing */


static an_error_severity arg_okay_for_old_style_param(an_operand  *operand,
                                                      a_type_ptr  formal_type)
/*
Returns a severity that indicates what kind of diagnostic (if any)
should be issued about passing the indicated operand to an old-style
parameter with the indicated type.
*/
{
  an_error_severity severity = (an_error_severity)es_none;

  if (!types_are_compatible(formal_type, operand->type)) {
    if (interchangeable_types(formal_type, operand->type)) {
      /* Types are interchangeable but not compatible (e.g.,
         unsigned int vs. int). */
      severity = (an_error_severity)es_remark;
#if TARG_NULL_IS_ALL_BITS_ZERO
    } else if (!strict_ansi_mode &&
               is_pointer_type(formal_type) &&
               is_integral_or_enum_type(operand->type) &&
               op_is_zero_constant(operand) &&
               skip_typerefs(formal_type)->size ==
                                        skip_typerefs(operand->type)->size) {
      /* An uncast zero can be passed for a pointer parameter if
         the architecture uses all zero bits for a NULL pointer. */
      severity = (an_error_severity)es_remark;
#endif /* TARG_NULL_IS_ALL_BITS_ZERO */
    } else {
      /* Types are outright incompatible. */
      severity = (an_error_severity)es_warning;
    } /* if */
  }  /* if */
  return severity;
}  /* arg_okay_for_old_style_param */


static void obtain_format_string_from_arg(an_expr_node_ptr   node,
                                          an_arg_check_block *arg_block)
/*
The "node" is being used as a printf or scanf format string.  If the
format string can be deduced, set appropriate fields in arg_block.
*/
{
  a_constant_ptr con_ptr;
#if GNU_EXTENSIONS_ALLOWED
  a_routine_ptr  rout;
#endif /* GNU_EXTENSIONS_ALLOWED */

  node = skip_parens(node);
  /* Skip any cast operations (e.g., from char* to char const*). */
  while (node->kind == (an_expr_node_kind)enk_operation &&
         node->variant.operation.kind == (an_expr_operator_kind)eok_cast) {
    node = skip_parens(node->variant.operation.operands);
  }  /* while */
#if GNU_EXTENSIONS_ALLOWED
  /* Check to see if this argument is a call to a routine with the
     "format_arg" attribute. */
  if (is_operation_node(node) &&
      (node_operator_is(node, eok_call) ||
       node_operator_is(node, eok_dot_member_call) ||
       node_operator_is(node, eok_points_to_member_call)) &&
      (rout = routine_from_function_expr(node->variant.operation.operands))
                                                                     != NULL) {
    a_routine_type_supplement_ptr rtsp;
    int                           arg_ctr;
    rtsp = skip_typerefs(rout->type)->variant.routine.extra_info;
    if (rtsp->arg_pragma != (a_pragma_kind)pk_printf_args &&
        rtsp->arg_pragma != (a_pragma_kind)pk_scanf_args &&
        rtsp->fmt_arg != 0) {
      /* The call is to a function with the "format_arg" attribute.
         Recurse on the appropriate argument. */
      node = node->variant.operation.operands->next;
      for (arg_ctr = 1; arg_ctr < rtsp->fmt_arg && node != NULL; 
           arg_ctr++) {
        node = node->next;
      }  /* for */
      if (node != NULL) {
        obtain_format_string_from_arg(node, arg_block);
      }  /* if */
    }  /* if */
  } else
#endif /* GNU_EXTENSIONS_ALLOWED */
  /* Do not insert code here. */
  /* See if the format string is a narrow string literal. */
  if (expr_is_pointer_to_string_literal(node, &con_ptr) &&
      is_normal_character_kind(con_ptr->character_kind)) {
    /* Check that the string is null-terminated. */
    arg_block->fmt_string = con_ptr->variant.string.value;
    if (arg_block->fmt_string[con_ptr->variant.string.length-1] != '\0') {
      /* String is not null-terminated. */
      arg_block->fmt_string = NULL;
    }  /* if */
  }  /* if */
}  /* obtain_format_string_from_arg */


static void process_call_argument(an_arg_operand_ptr arg_operand,
                                  an_arg_check_block *arg_block)
/*
Check the argument expression indicated by arg_operand against the
corresponding parameter.  If it is compatible, convert it if necessary;
otherwise, issue an error.  *arg_block contains information about the
current parameter, and is updated at the end of the call to describe the
next parameter.
*/
{
  a_boolean         do_default_promotion;
  a_boolean         arg_is_fmt_string = FALSE;
  an_operand        *operand = &arg_operand->operand;
  a_param_type_ptr  ptp = arg_block->curr_param_type;

  /* Count the arguments. */
  arg_block->arg_ctr++;
  /* Check for too many arguments and determine whether or not the default
     argument promotions apply to this argument. */
  do_default_promotion = TRUE;
  if (arg_block->unknown_dependent_function) {
    /* The routine to be called is not known because it's
       template-dependent. */
    do_default_promotion = FALSE;
  } else if (!arg_block->have_param_info) {
    /* We have no information on parameter types. */
  } else if (arg_block->prototyped) {
    /* Prototyped parameter list. */
    if (ptp != NULL) {
      do_default_promotion = FALSE;
    } else {
      /* No more formal arguments in the list. */
      if (!arg_block->has_ellipsis) {
        /* No ellipsis, so error: extra actual argument. */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (microsoft_mode && C_mode()) {
          /* MSVC++ 4.2 allows extra arguments with just a warning in
             C mode. */
          expr_pos_warning(ec_too_many_arguments, &operand->position);
        } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        {
          expr_pos_error(ec_too_many_arguments, &operand->position);
        }  /* if */
      }  /* if */
      arg_block->have_param_info = FALSE;
    }  /* if */
  } else {
    /* Old-style parameter list, for a function with a body (i.e., we know
       the argument types). */
    if (ptp == NULL) {
      /* No more formal arguments in the list. */
      if (arg_block->varargs_count == NOT_LINT_VARARGS) {
        /* A lint-style varargs comment does not apply, so warning:
           extra actual argument. */
        expr_pos_warning(ec_too_many_arguments, &operand->position);
        arg_block->have_param_info = FALSE;
      }  /* if */
    }  /* if */
  }  /* if */
  /* Do the argument conversion or promotion. */
  if (do_default_promotion) {
    /* Either an ellipsis was encountered or this is an old-style argument
       list; do the default argument promotion. */
    arg_default_promote_operand(operand, arg_block->has_ellipsis);
    /* If this is an old-style call and we have the list of types as
       defined by the function body, check the promoted type of the
       actual against the formal. */
    if (arg_block->have_param_info && !arg_block->prototyped && ptp != NULL) {
      /* Compare the type of the promoted actual with the promoted formal
         without qualifiers. */
      a_type_ptr  ptype = skip_typerefs(ptp->type);
      if (!is_error_type(ptype)) {
        a_type_ptr        formal_type = default_argument_promotion(ptype);
        an_error_severity severity;
        severity = arg_okay_for_old_style_param(operand, formal_type);
#if GNU_EXTENSIONS_ALLOWED
        if (severity == (an_error_severity)es_warning &&
            (ptp->is_transparent ||
             (is_union_type(ptype) &&
              (ptype->variant.class_struct_union.is_transparent)))) {
          /* This argument might be okay if its type matches one of
             the field types in the transparent union. */
          a_field_ptr       f;
          an_error_severity new_severity;
          for (f = ptype->variant.class_struct_union.field_list;
               f != NULL;
               f = f->next) {
            new_severity = arg_okay_for_old_style_param(operand, f->type);
            if ((int)new_severity < (int)severity) {
              severity = new_severity;
              if (severity == (an_error_severity)es_none) {
                break;
              }  /* if */
            }  /* if */
          }  /* for */
        }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
        if (severity != (an_error_severity)es_none) {
          expr_pos_diagnostic(severity, ec_old_style_incompatible_param,
                              &operand->position);
        }  /* if */
      }  /* if */
    }  /* if */
  } else if (arg_block->unknown_dependent_function) {
    /* Argument of unknown template-dependent function. */
    prep_generic_operand(operand);
  } else {
    /* Parameter is prototyped. */
    /* Check the argument for compatibility against the parameter,
       casting it if necessary.  Also convert from lvalue to rvalue
       when appropriate. */
    prep_argument_operand(operand, ptp,
                          (a_conv_descr_ptr)NULL, ec_incompatible_param);
#if GNU_EXTENSIONS_ALLOWED
    if (ptp->nonnull && op_is_null_pointer_value(operand)) {
      /* The parameter carries the GNU "nonnull" attribute and a null pointer
         is passed through it. */
      expr_pos_warning(ec_null_argument_for_nonnull_parameter,
                       &operand->position);
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  }  /* if */
  if (ptp != NULL) {
    /* Advance to the next parameter type entry in preparation for the
       next call of this routine. */
    arg_block->curr_param_type = ptp->next;
  }  /* if */
  /* If this is a call to a function with a printf- or scanf-style
     argument list and the ellipsis is next, the current argument is
     the format string.  See if it is constant; if so, we will be
     able to check the rest of the arguments against the format string
     as we scan them. */
  if (arg_block->arg_list_kind == (a_pragma_kind)pk_printf_args ||
      arg_block->arg_list_kind == (a_pragma_kind)pk_scanf_args) {
    a_boolean  ellipsis_next = (arg_block->have_param_info && 
                                arg_block->curr_param_type == NULL);
    if (ellipsis_next) {
      arg_block->printf_scanf_args = arg_operand->next;
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED
    if (arg_block->fmt_arg != 0) {
      arg_is_fmt_string = (arg_block->arg_ctr == arg_block->fmt_arg);
    } else
#endif /* GNU_EXTENSIONS_ALLOWED */
    /* Do not insert code here. */
    {
      if (ellipsis_next) {
        arg_is_fmt_string = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (arg_is_fmt_string) {
    obtain_format_string_from_arg(make_node_from_operand(operand), arg_block);
  }  /* if */
}  /* process_call_argument */


static an_arg_operand_ptr nth_printf_scanf_arg(int                 n,
                                               an_arg_check_block  *arg_block)
/*
Return the n-th ellipsis argument in a printf/scanf-like argument list
described by *arg_block (or NULL if there is no n-th ellipsis argument).
*/
{
  an_arg_operand_ptr  arg_operand = arg_block->printf_scanf_args;
  int                 k;

  for (k = 1; k < n && arg_operand != NULL; ++k) {
    arg_operand = arg_operand->next;
  }  /* for */
  return arg_operand;
}  /* nth_printf_scanf_arg */


static void check_printf_scanf_arg_list(an_arg_check_block  *arg_block)
/*
We are processing a call to a function with a constant printf/scanf-like format
string.  Check that the argument list is consistent with the contents of that
format string.  arg_block contains some information about the call arguments
(including a pointer to the format string and a pointer to the ellipsis
arguments).
*/
{
  char                 *fmt_string = arg_block->fmt_string;
  an_arg_operand_ptr   arg = arg_block->printf_scanf_args;
  a_type_ptr           type = NULL, alt_type = NULL;
  a_boolean            indirect, weakly_typed, weak_pointer_to_integral;
  a_printf_scan_state  pss = pss_new_specifier;
  a_boolean            is_scanf = (arg_block->arg_list_kind ==
                                                 (a_pragma_kind)pk_scanf_args);
  int                  value_pos = 0, init_value_pos = 0;
  a_boolean            explicit_position_seen = FALSE;

  while (fmt_string != NULL) {
    /* Determine the type specified by the format string for the next
       argument. */
    type = next_printf_scanf_arg_type(is_scanf, &fmt_string, &pss,
                                      &indirect, &weakly_typed,
                                      &weak_pointer_to_integral, &alt_type,
                                      &value_pos, &init_value_pos);
    if (value_pos != 0) {
      /* The format specifier contained a positional field indicating which
         argument it formats. */
      explicit_position_seen = TRUE;
      if (value_pos == -2) {
        /* The format specifier was zero (e.g. "%00$s"), which is invalid. */
        expr_pos_warning(ec_positional_format_specifier_zero,
                         &arg_block->closing_paren_position);
        break;
      } else if (value_pos == -1) {
        /* The position was larger than what we are willing to check.
           If there are at least the limit's worth of printf/scanf-like
           arguments, silently give up on checking.  Otherwise, issue a
           warning. */
        if (nth_printf_scanf_arg(CHECKED_PRINTF_SCANF_ARG_POS_LIMIT,
                                 arg_block) != NULL) {
          break;
        } else {
          /* Setting arg NULL here will trigger a warning later on. */
          arg = NULL;
        }  /* if */
      } else {
        arg = nth_printf_scanf_arg(value_pos, arg_block);
      }  /* if */
    }  /* if */
    if (arg == NULL) {
      /* There is no argument at the given position: Issue a warning if one
         was expected and stop further checking. */
      if (type != NULL) {
        expr_pos_warning(ec_too_few_printf_args,
                         &arg_block->closing_paren_position);
      }  /* if */
      break;
    } else if (fmt_string == NULL) {
      /* An error occurred while scanning the specifier.  Issue a warning and
         stop the checking process here. */
      expr_pos_warning(ec_bad_printf_format_string, &arg->operand.position);
      break;
    } else if (type == NULL) {
      /* There were no more formatting specifiers.  If explicit position
         fields were seen, arg can validly be non-NULL. */
      if (!explicit_position_seen) {
        expr_pos_warning(ec_too_many_printf_args, &arg->operand.position);
      }  /* if */
      break;
    }  /* if */
    check_printf_scanf_arg(&arg->operand, type, alt_type,
                           indirect, weakly_typed, weak_pointer_to_integral);
    arg = arg->next;
  }  /* while */
}  /* check_printf_scanf_arg_list */

#if GNU_EXTENSIONS_ALLOWED

static void warn_if_missing_sentinel(an_arg_operand_ptr  arg_operand_list,
                                     an_arg_check_block  *arg_block)
/*
arg_block->sentinel_pos is nonzero.  Check that the corresponding argument
is a constant null pointer.
*/
{
  if (arg_block->arg_ctr < arg_block->sentinel_pos) {
    expr_pos_warning(ec_no_gnu_sentinel_argument,
                     &arg_block->closing_paren_position);
  } else if (arg_block->routine != NULL) {
    an_operand        *sentinel;
    int               k = arg_block->arg_ctr - arg_block->sentinel_pos;
    a_param_type_ptr  param = skip_typerefs(arg_block->routine->type)
                                ->variant.routine.extra_info->param_type_list;
    a_boolean         valid_sentinel_value;
    /* Skip to the operand that should be the sentinel. */
    while (k--) {
      arg_operand_list = arg_operand_list->next;
      if (param != NULL) param = param->next;
    }  /* while */
    sentinel = &arg_operand_list->operand;
    valid_sentinel_value = op_is_null_pointer_value(sentinel);
    /* Check that the operand is a valid sentinel. */
    if (gnu_mode && gnu_version >= 40002 && param != NULL) {
      /* gcc/g++ version 4.0.0 allowed a sentinel to correspond to a named
         parameter, but version 4.0.2 diagnosed such cases.  For example:
           int f(int *, ...)  __attribute__((sentinel));
           int x = f(NULL);  // Okay with g++ 4.0.0; warning with g++ 4.0.2.
         Different diagnostics are issued depending on whether the argument in
         the sentinel position is otherwise valid (constant null pointer) or
         not. */
      if (valid_sentinel_value) {
        expr_pos_warning(ec_gnu_sentinel_must_be_ellipsis_argument,
                         &sentinel->position);
      } else {
        expr_pos_warning(ec_no_gnu_sentinel_argument,
                         &arg_block->closing_paren_position);
      }  /* if */
    } else if (!valid_sentinel_value) {
      expr_pos_warning(ec_invalid_gnu_sentinel_argument, &sentinel->position);
    }  /* if */
  }  /* if */
}  /* warn_if_missing_sentinel */

#endif /* GNU_EXTENSIONS_ALLOWED */

static void process_end_of_call_arguments(an_arg_check_block *arg_block)
/*
A sequence of argument expressions has been processed through
process_call_argument, and *arg_block has been set accordingly.
We have now run out of argument expressions.  Do end-of-argument
list checking (e.g., for the presence of too few arguments).
*/
{
  /* Check for additional parameters not accounted for in the call. */
  if (!arg_block->have_param_info) {
    /* We don't have parameter information (anymore?), so we can't check. */
  } else if (arg_block->prototyped) {
    /* Prototyped parameter list. */
    if (arg_block->curr_param_type != NULL) {
      /* Not enough arguments? */
      /* If there is a default argument value, or several, use them. */
      if (arg_block->curr_param_type->default_arg_expr != NULL ||
          arg_block->curr_param_type->has_unevaluated_template_default) {
        an_expr_node_ptr curr_node =
                   expr_copy_default_arg_expr_list(arg_block->routine,
                                                   arg_block->curr_param_type);
        if (arg_block->argument_head == NULL) {
          arg_block->argument_head = curr_node;
        } else {
          arg_block->argument_tail->next = curr_node;
        }  /* if */
        arg_block->argument_tail = curr_node;
      } else {
        /* No default arguments. */
        /* Error: too few actual arguments. */
        expr_pos_error(ec_too_few_arguments,
                       &arg_block->closing_paren_position);
      }  /* if */
      /* Suppress the end-of-printf check below. */
      arg_block->fmt_string = NULL;
    }  /* if */
  } else {
    /* Old-style parameter list. */
    if ((arg_block->varargs_count == NOT_LINT_VARARGS &&
         arg_block->curr_param_type != NULL) ||
        arg_block->arg_ctr < arg_block->varargs_count) {
      /* Warning: too few actual arguments. */
      expr_pos_warning(ec_too_few_arguments,
                       &arg_block->closing_paren_position);
    }  /* if */
  }  /* if */
}  /* process_end_of_call_arguments */


void process_call_argument_list(an_arg_operand_ptr  arg_operand_list,
                                an_arg_check_block  *arg_block)
/*
Apply various transformations and checks to the given list of operands, which
is a list of arguments for a function call.  The list is transformed into a
list of expression nodes pointed to by arg_block->argument_head (and the
operand list is deallocated).  Some state information is recorded in *arg_block
(which must have been initialized by start_call_argument_processing).
*/
{
  an_arg_operand_ptr  arg_operand;

  for (arg_operand = arg_operand_list;
       arg_operand != NULL;
       arg_operand = arg_operand->next) {
    process_call_argument(arg_operand, arg_block);
  }  /* for */
  if (arg_block->fmt_string != NULL) {
    /* Check printf/scanf-like argument lists. */
    check_printf_scanf_arg_list(arg_block);
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (arg_block->sentinel_pos != 0) {
    warn_if_missing_sentinel(arg_operand_list, arg_block);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  /* Convert the operand list to an expression list. */
  for (arg_operand = arg_operand_list;
       arg_operand != NULL;
       arg_operand = arg_operand->next) {
    an_expr_node_ptr arg_expr;
    mark_expr_of_operand_as_pack_expansion_if_necessary(&arg_operand->operand);
    arg_expr = make_node_from_operand(&arg_operand->operand);
    if (arg_block->argument_head == NULL) {
      arg_block->argument_head = arg_expr;
    } else {
      arg_block->argument_tail->next = arg_expr;
    }  /* if */
    arg_block->argument_tail = arg_expr;
  }  /* for */
  /* Do processing for the end of the argument list. */
  process_end_of_call_arguments(arg_block);
  /* Free the argument list. */
  free_arg_operand_list(arg_operand_list);
}  /* process_call_argument_list */


static void prep_possible_ellipsis_argument_operand(
                                            an_operand       *operand,
                                            a_param_type_ptr param,
                                            a_conv_descr     *conversion)
/*
operand is the actual argument value for the parameter described by param.
Adjust it for use in the call.  Specifically, cast it to the proper type
(it's known to be valid as the argument).  *conversion indicates
the conversions needed (if any), or conversion may be NULL to
indicate no user-defined conversion.  Even if there is no user-defined
conversion, a cast may be required, and reference initialization must
be considered.  param may be NULL to indicate that the argument falls
under an ellipsis or old-style function.  Note that the argument/parameter
match has already made it through overload resolution.
*/
{
  if (param == NULL) {
    /* The actual argument was accepted under an ellipsis.  Do default
       argument promotions. */
    arg_default_promote_operand(operand, /*is_ellipsis=*/TRUE);
  } else {
    /* Cast the argument to the right type. */
    prep_argument_operand(operand, param,
                          conversion, ec_incompatible_param);
  }  /* if */
}  /* prep_possible_ellipsis_argument_operand */


static an_expr_node_ptr node_for_arg_of_overloaded_function_call(
                                      an_arg_operand_ptr       arg_operand,
                                      an_arg_match_summary_ptr arg_match,
                                      a_param_type_ptr         param,
                                      a_routine_ptr	       rout_ptr)
/*
arg_operand represents an argument to an overloaded function call (including
operator cases); the call has now been resolved to a specific function.
arg_match indicates how well the actual argument matches the formal parameter,
which is described by param.  Cast the argument value to the proper type,
convert it to expression form, and return a pointer to the expression.
arg_operand can be NULL to indicate that we've run out of actual
arguments (default argument values will be used).  param can be NULL
to indicate that we've run out of parameters (remaining arguments will
be processed under an ellipsis).  rout_ptr is the routine pointer for the
specific function being called.
*/
{
  an_expr_node_ptr arg = NULL;

  if (arg_operand == NULL) {
    /* Match uses a default argument value.  Get it from the parameter type
       entry. */
#if CHECKING
    if (param == NULL) {
      internal_error(
    "node_for_arg_of_overloaded_function_call: missing param for default arg");
    }  /* if */
#endif /* CHECKING */
    if (param->default_arg_expr != NULL ||
        param->has_unevaluated_template_default) {
      /* The parameter has a default value, or one can be generated for a
         template-based function. */
      arg = copy_default_arg_expr(
                          rout_ptr, param,
                          (a_boolean)expr_stack->inside_conditional_expression,
                          curr_expr_is_potentially_evaluated());
    }  /* if */
  } else {
    /* Actual argument is present (normal case). */
    /* Issue any warning about the conversion detected while evaluating the
       alternatives. */
    issue_warning_from_arg_match_summary(arg_match,
                                         &arg_operand->operand.position);
    if (arg_match->match_level == aml_error) {
      /* The argument match indicates the argument or the parameter had
         an error type.  Do not go through the normal casting, because
         for template functions it is possible the instantiation ended
         up with a different parameter type. */
      arg = error_node();
    } else {
      /* Cast the argument to the right type. */
      prep_possible_ellipsis_argument_operand(&arg_operand->operand, param,
                                              &arg_match->conversion);
      mark_expr_of_operand_as_pack_expansion_if_necessary(
                                                        &arg_operand->operand);
      arg = make_node_from_operand(&arg_operand->operand);
    }  /* if */
  }  /* if */
  return arg;
}  /* node_for_arg_of_overloaded_function_call */


void change_refs_on_selector(a_type_ptr routine_type,
                             an_operand *bound_function_selector)
/*
A function with the indicated routine type is being called with the indicated
operand as its selector object.  Change the type of references to the
selector appropriately.
*/
{
  /* The selector's address is implicitly taken if it is a class lvalue.
     (We don't mark a class rvalue as having its address taken, and
     if the selector is already a pointer its address has already been
     taken in a way that doesn't allow discrimination of const use.) */
  if (is_an_lvalue(bound_function_selector)) {
    a_symbol_reference_kind ref_kinds = SRK_ADDRESS_TAKEN;
    a_routine_type_supplement_ptr
                            rtsp = routine_type->variant.routine.extra_info;
    if (rtsp->qualifiers & TQ_CONST) {
      /* The function is a const function, so indicate that the selector's
         address is taken only in a way that does not allow modification. */
      ref_kinds |= SRK_CONST_ADDRESS_TAKEN;
    }  /* if */
    change_some_ref_kinds(bound_function_selector->ref_entries_list,
                          SRK_REFERENCE, ref_kinds);
  }  /* if */
}  /* change_refs_on_selector */


void adjust_overloaded_function_call_arguments(
                           a_symbol_ptr             function_symbol,
                           a_boolean                unknown_dependent_function,
                           a_type_ptr               routine_type,
                           a_boolean                have_selector,
                           an_operand               *bound_function_selector,
                           an_arg_operand_ptr       arg_operand_list,
                           an_arg_match_summary_ptr arg_match_list,
                           an_expr_node_ptr         *arg_expr_list)
/*
Overload resolution has been done, and it has been decided that the
function identified by function_symbol is the specific function to be
called for the argument list given by arg_operand_list.
function_symbol is NULL for an error, or for a surrogate function call
(in that case, routine_type gives the surrogate function type).  If
unknown_dependent_function is TRUE, the function is unknown because
one or more of the arguments has a template-dependent type
(function_symbol and routine_type will be NULL in that case).  If
have_selector is TRUE, there is also a selector object, given by
bound_function_selector (or, as a special case,
bound_function_selector can be NULL for a constructor case; we have a
selector, but we don't know what it is).  Adjust the selector object
and arguments to the proper types, issue any warnings detected on
those arguments during the overload resolution process, and return a
list of argument expressions in *arg_expr_list.  arg_match_list gives
the argument match summaries for the selector object and the
arguments.  arg_operand_list and arg_match_list are freed.
This routine is used for cases that look like calls (i.e., they have
argument lists in parentheses) or casts; it is not used for
overloaded operator cases.
*/
{
  an_arg_match_summary_ptr arg_match;
  an_arg_operand_ptr       arg_operand;
  an_expr_node_ptr         arg, prev_arg;
  a_param_type_ptr         param;
  a_routine_ptr            routine = NULL;

  if (function_symbol != NULL) {
    function_symbol = fundamental_symbol_of(function_symbol);
    routine = function_symbol->variant.routine.ptr;
    routine_type = skip_typerefs(routine->type);
  }  /* if */
  /* If there was an error, skip the processing except for freeing the
     lists. */
  if (routine_type != NULL) {
    arg_match = arg_match_list;
    if (have_selector) {
      if (arg_match == NULL || !arg_match->is_match_for_this_param) {
        /* An implied selector was added after overload resolution. */
      } else if (bound_function_selector != NULL) {
        /* Issue any warning about the "this" parameter detected while
           evaluating the alternatives. */
        issue_warning_from_arg_match_summary(arg_match,
                                           &bound_function_selector->position);
        /* Note that no cast is done here.  It was done when the "." or "->"
           operator was processed (that still may leave a difference here
           involving type qualifiers, but it's not meaningful). */
        change_refs_on_selector(routine_type, bound_function_selector);
      }  /* if */
    }  /* if */
    if (arg_match != NULL && arg_match->is_match_for_this_param) {
      /* Move past the match entry for the selector.  Note that this entry
         might be present even if this function doesn't need it. */
      arg_match = arg_match->next;
    }  /* if */
    prev_arg = NULL;
    /* Scan through the argument list. */
    for (arg_operand = arg_operand_list,
             param = routine_type->variant.routine.extra_info->param_type_list;
         arg_operand != NULL || param != NULL;) {
      check_assertion(arg_match != NULL ||
                      arg_operand == NULL);  /* For Coverity */
      arg = node_for_arg_of_overloaded_function_call(
                                         arg_operand, arg_match, param,
                                         routine);
      /* Add this argument to the end of the expression-form argument list
         being built up. */
      if (prev_arg == NULL) {
        *arg_expr_list = arg;
      } else {
        prev_arg->next = arg;
      }  /* if */
      prev_arg = arg;
      /* Advance to the next argument unless we've run out (additional
         arguments will come from default argument values). */
      if (arg_operand != NULL) {
        arg_operand = arg_operand->next;
        arg_match = arg_match->next;
      }  /* if */
      /* Advance to the next parameter unless we've run out (additional
         arguments will be processed under an ellipsis). */
      if (param != NULL) param = param->next;
    }  /* for */
  } else if (unknown_dependent_function) {
    /* The called function is unknown because some of the arguments
       are template dependent.  Make an argument list. */
    *arg_expr_list = prep_generic_argument_list(arg_operand_list);
  } else {
    /* There was an error.  Change the references on the operand lists to
       errors. */
    change_arg_operand_list_refs_to_error(arg_operand_list);
  }  /* if */
  /* Free the argument match list. */
  free_arg_match_summary_list(arg_match_list);
  /* Free the argument list. */
  free_arg_operand_list(arg_operand_list);
}  /* adjust_overloaded_function_call_arguments */


a_type_ptr select_and_prepare_to_call_overloaded_function(
                           a_symbol_ptr            overloaded_function_symbol,
                           a_boolean               is_template_id,
                           a_template_arg_ptr      template_arg_list,
                           a_boolean               have_selector,
                           an_operand              *bound_function_selector,
                           an_arg_operand_ptr      arg_operand_list,
                           a_boolean               do_arg_dep_lookup,
                           a_boolean               try_surrogate_functions,
                           a_boolean               is_property,
                           an_error_code           err_none_applies,
                           an_error_code           err_ambiguous,
                           an_operand              *orig_function_operand,
                           a_source_position       *call_position,
                           a_token_sequence_number paren_tok_seq_number,
                           a_source_position       *closing_paren_position,
                           a_boolean               *unknown_dependent_function,
                           a_boolean               *found_through_adl,
                           an_operand              *function_operand,
                           an_expr_node_ptr        *arg_expr_list)
/*
Determine which of the functions under overloaded_function_symbol
should be called given an argument list arg_operand_list.  The symbol
may be an overloaded function, a simple member or nonmember function,
or a projection symbol for one of those.  is_template_id is TRUE if
the symbol has an associated explicit template argument list; if so,
template_arg_list gives the argument list.  If have_selector is TRUE,
*bound_function_selector is a selector object.  Note that, for
constructor calls, bound_function_selector can be NULL when
have_selector is TRUE; we have a selector, but it's not available.
That's okay for constructors, because they cannot be const- or
volatile-qualified, and the selector expression is only needed for
that discrimination.  If have_selector is FALSE,
bound_function_selector must still point at an operand that can be
filled in if an implicit selector is generated.
bound_function_selector->selector_is_object_pointer is TRUE if the
selector is an object pointer, FALSE if it is an object.
orig_function_operand, if non-NULL, gives the operand that has been
used to hold the overloaded function name reference so far, which is
used to provide some additional information (e.g., source positions,
whether the name is qualified).  orig_function_operand must not be the
same operand as function_operand.  If orig_function_operand is NULL
(e.g., for a property reference), call_position provides a position
for the call.  If both orig_function_operand and call_position are
supplied, call_position is assumed to be a better position for the
call.  If an error of some sort is detected, issue an error at
the indicated position and return NULL.  err_none_applies is the error
code to use when no function applies, and err_ambiguous is the error
code to use when more than one function applies.  If there is no
error, an operand for the function is built in *function_operand, an
expression-form argument list is built and returned in *arg_expr_list
(with the arguments cast to the proper types), and the type of the
routine selected is returned.  do_arg_dep_lookup is TRUE if
argument-dependent lookup should be done; if it is TRUE,
overloaded_function_symbol may be an sk_undefined symbol, indicating
that nothing was found on a normal id lookup of the function name.
try_surrogate_functions is TRUE if surrogate functions should be
tried; that means looking for conversion functions from the selector
object to pointers to function type.  overloaded_function_symbol can
be NULL in that case.  is_property is TRUE if this call results from
the expansion of a Microsoft property reference.  paren_tok_seq_number
is the token sequence number of the opening parenthesis of the
argument list, but it's required only when do_arg_dep_lookup is TRUE;
it can be zero otherwise.  closing_paren_position is the position of
the closing parenthesis in the call; it is used only when
do_arg_dep_lookup is TRUE.  If the call is dependent, and the function
to be called cannot be determined, return *unknown_dependent_function
set to TRUE (unknown_dependent_function can be NULL if the call cannot
be dependent).  If found_through_adl is non-NULL and the callee was
found only through ADL, *found_through_adl is returned TRUE.  This
routine is called only in C++ mode.  arg_operand_list is freed by this
routine.
*/
{
  an_arg_match_summary_ptr arg_match_list;
  a_symbol_ptr             function_symbol, base_function_symbol;
  a_boolean                single_function;
  a_routine_ptr            routine = NULL;
  a_type_ptr               routine_type = NULL;
  a_symbol_ptr             surrogate_function_conv_sym = NULL;

  db_enter(4, "select_and_prepare_to_call_overloaded_function");
  check_assertion((orig_function_operand != NULL) ?
                                  (orig_function_operand != function_operand) :
                                  (call_position != NULL));
  if (call_position == NULL) {
    call_position = &orig_function_operand->position;
  }  /* if */
  /* Select the best function out of the overload set. */
  function_symbol = select_overloaded_function(overloaded_function_symbol,
                                               is_template_id,
                                               template_arg_list,
                                               have_selector,
                                               bound_function_selector,
                                               arg_operand_list,
                                               do_arg_dep_lookup,
                                               /*force_dependent=*/FALSE,
                                               err_none_applies,
                                               err_ambiguous,
                                               call_position,
                                               paren_tok_seq_number,
                                               &single_function,
                                               unknown_dependent_function,
                                               found_through_adl,
                                               try_surrogate_functions ?
                                                 &surrogate_function_conv_sym :
                                                 (a_symbol_ptr *)NULL,
                                               &arg_match_list);
  *arg_expr_list = NULL;
  if (function_symbol != NULL) {
    /* There was no error, i.e., a best function was chosen. */
    base_function_symbol = fundamental_symbol_of(function_symbol);
    routine = base_function_symbol->variant.routine.ptr;
    routine_type = skip_typerefs(routine->type);
    /* Do the things that would have been done to the symbol but weren't
       because the specific symbol was not known, and build an operand
       for the function. */
    make_resolved_overloaded_function_operand(function_symbol,
                                              overloaded_function_symbol,
                                              orig_function_operand,
                                              call_position,
                                              &have_selector,
                                              bound_function_selector,
                                              is_property,
                                              function_operand);
  } else if (surrogate_function_conv_sym != NULL) {
    /* A surrogate function was selected.  Convert the class object to
       a pointer to function using the conversion function, then call
       the function pointed to. */
    a_type_ptr conv_rout_type = arg_match_list->conversion.routine->type;
    a_type_ptr conversion_type = return_type_of(conv_rout_type);
    copy_operand(bound_function_selector, function_operand);
    user_convert_operand(function_operand,
                         conversion_type,
                         &arg_match_list->conversion,
                         (a_conv_descr *)NULL,
                         /*force_copy_to_temp=*/FALSE);
    /* See whether the conversion function returns a reference type. */
    if (arg_match_list->conversion.result_is_an_lvalue) {
      routine_type = conversion_type;
      if (is_pointer_type(routine_type)) {
        /* Deal with the case of a conversion function returning a
           reference to pointer to function. */
        routine_type = type_pointed_to(conversion_type);
        conv_lvalue_to_rvalue(function_operand);
      } else {
        /* A conversion function returning a reference to function. */
        conv_function_designator_to_ptr_to_function(function_operand,
                                                    (a_source_position *)NULL,
                                                    /*allow_ctor=*/FALSE,
                                                    /*will_call=*/TRUE);
      }  /* if */
    } else {
      routine_type = type_pointed_to(conversion_type);
    }  /* if */
    routine_type = skip_typerefs(routine_type);
    have_selector = FALSE;
  }  /* if */
  if (!single_function) {
    /* Build an expression-form argument list.  Convert the arguments on
       the argument list to the right types.  Free arg_operand_list
       and arg_match_list (the call is done even when function_symbol
       is NULL so that the freeing will be done). */
    adjust_overloaded_function_call_arguments(function_symbol,
                                              unknown_dependent_function!=NULL?
                                                *unknown_dependent_function :
                                                FALSE,
                                              routine_type,
                                              have_selector,
                                              bound_function_selector,
                                              arg_operand_list,
                                              arg_match_list,
                                              arg_expr_list);
  } else {
    /* There was only a single function in the "overload set".  It has
       been selected without checking whether it matches the argument list.
       Check now that the arguments match the parameters, and build the
       list of argument expressions. */
    an_arg_check_block arg_block;
    /* This shouldn't happen for nonstatic member functions. */
    check_assertion(function_symbol != NULL && !have_selector);
    function_symbol = fundamental_symbol_of(function_symbol);
    start_call_argument_processing(routine_symbol_type(function_symbol),
                                   function_symbol->variant.routine.ptr,
                                   &arg_block);
    if (closing_paren_position != NULL) {
      arg_block.closing_paren_position = *closing_paren_position;
    }  /* if */
    process_call_argument_list(arg_operand_list, &arg_block);
    *arg_expr_list = arg_block.argument_head;
  }  /* if */
  db_exit();
  return routine_type;
}  /* select_and_prepare_to_call_overloaded_function */


static a_boolean is_pointer_to_object_type(a_type_ptr tp)
/*
Return TRUE if the given type is a pointer to an object type.  Note that
object types can be incomplete in some cases.
*/
{
  a_boolean result = FALSE;

  if (is_pointer_type(tp)) {
    a_type_ptr underlying_type = type_pointed_to(tp);
    result = is_object_type(underlying_type);
  }  /* if */
  return result;
}  /* is_pointer_to_object_type */


static void try_conversion_function_match(
                            an_operand               *source_operand,
                            a_type_ptr               dest_type,
                            a_type_ptr               requested_type,
                            a_builtin_type_kind_set  builtin_types_allowed,
                            a_boolean                need_lvalue_result,
                            a_boolean                is_copy_initialization,
                            a_boolean                is_reference_binding,
                            a_candidate_function_ptr *candidate_functions)
/*
See if a class operand source_operand can be converted by a conversion function
to either

(a) dest_type, if dest_type is non-NULL, and an lvalue of that type if
    need_lvalue_result is TRUE, or
(b) a built-in type in the set given by builtin_types_allowed, if
    builtin_types_allowed != BTK_NONE, and a non-const lvalue of that
    type if need_lvalue_result is TRUE (if both (a) and (b) apply,
    (b) takes precedence, and dest_type is used only to guide the
    selection of template conversion functions and to establish the
    cost of any conversion needed after the conversion function).

In the case of a bitwise copy constructor, dest_type reflects the parameter
type of the constructor, which can be different from the type actually
specified in a cast; requested_type gives the original type and is used to
ensure that argument deduction for a conversion function template will use
the type actually specified rather than the constructor's parameter type.
(In all other cases, dest_type and requested_type should be the same.)

If a conversion function to do that conversion exists, evaluate how
well it matches the arguments and add it to the candidate_functions list,
setting "conversion" in the candidate function entry.
If is_reference_binding is TRUE, the result will be bound directly to
a reference, so consider conversions to a derived class of dest_type,
and allow appropriate cv-qualification adjustments, but do not
consider standard conversions after the conversion function; otherwise,
allow standard conversions on the result.  If is_copy_initialization
is TRUE, the result will be copied for a copy-initialization.
This routine is only used in C++ mode.
*/
{
  a_symbol_ptr              conversion_symbol, base_conversion_symbol;
  a_routine_ptr             conversion_routine;
  a_symbol_list_entry_ptr   slep;
  a_type_ptr                source_type, conv_routine_type, return_type;
  a_type_ptr                raw_return_type, eff_this_param_type;
  an_arg_match_summary      this_match;
  an_arg_match_summary_ptr  this_match_ptr;
  a_std_conv_descr          std_conversion;
  a_boolean                 compatible;
  a_boolean                 result_is_an_lvalue;
  a_boolean                 result_is_a_reference;
  a_candidate_function_ptr  candidate;
  a_base_class_ptr          bcp;
  a_boolean                 class_object_adjustment_required = FALSE;
  a_boolean                 template_conversions_started;
  a_boolean                 function_template_case;
  a_template_arg_ptr        template_arg_list;
  a_template_symbol_supplement_ptr
                            tssp;

  db_enter(4, "try_conversion_function_match");
  /* This routine is similar to try_overloaded_function_match. */
  if (builtin_types_allowed == BTK_BOOL) {
    /* There's only one type in the BTK_BOOL category, so make this a
       conversion to a specific type so that templates can be used. */
    dest_type = requested_type = bool_type();
    builtin_types_allowed = (a_builtin_type_kind_set)BTK_NONE;
  } else if (builtin_types_allowed == BTK_PTRDIFF_T) {
    /* There's only one type in the BTK_PTRDIFF_T category, so make this a
       conversion to a specific type so that templates can be used. */
    dest_type = requested_type = integer_type(targ_ptrdiff_t_int_kind);
    builtin_types_allowed = (a_builtin_type_kind_set)BTK_NONE;
  }  /* if */
  source_type = source_operand->type;
  check_assertion_str(is_class_struct_union_type(source_type),
                      "try_conversion_function_match: source not class");
  /* If the source type is a template class, instantiate it to make its
     conversion functions visible. */
  instantiate_template_class(source_type);
  /* Look at all the conversion functions for the source class.  After the
     end of the normal list, if we have a specific dest_type go through the
     list of template conversion functions. */
  template_conversions_started = FALSE;
  /*lint --e{850} slep modified in loop */
  for (slep = symbol_supplement_for_class(source_type)->conversion_list;
       ;
       slep = slep->next) {
    if (slep == NULL) {
      /* Either exit the loop or, if the list of template conversion functions
         is yet to be processed, process that. */
      if (!template_conversions_started && dest_type != NULL) {
        template_conversions_started = TRUE;
        slep = symbol_supplement_for_class(source_type)->
                                                    conversion_template_list;
      }  /* if */
      if (slep == NULL) break;
    }  /* if */
    conversion_symbol = slep->symbol;
#if DEBUG
    if (debug_level >= 4 || db_flag_is_set("overload")) {
      db_display_overload_level();
      db_symbol(conversion_symbol,
                "try_conversion_function_match: considering ", 4);
    }  /* if */
#endif /* DEBUG */
    /* Set template_arg_list early so that, on goto to reject_function, we
       know what has to be freed. */
    template_arg_list = NULL;
    conversion_routine = NULL;
    result_is_an_lvalue = FALSE;
    this_match_ptr = NULL;
    clear_std_conv_descr(&std_conversion);
    base_conversion_symbol = fundamental_symbol_of(conversion_symbol);
    function_template_case = (base_conversion_symbol->kind ==
                                          (a_symbol_kind)sk_function_template);
    if (is_ambiguous_by_inheritance(conversion_symbol)) {
      /* The symbol is ambiguous, and as such is an arbitrary
         representative of a set of functions that collided due
         to inheritance.  There's no point in seeing whether the function
         indicated matches up, since there might be another function
         that isn't represented that would match better.  Just put the
         symbol in the candidate functions set.  It will stay in there
         and cause the overload resolution to be ambiguous. */
      goto accept_function;
    }  /* if */
    if (!function_template_case) {
      /* The symbol is not a template. */
      conversion_routine = base_conversion_symbol->variant.routine.ptr;
      conv_routine_type = conversion_routine->type;
    } else {
      a_type_ptr eff_dest_type = requested_type;
      a_boolean  weird_gpp_case = FALSE;
      /* The symbol is a function template. */
      check_assertion(eff_dest_type != NULL);  /* For Coverity. */
      /* Don't do type deduction if that would produce a conversion
         function that returns an abstract class type (which would be
         invalid). */
      if (is_abstract_class_type(eff_dest_type)) goto reject_function;
      /* Do type deduction on the return type. */
      tssp = base_conversion_symbol->variant.template_info;
      conversion_routine = tssp->variant.function.routine;
      conv_routine_type = conversion_routine->type;
      return_type = return_type_of(conv_routine_type);
      /* Determine whether the desired type matches the type returned by the
         conversion template.  If normal deduction fails, check whether a
         qualification conversion can be used to obtain the desired type. */
      /* See [temp.deduct.conv] in the standard for the rules on dropping
         cv-qualifiers.  The "P" of that section is return_type (except that
         return_type is already reduced to the underlying type if the function
         returns a reference) and the "A" of that section is eff_dest_type
         (ditto for a reference type; is_reference_binding indicates that
         has happened). */
      /* g++ uses old [temp.deduct.conv] rules predating core issue 976
         and therefore doesn't drop the cv-qualifiers on P when A is not
         a reference (because at that point in the old rules, P had not
         been changed to the underlying type if it was a reference).
         Checked in 4.4. */
      if (gpp_mode && !is_reference_binding &&
          is_reference_type(il_return_type_of(conv_routine_type))) {
        weird_gpp_case = TRUE;
      }  /* if */
      if (matches_template_type(is_reference_binding ?
                                                  eff_dest_type :
                                                  skip_typerefs(eff_dest_type),
                                is_reference_binding || weird_gpp_case ?
                                                  return_type :
                                                  skip_typerefs(return_type),
                                &template_arg_list,
                                tssp->variant.function.decl_cache.
                                                         decl_info->parameters,
                                MTT_NO_FLAGS)) {
        /* Match. */
      } else if (matches_template_type_with_qualification_conversion(
                                 eff_dest_type,
                                 return_type,
                                 &template_arg_list,
                                 tssp->variant.function.decl_cache.
                                                         decl_info->parameters,
                                 MTT_IS_CONVERSION_TEMPLATE)) {
        /* Match with qualification conversion on pointer or
           pointer-to-member. */
      } else if (is_reference_binding &&
                 (check_template_arg_type_qualifiers(&return_type,
                                                     &eff_dest_type),
                  matches_template_type(eff_dest_type,
                                        return_type,
                                        &template_arg_list,
                                        tssp->variant.function.decl_cache.
                                                         decl_info->parameters,
                                        MTT_NO_FLAGS))) {
        /* Match with added cv-qualification under reference. */
      } else {
        /* Type deduction failed, so the conversion function is not viable. */
        goto reject_function;
      }  /* if */
      /* Make a version of the routine type with the proper types/values
         substituted for the template parameters. */
      conv_routine_type = wrapup_function_template_argument_deduction(
                                           &template_arg_list, 
                                           base_conversion_symbol,
                                           (a_template_param_ptr)NULL,
                                           /*is_partial_order_check=*/FALSE);
      if (conv_routine_type == NULL) goto reject_function;
    }  /* if */
    /* Is the type returned by this routine a type we want? */
    compatible = FALSE;
    conv_routine_type = skip_typerefs(conv_routine_type);
    return_type = return_type_of(conv_routine_type);
    raw_return_type = conv_routine_type->variant.routine.return_type;
    result_is_a_reference = is_reference_type(raw_return_type);
    result_is_an_lvalue = result_is_a_reference &&
                          is_lvalue_reference_type(raw_return_type);
    if (need_lvalue_result && !result_is_an_lvalue) {
      /* We need an lvalue result but the conversion function does
         not return one.  This is tested again later; the test here is for
         speed. */
      /* compatible = FALSE; -- already set. */
    } else if (dest_type != NULL && builtin_types_allowed == BTK_NONE) {
      /* We're looking for a specific type. */
      a_boolean types_match_ignoring_qualifiers =
              types_are_compatible_ignoring_qualifiers(dest_type, return_type);
      if (is_class_struct_union_type(return_type)) {
        /* The conversion function returns a class type. */
        bcp = NULL;
        if (types_match_ignoring_qualifiers ||
            ((is_reference_binding || is_copy_initialization) &&
             is_class_struct_union_type(dest_type) &&
             is_class_struct_union_type(return_type) &&
             (bcp = find_base_class_of(return_type, dest_type)) != NULL)) {
          /* The source and destination types are the same, ignoring
             qualifiers. */
          /* Or ... */
          /* The result of the conversion function is a derived class of the
             desired type, and we'll be binding a reference to the result
             (either directly, or because we're copying a class and the
             input parameter of the copy constructor is a reference). */
          /* Check the type qualifiers. */
          if (type_qualifiers_match(dest_type, return_type)) {
            /* The type qualifiers match. */
            compatible = TRUE;
          } else {
            class_object_adjustment_required = TRUE;
            if (is_copy_initialization) {
              /* In copy-initialization, the value will be copied, so
                 qualifiers are not significant. */
              compatible = TRUE;
            } else if (is_reference_binding &&
                       !any_qualifier_missing(dest_type, return_type)) {
              /* When binding a reference, it's okay to add qualifiers, but not
                 to drop them. */
              compatible = TRUE;
            }  /* if */
          }  /* if */
          if (compatible) {
            if (bcp != NULL) {
              /* Now that we know the conversion is okay, set other information
                 on the conversion for the derived --> base case. */
              class_object_adjustment_required = TRUE;
              std_conversion.nontrivial_conversion = TRUE;
              std_conversion.cast_base_class = bcp;
              std_conversion.type_qualifiers_added =
                                 any_qualifier_missing(return_type, dest_type);
            }  /* if */
          }  /* if */
        }  /* if */
      } else {
        /* The conversion function returns a nonclass type. */
        if (result_is_a_reference &&
            (!is_reference_binding || !types_match_ignoring_qualifiers)) {
          /* If the conversion function returns a reference to an array or
             function type, account for the type decay that follows. */
          if (is_array_type(return_type)) {
            return_type =
                       type_after_array_to_pointer_transformation(return_type);
            result_is_a_reference = FALSE;
            result_is_an_lvalue = FALSE;
          } else if (is_function_type(return_type)) {
            return_type =
             type_after_function_to_pointer_transformation(return_type,
                                                           (an_operand *)NULL);
            result_is_a_reference = FALSE;
            result_is_an_lvalue = FALSE;
          }  /* if */
          if (!result_is_a_reference) {
            types_match_ignoring_qualifiers =
              types_are_compatible_ignoring_qualifiers(dest_type, return_type);
          }  /* if */
        }  /* if */
        if (types_match_ignoring_qualifiers) {
          /* This conversion function returns the type we want, ignoring
             type qualifiers.  That means we can use it.  It's easy to
             see that we can use it in the case where the type qualifiers
             are the same or some are added; it's harder to see that for
             the case where qualifiers are being dropped (which can only
             happen when the destination is a reference).  In that case,
             the returned value is an lvalue, but it can be converted to
             an rvalue which has no type qualifiers.  To put it another way,
             a conversion function to "const int &" can serve as a conversion
             function to "int" by converting to a "const int" lvalue and then
             to an "int" rvalue.  Note that class types are processed
             above and don't get here. */
          compatible = TRUE;
          if (result_is_an_lvalue) {
            if (any_qualifier_missing(dest_type, return_type)) {
              if (is_array_type(return_type) ||
                  is_function_type(return_type)) {
                /* But you can't do this with array and function lvalues,
                   because they decay to pointers. */
                compatible = FALSE;
              } else {
                /* The function returns a reference type and the referenced
                   type has more qualifiers than necessary.  Force the
                   conversion of the result to an rvalue to drop the
                   type qualifiers. */
                result_is_an_lvalue = FALSE;
              }  /* if */
            }  /* if */
          }  /* if */
        } else if (!is_reference_binding &&
                   impl_conversion_possible(return_type,
                                            /*source_is_constant=*/FALSE,
                                            /*source_is_string_literal=*/FALSE,
                                            (a_constant_ptr)NULL, dest_type,
                                      /*allow_qualifier_or_eh_mismatch=*/FALSE,
                                            /*suppress_extensions=*/TRUE,
                                            ec_no_error, &std_conversion)) {
          /* This conversion function returns a type that can be converted
             via a standard conversion to the type we want. */
          compatible = TRUE;
          result_is_an_lvalue = FALSE;
        }  /* if */
      }  /* if */
    } else {
      /* We're looking for a built-in type described in general terms. */
      /* See if this conversion function returns an acceptable built-in
         type. */
      if (need_lvalue_result && is_const_qualified_type(return_type)) {
        /* Rule out const types if an lvalue is required. */
      } else if (
          ((builtin_types_allowed & BTK_INTEGRAL) != 0 &&
                                              is_integral_type(return_type)) ||
          ((builtin_types_allowed & BTK_ENUM) != 0 &&
                                              is_enum_type(return_type)) ||
          ((builtin_types_allowed & BTK_BOOL) != 0 &&
                                              is_bool_type(return_type)) ||
          ((builtin_types_allowed & BTK_FLOATING) != 0 &&
                                              is_floating_type(return_type)) ||
          ((builtin_types_allowed & BTK_POINTER) != 0 &&
                                              is_pointer_type(return_type)) ||
          ((builtin_types_allowed & BTK_POINTER_TO_OBJECT) != 0 &&
                                     is_pointer_to_object_type(return_type)) ||
          ((builtin_types_allowed & BTK_POINTER_TO_FUNCTION) != 0 &&
                             is_pointer_type(return_type) &&
                             is_function_type(type_pointed_to(return_type))) ||
          ((builtin_types_allowed & BTK_PTR_TO_MEMBER) != 0 &&
                                         is_ptr_to_member_type(return_type)) ||
          ((builtin_types_allowed & BTK_PTRDIFF_T) != 0 &&
                                         is_ptrdiff_t_type(return_type))) {
        /* This conversion function returns an acceptable built-in type. */
        compatible = TRUE;
        /* The result does not have to be forced to an rvalue. */
      } else if ((builtin_types_allowed & BTK_INTEGRAL) != 0 &&
                                         is_enum_type(return_type)) {
        /* The conversion function returns an enum type, which can be
           converted to the desired integral type. */
        compatible = TRUE;
        std_conversion.nontrivial_conversion = TRUE;
        if (dest_type != NULL) {
          a_type_ptr promoted_type =type_after_integral_promotion(return_type);
          if (types_are_compatible_ignoring_qualifiers(promoted_type,
                                                       dest_type)) {
            /* The conversion is a promotion. */
            std_conversion.promotion = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    /* Give up on this function if it does not return a type we can use. */
    if (!compatible) goto reject_function;
    if (need_lvalue_result && !result_is_an_lvalue) {
      /* We need an lvalue result but the conversion function does
         not return one.  This was tested previously, but since then
         result_is_an_lvalue may have been changed to FALSE. */
      goto reject_function;
    }  /* if */
    /* This conversion function meets the requirements for result type.
        However, we must also see whether or not it can be called for this
        argument (i.e., are the type qualifiers okay), and how good the
        match is. */
    eff_this_param_type =
                       this_param_type_for_overload_res(conv_routine_type,
                                                        conversion_symbol,
                                                        /*is_conv_func=*/TRUE);
    selector_match_with_this_param(source_operand,
                                   conversion_routine,
                                   eff_this_param_type,
                                   &this_match);
    /* Ignore this function if it cannot be called for this argument. */
    if (this_match.match_level == aml_none) goto reject_function;
    this_match_ptr = alloc_arg_match_summary();
    *this_match_ptr = this_match;
accept_function:
    /* The routine is viable. */
    /* Add the conversion function to the candidate functions list. */
    if (function_template_case) {
      add_function_template_to_candidate_functions_list(
                                         conversion_symbol,
                                         /*expl_template_arg_list_used=*/FALSE,
                                         template_arg_list,
                                         this_match_ptr,
                                         candidate_functions);
      /* candidate->conversion.routine and
         candidate->conversion.routine_symbol are not set now.
         They are set later if the function is instantiated. */
    } else {
      /* The routine is not a template. */
      add_function_to_candidate_functions_list(conversion_symbol,
                                               this_match_ptr,
                                               candidate_functions);
      candidate = *candidate_functions;
      candidate->conversion.routine = conversion_routine;
      candidate->conversion.routine_symbol = conversion_symbol;
    }  /* if */
    candidate = *candidate_functions;
    candidate->is_user_conversion = TRUE;
    candidate->conversion.class_object_adjustment_required =
                                              class_object_adjustment_required;
    candidate->conversion.std = std_conversion;
    candidate->conversion.result_is_an_lvalue = result_is_an_lvalue;
    goto next_function;
reject_function:
    /* Function was rejected.  Free anything allocated for it. */
    free_template_arg_list(template_arg_list);
next_function:;
  }  /* for */
  db_exit();
}  /* try_conversion_function_match */


static a_type_ptr rvalue_return_type_of(a_type_ptr conv_func_type)
/*
Return the return type of the conversion function with the indicated
routine type, assuming that the value will be converted to an rvalue.
*/
{
  a_type_ptr return_type = return_type_of(conv_func_type);
  /* Convert the type to an rvalue type. */
  return_type = do_implicit_type_transformations(return_type,
                                                 (an_operand *)NULL);
  return return_type;
}  /* rvalue_return_type_of */


static a_symbol_ptr find_conversion_function(
                                            a_type_ptr              class_type,
                                            a_type_ptr              dest_type,
                                            a_symbol_list_entry_ptr stop_on)
/*
See if there is a conversion function that converts class_type (a class type)
to dest_type.  If so, return a pointer to the symbol.  If not, return NULL.
If stop_on is non-NULL, stop looking upon encountering that entry in
the list of conversion functions of the class (the stop is before that
entry is processed, so NULL is returned if it is encountered).

Note:  This looks like a general-purpose routine, but it's not.  It won't
deal with differences like

  operator int() const;
  operator int();

For which one needs to know the exact type of the operand to be converted.
It also assumes that the converted value will be used as an rvalue.
This routine is useful as a quick way of seeing whether or not a particular
type appears on the list of conversion functions.
*/
{
  a_symbol_ptr             conversion_symbol;
  a_symbol_list_entry_ptr  slep;
  a_type_ptr               conv_routine_type, return_type;

#if CHECKING
  if (!is_class_struct_union_type(class_type)) {
    internal_error("find_conversion_function: source not class type");
  }  /* if */
#endif /* CHECKING */
  dest_type = skip_typerefs(dest_type);
  /* Examine each conversion function from the source class. */
  for (slep = symbol_supplement_for_class(class_type)->conversion_list;
       slep != NULL && slep != stop_on;
       slep = slep->next) {
    conversion_symbol = slep->symbol;
    reduce_projection_symbol_to_fundamental_symbol(conversion_symbol);
    conv_routine_type = routine_symbol_type(conversion_symbol);
    return_type = rvalue_return_type_of(conv_routine_type);
    if (identical_types(dest_type, return_type)) {
      /* Found the required function. */
      goto end_of_search;
    }  /* if */
  }  /* for */
  /* No matching function was found. */
  conversion_symbol = NULL;
end_of_search:
  return conversion_symbol;
}  /* find_conversion_function */


static char *operand_type_pattern_for_operator(an_opname_kind kind,
                                               a_boolean      unary_operator)
/*
Return a string describing the argument type patterns permitted for the
indicated operator (the unary version if unary_operator is TRUE).
The argument string contains one or more possible patterns separated
by semicolons, e.g., "AA;OI;IO"; each pattern has one letter (for
unary operators) or two letters (for binary operators) giving the type
code for the associated operand.  See the list of type code #defines
earlier in this file (e.g., INTEGRAL_TYPE_CODE).  The string begins
with LVALUE_FIRST_OPERAND_TYPE_CODE if the operator requires an lvalue
as its first operand.
*/
{
  char *operand_type_pattern;

  if (unary_operator) {
    switch (kind) {
      case onk_plus:
        /* Unary "+" takes an arithmetic or pointer (sic!) operand. */
        operand_type_pattern = "A;P";
        break;
      case onk_minus:
        /* Unary "-" takes an arithmetic operand. */
        operand_type_pattern = "A";
        break;
      case onk_not:
        if (bool_is_keyword) {
          /* "!" takes a bool operand. */
          operand_type_pattern = "B";
        } else {
          /* "!" takes an operand that can be tested in a boolean controlling
             expression. */
          operand_type_pattern = "b";
        }  /* if */
        break;
      case onk_compl:
        /* "~" takes an integral operand. */
        operand_type_pattern = "I";
        break;
      case onk_star:
        /* "*" takes an object pointer or function pointer operand. */
        operand_type_pattern = "O;F";
        break;
      case onk_plus_plus:
      case onk_minus_minus:
        /* "++" and "--" (prefix) take an arithmetic or object pointer lvalue.
           See below for postfix (which shows up as a two-operand operator). */
        operand_type_pattern = "La;O";
        break;
#if CHECKING
      default:
        internal_error("operand_type_pattern_for_operator: bad unary op");
#endif /* CHECKING */
    }  /* switch */
  } else {
    /* Binary operator. */
    switch (kind) {
      case onk_star:
      case onk_divide:
        /* "*" and "/" take arithmetic operands. */
        operand_type_pattern = "AA";
        break;
      case onk_remainder:
      case onk_shift_left:
      case onk_shift_right:
      case onk_ampersand:
      case onk_or:
      case onk_excl_or:
        /* "%", "<<", ">>", "&", "|", and "^" take integral operands. */
        operand_type_pattern = "II";
        break;
      case onk_plus:
        /* "+" takes arith+arith, pointer+ptrdiff_t, or ptrdiff_t+pointer. */
        operand_type_pattern = "AA;OD;DO";
        break;
      case onk_minus:
        /* "-" takes arith-arith, pointer-ptrdiff_t, or pointer-pointer. */
        operand_type_pattern = "AA;OD;=OO";
        break;
      case onk_lt:
      case onk_le:
      case onk_gt:
      case onk_ge:
        /* Relational operators take arithmetic or pointer operands.
           Also, if overloading on enums is enabled, matching enum types. */
        if (cfront_2_1_mode) {
          /* cfront 2.1 is confused and allows pointers to members on this
             case (they get rejected if chosen). */
          operand_type_pattern = "AA;=PP;=MM";
        } else if (microsoft_bugs &&
                   microsoft_version < 1310) {
          /* Microsoft considers only arithmetic types, not pointers.
             This is fixed in MSVC++ 7.1. */
          if (operator_overloading_on_enums_enabled) {
            /* MSVC++ 6.0 does seem not to have enums in the set (MSVC++ 7.0
               does), but it has compensating bugs that make things act mostly
               as if the enums were in the set, so we do that. */
            operand_type_pattern = "AA;=EE";
          } else {
            operand_type_pattern = "AA";
          }  /* if */
        } else if (operator_overloading_on_enums_enabled) {
          operand_type_pattern = "AA;=PP;=EE";
        } else {
          operand_type_pattern = "AA;=PP";
        }  /* if */
        break;
      case onk_eq:
      case onk_ne:
        /* Equality operators take arithmetic, pointer, or pointer-to-member
           operands.  Also, if overloading on enums is enabled, matching
           enum types. */
        /* MSVC++ 6.0 does seem not to have enums in the set (MSVC++ 7.0
           does), but it has compensating bugs that make things act mostly
           as if the enums were in the set, so we do that. */
        if (operator_overloading_on_enums_enabled) {
          operand_type_pattern = "AA;=PP;=MM;=EE";
        } else {
          operand_type_pattern = "AA;=PP;=MM";
        }  /* if */
        break;
      case onk_gnu_min:
      case onk_gnu_max:
        if (operator_overloading_on_enums_enabled) {
          operand_type_pattern = "AA;=PP;=EE";
        } else {
          operand_type_pattern = "AA;=PP";
        }  /* if */
        break;
      case onk_and_and:
      case onk_or_or:
        if (bool_is_keyword) {
          /* "&&" and "||" take bool operands. */
          operand_type_pattern = "BB";
        } else {
          /* "&&" and "||" take operands that can be tested in a boolean
             controlling expression. */
          operand_type_pattern = "bb";
        }  /* if */
        break;
      case onk_times_assign:
      case onk_divide_assign:
        /* "*=" and "/=" take arithmetic operands, the first an lvalue. */
        operand_type_pattern = "LaA";
        break;
      case onk_remainder_assign:
      case onk_shift_left_assign:
      case onk_shift_right_assign:
      case onk_and_assign:
      case onk_or_assign:
      case onk_excl_or_assign:
        /* "%=", "<<=", ">>=", "&=", "|=", and "^=" take integral operands,
           the first an lvalue. */
        operand_type_pattern = "LiI";
        break;
      case onk_plus_assign:
        /* "+=" takes arith+arith or pointer+ptrdiff_t, the first an lvalue. */
        operand_type_pattern = "LaA;OD";
        break;
      case onk_minus_assign:
        /* "-=" takes arith-arith or pointer-ptrdiff_t, the first an lvalue. */
        operand_type_pattern = "LaA;OD";
        break;
      case onk_subscript:
        /* "[]" takes pointer[ptrdiff_t] or ptrdiff_t[pointer]. */
        if (sun_mode) {
          operand_type_pattern = "OD";
        } else {
          operand_type_pattern = "OD;DO";
        }  /* if */
        break;
      case onk_plus_plus:
      case onk_minus_minus:
        /* "++" and "--" (postfix, which show up as two-operand operators)
           take an arithmetic or pointer lvalue.  A second implied
           operand is integer. */
        operand_type_pattern = "Lai;Oi";
        break;
      case onk_question:
        /* "?" (which shows up here as a two-operand operator) takes
           two operands (really the second and third) of arithmetic,
           pointer, or pointer-to-member type (the class and void cases
           are handled outside of this routine). */
        operand_type_pattern = "AA;=PP;=MM";
        break;
      case onk_arrow_star:
        /* "->*" takes a pointer to class and a pointer to member to the
           same class. */
        operand_type_pattern = "=OM";
        break;
#if CHECKING
      default:
        internal_error("operand_type_pattern_for_operator: bad binary op");
#endif /* CHECKING */
    }  /* switch */
  } /* if */
  return operand_type_pattern;
}  /* operand_type_pattern_for_operator */


static a_boolean type_matches_type_code(a_type_ptr type,
                                        char       type_code)
/*
Return TRUE if the indicated type matches the indicated type code, meaning
it fits that type description or can be converted to it.
*/
{
  a_boolean matches = FALSE;

  switch (type_code) {
    case INTEGRAL_TYPE_CODE:
    case PROMOTED_INTEGRAL_TYPE_CODE:
    case PTRDIFF_T_TYPE_CODE:
      matches = is_integral_or_enum_type(type);
      break;
    case ENUM_TYPE_CODE:
      matches = is_enum_type(type);
      break;
    case ARITH_TYPE_CODE:
      matches = is_arithmetic_type(type);
      break;
    case PROMOTED_ARITH_TYPE_CODE:
      matches = is_arithmetic_or_enum_type(type);
      break;
    case POINTER_TYPE_CODE:
      matches = is_pointer_type(type);
      break;
    case POINTER_TO_OBJECT_TYPE_CODE:
      matches = is_pointer_to_object_type(type);
      break;
    case POINTER_TO_FUNCTION_TYPE_CODE:
      matches = is_pointer_type(type) &&
                is_function_type(type_pointed_to(type));
      break;
    case PTR_TO_MEMBER_TYPE_CODE:
      matches = is_ptr_to_member_type(type);
      break;
    case BOOL_TYPE_CODE:
    case BOOL_EQUIVALENT_TYPE_CODE:
      /* Note that arithmetic includes bool, so is_bool_type doesn't need to
         be tested here. */
      matches = is_arithmetic_or_enum_type(type) ||
                is_pointer_type(type) ||
                is_ptr_to_member_type(type);
      break;
    case CLASS_TYPE_CODE:
      matches = is_class_struct_union_type(type);
      break;
    default:
      unexpected_condition_str("type_matches_type_code: bad type code");
  }  /* switch */
  return matches;
}  /* type_matches_type_code */


static a_builtin_type_kind_set builtin_type_set_for_type_code(char type_code)
/*
Build and return the built-in type kind set that corresponds to the indicated
type_code.
*/
{
  a_builtin_type_kind_set builtin_types_allowed = BTK_NONE;

  switch (type_code) {
    case INTEGRAL_TYPE_CODE:
    case PROMOTED_INTEGRAL_TYPE_CODE:
      builtin_types_allowed = BTK_INTEGRAL;
      break;
    case PTRDIFF_T_TYPE_CODE:
      builtin_types_allowed = BTK_PTRDIFF_T;
      break;
    case ENUM_TYPE_CODE:
      builtin_types_allowed = BTK_ENUM;
      break;
    case ARITH_TYPE_CODE:
    case PROMOTED_ARITH_TYPE_CODE:
      builtin_types_allowed = BTK_INTEGRAL | BTK_FLOATING;
      break;
    case POINTER_TYPE_CODE:
      builtin_types_allowed = BTK_POINTER;
      break;
    case POINTER_TO_OBJECT_TYPE_CODE:
      builtin_types_allowed = BTK_POINTER_TO_OBJECT;
      break;
    case POINTER_TO_FUNCTION_TYPE_CODE:
      builtin_types_allowed = BTK_POINTER_TO_FUNCTION;
      break;
    case PTR_TO_MEMBER_TYPE_CODE:
      builtin_types_allowed = BTK_PTR_TO_MEMBER;
      break;
    case BOOL_TYPE_CODE:
      builtin_types_allowed = BTK_BOOL;
      break;
    case BOOL_EQUIVALENT_TYPE_CODE:
      builtin_types_allowed = BTK_INTEGRAL | BTK_FLOATING | BTK_ENUM |
                              BTK_POINTER | BTK_PTR_TO_MEMBER;
      break;
    case CLASS_TYPE_CODE:
      /* Class types are not built-in types. */
      builtin_types_allowed = BTK_NONE;
      break;
    default:
      unexpected_condition_str(
                              "builtin_type_set_for_type_code: bad type code");
  }  /* switch */
  return builtin_types_allowed;
}  /* builtin_type_set_for_type_code */


static void determine_builtin_type_operand_conversion_cost(
                                            char                     type_code,
                                            an_opname_kind           kind,
                                            an_operand               *operand,
                                            an_arg_match_summary_ptr arg_match)
/*
*operand is an operand for a builtin operation, whose type is constrained
to be a type described by the indicated type code.  The operation is the
builtin operator described by "kind".  Determine the conversion cost (exact
match, promotion, etc.) for the operand and record it in arg_match.
*/
{
  an_arg_match_level match_level = aml_exact;

  if (cfront_2_1_mode) {
    /* cfront 2.1 considers all matches like this for builtins to be standard
       conversions. */
    match_level = aml_std_conversion;
  } else if (special_subscript_cost &&
             kind == (an_opname_kind)onk_subscript &&
             type_code == PTRDIFF_T_TYPE_CODE) {
    /* The subscript operator's integral operand is treated as a
       standard conversion always in cfront 3.0.2.  Who knows why,
       but this is used in jcool and tools.h++. */
    match_level = aml_std_conversion;
  } else {
    a_type_ptr operand_type = rvalue_type(operand->type);
    if (type_code == BOOL_TYPE_CODE) {
      /* A bool operand is wanted. */
      if (is_bool_type(operand_type)) {
        /* A bool operand was provided, so this is an exact match. */
        match_level = aml_exact;
      } else {
        /* Conversion of arithmetic, enum, pointer, or pointer to member
           to bool; this is a standard conversion. */
        match_level = aml_std_conversion;
        if (is_pointer_type(operand_type) ||
            is_ptr_to_member_type(operand_type)) {
          /* Conversion of a pointer or pointer to member to bool is worse
             than the others. */
          arg_match->conversion.std.ptr_or_pm_to_bool = TRUE;
        }  /* if */
      }  /* if */
    } else if (type_code == PROMOTED_INTEGRAL_TYPE_CODE ||
               type_code == PROMOTED_ARITH_TYPE_CODE ||
               type_code == INTEGRAL_TYPE_CODE ||
               type_code == ARITH_TYPE_CODE ||
               type_code == PTRDIFF_T_TYPE_CODE) {
      /* An arithmetic or integral operand is wanted. */
      /* cfront does not have the case of operands of type ptrdiff_t;
         treat them as promoted integral operands instead. */
      if (any_cfront_mode() && type_code == PTRDIFF_T_TYPE_CODE) {
        type_code = PROMOTED_INTEGRAL_TYPE_CODE;
      }  /* if */
      if (is_integral_or_enum_type(operand_type)) {
        if (type_code == PTRDIFF_T_TYPE_CODE) {
          /* An operand of type ptrdiff_t is wanted. */
          if (is_ptrdiff_t_type(operand_type)) {
            /* We have ptrdiff_t. */
            match_level = aml_exact;
          } else {
            a_type_ptr promoted_type =
                                operand_type_after_integral_promotion(operand);
            if (is_ptrdiff_t_type(promoted_type)) {
              /* The operand promotes to ptrdiff_t. */
              match_level = aml_promotion;
            } else {
              /* The operand converts to ptrdiff_t. */
              match_level = aml_std_conversion;
            }  /* if */
          }  /* if */
        } else if (is_enum_type(operand_type)) {
          if (any_cfront_mode()) {
            /* In cfront mode promotion of an enum doesn't have any cost. */
            /* match_level = aml_exact -- already set. */
          } else {
            /* An enum type is not an integral type, so there's always at
               least a promotion cost. */
            match_level = aml_promotion;
          }  /* if */
        } else {
          /* The operand has a non-enum integral type. */
          if (type_code == PROMOTED_INTEGRAL_TYPE_CODE ||
              type_code == PROMOTED_ARITH_TYPE_CODE) {
            /* The operand is a non-enum integral type, and a promoted type
               is required.  See if the type gets changed by promotion. */
            a_type_ptr promoted_type =
                                operand_type_after_integral_promotion(operand);
            if (microsoft_bugs) {
              /* MSVC++ (6.0, 7.0, 7.1) doesn't count these as promotions. */
              /* match_level = aml_exact -- already set. */
            } else if (!types_are_compatible(promoted_type, operand_type)) {
              /* The type gets changed by promotion, so the cost is a
                 promotion. */
              match_level = aml_promotion;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  arg_match->match_level = match_level;
}  /* determine_builtin_type_operand_conversion_cost */


static void try_builtin_operands_match(
                       an_opname_kind           kind,
                       char                     *operand_type_pattern,
                       a_boolean                first_operand_must_be_lvalue,
                       an_arg_operand_ptr       arg_operand_list,
                       a_candidate_function_ptr *candidate_functions,
                       a_type_ptr               specific_type)
/*
Subroutine for try_conversions_for_builtin_operator.  Check to see how
well the operand values given by arg_operand_list match the operand type
pattern string given by operand_type_pattern.  If they match, add
the built-in operator to the candidate_functions list.  The first
operand must be an lvalue if first_operand_must_be_lvalue (but this
routine need only check for the class cases).  The operator being
considered is described by "kind".  This is used for the case where
the pattern string contains no corresponding pointer or pointer to
member types, with specific_type == NULL, and with specific_type non-NULL
for pattern strings containing corresponding types (it indicates
the target type to be used).
*/
{
  a_boolean                okay;
  char                     type_code;
  char                     *type_pattern_position;
  an_arg_operand_ptr       arg_operand;
  an_arg_match_summary_ptr arg_match, arg_match_list, end_arg_match_list;
  a_type_ptr               operand_type;
  a_conv_descr             conversion;
  a_boolean                ambiguous, need_lvalue_result, operand_is_lvalue;
#if DEBUG
  unsigned long            narg;
#endif /* DEBUG */

  /* This routine is similar to try_overloaded_function_match (but it only
     looks at one type pattern per call). */
#if DEBUG
  if (debug_level >= 4 || db_flag_is_set("overload")) {
    db_display_overload_level();
    fprintf(f_debug, "try_builtin_operands_match: considering %s\n",
                     operand_type_pattern);
  }  /* if */
  narg = 0;
#endif /* DEBUG */
  arg_match_list = end_arg_match_list = NULL;
  okay = TRUE;
  need_lvalue_result = first_operand_must_be_lvalue;
  /* Go through the operands and determine the match level on each operand. */
  for (type_pattern_position = operand_type_pattern,
         arg_operand = arg_operand_list;
       arg_operand != NULL;
       type_pattern_position++, need_lvalue_result = FALSE,
                                             arg_operand = arg_operand->next) {
#if CHECKING
    if (*type_pattern_position == ';' ||
        *type_pattern_position == '\0') {
      /* The operand list and the type pattern do not have the same number
         of elements. */
      internal_error("try_builtin_operands_match: ran off pattern");
    }  /* if */
#endif /* CHECKING */
#if DEBUG
    narg++;
    if (debug_level >= 4 || db_flag_is_set("overload")) {
      db_display_overload_level();
      fprintf(f_debug, "try_builtin_operands_match: operand %lu\n", narg);
    }  /* if */
#endif /* DEBUG */
    /* Get the operand type. */
    operand_type = arg_operand->operand.type;
    operand_is_lvalue = is_an_lvalue(&arg_operand->operand);
    /* Add an entry to the end of the arg_match_list to record whether or
       not this argument matches. */
    arg_match = alloc_arg_match_summary();
    if (arg_match_list == NULL) {
      arg_match_list = arg_match;
    } else {
      end_arg_match_list->next = arg_match;
    }  /* if */
    end_arg_match_list = arg_match;
    /* See if the operand type matches the type code. */
    type_code = *type_pattern_position;
    if (specific_type == NULL) {
      /* Non-specific builtin type required. */
      if (is_class_struct_union_type(operand_type)) {
        /* The operand has a class type, so see if it can be converted to
           an appropriate built-in type. */
        a_type_ptr other_operand_type;
        /* Get the type of the other operand.  This is used to guide selection
           of template conversion functions if it's an appropriate type. */
        if (arg_operand_list->next == NULL) {
          /* No other type for unary operators. */
          other_operand_type = NULL;
        } else {
          if (arg_operand == arg_operand_list) {
            other_operand_type = arg_operand_list->next->operand.type;
          } else {
            other_operand_type = arg_operand_list->operand.type;
          }  /* if */
          if (!type_matches_type_code(other_operand_type, type_code)) {
            /* The other operand type is not a builtin type that could be
               used for the current operand, so ignore it. */
            other_operand_type = NULL;
          }  /* if */
        }  /* if */
        if (conversion_from_class_possible(&arg_operand->operand,
                                           other_operand_type,
                                     builtin_type_set_for_type_code(type_code),
                                           need_lvalue_result,
                                           /*is_copy_initialization=*/TRUE,
                                           /*is_reference_binding=*/FALSE,
                                           &conversion,
                                           &ambiguous,
                                           (a_candidate_function_ptr *)NULL) ||
            ambiguous) {
          /* The conversion can be done. */
          arg_match->match_level = aml_user_conversion;
          arg_match->conversion = conversion;
          arg_match->guide_type = other_operand_type;
        }  /* if */
      } else {
        /* A non-class operand.  See if it has (or can be converted to)
           the required type. */
        /* Do array --> pointer and function --> pointer transformations. */
        operand_type = do_implicit_type_transformations(operand_type,
                                                        &arg_operand->operand);
        /* Note that we do not try 0 --> pointer, because in this case it
           would require just making up a pointer type out of nowhere --
           there's no other operand that provides guidance on which pointer
           type is required. */
        if (type_matches_type_code(operand_type, type_code)) {
          /* The type is correct.  See what the cost is (there might be
             a promotion). */
          determine_builtin_type_operand_conversion_cost(type_code, kind,
                                                         &arg_operand->operand,
                                                         arg_match);
          arg_match->lvalue_to_rvalue_conversion_used = operand_is_lvalue;
        }  /* if */
      }  /* if */
    } else {
      /* A specific type is required.  Check that the operand can be
         converted to the type passed in. */
      a_type_ptr eff_specific_type = specific_type;
      if (*type_pattern_position == POINTER_TO_OBJECT_TYPE_CODE &&
          operand_type_pattern[1] == PTR_TO_MEMBER_TYPE_CODE) {
        /* For "->*", the first operand is a pointer to class, and the
           second is a pointer to member of the same class.  The
           specific_type is a pointer to member, so make the proper
           pointer type for the first operand. */
        a_type_ptr           class_type = pm_class_type(specific_type);
        a_type_qualifier_set qualifiers = TQ_NONE;
        if (is_pointer_type(operand_type)) {
          /* If the first operand has a pointer type, adopt the cv-qualifiers
             under the pointer type as part of the specific type.  This
             allows a first operand of, say "pointer to const X" with a
             second operand of type "pointer to member of X of type T". */
          a_type_ptr underlying_type = type_pointed_to(operand_type);
          qualifiers = get_type_qualifiers(underlying_type);
        }  /* if */
        eff_specific_type = make_pointer_type(make_qualified_type(class_type,
                                                                  qualifiers));
      }  /* if */
      if (type_code == CLASS_TYPE_CODE) {
        /* See if the operand can be converted to the specific type (which
           is a class type).  This is used for the operands of the "?"
           operator, and because that operator doesn't copy its operands,
           the analysis here treats the operands specially. */
        a_base_class_ptr bcp = NULL;
        check_assertion(is_class_struct_union_type(eff_specific_type));
        if (types_are_compatible_ignoring_qualifiers(operand_type,
                                                     eff_specific_type) ||
            (is_class_struct_union_type(operand_type) &&
             (bcp = find_base_class_of(operand_type,
                                       eff_specific_type)) != NULL)) {
          /* Same class, or derived --> base: preserves the identity of
             the class object. */
          if (any_qualifier_missing(eff_specific_type, operand_type)) {
            /* Some qualifier is dropped, so we can't do this.  Presumably
               we will be doing this test again on another call of this
               routine with specific_type the same as the (present)
               operand_type, and that will be viable. */
          } else {
            /* The operand can be made to match up. */
            if (!type_qualifiers_match(eff_specific_type, operand_type)) {
              arg_match->conversion.class_object_adjustment_required = TRUE;
            }  /* if */
            if (bcp == NULL) {
              /* A class used as the same class type. */
              arg_match->match_level = aml_exact;
            } else {
              /* A derived class used as the base class. */
              arg_match->match_level = aml_std_conversion;
              arg_match->conversion.std.cast_base_class = bcp;
              arg_match->conversion.std.nontrivial_conversion = TRUE;
              arg_match->conversion.class_object_adjustment_required = TRUE;
            }  /* if */
            arg_match->conversion.result_is_an_lvalue =
                                           is_an_lvalue(&arg_operand->operand);
          }  /* if */
        } else if (conversion_to_class_possible(
                                         &arg_operand->operand,
                                         eff_specific_type,
                                         /*try_bitwise_copy=*/FALSE,
                                         /*initializing_return_value=*/FALSE,
                                         /*is_copy_initialization=*/TRUE,
                                         /*orig_is_copy_initialization=*/TRUE,
                                         /*is_reference_binding=*/FALSE,
                                         &conversion,
                                         (a_conv_descr *)NULL,
                                         &ambiguous,
                                         (a_candidate_function_ptr *)NULL) ||
            ambiguous) {
          /* The conversion can be done as a conversion that doesn't preserve
             the identity of the class object. */
          arg_match->match_level = aml_user_conversion;
          arg_match->conversion = conversion;
          arg_match->param_type = eff_specific_type;
        }  /* if */
      } else if (is_class_struct_union_type(operand_type)) {
        /* The operand has a class type, so see if it can be converted to
           the specific type (which is a non-class type). */
        if (conversion_from_class_possible(&arg_operand->operand,
                                           eff_specific_type,
                                           (a_builtin_type_kind_set)BTK_NONE,
                                           need_lvalue_result,
                                           /*is_copy_initialization=*/TRUE,
                                           /*is_reference_binding=*/FALSE,
                                           &conversion,
                                           &ambiguous,
                                           (a_candidate_function_ptr *)NULL) ||
            ambiguous) {
          /* The conversion can be done with a conversion function. */
          arg_match->match_level = aml_user_conversion;
          arg_match->conversion = conversion;
          arg_match->param_type = eff_specific_type;
        }  /* if */
      } else {
        a_boolean        cfront_null_ptr_constant_case;
        a_std_conv_descr std_conv;
        a_boolean        source_is_constant;
        a_constant_ptr   source_constant;
        /* A non-class operand. */
        /* Do array --> pointer and function --> pointer transformations. */
        operand_type = do_implicit_type_transformations(operand_type,
                                                        &arg_operand->operand);
        source_is_constant = is_constant_operand(&arg_operand->operand);
        source_constant = &arg_operand->operand.variant.constant;
        if (is_an_lvalue(&arg_operand->operand) &&
            !(any_cfront_mode() || gpp_mode)) {
          /* Treat a constant-valued integral variable as its value.  This
             is useful when the value is a null pointer constant. */
          a_constant_ptr con_var_value =
                   value_of_constant_var_lvalue_operand(&arg_operand->operand);
          if (con_var_value != NULL) {
            source_is_constant = TRUE;
            source_constant = con_var_value;
          }  /* if */
        }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (microsoft_mode && !source_is_constant) {
          /* Microsoft mode allows some expressions as null pointer
             constants. */
          adjust_constant_operand_info_for_microsoft_null_pointer_test(
                                                      &arg_operand->operand,
                                                      &source_is_constant,
                                                      &source_constant,
                                                      (an_expr_node **)NULL);
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* See if we can convert the type we have to the type we want. */
        cfront_null_ptr_constant_case =
                               any_cfront_mode() &&
                               source_is_constant &&
                               is_null_pointer_constant(source_constant) &&
                               (is_pointer_type(eff_specific_type) ||
                                is_ptr_to_member_type(eff_specific_type));
        if (cfront_null_ptr_constant_case &&
            (!arg_operand->operand.is_cfront_null_pointer_constant ||
             (cfront_3_0_mode &&
              (kind == (an_opname_kind)onk_lt ||
               kind == (an_opname_kind)onk_gt ||
               kind == (an_opname_kind)onk_le ||
               kind == (an_opname_kind)onk_ge)))) {
          /* cfront requires that a null pointer constant be spelled "0"
             for it to be convertible to a pointer in overload resolution.
             This one isn't.   cfront 3.0 doesn't allow null pointer
             conversions on relational operators. */
          arg_match->match_level = aml_none;
        } else if (impl_conversion_possible(operand_type,
                                            source_is_constant,
                                            (a_boolean)arg_operand->operand.
                                                      is_simple_string_literal,
                                            source_constant,
                                            eff_specific_type,
                                      /*allow_qualifier_or_eh_mismatch=*/FALSE,
                                            /*suppress_extensions=*/TRUE,
                                            ec_no_error, &std_conv)) {
          /* The conversion can be done. */
          if (cfront_null_ptr_constant_case) {
            /* cfront uses standard weighting for these. */
            arg_match->match_level = cfront_2_1_mode ? 
                                                aml_std_conversion : aml_exact;
          } else {
            arg_match->match_level = std_conv.nontrivial_conversion ?
                                                aml_std_conversion : aml_exact;
          }  /* if */
          arg_match->conversion.std = std_conv;
          arg_match->param_type = eff_specific_type;
          arg_match->lvalue_to_rvalue_conversion_used = operand_is_lvalue;
        }  /* if */
      }  /* if */
    }  /* if */
    /* If this operand cannot be made to match, give up on this version of
       the built-in operator. */
    if (arg_match->match_level == aml_none) {
      okay = FALSE;
      break;
    }  /* if */
  }  /* for */
  if (okay) {
    /* The built-in operator can be used.  Add it to the list of
       candidate functions. */
    add_builtin_operator_to_candidate_functions_list(operand_type_pattern,
                                                     specific_type,
                                                     arg_match_list,
                                                     candidate_functions);
  } else {
    /* The built-in operator cannot be used. */
    free_arg_match_summary_list(arg_match_list);
  }  /* if */
}  /* try_builtin_operands_match */


static a_boolean specific_type_previously_handled(
                     a_type_ptr              specific_type,
                     a_type_ptr              class_type,
                     a_symbol_list_entry_ptr stop_on,
                     a_type_ptr              previous_class_type_considered,
                     a_type_ptr              previous_specific_type_considered)
/*
Helper routine for try_corresp_builtin_operands_match.  Return TRUE if
specific_type has already been tried as a target specific type.
class_type, if non-NULL, indicates the current operand class type,
and in that case stop_on, if non-NULL, indicates the entry on the
conversion functions list of class_type that is currently being
considered.  previous_class_type_considered, if non-NULL, indicates
the type of a previous class operand already considered;
previous_specific_type_considered, if non-NULL, indicates the type
of a previous non-class operand already considered.
*/
{
  a_boolean previously_handled = FALSE;

  if (previous_specific_type_considered != NULL &&
      identical_types(previous_specific_type_considered, specific_type)) {
    /* This type was the type of a previous non-class operand;
       it's already been considered. */
    previously_handled = TRUE;
  } else if (previous_class_type_considered != NULL) {
    /* Some types were tried on a previous class operand, so
       check to see if the specific type we're considering was
       already processed on the previous operand.  It was if the
       underlying class types of the operands are the same or if
       the previous class has a conversion function that converts
       to the specific type we're considering. */
    if (same_entities(class_type, previous_class_type_considered) ||
        find_conversion_function(previous_class_type_considered,
                                 specific_type,
                                 (a_symbol_list_entry_ptr)NULL) != NULL) {
      /* This specific type was tried when the first operand was
         processed, so do not try it again (if we did, it would
         look like an ambiguity). */
      previously_handled = TRUE;
    }  /* if */
  }  /* if */
  if (!previously_handled && class_type != NULL && stop_on != NULL &&
      find_conversion_function(class_type,
                               specific_type,
                               stop_on) != NULL) {
    /* The specific type was already handled as the result type of a
       previous conversion function for the current operand class type.
       (For example, a conversion function returning int and one
       returning int & have the same effective type if we're looking
       for an rvalue.) */
    previously_handled = TRUE;
  }  /* if */
  return previously_handled;
}  /* specific_type_previously_handled */


static void adjust_specific_type_for_previous_specific_type(
                                            a_type_ptr  *specific_type,
                                            a_type_ptr  previous_specific_type)
/*
Helper function for adjust_specific_type_for_previous_operand.  If
*specific_type and previous_specific_type are pointer types, look for
a pointer type that both can be converted to, e.g., by creating a type
with the union of the cv-qualifiers on the two types, and return
*specific_type updated to that.  Otherwise, leave *specific_type
unchanged.
*/
{
  if (is_pointer_type(previous_specific_type) &&
      is_pointer_type(*specific_type)) {
    /* Look for the usual pointer cases. */
    a_type_ptr composite =
                     multilevel_composite_pointer_type(*specific_type,
                                                       previous_specific_type);
    if (composite != NULL) {
      *specific_type = composite;
    } else {
      a_type_ptr underlying_type = type_pointed_to(*specific_type);
      a_type_ptr other_underlying_type =
                                     type_pointed_to(previous_specific_type);
      if (is_void_type(underlying_type) ||
          is_void_type(other_underlying_type)) {
        /* "pointer to cv T" can be converted to "pointer to cv void",
           so if at least one of the operands is a pointer to void,
           "pointer to cv-union void" will work as the common type. */
        a_type_ptr temp_type = void_type();
        temp_type = type_plus_qualifiers_from_second_type(temp_type,
                                                          underlying_type);
        temp_type = type_plus_qualifiers_from_second_type(temp_type,
                                                        other_underlying_type);
        *specific_type = make_pointer_type(temp_type);
      } else {
        /* Look for derived/base cases with cv-qualifier adjustment, e.g.,
             const Base *
           and
             volatile Derived *
           which requires the composite type
             const volatile Base *
        */
        a_boolean        baseward_cast;
        a_base_class_ptr bcp;
        if (f_related_class_pointers(*specific_type, previous_specific_type,
                                     &baseward_cast, &bcp)) {
          if (baseward_cast) {
            /* previous_specific_type is the base class, so swap the underlying
               types so that the composite type is built on the base class. */
            a_type_ptr temp_type = underlying_type;
            underlying_type = other_underlying_type;
            other_underlying_type = temp_type;
          }  /* if */
          underlying_type = type_plus_qualifiers_from_second_type(
                                                        underlying_type,
                                                        other_underlying_type);
          *specific_type = make_pointer_type(underlying_type);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* adjust_specific_type_for_previous_specific_type */


static void adjust_specific_type_for_previous_operand(
                     a_type_ptr              *specific_type,
                     a_type_ptr              class_type,
                     a_symbol_list_entry_ptr stop_on,
                     an_opname_kind          kind,
                     a_type_ptr              previous_class_type_considered,
                     a_type_ptr              previous_specific_type_considered)
/*
We are considering the type given by *specific_type as the operation
type for a built-in operation.  If *specific_type is the result type of a
conversion function, class_type indicates the class type of the operand;
otherwise, it is NULL.  If class_type is non-NULL, stop_on, if non-NULL,
indicates the entry on the conversion functions list of class_type that
is currently being considered.  The kind of operation is indicated by kind.
If there was a previous operand, the previous class type considered and
the previous specific type considered are given by the like-named
parameters; otherwise, they are NULL.  Adjust *specific_type as necessary
for the previous operand.
*/
{
  /* Make sure there is a previous operand, i.e., do nothing on the
     first operand. */
  if (previous_class_type_considered != NULL ||
      previous_specific_type_considered != NULL) {
    /* Look for a case like one where the first operand is or can be
       converted to
         const char *
       and the second operand is or can be converted to
         volatile char *
       and update the specific type to have the union of the cv-qualifiers,
       i.e.,
         const volatile char *
    */
    /* This applies only on operations where the result type does not
       depend on the operand types, i.e., relational operators and the pointer
       difference "-". */
    if (is_pointer_type(*specific_type)) {
      if (kind == (an_opname_kind)onk_eq ||
          kind == (an_opname_kind)onk_ne ||
          kind == (an_opname_kind)onk_gt ||
          kind == (an_opname_kind)onk_lt ||
          kind == (an_opname_kind)onk_ge ||
          kind == (an_opname_kind)onk_le ||
          kind == (an_opname_kind)onk_minus) {
        a_type_ptr orig_specific_type = *specific_type;
        /* Add cv-qualifiers from previously-considered pointer types. */
        if (previous_specific_type_considered != NULL) {
          /* The previous operand has a specific type. */
          adjust_specific_type_for_previous_specific_type(
                                            specific_type,
                                            previous_specific_type_considered);
        } else {
          /* The first operand has a class type. */
          a_symbol_list_entry_ptr slep;
          /* Examine each conversion function from the source class. */
          for (slep = symbol_supplement_for_class(
                              previous_class_type_considered)->conversion_list;
               slep != NULL;
               slep = slep->next) {
            a_symbol_ptr conversion_symbol = slep->symbol;
            a_symbol_ptr base_conversion_symbol =
                                      fundamental_symbol_of(conversion_symbol);
            a_type_ptr   conv_routine_type =
                                   routine_symbol_type(base_conversion_symbol);
            a_type_ptr   return_type= rvalue_return_type_of(conv_routine_type);
            adjust_specific_type_for_previous_specific_type(specific_type,
                                                            return_type);
          }  /* for */
        }  /* if */
        if (!identical_types(*specific_type, orig_specific_type)) {
          /* We came up with a different type.  Make sure we haven't handled
             this new type previously.  If we have, go back to the original
             type, because using this new type would repeat a previous analysis
             and likely result in an apparent ambiguity. */
          if (specific_type_previously_handled(
                                          *specific_type, class_type, stop_on,
                                          previous_class_type_considered,
                                          previous_specific_type_considered)) {
            *specific_type = orig_specific_type;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* adjust_specific_type_for_previous_operand */


static void try_corresp_builtin_operands_match(
                         an_opname_kind           kind,
                         char                     *operand_type_pattern,
                         a_boolean                first_operand_must_be_lvalue,
                         an_arg_operand_ptr       arg_operand_list,
                         a_candidate_function_ptr *candidate_functions)
/*
Subroutine for try_conversions_for_builtin_operator.  Check to see how
well the operand values given by arg_operand_list match the operand type
pattern string given by operand_type_pattern.  If they match, add
the built-in operator to the candidate_functions list.  The first
operand must be an lvalue if first_operand_must_be_lvalue (but this
routine need only check for the class cases).  The operator being
considered is described by "kind".  This routine is used for cases
where the pattern string contains two operands that must correspond
in some way, e.g., two pointers that must have the same type.
*/
{
  an_arg_operand_ptr       arg_operand;
  a_type_ptr               specific_type, operand_type, class_type;
  char                     *type_pattern_position;
  a_symbol_ptr             conversion_symbol, base_conversion_symbol;
  a_symbol_list_entry_ptr  slep;
  a_type_ptr               conv_routine_type, return_type;
  a_type_ptr               previous_class_type_considered;
  a_type_ptr               previous_specific_type_considered;
  a_boolean                any_approp_conversion_function_this_operand;

  db_enter(4, "try_corresp_builtin_operands_match");
  /* The reason the corresponding-types case is more complicated than other
     cases is, to pick a pointer example, that it is not sufficient to ask
     "can this class-type operand be converted to any pointer type?" --
     we must ask whether both of the pointer operands can be converted to
     a specific pointer type or something compatible with it. */
  /* Loop through the two operands. */
  previous_class_type_considered = NULL;
  previous_specific_type_considered = NULL;
  for (type_pattern_position = operand_type_pattern,
         arg_operand = arg_operand_list;
       arg_operand != NULL;
       type_pattern_position++, arg_operand = arg_operand->next) {
    operand_type = arg_operand->operand.type;
    if (*type_pattern_position == POINTER_TO_OBJECT_TYPE_CODE &&
        operand_type_pattern[1] == PTR_TO_MEMBER_TYPE_CODE) {
      /* For "->*", the first operand is a pointer to class, and the
         second is a pointer to member of the same class.  Don't
         consider any specific types generated from the first operand,
         because they don't fully specify the second operand. */
    } else if (is_class_struct_union_type(operand_type)) {
      /* This operand has a class type.  Look for conversion functions that
         convert the class type to an appropriate type. */
      class_type = skip_typerefs(operand_type);
      any_approp_conversion_function_this_operand = FALSE;
      /* Look at all the conversion functions for the source class. */
      for (slep = symbol_supplement_for_class(class_type)->conversion_list;
           slep != NULL;
           slep = slep->next) {
        conversion_symbol = slep->symbol;
        base_conversion_symbol = fundamental_symbol_of(conversion_symbol);
        conv_routine_type = routine_symbol_type(base_conversion_symbol);
        return_type = rvalue_return_type_of(conv_routine_type);
        if (type_matches_type_code(return_type, *type_pattern_position)) {
          /* We've found a conversion function to an appropriate type.  Make
             sure it's not a type we've already checked while examining a
             previous operand.  If it is, ignore it. */
          specific_type = return_type;
          if (!specific_type_previously_handled(
                                          specific_type, class_type, slep,
                                          previous_class_type_considered,
                                          previous_specific_type_considered)) {
            /* Try matching the operands, with the chosen specific type. */
            any_approp_conversion_function_this_operand = TRUE;
            adjust_specific_type_for_previous_operand(
                                            &specific_type, class_type, slep,
                                            kind,
                                            previous_class_type_considered,
                                            previous_specific_type_considered);
            try_builtin_operands_match(kind, operand_type_pattern,
                                       first_operand_must_be_lvalue,
                                       arg_operand_list,
                                       candidate_functions,
                                       specific_type);
          }  /* if */
        }  /* if */
      }  /* for */
      /* Remember if we've processed any class types. */
      if (any_approp_conversion_function_this_operand) {
        previous_class_type_considered = class_type;
      }  /* if */
      if (*type_pattern_position == CLASS_TYPE_CODE) {
        /* A class type is allowed for this operand, so try the match also with
           the original type. */
        a_type_ptr other_operand_type;
        specific_type = operand_type;
        /* Get the type of the other operand. */
        if (arg_operand == arg_operand_list) {
          other_operand_type = arg_operand_list->next->operand.type;
        } else {
          other_operand_type = arg_operand_list->operand.type;
        }  /* if */
        if (is_class_struct_union_type(other_operand_type) &&
            find_base_class_of(other_operand_type, operand_type)) {
          /* The other operand has a type that's a derived class of the
             current operand type.  Add the cv-qualifiers of the other
             operand type to the specific type to be considered.
             For a case like "x ? Base : const Derived" this allows
             us to try "const Base". */
          specific_type = type_plus_qualifiers_from_second_type(specific_type,
                                                           other_operand_type);
        }  /* if */
        /* If the type has been previously handled, ignore it. */
        if (!specific_type_previously_handled(
                                          specific_type, (a_type_ptr)NULL,
                                          (a_symbol_list_entry_ptr)NULL,
                                          previous_class_type_considered,
                                          previous_specific_type_considered)) {
          previous_specific_type_considered = specific_type;
          /* Try matching the operands, with the chosen specific type. */
          try_builtin_operands_match(kind, operand_type_pattern,
                                     first_operand_must_be_lvalue,
                                     arg_operand_list,
                                     candidate_functions,
                                     specific_type);
        }  /* if */
      }  /* if */
    } else {
      /* The operand does not have a class type.  See if standard conversions
         can be used to get to the desired type. */
      /* Do array --> pointer and function --> pointer transformations. */
      operand_type = do_implicit_type_transformations(operand_type,
                                                      &arg_operand->operand);
      operand_type = skip_typerefs(operand_type);
      if (microsoft_bugs &&
          arg_operand->operand.is_simple_string_literal &&
          string_literals_are_const) {
        /* MSVC++ 7.1 and 8.0 (which have const string literals) seem
           to generate built-in operators as if the strings are not const.
           Note that the array-to-pointer decay has already been done. */
        operand_type = type_pointed_to(operand_type);
        operand_type = skip_typerefs(operand_type);
        operand_type = make_pointer_type(operand_type);
      }  /* if */
      if (type_matches_type_code(operand_type, *type_pattern_position)) {
        /* The operand has an appropriate type. */
        specific_type = operand_type;
        /* If the type has been previously handled, ignore it. */
        if (!specific_type_previously_handled(
                                          specific_type, (a_type_ptr)NULL,
                                          (a_symbol_list_entry_ptr)NULL,
                                          previous_class_type_considered,
                                          previous_specific_type_considered)) {
          adjust_specific_type_for_previous_operand(
                                            &specific_type, (a_type_ptr)NULL,
                                            (a_symbol_list_entry_ptr)NULL,
                                            kind,
                                            previous_class_type_considered,
                                            previous_specific_type_considered);
          previous_specific_type_considered = specific_type;
          /* Try matching the operands, with the chosen specific type. */
          try_builtin_operands_match(kind, operand_type_pattern,
                                     first_operand_must_be_lvalue,
                                     arg_operand_list,
                                     candidate_functions,
                                     specific_type);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  db_exit();
}  /* try_corresp_builtin_operands_match */


static void try_conversions_for_builtin_operator(
                                 an_opname_kind           kind,
                                 a_boolean                unary_operator,
                                 an_arg_operand_ptr       arg_operand_list,
                                 a_candidate_function_ptr *candidate_functions)
/*
See if conversion functions can be used to convert the operands of an
operator to built-in types that would be suitable for the built-in
version of the operator.  The operator is specified by kind and unary_operator.
The operands are specified by arg_operand_list.  If the built-in operator
can be used, it is added to the candidate_functions list.
*/
{
  char       *operand_type_pattern;
  an_operand *first_operand;
  a_boolean  first_operand_must_be_lvalue = FALSE;

  db_enter(4, "try_conversions_for_builtin_operator");
  /* Determine the argument pattern for the operator, and whether or not
     the first operand must be an lvalue.  The pattern begins with "L"
     if the operator requires an lvalue as its first operand.  Following that
     are one or more semicolon-separated argument patterns, each one consisting
     of one letter (for unary operators) or two letters (for binary operators)
     indicating the allowed argument types.  As a concrete example, the
     pattern for "-=" is "LaA;OI", indicating that the operator requires
     an lvalue and takes operands of types arith-=arith or pointer-=int. */
  operand_type_pattern = operand_type_pattern_for_operator(kind,
                                                           unary_operator);
  if (*operand_type_pattern == LVALUE_FIRST_OPERAND_TYPE_CODE) {
    /* The operator requires an lvalue as its first operand.  Check that.
       If the operand has a class type it might be convertible to an lvalue
       via a conversion function returning a reference, but that's
       checked later. */
    /* In cfront 2.1 mode, do not require that conversions from class types
       yield lvalues; the error check gets done by the builtin operator. */
    if (!cfront_2_1_mode) first_operand_must_be_lvalue = TRUE;
    first_operand = &arg_operand_list->operand;
    if (!is_an_lvalue(first_operand) &&
        !is_class_struct_union_type(first_operand->type)) {
      /* The first operand is not an lvalue so the built-in operator cannot
         be used.  Give up. */
      goto end_of_check;
    }  /* if */
    /* Advance past the LVALUE_FIRST_OPERAND_TYPE_CODE. */
    operand_type_pattern++;
  }  /* if */
  /* Check the operands to see if they can be converted to the proper
     types. */
  /* Loop for each ";"-separated pattern in the string. */
  do {
    if (operand_type_pattern[0] == CORRESP_TYPE_CODE) {
      /* This operator takes operands of corresponding types (e.g., two
         pointers that must match).  Use a special routine that
         enumerates the types that can be generated by the applicable
         conversion functions. */
      /* If LVALUE_FIRST_OPERAND_TYPE_CODE and CORRESP_TYPE_CODE are
         ever allowed together, try_conversion_function_match will have
         to be changed to disallow conversions to const types on the
         lvalue operand. */
      check_assertion(!first_operand_must_be_lvalue);
      operand_type_pattern++;
      try_corresp_builtin_operands_match(kind, operand_type_pattern,
                                         first_operand_must_be_lvalue,
                                         arg_operand_list,
                                         candidate_functions);
    } else {
      /* There are no corresponding types in the argument pattern. */
      try_builtin_operands_match(kind, operand_type_pattern,
                                 first_operand_must_be_lvalue,
                                 arg_operand_list,
                                 candidate_functions,
                                 (a_type_ptr)NULL);
    }  /* if */
    /* Advance to the next pattern or stop the loop at the end of the
       string. */
    operand_type_pattern++;
    if (!unary_operator) operand_type_pattern++;
  } while (*operand_type_pattern++ == ';');
end_of_check:;
  db_exit();
}  /* try_conversions_for_builtin_operator */


static void prep_for_known_possible_conversion(an_operand   *operand,
                                               a_conv_descr *conversion)
/*
We have a case where a conversion has previously been determined to be
possible, and information about the conversion has been saved in "conversion".
Now we have decided to actually do the conversion, and we have gotten to
a point that uses conversion_possible to determine (again) whether or
not the conversion can be done and how.  Since we already know that, we
can skip the call of conversion_possible.  However, conversion_possible
does some things (like conversion from lvalue to rvalue) that need to be
done anyway.  This routine is called instead of conversion_possible and
does those things.
*/
{
  /* When there's a user-defined conversion routine, do not do the
     transformations.  They are done (if needed) when processing the
     argument of the conversion routine. */
  if (conversion->routine == NULL) {
    /* Convert lvalue --> rvalue, array --> pointer, and
       function --> pointer. */
    do_operand_transformations(operand,
                               TOPT_SUPPRESS_CHECK_FOR_INDEFINITE_FUNCTION);
  }  /* if */
}  /* prep_for_known_possible_conversion */


static void prep_special_selector_operand(an_operand *operand,
                                          a_type_ptr routine_type)
/*
For unconventional "this" arguments, convert the selector object given by
operand to the proper type (e.g., cast it to a base class if necessary).
This is needed for conversion functions, but not for function calls using
the usual notation or for operator functions (in those cases, the "catch up"
processing does the base class cast).  routine_type gives the type of the
routine being called.  operand is a class lvalue or rvalue, and never a
pointer to class.  Note that the selector might yet be used directly (e.g.,
for a bitwise assignment) instead of passed to a member function, so
this routine does not assume that the selector address will be taken.
*/
{
  if (is_class_struct_union_type(operand->type)) {
    a_type_ptr       this_param_type, this_class_type, operand_class_type;
    a_type_ptr       qual_this_class_type;
    a_base_class_ptr bcp;

    this_param_type = implicit_this_param_type_of(routine_type);
    qual_this_class_type = type_pointed_to(this_param_type);
    this_class_type = skip_typerefs(qual_this_class_type);
    operand_class_type = skip_typerefs(operand->type);
    if (!same_entities(operand_class_type, this_class_type) &&
        (bcp = find_base_class_of(operand_class_type, this_class_type))!=NULL){
      /* Do the cast to a base class.  Access checking is suppressed on this
         cast, because the cast is really necessary only because the function
         is inherited from a base class.  cv-qualifiers will also be adjusted
         if necessary. */
      base_class_cast_operand(operand, bcp, qual_this_class_type,
                              /*check_cast_access=*/FALSE,
                              /*is_implicit_cast=*/TRUE,
                              /*implicit_in_naming=*/FALSE,
                              /*is_object_pointer=*/TRUE);
    } else {
      /* Adjust cv-qualifiers if necessary. */
      adjust_class_object_type(operand, qual_this_class_type,
                               (a_base_class_ptr)NULL);
    }  /* if */
  } else {
    /* The selector does not have a class type. */
    check_assertion(is_error_type(operand->type) ||
                    is_template_param_type(operand->type));
  }  /* if */
}  /* prep_special_selector_operand */


void adjust_class_object_type(an_operand       *operand,
                              a_type_ptr       dest_type,
                              a_base_class_ptr bcp)
/*
*operand is a class lvalue or rvalue.  Adjust its type to dest_type,
which may differ from the current type in being a base class or having
different cv-qualifiers.  bcp is non-NULL to indicate the base class
case.  This adjustment does not make a new object; it merely adjusts
the operand type to access the same class object with a new type.
*/
{
  if (bcp != NULL) {
    /* Cast the pointer to the proper base class, and also adjust cv-qualifiers
       if necessary. */
    base_class_cast_operand(operand, bcp, dest_type,
                            /*check_cast_access=*/TRUE,
                            /*is_implicit_cast=*/TRUE,
                            /*implicit_in_naming=*/FALSE,
                            /*is_object_pointer=*/TRUE);
  } else if (!identical_types(operand->type, dest_type)) {
    /* Do a cv-qualifier adjustment. */
    if (is_an_lvalue(operand)) {
      adjust_lvalue_type(operand, dest_type);
    } else if (is_an_rvalue(operand)) {
      adjust_class_rvalue_type(operand, dest_type);
    } else {
      check_assertion(is_error_operand(operand));
    }  /* if */
  }  /* if */
}  /* adjust_class_object_type */


static void full_adjust_class_object_type(an_operand *operand,
                                          a_type_ptr dest_type)
/*
*operand is a class lvalue or rvalue.  Adjust its type to dest_type,
which may differ from the current type in being a base class or having
different cv-qualifiers.  This adjustment does not make a new object;
it merely adjusts the operand type to access the same class object with
a new type.  This routine differs from adjust_class_object_type in that
it will handle a base class cast without being given the base class
pointer.
*/
{
  a_type_ptr source_type = operand->type;

  if (!identical_types(source_type, dest_type)) {
    if (is_error_operand(operand)) {
      /* Leave an error operand alone. */
    } else if (is_error_type(dest_type)) {
      conv_to_error_operand(operand);
    } else {
      a_base_class_ptr bcp;
      if (types_are_compatible_ignoring_qualifiers(source_type, dest_type)) {
        bcp = NULL;
      } else {
        bcp = find_base_class_of(source_type, dest_type);
        check_assertion(bcp != NULL);
      }  /* if */
      adjust_class_object_type(operand, dest_type, bcp);
    }  /* if */
  }  /* if */
}  /* full_adjust_class_object_type */


static void do_class_object_adjustment(an_operand       *operand,
                                       a_type_ptr       dest_type,
                                       a_conv_descr_ptr conversion)
/*
operand is a class object (lvalue or rvalue).  It may need to be adjusted
by changing it to refer to a base class and/or adjusting the cv-qualifiers.
These adjustments are similar to standard conversions, but they're
not standard conversions, so they get their own routine.  dest_type
is the new type desired.  conversion->std.cast_base_class, if non-NULL,
indicates the base class to be referred to.  On return, the operand
is an rvalue or lvalue as required by conversion->result_is_an_lvalue.
*/
{
  if (conversion->class_object_adjustment_required) {
    /* Cast to a base class if necessary, and adjust cv_qualifiers. */
    adjust_class_object_type(operand, dest_type,
                             conversion->std.cast_base_class);
  }  /* if */
  /* If an rvalue is wanted, convert to an rvalue. */
  if (!conversion->result_is_an_lvalue) {
    conv_lvalue_to_rvalue(operand);
  }  /* if */
}  /* do_class_object_adjustment */


static void adjust_operand_for_builtin_operator(
                                   an_operand               *operand,
                                   a_candidate_function_ptr candidate_function,
                                   int                      operand_num,
                                   a_boolean                inside_conditional,
                                   an_arg_match_summary_ptr arg_match)
/*
operand is the operand_num-th operand of a built-in operator with
operands described by candidate_function.  inside_conditional is TRUE
if this operand is a conditional operand of the operator (e.g., the second
operand of "&&").  arg_match is the argument match entry for the operand.
Adjust the operand type to match the type requirement.
*/
{
  a_type_ptr specific_type;
  /* Get the type code for this operand (see
     operand_type_pattern_for_operator). */
  char       *operand_type_pattern = candidate_function->operand_type_pattern;
  char       type_code = operand_type_pattern[operand_num-1];

  if (!is_class_struct_union_type(operand->type) &&
      type_code != CLASS_TYPE_CODE) {
    /* Non-class operands need not be adjusted here; the built-in operator
       processing will do it. */
  } else {
    a_boolean saved_inside_conditional_expression;
    if (inside_conditional) {
      /* If this operand is under a conditional operator, make sure
         inside_conditional_expression is set in the expression stack so
         that any dynamic initializations used for conversions are marked as
         conditional (they will need conditional flags). */
      saved_inside_conditional_expression =
                                     expr_stack->inside_conditional_expression;
      expr_stack->inside_conditional_expression = TRUE;
    }  /* if */
    specific_type = candidate_function->specific_type;
    if (specific_type == NULL) {
      /* For some type codes, there is only one possible type, so convert to
         that type. */
      if (type_code == BOOL_TYPE_CODE) {
        specific_type = bool_type();
      } else if (type_code == PTRDIFF_T_TYPE_CODE) {
        specific_type = integer_type(targ_ptrdiff_t_int_kind);
      }  /* if */
    }  /* if */
    if (specific_type == NULL) {
      /* Non-specific type case.  The conversion function result type is the
         right type. */
      if (conv_usable(&arg_match->conversion)) {
        /* The conversion is usable.  Do it. */
        prep_for_known_possible_conversion(operand, &arg_match->conversion);
        user_convert_operand(operand, /*dest_type=*/(a_type_ptr)NULL,
                             &arg_match->conversion, (a_conv_descr *)NULL,
                             /*force_copy_to_temp=*/FALSE);
      } else {
        /* The conversion is not usable, e.g., because the conversion
           is ambiguous.  Redo the analysis of the conversion to get
           a detailed error message. */
        a_conv_descr             conversion;
        a_boolean                ambiguous;
        a_candidate_function_ptr ambiguity_list;

        if (conversion_from_class_possible(operand, arg_match->guide_type,
                                     builtin_type_set_for_type_code(type_code),
                                           /*need_lvalue_result=*/FALSE,
                                           /*is_copy_initialization=*/TRUE,
                                           /*is_reference_binding=*/FALSE,
                                           &conversion,
                                           &ambiguous, &ambiguity_list)) {
          unexpected_condition_str2("adjust_operand_for_builtin_operator:",
                                    "unusable conversion now succeeds");
        }  /* if */
        check_assertion_str2(ambiguous,
                             "adjust_operand_for_builtin_operator:",
                             "unusable conversion not ambiguous");
        /* A NULL ambiguity_list indicates a case that was undecidable because
           of an error (no additional error is needed). */
        if (ambiguity_list != NULL) {
          if (expr_error_should_be_issued()) {
            pos_ty_start_error(ec_ambiguous_conversion_to_builtin,
                               &operand->position, operand->type);
            diagnose_overload_ambiguity(ambiguity_list,
                                        (an_operand *)NULL,
                                        (an_arg_operand_ptr)NULL,
                                        (an_opname_kind)onk_none);
          }  /* if */
          free_candidate_function_list(ambiguity_list);
        }  /* if */
        conv_to_error_operand(operand);
      }  /* if */
    } else {
      /* A specific type is wanted.  Convert to the type indicated in
         candidate_function. */
      if (type_code == POINTER_TO_OBJECT_TYPE_CODE &&
          operand_type_pattern[1] == PTR_TO_MEMBER_TYPE_CODE) {
        /* For "->*", the first operand is a pointer to class, and the
           second is a pointer to member of the same class.  The
           specific_type is a pointer to member, so make the proper
           pointer type for the first operand. */
        specific_type = make_pointer_type(pm_class_type(specific_type));
      }  /* if */
      if (type_code == CLASS_TYPE_CODE &&
          arg_match->conversion.routine == NULL) {
        /* Conversion to a class type that preserves the identity of the
           class object. */
        do_class_object_adjustment(operand, specific_type,
                                   &arg_match->conversion);
      } else {
        prep_conversion_operand(operand, specific_type, 
                                (a_boolean *)NULL,
                                &arg_match->conversion,
                                /*initializing_return_value=*/FALSE,
                                /*is_copy_initialization=*/TRUE,
                                /*orig_is_copy_initialization=*/TRUE,
                                /*nontype_template_arg=*/FALSE,
                                ec_no_error,
                                &operand->position);
      }  /* if */
    }  /* if */
    if (inside_conditional) {
      /* Restore inside_conditional_expression. */
      expr_stack->inside_conditional_expression = 
                                           saved_inside_conditional_expression;
    }  /* if */
  }  /* if */
}  /* adjust_operand_for_builtin_operator */


static void make_generic_operation_operand(
                               an_opname_kind          kind,
                               a_boolean               unary_operator,
                               an_operand              *operand_1,
                               an_operand              *operand_2,
                               an_operand              *result,
                               a_source_position       *operator_position,
                               a_token_sequence_number operator_tok_seq_number,
                               a_source_position       *operator_position_2)
/*
Make an operand for a "generic" operation, i.e., one on template-dependent
operands in a prototype instantiation.  kind indicates the operation,
and unary_operator is TRUE if the operation is a unary operation.
operand_1 and operand_2 are the operands (operand_2 is needed only
for non-unary operations).  The result operand is returned in *result.
*operator_position gives the source position of the operator.
operator_tok_seq_number gives the token sequence number of the operator.
If non-NULL, operator_position_2 gives the source position of a secondary
operator (e.g., the "]" of a subscript operation).
*/
{
  an_expr_operator_kind generic_op =
                               operator_for_opname_kind(kind, unary_operator);

  if (kind == (an_opname_kind)onk_plus_plus ||
      kind == (an_opname_kind)onk_minus_minus) {
    /* Turn postfix "++" or "--" back into a true unary operation (it has a
       zero argument added for the purpose of calling an overloaded
       operator function). */
    unary_operator = TRUE;
  }  /* if */
  if (unary_operator) {
    template_unary_operation(generic_op, operand_1, result,
                             operator_position,
                             operator_tok_seq_number);
  } else {
    /* Two-operand operation. */
    template_binary_operation(generic_op, operand_1, operand_2,
                              result, operator_position,
                              operator_tok_seq_number,
                              operator_position_2);
  }  /* if */
}  /* make_generic_operation_operand */


static a_boolean some_candidate_matches_without_user_defined_convs(
                                           a_candidate_function_ptr candidates)
/*
Return TRUE if there is a candidate function on the list that matches without
a user-defined conversion.  This is used for a Sun-mode quirk.
*/
{
  a_boolean                result = FALSE;
  a_candidate_function_ptr cfp;
  an_arg_match_level       worst_match;

  for (cfp = candidates; cfp != NULL; cfp = cfp->next) {
    worst_match = worst_arg_match_level_for_candidate_arg(cfp);
    if (worst_match != aml_user_conversion) {
      result = TRUE;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* some_candidate_matches_without_user_defined_convs */


void check_for_operator_overloading(
                             an_opname_kind            kind,
                             a_boolean                 unary_operator,
                             a_boolean                 must_be_member_function,
                             a_boolean                 try_conversions,
                             a_boolean                 has_predef_meaning,
                             an_operand                *operand_1,
                             an_operand                *operand_2,
                             a_source_position         *operator_position,
                             a_token_sequence_number   operator_tok_seq_number,
                             a_nondependent_call_depth call_depth,
                             a_source_position         *operator_position_2,
                             an_operand                *result,
                             a_boolean                 *processed)
/*
operand_1 and operand_2 are the operands of an operator indicated by kind.
If unary_operator is TRUE, the operation has only one operand, which is
given by operand_1.  If must_be_member_function is TRUE, this operator
is one where the operator function must be a member function (=, [], (), ->).
try_conversions is TRUE if we should look for conversion functions that
can convert the operands to built-in types that are acceptable for the
built-in version of the operator.  has_predef_meaning is TRUE if the
operator has a predefined meaning for classes (comma, ->, =, and unary &).
It's also used for calls where we don't want an error if no function
matches, because we want to try something else (e.g., the anachronism that
allows a one-argument function for postfix "++" and "--").  Check to see
if there is an operator function that can be invoked for the arguments.
If so, create an operand for the call in *result and return *processed
TRUE.  Also return *processed TRUE if the operation is an ambiguous
overloaded function call.  Otherwise, return *processed FALSE, which means
the caller should try the built-in meaning of the operator.  In that case,
conversion functions will have been applied to the operands if that's
appropriate.  In cases where none of the operands has a class or enum type,
the operands are returned unchanged.  Note that this routine is called for
operator "?", with unary_operator FALSE; the two operands are the second
and third operands of the "?" ("?" cannot be overloaded, but conversion
functions could still apply).  operator_position gives the operator source
position.  operator_tok_seq_number gives the token sequence number of
the operator.  If operator_position_2 is non-NULL, it gives a secondary
operator position (e.g., the "]" in a subscript operation).
call_depth is usually zero, but if non-zero is a disambiguator for
operator_tok_seq_number.  This routine also checks for
template-dependent operands in a prototype instantiation, and builds a
generic expression for such cases (where operator overloading might
apply, but we can't tell).
*/
{
  an_arg_operand_ptr       arg_operand_list, arg_operand_list2, arg_operand;
  an_expr_node_ptr         arg_expr_list, end_arg_expr_list;
  a_symbol_ptr             nonmember_functions_symbol = NULL;
  a_symbol_ptr             member_functions_symbol;
  a_symbol_ptr             function_symbol, proj_function_symbol;
  a_boolean                operand_1_is_class;
  an_operand               function_operand;
  a_candidate_function_ptr candidate_functions;
  an_arg_match_summary_ptr arg_match;
  a_boolean                matched_except_for_missing_selector = FALSE;
  a_boolean                matched_except_for_selector = FALSE;
  a_boolean                member_is_best_match, have_selector;
  an_expr_node_ptr         arg;
  a_type_ptr               routine_type;
  a_param_type_ptr         param;
  an_operand               *bound_function_selector;
  a_boolean                ambiguous;
  a_boolean                undecidable_because_of_error;
  a_boolean                arg_operand_list_not_used;
  a_boolean                dependent_call = FALSE;
  a_boolean                defer_overload_resolution = FALSE;
  a_boolean                found_through_adl = FALSE;

  db_enter(4, "check_for_operator_overloading");
#if DEBUG
  overload_level++;
  if (debug_level >= 4 || db_flag_is_set("overload")) {
    db_display_overload_level();
    fprintf(f_debug, "Entering check_for_operator_overloading\n");
  }  /* if */
#endif /* DEBUG */
  *processed = FALSE;
  /* Check for template-dependent operands in a prototype instantiation. */
  if (is_template_dependent_context() &&
      (operand_is_dependent(operand_1) ||
       (!unary_operator && operand_is_dependent(operand_2)))) {
    /* There is at least one template-dependent operand, so we cannot
       check for operator overloading.  Just build an expression with a
       generic operator. */
    make_generic_operation_operand(kind, unary_operator, operand_1, operand_2,
                                   result, operator_position,
                                   operator_tok_seq_number,
                                   operator_position_2);
    *processed = TRUE;
  } else if (!curr_expr_kind_is_const()) {
    /* Check for operator overloading (but not in constant expressions). */
    if (is_error_operand(operand_1) || 
        (!unary_operator && is_error_operand(operand_2))) {
      /* One or both of the operands is an error operand. */
      if ((any_opname_function_symbol(kind) &&
           (!must_be_member_function ||
            is_error_operand(operand_1) ||
            is_class_struct_union_type(operand_1->type))) ||
          !has_predef_meaning) {
        /* There exists a function that overloads the operator.  Therefore,
           a match might have been possible.  However, we cannot tell.
           Assume there is a match and give up. */
        /* Or there is no predefined meaning for this operator. */
        *processed = TRUE;
        make_error_operand(result);
        operand_will_not_be_used_because_of_error(operand_1);
        if (!unary_operator) {
          operand_will_not_be_used_because_of_error(operand_2);
        }  /* if */
      } else {
        /* The operator is not overloaded.  Return to the caller to do
           the built-in operator processing. */
      }  /* if */
    } else {
      /* At least one operand must have a class type or enum type.
         An error operand for the second operand counts as a class operand. */
      operand_1_is_class = is_class_struct_union_type(operand_1->type);
      if (operand_1_is_class ||
          (operator_overloading_on_enums_enabled &&
           is_enum_type(operand_1->type)) ||
          (!unary_operator &&
           (is_class_struct_union_type(operand_2->type) ||
            (operator_overloading_on_enums_enabled &&
             is_enum_type(operand_2->type)) ||
            is_error_operand(operand_2)))) {
        /* Operator overloading may apply.  That is, the operation may be a
           call of an overloaded operator function or the operands may be
           convertible to built-in types appropriate for the built-in
           operator. */
        /* Change the operands into argument operand form. */
        arg_operand_list = alloc_arg_operand();
        copy_operand(operand_1, &arg_operand_list->operand);
        if (unary_operator) {
          arg_operand_list2 = NULL;
        } else {
          arg_operand_list2 = alloc_arg_operand();
          copy_operand(operand_2, &arg_operand_list2->operand);
          arg_operand_list->next = arg_operand_list2;
        }  /* if */
        /* candidate_functions will contain the list of viable functions. */
        candidate_functions = NULL;
        if (is_prototype_instantiation_context()) {
          /* In a prototype instantiation.  If the operands are dependent,
             the code above should have spotted that and generated a generic
             expression operator.  Note that nonreal instantiations are
             excluded. */
          check_assertion_str(!is_template_dependent_type(operand_1->type) &&
                              (unary_operator ||
                               !is_template_dependent_type(operand_2->type)),
                              "check_for_operator_overloading: dep operand");
        } else if (do_dependent_name_processing &&
                   is_nonspecialized_instantiation_context()) {
          /* In a real (not prototype) instantiation, and doing dependent
             name processing.  Look up this call to see whether it was a
             dependent call in the prototype instantiation.  If it was a
             nondependent call, it was recorded, along with (usually) the
             symbol chosen by overload resolution. */
          a_nondependent_call_info_ptr ndcall_info = NULL;
          if (operator_tok_seq_number != 0) {
            ndcall_info = get_nondependent_call_info(operator_tok_seq_number,
                                                     call_depth);
          }  /* if */
          dependent_call = (ndcall_info == NULL);
          proj_function_symbol = function_symbol = NULL;
          if (!dependent_call) proj_function_symbol = ndcall_info->symbol;
          if (proj_function_symbol != NULL) {
            /* We know the function selected for this nondependent call
               during the prototype instantiation.  Use that without going
               through overload resolution. */
            a_boolean is_member;
            function_symbol = fundamental_symbol_of(proj_function_symbol);
            is_member = routine_type_is_nonstatic_member_function(
                                         routine_symbol_type(function_symbol));
            if (is_member) {
              member_functions_symbol = proj_function_symbol;
            } else {
              nonmember_functions_symbol = proj_function_symbol;
            }  /* if */
            try_overloaded_function_match(
                                         proj_function_symbol,
                                         /*is_template_id=*/FALSE,
                                         (a_template_arg_ptr)NULL,
                                         is_member ? arg_operand_list2 :
                                                     arg_operand_list,
                                         /*have_selector=*/is_member,
                                         is_member ? operand_1 :
                                                     (an_operand *)NULL,
                                         /*ctor_conversion_case=*/FALSE,
                                         /*initializing_return_value=*/FALSE,
                                         /*effects_copy_initialization=*/FALSE,
                                         /*allow_udc_on_arguments=*/TRUE,
                                         /*arg_dep_lookup_done=*/FALSE,
                                         /*from_arg_dep_lookup=*/FALSE,
                                         /*dependent_call=*/FALSE,
                                         /*forced_dependent=*/FALSE,
                                         /*known_to_be_visible=*/TRUE,
                                         /*is_overloaded_operator=*/TRUE,
                                         &candidate_functions,
                                         &matched_except_for_missing_selector,
                                         &matched_except_for_selector);
            goto select_best_function;
          }  /* if */
        }  /* if */
        /* Find any member function for the operator. */
        if (operand_1_is_class) {
          /* Instantiate the type if it is a template class.  This ensures that
             member operator functions that could apply are declared. */
          instantiate_template_class(operand_1->type);
          member_functions_symbol = opname_member_function_symbol(kind,
                                               skip_typerefs(operand_1->type));
          if (member_functions_symbol != NULL) {
            /* There are member functions for this class type.  See how well
               they match up. */
#if CHECKING
            if (!member_functions_symbol->is_class_member) {
              internal_error(
                            "check_for_operator_overloading: func not member");
            }  /* if */
#endif /* CHECKING */
            /* Use the first operand as the selector expression, and
               the second operand as the first actual argument . */
            try_overloaded_function_match(
                                         member_functions_symbol,
                                         /*is_template_id=*/FALSE,
                                         (a_template_arg_ptr)NULL,
                                         arg_operand_list2,
                                         /*have_selector=*/TRUE,
                                         operand_1,
                                         /*ctor_conversion_case=*/FALSE,
                                         /*initializing_return_value=*/FALSE,
                                         /*effects_copy_initialization=*/FALSE,
                                         /*allow_udc_on_arguments=*/TRUE,
                                         /*arg_dep_lookup_done=*/FALSE,
                                         /*from_arg_dep_lookup=*/FALSE,
                                         dependent_call,
                                         /*forced_dependent=*/FALSE,
                                         /*known_to_be_visible=*/TRUE,
                                         /*is_overloaded_operator=*/TRUE,
                                         &candidate_functions,
                                         &matched_except_for_missing_selector,
                                         &matched_except_for_selector);
          }  /* if */
        }  /* if */
        /* Find any non-member function for the operator. */
        if (!must_be_member_function) {
          a_symbol_ptr             normal_sym, proj_normal_sym;
          a_symbol_locator         locator;
          a_type_list_entry_ptr    type_list = NULL;
          a_symbol_list_entry_ptr  symbol_list, slep;
          an_id_lookup_options_set idl_options;
          a_boolean                arg_dep_lookup_done = FALSE;
          /* If the second operand has a template class type, try to
             instantiate it to expose any friend functions it declares. */
          if (!unary_operator && is_class_struct_union_type(operand_2->type)) {
            instantiate_template_class(operand_2->type);
          }  /* if */
          /* Do the normal id lookup on the operator name, e.g., for "+"
             look up "operator +".  Ignore member functions, which were
             covered above. */
          make_opname_locator(kind, &locator, operator_position);
          idl_options = IDL_SKIP_CLASS_SCOPES;
          if (gpp_mode && gnu_version >= 30400 && dependent_call) {
            /* g++ 3.4 has a bug in dependent name lookup that allows
               entities declared after the point of lookup to be found.
               Emulate that. */
            idl_options |= IDL_SUPPRESS_DECL_SEQ_CHECK;
          }  /* if */
          normal_sym = normal_id_lookup(&locator, idl_options);
          proj_normal_sym = locator.specific_symbol;
          if (normal_sym != NULL &&
              !is_function_or_template_symbol(normal_sym)) {
            /* Ignore error symbols and like. */
            normal_sym = NULL;
          }  /* if */
          if (normal_sym == NULL) proj_normal_sym = NULL;
          if (normal_sym != NULL &&
              is_symbol_for_which_arg_dependent_lookup_should_be_suppressed(
                                                                 normal_sym)) {
            /* In certain cases, e.g., block externs, suppress the argument-
               dependent lookup. */
          } else {
            /* Build a list of the argument types, to be used to do
               argument-dependent lookup below. */
            arg_dep_lookup_done = TRUE;
            add_operand_to_arg_dependent_lookup_list(operand_1, &type_list);
            if (!unary_operator) {
              add_operand_to_arg_dependent_lookup_list(operand_2, &type_list);
            }  /* if */
          }  /* if */
          /* Do argument-dependent lookup, producing a list of symbols to
             be considered as candidate functions. */
          symbol_list = argument_dependent_lookup(proj_normal_sym, &locator,
                                                  &type_list);
          for (slep = symbol_list; slep != NULL; slep = slep->next) {
            nonmember_functions_symbol = slep->symbol;
            if (is_template_dependent_context() &&
                is_symbol_for_which_overload_resolution_should_be_deferred(
                                                 nonmember_functions_symbol)) {
              /* A symbol for which we cannot do overload resolution at
                 this time, e.g., a block extern symbol. */
              defer_overload_resolution = TRUE;
              break;
            } else {
              try_overloaded_function_match(
                                         nonmember_functions_symbol,
                                         /*is_template_id=*/FALSE,
                                         (a_template_arg_ptr)NULL,
                                         arg_operand_list,
                                         /*have_selector=*/FALSE,
                                         (an_operand *)NULL,
                                         /*ctor_conversion_case=*/FALSE,
                                         /*initializing_return_value=*/FALSE,
                                         /*effects_copy_initialization=*/FALSE,
                                         /*allow_udc_on_arguments=*/TRUE,
                                         arg_dep_lookup_done,
                                         /*from_arg_dep_lookup=*/
                                                 (slep != symbol_list ||
                                                  nonmember_functions_symbol !=
                                                  normal_sym),
                                         dependent_call,
                                         /*forced_dependent=*/FALSE,
                                         /*known_to_be_visible=*/FALSE,
                                         /*is_overloaded_operator=*/TRUE,
                                         &candidate_functions,
                                         &matched_except_for_missing_selector,
                                         &matched_except_for_selector);
            }  /* if */
          }  /* for */
          free_list_of_symbol_list_entries(symbol_list);
        }  /* if */
        if (sun_mode &&
            some_candidate_matches_without_user_defined_convs(
                                                        candidate_functions)) {
          /* The Sun compiler up to 5.8 seems to not try built-in operator
             matches if it has a user-written candidate that doesn't require
             a user-defined conversion to match.  5.9 seems to have eliminated
             that, but we don't yet have a sun_version option... */
          try_conversions = FALSE;
        }  /* if */
        /* See if the built-in meaning of the operator can apply if we
           convert the class operand(s) to a built-in type through use of
           a conversion function. */
        if (try_conversions) {
          /* See if we can find user-defined conversions to built-in types
             that will make the built-in operator feasible.  The argument
             matches are compared to the best match so far from the above
             searches. */
          try_conversions_for_builtin_operator(kind, unary_operator,
                                               arg_operand_list,
                                               &candidate_functions);
        }  /* if */
select_best_function:
        /* The candidate_functions list now contains all the viable
           functions.  Find the best. */
        select_best_candidate_functions(&candidate_functions,
                                        operator_position,
                                        &undecidable_because_of_error,
                                        &ambiguous);
        function_symbol = NULL;
        arg_expr_list = NULL;
        arg_operand_list_not_used = FALSE;
        if (defer_overload_resolution) {
          /* We're in a prototype or nonreal instantiation, and some
             candidate function was a block extern, so we can't really do
             overload resolution.  Create a generic expression. */
          make_generic_operation_operand(kind, unary_operator,
                                         operand_1, operand_2,
                                         result, operator_position,
                                         operator_tok_seq_number,
                                         operator_position_2);
          check_assertion(!dependent_call);
          if (is_prototype_instantiation_context()) {
            /* Make sure this call is treated as a nondependent call in
               a real instantiation. */
            record_nondependent_call((a_symbol_ptr)NULL,
                                     operator_tok_seq_number,
                                     call_depth);
          }  /* if */
          *processed = TRUE;
        } else if (undecidable_because_of_error) {
          /* There was a previous error. */
          *processed = TRUE;
          arg_operand_list_not_used = TRUE;
          make_error_operand(result);
        } else if (candidate_functions == NULL) {
          /* None of the functions applies. */
          if (has_predef_meaning) {
            /* The operator is one that has a predefined meaning when applied
               to classes (e.g., unary "&").  Leave the operator unprocessed
               and let the caller apply the built-in meaning. */
          } else {
            /* Error: no applicable operator function. */
            *processed = TRUE;
            if (expr_error_should_be_issued()) {
              pos_st_start_error(ec_no_matching_operator_function,
                                 operator_position,
                                 opname_names[(int)kind]);
              display_operand_types(arg_operand_list, kind);
              end_error();
            }  /* if */
            make_error_operand(result);
            arg_operand_list_not_used = TRUE;
          }  /* if */
        } else if (ambiguous) {
          /* More than one function applies and is a best match --
             ambiguity. */
          *processed = TRUE;
#if DEBUG
          if (debug_level >= 4) {
            db_candidate_function_list(candidate_functions);
          }  /* if */
#endif /* DEBUG */
          if (expr_error_should_be_issued()) {
            pos_st_start_error(ec_ambiguous_operator_function,
                               operator_position,
                               opname_names[(int)kind]);
            diagnose_overload_ambiguity(candidate_functions,
                                        (an_operand *)NULL,
                                        arg_operand_list,
                                        kind);
          }  /* if */
          make_error_operand(result);
          arg_operand_list_not_used = TRUE;
        } else {
          /* Exactly one function applies and is best. */
          a_symbol_ptr overloaded_function_symbol;
          proj_function_symbol = candidate_functions->function_symbol;
#if BACK_END_IS_CP_GEN_BE
          found_through_adl = candidate_functions->found_through_adl;
#endif /* BACK_END_IS_CP_GEN_BE */
          arg_match = candidate_functions->arg_matches;
          if (proj_function_symbol == NULL) {
            a_boolean op_1_inside_conditional = FALSE,
                      op_2_inside_conditional = FALSE;
            /* A built-in operator was selected. */
#if DEBUG
            if (debug_level >= 4 || db_flag_is_set("overload")) {
              db_display_overload_level();
              fprintf(f_debug, "check_for_operator_overloading: selected\n");
              db_candidate_function(candidate_functions);
            }  /* if */
#endif /* DEBUG */
            /* *processed is left FALSE so the caller will try the built-in
               meaning. */
            /* Determine if either operand is conditional. */
            if (kind == (an_opname_kind)onk_and_and ||
                kind == (an_opname_kind)onk_or_or) {
              op_2_inside_conditional = TRUE;
            } else if (kind == (an_opname_kind)onk_question) {
              /* Operands 1 and 2 under a "?" here are really the second and
                 third operands. */
              op_1_inside_conditional = TRUE;
              op_2_inside_conditional = TRUE;
            }  /* if */
            /* Convert the operands to the proper types. */
            adjust_operand_for_builtin_operator(operand_1,
                                                candidate_functions, 1,
                                                op_1_inside_conditional,
                                                arg_match);
            if (!unary_operator) {
              adjust_operand_for_builtin_operator(operand_2,
                                                  candidate_functions, 2,
                                                  op_2_inside_conditional,
                                                  arg_match->next);
            }  /* if */
          } else {
            a_boolean bitwise_assignment = FALSE;
            /* An operator function was selected. */
#if DEBUG
            if (debug_level >= 4 || db_flag_is_set("overload")) {
              db_display_overload_level();
              db_symbol(proj_function_symbol,
                        "check_for_operator_overloading: selected ", 4);
            }  /* if */
#endif /* DEBUG */
            *processed = TRUE;
            function_symbol = fundamental_symbol_of(proj_function_symbol);
            routine_type = routine_symbol_type(function_symbol);
            member_is_best_match = 
                       routine_type_is_nonstatic_member_function(routine_type);
            overloaded_function_symbol = member_is_best_match ?
                                                    member_functions_symbol :
                                                    nonmember_functions_symbol;
            if (do_dependent_name_processing &&
                is_prototype_instantiation_context()) {
              /* Record the outcome of overload resolution for a nondependent
                 call in a prototype instantiation.  Dependent calls in such
                 a context don't get here. */
              check_assertion(!dependent_call &&
                              operator_tok_seq_number != 0);
              record_nondependent_call(proj_function_symbol,
                                       operator_tok_seq_number,
                                       call_depth);
            }  /* if */
            /* Check for the builtin operator=. */
            if (kind == (an_opname_kind)onk_assign &&
                function_symbol->kind == (a_symbol_kind)sk_member_function &&
                function_symbol->variant.routine.ptr->
                                                    is_trivial_copy_function) {
              /* This function is the default bitwise copy assignment
                 operator, so generate an assignment instead of a call. */
              a_boolean access_error_reported;
#if DEBUG
              if (debug_level >= 4 || db_flag_is_set("overload")) {
                db_display_overload_level();
                fprintf(f_debug,
                 "check_for_operator_overloading: bitwise operator=\n");
              }  /* if */
#endif /* DEBUG */
              bitwise_assignment = TRUE;
              check_use_of_deleted_function(function_symbol,
                                            /*elided_ref=*/FALSE,
                                            operator_position);
              /* Check access and record the reference (but no call). */
              overloaded_function_catch_up(function_symbol,
                                           overloaded_function_symbol,
                                           (an_operand *)NULL,
                                           operator_position,
                                           /*elided_reference=*/TRUE,
                                           /*result_is_lvalue=*/FALSE,
                                           /*address_taken=*/FALSE,
                                           (an_operand *)NULL,
                                           &access_error_reported);
            }  /* if */
            arg_operand = arg_operand_list;
            bound_function_selector = NULL;
            if (member_is_best_match) {
              /* The function selected is a non-static member function.
                 Therefore, the first argument is to be used as the selector
                 object. */
              bound_function_selector = &arg_operand_list->operand;
              /* Issue any warning about the "this" parameter detected while
                 evaluating the alternatives. */
              issue_warning_from_arg_match_summary(
                                           arg_match,
                                           &bound_function_selector->position);
              /* The "real" argument list starts with the second argument. */
              arg_operand = arg_operand->next;
              arg_match = arg_match->next;
            }  /* if */
            param = routine_type->variant.routine.extra_info->param_type_list;
            if (bitwise_assignment) {
              /* For the bitwise operator= case, generate an assignment instead
                 of a call.  The assignment returns an lvalue. */
              a_type_ptr       result_type = sym_parent_class(function_symbol);
              an_expr_node_ptr assign_node, lhs_node, rhs_node;

              /* Make a pointer for the selector, and adjust its type if
                 necessary. */
              /* coverity[var_deref_model] */
              prep_special_selector_operand(bound_function_selector,
                                            routine_type);
              if (is_an_lvalue(bound_function_selector)) {
                modifying_lvalue(bound_function_selector,
                                 /*value_used=*/FALSE);
              } else if (is_an_rvalue(bound_function_selector)) {
                /* If bound_function_selector is a class rvalue, make an
                   lvalue for it so it can be used as the left operand of
                   the assignment. */
                conv_class_operand_to_object_pointer(bound_function_selector);
                conv_object_pointer_to_lvalue(bound_function_selector);
              }  /* if */
              /* Cast the source operand to the right type. */
              prep_assignment_operand(&arg_operand->operand,
                                      result_type,
                                      ec_incompatible_param,
                                      operator_position);
              rhs_node = make_node_from_operand(&arg_operand->operand);
              lhs_node = make_node_from_operand(bound_function_selector);
              lhs_node->next = rhs_node;
              assign_node = make_lvalue_operator_node(
                                            (an_expr_operator_kind)eok_assign,
                                            lhs_node->type, lhs_node);
              assign_node->variant.operation.
                                 returns_lvalue_instead_of_usual_rvalue = TRUE;
              make_lvalue_expression_operand(assign_node, result);
              /* Note that reference_to_implicitly_invoked_function is not
                 called. */
            } else {
              /* Not the builtin bitwise operator=. */
              /* Build an expression-form argument list.  Convert the arguments
                 on the argument list to the right types.  Note that for the
                 member function case we start at the second operand. */
              arg_expr_list = end_arg_expr_list = NULL;
              for (; arg_operand != NULL;
                   arg_operand = arg_operand->next,
                        arg_match = arg_match->next) {
                arg = node_for_arg_of_overloaded_function_call(
                                         arg_operand, arg_match, param,
                                         function_symbol->variant.routine.ptr);
                if (arg_expr_list == NULL) {
                  arg_expr_list = arg;
                } else {
                  end_arg_expr_list->next = arg;
                }  /* if */
                end_arg_expr_list = arg;
                /* Advance to the next parameter unless we're at an
                   ellipsis. */
                if (param != NULL) param = param->next;
              }  /* for */
              have_selector = member_is_best_match;
              if (have_selector) {
                change_refs_on_selector(routine_type, bound_function_selector);
              }  /* if */
              /* Do the things that would have been done to the symbol but
                 weren't because the specific symbol was not known, and build
                 an operand for the function. */
              make_resolved_overloaded_function_operand(
                                          proj_function_symbol,
                                          overloaded_function_symbol,
                                          (an_operand *)NULL,
                                          operator_position,
                                          &have_selector,
                                          bound_function_selector,
                                          /*is_property=*/FALSE,
                                          &function_operand);
              /* Make the call node and an operand for it. */
              assemble_function_call(&function_operand,
                                     bound_function_selector,
                                     arg_expr_list,
                                     /*compiler_generated=*/TRUE,
                                     /*arg_dep_lookup_suppressed=*/FALSE,
                                     /*qualified_function_name=*/FALSE,
                                     found_through_adl,
                                     /*uses_operator_syntax=*/TRUE,
                                     operator_position, result,
                                     (an_expr_node_ptr *)NULL);
            }  /* if */
          }  /* if */
        }  /* if */
        /* Free the candidate functions list. */
        free_candidate_function_list(candidate_functions);
        if (arg_operand_list_not_used) {
          /* There was an error and the arg_operand list was not used,
             so change all the references in it to error references. */
          change_arg_operand_list_refs_to_error(arg_operand_list);
        }  /* if */
        /* Free the argument list. */
        free_arg_operand_list(arg_operand_list);
      }  /* if */
    }  /* if */
  }  /* if */
  /* If an operand was created, put the right position in it. */
  if (*processed) {
    result->position = *operator_position;
    rule_out_expr_kinds(ROEK_CONSTANT, result);
  }  /* if */
#if DEBUG
  if (debug_level >= 4 || db_flag_is_set("overload")) {
    db_display_overload_level();
    fprintf(f_debug, "Leaving check_for_operator_overloading\n");
  }  /* if */
  overload_level--;
#endif /* DEBUG */
  db_exit();
}  /* check_for_operator_overloading */


a_boolean conversion_to_class_possible(
                          an_operand               *source_operand,
                          a_type_ptr               dest_type,
                          a_boolean                try_bitwise_copy,
                          a_boolean                initializing_return_value,
                          a_boolean                is_copy_initialization,
                          a_boolean                orig_is_copy_initialization,
                          a_boolean                is_reference_binding,
                          a_conv_descr             *conversion,
                          a_conv_descr             *ctor_arg_conversion,
                          a_boolean                *ambiguous,
                          a_candidate_function_ptr *ambiguity_list)
/*
If source_operand can be converted to the class type dest_type (via a
constructor, conversion function, or bitwise copy) set *conversion
to describe the conversion and return TRUE.  Otherwise, return FALSE.
The result is always an rvalue.  Bitwise copies are considered if
try_bitwise_copy is TRUE.  initializing_return_value is TRUE if the
initialization is being done to return a value in a return statement.
If is_copy_initialization is TRUE, the initialization is
copy-initialization ("="-form initialization); if FALSE, it's
direct-initialization ("()"-form initialization).  User-defined
conversions on constructor arguments are considered only for
direct-initialization.  In some cases, the caller has rewritten
a copy-initialization as a direct-initialization; in those cases,
orig_is_copy_initialization indicates whether the original initialization
was copy-initialization (this controls whether explicit constructors
are considered).  If is_reference_binding is TRUE, the result
will be bound to a reference, so also consider conversions to
derived classes of dest_type.  If ctor_arg_conversion is non-NULL,
return a description of the conversion to be done on the constructor
argument in *ctor_arg_conversion.  If more than one function matches,
set *ambiguous to TRUE and return FALSE.  If ambiguity_list is
non-NULL in that case, it is set to point to a list describing the set
of ambiguous functions; the caller must free that list.
*ambiguity_list is set to NULL to indicate a case that is undecidable
because of an error.  This routine is used only in C++ mode.
*/
{
  a_boolean                     okay, bitwise_copy_okay;
  a_boolean                     cctor_is_bitwise_copy;
  a_type_ptr                    class_type, source_type;
  a_candidate_function_ptr      candidate_functions;
  a_boolean                     matched_except_for_missing_selector = FALSE;
  a_boolean                     matched_except_for_selector = FALSE;
  a_boolean                     source_is_class, type_is_same;
  a_boolean                     type_is_same_or_derived;
  a_boolean                     adjusted_is_copy_initialization =
                                                        is_copy_initialization;
  a_boolean                     copy_initialization_done_as_direct = FALSE;
  a_boolean                     try_conversion_functions;
  a_boolean                     try_as_arg_of_bitwise_cctor;
  a_symbol_ptr                  class_symbol, constructor_symbol;
  a_class_symbol_supplement_ptr cssp;
  an_arg_operand_ptr            arg_operand_list;
  a_boolean                     undecidable_because_of_error;
  a_boolean                     ctor_arg_conversion_set = FALSE;
  a_base_class_ptr              bcp;
  a_type_qualifier_set          source_qualifiers;

  /* Note that this routine is like a simplified version of
     select_overloaded_function that works for user-defined conversion
     functions (no arguments, just a "this" parameter). */
  db_enter(4, "conversion_to_class_possible");
#if DEBUG
  overload_level++;
  if (debug_level >= 4 || db_flag_is_set("overload")) {
    db_display_overload_level();
    fprintf(f_debug, "Entering conversion_to_class_possible, dest_type = ");
    db_abbreviated_type(dest_type);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  *ambiguous = FALSE;
  okay = FALSE;
  clear_conv_descr(conversion);
  class_type = skip_typerefs(dest_type);
  /* If the class is a template class, instantiate it so that its
     constructors are visible. */
  instantiate_template_class(class_type);
  class_symbol = (a_symbol_ptr)(class_type->source_corresp.assoc_info);
  cssp = class_symbol->variant.class_struct_union.extra_info;
  source_type = source_operand->type;
  source_qualifiers = get_type_qualifiers(source_type);
  source_type = skip_typerefs(source_type);
  /* candidate_functions will contain the list of viable functions. */
  candidate_functions = NULL;
  /* Look for a relationship between the source and destination type. */
  type_is_same = identical_types(source_type, class_type);
  source_is_class = is_class_struct_union_type(source_type);
  bcp = (source_is_class && !type_is_same) ?
                            find_base_class_of(source_type, class_type) : NULL;
  type_is_same_or_derived = type_is_same || bcp != NULL;
  if (is_copy_initialization && type_is_same_or_derived) {
    /* Copy-initialization from the same class type or a derived class
       thereof is treated as direct-initialization.  See [dcl.init].
       This is strange, but the definition of auto_ptr depends on it. */
    adjusted_is_copy_initialization = FALSE;
    copy_initialization_done_as_direct = TRUE;
  }  /* if */
  /* Check for a same-class bitwise copy.  The derived-class bitwise copy
     is checked for below.  A bitwise copy cannot be done if the source
     has a volatile type (the generated notional bitwise copy constructor
     has a reference-to-const parameter and cannot copy a volatile object).
     Note that bitwise_copy_okay is TRUE only when no overload resolution
     is required in order to decide how to do the copy, and also only when
     there is no symbol for the copy constructor so that access checking and
     similar checks need not be done. */
  cctor_is_bitwise_copy = (cssp->construction_by_bitwise_copy_allowed &&
                           cssp->constructor == NULL);
  bitwise_copy_okay = try_bitwise_copy &&
                      cctor_is_bitwise_copy &&
                      !any_qualifier_in_set_missing(TQ_CONST, /*lint --e(845)*/
                                                    source_qualifiers);
  if (bitwise_copy_okay && type_is_same) {
    /* The source and destination types are the same class type, and a
       bitwise copy is allowed on that type.  That means there are no
       copy constructors, and therefore the bitwise copy is the best
       match. */
    conversion->class_identity_or_bitwise_copy = TRUE;
    okay = TRUE;
  } else if (is_template_dependent_context() &&
             (class_type->variant.class_struct_union.is_nonreal_class ||
              operand_is_dependent(source_operand))) {
    /* Assume we can convert to or from an unknown type in a prototype
       instantiation. */
    okay = TRUE;
    conversion->unknown_dependent_conversion = TRUE;
  } else {
    /* A same-class bitwise copy is not possible, so do the full overload
       resolution. */
    /* Make an argument list with just the source operand. */
    arg_operand_list = alloc_arg_operand();
    copy_operand(source_operand, &arg_operand_list->operand);
    constructor_symbol = cssp->constructor;
    if (constructor_symbol != NULL) {
      /* The class has constructors. */
      /* Try all the constructors with that argument list. */
      try_overloaded_function_match(constructor_symbol,
                                    /*is_template_id=*/FALSE,
                                    (a_template_arg_ptr)NULL,
                                    arg_operand_list,
                                    /*have_selector=*/FALSE, /* sic */
                                    (an_operand *)NULL,
                                    /*ctor_conversion_case=*/TRUE,
                                    initializing_return_value,
                                    /*effects_copy_initialization=*/
                                                   orig_is_copy_initialization,
                                    /*allow_udc_on_arguments=*/
                                              !adjusted_is_copy_initialization,
                                    /*arg_dep_lookup_done=*/FALSE,
                                    /*from_arg_dep_lookup=*/FALSE,
                                    /*dependent_call=*/FALSE,
                                    /*forced_dependent=*/FALSE,
                                    /*known_to_be_visible=*/FALSE,
                                    /*is_overloaded_operator=*/FALSE,
                                    &candidate_functions,
                                    &matched_except_for_missing_selector,
                                    &matched_except_for_selector);
    }  /* if */
    /* Determine whether conversion functions should be tried. */
    try_conversion_functions = FALSE;
    try_as_arg_of_bitwise_cctor = FALSE;
    if (!source_is_class) {
      /* Do not try conversion functions when the source is not a class. */
    } else if (type_is_same_or_derived) {
      /* Do not try conversion functions for a derived-to-base conversion. */
    } else if (adjusted_is_copy_initialization) {
      /* Try conversion functions for copy-initialization. */
      try_conversion_functions = TRUE;
    } else if (is_reference_binding) {
      /* Try conversion functions when converting to bind to a reference. */
      try_conversion_functions = TRUE;
    } else {
      /* Direct-initialization. */
      if (constructor_symbol == NULL && cctor_is_bitwise_copy) {
        /* Direct-initialization really only tries constructors, but if
           the only constructor is an implicit bitwise copy constructor
           check conversion functions also (conceptually, the result of
           calling the conversion function is the argument to the
           copy constructor). */
        try_conversion_functions = TRUE;
        if (any_cfront_mode()) {
          /* Cfront does this in a nonstandard way.  Old Sun compilers (5.0)
             did too, but newer ones have that fixed. */
        } else {
          try_as_arg_of_bitwise_cctor = TRUE;
        }  /* if */
      } else if (any_cfront_mode()) {
        /* In a departure from the standard, look for a conversion function
           that converts to exactly the required type.  That makes sense
           because in that case the copy constructor call can be elided
           and we call just one user-defined conversion rather than two. */
        try_conversion_functions = TRUE;
      }  /* if */
    }  /* if */
    if (try_conversion_functions) {
      /* Try to convert to the destination type by using conversion
         functions. */
      /* If the source type is a template class, instantiate it to make its
         conversion functions visible. */
      instantiate_template_class(source_type);
      if (cssp->target_of_conversion_function ||
          symbol_supplement_for_class(source_type)->conversion_template_list !=
                                                                        NULL) {
        /* There is at least one conversion function that converts some other
           class into the destination class, or the source class has template
           conversion functions.  See if there is a conversion function that
           does the job. */
        a_type_ptr eff_dest_type = dest_type;
        a_boolean  eff_is_copy_initialization= adjusted_is_copy_initialization;
        a_boolean  eff_is_reference_binding = is_reference_binding;
        if (try_as_arg_of_bitwise_cctor) {
          /* On an initialization of a class type whose "copy constructor"
             is a bitwise copy, the operand being examined is really the
             argument for the copy constructor, so allow conversions that
             produce something that can be bound to reference to const
             class_type. */
          eff_dest_type = make_qualified_type(class_type, TQ_CONST);
          eff_is_copy_initialization = FALSE;
          eff_is_reference_binding = TRUE;
        }  /* if */
        try_conversion_function_match(source_operand, eff_dest_type,
                                      dest_type,
                                      (a_builtin_type_kind_set)BTK_NONE,
                                      /*need_lvalue_result=*/FALSE,
                                      eff_is_copy_initialization,
                                      eff_is_reference_binding,
                                      &candidate_functions);
      }  /* if */
    }  /* if */
    /* If no functions are viable, check for the possibility of a bitwise
       copy from a derived class to a base class. */
    if (candidate_functions == NULL && bitwise_copy_okay && bcp != NULL &&
        /* Watch out for the case where the source type's definition
           has been partially processed -- we know that the destination
           type is a base class, but the source class is still
           incomplete, and one can't make an rvalue of an
           incomplete type. */
        /* instantiate_template_class need not be called here, because
           find_base_class_of has that effect. */
        !is_incomplete_type(source_type)) {
      /* Yes, this is a bitwise copy from a derived class to a base class. */
      conversion->class_identity_or_bitwise_copy = TRUE;
      conversion->std.cast_base_class = bcp;
      conversion->std.nontrivial_conversion = TRUE;
      okay = TRUE;
    }  else {
      /* The candidate_functions list now contains all the viable functions.
         Find the best ones. */
      select_best_candidate_functions(&candidate_functions,
                                      &source_operand->position,
                                      &undecidable_because_of_error,
                                      ambiguous);
      if (undecidable_because_of_error) {
        /* Note that candidate_functions is NULL
           (select_best_candidate_functions returns it that way in this case),
           so a NULL ambiguity_list will be returned to indicate "undecidable
           because of error".  *ambiguous is also set to TRUE. */
      } else if (candidate_functions == NULL) {
        /* No constructor or conversion function is suitable. */
      } else if (*ambiguous) {
        /* More than one constructor or conversion function matches at the same
           level.  Ambiguity. */
#if DEBUG
        if (debug_level >= 4) {
          db_candidate_function_list(candidate_functions);
        }  /* if */
#endif /* DEBUG */
      } else {
        /* Exactly one constructor or conversion function matches best. */
        okay = TRUE;
        /* Return information on how the conversion is to be done. */
        *conversion = candidate_functions->conversion;
        /* If this is a constructor call and the caller wants it, return
           also information on any conversion required for the argument
           (it might involve a user-defined conversion in the explicit
           cast case). */
        if (ctor_arg_conversion != NULL &&
            candidate_functions->conversion.routine->special_kind ==
                                    (a_special_function_kind)sfk_constructor) {
          check_assertion(
                 candidate_functions->arg_matches != NULL);  /* For Coverity */
          *ctor_arg_conversion = candidate_functions->arg_matches->conversion;
          ctor_arg_conversion_set = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
    free_arg_operand_list(arg_operand_list);
  }  /* if */
  if (*ambiguous) conversion->unusable = TRUE;
  if (*ambiguous && ambiguity_list != NULL) {
    /* Return the candidate functions list to the caller, for use in
       generating an ambiguity error.  The caller will free the list. */
    *ambiguity_list = candidate_functions;
  } else {
    /* Free the candidate functions list. */
    free_candidate_function_list(candidate_functions);
  }  /* if */
  if (okay && copy_initialization_done_as_direct) {
    conversion->copy_initialization_done_as_direct = TRUE;
  }  /* if */
  if (ctor_arg_conversion != NULL && !ctor_arg_conversion_set) {
    clear_conv_descr(ctor_arg_conversion);
  }  /* if */
#if DEBUG
  if (debug_level >= 4 || db_flag_is_set("overload")) {
    db_display_overload_level();
    fprintf(f_debug, "Leaving conversion_to_class_possible: %s\n",
                     okay ? "okay" : "not okay");
  }  /* if */
  overload_level--;
#endif /* DEBUG */
  db_exit();
  return okay;
}  /* conversion_to_class_possible */


a_boolean conversion_from_class_possible(
                            an_operand               *source_operand,
                            a_type_ptr               dest_type,
                            a_builtin_type_kind_set  builtin_types_allowed,
                            a_boolean                need_lvalue_result,
                            a_boolean                is_copy_initialization,
                            a_boolean                is_reference_binding,
                            a_conv_descr             *conversion,
                            a_boolean                *ambiguous,
                            a_candidate_function_ptr *ambiguity_list)
/*
If the class operand source_operand can be converted by a conversion function
to either

(a) dest_type, if dest_type is non-NULL, and an lvalue of that type if
    need_lvalue_result is TRUE, or
(b) a built-in type in the set given by builtin_types_allowed, if
    builtin_types_allowed != BTK_NONE, and a non-const lvalue of that
    type if need_lvalue_result is TRUE (if both (a) and (b) apply,
    (b) takes precedence, and dest_type is used only to guide the
    selection of template conversion functions).

then set *conversion to describe the conversion, and return TRUE.
Otherwise return FALSE.  If more than one function matches, set
*ambiguous to TRUE and return FALSE.  If ambiguity_list is non-NULL in
that case, it is set to point to a list describing the set of ambiguous
functions; the caller must free that list.  *ambiguity_list is set to
NULL to indicate a case that is undecidable because of an error.
If is_reference_binding is TRUE, the result will be bound directly to
a reference, so consider conversions to a derived class of dest_type,
and allow appropriate cv-qualification adjustments, but do not
consider standard conversions after the conversion function; otherwise,
allow standard conversions on the result.  If is_copy_initialization
is TRUE, the result will be copied for a copy-initialization.
Note that this routine does not look for constructors that can be
used as conversion functions or for the possibility of bitwise copying
(see conversion_to_class_possible).  This routine is used only in
C++ mode.
*/
{
  a_boolean                okay;
  a_candidate_function_ptr candidate_functions;
  a_boolean                undecidable_because_of_error;

  /* This routine is similar to select_overloaded_function. */
  db_enter(4, "conversion_from_class_possible");
#if DEBUG
  overload_level++;
  if (debug_level >= 4 || db_flag_is_set("overload")) {
    db_display_overload_level();
    fprintf(f_debug,"Entering conversion_from_class_possible, dest_type = ");
    db_abbreviated_type(dest_type);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  clear_conv_descr(conversion);
  if (is_template_dependent_context() &&
      (f_skip_typerefs(source_operand->type)->
                                 variant.class_struct_union.is_nonreal_class ||
       (dest_type != NULL && is_instantiation_dependent_type(dest_type)))) {
    /* Assume a conversion to or from an unknown type in a prototype
       instantiation is allowed. */
    okay = TRUE;
    conversion->unknown_dependent_conversion = TRUE;
  } else {
    candidate_functions = NULL;
    /* Find any viable conversion functions. */
    /* coverity[deref_ptr_in_call] */  /* Coverity bug. */
    try_conversion_function_match(source_operand, dest_type, dest_type,
                                  builtin_types_allowed, need_lvalue_result,
                                  is_copy_initialization,
                                  is_reference_binding,
                                  &candidate_functions);
    /* Of the viable functions, select the best. */
    select_best_candidate_functions(&candidate_functions,
                                    &source_operand->position,
                                    &undecidable_because_of_error,
                                    ambiguous);
    okay = FALSE;
    if (undecidable_because_of_error) {
      /* Note that candidate_functions is NULL (select_best_candidate_functions
         returns it that way in this case), so a NULL ambiguity_list will
         be returned to indicate "undecidable because of error".  *ambiguous
         is also set to TRUE. */
    } else if (candidate_functions == NULL) {
      /* There are no viable conversion functions. */
    } else if (*ambiguous) {
      /* There are several equally desirable functions. */
#if DEBUG
      if (debug_level >= 4) {
        db_candidate_function_list(candidate_functions);
      }  /* if */
#endif /* DEBUG */
    } else {
      /* There is exactly one best conversion function. */
      okay = TRUE;
      /* Return information on how the conversion is to be done. */
      *conversion = candidate_functions->conversion;
    }  /* if */
    if (*ambiguous) conversion->unusable = TRUE;
    if (*ambiguous && ambiguity_list != NULL) {
      /* Return the candidate functions list to the caller, for use in
         generating an ambiguity error.  The caller will free the list. */
      *ambiguity_list = candidate_functions;
    } else {
      /* Free the candidate functions list. */
      free_candidate_function_list(candidate_functions);
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 4 || db_flag_is_set("overload")) {
    db_display_overload_level();
    fprintf(f_debug, "Leaving conversion_from_class_possible: %s\n",
                     okay ? "okay" : "not okay");
  }  /* if */
  overload_level--;
#endif /* DEBUG */
  db_exit();
  return okay;
}  /* conversion_from_class_possible */


void try_to_convert_class_operand_to_builtin_type(
                                 an_operand              *operand,
                                 a_builtin_type_kind_set builtin_types_allowed,
                                 a_boolean               *processed)
/*
If *operand has a class type, see if it can be converted (via a conversion
function) to a built-in type of the set allowed by builtin_types_allowed.
If so, convert it and set *processed to TRUE.  The result is always an rvalue.
Issue an error and set *processed to TRUE if the conversion is ambiguous.
*/
{
  a_conv_descr             conversion;
  a_boolean                ambiguous;
  a_candidate_function_ptr ambiguity_list;

  /* Only look at this operand if it has a class type. */
  if (is_class_struct_union_type(operand->type)) {
    /* See if the class type can be converted to an acceptable built-in
       type. */
    if (conversion_from_class_possible(operand, (a_type_ptr)NULL,
                                       builtin_types_allowed,
                                       /*need_lvalue_result=*/FALSE,
                                       /*is_copy_initialization=*/TRUE,
                                       /*is_reference_binding=*/FALSE,
                                       &conversion,
                                       &ambiguous, &ambiguity_list)) {
      /* The conversion is possible -- do it. */
      /* Force the result to be an rvalue. */
      conversion.result_is_an_lvalue = FALSE;
      user_convert_operand(operand, /*dest_type=*/(a_type_ptr)NULL,
                           &conversion, (a_conv_descr *)NULL,
                           /*force_copy_to_temp=*/FALSE);
      *processed = TRUE;
    } else if (ambiguous) {
      /* There is more than one possible conversion to a built-in type. */
      /* A NULL ambiguity_list indicates a case that was undecidable because
         of an error (no additional error is needed). */
      if (ambiguity_list != NULL) {
        if (expr_error_should_be_issued()) {
          pos_ty_start_error(ec_ambiguous_conversion_to_builtin,
                             &operand->position, operand->type);
          diagnose_overload_ambiguity(ambiguity_list,
                                      (an_operand *)NULL,
                                      (an_arg_operand_ptr)NULL,
                                      (an_opname_kind)onk_none);
        }  /* if */
        free_candidate_function_list(ambiguity_list);
      }  /* if */
      conv_to_error_operand(operand);
      *processed = TRUE;
    }  /* if */
  }  /* if */
}  /* try_to_convert_class_operand_to_builtin_type */


a_boolean user_defined_conversion_possible(
                                      an_operand   *source_operand,
                                      a_type_ptr   dest_type,
                                      a_boolean    need_lvalue_result,
                                      a_boolean    initializing_return_value,
                                      a_boolean    is_copy_initialization,
                                      a_boolean    orig_is_copy_initialization,
                                      a_boolean    is_reference_binding,
                                      a_conv_descr *conversion,
                                      a_conv_descr *ctor_arg_conversion,
                                      a_boolean    *failed)
/*
Check whether or not the source operand can be converted to the
destination type by a user-defined conversion (constructor or
conversion routine) in an initialization.  If so, set *conversion
to describe the conversion and return TRUE.  If not, return FALSE.  If
a user-defined conversion is the only hope of converting the source
operand to the destination type (i.e., one or the other has a class
type), and no conversion was found, issue an error, change
source_operand to an error operand, set *failed to TRUE, and return
FALSE.  need_lvalue_result is TRUE if the result is required to be
an lvalue; otherwise, the result can be an lvalue or an rvalue.
initializing_return_value is TRUE if the initialization is being
done to return a value in a return statement.
If is_copy_initialization is TRUE, this is copy-initialization
("="-form initialization); if FALSE, it's direct-initialization
("()"-form initialization).  User-defined conversions on constructor
arguments are considered only for direct-initialization.  In some
cases, the caller has rewritten a copy-initialization as a
direct-initialization; in those cases, orig_is_copy_initialization
indicates whether the original initialization was copy-initialization
(this controls whether explicit constructors are considered).  If
is_reference_binding is TRUE, the result will be bound to a reference,
so also consider conversions to derived classes of dest_type.
If ctor_arg_conversion is non-NULL, return a description of the
conversion to be done on the constructor argument in
*ctor_arg_conversion.  Note that this routine should only be called
when the conversion must be done, not when we're just wondering if it
can be done, because it issues errors.  See 12.3 in the ARM.  This
routine is only called in C++ mode.  The destination type must not be
a reference type (the caller should have rewritten that case).
*/
{
  a_boolean                okay = FALSE, ambiguous;
  a_boolean                single_type_message = FALSE;
  a_type_ptr               source_type, diag_dest_type = dest_type, class_type;
  an_error_code            err_code;
  a_candidate_function_ptr ambiguity_list;

  *failed = FALSE;
  clear_conv_descr(conversion);
#if CHECKING
  if (is_reference_type(dest_type)) {
    internal_error("user_defined_conversion_possible: dest_type is reference");
  }  /* if */
#endif /* CHECKING */
  source_type = source_operand->type;
  if (!need_lvalue_result && is_class_struct_union_type(dest_type)) {
    /* The destination type is a class. */
    /* We don't try this if we need an lvalue result, because constructors
       don't yield lvalues.  If a conversion function applies it will
       be picked up below. */
    if (conversion_to_class_possible(source_operand, dest_type,
                                     /*try_bitwise_copy=*/TRUE,
                                     initializing_return_value,
                                     is_copy_initialization,
                                     orig_is_copy_initialization,
                                     is_reference_binding,
                                     conversion, ctor_arg_conversion,
                                     &ambiguous, &ambiguity_list)) {
      /* A user-defined conversion (constructor or conversion function) or
         bitwise copy is available to convert to the destination type. */
      okay = TRUE;
    } else {
      /* The conversion is not possible. */
      *failed = TRUE;
      class_type = f_skip_typerefs(dest_type);
      /* Pick the right error code. */
      if (is_class_struct_union_type(source_type)) {
        /* Both the source and destination types are classes. */
        a_type_ptr unqual_source_type = f_skip_typerefs(source_type);
        if (types_are_compatible(class_type, unqual_source_type)) {
          /* This is a copy constructor case. */
          err_code = ambiguous ? ec_ambiguous_copy_constructor :
                                 ec_no_suitable_copy_constructor;
          single_type_message = TRUE;
        } else {
          /* The source and destination types are different classes, so the
             message should indicate that both constructors and conversion
             functions were considered. */
          err_code = ambiguous ? ec_ambiguous_user_defined_conversion :
                                 ec_no_user_defined_conversion;
        }  /* if */
      } else {
        /* The source type is a non-class and the destination type is a class,
           so the message should indicate that constructors were considered. */
        err_code = ambiguous ? ec_ambiguous_constructor_for_conversion :
                               ec_no_constructor_for_conversion;
        /* Drop type qualifiers on the destination type because a constructor
           conversion is really to the unqualified type.  Having "const"
           in the diagnostic because of a copy constructor can be confusing. */
        diag_dest_type = skip_typerefs(dest_type);
      }  /* if */
    }  /* if */
  } else if (is_class_struct_union_type(source_type)) {
    /* The source type is a class (and the destination type is not a class,
       or we want an lvalue result). */
    if (conversion_from_class_possible(source_operand, dest_type,
                                       (a_builtin_type_kind_set)BTK_NONE,
                                       need_lvalue_result,
                                       is_copy_initialization,
                                       is_reference_binding,
                                       conversion,
                                       &ambiguous, &ambiguity_list)) {
      /* There is a conversion function that converts from the source class
         type to the destination type. */
      okay = TRUE;
    } else {
      *failed = TRUE;
      /* Pick the right error code. */
      err_code = ambiguous ? ec_ambiguous_conversion_function :
                             ec_no_conversion_function;
    }  /* if */
  } else if (is_template_param_type(source_type) ||
             is_template_param_type(dest_type)) {
    /* A template parameter type might be a class type.  Assume the conversion
       is possible. */
    okay = TRUE;
    conversion->unknown_dependent_conversion = TRUE;
  }  /* if */
  if (*failed) {
    /* The conversion failed. */
    if (!ambiguous) {
      /* No conversion applies. */
      if (is_error_type(dest_type) || is_error_type(source_type)) {
        /* Some previous error. */
      } else if (is_incomplete_type(dest_type) &&
                 is_class_struct_union_type(dest_type)) {
        /* Conversion to an incomplete class type is not possible (in this
           case, anyway).  Use a different message for clarity. */
        if (expr_error_should_be_issued()) {
          pos_ty_error(ec_converting_to_incomplete_class,
                       &source_operand->position, diag_dest_type);
        }  /* if */
      } else {
        /* Put out the usual message (which has already been chosen to
           describe the problem). */
        if (expr_error_should_be_issued()) {
          if (single_type_message) {
            /* Single-type case. */
            pos_ty_error(err_code, &source_operand->position, class_type);
          } else {
            /* Normal double-type case. */
            type2_error_in_operand(err_code, source_operand,
                                   source_type, diag_dest_type);
          }  /* if */
        }  /* if */
      }  /* if */
    } else {
      /* More than one conversion applies (ambiguity). */
      /* A NULL ambiguity_list indicates a case that was undecidable because
         of an error (no additional error is needed). */
      if (ambiguity_list != NULL) {
        if (expr_error_should_be_issued()) {
          if (single_type_message) {
            pos_ty_start_error(err_code, &source_operand->position,
                               class_type);
          } else {
            pos_ty2_start_error(err_code, &source_operand->position,
                                source_type, diag_dest_type);
          }  /* if */
          diagnose_overload_ambiguity(ambiguity_list,
                                      (an_operand *)NULL,
                                      (an_arg_operand_ptr)NULL,
                                      (an_opname_kind)onk_none);
        }  /* if */
        free_candidate_function_list(ambiguity_list);
      }  /* if */
    }  /* if */
    conv_to_error_operand(source_operand);
  }  /* if */
  return okay;
}  /* user_defined_conversion_possible */


static void issue_any_conversion_diagnostics(a_std_conv_descr_ptr std_conv,
                                             a_conv_descr_ptr     conversion,
                                             a_source_position    *err_pos,
                                             a_type_ptr           source_type,
                                             a_type_ptr           dest_type)
/*
Issue diagnostics indicated by the conversion std_conv from
source_type to dest_type.
*/
{
  if (std_conv->exception_spec_incompatibility) {
    /* In assignments and initializations, exception specifications
       under pointers-to-functions and pointers-to-member-functions
       must obey certain rules, but they don't in this case.  (GNU
       compilers don't diagnose this: We issue a warning when emulating
       those compilers.) */
    expr_pos_diagnostic(gpp_mode ? es_warning : es_error,
                        ec_incompatible_exception_specs, err_pos);
  }  /* if */
  /* Warn on oddball conversions. */
  if (std_conv->warning_suggested != ec_no_error) {
    /* The "opt_ty2" routine puts in the types if the specific error
       message has fill-ins for them, and otherwise ignores the types. */
    if (expr_diagnostic_should_be_issued(es_warning,
                                         std_conv->warning_suggested)) {
      pos_opt_ty2_warning(std_conv->warning_suggested, err_pos,
                          source_type, dest_type);
    }  /* if */
    conversion->std.warning_suggested = ec_no_error;
  }  /* if */
}  /* issue_any_conversion_diagnostics */


#if !GNU_EXTENSIONS_ALLOWED
/*ARGSUSED*/ /* <-- is_transparent is only used if GNU extensions are
                    allowed. */
#endif /* !GNU_EXTENSIONS_ALLOWED */
static a_boolean conversion_possible(
                                 an_operand        *source_operand,
                                 a_type_ptr        dest_type,
                                 a_boolean         *is_transparent,
                                 a_type_ptr        orig_dest_type,
                                 a_boolean         need_lvalue_result,
                                 a_boolean         initializing_return_value,
                                 a_boolean         is_copy_initialization,
                                 a_boolean         orig_is_copy_initialization,
                                 a_boolean         is_reference_binding,
                                 an_error_code     incompatible_err,
                                 a_source_position *err_pos,
                                 a_conv_descr      *conversion,
                                 a_conv_descr      *ctor_arg_conversion)
/*
Check whether or not the source operand can be converted to the
destination type, implicitly in an initialization.  If so, set
*conversion to describe the conversion (and if ctor_arg_conversion
is non-NULL, set *ctor_arg_conversion to describe the conversion on
the argument of a constructor, if applicable), and return TRUE.  If not,
issue the error incompatible_err at the position err_pos, change the
operand to an error operand, and return FALSE.  The result of the
conversion must be an lvalue if need_lvalue_result is TRUE.
initializing_return_value is TRUE if the initialization is being done
to return a value in a return statement.  If is_copy_initialization
is TRUE, this is copy-initialization ("="-form); otherwise,
it's direct-initialization ("()"-form).  See
user_defined_conversion_possible for orig_is_copy_initialization.  If
is_reference_binding is TRUE, the result will be bound to a reference,
so also consider conversions to derived classes of dest_type.
Note that this routine should only be called when the conversion must
be done, not when we're just wondering if it can be done, because it
does operand transformations on source_operand and issues errors.  The
destination type must not be a reference type (the caller should have
rewritten that case).  orig_dest_type is the original destination type
(not rewritten) for use in error messages.  If *is_transparent is TRUE
the destination is a transparent union parameter (a GNU C extension).
If it is non-NULL (but FALSE), then the operand is a parameter -- but
not one that is explicitly marked transparent.  If is_transparent is
NULL, the operand is not a parameter.
*/
{
  a_boolean          okay = FALSE, failed = FALSE, ambiguous;
  a_type_ptr         source_type;
  a_std_conv_descr   std_conv;
  an_arg_match_level match_level;

  db_enter(4, "conversion_possible");
  clear_conv_descr(conversion);
#if CHECKING
  if (is_reference_type(dest_type)) {
    internal_error("conversion_possible: dest_type is reference");
  }  /* if */
#endif /* CHECKING */
  if (!C_mode() && !curr_expr_kind_is_const() &&
      user_defined_conversion_possible(source_operand, dest_type,
                                       need_lvalue_result,
                                       initializing_return_value,
                                       is_copy_initialization,
                                       orig_is_copy_initialization,
                                       is_reference_binding,
                                       conversion, ctor_arg_conversion,
                                       &failed)) {
    /* A user-defined conversion can be done. */
    okay = TRUE;
  } else if (!failed) {
    a_boolean      source_is_constant;
    a_constant_ptr source_constant;

    /* No user-defined conversion applies. */
    /* Do the lvalue --> rvalue transformation et al. */
    do_operand_transformations(source_operand,
                               TOPT_SUPPRESS_CHECK_FOR_INDEFINITE_FUNCTION);
    /* Note that the source type is extracted after the lvalue to
       rvalue transformation. */
    source_type = source_operand->type;
    source_is_constant = is_constant_operand(source_operand);
    source_constant = &source_operand->variant.constant;
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode && !source_is_constant) {
      /* Microsoft mode allows some expressions as null pointer constants. */
      adjust_constant_operand_info_for_microsoft_null_pointer_test(
                                                      source_operand,
                                                      &source_is_constant,
                                                      &source_constant,
                                                      (an_expr_node **)NULL);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (is_indefinite_function_operand(source_operand)) {
      /* The source is an indefinite function, i.e., the address of an
         overloaded function.  It can be converted to an appropriate
         pointer or pointer-to-member type (WP [over.over], ARM 13.3). */
      a_boolean unknown_dependent_function;

      if (find_addr_of_overloaded_function_match(
                                           source_operand->symbol,
                                           (a_boolean)source_operand->
                                                           is_template_id,
                                           source_operand->template_arg_list,
                                           is_a_function_designator(
                                                               source_operand),
                                           dest_type,
                                           /*is_cast=*/FALSE,
                                           /*is_static_cast=*/FALSE,
                                           &match_level,
                                           &conversion->std,
                                           (a_boolean *)NULL,
                                           &unknown_dependent_function,
                                           &ambiguous) != NULL) {
        okay = TRUE;
        if (conversion->std.exception_spec_incompatibility) {
          /* In assignments and initializations, exception specifications
             under pointers-to-functions and pointers-to-member-functions
             must obey certain rules, but they don't in this case. */
          expr_pos_error(ec_incompatible_exception_specs, err_pos);
        }  /* if */
      } else if (unknown_dependent_function) {
        okay = TRUE;
        conversion->unknown_dependent_conversion = TRUE;
      } else if (ambiguous) {
        /* More than one function matches. */
        if (expr_error_should_be_issued()) {
          pos_sy_error(ec_ambiguous_ptr_to_overloaded_function, err_pos,
                       source_operand->symbol);
        }  /* if */
        conv_to_error_operand(source_operand);
      } else {
        /* No match. */
        if (!is_error_type(dest_type)) {
          if (expr_error_should_be_issued()) {
            pos_sy_error(ec_no_match_for_addr_of_overloaded_function, err_pos,
                         source_operand->symbol);
          }  /* if */
        }  /* if */
        conv_to_error_operand(source_operand);
      }  /* if */
    } else if (C_mode() && is_class_struct_union_type(dest_type) &&
               types_are_compatible_ignoring_qualifiers(dest_type,
                                                        source_type)) {
      /* In C, a struct or union is compatible with the same struct or union.
         Qualifiers can be added or dropped since this is a value
         conversion. */
      conversion->class_identity_or_bitwise_copy = TRUE;
      okay = TRUE;
    } else if (impl_conversion_possible(source_type,
                                        source_is_constant,
                                        (a_boolean)source_operand->
                                                      is_simple_string_literal,
                                        source_constant,
                                        dest_type,
                                      /*allow_qualifier_or_eh_mismatch=*/FALSE,
                                        /*suppress_extensions=*/FALSE,
                                        incompatible_err,
                                        &std_conv)) {
      /* An implicit conversion is legal. */
      okay = TRUE;
      conversion->std = std_conv;
      issue_any_conversion_diagnostics(&std_conv, conversion,
                                       err_pos, source_type,
                                       orig_dest_type);
#if GNU_EXTENSIONS_ALLOWED
    } else if (gcc_mode &&
               !is_error_operand(source_operand) &&
               ((is_transparent != NULL && *is_transparent) ||
                (is_transparent != NULL && is_union_type(dest_type) &&
                 skip_typerefs(dest_type)->variant.class_struct_union.
                                                           is_transparent))) {
      /* The destination is a parameter of a union type declared as
         a transparent union (a GNU C extension).  The argument can be
         converted to the type of any member of the union. */
      a_field_ptr f =
              transparent_union_conversion_possible(source_operand, dest_type);
      /* If none of the fields was satisfactory, issue an error. */
      if (f == NULL) {
        goto error;
      }  /* if */
      /* The implicit conversion to "f" is legal. */
      okay = TRUE;
      conversion->std = std_conv;
      issue_any_conversion_diagnostics(&std_conv, conversion, err_pos,
                                       source_type, f->type);
      /* Convert from the type of the field to the type of the
         union, using a dynamic initializer generated on the fly. */
      prep_transparent_union_conversion_operand(dest_type, f, source_operand);
#endif /* GNU_EXTENSIONS_ALLOWED */
    } else {
#if GNU_EXTENSIONS_ALLOWED
error:
#endif /* GNU_EXTENSIONS_ALLOWED */
      /* The conversion is not legal. */
      if (expr_error_should_be_issued()) {
        /* The "opt_ty2" routine puts in the types if the specific error
           message has fill-ins for them, and otherwise ignores the types. */
        pos_opt_ty2_error(incompatible_err, err_pos,
                          source_type, orig_dest_type);
      }  /* if */
      conv_to_error_operand(source_operand);
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "conversion_possible: %s\n", okay ? "okay" : "not okay");
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return okay;
}  /* conversion_possible */


static void prep_class_bitwise_copy_operand(an_operand *source_operand,
                                            a_type_ptr dest_type)
/*
source_operand is to be copied bitwise to an entity of type dest_type.
Both have class types.  Adjust source_operand if necessary, specifically
for the case where the source type is a derived class of dest_type,
and convert it to an rvalue if it isn't one already.  This routine does
not do the bitwise copy; it just prepares the operand for it.  Note
also that this routine is called for the identity case where the class
type is already correct and nothing should be done to it.
*/
{
  if (C_mode()) {
    /* In C mode, the types will always be the same, ignoring cv-qualifiers.
       We don't want to adjust the cv-qualifiers of the operand to match
       the destination type, since rvalues don't have cv-qualifiers in C. */
  } else {
    /* Adjust the class object type if necessary. */
    full_adjust_class_object_type(source_operand, dest_type);
  }  /* if */
  /* Make the source an rvalue. */
  do_operand_transformations(source_operand, TOPT_NO_OPTIONS);
}  /* prep_class_bitwise_copy_operand */


static void set_up_for_conversion_function_call(
                                           an_operand       *operand,
                                           a_routine_ptr    conversion_routine,
                                           a_symbol_ptr     conversion_symbol,
                                           an_expr_node_ptr *arg_expr_list)
/*
Prepare for generating a call of a conversion function, but do not
actually create the call.  Check accessibility of the routine and adjust
the operand type if necessary so that it will be appropriate for the call.
Return an argument list for the call in *arg_expr_list.  This routine
is used only in C++ mode.
*/
{
  a_type_ptr routine_type = conversion_routine->type;

  /* Check that the conversion function is accessible and mark it as
     referenced. */
  expr_reference_to_implicitly_invoked_function(conversion_symbol,
                                                &operand->position,
                                                operand->type,
                                                /*honor_virtual=*/TRUE);
  check_assertion_str(routine_type_is_nonstatic_member_function(routine_type),
                     "set_up_for_conversion_function_call: no this parameter");
  /* Check for the cfront anachronism that allows a non-const function to be
     called for a const selector (see determine_selector_match_level). */
  if (cfront_2_1_mode &&
      is_const_qualified_type(operand->type)) {
    if (!(routine_type->variant.routine.extra_info->qualifiers & TQ_CONST)) {
      expr_pos_warning(ec_const_function_anachronism, &operand->position);
      /* prep_special_selector_operand (call below) will drop the const. */
    }  /* if */
  }  /* if */
  change_refs_on_selector(routine_type, operand);
  /* Convert the operand to the proper type to be the "this" argument of the
     conversion function. */
  prep_special_selector_operand(operand, routine_type);
  /* Make an expression for the argument. */
  *arg_expr_list = make_node_from_operand(operand);
}  /* set_up_for_conversion_function_call */


static void set_up_for_constructor_call(an_operand       *operand,
                                        a_routine_ptr    ctor_routine,
                                        a_conv_descr     *ctor_arg_conversion,
                                        an_expr_node_ptr *arg_expr_list,
                                        a_boolean        *class_bitwise_copy)
/*
Prepare for generating a call of a one-argument constructor (i.e.,
a copy constructor or a constructor used as a conversion function),
but do not actually create the call.  *operand is the argument for
the call.  Check accessibility of the routine and adjust the
operand type if necessary so that it will be appropriate for the call.
If ctor_arg_conversion is non-NULL, it points to the conversion to be
used for the constructor argument.  Return an argument list for the
call in *arg_expr_list.  If the constructor being called is a trivial
bitwise copy constructor, set up *arg_expr_list so it can be the source
for the bitwise copy and return *class_bitwise_copy TRUE.  This routine
is used only in C++ mode.
*/
{
  a_type_ptr ctor_class = parent_class_of(ctor_routine);

  *class_bitwise_copy = FALSE;
  if (ctor_routine->is_trivial_copy_function &&
      (ctor_arg_conversion != NULL ||
       (is_class_struct_union_type(operand->type) &&
        is_same_class_or_base_class_thereof(operand->type, ctor_class)))) {
    /* The constructor is a trivial bitwise copy constructor. */
    *class_bitwise_copy = TRUE;
    expr_reference_to_trivial_copy_constructor(ctor_class, &operand->position);
    if (ctor_arg_conversion == NULL ||
        is_null_user_conv_descr(ctor_arg_conversion)) {
      /* No user-defined conversion on the argument, so this is a simple
         copy (or bitwise slice from a derived to a base class object). */
      prep_class_bitwise_copy_operand(operand, ctor_class);
    } else {
      /* There is a user-defined conversion on the argument. */
      ctor_arg_conversion->result_is_an_lvalue = FALSE;
      user_convert_operand(operand,
                           ctor_class,
                           ctor_arg_conversion,
                           (a_conv_descr *)NULL,
                           /*force_copy_to_temp=*/FALSE);
    }  /* if */
    *arg_expr_list = make_node_from_operand(operand);
  } else {
    /* Normal case, not a bitwise copy constructor. */
    a_type_ptr       routine_type = skip_typerefs(ctor_routine->type);
    a_routine_type_supplement_ptr
                     rtsp = routine_type->variant.routine.extra_info;
    a_param_type_ptr param_list = rtsp->param_type_list;

    check_assertion_str(param_list != NULL || rtsp->has_ellipsis,
                        "set_up_for_constructor_call: no first parameter");
    /* Check that the constructor is accessible and mark it as referenced. */
    expr_reference_to_implicitly_invoked_function(symbol_for(ctor_routine),
                                                  &operand->position,
                                                  ctor_class,
                                                  /*honor_virtual=*/FALSE);
    /* Convert the argument to the right type.  We don't expect an error
       here, since presumably we've chosen the proper function to call
       through overload resolution. */
    prep_possible_ellipsis_argument_operand(operand, param_list,
                                            ctor_arg_conversion);
    /* Make an expression for the argument. */
    *arg_expr_list = make_node_from_operand(operand);
    /* If the constructor has default arguments after the first, add
       arguments for them. */
    if (param_list != NULL) {
      (*arg_expr_list)->next = expr_copy_default_arg_expr_list(ctor_routine,
                                                             param_list->next);
    }  /* if */
  }  /* if */
}  /* set_up_for_constructor_call */


static void make_constructor_dynamic_init(a_routine_ptr     ctor_routine,
                                          an_expr_node_ptr  arg_expr_list,
                                          a_type_ptr        temp_type,
                                          a_boolean         result_is_lvalue,
                                          a_boolean         class_bitwise_copy,
                                          a_boolean         is_explicit_cast,
                                          a_source_position *position,
                                          an_operand        *result)
/*
Create an enk_temp_init node that calls the constructor ctor_routine with
the argument list arg_expr_list.  Set *result to an lvalue for the
resulting temporary if result_is_lvalue is TRUE, or an rvalue otherwise.
The constructor is a bitwise copy constructor if class_bitwise_copy is TRUE.
The argument list has already been prepared for the call (default
arguments have been added, the argument types have been adjusted, etc.).
temp_type is the type of the temporary; its cv-unqualified version must
be the class of which the constructor is a member.  If it is NULL, the
class type is used.  ctor_routine can be NULL to indicate that the
constructor is unknown because one or more of the arguments is
template-dependent in a prototype instantiation.  temp_type must be
non-NULL in that case.  is_explicit_cast is TRUE if this node represents
an explicit cast.  *position gives the source position.
*/
{
  a_dynamic_init_ptr  dip;
  an_expr_node_ptr    temp_init_node;
  a_dynamic_init_kind kind;

  if (ctor_routine == NULL) {
    check_assertion(temp_type != NULL);
  } else {
    a_type_ptr class_type;
    check_assertion_str(ctor_routine->special_kind ==
                                      (a_special_function_kind)sfk_constructor,
                     "make_constructor_dynamic_init: routine not constructor");
    class_type = parent_class_of(ctor_routine);
    if (temp_type == NULL) {
      temp_type = class_type;
    } else {
      check_assertion_str(types_are_compatible_ignoring_qualifiers(class_type,
                                                                   temp_type),
                          "make_constructor_dynamic_init: bad temp_type");
    }  /* if */
  }  /* if */
  /* Create the dynamic initialization entry and the enk_temp_init node. */
  kind = (class_bitwise_copy ? (a_dynamic_init_kind)dik_expression :
                               (a_dynamic_init_kind)dik_constructor);
  temp_init_node = create_expr_temporary(temp_type,
                                         result_is_lvalue,
                                         is_explicit_cast,
                                         /*suppress_abstract_test=*/FALSE,
                                         kind,
                                         position,
                                         &dip);
  if (class_bitwise_copy) {
    /* Use a dik_expression to do a bitwise copy. */
    dip->variant.expression = arg_expr_list;
  } else {
    /* Use a dik_constructor to call the constructor routine. */
    dip->variant.constructor.ptr = ctor_routine;
    dip->variant.constructor.args = arg_expr_list;
    dip->variant.constructor.value_initialization = FALSE;
  }  /* if */
  /* Make an operand for the overall expression. */
  make_lvalue_or_rvalue_expression_operand(temp_init_node, result);
  rule_out_expr_kinds(ROEK_CONSTANT, result);
}  /* make_constructor_dynamic_init */


static void temp_init_by_bitwise_copy_from_operand(an_operand *operand,
                                                   a_boolean  result_is_lvalue,
                                                   a_boolean  is_explicit_cast)
/*
Create a temporary and initialize it by bitwise copy from the given operand.
Create an enk_temp_init node for the initialization, and update *operand
to refer to that node.  The result is an lvalue for the temporary if
result_is_lvalue is TRUE, an rvalue otherwise.  is_explicit_cast is TRUE
if this node represents an explicit cast.
*/
{
  a_dynamic_init_ptr dip;
  an_expr_node_ptr   temp_init_node;

  /* Allocate the dynamic initialization entry and the enk_temp_init node. */
  temp_init_node = create_expr_temporary(operand->type,
                                         result_is_lvalue,
                                         is_explicit_cast,
                                         /*suppress_abstract_test=*/FALSE,
                                         (a_dynamic_init_kind)dik_expression,
                                         &operand->position,
                                         &dip);
  conv_lvalue_to_rvalue(operand);
  dip->variant.expression = make_node_from_operand(operand);
  /* Make an operand for the overall expression. */
  make_lvalue_or_rvalue_expression_operand(temp_init_node, operand);
  rule_out_expr_kinds(ROEK_CONSTANT, operand);
}  /* temp_init_by_bitwise_copy_from_operand */


void user_convert_operand(an_operand   *operand,
                          a_type_ptr   dest_type,
                          a_conv_descr *conversion,
                          a_conv_descr *ctor_arg_conversion,
                          a_boolean    force_copy_to_temp)
/*
Do the user-defined conversion indicated by *conversion to convert
*operand to dest_type.  dest_type may be NULL to indicate that
no additional conversion is needed after the conversion function is called.
If ctor_arg_conversion is non-NULL, it describes the conversion to be done
on the argument of the user-defined conversion, which in that case will
be a constructor call.  *conversion generally must indicate a user-defined
conversion (possibly a bitwise copy), but for convenience this routine
will also handle a conversion with only class_object_adjustment_required
indicated.  Note that this routine converts the operand to a
destination type, but does not copy it anywhere; that's up to the
caller.  That's particularly significant when the "conversion" is a
class bitwise copy or simple class object adjustment: the processing
here changes the operand to access the same class object with the new
type, but does not copy it to a temporary.  However, if in such a case
force_copy_to_temp is TRUE and conversion->result_is_an_lvalue
indicates an rvalue result is required, a temporary will be created,
the operand will be copied into it, and the result is an rvalue for
the temporary.
*/
{
  an_expr_node_ptr  rout_node, arg_expr_list;
  an_operand        orig_operand;
  a_routine_ptr     conversion_routine;
  a_boolean         is_explicit_cast;

  orig_operand = *operand;
  conversion_routine = conversion->routine;
#if CHECKING
  if (conversion->unusable) {
    /* The conversion was unusable.  That should have been figured out
       again and shouldn't get here. */
    internal_error("user_convert_operand: unusable conversion");
  }  /* if */
#endif /* CHECKING */
  is_explicit_cast = conversion->is_explicit_cast;
  if (conversion->result_is_an_lvalue) force_copy_to_temp = FALSE;
  if (conversion->class_identity_or_bitwise_copy) {
    /* Bitwise copy of a class. */
    prep_class_bitwise_copy_operand(operand, dest_type);
    if (force_copy_to_temp) {
      /* Make a copy of the class object in a temporary. */
      expr_reference_to_trivial_copy_constructor(operand->type,
                                                 &operand->position);
      temp_init_by_bitwise_copy_from_operand(operand,
                                             /*result_is_lvalue=*/FALSE,
                                             is_explicit_cast);
    }  /* if */
  } else if (conversion->unknown_dependent_conversion) {
    /* Conversion from or to a template-dependent type in a prototype
       instantiation.  Render as a cast. */
    if (dest_type == NULL) dest_type = type_of_unknown_templ_param_nontype;
    generic_cast_operand(operand, dest_type, csf_none, !is_explicit_cast,
                         &orig_operand.position);
  } else if (conversion_routine == NULL) {
    /* A simple class object type adjustment without a call of a conversion
       routine. */
    check_assertion(conversion->class_object_adjustment_required &&
                    dest_type != NULL);
    do_class_object_adjustment(operand, dest_type, conversion);
    if (force_copy_to_temp) {
      temp_init_from_operand(operand, /*result_is_lvalue=*/FALSE);
    }  /* if */
  } else if (conversion_routine->special_kind ==
                                     (a_special_function_kind)sfk_conversion) {
    /* Conversion function. */
    an_expr_node_ptr conv_function_call_node;

    set_up_for_conversion_function_call(operand, conversion_routine,
                                        conversion->routine_symbol,
                                        &arg_expr_list);
    /* Conversion routines are called directly. */
    /* Make a node for the function. */
    rout_node = function_rvalue_expr(conversion_routine);
    rout_node->next = arg_expr_list;
    /* Make an operand for the call. */
    make_function_call(rout_node, conversion_routine->type,
                       (a_boolean)conversion_routine->is_virtual,
                       /*virtual_suppressed=*/FALSE,
                       /*selector_is_object_pointer=*/FALSE,
                       /*compiler_generated=*/!is_explicit_cast,
                       /*is_conversion=*/TRUE,
                       /*arg_dep_lookup_suppressed=*/FALSE,
                       /*qualified_function_name=*/FALSE,
                       /*found_through_adl=*/FALSE,
                       /*uses_operator_syntax=*/FALSE,
                       &orig_operand.position, operand,
                       &conv_function_call_node);
    if (dest_type == NULL) {
      /* No specified destination type.  The result type of the conversion
         function is what we want. */
      /* If an rvalue is wanted, convert to an rvalue. */
      if (!conversion->result_is_an_lvalue) {
        do_operand_transformations(operand, TOPT_NO_OPTIONS);
      }  /* if */
    } else if (is_class_struct_union_type(operand->type) ||
               is_class_struct_union_type(dest_type)) {
      /* Class types get special handling: they can involve derived --> base
         conversions, and class rvalues retain their cv-qualifiers.
         The "or" test is needed because one or the other might be an
         error type. */
      do_class_object_adjustment(operand, dest_type, conversion);
    } else {
      /* Nonclass case. */
      if (!conversion->result_is_an_lvalue || 
          conversion->std.nontrivial_conversion) {
        /* The caller will not accept an lvalue, or a standard conversion
           must be done, so convert an lvalue to an rvalue.  The operand
           could only be an lvalue if the conversion function returns a
           reference. */
        do_operand_transformations(operand, TOPT_NO_OPTIONS);
      }  /* if */
      /* Do any necessary standard or trivial conversion. */
      if (is_an_rvalue(operand)) {
        an_expr_node_ptr before_cast = (is_expression_operand(operand)) ?
                                            operand->variant.expression : NULL;
        cast_operand(dest_type, operand,
                     /*is_implicit_cast=*/!is_explicit_cast);
        if (is_explicit_cast && conv_function_call_node != NULL &&
            is_expression_operand(operand) &&
            operand->variant.expression != before_cast) {
          /* If there's a cast on top of the conversion function call, that is
             the explicit conversion; the conversion function call is an
             implicit side effect of that cast and should be marked as
             compiler-generated. */
          conv_function_call_node->variant.operation.compiler_generated = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  } else {
    a_boolean class_bitwise_copy;
#if CHECKING
    if (conversion_routine->special_kind !=
                                    (a_special_function_kind)sfk_constructor) {
      internal_error("user_convert_operand: not conversion or constructor");
    }  /* if */
#endif /* CHECKING */
    /* Constructor. */
    /* Make a constructor dynamic init into a temporary, and an operand for
       the value it produces. */
    set_up_for_constructor_call(operand, conversion_routine,
                                ctor_arg_conversion, &arg_expr_list,
                                &class_bitwise_copy);
    if (class_bitwise_copy && !force_copy_to_temp) {
      /* We don't need to force a bitwise copy; we can use the copy we have. */
    } else {
      make_constructor_dynamic_init(conversion_routine, arg_expr_list,
                                    dest_type, /*result_is_lvalue=*/FALSE,
                                    class_bitwise_copy, is_explicit_cast,
                                    &orig_operand.position, operand);
    }  /* if */
  }  /* if */
  /* Restore the original source position, etc. */
  restore_operand_details(operand, &orig_operand);
}  /* user_convert_operand */


static void convert_operand(an_operand   *source_operand,
                            a_type_ptr   dest_type,
                            a_conv_descr *conversion)
/*
Convert source_operand to dest_type.  *conversion describes the
conversion (which might involve a user-defined conversion).
The conversion is assumed not to be due to an explicit cast.
*/
{
#if CHECKING
  if (conversion->unusable) {
    /* The conversion was unusable.  That should have been figured out
       again and shouldn't get here. */
    internal_error("convert_operand: unusable conversion");
  }  /* if */
#endif /* CHECKING */
  if (!is_null_user_conv_descr(conversion)) {
    /* Call a user-defined conversion routine. */
    user_convert_operand(source_operand, dest_type, conversion,
                         (a_conv_descr *)NULL,
                         /*force_copy_to_temp=*/FALSE);
  } else {
    /* Cast the operand to the result type. */
    cast_operand_special(dest_type, source_operand, (a_source_position *)NULL,
                         /*check_cast_access=*/TRUE,
                         /*is_implicit_cast=*/!conversion->is_explicit_cast,
                         /*is_reinterpret_cast=*/FALSE,
                         /*reinterpret_semantics=*/FALSE);
  }  /* if */
}  /* convert_operand */


static a_boolean conversion_usable_or_possible(
                                 an_operand        *source_operand,
                                 a_type_ptr        dest_type,
                                 a_boolean         *is_transparent,
                                 a_type_ptr        orig_dest_type,
                                 a_boolean         need_lvalue_result,
                                 a_boolean         initializing_return_value,
                                 a_boolean         is_copy_initialization,
                                 a_boolean         orig_is_copy_initialization,
                                 a_boolean         is_reference_binding,
                                 an_error_code     incompatible_err,
                                 a_source_position *err_pos,
                                 a_conv_descr      **p_conversion,
                                 a_conv_descr      *local_conversion)
/*
See if source_operand can be converted to dest_type (see conversion_possible
for details on the parameters).  Return TRUE if it can.  If *p_conversion
is non-NULL, the feasibility of the conversion has previously been determined.
Otherwise, set *p_conversion to point to *local_conversion (probably a
local variable in the caller), and call conversion_possible to fill in
the conversion information.  The result of the conversion must be an
lvalue if need_lvalue_result is TRUE.  initializing_return_value is
TRUE if the initialization is being done to return a value in a
return statement.  If is_copy_initialization is TRUE, this is
copy-initialization ("="-form); otherwise, it's
direct-initialization ("()"-form).  See user_defined_conversion_possible
for the meaning of orig_is_copy_initialization.  If is_reference_binding
is TRUE, the result will be bound to a reference, so also consider
conversions to derived classes of dest_type.  orig_dest_type is the
destination type before any rewriting, for use in error messages.
See conversion_possible for the meaning of is_transparent.
*/
{
  a_boolean possible;

  /* See if the conversion is possible.  If *p_conversion is non-NULL,
     we already know that the conversion is possible and how to do it. */
  if (conv_usable(*p_conversion)) {
    possible = TRUE;
    prep_for_known_possible_conversion(source_operand, *p_conversion);
  } else {
    *p_conversion = local_conversion;
    possible = conversion_possible(source_operand, dest_type, is_transparent,
                                   orig_dest_type,
                                   need_lvalue_result,
                                   initializing_return_value,
                                   is_copy_initialization,
                                   orig_is_copy_initialization,
                                   is_reference_binding,
                                   incompatible_err, err_pos,
                                   *p_conversion, (a_conv_descr *)NULL);
  }  /* if */
  return possible;
}  /* conversion_usable_or_possible */


static void prep_conversion_operand(
                                 an_operand        *source_operand,
                                 a_type_ptr        dest_type,
                                 a_boolean         *is_transparent,
                                 a_conv_descr      *conversion,
                                 a_boolean         initializing_return_value,
                                 a_boolean         is_copy_initialization,
                                 a_boolean         orig_is_copy_initialization,
                                 a_boolean         nontype_template_arg,
                                 an_error_code     incompatible_err,
                                 a_source_position *err_pos)
/*
Convert source_operand to dest_type if that is possible.  If not,
issue incompatible_err at *err_pos.  initializing_return_value is TRUE
if the initialization is being done to return a value in a return
statement.  If is_copy_initialization is TRUE, this is
copy-initialization ("="-form); otherwise, it's direct-initialization
("()"-form).  See user_defined_conversion_possible for the meaning
of orig_is_copy_initialization.  If nontype_template_arg is TRUE, this
is a nontype template argument.  source_operand may be an rvalue or an
lvalue.  On return, it will always be an rvalue.  If conversion is
non-NULL, the conversion has previously been found to be acceptable,
and *conversion describes it.  dest_type must not be a reference type.
See conversion_possible for the meaning of is_transparent.
*/
{
  a_conv_descr local_conversion;

#if CHECKING
  if (is_reference_type(dest_type)) {
    internal_error("prep_conversion_operand: dest_type is reference");
  }  /* if */
#endif /* CHECKING */
  /* See if the conversion is possible. */
  if (conversion_usable_or_possible(source_operand, dest_type, is_transparent,
                                    dest_type, /*need_lvalue_result=*/FALSE,
                                    initializing_return_value,
                                    is_copy_initialization,
                                    orig_is_copy_initialization,
                                    /*is_reference_binding=*/FALSE,
                                    incompatible_err, err_pos,
                                    &conversion,
                                    &local_conversion)) {
    /* Some conversions are not allowed on a nontype template argument. */
    if (nontype_template_arg &&
        !conversion_allowed_for_nontype_template_argument(&conversion->std)) {
      if (expr_diagnostic_should_be_issued(es_discretionary_error,
                                           incompatible_err)) {
        pos_ty2_diagnostic(es_discretionary_error, incompatible_err, err_pos,
                           source_operand->type, dest_type);
      }  /* if */
    }  /* if */
    /* The types are compatible.  Do the conversion. */
    /* Force the result to be an rvalue. */
    conversion->result_is_an_lvalue = FALSE;
    convert_operand(source_operand, dest_type, conversion);
  }  /* if */
}  /* prep_conversion_operand */


void check_access_to_elided_copy_constructor(a_type_ptr        source_type,
                                             a_routine_ptr     elided_cctor,
                                             a_source_position *err_pos)
/*
A conversion from source_type (a possibly-qualified class type) is being done
by eliding a copy constructor.  Check that the copy constructor that would
have been referenced exists and is accessible (ARM 12.6.1) and callable
(we assume that the thing being copied is an rvalue because it's the result
of a constructor call).  Issue an error (or a warning, depending on the
mode) at *err_pos if not.  If the caller has already determined the copy
constructor that was elided, it is passed in as elided_cctor; otherwise,
elided_cctor is passed as NULL.
*/
{
  a_type_ptr   class_type = skip_typerefs(source_type);
  a_symbol_ptr cctor_sym;
  a_boolean    ambiguous = FALSE, uncallable = FALSE;
  a_boolean    class_bitwise_copy = FALSE;

  /* The diagnostics here are issued only in strict mode. */
  /* Avoid problems when the source is an error. */
  if (strict_ansi_mode && !is_error_type(source_type)) {
    if (elided_cctor != NULL) {
      cctor_sym = symbol_for(elided_cctor);
      { a_param_type_ptr     ptp = elided_cctor->type->
                                   variant.routine.extra_info->param_type_list;
        a_type_qualifier_set qualifiers;
        a_type_ptr           under_type;
        check_assertion(ptp != NULL && is_reference_type(ptp->type));
        under_type = type_pointed_to(ptp->type);
        qualifiers = get_type_qualifiers(under_type);
        if ((qualifiers & (TQ_CONST | TQ_VOLATILE)) ==
                          (TQ_CONST | TQ_VOLATILE)) {
          /* A copy constructor with a parameter of type reference to const
             volatile cannot copy an rvalue.  Due to a standards quirk this is
             not checked for in overload resolution, but it still makes the
             copy constructor uncallable. */
          uncallable = TRUE;
        }  /* if */
      }
    } else {
      cctor_sym = select_overloaded_copy_constructor(
                                      class_type,
                                      get_type_qualifiers(source_type),
                                      /*source_is_rvalue=*/TRUE,
                                      err_pos,
                                      &ambiguous, &uncallable,
                                      &class_bitwise_copy);
    }  /* if */
    if (class_bitwise_copy) {
      /* A bitwise copy is allowed.  The trivial copy constructor is usually
         public, but it can be nonpublic if it's user-declared and
         defaulted. */
      expr_reference_to_trivial_copy_constructor(class_type, err_pos);
    } else if (ambiguous) {
      /* More than one applicable copy constructor. */
      if (expr_diagnostic_should_be_issued(strict_ansi_discretionary_severity,
                                           ec_ambiguous_copy_constructor)) {
        pos_ty_diagnostic(strict_ansi_discretionary_severity,
                          ec_ambiguous_copy_constructor, err_pos, class_type);
      }  /* if */
    } else if (uncallable) {
      /* The copy constructor that might have been used is uncallable,
         e.g., because its input parameter cannot be bound to an rvalue. */
      if (expr_diagnostic_should_be_issued(strict_ansi_discretionary_severity,
                                           ec_uncallable_elided_cctor)) {
        pos_sy_diagnostic(strict_ansi_discretionary_severity,
                          ec_uncallable_elided_cctor,
                          err_pos, cctor_sym);
      }  /* if */
    } else if (cctor_sym == NULL) {
      /* No applicable copy constructor. */
      if (expr_diagnostic_should_be_issued(strict_ansi_discretionary_severity,
                                           ec_no_suitable_copy_constructor)) {
        pos_ty_diagnostic(strict_ansi_discretionary_severity,
                          ec_no_suitable_copy_constructor, err_pos,
                          class_type);
      }  /* if */
    } else if (expr_access_checking_should_be_done() &&
               !have_access_to_symbol(cctor_sym)) {
      /* The copy constructor is inaccessible. */
      a_boolean error_detected = FALSE;
      a_boolean *p_error_detected = NULL;
      /* If errors are suppressed, get a returned variable instead of issuing
         any error. */
      if (expr_stack->suppress_diagnostics) p_error_detected = &error_detected;
      record_access_error(cctor_sym, (a_symbol_ptr)NULL, (a_type_ptr)NULL,
                          err_pos, (a_symbol_locator*)NULL,
                          strict_ansi_discretionary_severity,
                          ec_inaccessible_elided_cctor, p_error_detected);
      if (error_detected) record_suppressed_error();
    } else {
      /* No error.  The C++98 standard requires that the definition of the
         copy constructor be generated even though it is not called, so
         force that now. */
      force_definition_of_compiler_generated_routine(
                                               cctor_sym->variant.routine.ptr);
      check_use_of_deleted_function(cctor_sym, /*elided_ref=*/TRUE, err_pos);
    }  /* if */
  }  /* if */
}  /* check_access_to_elided_copy_constructor */


a_boolean operand_is_temp_init(an_operand *operand)
/*
Return TRUE if the given operand is an expression operand for an enk_temp_init
(which represents an expression temporary).  Whether the enk_temp_init
returns the value or address of the temporary is immaterial.  Note that
there might be parentheses on top of the enk_temp_init node.
*/
{
  a_boolean is_temp_init = FALSE;

  if (is_expression_operand(operand)) {
    an_expr_node_ptr node = skip_parens(operand->variant.expression);
    if (node->kind == (an_expr_node_kind)enk_temp_init) {
      /* The operand is an enk_temp_init for the value of a temporary. */
      is_temp_init = TRUE;
    }  /* if */
  }  /* if */
  return is_temp_init;
}  /* operand_is_temp_init */


a_boolean is_temp_init_usable_in_optimization(
                                          an_operand         *source_operand,
                                          a_boolean          suppress_dtor,
                                          an_expr_node_ptr   *p_temp_init_node,
                                          a_dynamic_init_ptr *p_dip)
/*
Determine whether or not source_operand is an enk_temp_init node that can be
used in a copy constructor elision optimization.  Return TRUE if so, and
also set *p_temp_init_node and *p_dip to the underlying expression node
and dynamic initialization entry.  If suppress_dtor is TRUE, any destruction
indicated in the initialization is cleared (this is used, for example,
for a return, because the caller will do the destruction).
*/
{
  a_boolean          is_usable_temp_init = FALSE;
  an_expr_node_ptr   temp_init_node;
  a_dynamic_init_ptr dip;

  *p_temp_init_node = NULL;
  *p_dip = NULL;
  if (operand_is_temp_init(source_operand)) {
    /* The operand is an enk_temp_init. */
    temp_init_node = skip_parens(source_operand->variant.expression);
    check_assertion(temp_init_node->kind == (an_expr_node_kind)enk_temp_init);
    dip = temp_init_node->variant.init.dynamic_init;
    /* Avoid problems with dynamic inits with kind dik_none, created for
       functional-notation casts with no arguments (e.g., X()) for classes
       with no constructors. */
    if (dip->kind != (a_dynamic_init_kind)dik_none) {
      is_usable_temp_init = TRUE;
      /* Take the dynamic init off whatever destruction list it is on, if any,
         because it will be given to the caller, who will put it on a
         list at that level. */
      remove_from_destruction_list(dip);
      dip->static_temp = FALSE;
      dip->has_temporary_lifetime = FALSE;
      if (suppress_dtor && dip->destructor != NULL) {
        /* We don't want destruction indicated here (because someone else
           will take care of the destruction), so clear the destructor pointer.
           Note that in these cases the destructor field was filled in
           but the destructor routine has not been marked as referenced,
           because we're in a cctor elision initializer expression
           (see alloc_dtor_dynamic_init and fix_up_dynamic_init_dtors). */
        dip->destructor = NULL;
      }  /* if */
      *p_temp_init_node = temp_init_node;
      *p_dip = dip;
    }  /* if */
  }  /* if */
  return is_usable_temp_init;
}  /* is_temp_init_usable_in_optimization */


static a_dynamic_init_ptr alloc_dynamic_init_possibly_with_dtor(
                                             a_dynamic_init_kind kind,
                                             a_boolean           fill_in_dtor,
                                             a_type_ptr          temp_type,
                                             a_source_position   *position)
/*
Allocate a dynamic initialization entry of type kind and return a pointer
to it.  The entity to be initialized is of type temp_type.  *position
indicates the source position of the initialization.  fill_in_dtor
is TRUE if the dynamic initialization should indicate destruction.
*/
{
  a_dynamic_init_ptr dip;

  if (!fill_in_dtor) {
    /* No destructor call wanted. */
    dip = alloc_expr_dynamic_init(kind);
  } else {
    dip = alloc_dtor_dynamic_init(kind, temp_type, position);
  }  /* if */
  return dip;
}  /* alloc_dynamic_init_possibly_with_dtor */


static void determine_dynamic_init_for_class_init(
                                   an_operand         *source_operand,
                                   a_type_ptr         dest_type,
                                   a_conv_descr       *conversion,
                                   a_conv_descr       *ctor_arg_conversion,
                                   a_boolean          fill_in_dtor,
                                   a_dynamic_init_ptr *p_dip,
                                   an_expr_node_ptr   *p_temp_init_node)
/*
An entity of type dest_type (a class type) is being initialized from
source_operand.  The constructor or conversion function required to do the
copy and/or conversion is given by *conversion.  If ctor_arg_conversion
is non-NULL, it gives the conversion on the first argument of the
constructor (important only in some nonstandard modes).  Create a dynamic
initialization entry to do the initialization (and any required
destruction, if fill_in_dtor is TRUE) and return a pointer to
it in *p_dip (or return *p_dip == NULL for an error).  If
p_temp_init_node is non-NULL, create an enk_temp_init node (for the
temporary as an lvalue) pointing to that dynamic initialization entry,
and return a pointer to it in *p_temp_init_node.  An error node is
returned for an error.  dest_type is allowed to be a class having no
constructors at all.  The initialization represented is an "="
initialization, i.e.,

  dest_type var = source_operand;

This routine does copy constructor elision, i.e., it checks for cases
where a constructor or other routine can be called to generate its
result directly in the entity to be initialized.  If that can be
done, we have in effect optimized out a call of a copy constructor
(i.e., we have elided it).  The language requires that we still check
to see that the copy constructor we would have used exists and is
callable.

This routine is used in both C and C++ mode, although the fancier cases
happen only in C++ mode.
*/
{
  a_dynamic_init_ptr dip = NULL;
  a_routine_ptr      conversion_routine;
  an_expr_node_ptr   arg_expr_list, temp_init_node;
  a_boolean          class_bitwise_copy, elision_done = FALSE;
  a_routine_ptr      elided_cctor = NULL;
  a_type_ptr         class_type = skip_typerefs(dest_type);
  a_type_ptr         elision_source_type;

  temp_init_node = NULL;
  conversion_routine = conversion->routine;
  class_bitwise_copy = conversion->class_identity_or_bitwise_copy;
  if (class_bitwise_copy) {
    /* The operation is a class bitwise copy (of the simplest kind, where
       the copy constructor is implicit and not user-declared). */
    if (!C_mode() &&
        identical_types_ignoring_qualifiers(source_operand->type,
                                            class_type)) {
      /* The source and destination types are the same, so the bitwise copy
         is a "copy constructor call" that may be eligible for elision. */
      /* See whether the source is a temporary that can be eliminated. */
      if (is_temp_init_usable_in_optimization(source_operand,
                                              !fill_in_dtor,
                                              &temp_init_node,
                                              &dip)) {
        /* Eliminate the temporary and the bitwise copy. */
        elision_done = TRUE;
        elision_source_type = source_operand->type;
        class_bitwise_copy = FALSE;
      }  /* if */
    }  /* if */
    if (class_bitwise_copy) {
      expr_reference_to_trivial_copy_constructor(class_type,
                                                 &source_operand->position);
    }  /* if */
  } else if (conversion->unknown_dependent_conversion) {
    /* Conversion to or from an unknown template-dependent type in a
       prototype instantiation. */
  } else if (conversion_routine == NULL) {
    /* There was a previous error. */
#if CHECKING
    if (!is_error_operand(source_operand)) {
      internal_error(
        "determine_dynamic_init_for_class_init: not bitwise copy, no routine");
    }  /* if */
#endif /* CHECKING */
  } else {
    /* There is a conversion routine. */
    if (conversion_routine->special_kind ==
                                    (a_special_function_kind)sfk_constructor) {
      /* The routine is a constructor (copy or not). */
      if (identical_types_ignoring_qualifiers(source_operand->type,
                                              class_type) &&
          is_copy_constructor(conversion_routine, class_type,
                              (a_type_qualifier_set *)NULL,
                              /*include_move_ctors=*/TRUE,
                              /*is_declarative_context=*/FALSE)) {
        /* The conversion routine is a copy constructor, and the source and
           destination have the same type (i.e., the source is not a derived
           class of the destination). */
        /* Look at the top of the expression that is the input to the copy
           constructor, to see if it is something that creates a temporary.
           If it is, the temporary and the copy constructor call can be
           optimized away. */
        if (is_temp_init_usable_in_optimization(source_operand,
                                                !fill_in_dtor,
                                                &temp_init_node,
                                                &dip)) {
          elision_done = TRUE;
          elision_source_type = source_operand->type;
          elided_cctor = conversion_routine;
        }  /* if */
      } else {
        /* The conversion routine is a non-copy constructor, so copy
           constructor elision is being done. */
        /* But if the initialization is treated as direct initialization,
           there's no elision.  This comes up in the auto_ptr trick of
           copying an rvalue of a class type to the same type by use of
           a helper class. */
        elision_done = !conversion->copy_initialization_done_as_direct;
        elision_source_type = class_type;
      }  /* if */
    } else {
#if CHECKING
      if (conversion_routine->special_kind !=
                                     (a_special_function_kind)sfk_conversion) {
        internal_error(
                    "determine_dynamic_init_for_class_init: bad special kind");
      }  /* if */
#endif /* CHECKING */
      /* The routine is a conversion function.  Do the conversion and then
         try to find a copy constructor that can copy the result of the
         conversion for the caller. */
      /* Convert only to the return type of the conversion function at
         this point, and not to the destination type if it is different. */
      user_convert_operand(source_operand, (a_type_ptr)NULL,
                           conversion, (a_conv_descr *)NULL,
                           /*force_copy_to_temp=*/FALSE);
      /* See if the result of the conversion is already in a temporary
         of the right type. */
      if (identical_types_ignoring_qualifiers(source_operand->type,
                                              class_type) &&
          is_temp_init_usable_in_optimization(source_operand,
                                              !fill_in_dtor,
                                              &temp_init_node,
                                              &dip)) {
        elision_done = TRUE;
        elision_source_type = source_operand->type;
      } else {
        if (is_error_operand(source_operand)) {
          conversion_routine = NULL;
        } else {
          /* See if an appropriate copy constructor exists. */
          conversion_routine = expr_select_copy_constructor(
                                class_type,
                                get_type_qualifiers(source_operand->type),
                                is_an_rvalue(source_operand),
                                &source_operand->position,
                                &class_bitwise_copy,
                                /*record_ref=*/TRUE);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (elision_done) {
    /* Copy constructor elision is being done.  Check access to the elided
       copy constructor. */
    check_access_to_elided_copy_constructor(elision_source_type,
                                            elided_cctor,
                                            &source_operand->position);
  }  /* if */
  /* Allocate the dynamic initialization entry. */
  if (dip != NULL) {
    /* The dynamic initialization entry was already allocated above.  This
       happens for the elision cases. */
  } else if (is_error_operand(source_operand)) {
    /* Some previous error.  This is important when we get to this
       routine in a constant expression. */
  } else if (class_bitwise_copy) {
    /* The operation is a class bitwise copy, so use a dik_expression. */
    a_dynamic_init_kind kind = (a_dynamic_init_kind)dik_expression;
    prep_class_bitwise_copy_operand(source_operand, dest_type);
    if (is_constant_operand(source_operand)) {
      /* In some cases (e.g., when the initializer is a compound literal
         in g++ mode) the dynamic initialization should use a constant. */
      kind = (a_dynamic_init_kind)dik_constant;
    }  /* if */
    dip = alloc_dynamic_init_possibly_with_dtor(
                                          kind,
                                          fill_in_dtor,
                                          class_type,
                                          &source_operand->position);
    if (kind == (a_dynamic_init_kind)dik_constant) {
      dip->variant.constant =
                   alloc_shareable_constant(&source_operand->variant.constant);
    } else {
      dip->variant.expression = make_node_from_operand(source_operand);
    }  /* if */
  } else if (conversion->unknown_dependent_conversion) {
    /* Conversion to or from an unknown template-dependent type in a
       prototype instantiation. */
    prep_generic_operand(source_operand);
    /* Set the dynamic init entry to represent "constructor" initialization,
       leaving the constructor pointer NULL. */
    dip = alloc_expr_ctor_dynamic_init((a_routine_ptr)NULL,
                                       make_node_from_operand(source_operand),
                                       /*add_default_args=*/FALSE,
                                       /*implied_source=*/FALSE);
  } else if (conversion_routine != NULL) {
    /* conversion_routine is a constructor (copy or other). */
    a_dynamic_init_kind kind;
    set_up_for_constructor_call(source_operand, conversion_routine,
                                ctor_arg_conversion, &arg_expr_list,
                                &class_bitwise_copy);
    kind = (class_bitwise_copy ? (a_dynamic_init_kind)dik_expression :
                                 (a_dynamic_init_kind)dik_constructor);
    dip = alloc_dynamic_init_possibly_with_dtor(kind,
                                                fill_in_dtor,
                                                class_type,
                                                &source_operand->position);
    if (class_bitwise_copy) {
      /* Use a dik_expression entry to do a bitwise copy. */
      dip->variant.expression = arg_expr_list;
    } else {
      /* Use a dik_constructor entry to call the constructor. */
      dip->variant.constructor.ptr = conversion_routine;
      dip->variant.constructor.args = arg_expr_list;
    }  /* if */
  } else {
    /* Some error. */
    dip = NULL;
  }  /* if */
  if (dip != NULL && dip->kind == (a_dynamic_init_kind)dik_constructor) {
    /* Indicate whether this is an unelided copy constructor call. */
    if (!elision_done) {
      dip->variant.constructor.is_implicit_copy_for_copy_initialization = TRUE;
    }  /* if */
  }  /* if */
  /* Build an enk_temp_init node if one is needed and one did not exist
     already. */
  if (p_temp_init_node != NULL) {
    /* Note that the code here is very similar to the code at the end of
       scan_ctor_arguments. */
    if (temp_init_node == NULL) {
      if (dip == NULL) {
        /* Some error.  Return an error node. */
        temp_init_node = error_node();
      } else {
        temp_init_node = alloc_temp_init_node(dest_type, dip,
                                              /*is_lvalue=*/TRUE,
                                              /*is_explicit_cast=*/FALSE);
      }  /* if */
    } else {
      /* Existing enk_temp_init; make sure it is marked as an lvalue. */
      temp_init_node->is_lvalue = TRUE;
      /* Adjust cv-qualifiers on the type if necessary. */
      temp_init_node->type = dest_type;
      /* Put the dynamic initialization on a destruction list if
         appropriate. */
      set_temp_init_dynamic_init_lifetime(temp_init_node);
    }  /* if */
    *p_temp_init_node = temp_init_node;
  }  /* if */
  *p_dip = dip;
}  /* determine_dynamic_init_for_class_init */


static a_boolean selected_function_is_move_constructor(
                                                      a_conv_descr *conversion,
                                                      a_type_ptr   class_type)
/*
Return TRUE if the function selected and indicated in *conversion is a
move constructor for the class given by class_type.
*/
{
  a_boolean     is_move_constructor = FALSE;
  a_routine_ptr rout = conversion->routine;

  class_type = skip_typerefs(class_type);
  if (rout != NULL &&
      rout->special_kind == (a_special_function_kind)sfk_constructor) {
    a_type_ptr       rout_type = skip_typerefs(rout->type);
    a_param_type_ptr ptp =
                        rout_type->variant.routine.extra_info->param_type_list;
    check_assertion(identical_types(parent_class_of(rout), class_type));
    if (ptp != NULL &&
        is_rvalue_reference_type(ptp->type)) {
      a_type_ptr param_type = type_pointed_to(ptp->type);
      param_type = skip_typerefs(param_type);
      if (identical_types_ignoring_qualifiers(param_type, class_type)) {
        is_move_constructor = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return is_move_constructor;
}  /* selected_function_is_move_constructor */


static a_boolean variable_scope_okay_for_throw_move_optimization(
                                                            a_variable_ptr var)
/*
The variable var is being thrown in a throw operation, and it's eligible
for a copy optimization (it's local and non-static, etc.).  See whether it's
eligible for a move optimization because it would be destroyed on the
throw cleanup anyway.  That involves checking its scope relative to any
enclosing try statements.
*/
{
  a_boolean     okay = FALSE;
  a_scope_depth depth;

  /* Consider:
       struct A {
         A();
         A(const A&);
         A(A&&);
         ~A();
       };
       void f() {
         A a;
         throw a;  // Optimizable
       }
       void f2() {
         try {
           A a;
           throw a;  // Optimizable
         } catch (...) {
         }
       }
       void f3() {
         A a;
         try {
           throw a;  // Not optimizable
         } catch (...) {
         }
       }
  */
  if (!scope_stack[depth_scope_stack].within_try_block) {
    /* The current position is not inside a try block, so the local variable
       will be destroyed for sure on the throw. */
    okay = TRUE;
  } else {
    /* The current position is inside a try block, so the local variable will
       be destroyed on the throw only if it is declared within the innermost
       try block. */
    for (depth = depth_scope_stack;; depth--) {
      check_assertion(depth_innermost_function_scope > 0 &&
                      depth > depth_innermost_function_scope);
      if (var->source_corresp.parent_scope == scope_stack[depth].il_scope) {
        /* We found the local variable before we ran into a try block, so it
           will be destroyed on the throw. */
        okay = TRUE;
        break;
      }  /* if */
      /* Stop looking when we get to the innermost try block, after checking
         whether the variable is in its associated block scope. */
      if (scope_stack[depth].is_try_block) break;
    }  /* for */
  }  /* if */
  return okay;
}  /* variable_scope_okay_for_throw_move_optimization */


void prep_elision_initializer_operand(
                                  an_operand         *source_operand,
                                  a_type_ptr         dest_type,
                                  a_boolean          initializing_return_value,
                                  a_boolean          move_optimization_allowed,
                                  a_boolean          fill_in_dtor,
                                  an_error_code      err_code,
                                  a_dynamic_init_ptr *dip)
/*
An entity of (class) type dest_type is being initialized from source_operand.
Convert it if necessary (issuing an error if the conversion cannot be done),
and build a dynamic initialization entry to describe the initialization.
initializing_return_value is TRUE if the initialization is being done
to return a value in a return statement.  move_optimization_allowed is
TRUE if this is a context where it's allowed to use a move constructor
to do the copy instead of a copy constructor.  The dynamic initialization
entry will also indicate a destructor if appropriate and if
fill_in_dtor is TRUE.  Return a pointer to the dynamic initialization
entry in *dip (or NULL for an error).  err_code is the error code to be
used in case of error.  source_operand may be changed by this routine.
This routine is used in both C and C++ mode, but it exists to do copy
constructor elision in C++ mode.  This is an initialization with the
"=" semantics (copy-initialization).
*/
{
  a_conv_descr   conversion, ctor_arg_conversion;
  an_operand     orig_operand;
  a_boolean      is_copy_initialization = TRUE;
  a_boolean      orig_is_copy_initialization = is_copy_initialization;
  a_variable_ptr var;

  orig_operand = *source_operand;
  *dip = NULL;
  /* Microsoft VC++ treats copy-initialization as direct-initialization
     in some cases.  All the cases that come through here are treated
     that way. */
  if (microsoft_bugs) is_copy_initialization = FALSE;
  if (move_optimization_allowed &&
      rvalue_references_enabled &&
      operand_is_lvalue_for_variable(source_operand, &var)) {
    /* The move constructor optimization might apply here.  Check further. */
    if (variable_eligible_for_copy_optimization(var,
                                                initializing_return_value) &&
        (initializing_return_value ||
         variable_scope_okay_for_throw_move_optimization(var))) {
      /* The move optimization might apply here.  Build a version of the
         operand that has been converted to an rvalue and try the conversion
         from that first.  Convert the variable to an rvalue by casting to
         an rvalue reference type, so for example
           A x;
           return x;
         becomes
           A x;
           return static_cast<A &&>(x);
      */
      a_boolean  ambiguous;
      an_operand rvalue_operand;
      rvalue_operand = *source_operand;
      cast_operand_for_reference_cast(&rvalue_operand,
                                      make_rvalue_reference_type(
                                                          rvalue_operand.type),
                                      &rvalue_operand.position,
                                      /*check_cast_access=*/FALSE,
                                      /*is_implicit_cast=*/TRUE,
                                      /*reinterpret_semantics=*/FALSE); 
      if (conversion_to_class_possible(&rvalue_operand, dest_type,
                                       /*try_bitwise_copy=*/TRUE,
                                       initializing_return_value,
                                       is_copy_initialization,
                                       orig_is_copy_initialization,
                                       /*is_reference_binding=*/FALSE,
                                       &conversion, &ctor_arg_conversion,
                                       &ambiguous,
                                       (a_candidate_function_ptr *)NULL)) {
        /* The conversion is possible.  Additionally, the selected function
           has to be a move constructor. */
        if (selected_function_is_move_constructor(&conversion, dest_type)) {
          /* The move optimization applies. */
          *source_operand = rvalue_operand;
          goto conversion_determined;
        }  /* if */
      } else if (ambiguous) {
        /* If there's an ambiguity, keep the rvalue operand and go do the
           overload resolution again to get the error. */
        *source_operand = rvalue_operand;
        goto after_check;
      }  /* if */
      /* We failed on matching the rvalue case for the move optimization.
         Keep the original operand (an lvalue) and try again. */
#if CHECKING
      /* We're counting on the fact that the cast to a reference type above
         doesn't change the original expression. */
      { a_variable_ptr var2;
        check_assertion(operand_is_lvalue_for_variable(source_operand,
                                                       &var2) &&
                        var == var2);
      }
#endif /* CHECKING */
after_check:;
    }  /* if */
  }  /* if */
  /* Look for a constructor to convert the expression to the required
     class type. */
  if (conversion_possible(source_operand, dest_type, 
                          (a_boolean *)NULL, dest_type,
                          /*need_lvalue_result=*/FALSE,
                          initializing_return_value,
                          is_copy_initialization,
                          orig_is_copy_initialization,
                          /*is_reference_binding=*/FALSE,
                          err_code,
                          &source_operand->position,
                          &conversion, &ctor_arg_conversion)) {
conversion_determined:
    /* The conversion is possible.  Determine the routine and argument
       list to return to the caller. */
    determine_dynamic_init_for_class_init(source_operand, dest_type,
                                          &conversion, &ctor_arg_conversion,
                                          fill_in_dtor,
                                          dip, (an_expr_node_ptr *)NULL);
  }  /* if */
  /* Restore the original source position, etc. */
  restore_operand_details(source_operand, &orig_operand);
}  /* prep_elision_initializer_operand */


void temp_init_from_operand(an_operand *operand,
                            a_boolean  result_is_lvalue)
/*
Create an enk_temp_init node that initializes a temporary to a copy of
the indicated operand.  The source operand can be an rvalue or an
lvalue.  On return, *operand will have been changed to an lvalue for
the temporary if result_is_lvalue is TRUE, or an rvalue for the
temporary if result_is_lvalue is FALSE.  Used only in C++ mode.
*/
{
  a_boolean          cctor_case, class_bitwise_copy;
  a_type_ptr         temp_type, unqual_temp_type;
  a_routine_ptr      cctor_routine;
  an_expr_node_ptr   cctor_arg;
  an_operand         orig_operand;

  orig_operand = *operand;
  temp_type = operand->type;
  unqual_temp_type = skip_typerefs(temp_type);
  cctor_case = FALSE;
  if (is_class_struct_union_type(unqual_temp_type)) {
    /* The operand and temporary have a class type.  If it's a C-style
       struct, a direct copy can be done.  Otherwise, look for a copy
       constructor to use. */
    if (C_dialect != C_dialect_cplusplus) {
      /* C struct. */
      /* cctor_case = FALSE;  -- already set */
    } else {
      /* A copy constructor must be used.  An error is issued if there
         is no applicable copy constructor.  No access checking is done
         here because set_up_for_constructor_call does it below. */
      cctor_routine = expr_select_copy_constructor(
                                unqual_temp_type,
                                get_type_qualifiers(operand->type),
                                is_an_rvalue(operand),
                                &operand->position,
                                &class_bitwise_copy,
                                /*record_ref=*/FALSE);
      if (class_bitwise_copy) {
        /* A bitwise copy can be done. */
        /* cctor_case = FALSE;  -- already set */
      } else if (cctor_routine == NULL) {
        /* No appropriate copy constructor.  The error has already been
           issued.  */
        cctor_case = TRUE;
        conv_to_error_operand(operand);
      } else {
        /* Make the dynamic init call the copy constructor. */
        cctor_case = TRUE;
        set_up_for_constructor_call(operand, cctor_routine,
                                    (a_conv_descr *)NULL, &cctor_arg,
                                    &class_bitwise_copy);
        make_constructor_dynamic_init(cctor_routine, cctor_arg, temp_type,
                                      result_is_lvalue,
                                      class_bitwise_copy,
                                      /*is_explicit_cast=*/FALSE,
                                      &orig_operand.position,
                                      operand);
      }  /* if */
    }  /* if */
  }  /* if */
  if (!cctor_case) {
    /* Normal case -- use a dik_expression initialization to copy the
       operand into the temporary. */
    temp_init_by_bitwise_copy_from_operand(operand, result_is_lvalue,
                                           /*is_explicit_cast=*/FALSE);
  }  /* if */
  /* Restore the original source position, etc. */
  restore_operand_details(operand, &orig_operand);
}  /* temp_init_from_operand */


static void convert_operand_into_temp(an_operand    *source_operand,
                                      a_type_ptr    dest_type,
                                      a_type_ptr    orig_dest_type,
                                      a_conv_descr  *conversion,
                                      an_error_code incompatible_err,
                                      a_boolean     *err)
/*
Convert source_operand to dest_type, put it into a newly-created
temporary, and return an lvalue for the temporary in source_operand.  The
caller will bind a reference to dest_type to the temporary returned, so
there is some latitude in the type of temporary created (i.e., it could
be of a derived class type, or could have fewer cv-qualifiers).  If the
conversion is not possible, issue the error incompatible_err, convert
source_operand to an error operand, and return *err TRUE.  orig_dest_type
is the destination (reference) type before any rewriting, for use in
error messages.  If conversion is non-NULL, the conversion is already
known to be possible, and *conversion describes it.  dest_type must not
be a reference type.  Only used in C++.  This is copy-initialization.
*/
{
  a_conv_descr local_conversion;
  an_operand   orig_operand;
  a_boolean    have_temp;
  a_boolean    is_explicit_cast = FALSE;

  *err = FALSE;
  orig_operand = *source_operand;
#if CHECKING
  if (is_reference_type(dest_type)) {
    internal_error("convert_operand_into_temp: dest_type is reference");
  }  /* if */
#endif /* CHECKING */
  if (conversion != NULL &&
      conversion->is_explicit_cast) {
    /* The overall result is the result of an explicit cast and must be
       so marked.  Generally, the dynamic-init under the enk_temp_init will
       be marked.  However, if the conversion calls a conversion function
       that returns exactly the required destination type, mark that
       conversion function call as the explicit cast, and not the
       dynamic-init. */
    is_explicit_cast = TRUE;
    conversion->is_explicit_cast = FALSE;
    if (conversion->routine != NULL &&
        conversion->routine->special_kind ==
                                     (a_special_function_kind)sfk_conversion) {
      a_type_ptr conversion_type = return_type_of(conversion->routine->type);
      if (identical_types(conversion_type, dest_type)) {
        /* The conversion function returns the right type. */
        is_explicit_cast = FALSE;
        conversion->is_explicit_cast = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  /* See if the conversion is possible. */
  if (conversion_usable_or_possible(source_operand, dest_type, 
                                    (a_boolean *)NULL, orig_dest_type,
                                    /*need_lvalue_result=*/FALSE,
                                    /*initializing_return_value=*/FALSE,
                                    /*is_copy_initialization=*/TRUE,
                                    /*orig_is_copy_initialization=*/TRUE,
                                    /*is_reference_binding=*/FALSE, /* sic */
                                    incompatible_err,
                                    &source_operand->position,
                                    &conversion,
                                    &local_conversion)) {
    a_type_ptr cv_qual_adjusted_type = NULL;
    /* Yes, the conversion is possible.  Do it. */
    if (conversion->class_object_adjustment_required) {
      /* The result of the conversion function is a class rvalue that can
         be bound to but has a slightly different type than dest_type
         (because of derived --> base issues or cv-qualifier differences).
         Do the conversion, but make the temporary have the type of the
         result of the conversion function rather than dest_type.
         Core issue 1138 says that after that we just bind to the
         result (or a subobject thereof); there's no additional copy. */
      user_convert_operand(source_operand, /*dest_type=*/(a_type_ptr)NULL,
                           conversion, (a_conv_descr *)NULL,
                           /*force_copy_to_temp=*/FALSE);
      /* Core issue 1138 is C++0X.  Before that, we did something that
         wasn't right, but we leave it the way it was to avoid affecting
         existing code. */
      if ((cpp0x_mode || cpp0x_sfinae_enabled) &&
          !type_qualifiers_match(source_operand->type, dest_type) &&
          !is_error_operand(source_operand) &&
          !is_error_type(dest_type)) {
        /* There's a cv-qualifier adjustment required on the result of
           the conversion function.  Determine the type we'd like the
           temporary to have, with the same cv-qualifiers as the
           final result type. */
        cv_qual_adjusted_type = type_plus_qualifiers_from_second_type(
                                           skip_typerefs(source_operand->type),
                                           dest_type);
      }  /* if */
    } else {
      /* Normal case. */
      convert_operand(source_operand, dest_type, conversion);
    }  /* if */
    /* In some cases, the result is already in a temporary. */
    have_temp = FALSE;
    if (operand_is_temp_init(source_operand)) {
      /* The conversion routine returns its value into a temporary, so
         we already have a temporary. */
      have_temp = TRUE;
    }  /* if */
    if (have_temp && is_an_lvalue(source_operand)) {
      /* The result of the conversion is already a temporary that is an
         lvalue (in particular, this includes array lvalues). */
      /* Adjust the cv-qualifiers on the temp-init if necessary, which
         changes the cv-qualifiers of the temporary.  Base class differences,
         if any, are handled later. */
      if (cv_qual_adjusted_type != NULL) {
        source_operand->variant.expression->type = cv_qual_adjusted_type;
        source_operand->type = cv_qual_adjusted_type;
      }  /* if */
    } else if (have_temp && is_class_struct_union_type(source_operand->type) &&
               is_an_rvalue(source_operand)) {
      /* The result of the conversion is already a class temporary, but
         it's an rvalue.  Convert it to an lvalue. */
      conv_class_rvalue_operand_to_lvalue(source_operand);
      /* Adjust the cv-qualifiers on the temp-init if necessary, which
         changes the cv-qualifiers of the temporary.  Base class differences,
         if any, are handled later. */
      if (cv_qual_adjusted_type != NULL &&
          operand_is_temp_init(source_operand)) {
        source_operand->variant.expression->type = cv_qual_adjusted_type;
        source_operand->type = cv_qual_adjusted_type;
      }  /* if */
    } else {
      /* Initialize a temporary with the converted value. */
      if (cv_qual_adjusted_type != NULL) {
        /* Adjust the cv-qualifiers before we create the temporary. */
        if (is_an_lvalue(source_operand)) {
          adjust_lvalue_type(source_operand, cv_qual_adjusted_type);
        } else if (is_an_rvalue(source_operand)) {
          adjust_class_rvalue_type(source_operand, cv_qual_adjusted_type);
        }  /* if */
      }  /* if */
      temp_init_from_operand(source_operand, /*result_is_lvalue=*/TRUE);
    }  /* if */
    if (is_explicit_cast) {
      if (operand_is_temp_init(source_operand)) {
        a_dynamic_init_ptr dip =
                 source_operand->variant.expression->variant.init.dynamic_init;
        dip->is_explicit_cast = TRUE;
      } else if (is_error_operand(source_operand)) {
        normalize_error_operand(source_operand);
      } else {
        unexpected_condition();
      }  /* if */
    }  /* if */
    /* Handle base class casts, if any.  cv-qualifier adjustments should have
       been handled above. */
    adjust_lvalue_type(source_operand, dest_type);
  } else {
    /* The conversion is not possible.  The error has already been issued. */
    *err = TRUE;
  }  /* if */
  /* Restore the original source position, etc. */
  restore_operand_details(source_operand, &orig_operand);
}  /* convert_operand_into_temp */


static void adjust_top_temporary_for_binding_to_reference(
                                                    an_operand *operand,
                                                    a_boolean  static_lifetime)
/*
operand is the initializer expression being bound to a reference.
It has already been massaged into an object of the right type, and a
temporary has been generated if necessary.  If the top of the expression
is a temporary, ensure that the temporary will have an appropriate lifetime
so it will last as long as the reference.  If static_lifetime is TRUE, the
reference is static; otherwise, it is automatic.  This is needed for cases
like

  void f() {
    static const A& r = A(1) + A(2);  // Lifetime extended to static lifetime
           const A& s = A(1) + A(2);  // Lifetime extended to scope instead
                                      //   of just full expression
  }

*/
{
  an_expr_node_ptr       node;
  a_dynamic_init_ptr     dip;
  an_object_lifetime_ptr lifetime;

  if (is_expression_operand(operand)) {
    node = operand->variant.expression;
    node = skip_parens(node);
    /* Drop any adjustment of the type. */
    node = expr_before_type_adjustment(node);
    /* Drop any field selections on top of the expression.  (The C++ standard
       says that if the object bound to is a subobject of a complete object
       that is a temporary, the complete object temporary has its lifetime
       extended.)  The lifetime of a temporary is also extended when it is
       the second operand of a comma operation (core issue 462). */
    node = skip_parens(node);
    while (is_operation_node(node)) {
      if (node_operator_is(node, eok_dot_field)) {
        node = node->variant.operation.operands;
      } else if (node_operator_is(node, eok_comma)) {
        node = node->variant.operation.operands->next;
      } else {
        break;
      }  /* if */
      node = skip_parens(node);
    }  /* while */
    if (!node->is_lvalue &&
        is_call_node(node) &&
        is_class_struct_union_type(node->type)) {
      /* A call returning a class object by value.  Add an enk_temp_init
         to create a front-end temporary so we can adjust its lifetime. */
      an_expr_node_ptr node_copy, new_node;
      an_operand       local_operand;
      node_copy = copy_node(node);
      make_expression_operand(node_copy, &local_operand);
      temp_init_from_operand(&local_operand, /*result_is_lvalue=*/FALSE);
      new_node = make_node_from_operand(&local_operand);
      /* Overwrite the original node so we alter the original expression,
         under any nodes we might have stripped off above. */
      check_assertion(identical_types(node_copy->type, new_node->type));
      overwrite_node(node, new_node);
    }  /* if */
    dip = NULL;
    if (node->kind == (an_expr_node_kind)enk_temp_init) {
      dip = node->variant.init.dynamic_init;
    } else if (node->kind == (an_expr_node_kind)enk_lambda) {
      dip = node->variant.lambda.initialization;
    }  /* if */
    if (dip != NULL) {
      if (static_lifetime) dip->static_temp = TRUE;
      dip->has_temporary_lifetime = FALSE;
      lifetime = dip->lifetime;
      /* The "lifetime != NULL" test here deals with initializations that
         do not need a destructor. */
      if (lifetime != NULL) {
        an_object_lifetime_kind kind = lifetime->kind;
        if (static_lifetime ?
                   !is_static_object_lifetime_kind(kind) :
                   (kind == (an_object_lifetime_kind)olk_expr_temporary)) {
          /* The dynamic init for the temporary is attached to an
             inappropriate lifetime, so it must be removed and put into another
             lifetime. */
          remove_from_destruction_list(dip);
          record_end_of_lifetime_destruction(dip, static_lifetime,
                                             /*block_lifetime=*/TRUE);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* adjust_top_temporary_for_binding_to_reference */


void force_complete_type_if_a_variable(an_operand *operand)
/*
If the indicated operand is a reference to a static data member
variable (as either an lvalue or an rvalue), make sure the type of
the static data member is completed by instantiating it if necessary.
The type of the operand will be updated if necessary.
*/
{
  a_variable_ptr   var = NULL;
  an_expr_node_ptr expr = NULL;

  /* See whether the operand is simply a reference to a variable. */
  if (is_expression_operand(operand)) {
    expr = skip_parens(operand->variant.expression);
    if (is_variable_node(expr)) {
      var = expr->variant.variable;
    }  /* if */
  }  /* if */
  if (var != NULL) {
    /* Yes, the operand is simply a variable.  If it's a static data
       member, force instantiation of it (this does something
       meaningful when the data member is an unknown-bound array). */
    a_type_ptr orig_var_type = var->type;
    complete_variable_type_is_needed(var);
    if (var->type != orig_var_type) {
      /* Modify the operand to get the right type in all the right places. */
      if (is_an_lvalue(operand)) {
        operand->type = var->type;
      } else {
        operand->type = rvalue_type(var->type);
      }  /* if */
      expr = operand->variant.expression;
      while (is_operation_node(expr) && node_operator_is(expr, eok_parens)) {
        /* Record the updated type on any parentheses above the variable. */
        expr->type = operand->type;
        expr = expr->variant.operation.operands;
      }  /* while */
      check_assertion(is_variable_node(expr));
      expr->type = operand->type;
    }  /* if */
  }  /* if */
}  /* force_complete_type_if_a_variable */


a_boolean direct_reference_binding_possible(
                                       an_operand   *source_operand,
                                       a_type_ptr   source_type,
                                       a_type_ptr   dest_type,
                                       a_boolean    is_cast,
                                       a_boolean    *ref_to_const,
                                       a_boolean    *ref_to_const_volatile,
                                       a_boolean    *binding_to_rvalue_allowed,
                                       a_boolean    *dropping_qualifiers,
                                       a_boolean    *p_template_case,
                                       a_symbol_ptr *function_symbol)
/*
See if it is possible to directly bind a reference of type dest_type
to source_operand.  If so, return TRUE.  source_operand can be NULL, in
which case source_type gives the operand type.  is_cast is TRUE if the
context is a cast (source_operand is being cast to dest_type).
*ref_to_const is returned TRUE if the reference is to const.
*ref_to_const_volatile is returned TRUE if the reference is to const
volatile.  *binding_to_rvalue_allowed is returned TRUE if the reference
can be bound to an rvalue.  *dropping_qualifiers is returned TRUE if the
reference binding would drop type qualifiers (i.e., the types are such
that the binding could be done except for the qualifiers).
*p_template_case is returned TRUE if the match was assumed to be
possible because we have some template-dependent types.
If function_symbol is non-NULL, consider also the possibility that
the source operand is an indefinite function and the reference is
a reference to function.  Set *function_symbol to the specific function
selected, or NULL if that's not applicable.
Note that this routine does not check for conversion functions that
return lvalues to which the reference could be directly bound; see
conversion_for_direct_reference_binding_possible.
The condition tested by this function is similar to the
"reference-compatible" attribute of the WP [dcl.init.ref], except that
the latter is type-based only (whereas this function also considers
the lvalueness of the source_operand), and this function deals with
some extensions.  Also, note that this function indicates whether
direct binding is "possible" and not whether it is "valid".
*/
{
  a_boolean  direct_binding_possible, type_is_correct_or_derived;
  a_boolean  template_case = FALSE, is_rvalue_ref;
  a_type_ptr base_dest_type, unqual_dest_type, unqual_source_type;
                                           
  if (function_symbol != NULL) *function_symbol = NULL;
  check_assertion(is_reference_type(dest_type));
  base_dest_type = type_pointed_to(dest_type);
  is_rvalue_ref = is_rvalue_reference_type(dest_type);
  if (source_operand != NULL) {
    /* Instantiate a static data member array to make sure we know
       its size. */
    if (is_array_type(base_dest_type)) {
      force_complete_type_if_a_variable(source_operand);
    }  /* if */
    source_type = source_operand->type;
  }  /* if */
  /* The "unqual" types are the unqualified versions of the base types,
     except that array types can still be qualified down at the element
     level. */
  unqual_source_type = skip_typerefs(source_type);
  unqual_dest_type = skip_typerefs(base_dest_type);
  /* See if the types are correct without conversion. */
  type_is_correct_or_derived = FALSE;
  if (types_are_compatible_ignoring_qualifiers(unqual_dest_type,
                                               unqual_source_type)) {
    /* The type is correct, ignoring (first-level) qualifiers.
       Note that this handles qualified array cases. */
    type_is_correct_or_derived = TRUE;
  } else if (is_template_dependent_context() &&
             (is_template_dependent_type(unqual_dest_type) ||
              is_template_dependent_type(unqual_source_type))) {
    /* Assume a match for unknown template parameter types. */
    type_is_correct_or_derived = TRUE;
    template_case = TRUE;
  } else if (is_class_struct_union_type(unqual_dest_type) &&
             is_class_struct_union_type(unqual_source_type) &&
             find_base_class_of(unqual_source_type,
                                unqual_dest_type) != NULL) {
    /* The initializer has a derived type. */
    type_is_correct_or_derived = TRUE;
  } else if ((any_cfront_mode() || microsoft_bugs) &&
             is_pointer_type(unqual_dest_type) &&
             is_pointer_type(unqual_source_type) &&
#ifdef pointer_types_have_same_repr
             pointer_types_have_same_repr(unqual_dest_type,
                                          unqual_source_type) &&
#endif /* ifdef pointer_types_have_same_repr */
             same_type_with_added_qualifiers(unqual_source_type,
                                             unqual_dest_type,
                                             /*ignore_qualifiers=*/FALSE,
                                             (a_boolean *)NULL)) {
    /* The type is a pointer type and is correct, except that the
       destination type has some extra qualifiers that are not present on
       the source type (at any level).  This is an extension. */
    type_is_correct_or_derived = TRUE;
  } else if (function_symbol != NULL &&
             source_operand != NULL &&
             is_indefinite_function_operand(source_operand) &&
             is_a_function_designator(source_operand)) {
    /* The source is a function designator for a set of overloaded functions.
       If there is an overloaded function with the right type, the reference
       can be bound to it. */
    an_arg_match_level match_level;
    a_boolean          ambiguous, unknown_dependent_function;

    *function_symbol =
        find_addr_of_overloaded_function_match(source_operand->symbol,
                                               (a_boolean)source_operand->
                                                                is_template_id,
                                               source_operand->
                                                             template_arg_list,
                                               /*source_is_lvalue=*/TRUE,
                                               dest_type,
                                               /*is_cast=*/FALSE,
                                               /*is_static_cast=*/FALSE,
                                               &match_level,
                                               (a_std_conv_descr *)NULL,
                                               (a_boolean *)NULL,
                                               &unknown_dependent_function,
                                               &ambiguous);
    if (ambiguous) {
      /* More than one function matches. */
      if (expr_error_should_be_issued()) {
        pos_sy_error(ec_ambiguous_ptr_to_overloaded_function,
                     &source_operand->position,
                     source_operand->symbol);
      }  /* if */
      conv_to_error_operand(source_operand);
    } else if (unknown_dependent_function) {
      type_is_correct_or_derived = TRUE;
      template_case = TRUE;
    } else if (*function_symbol != NULL) {
      type_is_correct_or_derived = TRUE;
    }  /* if */
  }  /* if */
  direct_binding_possible = type_is_correct_or_derived;
  /* Determine whether or not the reference is to a const type. */
  *ref_to_const = is_const_qualified_type(base_dest_type);
  *binding_to_rvalue_allowed = *ref_to_const;
  *ref_to_const_volatile = FALSE;
  if (is_rvalue_ref) {
    /* An rvalue reference can bind (only) to an rvalue.  In a cast,
       however, the source can be an lvalue. */
    *binding_to_rvalue_allowed = TRUE;
    if (!is_cast && source_operand != NULL && !is_an_rvalue(source_operand)) {
      direct_binding_possible = FALSE;
    }  /* if */
  } else if (!(any_cfront_mode() || microsoft_bugs) && *ref_to_const &&
             is_volatile_qualified_type(base_dest_type)) {
    /* A reference to const volatile may not be bound to an rvalue.
       This was added after the ARM. */
    *binding_to_rvalue_allowed = FALSE;
    *ref_to_const_volatile = TRUE;
  }  /* if */
  if (type_is_correct_or_derived && !*binding_to_rvalue_allowed &&
      source_operand != NULL && is_an_rvalue(source_operand) &&
      /* With template-dependent cases, we can only be sure about
         lvalue-ness in constant expressions. */
      (!template_case || curr_expr_kind_is_const())) {
    /* The reference may not be bound to an rvalue, and the source_operand
       is an rvalue.  The binding is still possible, though not allowed,
       if the operand has a class type, and using that interpretation
       allows for clearer error messages later. */
    if (!is_class_struct_union_type(unqual_source_type) && !template_case) {
      direct_binding_possible = FALSE;
    }  /* if */
  }  /* if */
  /* The destination type must have no fewer type qualifiers than the source
     type to be usable without conversion (ARM 8.4.3). */
  *dropping_qualifiers = FALSE;
  if (type_is_correct_or_derived && !template_case) {
    a_type_qualifier_set source_quals = get_type_qualifiers(source_type);
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* It's allowed to drop __unaligned or __restrict in Microsoft mode.
       MSVC++ issues no diagnostic. */
    if (microsoft_mode) {
      source_quals &= ~(TQ_UNALIGNED | TQ_RESTRICT);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (source_quals != TQ_NONE) {
      a_type_qualifier_set dest_quals = get_type_qualifiers(base_dest_type);
      if (any_qualifier_in_set_missing(dest_quals, source_quals)) {
        *dropping_qualifiers = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (*dropping_qualifiers) {
    /* There are fewer qualifiers on the destination than on the source,
       so the initialization would involve dropping qualifiers. */
    direct_binding_possible = FALSE;
  }  /* if */
  if (direct_binding_possible && *binding_to_rvalue_allowed &&
      source_operand != NULL && is_bit_field_operand(source_operand) &&
      !template_case) {
    /* For a bit-field case like
         struct A { int i:2; } a;
         const int &r = a.i;
       disallow direct binding.  Note that in the ref to nonconst
       case we leave the operand as it is to get a more specific error
       message about taking the address of a bit field. */
    direct_binding_possible = FALSE;
  }  /* if */
  *p_template_case = template_case;
  return direct_binding_possible;
}  /* direct_reference_binding_possible */


static a_boolean microsoft_can_bind_ref_to_rvalue(an_operand *operand)
/*
Return TRUE if in Microsoft mode it is okay to bind a reference to
non-const to the indicated (rvalue) operand.
*/
{
  a_boolean can_bind = FALSE;

  /* Note that the testing here is for reference binding in general,
     e.g., for the initializer of the declaration of a reference
     variable.  Argument matching is more permissive in some cases;
     see determine_arg_match_level. */
  if (is_an_rvalue(operand)) {
    if (is_expression_operand(operand)) {
      an_expr_node_ptr expr = skip_parens(operand->variant.expression);
      if (expr->kind == (an_expr_node_kind)enk_new_delete) {
        a_new_delete_supplement_ptr ndsp = expr->variant.new_delete;
        if (ndsp->is_new) {
          /* A reference to non-const can bind to a "new". */
          can_bind = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return can_bind;
}  /* microsoft_can_bind_ref_to_rvalue */


static void check_for_returning_reference_to_local_entity(an_operand *operand)
/*
operand is the entity being bound to a reference in a return statement.
Issue a warning if it is a local entity.
*/
{
  a_boolean is_temp;

  if (is_expression_operand(operand)) {
    if (is_lvalue_for_auto_object(operand->variant.expression, &is_temp) ||
        is_rvalue_for_auto_object(operand->variant.expression, &is_temp)) {
      /* The expression is an lvalue or class rvalue (object) for a local
         entity.  Use a different message for temporaries and local
         variables. */
      expr_pos_warning(is_temp ? ec_return_ref_init_requires_temp :
                                 ec_returning_ref_to_local_variable,
                       &operand->position);
    }  /* if */
  }  /* if */
}  /* check_for_returning_reference_to_local_entity */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void add_copy_to_temp_for_microsoft_rvalue_question_mark(
                                                           an_operand *operand)
/*
According to the C++ standard, a "?" operator that returns a class
rvalue copies one or the other of its operands into a single result
temporary (see core issue 446).  MSVC++ doesn't do that.  Its
approximation of that is to add an additional copy into a temporary
when a reference variable is bound to a class rvalue "?", which
solves the trickiest problem, that of extending the lifetime of the
temporary to match the lifetime of the reference.  This routine
is called in Microsoft mode when a reference is being bound to "operand".
If the operand is a class rvalue "?" operation, the extra copy to a
temporary is added.
*/
{
  check_assertion(microsoft_mode);
  if (is_an_rvalue(operand) &&
      is_class_struct_union_type(operand->type) &&
      is_expression_operand(operand)) {
    an_expr_node_ptr expr = skip_parens(operand->variant.expression);
    if (is_operation_node(expr) &&
        expr->variant.operation.kind == (an_expr_operator_kind)eok_question) {
      temp_init_from_operand(operand, /*result_is_lvalue=*/FALSE);
    }  /* if */
  }  /* if */
}  /* add_copy_to_temp_for_microsoft_rvalue_question_mark */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

void prep_reference_initializer_operand(
                              an_operand    *source_operand,
                              a_type_ptr    dest_type,
                              a_conv_descr  *conversion,
                              a_boolean     initializing_return_value,
                              a_boolean     initializing_variable,
                              a_boolean     static_lifetime,
                              a_boolean     bitwise_assignment_param,
                              a_boolean     leave_as_object,
                              an_error_code incompatible_err)
/*
A reference of type dest_type is being initialized from the indicated
source_operand.  Check that the operand has the right type (and other
attributes), converting it if necessary.  On return, *source_operand
will contain an rvalue reference to the object to which the reference
should be bound (i.e., if it's an expression it will have an
eok_reference_to on top, or it could be an address constant with
reference type); if leave_as_object is TRUE, an lvalue or class rvalue
for the object is returned instead.  initializing_return_value is TRUE if
the initialization is being done to return a value in a return statement.
initializing_variable is TRUE if this initialization is for a
variable.  In that case, static_lifetime is TRUE if the variable is
static.  If bitwise_assignment_param is TRUE, this call is analyzing
the parameter of a notional generated copy assignment operator.  If
the operand and type are incompatible, the error incompatible_err is
issued.  If conversion is non-NULL, the initializer has previously
been found to be acceptable, and *conversion describes it.
*/
{
  a_type_ptr   orig_source_type = source_operand->type;
  an_operand   orig_operand;
  a_type_ptr   base_dest_type, adj_base_dest_type;
  a_boolean    err = FALSE, dropping_qualifiers, ambiguous, is_rvalue_ref;
  a_boolean    direct_binding_possible = FALSE;
  a_boolean    binding_to_rvalue_allowed = FALSE;
  a_boolean    direct_binding_conversion_possible = FALSE;
  a_boolean    ref_to_const = FALSE;
  a_boolean    ref_to_const_volatile = FALSE;
  a_boolean    operand_was_rvalue;
  a_boolean    warn = FALSE, template_case = FALSE;
  a_conv_descr conv_for_direct_binding;
  a_candidate_function_ptr
               ambiguity_list = NULL;
  a_symbol_ptr function_symbol = NULL;

  check_assertion(is_reference_type(dest_type));
  is_rvalue_ref = is_rvalue_reference_type(dest_type);
  orig_operand = *source_operand;
  if (conversion != NULL &&
      conversion->conversion_for_direct_reference_binding &&
      !conversion->unusable) {
    /* It was previously determined that a conversion function can be used
       to convert the source operand to an lvalue to which the reference can
       be directly bound. */
    direct_binding_conversion_possible = TRUE;
    if (conversion->unknown_dependent_conversion) template_case = TRUE;
  } else if (is_template_dependent_context() &&
             (is_template_dependent_type(dest_type) ||
              is_template_dependent_type(orig_source_type))) {
    /* When dealing with unknown types in a prototype instantiation,
       assume a match. */
    template_case = TRUE;
  } else {
    /* Compare the operand type and the reference type to see if direct
       binding is possible. */
    direct_binding_possible =
                  direct_reference_binding_possible(source_operand,
                                                    (a_type_ptr)NULL,
                                                    dest_type,
                                                    /*is_cast=*/FALSE,
                                                    &ref_to_const,
                                                    &ref_to_const_volatile,
                                                    &binding_to_rvalue_allowed,
                                                    &dropping_qualifiers,
                                                    &template_case,
                                                    &function_symbol);
    if (!direct_binding_possible && !curr_expr_kind_is_const() &&
        is_class_struct_union_type(source_operand->type)) {
      /* It might be possible to convert the source operand to an lvalue
         via a conversion function, and then bind the reference directly to
         the result. */
      if (conversion_for_direct_reference_binding_possible(
                                                      source_operand,
                                                      dest_type,
                                                      /*question_conv=*/FALSE,
                                                      &conv_for_direct_binding,
                                                      &ambiguous,
                                                      &ambiguity_list) ||
          ambiguous) {
        direct_binding_conversion_possible = TRUE;
        conversion = &conv_for_direct_binding;
        if (conversion->unknown_dependent_conversion) template_case = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  base_dest_type = type_pointed_to(dest_type);
  operand_was_rvalue = is_an_rvalue(source_operand);
  adj_base_dest_type = base_dest_type;
  if (bitwise_assignment_param) {
    /* For the bitwise assignment case, the reference type is
       reference-to-const, but it's fabricated.  There's no real need to
       add "const" in the cases where the original object is bound to,
       so use the dest type with the original object cv-qualifiers. */
    if (f_same_entities(skip_typerefs(base_dest_type),
                        skip_typerefs(orig_source_type))) {
      /* Preserve a typedef from the original source type. */
      adj_base_dest_type = orig_source_type;
    } else {
      adj_base_dest_type = make_identically_qualified_type(base_dest_type,
                                                           orig_source_type);
    }  /* if */
  }  /* if */
  if (is_error_operand(source_operand)) {
    /* Leave an error operand alone. */
  } else if (is_error_type(base_dest_type)) {
    /* If the reference is to an error type, return an error operand. */
    conv_to_error_operand(source_operand);
  } else if (template_case) {
    /* Some unknown types in a prototype instantiation.  Assume the binding
       can be done. */
    if (curr_expr_kind_is_const()) {
      /* In a constant expression (i.e., nontype template argument),
         we can check that the source operand is an lvalue.  Elsewhere,
         the lvalue-ness of some operands is not knowable. */
      if (is_rvalue_ref) {
        /* An rvalue reference can only bind to an rvalue.  We don't expect to
           be able to generate an rvalue in a constant expression, but let it
           pass for now and avoid the checking and possible error for the
           lvalue reference cases below. */
        prep_generic_operand_full(source_operand,
                                  /*lvalue_expected=*/FALSE,
                                  /*rvalue_expected=*/TRUE);
      } else {
        change_nonreal_member_constant_operand_to_lvalue(source_operand);
        if (is_an_lvalue(source_operand)) {
          /* Okay, an lvalue. */
        } else if (is_a_function_designator(source_operand) &&
                   /* Avoid member functions; you can't bind references to
                      them. */
                   !is_sym_for_member_operand(source_operand)) {
          /* Okay, a function designator. */
          if (is_indefinite_function_operand(source_operand)) {
            /* Replace an indefinite function by the address of an unknown
               function in the set. */
            conv_indefinite_function_operand_to_unknown_dependent_function(
                                                    source_operand,
                                                    /*force_to_rvalue=*/FALSE);
          }  /* if */
        } else {
          /* Binding a reference to an rvalue in a constant expression. */
          if (!is_error_operand(source_operand)) {
            error_in_operand(ec_expr_not_an_lvalue_or_function_designator,
                             source_operand);
          }  /* if */
        }  /* if */
      }  /* if */
    } else {
      /* Not a constant expression. */
      prep_generic_operand_full(source_operand,
                                /*lvalue_expected=*/!is_rvalue_ref,
                                /*rvalue_expected=*/is_rvalue_ref);
    }  /* if */
  } else if (is_rvalue_ref && !is_an_rvalue(source_operand)) {
    /* An rvalue reference cannot be bound to an lvalue. */
    expr_pos_error(ec_rvalue_reference_bound_to_lvalue,
                   &source_operand->position);
    conv_to_error_operand(source_operand);
  } else if (direct_binding_conversion_possible) {
    /* The initial value can be converted to an lvalue of the right type
       through use of a conversion function returning a reference. */
    if (ambiguity_list != NULL) {
      /* The conversion is ambiguous.  Put out an error. */
      if (expr_error_should_be_issued()) {
        pos_ty2_start_error(ec_ambiguous_conversion_function,
                            &source_operand->position, orig_source_type,
                            base_dest_type);
        diagnose_overload_ambiguity(ambiguity_list,
                                    (an_operand *)NULL,
                                    (an_arg_operand_ptr)NULL,
                                    (an_opname_kind)onk_none);
      }  /* if */
      free_candidate_function_list(ambiguity_list);
      conv_to_error_operand(source_operand);
    } else {
      /* Do the conversion, producing an lvalue. */
      convert_operand(source_operand, base_dest_type, conversion);
    }  /* if */
  } else if (direct_binding_possible && is_an_lvalue(source_operand)) {
    /* The initial value is an lvalue of the right type; the binding
       can be done directly. */
    if (op_is_null_address_lvalue(source_operand)) {
      /* Initializing a reference to NULL, which is not allowed:
           int &p = *(int *)0;
      */
      if (!strict_ansi_mode ||
          !curr_expr_is_potentially_evaluated()) {
        expr_pos_warning(ec_null_reference, &source_operand->position);
      } else {
        error_in_operand(ec_null_reference, source_operand);
      }  /* if */
    } else if (curr_expr_kind_is(ek_template_arg) &&
               is_class_struct_union_type(base_dest_type) &&
               find_base_class_of(orig_source_type, base_dest_type) != NULL) {
      /* A derived-base binding is not allowed in a nontype template
         argument. */
      if (expr_diagnostic_should_be_issued(es_discretionary_error,
                                           incompatible_err)) {
        pos_ty2_diagnostic(es_discretionary_error, incompatible_err,
                           &source_operand->position, orig_source_type,
                           dest_type);
      }  /* if */
    }  /* if */
    /* Do any base-class or cv-qualifier adjustment. */
    adjust_lvalue_type(source_operand, adj_base_dest_type);
    if (ref_to_const) {
      /* For a reference to const, tone down the reference kinds to
         indicate the address is taken in a way that can't modify the
         entity. */
      change_some_ref_kinds(source_operand->ref_entries_list,
                            SRK_ADDRESS_TAKEN,
                            SRK_ADDRESS_TAKEN | SRK_CONST_ADDRESS_TAKEN);
    }  /* if */
  } else if (direct_binding_possible &&
             is_a_function_designator(source_operand)) {
    /* The initial value is a function designator of the right type;
       the binding can be done directly. */
    if (function_symbol != NULL) {
      a_boolean access_error_reported;
      /* The initial value is a set of overloaded functions, out of which
         one has been selected. */
      /* Do whatever would have been done with the function if we had
         known all along which function was intended.  Make an operand
         for the specific function as a function designator. */
      check_assertion(is_indefinite_function_operand(source_operand));
      overloaded_function_catch_up(function_symbol,
                                   source_operand->symbol,
                                   &orig_operand,
                                   (a_source_position *)NULL,
                                   /*elided_reference=*/FALSE,
                                   /*result_is_lvalue=*/TRUE,
                                   /*address_taken=*/FALSE,
                                   source_operand,
                                   &access_error_reported);
    } else {
      /* Normal case (not an indefinite function). */
      /* Check compatibility of exception specifications.  The source type
         cannot be less restrictive than the destination type.  (GCC doesn't
         diagnose this.) */
      if (!exception_spec_conversion_possible(source_operand->type,
                                              base_dest_type)) {
        expr_pos_diagnostic(gpp_mode ? es_warning : es_discretionary_error,
                            ec_incompatible_exception_specs,
                            &source_operand->position);
      }  /* if */
    }  /* if */
  } else if ((direct_binding_possible || dropping_qualifiers) &&
             is_class_struct_union_type(base_dest_type)) {
    a_boolean operand_was_temp_init;
    if (any_cfront_mode()) {
      operand_was_temp_init = operand_is_temp_init(source_operand);
    }  /* if */
    /* The source is a class rvalue but otherwise has the right type,
       so we can bind directly if the reference is to const non-volatile
       (and in some other cases in cfront mode or when anachronisms
       are allowed).  No temporary is required.  Get the address of
       the rvalue, then cast the pointer to the right type to handle
       the derived-class case.  We also allow some error cases (e.g.,
       when the reference is to non-const) to come through here to
       get better error messages. */
    /* Also come here when the source is a class that's wrong only
       because qualifiers are dropped.  That's an error, but it's
       better to handle it here rather than later -- if we go on
       to the call of convert_operand_into_temp we would be looking
       at copy constructors, which really isn't appropriate and
       produces confusing error messages. */
    if (!dropping_qualifiers) {
      /* [dcl.init.ref] of the C++98 standard requires that the copy
         constructor be callable whether or not it is actually called.
         We never call it, but we must check it anyway.  We only check
         in strict mode.  However, core issue 391 eliminated this
         check for C++0x, by requiring the direct binding and therefore
         eliminating the idea that any copy constructor call is
         being elided. */
      if (strict_ansi_mode && !cpp0x_mode) {
        check_access_to_elided_copy_constructor(orig_source_type,
                                                /*elided_cctor=*/
                                                             (a_routine *)NULL,
                                                &source_operand->position);
      }  /* if */
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode && direct_binding_possible && initializing_variable) {
      add_copy_to_temp_for_microsoft_rvalue_question_mark(source_operand);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    full_adjust_class_object_type(source_operand, adj_base_dest_type);
    if (dropping_qualifiers) {
      /* Type qualifiers were dropped on this binding. */
      if (expr_error_should_be_issued()) {
        if (bitwise_assignment_param) {
          /* Use a different message for the bitwise operator= case.  The
             normal message is confusing to programmers. */
          pos_ty_error(ec_no_suitable_assignment_operator,
                       &source_operand->position,
                       f_skip_typerefs(base_dest_type));
        } else {
          pos_ty2_error(ec_qualifier_dropped_in_ref_init,
                        &source_operand->position,
                        dest_type, orig_source_type);
        }  /* if */
      }  /* if */
      conv_to_error_operand(source_operand);
    } else if (!binding_to_rvalue_allowed && operand_was_rvalue) {
      /* Can't bind this reference to an rvalue. */
      an_error_severity err_severity = es_error;
      /* Some cases get only a warning. */
      /* In cfront mode we allow this for a ref to non-const if
         the source was already a temporary or if we're initializing
         a non-variable (e.g., we're passing an argument). */
      /* When anachronisms are allowed we allow this for a ref to
         non-const. */
      if (any_cfront_mode() ? (!initializing_variable ||
                               operand_was_temp_init) :
                              allow_anachronisms) {
        err_severity = es_warning;
      } else if (ref_to_const_volatile &&
                 (any_cfront_mode() || allow_anachronisms)) {
        /* The reference to const volatile case gets only a warning in cfront
           or anachronisms mode (it's a recent change to the language). */
        err_severity = es_warning;
      } else if (allow_nonconst_ref_anachronism) {
        /* Because this shows up in a lot of code, there's a separate
           anachronism to allow this. */
        err_severity = es_warning;
      }  /* if */
      expr_pos_diagnostic(err_severity,
                          ref_to_const_volatile ?
                                       ec_const_volatile_ref_init_from_rvalue :
                                       ec_nonconst_ref_init_from_rvalue,
                          &source_operand->position);
      if ((int)err_severity > (int)es_warning) {
        conv_to_error_operand(source_operand);
      }  /* if */
    }  /* if */
  } else if (direct_binding_possible && is_an_rvalue(source_operand) &&
             is_array_type(base_dest_type)) {
    /* Direct binding of a reference to an rvalue array.  This was made
       valid by core issue 450.  MSVC++ allows this since version 7.0,
       Sun allows it in Studio 11, and g++ doesn't allow it even in 4.1,
       but we'll go ahead and allow it in all modes. */
    do_array_to_pointer_conversion(source_operand);
    conv_object_pointer_to_lvalue(source_operand);
    adjust_lvalue_type(source_operand, adj_base_dest_type);
  } else if (direct_binding_possible &&
             is_rvalue_reference_object_operand(source_operand)) {
    /* A reference can be bound directly to an rvalue reference
       object.  Only non-class cases get here. */
    conv_rvalue_reference_object_to_lvalue(source_operand);
    adjust_lvalue_type(source_operand, adj_base_dest_type);
  } else {
    /* The initialization cannot be done directly; a temporary must be
       used and/or an implicit conversion must be done. */
    a_boolean cfront_argument_case = any_cfront_mode() &&
                                     !initializing_variable;
    if (dropping_qualifiers && !cfront_argument_case) {
      /* Type qualifiers were dropped (and otherwise the type is okay).
         Note that testing this early means that an implicit conversion
         cannot be used to drop the qualifiers.  cfront allows dropping
         qualifiers when passing nonclass arguments (class cases were
         handled above). */
      if (expr_error_should_be_issued()) {
        pos_ty2_error(ec_qualifier_dropped_in_ref_init,
                      &source_operand->position,
                      dest_type, orig_source_type);
      }  /* if */
      conv_to_error_operand(source_operand);
    } else if (!binding_to_rvalue_allowed &&
               !(allow_anachronisms ||
                 any_cfront_mode() ||
                 (microsoft_bugs &&
                  microsoft_can_bind_ref_to_rvalue(source_operand)))) {
      /* A temporary cannot be used when binding a reference to non-const,
         except as an anachronism. */
      /* Use a different message for the case where the operand is
         an rvalue. */
      if (operand_was_rvalue) {
        error_in_operand(ref_to_const_volatile ?
                                       ec_const_volatile_ref_init_from_rvalue :
                                       ec_nonconst_ref_init_from_rvalue,
                         source_operand);
      } else {
        if (expr_error_should_be_issued()) {
          pos_ty2_error(ref_to_const_volatile ?
                                       ec_bad_const_volatile_ref_init :
                                       ec_bad_nonconst_ref_init,
                        &source_operand->position,
                        dest_type, orig_source_type);
        }  /* if */
        conv_to_error_operand(source_operand);
      }  /* if */
    } else if (curr_expr_kind_is_const()) {
      /* In a constant context (e.g., a nontype template argument),
         a temporary is not allowed. */
      error_in_operand(ec_init_needing_temp_not_allowed, source_operand);
    } else {
      /* Allocate a temporary and copy the operand into it, converting
         if necessary.  source_operand is set to the address of the
         temporary. */
      convert_operand_into_temp(source_operand, base_dest_type, dest_type,
                                conversion, incompatible_err, &err);
      if (err) {
        /* The conversion could not be done.  An error has already been
           issued. */
      } else if (!binding_to_rvalue_allowed) {
        /* A reference to non-const or to const volatile is initialized
           in a way that requires a temporary.  This is an error, but
           we're allowing it as an anachronism or cfront-ism.  The error
           in other modes was issued above. */
        if (any_cfront_mode()) {
          if (cfront_argument_case ||
              (cfront_3_0_mode && innermost_function_scope != NULL) ||
              (cfront_2_1_mode && operand_is_temp_init(source_operand) &&
               skip_parens(source_operand->variant.expression)->variant.
                                 init.dynamic_init->kind ==
                                       (a_dynamic_init_kind)dik_constructor)) {
            /* In cfront mode we allow this also for a ref to non-const if
               we're passing an argument, or if we have a constructed
               temporary in 2.1 mode, or if we're initializing a non-global
               in 3.0 mode. */
            expr_pos_warning(ref_to_const_volatile ?
                                       ec_const_volatile_ref_init_anachronism :
                                       ec_nonconst_ref_init_anachronism,
                             &source_operand->position);
            warn = TRUE;
          } else {
            /* cfront doesn't allow this case. */
            /* Use a different message for the case where the operand is
               an rvalue. */
            if (operand_was_rvalue) {
              error_in_operand(ref_to_const_volatile ?
                                       ec_const_volatile_ref_init_from_rvalue :
                                       ec_nonconst_ref_init_from_rvalue,
                               source_operand);
            } else {
              if (expr_error_should_be_issued()) {
                pos_ty2_error(ref_to_const_volatile ?
                                       ec_bad_const_volatile_ref_init :
                                       ec_bad_nonconst_ref_init,
                              &source_operand->position,
                              dest_type, orig_source_type);
              }  /* if */
              conv_to_error_operand(source_operand);
            }  /* if */
            err = TRUE;
          }  /* if */
        } else {
          /* Allowed as an anachronism. */
          an_error_severity severity;
          if (microsoft_mode) {
            severity = es_warning;
          } else {
            check_assertion(allow_anachronisms);
            severity = anachronism_error_severity;
          }  /* if */
          expr_pos_diagnostic(severity,
                              ref_to_const_volatile ?
                                       ec_const_volatile_ref_init_anachronism :
                                       ec_nonconst_ref_init_anachronism,
                              &source_operand->position);
          if (severity == es_error) {
            err = TRUE;
          } else {
            warn = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
      if (!err && !warn) {
        /* Let the user know a temp was used. */
        if (expr_diagnostic_should_be_issued(es_remark,
                                             ec_temp_used_for_ref_init)) {
          pos_remark(ec_temp_used_for_ref_init, &source_operand->position);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (initializing_variable) {
    /* If the top thing in the initializer is a temporary, make sure the
       temporary has a lifetime as long as the reference. */
    adjust_top_temporary_for_binding_to_reference(source_operand,
                                                  static_lifetime);
  } else if (initializing_return_value && !template_case) {
    /* Check for returning a reference to a local entity. */
    check_for_returning_reference_to_local_entity(source_operand);
  }  /* if */
  if (!leave_as_object) {
    /* Final step: add the reference-to to turn the lvalue or class rvalue
       into an rvalue for the reference. */
    if (is_rvalue_ref && template_case) {
      /* For some template-dependent rvalue reference cases we may still have
         an rvalue here. */
    } else {
      take_reference_to_operand(source_operand, is_rvalue_ref);
    }  /* if */
  }  /* if */
  /* Restore the original source position, etc. */
  restore_operand_details(source_operand, &orig_operand);
}  /* prep_reference_initializer_operand */


void prep_initializer_operand(an_operand    *source_operand,
                              a_type_ptr    dest_type,
                              a_boolean     *is_transparent,
                              a_conv_descr  *conversion,
                              a_boolean     initializing_return_value,
                              a_boolean     initializing_variable,
                              a_boolean     static_lifetime,
                              a_boolean     is_copy_initialization,
                              a_boolean     nontype_template_arg,
                              an_error_code incompatible_err)
/*
Check the operand for initializer compatibility against the type supplied.
Cast the operand if required to make it the right type.  Convert the
operand from an lvalue to an rvalue if necessary (it usually is).
initializing_return_value is TRUE if the initialization is being done
to return a value in a return statement.  initializing_variable is
TRUE if this initialization is for a variable.  In that case,
static_lifetime is TRUE if the variable is static.  is_copy_initialization
is TRUE if this is copy-initialization ("="-form); otherwise, it is
direct-initialization ("()"-form).  nontype_template_arg is TRUE if
the operand is a nontype template argument.  If the operand and type
are incompatible, the error incompatible_err is issued.  This routine is
used for initialization, function call arguments, and return
expressions, i.e., for "="-type initializations.  It is not used when
copy constructor elision is possible; see
prep_elision_initializer_operand.  If conversion is non-NULL, the
initializer has previously been found to be acceptable, and
*conversion describes it.  See conversion_possible for the meaning
of is_transparent.
*/
{
  a_boolean orig_is_copy_initialization = is_copy_initialization;

  /* Microsoft VC++ treats
       return expr;
     and
       A x = expr;
     as direct-initialization.  Argument passing is not affected. */
  if (microsoft_bugs &&
      (initializing_return_value || initializing_variable)) {
    is_copy_initialization = FALSE;
  }  /* if */
  if (is_error_operand(source_operand)) {
    /* Previous error.  Leave the operand alone. */
  } else if (is_reference_type(dest_type)) {
    /* Reference initialization. */
    prep_reference_initializer_operand(source_operand, dest_type,
                                       conversion,
                                       initializing_return_value,
                                       initializing_variable,
                                       static_lifetime,
                                       /*bitwise_assignment_param=*/FALSE,
                                       /*leave_as_object=*/FALSE,
                                       incompatible_err);
  } else {
    /* Normal case (not initializing a reference). */
    prep_conversion_operand(source_operand, dest_type, is_transparent,
                            conversion,
                            initializing_return_value,
                            is_copy_initialization,
                            orig_is_copy_initialization,
                            nontype_template_arg,
                            incompatible_err,
                            &source_operand->position);
  }  /* if */
  if (expr_stack->favor_constant_result) {
    force_operand_to_constant_if_possible(source_operand);
  }  /* if */
}  /* prep_initializer_operand */


void prep_arg_passed_via_copy_constructor(an_operand    *source_operand,
                                          a_type_ptr    param_type,
                                          a_conv_descr  *conversion,
                                          an_error_code err_code)
/*
*source_operand is the actual argument for a formal parameter of type
param_type (a possibly-qualified class type).  A copy constructor is to
be used to pass the argument.  Generate an initialization of a temporary
via a copy constructor call, and set *source_operand to the address of
the temporary, which is what is passed as the argument.  If the copy
constructor call cannot be generated, issue the error err_code.  If
conversion is non-NULL, the copy construction has previously been
found to be acceptable, and *conversion describes it.
*/
{
  a_conv_descr       local_conversion;
  an_expr_node_ptr   temp_init_node;
  a_dynamic_init_ptr dip;

  /* See if the conversion is possible. */
  if (conversion_usable_or_possible(source_operand, param_type,
                                    (a_boolean *)NULL, param_type,
                                    /*need_lvalue_result=*/FALSE,
                                    /*initializing_return_value=*/FALSE,
                                    /*is_copy_initialization=*/TRUE,
                                    /*orig_is_copy_initialization=*/TRUE,
                                    /*is_reference_binding=*/FALSE,
                                    err_code, &source_operand->position,
                                    &conversion,
                                    &local_conversion)) {
    an_operand orig_operand;
    orig_operand = *source_operand;
    if (is_abstract_class_type(param_type)) {
      /* The type is an abstract class type, so a parameter of the type
         cannot be passed.  This is usually caught when the parameter
         declaration is handled, but some modes allow such declarations
         by with a warning. */
      if (expr_error_should_be_issued()) {
        abstract_class_diagnostic(es_error, ec_abstract_class_param_type,
                                  param_type, &source_operand->position);
      }  /* if */
      conv_to_error_operand(source_operand);
    } else {
     /* Build an enk_temp_init node and a dynamic init entry that
         will initialize the temporary.  The temporary's address is passed
         to the called routine. */
      determine_dynamic_init_for_class_init(source_operand, param_type,
                                            conversion, (a_conv_descr *)NULL,
                                            /*fill_in_dtor=*/TRUE,
                                            &dip, &temp_init_node);
      make_lvalue_expression_operand(temp_init_node, source_operand);
      restore_operand_details(source_operand, &orig_operand);
    }  /* if */
    rule_out_expr_kinds(ROEK_CONSTANT, source_operand);
  }  /* if */
}  /* prep_arg_passed_via_copy_constructor */


#if !(GNU_EXTENSIONS_ALLOWED && USER_CONTROL_OF_STRUCT_PACKING)
/*ARGSUSED*/  /* <-- operand is not used in that case. */
#endif /* !(GNU_EXTENSIONS_ALLOWED && USER_CONTROL_OF_STRUCT_PACKING) */
static a_boolean is_gnu_packed_field_operand(an_operand *operand)
/*
Return TRUE if the operand is reference to a packed field in GNU mode.
*/
{
  a_boolean is_packed_field = FALSE;

#if GNU_EXTENSIONS_ALLOWED && USER_CONTROL_OF_STRUCT_PACKING
  if (is_expression_operand(operand) &&
      is_an_lvalue(operand)) {
    an_expr_node_ptr expr = skip_parens(operand->variant.expression);
    if (is_operation_node(expr) &&
        (node_operator_is(expr, eok_dot_field) ||
         node_operator_is(expr, eok_points_to_field))) {
      a_field_ptr field= expr->variant.operation.operands->next->variant.field;
      if (field->is_packed ||
          parent_class_of(field)->variant.class_struct_union.is_packed) {
        is_packed_field = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED && USER_CONTROL_OF_STRUCT_PACKING */
  return is_packed_field;
}  /* is_gnu_packed_field_operand */


void prep_argument_operand(an_operand       *source_operand,
                           a_param_type_ptr formal_param,
                           a_conv_descr     *conversion,
                           an_error_code    err_code)
/*
Check that *source_operand is acceptable as an actual argument for the
formal parameter described by formal_param.  If not, issue the error err_code.
If so, convert the operand to the formal parameter type.
If conversion is non-NULL, the argument has previously been found
to be acceptable (as far as overload resolution checks that), and
*conversion describes it.
*/
{
  a_type_ptr param_type = formal_param->type;
  a_boolean  adjusted_for_ref_to_non_const = FALSE;
#if GNU_EXTENSIONS_ALLOWED
  a_boolean  is_transparent = formal_param->is_transparent;
#endif /* GNU_EXTENSIONS_ALLOWED */

  /* If the parameter is a template class, make sure it is instantiated so
     we know if a copy constructor should be used. */
  complete_type_is_needed(param_type);
  if (formal_param->passed_via_copy_constructor) {
    /* Argument is initialized by a copy constructor. */
    prep_arg_passed_via_copy_constructor(source_operand, param_type,
                                         conversion, err_code);
  } else {
    /* Normal argument. */
    if (microsoft_mode && conversion != NULL &&
        is_lvalue_reference_type(param_type) && is_an_rvalue(source_operand)) {
      /* In Microsoft mode, a reference to non-const is sometimes
         allowed to bind to an rvalue.  If that's been done (which we
         know because conversion != NULL means that we've made it through
         overload resolution), adjust the code to make it valid. */
      a_type_ptr underlying_type = type_pointed_to(param_type);
      if (!is_const_qualified_type(underlying_type)) {
        /* For certain cases, convert the rvalue back to an lvalue. */
        revert_microsoft_rvalue_to_lvalue_if_possible(source_operand);
        if (is_an_rvalue(source_operand)) {
          /* For remaining cases, change the reference type to reference
             to const so that the binding is valid. */
          underlying_type = make_qualified_type(underlying_type, TQ_CONST);
          param_type = make_reference_type(underlying_type);
          adjusted_for_ref_to_non_const = TRUE;
        }  /* if */
      }  /* if */
    } else if (gpp_mode && gnu_version >= 30400 &&
               is_reference_type(param_type) &&
               is_gnu_packed_field_operand(source_operand)) {
      /* g++ from version 3.4 on uses a temporary to pass a packed field to
         a reference to const parameter, which avoids passing an unaligned
         pointer. */
      a_type_ptr underlying_type = type_pointed_to(param_type);
      if (is_const_qualified_type(underlying_type) &&
          identical_types_ignoring_qualifiers(underlying_type,
                                              source_operand->type)) {
        a_boolean err;
        convert_operand_into_temp(source_operand,
                                  underlying_type,
                                  param_type,
                                  (a_conv_descr *)NULL,
                                  ec_bad_cast, &err);
      }  /* if */
    } else if (gpp_mode && gnu_version >= 40200 &&
               is_volatile_qualified_type(source_operand->type) &&
               is_an_lvalue(source_operand) &&
               is_class_struct_union_type(param_type) &&
               symbol_supplement_for_class(param_type)->
                                        construction_by_bitwise_copy_allowed &&
               identical_types_ignoring_qualifiers(param_type,
                                                   source_operand->type)) {
      /* g++ allows a volatile lvalue of a bitwise-copyable class type to be
         passed as an argument even though the notional copy constructor
         can't copy a volatile value. */
      adjust_lvalue_type(source_operand, param_type);
    }  /* if */
    if (!adjusted_for_ref_to_non_const) {
      /* Normal case. */
      prep_initializer_operand(source_operand, param_type,
#if GNU_EXTENSIONS_ALLOWED
                               &is_transparent,
#else /* !GNU_EXTENSIONS_ALLOWED */
                               (a_boolean *)NULL,
#endif /* !GNU_EXTENSIONS_ALLOWED */
                               conversion,
                               /*initializing_return_value=*/FALSE,
                               /*initializing_variable=*/FALSE,
                               /*static_lifetime=*/FALSE,
                               /*is_copy_initialization=*/TRUE,
                               /*nontype_template_arg=*/FALSE,
                               err_code);
    } else {
      /* Handle the special adjustment for a ref to non-const (see above). */
      a_type_ptr adj_type;
      prep_reference_initializer_operand(source_operand, param_type,
                                         conversion,
                                         /*initializing_return_value=*/FALSE,
                                         /*initializing_variable=*/FALSE,
                                         /*static_lifetime=*/FALSE,
                                         /*bitwise_assignment_param=*/FALSE,
                                         /*leave_as_object=*/TRUE,
                                         err_code);
      /* Adjust the object type back to the non-const type and then
         turn it into a reference. */
      adj_type = type_pointed_to(formal_param->type);
      if (is_an_lvalue(source_operand)) {
        adjust_lvalue_type(source_operand, adj_type);
      } else if (is_an_rvalue(source_operand)) {
        adjust_class_object_type(source_operand,
                                 adj_type,
                                 (a_base_class_ptr)NULL);
      } else {
        check_assertion(is_error_operand(source_operand));
      }  /* if */
      take_reference_to_operand(source_operand,
                                /*rvalue_reference_case=*/FALSE);
    }  /* if */
  }  /* if */
  if (favor_constant_result_for_nonstatic_init) {
    force_operand_to_constant_if_possible(source_operand);
  }  /* if */
}  /* prep_argument_operand */


void prep_assignment_operand(an_operand        *source_operand,
                             a_type_ptr        dest_type,
                             an_error_code     incompatible_err,
                             a_source_position *err_pos)
/*
Check the operand for assignment compatibility against the type supplied.
Cast the operand if required to make it the right type.  The operand has
not undergone the lvalue --> rvalue transformations (yet).  If the operand
and type are incompatible, issue the error incompatible_err at position
*err_pos.  Note that for classes in C++, this routine is only called for
cases where bitwise copying applies.
*/
{
  if (!C_mode() && is_class_struct_union_type(dest_type)) {
    a_type_ptr class_type = skip_typerefs(dest_type), param_type;
    /* C++ assignment of a class. */
    if ((strict_ansi_mode || microsoft_mode) && is_qualified_type(dest_type)) {
      /* The bitwise copy is defined in terms of a notional generated copy
         assignment operator which is not cv-qualified and therefore cannot
         assign into a cv-qualified left operand. */
      if (expr_error_should_be_issued()) {
        pos_ty_error(ec_no_suitable_assignment_operator, err_pos, class_type);
      }  /* if */
    } else {
      /* The bitwise copy is defined in terms of a notional generated copy
         assignment operator whose parameter is a reference to const.
         Process the source operand as if it will be bound to such a
         reference. */
      a_type_qualifier_set ref_qualifiers = TQ_CONST;
#if NEAR_AND_FAR_ALLOWED
      if (near_and_far_enabled()) {
        /* Allow a "far" object to be copied. */
        if (is_far_type(source_operand->type)) ref_qualifiers |= TQ_FAR ;
      }  /* if */
#endif /* NEAR_AND_FAR_ALLOWED */
      param_type = make_reference_type(make_qualified_type(class_type,
                                                           ref_qualifiers));
      prep_reference_initializer_operand(source_operand, param_type,
                                         (a_conv_descr_ptr)NULL,
                                         /*initializing_return_value=*/FALSE,
                                         /*initializing_variable=*/FALSE,
                                         /*static_lifetime=*/FALSE,
                                         /*bitwise_assignment_param=*/TRUE,
                                         /*leave_as_object=*/TRUE,
                                         incompatible_err);
      conv_lvalue_to_rvalue(source_operand);    
    }  /* if */
  } else {
    /* Nonclass assignment, and C mode struct assignment. */
    /* See if the source and destination types are compatible, and convert the
       source operand to the destination type. */
    prep_conversion_operand(source_operand, dest_type, 
                            (a_boolean *)NULL, (a_conv_descr_ptr)NULL,
                            /*initializing_return_value=*/FALSE,
                            /*is_copy_initialization=*/TRUE,
                            /*orig_is_copy_initialization=*/TRUE,
                            /*nontype_template_arg=*/FALSE,
                            incompatible_err, err_pos);
  }  /* if */
  if (favor_constant_result_for_nonstatic_init) {
    force_operand_to_constant_if_possible(source_operand);
  }  /* if */
}  /* prep_assignment_operand */

#if GNU_EXTENSIONS_ALLOWED

a_field_ptr transparent_union_conversion_possible(an_operand *source_operand,
                                                  a_type_ptr union_type)
/*
Return non-NULL if it is okay to implicitly convert source_operand to any
of the fields of union_type (which must be a union, or a typeref for one).
This is the condition GCC uses for casting to a union and for passing a
transparent union parameter.  If it is okay, the first field that satisfies
the requirement is returned.  Otherwise, NULL is returned.
*/
{
  a_type_ptr  source_type;
  a_field_ptr f;

  db_enter(3, "transparent_union_conversion_possible");
  union_type = skip_typerefs(union_type);
  check_assertion(union_type->kind == (a_type_kind)tk_union);
  source_type = source_operand->type;
  for (f = union_type->variant.class_struct_union.field_list;
       f != NULL;
       f = f->next) {
    a_type_ptr dest_type = f->type;
    /* Try every field type in turn.  Note that a more-or-less exact type
       match is required, not a conversion, except for null pointer
       constants, and except that a "void *" can be converted to/from a pointer
       type. */
    if (interchangeable_types(source_type, dest_type) ||
        (is_pointer_type(dest_type) &&
         ((is_constant_operand(source_operand) &&
           is_null_pointer_constant(&source_operand->variant.constant)) ||
          (is_pointer_type(source_type) &&
           (is_void_type(type_pointed_to(source_type)) ||
            is_void_type(type_pointed_to(dest_type))))))) {
      /* source_operand can be converted to the type of this member. */
      break;
    } /* if */
  }  /* for */
  db_exit();
  return f;
}  /* transparent_union_conversion_possible */


void prep_transparent_union_conversion_operand(a_type_ptr  dest_type,
                                               a_field_ptr field,
                                               an_operand  *source_operand)
/*
Generate a dynamic initializer node which initializes given field of a
union of type dest_type.  The initializer is given by source_operand and
the resulting initializer node is returned through source_operand.
(This is used to implement transparent unions and union casts: both are
GNU C extensions.)  In a constant expression, the result is a constant
aggregate constant.
*/
{
  a_constant_ptr      aggr_con;
  a_constant_ptr      designator_con;
  a_constant_ptr      member_con;
  a_dynamic_init_ptr  field_init;
  a_dynamic_init_ptr  aggr_init;
  an_expr_node_ptr    init_expr;
  a_type_ptr          field_type = rvalue_type(field->type);
  an_operand          orig_operand;

  db_enter(3, "prep_transparent_union_conversion_operand");
  /* Make sure we have an rvalue. */
  conv_lvalue_to_rvalue(source_operand);
  /* Convert the source expression to the destination type if necessary. */
  cast_operand(field_type, source_operand, /*is_implicit_cast=*/TRUE);
  orig_operand = *source_operand;
  /* Build a designator indicating which field should be initialized. */
  designator_con = alloc_constant((a_constant_repr_kind)ck_designator);
  designator_con->variant.designator.field = field;
  /* Build a dynamic initializer indicating how the field should
     be initialized. */
  if (is_expression_operand(source_operand)) {
    field_init = alloc_dynamic_init((a_dynamic_init_kind)dik_expression);
    field_init->variant.expression = source_operand->variant.expression;
    member_con = alloc_constant((a_constant_repr_kind)ck_dynamic_init);
    member_con->type = field_type;
    member_con->variant.dynamic_init = field_init;
  } else if (is_constant_operand(source_operand)) {
    member_con = alloc_constant(source_operand->variant.constant.kind);
    extract_constant_from_operand(source_operand, member_con);
  } else {
    /* There should not be any other operand kinds in C, and GCC
       extensions are only available in C mode.  Note that we do
       not enter this code at all if the source_operand is 
       already erroneous. */
    unexpected_condition();
  } /* if */
  /* Build the entire aggregate initializer. */
  designator_con->next = member_con;
  aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
  aggr_con->type = dest_type;
  aggr_con->variant.aggregate.first_constant = designator_con;
  aggr_con->variant.aggregate.last_constant = member_con;
  if (curr_expr_kind_is_const()) {
    /* Return a constant aggregate in a constant expression.  This can
       then be used as the initializer for a variable. */
    make_constant_operand(aggr_con, source_operand);
  } else {
    /* Build a dynamic initializer for the aggregate. */
    aggr_init = 
      alloc_dynamic_init((a_dynamic_init_kind)dik_nonconstant_aggregate);
    aggr_init->variant.constant = aggr_con;
    /* Build an expression for the initializer. */
    init_expr = alloc_temp_init_node(dest_type, aggr_init, 
                                     /*is_lvalue=*/FALSE,
                                     /*is_explicit_cast=*/FALSE);
    make_expression_operand(init_expr, source_operand);
    rule_out_expr_kinds(ROEK_CONSTANT, source_operand);
  }  /* if */
  restore_operand_details(source_operand, &orig_operand);
  db_exit();
}  /* prep_transparent_union_conversion_operand */

#endif /* GNU_EXTENSIONS_ALLOWED */

a_boolean nontype_template_arg_conversion_possible(an_operand *operand,
                                                   a_type_ptr param_type)
/*
*operand is an operand for a nontype template argument.  See if it can
be converted to the template parameter type param_type, and return TRUE
if so.
*/
{
  a_boolean            compatible = FALSE;
  an_arg_match_summary arg_summary;

  determine_arg_match_level(operand, (a_type_ptr)NULL, param_type,
                            /*param_type_is_deduced=*/FALSE,
                            /*try_user_conversions=*/FALSE,
                            &arg_summary);
  compatible = (arg_summary.match_level != aml_none);
  if (compatible) {
    /* Some conversions are not allowed on a nontype template argument. */
    if (!conversion_allowed_for_nontype_template_argument(
                                                &arg_summary.conversion.std)) {
      compatible = FALSE;
    }  /* if */
  }  /* if */
  return compatible;
}  /* nontype_template_arg_conversion_possible */


a_boolean conditional_operator_conversion_possible(an_operand   *op1,
                                                   an_operand   *op2,
                                                   a_conv_descr *conv,
                                                   a_boolean    *ambiguous)
/*
Determine whether op1 can be converted to match op2 in the sense
defined in the C++ standard, 5.16 [expr.cond] paragraph 3.  This is
used in attempting to convert between the second and third operands
of a "?" operator.  If the conversion is possible, set *conv to
describe the conversion and return TRUE.  If the conversion is ambiguous,
set *ambiguous to TRUE and return TRUE.  However, if ambiguous == NULL
in that case, issue the ambiguity error (and still return TRUE).
One or the other of the operands must have a class type.  This is
used only in C++ mode.
*/
{
  a_boolean        possible = FALSE, local_ambiguous = FALSE;
  a_boolean        related_classes = FALSE;
  a_base_class_ptr bcp = NULL;
  a_type_ptr       op1_type = op1->type;
  a_type_ptr       op2_type = op2->type;
  a_type_ptr       conv_dest_type = NULL;
  a_candidate_function_ptr
                   ambiguity_list = NULL;
  a_candidate_function_ptr
                   *p_ambiguity_list = NULL;
  a_boolean        error_issued = FALSE;

  check_assertion(!C_mode());
  if (ambiguous == NULL) {
    /* Ambiguity errors are to be issued by this routine, so ask the
       conversion routines to pass back a list of the candidates if
       there is an ambiguity. */
    p_ambiguity_list = &ambiguity_list;
  }  /* if */
  clear_conv_descr(conv);
  if (is_indefinite_function_operand(op2)) {
    /* Can't convert to an indefinite function operand. */
    possible = FALSE;
    goto end_of_routine;
  }  /* if */
  if (is_an_lvalue(op2)) {
    a_boolean ref_to_const, ref_to_const_volatile;
    a_boolean binding_to_rvalue_allowed, dropping_qualifiers;
    a_boolean template_case;
    /* op2 is an lvalue.  Attempt to convert op1 to an lvalue of the type
       of op2.  The standard defines this in terms of a notional
       conversion to "reference to op2_type". */
    conv_dest_type = make_reference_type(op2_type);
    if (!is_an_rvalue(op1) &&
        direct_reference_binding_possible(op1,
                                          (a_type_ptr)NULL,
                                          conv_dest_type,
                                          /*is_cast=*/FALSE,
                                          &ref_to_const,
                                          &ref_to_const_volatile,
                                          &binding_to_rvalue_allowed,
                                          &dropping_qualifiers,
                                          &template_case,
                                          (a_symbol **)NULL)) {
      possible = TRUE;
      conv->class_object_adjustment_required = TRUE;
      conv->result_is_an_lvalue = TRUE;
    } else if (!curr_expr_kind_is_const() &&
               is_class_struct_union_type(op1_type)) {
      /* It might be possible to convert the source operand to an lvalue
         via a conversion function, and then bind the reference directly to
         the result. */
      if (conversion_for_direct_reference_binding_possible(
                                           op1,
                                           conv_dest_type,
                                           /*question_conv=*/TRUE,
                                           conv,
                                           &local_ambiguous,
                                           p_ambiguity_list) ||
          local_ambiguous) {
        possible = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (!possible) {
    if (is_class_struct_union_type(op1_type) &&
        is_class_struct_union_type(op2_type)) {
      /* Both operands have class type.  See if the class types are related. */
      a_type_ptr base_op1_type = skip_typerefs(op1_type);
      a_type_ptr base_op2_type = skip_typerefs(op2_type);
      if (identical_types(base_op1_type, base_op2_type)) {
        /* Same class type. */
        possible = TRUE;
        related_classes = TRUE;
      } else if ((bcp = find_base_class_of(base_op1_type,
                                           base_op2_type)) != NULL) {
        /* op2_type is a base class of op1_type. */
        possible = TRUE;
        related_classes = TRUE;
      } else if (find_base_class_of(base_op2_type,
                                    base_op1_type) != NULL) {
        /* op1_type is a base class of op2_type.  The conversion isn't
           possible this way, but the fact that the classes are related
           prevents searching for other matches below. */
        related_classes = TRUE;
      }  /* if */
    }  /* if */
    if (related_classes) {
      /* The types are related classes. */
      if (possible) {
        /* Make sure that cv-qualifiers aren't dropped in the conversion. */
        if (any_qualifier_missing(op2_type, op1_type)) {
          possible = FALSE;
          if (microsoft_bugs && bcp != NULL) {
            /* MSVC++ allows dropping cv-qualifiers on a conversion from
               derived to base. */
            possible = TRUE;
          }  /* if */
        }  /* if */
        /* Check for an ambiguous base class. */
        if (bcp != NULL && bcp->ambiguous) {
          conv->unusable = TRUE;
          local_ambiguous = TRUE;
          if (ambiguous == NULL) {
            /* Issue the ambiguity error.  Note that the base class is
               always the underlying type of op2, never of op1, in this
               case. */
            if (expr_error_should_be_issued()) {
              pos_ty_error(ec_ambiguous_base_class, &op1->position,
                           skip_typerefs(op2->type));
            }  /* if */
            error_issued = TRUE;
          }  /* if */
        }  /* if */
        /* Set *conv to indicate the related-class conversion. */
        conv->std.cast_base_class = bcp;
        conv->std.nontrivial_conversion = (bcp != NULL);
        conv->class_object_adjustment_required = TRUE;
        conv->result_is_an_lvalue = FALSE;
      }  /* if */
    } else {
      /* Not related classes.  See whether op1 can be converted to the
         type of op2 as an rvalue. */
      conv_dest_type = rvalue_type(op2_type);
      if (is_class_struct_union_type(op2_type)) {
        if (conversion_to_class_possible(op1,
                                         conv_dest_type,
                                         /*try_bitwise_copy=*/TRUE,
                                         /*initializing_return_value=*/FALSE,
                                         /*is_copy_initialization=*/TRUE,
                                         /*orig_is_copy_initialization=*/TRUE,
                                         /*is_reference_binding=*/FALSE,
                                         conv, (a_conv_descr *)NULL,
                                         &local_ambiguous,
                                         p_ambiguity_list) ||
            local_ambiguous) {
          possible = TRUE;
        }  /* if */
      } else {
        check_assertion(is_class_struct_union_type(op1_type));
        if (conversion_from_class_possible(op1,
                                           conv_dest_type,
                                           (a_builtin_type_kind_set)BTK_NONE,
                                           /*need_lvalue_result=*/FALSE,
                                           /*is_copy_initialization=*/TRUE,
                                           /*is_reference_binding=*/FALSE,
                                           conv,
                                           &local_ambiguous,
                                           p_ambiguity_list) ||
            local_ambiguous) {
          possible = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (ambiguous != NULL) {
    if (microsoft_bugs && local_ambiguous) {
      /* MSVC++ considers an ambiguous conversion to be impossible. */
      possible = FALSE;
      local_ambiguous = FALSE;
    }  /* if */
    *ambiguous = local_ambiguous;
  } else if (local_ambiguous) {
    /* The conversion is ambiguous.  Issue an error. */
    if (!error_issued) {
      if (expr_error_should_be_issued()) {
        pos_ty2_start_error(ec_ambiguous_user_defined_conversion,
                            &op1->position, op1->type, conv_dest_type);
        diagnose_overload_ambiguity(ambiguity_list,
                                    (an_operand *)NULL,
                                    (an_arg_operand_ptr)NULL,
                                    (an_opname_kind)onk_none);
      }  /* if */
      free_candidate_function_list(ambiguity_list);
    }  /* if */
    conv_to_error_operand(op1);
  }  /* if */
end_of_routine:
  return possible;
}  /* conditional_operator_conversion_possible */


a_symbol_ptr select_overloaded_copy_constructor(
                                   a_type_ptr            class_type,
                                   a_type_qualifier_set  required_qualifiers,
                                   a_boolean             source_is_rvalue,
                                   a_source_position     *pos,
                                   a_boolean             *ambiguous,
                                   a_boolean             *uncallable,
                                   a_boolean             *class_bitwise_copy)
/*
Find and return a pointer to a symbol representing a copy constructor for
the class indicated by class_type and accepting a first parameter whose type
is qualified as specified by required_qualifiers, and an rvalue if
source_is_rvalue is TRUE (source_is_rvalue FALSE should be used if the
rvalueness of the source is irrelevant).  pos is a source position,
used if a template needs to be instantiated.  If no acceptable copy
constructor is found, return NULL.  If more than one acceptable copy
constructor is found and only one of them is the best match, return
that one; otherwise set *ambiguous to TRUE and return NULL.  If no
acceptable copy constructor is found but one would have been
acceptable except that it's uncallable, return that one and set
*uncallable to TRUE.  uncallable can be NULL if that feature is not
wanted.  If a bitwise copy is allowed, return NULL and
*class_bitwise_copy TRUE (this is also returned when the class_type
is template-dependent in a prototype instantiation).  This routine is
used only in C++ mode.  It does not do access checking on the copy
constructor.
*/
{
  a_symbol_ptr                   sym, cctor_sym = NULL, uncallable_sym = NULL;
  a_boolean                      is_overloaded_function;
  a_type_qualifier_set           qualifiers;
  a_boolean                      multiple_uncallable = FALSE;
  a_class_symbol_supplement_ptr  cssp;
  a_routine_ptr                  routine;
  a_type_ptr                     routine_type, arg_type, param_type;
  a_type_ptr                     und_param_type;
  a_routine_type_supplement_ptr  rtsp;
  a_template_arg_ptr             template_arg_list;
  a_param_type_ptr               ptp;
  an_arg_match_summary_ptr       arg_match;
  a_candidate_function_ptr       candidate_functions;
  a_boolean                      undecidable_because_of_error;

  /* This routine is similar to select_overloaded_function. */
  db_enter(4, "select_overloaded_copy_constructor");
#if DEBUG
  overload_level++;
  if (debug_level >= 4 || db_flag_is_set("overload")) {
    db_display_overload_level();
    fprintf(f_debug,
            "Entering select_overloaded_copy_constructor, class_type = ");
    db_abbreviated_type(class_type);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  if (uncallable != NULL) *uncallable = FALSE;
  *class_bitwise_copy = FALSE;
  *ambiguous = FALSE;
  class_type = skip_typerefs(class_type);
  instantiate_template_class(class_type);
  cssp = symbol_supplement_for_class(class_type);
  if (cssp->construction_by_bitwise_copy_allowed ||
      class_type->variant.class_struct_union.is_nonreal_class) {
    /* A bitwise copy is allowed.  Also used when the class is nonreal,
       because we don't know about constructors in that case. */
    cctor_sym = NULL;
    if (!sun_mode && 
        any_qualifier_in_set_missing(TQ_CONST,
                                     required_qualifiers /*lint --e(845)*/)) {
      /* Strictly speaking, a bitwise copy constructor has an input
         parameter of type ref to const class, and therefore it cannot
         copy a volatile-qualified object. */
    } else {
      *class_bitwise_copy = TRUE;
    }  /* if */
  } else {
    arg_type = make_qualified_type(class_type, required_qualifiers);
    sym = cssp->constructor;
#if CHECKING
    if (sym == NULL) {
      internal_error("select_overloaded_copy_constructor: NULL constructor");
    }  /* if */
#endif /* CHECKING */
    /* If sym is an overloaded function symbol we need to go through the whole
       list. */
    if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
      is_overloaded_function = TRUE;
      sym = sym->variant.overloaded_function.symbols;
    } else {
      is_overloaded_function = FALSE;
    }  /* if */
    /* Examine each constructor for this class to find a copy constructor.
       There may be more than one.  For instance, there may be a copy
       constructor that can copy a const object and another that cannot. */
    candidate_functions = NULL;
    for (; sym != NULL; sym = (is_overloaded_function ? sym->next : NULL)) {
#if DEBUG
      if (debug_level >= 4 || db_flag_is_set("overload")) {
        db_display_overload_level();
        db_symbol(sym, "select_overloaded_copy_constructor: considering ", 4);
      }  /* if */
#endif /* DEBUG */
      arg_match = NULL;
      template_arg_list = NULL;
      if (sym->kind == (a_symbol_kind)sk_function_template) {
        /* Try type deduction on a template. */
        routine = sym->variant.template_info->variant.function.routine;
        routine_type = skip_typerefs(routine->type);      
        rtsp = routine_type->variant.routine.extra_info;
        ptp = rtsp->param_type_list;
        if (ptp == NULL /* Error recovery */ ||
            !deduce_one_parameter(ptp, (an_arg_operand **)NULL, arg_type,
                                  sym, &template_arg_list)) {
          /* Deduction failed. */
          goto reject_function;
        }  /* if */
        routine_type = wrapup_function_template_argument_deduction(
                                           &template_arg_list, sym,
                                           (a_template_param_ptr)NULL,
                                           /*is_partial_order_check=*/FALSE);
        if (routine_type == NULL) {
          /* Deduction failed. */
          goto reject_function;
        }  /* if */
      } else {
        /* Not a template. */
        check_assertion(sym->kind == (a_symbol_kind)sk_member_function);
        routine = sym->variant.routine.ptr;
        routine_type = routine->type;
      }  /* if */
      routine_type = skip_typerefs(routine_type);
      if (!is_copy_constructor_type(routine_type, class_type,
                                    (a_type_qualifier_set *)NULL,
                                    /*include_move_ctors=*/source_is_rvalue,
                                    /*is_declarative_context=*/FALSE)) {
        /* Not a copy constructor. */
        goto reject_function;
      }  /* if */
      /* This is a copy constructor.  See if it is callable. */
      rtsp = routine_type->variant.routine.extra_info;
      ptp = rtsp->param_type_list;
      param_type = ptp->type;
      check_assertion(is_reference_type(param_type));
      arg_match = alloc_arg_match_summary();
      determine_arg_match_level((an_operand *)NULL, arg_type,
                                param_type,
                                /*param_type_is_deduced=*/FALSE,
                                /*try_user_conversions=*/FALSE,
                                arg_match);
      if (arg_match->match_level == aml_none) {
        /* This copy constructor cannot be used. */
        goto reject_function;
      }  /* if */
      und_param_type = type_pointed_to(param_type);
      qualifiers = get_type_qualifiers(und_param_type);
      if (source_is_rvalue && 
          ((qualifiers & TQ_CONST) == 0 ||
           (qualifiers & (TQ_CONST | TQ_VOLATILE)) ==
                         (TQ_CONST | TQ_VOLATILE))) {
        /* A copy constructor whose input parameter is a reference to
           non-const or a reference to const volatile cannot copy an
           rvalue.  Keep looking for a suitable copy constructor, but
           remember this one in case it's the best we find. */
        if (uncallable_sym != NULL) {
          /* There's more than one uncallable copy constructor, so we
             can't return just one. */
          multiple_uncallable = TRUE;
        } else {
          uncallable_sym = sym;
        }  /* if */
        goto reject_function;
      }  /* if */
      /* sym represents a suitable copy constructor.  Add it to the
         list of viable functions. */
      if (sym->kind == (a_symbol_kind)sk_function_template) {
        /* The symbol is a function template. */
        add_function_template_to_candidate_functions_list(
                                         sym,
                                         /*expl_template_arg_list_used=*/FALSE,
                                         template_arg_list,
                                         arg_match,
                                         &candidate_functions);
      } else {
        /* The symbol is a normal function. */
        add_function_to_candidate_functions_list(sym,
                                                 arg_match,
                                                 &candidate_functions);
      }  /* if */
      goto next_function;
reject_function:
      /* The function is not viable. */
      /* Free any argument match summary entry built for it. */
      free_arg_match_summary_list(arg_match);
      /* Free any template argument list built for it. */
      free_template_arg_list(template_arg_list);
next_function:;
      /* Keep looping to try all the functions in the overload set. */
    }  /* for */
    /* Pick the best copy constructor. */
    select_best_candidate_functions(&candidate_functions, pos,
                                    &undecidable_because_of_error, ambiguous);
    cctor_sym = NULL;
    if (undecidable_because_of_error) {
      /* Previous error. */
    } else if (candidate_functions == NULL) {
      /* There are no viable conversion functions. */
    } else if (*ambiguous) {
      /* There are several equally desirable functions. */
    } else {
      /* There is exactly one best function. */
      cctor_sym = candidate_functions->function_symbol;
    }  /* if */
    /* Free the candidate functions list. */
    free_candidate_function_list(candidate_functions);
    if (cctor_sym == NULL && uncallable_sym != NULL && !multiple_uncallable &&
        uncallable != NULL) {
      /* We have no copy constructor that is suitable, but we did find
         exactly one copy constructor that would have been suitable except
         that it's not callable. */
      cctor_sym = uncallable_sym;
      *uncallable = TRUE;
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 4 || db_flag_is_set("overload")) {
    db_display_overload_level();
    db_symbol(cctor_sym,
              "Leaving select_overloaded_copy_constructor, cctor_sym = ", 4);
  }  /* if */
  overload_level--;
#endif /* DEBUG */
  db_exit();
  return cctor_sym;
}  /* select_overloaded_copy_constructor */


a_routine_ptr select_assignment_operator_for_memberwise_copy(
                                              a_type_ptr        class_type,
                                              an_expr_node_ptr  source_expr,
                                              an_expr_node_ptr  dest_expr,
                                              a_source_position *dest_decl_pos)
/*
Perform overload resolution to select the assignment operator to use in
copying a base or member subobject in the implicit definition of a copy
assignment operator and return a pointer to the selected routine.  If an
error is detected, the returned value will be NULL (and the appropriate
diagnostic will have been issued).  Note that this routine should not be
called directly but only via find_assignment_operator_for_memberwise_copy.
Its processing is similar to that of check_for_operator_overloading, except
that the context is more restricted and thus fewer possibilities need to be
handled here.

source_expr is an lvalue that refers to the base or member subobject of the
class object that is being copied; dest_expr is an lvalue that refers to the
corresponding subobject of the target object.  class_type is the type of the
subobject to be copied.  dest_decl_pos is the position in the class
definition of the base specifier or member declaration for the subobject
to be copied.
*/
{
  an_operand               operand_1, operand_2;
  an_arg_operand_ptr       arg_operand_list;
  a_symbol_ptr             member_functions_symbol;
  a_symbol_ptr             function_symbol, proj_function_symbol;
  a_candidate_function_ptr candidate_functions;
  a_boolean                matched_except_for_missing_selector = FALSE;
  a_boolean                matched_except_for_selector = FALSE;
  a_boolean                ambiguous;
  a_boolean                undecidable_because_of_error;
  a_routine_ptr            rout = NULL;

  check_assertion(is_immediate_class_type(class_type));
  if (class_type->variant.class_struct_union.copy_assignment_decl_suppressed) {
    /* In order to emulate the behavior of the Microsoft compiler, we
       suppress the declaration of the implicit copy assignment operator
       for a class that has a member with reference or const-qualified
       type; this allows overload resolution to select a different
       assignment operator for the copy.  However, the Microsoft compiler
       only does this for direct assignments; when generating the
       definition of an implicitly-declared copy assignment operator, it
       reports an error for such subobjects, rather than performing
       overload resolution among the remaining assignment operators. */
    if (expr_error_should_be_issued()) {
      pos_ty_error(ec_no_suitable_assignment_operator, dest_decl_pos,
                   class_type);
    }  /* if */
  } else {
#if DEBUG
    overload_level++;
#endif /* DEBUG */
    /* Make operands from the source and destination expressions.  The
       dest_expr operand represents the "this" pointer of the
       operator= and thus we want to convert it to an object pointer. */
    make_lvalue_expression_operand(dest_expr, &operand_1);
    take_address_of_lvalue(&operand_1, ((a_source_position *)NULL));
    operand_1.selector_is_object_pointer = TRUE;
    make_lvalue_expression_operand(source_expr, &operand_2);
    /* Change the source operand into argument operand form. */
    arg_operand_list = alloc_arg_operand();
    copy_operand(&operand_2, &arg_operand_list->operand);
    /* candidate_functions will contain the list of viable functions. */
    candidate_functions = NULL;
    member_functions_symbol = opname_member_function_symbol(
                                                 (an_opname_kind)onk_assign,
                                                 class_type);
    if (member_functions_symbol != NULL) {
      /* There are assignment operators for this class type.  See how well they
         match up.  Use the first operand as the selector expression, and the
         second operand as the actual argument. */
      try_overloaded_function_match(member_functions_symbol,
                                    /*is_template_id=*/FALSE,
                                    (a_template_arg_ptr)NULL,
                                    arg_operand_list,
                                    /*have_selector=*/TRUE,
                                    &operand_1,
                                    /*ctor_conversion_case=*/FALSE,
                                    /*initializing_return_value=*/FALSE,
                                    /*effects_copy_initialization=*/FALSE,
                                    /*allow_udc_on_arguments=*/FALSE,
                                    /*arg_dep_lookup_done=*/FALSE,
                                    /*from_arg_dep_lookup=*/FALSE,
                                    /*dependent_call=*/FALSE,
                                    /*forced_dependent=*/FALSE,
                                    /*known_to_be_visible=*/TRUE,
                                    /*is_overloaded_operator=*/TRUE,
                                    &candidate_functions,
                                    &matched_except_for_missing_selector,
                                    &matched_except_for_selector);
      /* The candidate_functions list now contains all the viable functions.
         Find the best. */
      select_best_candidate_functions(&candidate_functions, dest_decl_pos,
                                      &undecidable_because_of_error,
                                      &ambiguous);
      if (undecidable_because_of_error) {
        /* There was a previously-reported error. */
      } else if (candidate_functions == NULL) {
        /* There is no applicable operator= function. */
        if (expr_error_should_be_issued()) {
          if (get_type_qualifiers(source_expr->type) == TQ_CONST) {
            /* The common case: missing const assignment operator function. */
            pos_ty_error(ec_missing_const_assignment_operator, dest_decl_pos,
                         class_type);
          } else {
            /* Unusual case: volatile or const-volatile expected. */
            pos_ty_error(ec_no_suitable_assignment_operator, dest_decl_pos,
                         class_type);
          }  /* if */
        }  /* if */
      } else if (ambiguous) {
        /* More than one operator= function applies and is a best match. */
        if (expr_error_should_be_issued()) {
          pos_ty_error(ec_ambiguous_assignment_operator, dest_decl_pos,
                       class_type);
        }  /* if */
      } else {
        /* Exactly one operator= function applies and is best. */
        proj_function_symbol = candidate_functions->function_symbol;
        function_symbol = fundamental_symbol_of(proj_function_symbol);
        /* Check that the function is accessible and mark it referenced. */
        expr_reference_to_implicitly_invoked_function(proj_function_symbol,
                                                      dest_decl_pos,
                                                      (a_type_ptr)NULL,
                                                      /*honor_virtual=*/FALSE);
        rout = function_symbol->variant.routine.ptr;
      }  /* if */
      free_candidate_function_list(candidate_functions);
    }  /* if */
    free_arg_operand_list(arg_operand_list);
#if DEBUG
    overload_level--;
#endif /* DEBUG */
  }  /* if */
  return rout;
}  /* select_assignment_operator_for_memberwise_copy */


void deduce_auto_type(a_decl_parse_state  *dps)
/*
*dps describes a declaration of an "auto" variable, including its initializer.
Deduce the "auto" type specifier and store the resulting type in dps->type.
Deduction failures are diagnosed as errors.
*/
{
  an_arg_operand_ptr    auto_arg_operand;
  an_operand            *arg;
  a_template_param_ptr  templ_param;
  a_template_arg_ptr    templ_arg = NULL;
  a_type_ptr            type = dps->declared_type, orig_type = type;
  a_type_ptr            arg_type;
  a_type_ptr            qc_param_type = NULL;
  a_type_ptr            qc_arg_type = NULL;
  a_boolean             subst_error = FALSE;

  check_assertion(dps->auto_type_specifier_seen && dps->auto_type != NULL);
  auto_arg_operand = dps->prescanned_initializer_cache.first_expression;
  check_assertion(auto_arg_operand != NULL && auto_arg_operand->next == NULL);
  arg = &auto_arg_operand->operand;
  arg_type = arg->type;
  if (is_managed_nullptr_type(arg_type)) {
    /* The Microsoft C++/CLI compiler deduces std::nullptr_t from an auto
       initializer of the managed nullptr type. */
    arg_type = standard_nullptr_type();
  }  /* if */
  dps->type = NULL;
  templ_param = alloc_template_param(symbol_for(dps->auto_type));
  /* Adjust the argument and parameter types for deduction.  Some types can
     never succeed: Issue an error and don't attempt deduction any further. */
  if (!adjust_deduction_pair(&type, &arg_type, arg, templ_param,
                             (a_template_arg *)NULL,
                             &qc_param_type, &qc_arg_type,
                             (a_boolean *)NULL)) {
    expr_pos_error(ec_cannot_deduce_auto_type, &dps->auto_pos);
    goto set_type;
  }  /* if */
  if (!deduce_from_one_pair(type, arg_type, qc_param_type, qc_arg_type,
                            &templ_arg, templ_param)) {
    /* Deduction failed. */
    expr_pos_error(ec_cannot_deduce_auto_type, &dps->auto_pos);
    goto set_type;
  }  /* if */
  if (templ_arg == NULL) {
    /* Deduction produced no argument because an error type was involved. */
    check_assertion(total_errors != 0 &&
                    is_or_contains_error_type(orig_type));
    goto set_type;
  }  /* if */
  check_assertion(templ_arg->kind == (a_templ_arg_kind)tak_type);
  /* Substitute the deduced type to obtain the actual type for the current
     declaration.  A substitution failure is an error. */
  dps->type = copy_type_with_substitution(orig_type, templ_arg, templ_param,
                                          &dps->declarator_pos,
                                          CTWS_NO_OPTIONS, &subst_error);
  if (subst_error) {
    /* Substitution failed. */
    expr_pos_error(ec_cannot_deduce_auto_type, &dps->auto_pos);
    dps->type = NULL;
    goto set_type;
  }  /* if */
  if (dps->deduced_auto_type != NULL &&
      !identical_types(dps->deduced_auto_type, templ_arg->variant.type)) {
    /* This is a declaration with multiple declarators and the type deduced
       for a previous declarator is not consistent with the current deduction:
       Issue an error. */
    if (expr_error_should_be_issued()) {
      pos_ty2_error(ec_inconsistent_deduction_of_auto, &dps->declarator_pos,
                    templ_arg->variant.type, dps->deduced_auto_type);
    }  /* if */
  }  /* if */
  /* Record the type deduced for the "auto" specifier. */
  dps->deduced_auto_type = templ_arg->variant.type;
  /* Check that the actual (deduced) type of the declaration is applicable to
     the declared entity (in particular, this checks for compatibility with
     previous declarations of the same entity). */
  check_deduced_auto_type(dps);
set_type:
  if (dps->type == NULL) {
    /* An error occurred: Recover with an error type and proceed as if "auto"
       had not been seen. */
    dps->specifiers_type = dps->deduced_auto_type = dps->type = error_type();
    dps->auto_type_specifier_seen = FALSE;
  }  /* if */
  if (dps->sym != NULL) {
    /* Update the type in the IL entry. */
    if (dps->sym->kind == (a_symbol_kind)sk_variable) {
      dps->sym->variant.variable.ptr->type = dps->type;
    } else if (dps->sym->kind == (a_symbol_kind)sk_static_data_member) {
      dps->sym->variant.static_data_member.variable->type = dps->type;
    } else {
      unexpected_condition();
    }  /* if */
  }  /* if */
  if (templ_arg != NULL) free_template_arg_list(templ_arg);
}  /* deduce_auto_type */


void overload_init(void)
/* 
Initialize things related to overload resolution in expression scanning.
These are initializations that must be redone for each compilation.
*/
{
  /* In overload.h: */
  avail_candidate_functions = NULL;
  avail_arg_match_summaries = NULL;
#if DEBUG
  num_candidate_functions_allocated = 0;
  overload_level = 0;
#endif /* DEBUG */
}  /* overload_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2010 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
