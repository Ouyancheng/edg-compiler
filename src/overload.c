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

/* Forward declarations required because of out-of-order references. */
static void prep_conversion_operand(an_operand        *source_operand,
                                    a_type_ptr        dest_type,
                                    a_conv_descr      *conversion,
                                    a_boolean         is_copy_initialization,
                                    a_boolean         nontype_template_arg,
                                    an_error_code     incompatible_err,
                                    a_source_position *err_pos);
static a_boolean operand_is_temp_init(an_operand *operand);


/*
Return TRUE if the indicated symbol is invisible because it was
declared in a friend declaration and not confirmed with an explicit
declaration, when friend injection is turned off.
*/
#define symbol_is_invisible_friend(sym) \
  ((sym)->is_invisible && !(sym)->is_class_member)


static void clear_conv_descr(a_conv_descr_ptr conv)
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
  clear_std_conv_descr(&conv->std);
}  /* clear_conv_descr */


a_symbol_ptr find_addr_of_overloaded_function_match(
                                a_symbol_ptr       ovl_sym,
                                a_boolean          is_template_id,
                                a_template_arg_ptr template_arg_list,
                                a_type_ptr         dest_type,
                                a_boolean          is_cast,
                                an_arg_match_level *match_level,
                                a_std_conv_descr   *std_conv,
                                a_boolean          *unknown_dependent_function,
                                a_boolean          *ambiguous)
/*
ovl_sym is the symbol from an indefinite function operand representing
the address of an overloaded function.  (For completeness, ovl_sym
can be a simple function or a template).  is_template_id is TRUE if ovl_sym
is followed by an explicit template argument list, in which case
template_arg_list gives the argument list.  The indefinite function is
being converted to a destination type dest_type.  If dest_type is a
pointer, reference, or pointer-to-member type that could be a
pointer/reference to one of the overloaded functions, return a pointer
to that function's symbol (possibly a projection symbol); otherwise,
return NULL.  Also set *match_level to indicate whether or not any
conversion is needed after the coercion to a specific function pointer
and set *std_conv to indicate any such conversion.  If the function
cannot be determined because some template-dependent types are involved
(in a prototype instantiation), return NULL and *unknown_dependent_function
TRUE.  If more than one function matches, return NULL and *ambiguous TRUE.
See WP [over.over], and ARM 13.3, "Address of Overloaded Function".  If
is_cast is TRUE, this disambiguation is being done via an explicit
cast.
*/
{
  a_boolean        is_ptr = FALSE, is_ref = FALSE, is_ptr_to_member = FALSE;
  a_boolean        sym_is_list, need_templates_pass;
  a_boolean        dest_type_has_type_qualifiers = FALSE;
  a_type_ptr       routine_type, dest_class, ptr_routine_type;
  a_type_ptr       dest_underlying_type;
  a_symbol_ptr     sym, proj_sym, match_sym = NULL;
  unsigned long    number_of_matches = 0;
  a_std_conv_descr std_conversion;
  a_boolean        exception_spec_checked = FALSE;

  db_enter(4, "find_addr_of_overloaded_function_match");
  clear_std_conv_descr(std_conv);
  *ambiguous = FALSE;
  *unknown_dependent_function = FALSE;
  if (is_template_dependent_context() &&
      (is_template_dependent_type(dest_type) ||
       (is_template_id &&
        template_arg_list_involves_template_param(template_arg_list)))) {
    /* The destination type is not fully known, or the template argument
       list contains template-dependent types (in a prototype
       instantiation). */
    *unknown_dependent_function = TRUE;
  } else if (is_pointer_type(dest_type)) {
    dest_class = NULL;
    is_ptr = TRUE;
    dest_underlying_type = type_pointed_to(dest_type);
  } else if (is_reference_type(dest_type)) {
    dest_class = NULL;
    is_ref = TRUE;
    dest_underlying_type = type_pointed_to(dest_type);
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
    if (is_template_id) {
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
            if (dest_class ==
                   (routine_type_is_nonstatic_member_function(routine_type) ?
                                              sym->parent.class_type : NULL)) {
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
          a_boolean  is_new_template_instance;
          match_sym = matching_template_function(sym, dest_underlying_type,
                                                 template_arg_list,
                                                 is_template_id,
                                                 /*is_decl_context=*/FALSE,
                                                 &is_new_template_instance);
          *match_level = aml_exact;
          number_of_matches = 1;
        }  /* if */
      }  /* if */
    }  /* if */
    if (number_of_matches == 0 && !is_ref) {
      /* Try matches involving an implicit conversion.  This is here
         primarily for the pointer-to-member case, but it makes sense to
         handle the normal pointer case too in case the implicit conversion
         rules change (also, it makes the error message clearer in the
         case where dest_type is "void *").  Implicit conversions are
         not attempted for the reference case. */
      for (proj_sym = ovl_sym;
           proj_sym != NULL;
           proj_sym = (sym_is_list ? proj_sym->next : NULL)) {
        /* Remove projections added for namespaces, if any. */
        sym = fundamental_symbol_of(proj_sym);
        if (symbol_is_invisible_friend(proj_sym)) {
          /* Ignore invisible symbols from friend declarations. */
        } else if (sym->kind == (a_symbol_kind)sk_function_template) {
          /* Template.  Could be converted to "void *", but that would always
             be ambiguous. */
          if (is_ptr && is_void_type(dest_underlying_type)) {
            goto is_ambiguous;
          }  /* if */
        } else if (is_template_id) {
          /* There is an explicit template argument list, so do not consider
             non-templates. */
        } else {
          /* Not a function template (i.e., a normal function). */
          routine_type = routine_symbol_type(sym);
          if (routine_type_is_nonstatic_member_function(routine_type)) {
            /* The class of the pointer to member is always the class in
               which the function is defined, not any derived class
               indicated in the projection symbol. */
            ptr_routine_type = ptr_to_member_type(routine_type,
                                                  sym->parent.class_type);
          } else {
            ptr_routine_type = make_pointer_type(routine_type);
          }  /* if */
          /* See if the type of the function can be converted to the required
             destination type.  For the explicit cast case, use
             static_cast_conversion_possible to find cases of a
             pointer-to-member of a derived class cast to a pointer-to-member
             of a base class.  expl_conversion_possible would be too broad
             because it would also allow changing the member type. */
          if (ptr_routine_type != NULL &&
              (is_cast ?
                (clear_std_conv_descr(&std_conversion),
                 static_cast_conversion_possible(
                                     ptr_routine_type,
                                     /*source_is_constant=*/FALSE,
                                     /*source_is_string_literal=*/FALSE,
                                     (a_constant_ptr)NULL,
                                     dest_type,
                                     /*allow_qualifier_or_eh_mismatch=*/FALSE,
                                     ec_no_error,
                                     &std_conversion.warning_suggested)) :
                impl_conversion_possible(ptr_routine_type,
                                         /*source_is_constant=*/FALSE,
                                         /*source_is_string_literal=*/FALSE,
                                         (a_constant_ptr)NULL,
                                         dest_type,
                                     /*allow_qualifier_or_eh_mismatch=*/FALSE,
                                         /*suppress_extensions=*/TRUE,
                                         ec_no_error,
                                         &std_conversion))) {
            /* A match. */
            match_sym = proj_sym;
            *match_level = aml_std_conversion;
            *std_conv = std_conversion;
            number_of_matches++;
            exception_spec_checked = TRUE;
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
    if (number_of_matches > 1) {
is_ambiguous:
      /* Ambiguous case. */
      *ambiguous = TRUE;
      match_sym = NULL;
    }  /* if */
  }  /* if */
  if (match_sym != NULL) {
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
  if (debug_level >= 4) {
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
                                                   guide_type,
                                                   /*is_cast=*/FALSE,
                                                   &match_level,
                                                   &std_conversion,
                                                   &unknown_dependent_function,
                                                   &ambiguous);
  if (func_sym == NULL ||
      !conversion_allowed_for_nontype_template_argument(&std_conversion) ||
      std_conversion.exception_spec_incompatibility) {
    *err = TRUE;
  } else {
    /* Build a pointer-to-function or pointer-to-member constant. */
    a_routine_ptr routine = func_sym->variant.routine.ptr;
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


static
a_boolean indefinite_function_can_be_template_arg(an_operand   *operand,
                                                  a_type_ptr   param_type,
                                                  a_type_ptr   *arg_type,
                                                  a_symbol_ptr templ_sym)
/*
operand is an indefinite function operand.  See if it can be matched against
a parameter of type param_type from a function template.  If so, return
TRUE and set *arg_type to the argument type to use.  *arg_type is not
changed if this function returns FALSE.  templ_sym points to the
function template symbol associated with the template associated with
param_type.
*/
{
  a_boolean    can_be_arg = FALSE;
  a_symbol_ptr sym = operand->variant.symbol, proj_sym;
  a_type_ptr   matching_arg_type = NULL;

  reduce_projection_symbol_to_fundamental_symbol(sym);
  if (sym->kind == (a_symbol_kind)sk_function_template) {
    /* There's no way to match up a function template as an argument to a
       function template. */
    /* can_be_arg = FALSE;  -- already set. */
  } else {
    a_template_symbol_supplement_ptr	tssp;
    check_assertion(sym->kind == (a_symbol_kind)sk_overloaded_function);
    tssp = template_supplement_for_symbol(templ_sym);
    for (proj_sym = sym->variant.overloaded_function.symbols;
         proj_sym != NULL;
         proj_sym = proj_sym->next) {
      /* Remove projections for namespaces, if any. */
      sym = fundamental_symbol_of(proj_sym);
      if (sym->kind == (a_symbol_kind)sk_function_template) {
        /* There's no way to match up a function template as an argument to a
           function template, so ignore this symbol. */
      } else {
        /* Not a function template. */
        a_type_ptr routine_type = routine_symbol_type(sym), ptr_routine_type;
        if (routine_type_is_nonstatic_member_function(routine_type)) {
          ptr_routine_type = ptr_to_member_type(routine_type,
                                                sym->parent.class_type);
        } else {
          ptr_routine_type = make_pointer_type(routine_type);
        }  /* if */
        if (tentatively_matches_template_type(
                    ptr_routine_type, param_type,
                    tssp->variant.function.decl_cache.decl_info->parameters)) {
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
  amsp->const_anachronism          = FALSE;
  amsp->is_match_for_this_param    = FALSE;
  amsp->arg_is_constant            = FALSE;
  amsp->param_type                 = NULL;
  amsp->guide_type                 = NULL;
  clear_conv_descr(&amsp->conversion);
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
  }  /* if */
  if (amsp->match_level == aml_user_conversion &&
      amsp->conversion.std.nontrivial_conversion) {
    if (amsp->conversion.std.promotion) {
      fprintf(f_debug, " (plus promotion)");
    } else {
      fprintf(f_debug, " (plus conversion)");
    }  /* if */
  }  /* if */
  if (amsp->conversion.std.type_qualifiers_added) {
    fprintf(f_debug, " (type qualifiers added)");
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
  cfp->is_user_conversion = FALSE;
  clear_conv_descr(&cfp->conversion);
  cfp->specific_type = NULL;
  cfp->arg_matches = NULL;
  cfp->current_arg_match = NULL;
  cfp->next_in_arg_best_match_set = NULL;
  cfp->in_best_match_set = FALSE;
  cfp->in_best_match_set_for_some_argument = FALSE;
  cfp->in_best_match_set_for_curr_argument = FALSE;
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
  octl.output_str = put_str_to_temp_text_buffer;
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
resolution problem.
*/
{
  if (!is_error_type(object_type)) {
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
			/* First operand must be an lvalue. */
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
#define OBJECT_POINTER_TYPE_CODE 'O'
			/* Pointer to object type. */
#define FUNCTION_POINTER_TYPE_CODE 'F'
			/* Pointer to function. */
#define PTR_TO_MEMBER_TYPE_CODE 'M'
			/* Pointer to member. */
#define BOOL_TYPE_CODE 'B'
			/* bool. */
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
    case OBJECT_POINTER_TYPE_CODE:
      str = "pointer-to-object";
      break;
    case FUNCTION_POINTER_TYPE_CODE:
      str = "pointer-to-function";
      break;
    case PTR_TO_MEMBER_TYPE_CODE:
      str = "pointer-to-member";
      break;
    case BOOL_TYPE_CODE:
      str = "bool";
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
                                                         a_type_ptr arg_type,
                                                         a_type_ptr param_type)
/*
Return TRUE if when initializing a parameter of type param_type (a reference
type) from an argument of type arg_type (an array type), the array -->
pointer transformation should be done.
*/
{
  a_boolean transform_needed = TRUE, dropping_qualifiers;
  a_boolean ref_to_const, ref_to_const_volatile, binding_to_rvalue_allowed;

  /* The array --> pointer transformation is wanted except when initializing
     a reference to the right array type, e.g.,
       char (&r)[4] = "abc";
  */
  if (direct_reference_binding_possible((an_operand *)NULL,
                                        arg_type,
                                        param_type,
                                        &ref_to_const,
                                        &ref_to_const_volatile,
                                        &binding_to_rvalue_allowed,
                                        &dropping_qualifiers,
                                        (a_symbol **)NULL)) {
    transform_needed = FALSE;
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

  /* The function --> pointer transformation is wanted except when
     initializing a reference to the right function type, as in
       void f();
       void (&r)() = f;
  */
  if (direct_reference_binding_possible((an_operand *)NULL,
                                        arg_type,
                                        param_type,
                                        &ref_to_const,
                                        &ref_to_const_volatile,
                                        &binding_to_rvalue_allowed,
                                        &dropping_qualifiers,
                                        (a_symbol **)NULL)) {
    transform_needed = FALSE;
  }  /* if */
  return transform_needed;
}  /* function_transformation_needed_on_reference_init */


static a_boolean conversion_for_direct_reference_binding_possible(
                                      an_operand               *source_operand,
                                      a_type_ptr               dest_type,
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
must free that list.
*/
{
  a_boolean  okay;
  a_type_ptr base_dest_type = type_pointed_to(dest_type);

  if (microsoft_bugs &&
      (!is_an_lvalue(source_operand) || operand_is_temp_init(source_operand))){
    /* The Microsoft compiler (VC++ 6.0) implements an older rule in
       the Working Paper that does not allow a conversion function
       to be used for a direct reference binding unless the original
       expression is an lvalue.  Note that because there is another
       Microsoft change that makes the result of a function call that
       returns a class into an lvalue, we have to test for temp init
       expressions specially. */
    okay = FALSE;
    *ambiguous = FALSE;
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
    /* The flag here is deliberately not set when *ambiguous is TRUE. */
    if (okay) conversion->conversion_for_direct_reference_binding = TRUE;
  }  /* if */
  return okay;
}  /* conversion_for_direct_reference_binding_possible */


static void determine_arg_match_level(
                               an_operand           *arg_operand,
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
  a_boolean         param_is_reference;
  a_boolean         source_can_be_rvalue = TRUE;
  a_boolean         param_is_class_type, arg_is_class_type;
  a_boolean         ref_type_qualifiers_dropped, ref_type_qualifiers_added;
  a_boolean         uses_type_qualifiers_dropped_anachronism = FALSE;
  a_std_conv_descr  std_conversion;
  a_base_class_ptr  bcp;
  a_boolean         ambiguous;
  a_boolean         arg_operand_is_constant, arg_converted_to_rvalue = FALSE;
  a_boolean         arg_operand_is_simple_string_literal;
  a_constant_ptr    arg_operand_constant;
  an_operand        implicit_arg_operand;
  a_type_ptr        orig_param_type = param_type;
  a_type_ptr        unqual_arg_type, unqual_param_type;

  db_enter(4, "determine_arg_match_level");
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
  }  /* if */
  /* Try an exact match or one involving trivial conversions.  This is
     case [1] in the ARM.  Trivial conversions are
       From:      To:
       T          T&
       T          qualified T
       T*         (qualified T)*
       T[]        T*
       T(args)    (*T)(args)
       T(args)    (X::*T)(args)    -- not in ARM (extension)
     T& --> T is done automatically in expression processing; an argument
     here never has a reference type T& -- it's an lvalue of type T.  Cases
     that involve
       From:      To:
       T*         (qualified T)*
       T          (qualified T)&
     (the latter coming from T --> qualified T --> (qualified T)&, and making
     more sense here than the ARM's T& --> (qualified T)& because the argument
     cannot be a reference) are considered worse than those that do not
     involve them.
  */
  /* Remove parts of the param type that could be added by trivial
     conversions, hoping thereby to end up with the arg type. */
  ref_type_qualifiers_dropped = ref_type_qualifiers_added = FALSE;
  param_is_reference = is_reference_type(param_type);
  /* See if the array --> pointer and function --> pointer transformations
     should be done. */
  if (is_array_type(arg_type) &&
      (!param_is_reference ||
       array_transformation_needed_on_reference_init(arg_type, param_type))) {
    /* Simulate the array --> pointer transformation.  After the transformation
       we have only a type for the argument, and no arg_operand. */
    arg_type = type_after_array_to_pointer_transformation(arg_type);
    arg_operand = NULL;
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
  }  /* if */
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
    /* The reference can bind to an rvalue if it is a reference to const. */
    /* Note that, as of Feb. 1995, the WP does not require the test for
       const volatile here.  It's not clear whether that's an oversight or
       not.  (A core working group discussed it in Austin in March 1995
       and decided it didn't care to bring it up in full committee.) */
    if ((microsoft_bugs && param_type_is_deduced) ||
        any_cfront_mode() ||
        allow_anachronisms) {
      /* A reference to non-const that's deduced can bind to an rvalue in
         Microsoft bugs mode (VC++ 6.0).  A reference to non-const can
        bind to an rvalue in cfront mode or anachronisms mode. */
      source_can_be_rvalue = TRUE;
    } else {
      /* Normal case.  A reference can bind to an rvalue only if it's
         a reference to const. */
      source_can_be_rvalue = ((param_type_qualifiers & TQ_CONST) != 0);
    }  /* if */
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
      a_constant_ptr con_var_value = NULL;
      if (is_constant_operand(arg_operand)) {
        a_constant_ptr con = &arg_operand->variant.constant;
        if (con_is_exact_addr_of_variable(con)) {
          con_var_value =
                     var_constant_value(con->variant.address.variant.variable);
        }  /* if */
      } else if (is_expression_operand(arg_operand)) {
        con_var_value =
             value_of_constant_var_lvalue_expr(arg_operand->variant.expression,
                                               (a_variable **)NULL);
      }  /* if */
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
                                                      &arg_operand_constant);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    }  /* if */
  }  /* if */
  arg_summary->arg_is_constant = arg_operand_is_constant;
  /* If the type qualifiers are not okay, do not check for the simple
     matches; go directly to user-defined conversions (which do their own
     variety of checking of type qualifiers). */
  if (!ref_type_qualifiers_dropped) {
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
        arg_summary->conversion.
                      user_conversion_for_class_copy_must_be_determined = TRUE;
      }  /* if */
      goto have_level;
    }  /* if */
    /* Check for another exact match case, for pointers involving addition
       of type qualifiers on the type pointed to (the "T* --> qualified T *"
       case). */
    if (is_pointer_type(param_type) && is_pointer_type(arg_type)
#ifdef pointer_types_have_same_repr
        /* Checking that the pointer representation is the same here forces
           a conversion that changes pointer representation to be a
           conversion, not an exact match.  That is a language choice on
           an extension.  If that's not right, remove this. */
        && pointer_types_have_same_repr(param_type, arg_type)
#endif /* ifdef pointer_types_have_same_repr */
                                                             ) {
      a_type_ptr arg_type_pointed_to = type_pointed_to(arg_type);
      a_type_ptr param_type_pointed_to = type_pointed_to(param_type);
      if (qualification_conversion_possible(arg_type_pointed_to,
                                            param_type_pointed_to,
                                            (a_boolean *)NULL,
                                            /*ignore_underlying_type=*/FALSE)){
        /* Some qualifiers are being added.  This is the
           "T* --> qualified T *" case, which should be remembered
           as a possible tie-breaker later.  Note that the case where
           the types are the same would have been handled earlier and
           would not come here. */
        arg_summary->match_level = aml_exact;
        arg_summary->conversion.std.type_qualifiers_added = TRUE;
        goto have_level;
      }  /* if */
    }  /* if */
    if (arg_operand != NULL && is_indefinite_function_operand(arg_operand) &&
        (!param_is_reference || is_a_function_designator(arg_operand))) {
      /* The source is an indefinite function, i.e., the address of an
         overloaded function.  It can be converted to an appropriate
         pointer, reference, or pointer-to-member type.  For the
         pointer and pointer-to-member cases, the operand can be a function
         designator or pointer to function; for the reference case it
         must be a function designator. */
      a_boolean unknown_dependent_function;
      if (find_addr_of_overloaded_function_match(arg_operand->variant.symbol,
                                                 (a_boolean)arg_operand->
                                                                is_template_id,
                                                 arg_operand->
                                                             template_arg_list,
                                                 orig_param_type,
                                                 /*is_cast=*/FALSE,
                                                 &arg_summary->match_level,
                                                 &std_conversion,
                                                 &unknown_dependent_function,
                                                 &ambiguous) != NULL ||
          unknown_dependent_function ||
          ambiguous) {
        /* There is a suitable indefinite function, or more than one.
           arg_summary->match_level has been set appropriately. */
        arg_summary->conversion.std = std_conversion;
        if (ambiguous) arg_summary->conversion.unusable = TRUE;
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
          !arg_operand->is_cfront_null_pointer_constant)) {
      /* Match with standard conversions. */
      arg_summary->match_level = aml_std_conversion;
      arg_summary->conversion.std = std_conversion;
      arg_converted_to_rvalue = TRUE;
      if (std_conversion.promotion) {
        /* This standard conversion is a promotion. */
        arg_summary->match_level = aml_promotion;
      } else if (std_conversion.conv_of_string_literal_to_ptr_to_nonconst) {
        /* The deprecated conversion from a string literal to a pointer to
           nonconst counts as an exact match (it's worse than other exact
           matches that do not use that deprecated conversion). */
        arg_summary->match_level = aml_exact;
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
    if (param_is_reference && arg_is_class_type &&
        (conversion_for_direct_reference_binding_possible(orig_arg_operand,
                                                          orig_param_type,
                                                          &conversion,
                                                          &ambiguous,
                                           (a_candidate_function_ptr *)NULL) ||
         ambiguous)) {
      /* The parameter is a reference, and there exists a conversion function
         that can convert the argument to an lvalue that the reference can
         bind to directly. */
      set_arg_summary_for_user_conversion(arg_summary, &conversion,
                                          orig_param_type, param_is_reference);
      goto have_level;
    } else if (ref_type_qualifiers_dropped &&
               (identical_types(unqual_arg_type, unqual_param_type) ||
                (arg_is_class_type && param_is_class_type &&
                 find_base_class_of(arg_type, param_type) != NULL))) {
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
                                             /*is_copy_initialization=*/TRUE,
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
    } else if (arg_is_class_type &&
               (conversion_from_class_possible(orig_arg_operand, param_type,
                                               (a_builtin_type_kind_set)
                                                                      BTK_NONE,
                                               /*need_lvalue_result=*/
                                                         !source_can_be_rvalue,
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
      arg_summary->anachronism_used = TRUE;
    }  /* if */
    if (!source_can_be_rvalue &&
        (arg_converted_to_rvalue ||
         (arg_operand != NULL && is_an_rvalue(arg_operand)))) {
      /* You can't bind a reference to non-const to an rvalue.  This was a
         post-ARM change (in the ARM, the binding would be okay in overload
         resolution and would get an error later if chosen). */
      if (allow_nonconst_ref_anachronism && param_is_class_type) {
        /* The anachronism of binding a reference to nonconst to a class
           rvalue is enabled.  Leave this alone.  A warning will be issued
           is this binding is actually used. */
      } else {
        arg_summary->match_level = aml_none;
      }  /* if */
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 4) {
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


static void determine_selector_match_level(a_type_ptr           arg_type,
                                           a_type_ptr           param_type,
                                           an_arg_match_summary *match_summary)
/*
Determine how well a selector argument of type arg_type matches a "this"
parameter with type param_type.  match_summary is set to indicate the level
of match.  If the anachronism of allowing a call of a non-const function
with a const selector is enabled, allow that kind of mismatch here.
*/
{
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
      match_summary->anachronism_used = TRUE;
    }  /* if */
  }  /* if */
}  /* determine_selector_match_level */


void selector_match_with_this_param(
                               an_operand           *bound_function_selector,
                               a_boolean            selector_is_object_pointer,
                               a_routine_ptr        rout,
                               a_type_ptr           this_param_type,
                               an_arg_match_summary *this_match_summary)
/*
Determine how well the selector object indicated by *bound_function_selector
matches the "this" parameter (of type this_param_type) of a member function.
If selector_is_object_pointer is TRUE, *bound_function_selector has
already been converted to object pointer form.  Otherwise, it's just
an object (lvalue or rvalue).  Return the match summary in *this_match_summary.
If the specific routine being called is known, rout points to the
routine entry; otherwise, rout is NULL.  rout must be non-NULL when
calling a constructor or destructor, so that those can be treated as a
special case: constructors and destructors can be called for const- and
volatile-qualified objects even though they themselves are not (and
cannot be) const- or volatile-qualified.  bound_function_selector is
not used in that case, and can be NULL.
*/
{
  a_type_ptr selector_type;
  a_type_ptr ptr_selector_type;

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
    /* Determine the effective selector type. */
    selector_type = bound_function_selector->type;
    if (m_is_error_type(selector_type)) {
      /* An error type matches anything, but not very well. */
      clear_arg_match_summary(this_match_summary);
      this_match_summary->match_level = aml_error;
      this_match_summary->is_match_for_this_param = TRUE;
    } else {
      if (selector_is_object_pointer) {
        if (is_template_param_type(selector_type)) {
          /* Unknown type, in a prototype instantiation. */
          selector_type = type_of_unknown_templ_param_nontype;
        } else {
          selector_type = type_pointed_to(selector_type);
        }  /* if */
      }  /* if */
      ptr_selector_type = make_pointer_type(selector_type);
      /* See how well the selector type and the "this" parameter type
         match up. */
      determine_selector_match_level(ptr_selector_type,
                                     this_param_type,
                                     this_match_summary);
    }  /* if */
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
    rtsp->this_class = proj_function_symbol->parent.class_type;
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


static a_boolean deduce_one_parameter(a_type_ptr         param_type,
                                      an_operand         *arg_operand,
                                      a_type_ptr         arg_type,
                                      a_symbol_ptr       template_sym,
                                      a_template_arg_ptr *template_arg_list)
/*
Do template argument deduction on one parameter of a function template.
param_type is the type of the parameter (and requires deduction).
arg_operand is the argument; it can be NULL, in which case arg_type gives
the argument type.  template_sym is the symbol for the function_template
(not a projection symbol).  *template_arg_list points to the template
argument list so far; anything deduced is added to that.  Return TRUE
if the deduction succeeds, FALSE if it fails.
*/
{
  a_template_symbol_supplement_ptr
            tssp = template_sym->variant.template_info;
  a_boolean param_is_reference = is_reference_type(param_type);
  a_boolean deduction_okay = FALSE;

  if (arg_operand != NULL) arg_type = arg_operand->type;
  /* Certain top-level parts of the parameter type (e.g., references)
     are processed here before going to the type deduction routine.
     The code here must match determine_arg_match_level and
     overload_distinguishable. */
  if (arg_operand != NULL && is_indefinite_function_operand(arg_operand)) {
    /* For an overloaded function, each possibility must be tried.
       Only one is allowed to match. */
    if (!indefinite_function_can_be_template_arg(arg_operand,
                                                 param_type,
                                                 &arg_type,
                                                 template_sym)) goto done;
  }  /* if */
  if (param_is_reference) {
    /* The parameter has a reference type. */
    /* Drop the reference type. */
    param_type = type_pointed_to(param_type);
    if (arg_operand != NULL &&
        is_sym_for_member_operand(arg_operand) &&
        is_a_function_designator(arg_operand)) {
      /* Convert a member name to a pointer-to-member.  This comes up with
         the extension that allows A::x to be used for a pointer to member
         function, without the standard preceding "&", and is necessary
         when the parameter has type "reference to const pointer to
         member". */
      arg_type = type_after_function_to_pointer_transformation(arg_type,
                                                               arg_operand);
    }  /* if */
    /* Check and adjust the top-level type qualifiers. */
    check_template_arg_type_qualifiers(&arg_type, &param_type);
  } else {
    /* Not a reference. */
    /* See if any implicit transformations (e.g., array --> pointer) should
       be done. */
    if (is_array_type(arg_type)) {
      /* Simulate the array --> pointer transformation.  */
      arg_type = type_after_array_to_pointer_transformation(arg_type);
    } else if (is_function_type(arg_type)) {
      /* Simulate the function --> pointer transformation. */
      arg_type = type_after_function_to_pointer_transformation(arg_type,
                                                               arg_operand);
    }  /* if */
    /* The argument will be passed by copying it, so its cv-qualifiers
       are not significant. */
    arg_type = skip_typerefs(arg_type);
    /* Top-level type qualifiers on the parameter type are also not
       significant. */
    param_type = skip_typerefs(param_type);
    /* An incomplete type operand cannot be made to match anything.
       This comes up for something like
         struct A *p;
         template<class T> void f(T);
         void m() { f(*p); }
    */
    complete_type_is_needed(arg_type);
    if (is_incomplete_type(arg_type)) goto done;
  }  /* if */
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
  /* Do template argument deduction, trying to develop a list of
     template arguments that will produce an instance that matches
     the argument list. */
  /* As the matching is attempted, template_arg_list is filled in with
     the bindings for the template arguments.  This is needed during the
     matching process to ensure that each argument is used consistently
     and also later routine to build the instantiation.  The
     MTT_ALLOW_CONVERSION option is used to allow an argument requiring
     a conversion from Derived<T> to Base<T>.  This conversion was not
     allowed by the ARM but has been blessed by the standards committee. */
  if (matches_template_type(arg_type, param_type, template_arg_list,
                            tssp->variant.function.decl_cache.
                                                         decl_info->parameters,
                            MTT_ALLOW_CONVERSION)) {
    deduction_okay = TRUE;
  }  /* if */
done:
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

  db_enter(4, "function_template_call_argument_deduction");
  check_assertion(template_sym->kind == (a_symbol_kind)sk_function_template);
  check_assertion(routine_type->kind == (a_type_kind)tk_routine);
  rtsp = routine_type->variant.routine.extra_info;
  /* Look through the arguments/parameters to do template argument
     deduction. */
  for (ptp = rtsp->param_type_list, arg_operand = arg_operand_list;
       ptp != NULL && arg_operand != NULL;
       ptp = ptp->next, arg_operand = arg_operand->next) {
    if (ptp->type_involves_deduced_template_param) {
      /* A parameter that requires type deduction.  Do the deduction. */
      if (!deduce_one_parameter(ptp->type, &arg_operand->operand,
                                (a_type_ptr)NULL,
                                template_sym, template_arg_list)) {
        /* Deduction failed. */
        goto done;
      }  /* if */
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
       should have a default argument expression. */
    check_assertion_str(ptp->has_default_arg,
        "function_template_call_argument_deduction: missing default arg expr");
  }  /* if */
#endif /* CHECKING */
  /* Make sure that the types of nontype template parameters that depend
     on other template parameters agree with the types of the deduced
     values.  Also check for the case where not all template parameters
     have been deduced.  Create a routine type with all the substitution
     done. */
  updated_routine_type = wrapup_function_template_argument_deduction(
                                                   *template_arg_list,
                                                   template_sym,
                                                   (a_template_param_ptr)NULL);
done:;
  db_exit();
  return updated_routine_type;
}  /* function_template_call_argument_deduction */


static a_boolean candidate_function_is_visible(
                                      a_symbol_ptr function_symbol,
                                      a_boolean    is_template_id,
                                      a_boolean    effects_copy_initialization,
                                      a_boolean    from_arg_dep_lookup,
                                      a_boolean    dependent_call)
/*
Return TRUE if the indicated candidate function (possibly a projection
symbol, but not an overloaded function) is visible.  That is, return
FALSE if it should be considered invisible for some reason.  For example,
functions injected by friend declarations are generally invisible.
is_template_id is TRUE if the function symbol has an associated
explicit template argument list.  effects_copy_initialization is
TRUE if this call is the user-defined conversion in a copy-initialization
(constructors marked "explicit" are considered invisible).
from_arg_dep_lookup is TRUE if the function was found by argument-dependent
lookup.  dependent_call is TRUE if the call is a template-dependent
call.
*/
{
  a_boolean     visible = TRUE, function_template_case;
  a_routine_ptr routine;

  /* Ignore friend functions that aren't visible.  Note that this
     test is done on the projection symbol, if any, and not on the
     underlying fundamental symbol. */
  if (symbol_is_invisible_friend(function_symbol)) {
    visible = FALSE;
    goto end_of_function;
  }  /* if */
  /* Remove projection, if any. */
  function_symbol = fundamental_symbol_of(function_symbol);
  function_template_case = (function_symbol->kind ==
                                          (a_symbol_kind)sk_function_template);
  if (do_dependent_name_processing && !from_arg_dep_lookup &&
      depth_innermost_instantiation_scope != NO_SCOPE_DEPTH &&
      !function_symbol->is_class_member &&
      !is_local_symbol(function_symbol) &&
      function_symbol->decl_seq > get_effective_decl_seq()) {
    /* This symbol is not visible in this template instantiation (it
       was declared after the template definition). */
    visible = FALSE;
    goto end_of_function;
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
    visible = FALSE;
    goto end_of_function;
  }  /* if */
  if (effects_copy_initialization && routine->is_explicit_constructor) {
    /* Constructors marked "explicit" are to be ignored. */
    visible = FALSE;
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
                 a_boolean                selector_is_object_pointer,
                 a_type_ptr               implicit_selector_type,
                 a_boolean                ctor_conversion_case,
                 a_boolean                effects_copy_initialization,
                 a_boolean                allow_udc_on_arguments,
                 a_boolean                from_arg_dep_lookup,
                 a_boolean                dependent_call,
                 a_boolean                known_to_be_visible,
                 a_candidate_function_ptr *candidate_functions,
                 a_boolean                *matched_except_for_missing_selector,
                 a_boolean                *matched_except_for_selector)
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
pointer if selector_is_object_pointer is TRUE, an object otherwise.
implicit_selector_type indicates the type of an implicit "this->"
selector, if applicable, or is NULL otherwise.  Any viable functions
are added to the candidate_functions list along with information on
the level of argument matches.  If a match would have been found
except for the absence of a selector, set
*matched_except_for_missing_selector TRUE, and if a match would have
been found except for a mismatch on the selector, set
*matched_except_for_selector TRUE; those allow different error
messages.  If ctor_conversion_case is TRUE, this analysis is being
done as part of resolving an implicit or explicit conversion to a
class type: the functions are constructors, have_selector is FALSE
(sic; the "this" parameter is not matched up); the "conversion" field
is set in any candidate function entries created.
effects_copy_initialization is TRUE if this call is the user-defined
conversion in a copy-initialization; constructors that are marked
"explicit" are ignored.  allow_udc_on_arguments is TRUE if
user-defined conversions should be allowed on the argument matches.
from_arg_dep_lookup is TRUE if the function was found by
argument-dependent lookup.  dependent_call is TRUE if the call is a
template-dependent call.  known_to_be_visible is TRUE if the function
is known to be visible and the visibility check should be suppressed.
*/
{
  a_symbol_ptr             function_symbol;
  a_routine_ptr            routine;
  a_routine_type_supplement_ptr
                           rtsp;
  an_arg_operand_ptr       arg_operand;
  a_param_type_ptr         param, template_param = NULL;
#if DEBUG
  unsigned long            narg = 0;
#endif /* DEBUG */
  a_boolean                reached_ellipsis;
  an_arg_match_summary_ptr this_match, this_match_next;
  an_arg_match_summary_ptr arg_match = NULL;
  an_arg_match_summary_ptr arg_match_list = NULL;
  an_arg_match_summary_ptr end_arg_match_list = NULL;
  a_boolean                function_template_case = FALSE;
  a_template_arg_ptr       local_template_arg_list = NULL;

  if (proj_function_symbol != NULL) {
    /* Normal case: a known function. */
    if (!known_to_be_visible &&
        !candidate_function_is_visible(proj_function_symbol,
                                       is_template_id,
                                       effects_copy_initialization,
                                       from_arg_dep_lookup,
                                       dependent_call)) {
      /* The function is not visible, so ignore it. */
      goto reject_function;
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
        routine_type = substitute_template_arguments(
                         function_symbol, template_arg_list,
                         &local_template_arg_list, (a_template_param_ptr)NULL);
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
  if (function_template_case) {
    /* Save the pointer to the first parameter in the template version
       (i.e., before deduction) for later use.  Note that this is after
       substitution of explicitly-specified template arguments. */
    template_param = param;
  }  /* if */
  for (arg_operand = arg_operand_list;
       arg_operand != NULL;
       arg_operand = arg_operand->next) {
    /* See if the parameter list is exhausted. */
    if (param == NULL) {
      /* More arguments than required.  No match unless there is an
         ellipsis. */
      if (rtsp->has_ellipsis) break;
      goto reject_function;
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
    if (!param->has_unevaluated_template_default &&
        param->default_arg_expr == NULL) goto reject_function;
#if DEBUG
    if (debug_level >= 4) {
      fprintf(f_debug, "determine_function_viability: default arg match\n");
    }  /* if */
#endif /* DEBUG */
  }  /* if */
  /* The function looks okay from the standpoint of argument count. */
  if (function_template_case) {
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
  }  /* if */
  /* Look at each argument and see whether or not it can match the formal
     parameter, and if so, how well. */
  reached_ellipsis = FALSE;
  param = rtsp->param_type_list;
  for (arg_operand = arg_operand_list;
       arg_operand != NULL;
       arg_operand = arg_operand->next) {
#if DEBUG
    narg++;
    if (debug_level >= 4) {
      fprintf(f_debug, "determine_function_viability: arg %lu\n", narg);
    }  /* if */
#endif /* DEBUG */
    /* Add an entry to the end of the arg_match_list to record whether or
       not this argument matches. */
    arg_match = alloc_arg_match_summary();
    if (arg_match_list == NULL) {
      arg_match_list = arg_match;
    } else {
      end_arg_match_list->next = arg_match;
    }  /* if */
    end_arg_match_list = arg_match;
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
      if (debug_level >= 4) {
        fprintf(f_debug, "determine_function_viability: ellipsis match\n");
      }  /* if */
#endif /* DEBUG */
    } else {
      a_boolean param_type_is_deduced = FALSE;
      /* Both the actual argument and formal parameter are available.
         See how well they match. */
      if (template_param != NULL &&
          template_param->type_involves_deduced_template_param) {
        param_type_is_deduced = TRUE;
      }  /* if */
      determine_arg_match_level(&arg_operand->operand, (a_type_ptr)NULL,
                                param->type,
                                param_type_is_deduced,
                                /*try_user_conversions=*/
                                                        allow_udc_on_arguments,
                                arg_match);
      /* If no match is possible, go on to the next function. */
      if (arg_match->match_level == aml_none) goto reject_function;
    }  /* if */
    /* Go on to the next parameter. */
    if (!reached_ellipsis) {
      param = param->next;
      if (function_template_case) {
        check_assertion(template_param != NULL);
        template_param = template_param->next;
      }  /* if */
    }  /* if */
  }  /* for */
  /* If param != NULL here, there are default arguments (because we
     got past the argument-count check above). */
  check_assertion_str(param == NULL || param->has_default_arg,
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
          if (is_reference_type(conv_rout_type->variant.routine.return_type)) {
            /* A conversion function returning a reference type creates
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
        a_type_ptr this_param_type =
                      this_param_type_for_overload_res(routine_type,
                                                       proj_function_symbol,
                                                       /*is_conv_func=*/FALSE);
        if (implicit_selector_type != NULL) {
          /* The selector is an implicit "this->".  See how well it
             matches.  It might not match at all. */
          determine_selector_match_level(implicit_selector_type,
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
                                         selector_is_object_pointer,
                                         routine,
                                         this_param_type, this_match);
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
      /* We have no selector. */
      if (function_is_nonstatic_member_function) {
        /* A selector is needed and one is not available, so the function
           is not suitable.  Remember this case to select a different
           error message if it turns out no function matches. */
        *matched_except_for_missing_selector = TRUE;
        goto reject_function;
      }  /* if */
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
    if (!function_template_case) candidate->conversion.routine = routine;
  }  /* if */
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
                 a_boolean                selector_is_object_pointer,
                 a_boolean                ctor_conversion_case,
                 a_boolean                effects_copy_initialization,
                 a_boolean                allow_udc_on_arguments,
                 a_boolean                from_arg_dep_lookup,
                 a_boolean                dependent_call,
                 a_boolean                known_to_be_visible,
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
selector_is_object_pointer is TRUE, an object otherwise.  Any viable
functions are added to the candidate_functions list along with
information on the level of argument matches.  If a match would have
been found except for the absence of a selector, set
*matched_except_for_missing_selector TRUE, and if a match would have
been found except for a mismatch on the selector, set
*matched_except_for_selector TRUE; those allow different
error messages.  If ctor_conversion_case is TRUE, this analysis is
being done as part of resolving an implicit or explicit conversion
to a class type: the functions are constructors, have_selector is FALSE
(sic; the "this" parameter is not matched up); the "conversion" field
is set in any candidate function entries created.  effects_copy_initialization
is TRUE if this call is the user-defined conversion in a copy-initialization;
constructors that are marked "explicit" are ignored.  allow_udc_on_arguments
is TRUE if user-defined conversions should be allowed on the argument
matches.  from_arg_dep_lookup is TRUE if the function was found by
argument-dependent lookup.  dependent_call is TRUE if the call is a
template-dependent call.  known_to_be_visible is TRUE if the function
is known to be visible and the visibility check should be suppressed.
*/
{
  a_boolean     overloaded_function_case;
  a_symbol_ptr  function_symbol, proj_function_symbol;
  a_type_ptr    implicit_selector_type = NULL;

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
  /* Look at each instance of the overloaded function and see whether or
     not it can match the actual arguments, and if so, how well. */
  for (; proj_function_symbol != NULL;
       proj_function_symbol = overloaded_function_case ? 
                                                   proj_function_symbol->next :
                                                   NULL) {
#if DEBUG
    if (debug_level >= 4) {
      db_symbol(proj_function_symbol,
                "try_overloaded_function_match: considering ", 2);
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
                                 selector_is_object_pointer,
                                 implicit_selector_type,
                                 ctor_conversion_case,
                                 effects_copy_initialization,
                                 allow_udc_on_arguments,
                                 from_arg_dep_lookup,
                                 dependent_call,
                                 known_to_be_visible,
                                 candidate_functions,
                                 matched_except_for_missing_selector,
                                 matched_except_for_selector);
  }  /* for */
}  /* try_overloaded_function_match */

#if MICROSOFT_EXTENSIONS_ALLOWED

a_boolean overloaded_function_match_possible(
                                a_symbol_ptr       overloaded_function_symbol,
                                a_boolean          is_template_id,
                                a_template_arg_ptr template_arg_list,
                                an_arg_operand_ptr arg_operand_list,
                                a_boolean          have_selector,
                                an_operand         *bound_function_selector,
                                a_boolean          selector_is_object_pointer)
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
                                selector_is_object_pointer,
                                /*ctor_conversion_case=*/FALSE,
                                /*effects_copy_initialization=*/FALSE,
                                /*allow_udc_on_arguments=*/TRUE,
                                /*from_arg_dep_lookup=*/FALSE,
                                /*dependent_call=*/FALSE,
                                /*known_to_be_visible=*/FALSE,
                                &candidate_functions,
                                &matched_except_for_missing_selector,
                                &matched_except_for_selector);
  possible = (candidate_functions != NULL);
  free_candidate_function_list(candidate_functions);
  return possible;
}  /* overloaded_function_match_possible */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void try_surrogate_function_match(
                             an_operand               *ptr_class_object,
                             an_arg_operand_ptr       arg_operand_list,
                             a_candidate_function_ptr *candidate_functions)
/*
Find any candidate surrogate functions and add them to the candidate_functions
list.  Look for conversion functions that convert the class object pointed
to by ptr_class_object to pointer to function.  Each function pointed to
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

  class_type = type_pointed_to(ptr_class_object->type);
  check_assertion(is_class_struct_union_type(class_type));
  /* Look at all conversion functions.  (The standard calls for conversion
     functions in accessible base classes, but that seems wrong.) */
  for (slep = symbol_supplement_for_class(class_type)->conversion_list;
       slep != NULL;
       slep = slep->next) {
    surrogate_function_conv_sym = slep->symbol;

#if DEBUG
    if (debug_level >= 4) {
      db_symbol(surrogate_function_conv_sym,
                "try_surrogate_function_match: considering ", 2); 
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
        determine_selector_match_level(ptr_class_object->type,
                                       this_param_type,
                                       &match);
        if (match.match_level != aml_none) {
          /* See how the arguments match up against the surrogate function
             parameters. */
          determine_function_viability((a_symbol_ptr)NULL,
                                       /*is_template_id=*/FALSE,
                                       (a_template_arg_ptr)NULL,
                                       surrogate_function_conv_sym,
                                       underlying_type,
                                       arg_operand_list,
                                       /*have_selector=*/TRUE,
                                       ptr_class_object,
                                       /*selector_is_object_pointer=*/TRUE,
                                       (a_type_ptr)NULL,
                                       /*ctor_conversion_case=*/FALSE,
                                       /*effects_copy_initialization=*/FALSE,
                                       /*allow_udc_on_arguments=*/TRUE,
                                       /*from_arg_dep_lookup=*/FALSE,
                                       /*dependent_call=*/FALSE,
                                       /*known_to_be_visible=*/FALSE,
                                       candidate_functions,
                                       &matched_except_for_missing_selector,
                                       &matched_except_for_selector);
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
        /* Test for adding cv-qualifiers immediately below a reference. */
        a_boolean param1_is_ref =
                             is_ref_or_ref_equivalent(param_type1, arg_match1);
        a_boolean param2_is_ref =
                             is_ref_or_ref_equivalent(param_type2, arg_match2);
        a_type_qualifier_set
                  qualifiers1 = TQ_NONE, qualifiers2 = TQ_NONE;
        if (param1_is_ref) {
          param_type1 = type_pointed_to(param_type1);
          qualifiers1 = get_type_qualifiers(param_type1);
        }  /* if */
        if (param2_is_ref) {
          param_type2 = type_pointed_to(param_type2);
          qualifiers2 = get_type_qualifiers(param_type2);
        }  /* if */
        /* The tiebreaker for adding cv-qualifiers under a reference is
           applied only when both parameters are references, according to the
           standard.  However, in a compatibility mode it is applied when
           at least one parameter is a reference. */
        if ((param1_is_ref && param2_is_ref) ||
            (single_ref_qual_ovl_res_tiebreaker &&
             (param1_is_ref || param2_is_ref) &&
             /* In Microsoft bugs mode, the tie-breaker applies only when the
                argument is not a constant. */
             (!microsoft_bugs || !arg_match1->arg_is_constant))) {
          /* The tiebreaker applies only when the qualifiers under the
             references are different. */
          if (qualifiers1 != qualifiers2) {
            /* The tiebreaker applies only if the underlying types are the
               same. */
            if (types_are_compatible_ignoring_qualifiers(param_type1,
                                                         param_type2)) {
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
              same_type_with_added_qualifiers(param_type2, param_type1,
                                              /*ignore_qualifiers=*/FALSE,
                                              &qualifiers_added) &&
              qualifiers_added) {
            /* param_type1 has more qualifiers than param_type2, and the
               types are otherwise compatible.  Therefore fewer qualifiers
               are added to get to param_type2, and argument 2 is better. */
            cmp = -1;
          } else if (arg_match2->conversion.std.type_qualifiers_added &&
                     same_type_with_added_qualifiers(param_type1, param_type2,
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
  /* Use of an anachronism (e.g., calling a const function for a
     non-const object) can break a tie. */
  if (cmp == 0 &&
      arg_match1->anachronism_used != arg_match2->anachronism_used) {
    if (arg_match1->anachronism_used) {
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


static int compare_arg_match_levels(an_arg_match_summary *arg_match1,
                                    an_arg_match_summary *arg_match2)
/*
Compare two argument match summary entries and return

  +1 if arg_match1 is a better match than arg_match2,
   0 if the two matches are equal, or
  -1 if arg_match1 is a worse match than arg_match2.

*/
{
  int cmp = 0;

  /* Compare the gross match levels. */
  if (arg_match1->match_level == aml_none ||
      arg_match2->match_level == aml_none) {
    /* The match for the "this parameter" of a static member function
       has a "none" match level.  It's no better and no worse than any
       other match. */
  } else if ((int)arg_match1->match_level < (int)arg_match2->match_level) {
    /* arg_match1 is better. */
    cmp = 1;
  } else if ((int)arg_match1->match_level > (int)arg_match2->match_level) {
    /* arg_match2 is better. */
    cmp = -1;
  } else if (!do_late_ovl_res_tiebreaker &&
             (cmp=compare_argument_tiebreakers(arg_match1, arg_match2)) != 0) {
    /* The argument tiebreakers (applied early, which is the standard-
       conforming way) prefer one match over the other. */
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
      type = type->variant.routine.return_type;
      type = skip_typerefs(type);
      if (is_reference_type(type)) type = type_pointed_to(type);
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
    if (same_type_with_added_qualifiers(type2, type1,
					/*ignore_qualifiers=*/FALSE,
                                        &qualifiers_added) &&
        qualifiers_added) {
      same_with_added_qualifiers = TRUE;
    }  /* if */
  }  /* if */
  return same_with_added_qualifiers;
}  /* candidate_return_type_same_with_added_qualifiers */


static a_boolean candidate_is_conversion(a_candidate_function_ptr cfp)
/*
Return TRUE if the indicated candidate function is a non-template conversion
function.
*/
{
  a_boolean    is_conv = FALSE;
  a_symbol_ptr sym = cfp->function_symbol;

  if (sym != NULL) {
    sym = fundamental_symbol_of(sym);
    /* Don't process templates. */
    if (sym->kind == (a_symbol_kind)sk_member_function) {
      a_routine_ptr rout = sym->variant.routine.ptr;
      if (rout->special_kind == (a_special_function_kind)sfk_conversion) {
        is_conv = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return is_conv;
}  /* candidate_is_conversion */


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
  } else if (microsoft_bugs &&
             cfp1->is_user_conversion &&
             !candidate_is_conversion(cfp1) &&
             candidate_is_conversion(cfp2)) {
    /* Microsoft picks a constructor over a conversion function to do
       a conversion. */
    cmp = 1;
  } else if (microsoft_bugs &&
             cfp1->is_user_conversion &&
             candidate_is_conversion(cfp1) &&
             !candidate_is_conversion(cfp2)) {
    /* Microsoft picks a constructor over a conversion function to do
       a conversion. */
    cmp = -1;
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
  } else if (cfp1->is_function_template != cfp2->is_function_template) {
    /* The fact that one function is a function template and the other
       is not can serve as a tie-breaker. */
    if (cfp1->is_function_template) {
      /* cfp1 is a function template and cfp2 is not, so cfp2 is better. */
      cmp = -1;
    } else {
      /* cfp2 is a function template and cfp1 is not, so cfp1 is better. */
      cmp = 1;
    }  /* if */
  } else if (cfp1->is_function_template && cfp2->is_function_template) {
    /* cfp1 and cfp2 are function templates.  Determine whether either of
       the templates is more specialized than the other. */
    cmp = compare_function_templates(cfp1->function_symbol,
                                     cfp2->function_symbol);
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
               canonical_il_entry_of(sym1->variant.projection.extra_info->
                                                     fundamental_base_class) !=
               canonical_il_entry_of(sym2->variant.projection.extra_info->
                                                     fundamental_base_class)) {
      /* When dealing with class member projections, if the subobjects
         involved are different (e.g., because of an ambiguous base class)
         the functions are different because they deal with different base
         class subobjects. */
      /* same = FALSE; -- already set. */
    } else {
      a_routine_ptr rout1, rout2;
      sym1 = fundamental_symbol_of(sym1);
      sym2 = fundamental_symbol_of(sym2);
      /* Note that sym1 or sym2 can be a function template here. */
      if (sym1 == sym2 ||
          ((sym1->kind == (a_symbol_kind)sk_routine ||
            sym1->kind == (a_symbol_kind)sk_member_function) &&
           sym1->kind == sym2->kind &&
           /* Compare IL entry pointers to deal with block extern symbols. */
           (rout1 = sym1->variant.routine.ptr,
            rout2 = sym2->variant.routine.ptr,
            same_routine_entities(rout1, rout2)))) {
        same = TRUE;
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
        if (compare_arg_match_levels(best_curr_arg, curr_arg) > 0 ||
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
      break;
    }  /* if */
check_next_function:;
  }  /* for */
  return match_is_better;
}  /* match_is_better_on_at_least_one_arg */


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
  unsigned long            number_in_best_match_set;
  an_arg_match_summary_ptr curr_arg;
  int                      cmp;
  a_boolean                overall_ambiguity = FALSE, any_error_match = FALSE;

  db_enter(4, "select_best_candidate_functions");
#if DEBUG
  if (debug_level >= 4) {
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
    for (cfp = candidates; cfp != NULL; cfp = cfp->next) {
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
    }  /* for */
    if (overall_ambiguity) goto create_final_list;
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
            cmp = compare_arg_match_levels(curr_arg,
                                           func_in_set->current_arg_match);
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
  if (debug_level >= 4) {
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
    a_symbol_ptr ovl_sym = operand->variant.symbol, sym;
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


a_symbol_ptr select_overloaded_function(
                         a_symbol_ptr             overloaded_function_symbol,
                         a_boolean                is_template_id,
                         a_template_arg_ptr       template_arg_list,
                         a_boolean                have_selector,
                         an_operand               *bound_function_selector,
                         an_arg_operand_ptr       arg_operand_list,
                         a_boolean                do_arg_dep_lookup,
                         an_error_code            err_none_applies,
                         an_error_code            err_ambiguous,
                         a_source_position        *call_position,
                         a_token_sequence_number  paren_tok_seq_number,
                         a_boolean                *single_function,
                         a_boolean                *unknown_dependent_function,
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
that discrimination.  do_arg_dep_lookup is TRUE if argument-dependent
lookup should be done; if it is TRUE, overloaded_function_symbol may
be an sk_undefined symbol, indicating that nothing was found on a
normal id lookup of the function name.  call_position is the source
position of the call.  paren_tok_seq_number is the token sequence
number of the opening parenthesis of the argument list, but it's
required only when do_arg_dep_lookup is TRUE; it can be zero
otherwise.  If an error of some sort is detected, issue an error at
that position and return NULL.  err_none_applies is the error code to
use when no function applies, and err_ambiguous is the error code to
use when more than one function applies.  If there is no error, an
argument match list is returned in *arg_match_list (the caller must
free this) and the symbol selected is returned.  If single_function is
non-NULL and the set of functions to be considered (the symbol passed
in, if not undefined, plus any symbols added by argument-dependent
lookup) contains exactly one function, set *single_function to TRUE
and return the function, without checking whether the function matches
the argument list provided (this allows the caller to revert to the
simpler processing used for non-overloaded functions, which can
produce clearer error messages).  If the call is dependent, and
the function to be called cannot be determined, return
*unknown_dependent_function set to TRUE (unknown_dependent_function
can be NULL if the call cannot be dependent).  If
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
  if (!have_selector) bound_function_selector = NULL;
  /* candidate_functions will contain the list of viable functions. */
  candidate_functions = NULL;
  if (single_function != NULL) *single_function = FALSE;
  /* The "single function" processing is not compatible with trying
     surrogate functions. */
  if (surrogate_function_conv_sym != NULL) single_function = NULL;
  if (overloaded_function_symbol != NULL) {
    sym_is_undefined = (overloaded_function_symbol->kind ==
                                                  (a_symbol_kind)sk_undefined);
    if (do_arg_dep_lookup && !sym_is_undefined) {
      /* If the function found is a block extern or a member function, suppress
         argument-dependent lookup.  The member function part of that is
         in the standard.  The block extern part is not, but was strongly
         supported as a change at the Nov. 98 standards committee meeting.
         Using-declarations are treated similarly. */
      if (overloaded_function_symbol->is_class_member) {
        do_arg_dep_lookup = FALSE;
      } else if (!strict_ansi_mode &&
                 is_local_symbol(overloaded_function_symbol)) {
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
      if (is_template_dependent_type(arg_operand->operand.type)) {
        dependent_call = TRUE;
        break;
      }  /* if */
    }  /* for */
    if (!dependent_call && overloaded_function_symbol != NULL &&
        overloaded_function_symbol->is_class_member &&
        (have_selector ?
                (bound_function_selector != NULL &&
                 is_template_dependent_type(bound_function_selector->type)) :
                TRUE)) {
      /* The selector object is dependent.  An implicit selector is
         always dependent in a prototype instantiation. */
      dependent_call = TRUE;
    }  /* if */
    if (!dependent_call && is_template_id &&
        template_arg_list_involves_template_param(template_arg_list)) {
      /* A call like f<T>(1), where the explicit template argument
         list includes dependent arguments. */
      dependent_call = TRUE;
    }  /* if */
    if (overloaded_function_symbol != NULL &&
        is_block_extern_symbol(overloaded_function_symbol)) {
      /* A block extern declaration can be dependent (e.g., it
         can have dependent parameter types or dependent default
         argument expressions), so we can't do overload resolution. */
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
             depth_innermost_instantiation_scope != NO_SCOPE_DEPTH) {
    /* In a real (not prototype) instantiation, and doing dependent
       name processing.  Look up this call to see whether it was a
       dependent call in the prototype instantiation.  If it was a
       nondependent call, it was recorded, along with (usually) the
       symbol chosen by overload resolution. */
in_instantiation:
    if (!do_arg_dep_lookup) {
      /* Calls where argument-dependent lookup is turned off are
         not recorded, but they're always considered non-dependent. */
      dependent_call = FALSE;
    } else {
      a_nondependent_call_info_ptr ndcall_info;
      ndcall_info = get_nondependent_call_info(paren_tok_seq_number);
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
      if (single_function != NULL && !have_selector) {
        /* If the function is a single non-overloaded function, overload
           resolution is not required. */
        function_symbol = fundamental_symbol_of(overloaded_function_symbol);
        if ((function_symbol->kind == (a_symbol_kind)sk_routine ||
             function_symbol->kind == (a_symbol_kind)sk_member_function) &&
            (known_to_be_visible ||
             candidate_function_is_visible(overloaded_function_symbol,
                                           is_template_id,
                                         /*effects_copy_initialization=*/FALSE,
                                           /*from_arg_dep_lookup=*/FALSE,
                                           dependent_call))) {
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
                                    /*selector_is_object_pointer=*/TRUE,
                                    /*ctor_conversion_case=*/FALSE,
                                    /*effects_copy_initialization=*/FALSE,
                                    /*allow_udc_on_arguments=*/TRUE,
                                    /*from_arg_dep_lookup=*/FALSE,
                                    dependent_call,
                                    known_to_be_visible,
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
                                          /*from_arg_dep_lookup=*/
                                               (symbol_list->symbol !=
                                                normal_lookup_function_symbol),
                                          dependent_call)) {
          /* This must be either the only entry on the list, or all other
             entries on the list must be the same symbol. */
          for (slep = symbol_list->next; slep != NULL; slep = slep->next) {
            if (slep->symbol != symbol_list->symbol) break;
          }  /* for */
          if (slep == NULL) {
            free_list_of_symbol_list_entries(symbol_list);
            *single_function = TRUE;
            function_symbol = symbol_list->symbol;
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
                                      /*selector_is_object_pointer=*/TRUE,
                                      /*ctor_conversion_case=*/FALSE,
                                      /*effects_copy_initialization=*/FALSE,
                                      /*allow_udc_on_arguments=*/TRUE,
                                      /*from_arg_dep_lookup=*/
                                               (slep != symbol_list ||
                                                function_symbol !=
                                                normal_lookup_function_symbol),
                                      dependent_call,
                                      /*known_to_be_visible=*/FALSE,
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
      pos_error(ec_member_ref_requires_object, call_position);
    } else if (sym_is_undefined && !some_function_tried) {
      /* The function symbol is not defined, and no functions were added
         by argument-dependent lookup, so the best diagnostic is one
         that says the name is undefined. */
      enter_undefined_symbol(overloaded_function_symbol);
      pos_st_error(ec_undefined_identifier, call_position,
                   overloaded_function_symbol->header->identifier);
    } else if (overloaded_function_symbol == NULL) {
      /* Call of class object, no operator() or appropriate conversion
         functions to pointer to function type. */
      check_assertion(surrogate_function_conv_sym != NULL);
      pos_error(ec_bad_call_of_class_object, call_position);
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
      }  /* if */
      pos_sy_start_error(err_none_applies, call_position,
                         overloaded_function_symbol);
      display_argument_list_types(object_type, arg_operand_list);
      end_error();
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
      pos_sy_error(ec_ambiguous_name, call_position,
                   overloaded_function_symbol);
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
        pos_ty_start_error(ec_ambiguous_class_call, call_position,
                           type_pointed_to(bound_function_selector->type));
      } else {
        /* Normal case (not a class call). */
        check_assertion(overloaded_function_symbol != NULL);
        pos_sy_start_error(err_ambiguous, call_position,
                           overloaded_function_symbol);
      }  /* if */
      diagnose_overload_ambiguity(candidate_functions,
                                  bound_function_selector,
                                  arg_operand_list,
                                  (an_opname_kind)onk_none);
    }  /* if */
  } else {
    /* Exactly one function applies and is best. */
    function_symbol = candidate_functions->function_symbol;
    *arg_match_list = candidate_functions->arg_matches;
    /* Prevent freeing of the arg_match_list when the candidate_functions
       list is freed. */
    candidate_functions->arg_matches = NULL;
    if (candidate_functions->surrogate_function_conv_sym != NULL) {
      /* The best function is a surrogate function. */
      *surrogate_function_conv_sym =
                              candidate_functions->surrogate_function_conv_sym;
#if DEBUG
      if (debug_level >= 4) {
        db_symbol(*surrogate_function_conv_sym,
                  "select_overloaded_function: selected surrogate ", 2); 
      }  /* if */
#endif /* DEBUG */
    } else {
#if DEBUG
      if (debug_level >= 4) {
        db_symbol(function_symbol,
                  "select_overloaded_function: selected ", 2); 
      }  /* if */
#endif /* DEBUG */
    }  /* if */
  }  /* if */
  /* Free the candidate functions list. */
  free_candidate_function_list(candidate_functions);
have_function:
  if (do_dependent_name_processing && is_prototype_instantiation_context() &&
      !dependent_call && do_arg_dep_lookup) {
    /* Record the outcome of overload resolution for a nondependent call
       in a prototype instantiation.  Dependent calls in such a context
       don't get here.  Calls where argument-dependent lookup is turned
       off are not recorded; they're considered non-dependent. */
    /* Note that function_symbol can be NULL here, e.g., for a call of
       a (possibly dependent) block extern declaration, which must be
       resolved in the real instantiation. */
    check_assertion(paren_tok_seq_number != 0);
    record_nondependent_call(function_symbol, paren_tok_seq_number);
  }  /* if */
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
    pos_warning(amsp->conversion.std.warning_suggested, err_pos);
  } else if (amsp->const_anachronism) {
    /* This call depends on the anachronism that allows a non-const function
       to be called with a const object. */
    pos_warning(ec_const_function_anachronism, err_pos);
  }  /* if */
}  /* issue_warning_from_arg_match_summary */


a_type_ptr operand_complete_object_type(an_operand *operand,
                                        a_boolean  call_case)
/*
Return the type of the complete object that contains the location indicated
by operand (an address), or NULL if no complete object type can be determined.
call_case is TRUE if the answer will be used to optimize a virtual function
call.  NULL is always a safe answer; non-NULL values may permit optimizations.
Note that "complete object" means an object that is not a base class of
another object, not necessarily a top-level object.  This is used only in
C++ mode; it is useful to know what the complete object type is to optimize
base class casts and virtual function calls.
*/
{
  a_type_ptr complete_object_type = NULL;

  if (is_constant_operand(operand)) {
    complete_object_type =
                          con_complete_object_type(&operand->variant.constant);
  } else if (is_expression_operand(operand)) {
    complete_object_type = 
                         node_complete_object_type(operand->variant.expression,
                                                   call_case);
  }  /* if */
  return complete_object_type;
}  /* operand_complete_object_type */


static a_routine_ptr function_from_virtual_function_operand(
                                                  an_operand *function_operand)
/*
Given an operand for the address of a virtual function, extract and return a
pointer to the routine entry for the function.  This is possible in general
with virtual functions (and not all functions) because the function operand
for a virtual function is always just a simple address (you can't use an
expression to identify a virtual function; you can use a pointer to member
function, but that case does not come here).
*/
{
  a_constant_ptr con;

#if CHECKING
  if (!is_constant_operand(function_operand)) {
    internal_error("function_from_virtual_function_operand: bad operand");
  }  /* if */
#endif /* CHECKING */
  con = &function_operand->variant.constant;
#if CHECKING
  if (con->kind != (a_constant_repr_kind)ck_address ||
      con->variant.address.kind != (an_address_base_kind)abk_routine) {
    internal_error("function_from_virtual_function_operand: bad constant");
  }  /* if */
#endif /* CHECKING */
  return con->variant.address.variant.routine;
}  /* function_from_virtual_function_operand */


void bind_member_function_operand_to_selector(
                                           an_operand *function_operand,
                                           an_operand *bound_function_selector)
/*
Bind the operand for a function to an associated selector object.
*/
{
  a_routine_ptr function;

  function_operand->bound_function = TRUE;
  if (function_operand->virtual_function) {
    /* Virtual function call. */
    /* If the left operand is a complete object, we know the routine to
       call without going through the virtual function mechanism.
       We do a simple test, that the left operand type matches the
       complete object type.  Fancier schemes run into trouble when
       an overload set contains both virtual and nonvirtual functions:
         struct A {
           virtual int f();
                   int f(int);
         };
         struct B : public A {
                   int f();
                   int f(int);
         };
         B b;
         ((A *)&b)->f();  // B::f()
         ((A *)&b)->f(1); // A::f(int);
       One would have to have a way to adjust the "this" pointer back
       to the derived class to optimize the first case.
    */
    /* Suppress the optimization if the name came from a using-declaration,
       because in that case the name that was found might not be the
       final overrider. */
    if (!function_operand->is_using_decl_name &&
        operand_complete_object_type(bound_function_selector,
                                     /*call_case=*/TRUE) ==
        type_pointed_to(bound_function_selector->type)) {
      function_operand->virtual_function = FALSE;
      /* Set the IL referenced flag for the function.  It wasn't set
         when the call was thought to be virtual, since a virtual call
         does not necessarily end up at the indicated routine. */
      /* Extract the routine being called.  For virtual function calls, the
         operand identifying the function is always just a simple address
         of a function. */
      function = function_from_virtual_function_operand(function_operand);
      if_evaluating_mark_routine_referenced(function);
    }  /* if */
  }  /* if */
}  /* bind_member_function_operand_to_selector */


void overloaded_function_catch_up(a_symbol_ptr      function_symbol,
                                  a_symbol_ptr      overloaded_function_symbol,
                                  a_boolean         is_qualified_name,
                                  a_source_position *function_position,
                                  a_source_position *id_position,
                                  a_boolean         elided_reference,
                                  a_boolean         address_taken,
                                  an_operand        *operand,
                                  a_boolean         *access_error_reported)
/*
We've just determined which specific function within a set of overloaded
functions is being referenced, i.e., function_symbol is being called
within the set given by overloaded_function_symbol.  (function_symbol is
not an overloaded function, but it might be a projection symbol.)  Do
whatever would have been done with the function along the way if we had
known all along which specific function was intended.  That is, "catch
up" with the processing that would have been done up to this point for a
non-overloaded function.  While this is usually used for overloaded
functions, it is also used for a few cases where the function is not
overloaded but it is not convenient to note that fact before scanning the
arguments (e.g., operator overloading).  Therefore, while
overloaded_function_symbol is typically an sk_overloaded_function
containing function_symbol, it may be the same as function_symbol, or it
may be a projection symbol for one of those, or it might be an
sk_function_template symbol.  Generate an operand for a pointer
to the specific function in *operand.  function_position is used as
the source position for that operand.  id_position is the position of
the function name identifier in the call (e.g., if the name is "X::f",
id_position gives the position of the "f").  is_qualified_name is TRUE if a
qualified name was used to name the function (that suppresses the
virtual-ness of the function).  Access control and ambiguity checking are
always done, even if the overloaded_function_symbol is a non-overloaded
function.  operand can be NULL if it is not necessary to generate the
function designator operand.  elided_reference is TRUE if the routine was
referenced in the program but the reference is being elided in the
intermediate language (operand should be NULL in that case).
address_taken is TRUE if the address of the function is being taken (as
opposed to the function being called); it controls the type of reference
recorded.  On return, *access_error_reported is TRUE if an access control
checking error was detected and reported.
*/
{
  a_symbol_ptr     base_function_symbol =
                                        fundamental_symbol_of(function_symbol);
  a_symbol_ptr     base_overloaded_function_symbol =
                             fundamental_symbol_of(overloaded_function_symbol);
  a_symbol_locator function_symbol_locator;
  a_ref_entry_ptr  rep;

#if CHECKING
  if (base_function_symbol->kind != (a_symbol_kind)sk_routine &&
      base_function_symbol->kind != (a_symbol_kind)sk_member_function) {
    internal_error("overloaded_function_catch_up: bad function_symbol");
  }  /* if */
#endif /* CHECKING */
  /* The address of the function is not really taken if the current expression
     is not evaluated. */
  if (!curr_expr_is_potentially_evaluated()) address_taken = FALSE;
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
    overload_check_ambiguity_and_verify_access(&function_symbol_locator,
                                               overloaded_function_symbol);
  } else {
    /* Non-overloaded function; use the normal routine.
       overloaded_function_symbol is either the same as function_symbol
       or is a projection symbol for it. */
    make_locator_for_symbol(overloaded_function_symbol,
                            &function_symbol_locator);
    function_symbol_locator.source_position = *id_position;
    check_ambiguity_and_verify_access(&function_symbol_locator);
  }  /* if */
  *access_error_reported =
                         function_symbol_locator.access_control_error_reported;
  if (is_error_locator(function_symbol_locator)) {
    /* An error locator is returned for an ambiguous case. */
    if (operand != NULL) {
      make_error_operand(operand);
      operand->position = *function_position;
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
                                  (address_taken ? SRK_ADDRESS_TAKEN : 0),
                                base_function_symbol, id_position,
                                /*update_il_entry=*/FALSE);
        if_evaluating_mark_routine_referenced(base_function_symbol->
                                                         variant.routine.ptr);
      } else {
        /* Normal case: build an operand for the function. */
        /* Record that the function was referenced, for cross-reference (etc.)
           purposes. */
        rep = ref_entry(base_function_symbol, id_position);
        make_function_designator_operand(function_symbol,
                                         is_qualified_name,
                                         function_position, rep, operand);
        /* Convert the operand to a function pointer. */
        conv_function_designator_to_ptr_to_function(operand,
                                                    /*allow_ctor=*/FALSE);
        if (!address_taken) {
          /* Change the kind of reference to the function from "address taken"
             to "reference". */
          change_some_ref_kinds(operand->ref_entries_list, SRK_ADDRESS_TAKEN,
                                SRK_REFERENCE);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* overloaded_function_catch_up */


void combine_unneeded_selector_with_operand(
                                           an_operand *bound_function_selector,
                                           a_boolean  is_arrow_operator,
                                           an_operand *operand)
/*
*operand is a reference to a static class member, and *bound_function_selector
is an unneeded selector for that reference.  Save it by attaching it to
*operand (it must be evaluated, even though its type only -- and not its
value -- is used to select the member referenced).  is_arrow_operator is
TRUE if the selector is a pointer, and FALSE if it is a class.
*/
{
  an_operand            orig_operand;
  an_expr_node_ptr      selector_expr, expr;
  an_operand_state      saved_operand_state = operand->state;
  an_expr_operator_kind op;

  if (curr_expr_kind_is_const()) {
    /* In a constant expression, just throw away the left operand.  This
       comes up in prototype instantiations and with an extension in
       cfront and Microsoft modes:
         struct A { enum { e1 = 1 }; } a;
         int x[a.e1];
    */
    discard_operand(bound_function_selector);
  } else {
    orig_operand = *operand;
    /* In some cases (with bound function references) the selector is
       standardized to a pointer.  Change back to the "." form for
       some instances.  This is necessary for the enk_temp_init case
       to avoid using the enk_temp_init address as an rvalue. */
    if (is_arrow_operator && is_expression_operand(bound_function_selector)) {
      selector_expr = bound_function_selector->variant.expression;
      if ((selector_expr->kind == (an_expr_node_kind)enk_temp_init &&
           selector_expr->variant.init.result_is_addr) ||
          is_variable_address_node(selector_expr)) {
        conv_object_pointer_to_lvalue(bound_function_selector);
        is_arrow_operator = FALSE;
      }  /* if */
    }  /* if */
    selector_expr = make_node_from_operand(bound_function_selector);
    expr = make_node_from_operand(operand);
    selector_expr->next = expr;
    /* Determine the operator to use. */
    if (is_arrow_operator) {
      op = (an_expr_operator_kind)eok_points_to_static;
    } else if (is_an_lvalue(bound_function_selector)) {
      op = (an_expr_operator_kind)eok_lvalue_dot_static;
    } else {
      op = (an_expr_operator_kind)eok_rvalue_dot_static;
    }  /* if */
    /* Make a node for the selector and the operand. */
    expr = make_operator_node(op, expr->type, selector_expr);
    if (is_an_lvalue(operand)) {
      expr->variant.operation.returns_lvalue_instead_of_usual_rvalue = TRUE;
    } /* if */
    make_expression_operand(expr, operand->type, operand);
    operand->state = saved_operand_state;
    restore_operand_details(operand, &orig_operand);
  }  /* if */
}  /* combine_unneeded_selector_with_operand */


void cast_pointer_for_field_selection(
                               an_operand        *operand_1,
                               a_boolean         *is_arrow_operator,
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
operand.  *is_arrow_operator is TRUE for "->", FALSE for "." (it will be set
to TRUE on return if the operation is normalized into "->" form).
member_sym is the referenced member (possibly a projection symbol, but the
presence or absence of a projection is ignored).  projection_member_sym is
either the same as member_sym or a projection thereof, or, for a reference
to an overloaded function, the symbol for the overload set or a projection
thereof -- it identifies the symbol that was actually named in the member
reference, and a projection symbol on it is significant.
access_control_error_reported is TRUE if an access control error has
already been reported.  do_protected_member_check is TRUE if the
protected member access check of ARM 11.5 should be done.  *member_pos
gives the source position of the member name reference.
*/
{
  a_type_ptr       desired_class = projection_member_sym->parent.class_type;
  a_type_ptr       class_struct_union_type;
  a_base_class_ptr bcp;

  /* Leave an error operand alone. */
  if (!is_error_operand(operand_1)) {
    class_struct_union_type = operand_1->type;
    if (*is_arrow_operator) {
      if (is_template_param_or_nonreal_class_type(class_struct_union_type)) {
        /* Pointer type is unknown, in a prototype instantiation.  Or, the
           selector is a nonreal class type, which might have an operator->
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
    if (is_template_param_type(class_struct_union_type) ||
        class_struct_union_type->variant.class_struct_union.is_nonreal_class ||
        desired_class->variant.class_struct_union.is_nonreal_class) {
      /* Don't do any checking on nonreal classes in prototype
         instantiations. */
    } else {
      /* If the member is protected, it can only be accessed through an object
         or pointer of a type to which we have member access (ARM 11.5). */
      if (do_protected_member_check && !access_control_error_reported) {
        check_protected_member_access(member_sym, member_pos,
                                      class_struct_union_type);
      }  /* if */
      /* Do nothing if the type is already okay (which it almost always
         will be; only in cases involving qualified names can it be
         different). */
      if (class_struct_union_type != desired_class) {
        /* Some adjustment is required.  Find out how the classes are
           related to one another. */
        bcp = find_base_class_of(class_struct_union_type, desired_class);
        check_assertion(bcp != NULL);
        /* Cast the left operand to the proper type. */
        base_class_cast_operand(operand_1, bcp, is_arrow_operator,
                                /*check_cast_access=*/
                                                !access_control_error_reported,
                                /*is_implicit_cast=*/TRUE,
                                /*implicit_in_naming=*/FALSE,
                                /*is_object_pointer=*/TRUE);
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
        base_class_cast_operand(operand_1, bcp, is_arrow_operator,
                                /*check_cast_access=*/FALSE,
                                /*is_implicit_cast=*/TRUE,
                                /*implicit_in_naming=*/TRUE,
                                /*is_object_pointer=*/TRUE);
        class_struct_union_type = bcp->type;
      }  /* if */
      if (projection_member_sym != member_sym) {
        if (member_sym->parent.class_type != class_struct_union_type) {
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
          base_class_cast_operand(operand_1, bcp, is_arrow_operator,
                                  /*check_cast_access=*/FALSE,
                                  /*is_implicit_cast=*/TRUE,
                                  /*implicit_in_naming=*/TRUE,
                                  /*is_object_pointer=*/TRUE);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* cast_pointer_for_field_selection */


a_boolean variable_this_exists(a_variable_ptr *this_var)
/*
Return TRUE if there is a currently-visible "this" variable.  If there is,
also set *this_var to point to the variable entry for it.  This routine
is called only in C++ mode.
*/
{
  a_boolean   this_exists;
  a_scope_ptr il_scope;

  *this_var = NULL;
  if (depth_innermost_function_scope == NO_SCOPE_DEPTH) {
    /* We're not inside a function. */
    this_exists = FALSE;
  } else {
    il_scope = scope_stack[depth_innermost_function_scope].il_scope;
#if CHECKING
    if (il_scope == NULL) {
      internal_error("variable_this_exists: NULL IL scope for function");
    }  /* if */
#endif /* CHECKING */
    *this_var = il_scope->variant.routine.this_param_variable;
    this_exists = (*this_var != NULL);
  }  /* if */
  return this_exists;
}  /* variable_this_exists */


#if !EXTRA_SOURCE_POSITIONS_IN_IL
/*ARGSUSED*/ /* <-- is_implicit is not used in that case. */
#endif /* !EXTRA_SOURCE_POSITIONS_IN_IL */
void make_this_variable_operand(a_variable_ptr this_var,
                                a_boolean      is_implicit,
                                an_operand     *result)
/*
Make an operand for the value of the "this" variable this_var.  The reference
is implicit if is_implicit is TRUE.  The source position of the operand is
set to "pos_curr_token".  The position in the expression, which exists when
EXTRA_SOURCE_POSITIONS_IN_IL is TRUE, is set only when is_implicit is
FALSE.  The operand is an rvalue.
*/
{
  an_expr_node_ptr node;

  /* Make a variable value node for the variable. */
  node = var_rvalue_expr(this_var);
  /* Make an operand for the node. */
  make_expression_operand(node, node->type, result);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (!is_implicit) {
    /* Set the position in the expression too when the reference is
       explicit. */
    set_operand_expr_position_if_expr(result, (a_source_position *)NULL);
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
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
"this->", not for the explicit case.
*/
{
  a_variable_ptr   this_var;
  a_type_ptr       member_class, this_class;
  a_boolean        okay, template_case = FALSE;
  a_base_class_ptr bcp;

  /* This routine is similar to cast_pointer_for_field_selection. */
  if (curr_expr_kind_is_const()) {
    /* Nonstatic members are not allowed in constant expressions. */
    pos_error(ec_expr_not_constant, member_pos);
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
      member_class = projection_member_sym->parent.class_type;
      if (this_class == member_class) {
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
      pos_error(ec_member_ref_requires_object, member_pos);
      make_error_operand(result);
    } else {
      /* The "this" pointer can be used to access the member. */
      /* Make an operand for the value of the "this" pointer. */
      make_this_variable_operand(this_var, /*is_implicit=*/TRUE, result);
      /* Get position right in case of errors below. */
      result->position = *member_pos;
#if EXTRA_SOURCE_POSITIONS_IN_IL
      result->end_position = *member_pos;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
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
                                                   fund_sym->parent.class_type,
                                                   underlying_this_type);
        member_ptr = make_pointer_type(member_ptr);
        cast_operand(member_ptr, result,
                     /*check_cast_access=*/FALSE,
                     /*is_implicit_cast=*/TRUE,
                     /*is_reinterpret_cast=*/FALSE,
                     /*reinterpret_sementics=*/FALSE);
      } else {
        /* Cast the "this" value to the class of the member. */
        /* Note that no ARM 11.5 protected member access check is needed,
           because an access through "this" is always acceptable under the
           rules in that section. */
        a_boolean is_arrow_operator = TRUE;
        cast_pointer_for_field_selection(result,
                                         &is_arrow_operator,
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
  return okay;
}  /* make_this_pointer_operand */


static void make_resolved_overloaded_function_operand(
                                 a_symbol_ptr       function_symbol,
                                 a_symbol_ptr       overloaded_function_symbol,
                                 a_boolean          *have_selector,
                                 an_operand         *bound_function_selector,
                                 a_boolean          is_qualified_name,
                                 a_source_position  *function_position,
                                 a_source_position  *id_position,
                                 an_operand         *function_operand)
/*
Overload resolution has been done, and it has been decided that, of the
functions in overloaded_function_symbol (which may be a projection symbol
and/or just a simple function), function_symbol is the specific function to
be called (also possibly a projection symbol).  Create a function designator
operand for the function in *function_operand.  The function was named with
a qualified name if is_qualified_name is TRUE.  The reference has an
associated selector object if *have_selector is TRUE; in that case,
bound_function_selector gives the object, and function_operand is bound to
that object.  Even when *have_selector is FALSE going in,
bound_function_selector must point at an operand that can be filled in
if an implicit selector is generated (*have_selector is set to TRUE for that
case).  function_position gives the source position of the function
in the call.  id_position gives the source position of the function name 
identifier in the call.
*/
{
  a_symbol_ptr base_function_symbol = fundamental_symbol_of(function_symbol);
  a_boolean    access_error_reported;
  a_type_ptr   routine_type;
  a_boolean    is_arrow_operator = TRUE;

  /* Do whatever would have been done to the function had we known
     originally which specific function was intended. */
  overloaded_function_catch_up(function_symbol,
                               overloaded_function_symbol,
                               is_qualified_name,
                               function_position,
                               id_position,
                               /*elided_reference=*/FALSE,
                               /*address_taken=*/FALSE,
                               function_operand,
                               &access_error_reported);
  /* Check whether or not a selector is needed. */
  routine_type = routine_symbol_type(base_function_symbol);
  if (routine_type_is_nonstatic_member_function(routine_type)) {
    /* The function needs a selector. */
    if (!*have_selector) {
      /* We don't have a selector.  Try to generate one. */
      if (make_this_pointer_operand(function_symbol,
                                    overloaded_function_symbol,
                                    function_position,
                                    (a_boolean)function_operand->
                                         access_control_error_reported,
                                    bound_function_selector)) {
        /* The selector was generated without problem. */
      } else {
        /* There was some problem in generating the selector. */
        conv_to_error_operand(function_operand);
      }  /* if */
      *have_selector = TRUE;
    } else {
      a_source_position selector_position;
      /* We have a selector. */
      /* Cast the selector to the class of the member symbol. */
      /* Also do the ARM 11.5 access checking for the type of selector used
         to access a protected member. */
      selector_position = bound_function_selector->position;
      cast_pointer_for_field_selection(bound_function_selector,
                                       &is_arrow_operator,
                                       function_symbol,
                                       overloaded_function_symbol,
                                       access_error_reported,
                                       /*do_protected_member_check=*/TRUE,
                                       &selector_position);
    }  /* if */
    /* Bind the function to the selector. */
    bind_member_function_operand_to_selector(function_operand,
                                             bound_function_selector);
  } else {
    /* The routine does not need a selector. */
    if (*have_selector) {
      /* Attach the unneeded selector provided to the function operand. */
      combine_unneeded_selector_with_operand(bound_function_selector,
                                             is_arrow_operator,
                                             function_operand);
      *have_selector = FALSE;
    }  /* if */
  }  /* if */
}  /* make_resolved_overloaded_function_operand */


static a_type_ptr next_printf_scanf_arg_type(
                                          a_boolean           is_scanf,
                                          char                **fmt_string_ptr,
                                          a_printf_scan_state *pss_ptr,
                                          a_source_position   *err_pos,
                                          a_boolean           *indirect,
                                          a_boolean           *weakly_typed,
                                          a_type_ptr          *alt_type)
/*
Return the type that the next argument to a printf or scanf call should have,
by finding the next thing in the format string that consumes an argument.
Return NULL if no further arguments are needed.  is_scanf is TRUE for
scanf/FALSE for printf; *fmt_string_ptr points to the current position 
in the format string (it will be updated); and *pss_ptr is maintained
to handle resuming the scan after a "*" field width or precision.
If there is an error in the format string, issue a warning at *err_pos and set
*fmt_string_ptr to NULL.  *indirect is returned TRUE if the type returned
has an added pointer level relative to the type indicated in the formatting
string, e.g., for scanf.  *weakly_typed is returned TRUE if the formatting
specifier is one that is weakly typed, e.g. "%x".  *alt_type is usually
returned NULL, but if some alternate type is also valid for the next
argument (i.e., in addition to the type returned), *alt_type is set to
the alternate type.

See 4.9.6.1 in the standard for printf, 4.9.6.2 for scanf.
*/
{
  a_type_ptr          required_type;
  char                *fmt_string = *fmt_string_ptr;
  a_printf_scan_state pss = *pss_ptr;
  a_boolean           l_size, L_size, h_size, add_pointer;
#if LONG_LONG_ALLOWED
  a_boolean           ll_size;
#endif /* LONG_LONG_ALLOWED */
  a_boolean           suppress_assignment = FALSE;

  *weakly_typed = FALSE;
  *indirect = FALSE;
  *alt_type = NULL;
  /* Pick up in the middle if the previous call returned a field width
     or precision. */
  if (pss == pss_after_field_width) goto after_field_width;
  if (pss == pss_after_precision) goto after_precision;

  /* Look for the next "%" in the string, or the null that terminates it. */
another_specifier:;
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
        pss = pss_after_precision;
        goto end_of_scan;
      }  /* if */
    }  /* if */
after_precision:;
    /* The optional size character is next.  l indicates long integer
       (sometimes double), L long double, and h short integer. */
    l_size = L_size = h_size = FALSE;
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
      h_size = TRUE;
      fmt_string++;
    }  /* if */
    /* The next character indicates the conversion type, e.g., "d" for
       decimal.  Determine the required type.  For most (but not all)
       scanf cases, "pointer to" will be added afterwards. */
    *indirect = add_pointer = is_scanf;
    switch (*fmt_string++) {
      case 'd':
      case 'i':
        /* int conversion.  If "l" was specified, long conversion;
           if "h" was specified for scanf, short conversion. */
#if LONG_LONG_ALLOWED
        /* If "ll" was specified, long long conversion. */
#endif /* LONG_LONG_ALLOWED */
        if (l_size) {
          required_type = integer_type((an_integer_kind)ik_long);
#if LONG_LONG_ALLOWED
        } else if (ll_size) {
          required_type = integer_type((an_integer_kind)ik_long_long);
          if (strict_ansi_mode) {
            pos_diagnostic((int)strict_ansi_error_severity < (int)es_warning ?
                             strict_ansi_error_severity : es_warning,
                           ec_nonstd_printf_format_string, err_pos);
          }  /* if */
#endif /* LONG_LONG_ALLOWED */
        } else if (h_size && is_scanf) {
          required_type = integer_type((an_integer_kind)ik_short);
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
           conversion; if "h" was specified for scanf, unsigned short 
           conversion. */
#if LONG_LONG_ALLOWED
        /* If "ll" was specified, unsigned long long conversion. */
#endif /* LONG_LONG_ALLOWED */
        if (l_size) {
          required_type = integer_type((an_integer_kind)ik_unsigned_long);
#if LONG_LONG_ALLOWED
        } else if (ll_size) {
          required_type = integer_type((an_integer_kind)ik_unsigned_long_long);
          if (strict_ansi_mode) {
            pos_diagnostic((int)strict_ansi_error_severity < (int)es_warning ?
                             strict_ansi_error_severity : es_warning,
                           ec_nonstd_printf_format_string, err_pos);
          }  /* if */
#endif /* LONG_LONG_ALLOWED */
        } else if (h_size && is_scanf) {
          required_type = integer_type((an_integer_kind)ik_unsigned_short);
        } else {
          required_type = integer_type((an_integer_kind)ik_unsigned_int);
        }  /* if */
        break;
      case 'a':
      case 'A':
      case 'f':
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
        /* int conversion for printf, string conversion for scanf. */
        if (is_scanf) {
          /* add_pointer is TRUE, so "char" will become "char *". */
          required_type = integer_type((an_integer_kind)ik_char);
        } else {
          required_type = integer_type((an_integer_kind)ik_int);
        }  /* if */
        break;
      case 's':
        /* String conversion.  "pointer to" will be added to make
           "char" into "char *", for both printf and scanf. */
        required_type = integer_type((an_integer_kind)ik_char);
        add_pointer = TRUE;
        /* *indirect is not set on purpose. */
        if (!is_scanf) {
          /* Allow a const char * argument to be passed to a %s printf
             specifier. */
          *alt_type = make_pointer_type(
                          make_qualified_type(required_type,
                                              (a_type_qualifier_set)TQ_CONST));
        }  /* if */
        break;
      case 'p':
        /* Pointer conversion.  Basic type is "void *". */
        required_type = make_pointer_type(void_type());
        *weakly_typed = TRUE;
        break;
      case 'n':
        /* Return number of characters read or written so far.
           Argument is "int *" for both printf and scanf, or "short *"
           if "h" was specified, or "long *" if "l" was specified. */
        if (l_size) {
          required_type = integer_type((an_integer_kind)ik_long);
        } else if (h_size) {
          required_type = integer_type((an_integer_kind)ik_short);
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
        required_type = integer_type((an_integer_kind)ik_char);
        break;
      default:
default_case:;
        /* Unknown formatting character.  Give warning and abandon checking. */
        pos_warning(ec_bad_printf_format_string, err_pos);
        required_type = NULL;
        fmt_string = NULL;
        goto end_of_scan;
    }  /* switch */
    /* Add a "pointer to" to the required type if necessary. */
    if (add_pointer) required_type = make_pointer_type(required_type);
  }  /* if */
  /* Next time around, look for a new specifier. */
  pss = pss_new_specifier;
  /* If there was an assignment-suppressing character "*" in a scanf, go
     get the next specifier. */
  if (suppress_assignment) goto another_specifier;
end_of_scan:;
  *fmt_string_ptr = fmt_string;
  *pss_ptr = pss;
  return required_type;
}  /* next_printf_scanf_arg_type */


static void check_printf_scanf_arg(an_operand          *argument_operand,
                                   a_boolean           is_scanf,
                                   char                **fmt_string_ptr,
                                   a_printf_scan_state *pss_ptr)
/*
Check an argument of a printf- or scanf-type function call to see if its
type matches the corresponding formatting specifier in the format string.
argument_operand points to the argument, is_scanf is TRUE for scanf/FALSE
for printf, and *fmt_string_ptr and *pss_ptr give the current position in the
format string (they are updated on return).
*/
{
  a_type_ptr required_type, eff_required_type, eff_argument_type;
  a_type_ptr alt_type;
  a_boolean  indirect, weakly_typed;

  /* Find the next formatting specifier in the string. */
  required_type = next_printf_scanf_arg_type(is_scanf, fmt_string_ptr,
                                             pss_ptr,
                                             &argument_operand->position,
                                             &indirect, &weakly_typed,
                                             &alt_type);
  /* If *fmt_string_ptr was set to NULL there was an error in the format
     string. */
  if (*fmt_string_ptr != NULL) {
    if (required_type == NULL) {
      /* There were no more formatting specifiers. */
      pos_warning(ec_too_many_printf_args, &argument_operand->position);
      /* Stop checking to avoid redundant errors. */
      *fmt_string_ptr = NULL;
    } else {
      /* Check that the argument type matches the specifier type.  Note
         the use of "interchangeable" rather than "compatible", because
         we want to allow things like "printf("%lx", (long)i);". */
      eff_required_type = required_type;
      eff_argument_type = argument_operand->type;
      if (indirect) {
        /* In cases where an extra indirection is added to the required
           type so that a value can be returned from the routine, remove
           the extra level of pointer type.  That allows matching things
           like "int *" and "unsigned int *".  This is slightly looser
           matching than is allowed without warning for normal function
           calls, but here we know what the runtime routine is doing. */
        if (!is_pointer_type(eff_argument_type)) goto mismatch;
        eff_argument_type = type_pointed_to(eff_argument_type);
        eff_required_type = type_pointed_to(eff_required_type);
      }  /* if */
      /* Drop type qualifiers. */
      eff_argument_type = skip_typerefs(eff_argument_type);
      eff_required_type = skip_typerefs(eff_required_type);
      if (types_are_compatible(eff_required_type, eff_argument_type)) {
        /* The types are exactly the same. */
      } else if (alt_type != NULL &&
                 types_are_compatible(alt_type, eff_argument_type)) {
        /* The type matches the alternate acceptable type. */
      } else if (weakly_typed &&
                 is_integral_or_enum_type(eff_required_type) &&
                 is_integral_or_enum_type(eff_argument_type) &&
                 integral_types_the_same_except_for_signedness(
                                       eff_required_type, eff_argument_type)) {
        /* For a weakly-typed specifier like "%x", allow an integral type
           even if its signedness is different. */
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
        pos_remark(ec_printf_arg_mismatch, &argument_operand->position);
      } else {
        /* The argument type does not match the required type. */
mismatch:
        if (!is_error_type(eff_argument_type)) {
          pos_warning(ec_printf_arg_mismatch, &argument_operand->position);
        }  /* if */
      }  /* if */
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
  arg_block->fmt_string = NULL;
  arg_block->pss = pss_new_specifier;
  arg_block->closing_paren_position = null_source_position;
  if (function_type != NULL) {
    /* The function type is known, so set the block to match it. */
    a_routine_type_supplement_ptr extra_info;

    /* Get information on the parameters of the function. */
    function_type = skip_typerefs(function_type);
#if CHECKING
    if (function_type->kind != (a_type_kind)tk_routine) {
      internal_error("scan_call_arguments: bad function type");
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
  }  /* if */
}  /* start_call_argument_processing */


void process_call_argument(an_operand         *argument_operand,
                           an_arg_check_block *arg_block)
/*
Check the argument expression indicated by argument_operand against the
corresponding parameter.  If it is compatible, convert it if necessary and
add the expression to the list of argument expressions attached to
*arg_block; otherwise, issue an error.  *arg_block contains information
about the current parameter, and is updated at the end of the call to
describe the next parameter.
*/
{
  an_expr_node_ptr curr_node;
  a_boolean        do_default_promotion;

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
    if (arg_block->curr_param_type != NULL) {
      do_default_promotion = FALSE;
    } else {
      /* No more formal arguments in the list. */
      if (!arg_block->has_ellipsis) {
        /* No ellipsis, so error: extra actual argument. */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (microsoft_mode && C_mode()) {
          /* MSVC++ 4.2 allows extra arguments with just a warning in
             C mode. */
          pos_warning(ec_too_many_arguments, &argument_operand->position);
        } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        {
          pos_error(ec_too_many_arguments, &argument_operand->position);
        }  /* if */
      }  /* if */
      arg_block->have_param_info = FALSE;
    }  /* if */
  } else {
    /* Old-style parameter list, for a function with a body (i.e., we know
       the argument types). */
    if (arg_block->curr_param_type == NULL) {
      /* No more formal arguments in the list. */
      if (arg_block->varargs_count == NOT_LINT_VARARGS) {
        /* A lint-style varargs comment does not apply, so warning:
           extra actual argument. */
        pos_warning(ec_too_many_arguments, &argument_operand->position);
        arg_block->have_param_info = FALSE;
      }  /* if */
    }  /* if */
  }  /* if */
  /* Do the argument conversion or promotion. */
  if (do_default_promotion) {
    /* Either an ellipsis was encountered or this is an old-style argument
       list; do the default argument promotion. */
    arg_default_promote_operand(argument_operand);
    /* If this is an old-style call and we have the list of types as
       defined by the function body, check the promoted type of the
       actual against the formal. */
    if (arg_block->have_param_info &&
        !arg_block->prototyped &&
        arg_block->curr_param_type != NULL) {
      /* Compare the type of the promoted actual with the promoted formal
         without qualifiers. */
      if (!is_error_type(arg_block->curr_param_type->type)) {
        a_type_ptr formal_type = default_argument_promotion(
                              skip_typerefs(arg_block->curr_param_type->type));
        if (!types_are_compatible(formal_type, argument_operand->type)) {
          if (interchangeable_types(formal_type, argument_operand->type)) {
            /* Types are interchangeable but not compatible (e.g.,
               unsigned int vs. int). */
            pos_remark(ec_old_style_incompatible_param,
                       &argument_operand->position);
#if TARG_NULL_IS_ALL_BITS_ZERO
          } else if (!strict_ansi_mode &&
                     is_pointer_type(formal_type) &&
                     is_integral_or_enum_type(argument_operand->type) &&
                     op_is_zero_constant(argument_operand) &&
                     skip_typerefs(formal_type)->size ==
                                 skip_typerefs(argument_operand->type)->size) {
            /* An uncast zero can be passed for a pointer parameter if
               the architecture uses all zero bits for a NULL pointer. */
            pos_remark(ec_old_style_incompatible_param,
                       &argument_operand->position);
#endif /* TARG_NULL_IS_ALL_BITS_ZERO */
          } else {
            /* Types are outright incompatible. */
            pos_warning(ec_old_style_incompatible_param,
                        &argument_operand->position);
          }  /* if */
        }  /* if */
      }  /* if */
    } else if (arg_block->fmt_string != NULL) {
      /* Check a printf or scanf argument type against the corresponding
         formatting specifier in the format string. */
      check_printf_scanf_arg(argument_operand,
                             (arg_block->arg_list_kind ==
                                                 (a_pragma_kind)pk_scanf_args),
                             &arg_block->fmt_string, &arg_block->pss);
    }  /* if */
  } else if (arg_block->unknown_dependent_function) {
    /* Argument of unknown template-dependent function. */
    prep_generic_operand(argument_operand, /*lvalue_expected=*/FALSE);
  } else {
    /* Parameter is prototyped. */
    /* Check the argument for compatibility against the parameter,
       casting it if necessary.  Also convert from lvalue to rvalue
       when appropriate. */
    prep_argument_operand(argument_operand, arg_block->curr_param_type,
                          (a_conv_descr_ptr)NULL, ec_incompatible_param);
  }  /* if */
  /* Link the new argument into the list of arguments. */
  curr_node = make_node_from_operand(argument_operand);
  if (arg_block->argument_head == NULL) {
    arg_block->argument_head = curr_node;
  } else {
    arg_block->argument_tail->next = curr_node;
  }  /* if */
  arg_block->argument_tail = curr_node;
  if (arg_block->curr_param_type != NULL) {
    /* Advance to the next parameter type entry in preparation for the
       next call of this routine. */
    arg_block->curr_param_type = arg_block->curr_param_type->next;
  }  /* if */
  /* If this is a call to a function with a printf- or scanf-style
     argument list and the ellipsis is next, the current argument is
     the format string.  See if it is constant; if so, we will be
     able to check the rest of the arguments against the format string
     as we scan them. */
  if ((arg_block->arg_list_kind == (a_pragma_kind)pk_printf_args ||
       arg_block->arg_list_kind == (a_pragma_kind)pk_scanf_args) &&
      arg_block->have_param_info && arg_block->curr_param_type == NULL) {
    /* See if the format string is a constant (actually, the address
       of a constant string). */
    if (curr_node->kind == (an_expr_node_kind)enk_constant) {
      /* The node is a constant. */
      a_constant_ptr con_ptr = curr_node->variant.constant;
      if (con_ptr->kind == (a_constant_repr_kind)ck_address &&
          con_ptr->variant.address.kind == (an_address_base_kind)abk_constant){
        /* The constant is a pointer to a constant.  We know the
           type is right because we passed the prototyped parameter
           type test above. */
        con_ptr = con_ptr->variant.address.variant.constant;
        if (con_ptr->kind == (a_constant_repr_kind)ck_string &&
            char_int_kind_from_string_type(con_ptr->type) ==
                                                         plain_char_int_kind) {
          /* The constant pointed to is a string (and not a wide string).
             Check that it is null-terminated. */
          arg_block->fmt_string = con_ptr->variant.string.value;
          arg_block->pss = pss_new_specifier;
          if (arg_block->fmt_string[con_ptr->variant.string.length-1] != '\0'){
            /* String is not null-terminated. */
            arg_block->fmt_string = NULL;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* process_call_argument */


void process_end_of_call_arguments(an_arg_check_block *arg_block)
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
        pos_error(ec_too_few_arguments, &arg_block->closing_paren_position);
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
      pos_warning(ec_too_few_arguments, &arg_block->closing_paren_position);
    }  /* if */
  }  /* if */
  if (arg_block->fmt_string != NULL) {
    /* For a printf- or scanf-like function, check that all the formatting
       specifiers were used. */
    a_boolean  indirect, weakly_typed;
    a_type_ptr alt_type;
    if (next_printf_scanf_arg_type((arg_block->arg_list_kind ==
                                                 (a_pragma_kind)pk_scanf_args),
                                   &arg_block->fmt_string,
                                   &arg_block->pss,
                                   &arg_block->closing_paren_position,
                                   &indirect, &weakly_typed,
                                   &alt_type) != NULL) {
      /* There are no more arguments, but the format string has more
         formatting specifiers. */
      pos_warning(ec_too_few_printf_args, &arg_block->closing_paren_position);
    }  /* if */
  }  /* if */
}  /* process_end_of_call_arguments */


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
    arg_default_promote_operand(operand);
  } else {
    /* Cast the argument to the right type. */
    prep_argument_operand(operand, param, conversion, ec_incompatible_param);
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
      arg = make_node_from_operand(&arg_operand->operand);
    }  /* if */
  }  /* if */
  return arg;
}  /* node_for_arg_of_overloaded_function_call */


void change_refs_on_selector_if_const_function(
                                           a_type_ptr routine_type,
                                           an_operand *bound_function_selector)
/*
A function with the indicated routine type is being called with the indicated
operand as its selector object.  If the function is const, change the type
of references to the object to be const-address-taken.
*/
{
  a_type_qualifier_set  this_qualifiers =
                         routine_type->variant.routine.extra_info->qualifiers;
  if (this_qualifiers & TQ_CONST) {
    /* The function is a const function, so indicate that the selector's
       address is taken only in a way that does not allow modification. */
    change_some_ref_kinds(bound_function_selector->ref_entries_list,
                          SRK_ADDRESS_TAKEN,
                          SRK_ADDRESS_TAKEN | SRK_CONST_ADDRESS_TAKEN);
  }  /* if */
}  /* change_refs_on_selector_if_const_function */


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
      check_assertion_str2(arg_match->is_match_for_this_param,
                           "adjust_overloaded_function_call_arguments:",
                           "is_match_for_this_param not set");
      /* Issue any warning about the "this" parameter detected while
         evaluating the alternatives. */
      if (bound_function_selector != NULL) {
        issue_warning_from_arg_match_summary(arg_match,
                                           &bound_function_selector->position);
      }  /* if */
      /* Note that no cast is done here.  It was done when the "." or "->"
         operator was processed (that still may leave a difference here
         involving type qualifiers, but it's not meaningful). */
      /* If the function is const, change the reference kinds on the
         selector. */
      change_refs_on_selector_if_const_function(routine_type,
                                                bound_function_selector);
    }  /* if */
    if (arg_match != NULL && arg_match->is_match_for_this_param) {
      /* Move past the match entry for the selector.  Note that this entry
         might be present even if this function doesn't need it. */
      arg_match = arg_match->next;
    }  /* if */
    prev_arg = NULL;
    /* Scan though the argument list. */
    for (arg_operand = arg_operand_list,
             param = routine_type->variant.routine.extra_info->param_type_list;
         arg_operand != NULL || param != NULL;) {
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
                           a_boolean               is_qualified_name,
                           an_error_code           err_none_applies,
                           an_error_code           err_ambiguous,
                           a_source_position       *call_position,
                           a_token_sequence_number paren_tok_seq_number,
                           a_source_position       *function_position,
                           a_source_position       *id_position,
                           a_source_position       *closing_paren_position,
                           a_boolean               *unknown_dependent_function,
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
filled in if an implicit selector is generated.  do_arg_dep_lookup is
TRUE if argument-dependent lookup should be done; if it is TRUE,
overloaded_function_symbol may be an sk_undefined symbol, indicating
that nothing was found on a normal id lookup of the function name.
try_surrogate_functions is TRUE if surrogate functions should be tried;
that means looking for conversion functions from the selector object
to pointers to function type.  overloaded_function_symbol can be NULL
in that case.  is_qualified_name is TRUE if a qualified name was used
to name the function (that suppresses the virtual-ness of the
function).  arg_operand_list is freed by this routine.  call_position
is the source position of the call.  paren_tok_seq_number is the token
sequence number of the opening parenthesis of the argument list, but
it's required only when do_arg_dep_lookup is TRUE; it can be zero
otherwise.  If an error of some sort is detected, issue an error at
that position and return NULL.  err_none_applies is the error code to
use when no function applies, and err_ambiguous is the error code to
use when more than one function applies.  If there is no error, an
operand for the function is built in *function_operand, an
expression-form argument list is built and returned in *arg_expr_list
(with the arguments cast to the proper types), and the type of the
routine selected is returned.  function_position is the position of
the function name or equivalent in the call, usually the same as
call_position.  id_position is the source position of the function
name identifier in the call.  closing_paren_position is the position
of the closing parenthesis in the call; it is used only when
do_arg_dep_lookup is TRUE.  If the call is dependent, and the function
to be called cannot be determined, return *unknown_dependent_function
set to TRUE (unknown_dependent_function can be NULL if the call cannot
be dependent).  This routine is called only in C++ mode.
*/
{
  an_arg_match_summary_ptr arg_match_list;
  a_symbol_ptr             function_symbol, base_function_symbol;
  a_boolean                single_function;
  a_routine_ptr            routine = NULL;
  a_type_ptr               routine_type = NULL;
  a_symbol_ptr             surrogate_function_conv_sym = NULL;

  db_enter(4, "select_and_prepare_to_call_overloaded_function");
  /* Select the best function out of the overload set. */
  function_symbol = select_overloaded_function(overloaded_function_symbol,
                                               is_template_id,
                                               template_arg_list,
                                               have_selector,
                                               bound_function_selector,
                                               arg_operand_list,
                                               do_arg_dep_lookup,
                                               err_none_applies,
                                               err_ambiguous,
                                               call_position,
                                               paren_tok_seq_number,
                                               &single_function,
                                               unknown_dependent_function,
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
                                              &have_selector,
                                              bound_function_selector,
                                              is_qualified_name,
                                              function_position,
                                              id_position,
                                              function_operand);
  } else if (surrogate_function_conv_sym != NULL) {
    /* A surrogate function was selected.  Convert the class object to
       a pointer to function using the conversion function, then call
       the function pointed to. */
    a_type_ptr conversion_type =
                      return_type_of(arg_match_list->conversion.routine->type);
    copy_operand(bound_function_selector, function_operand);
    conv_object_pointer_to_lvalue(function_operand);
    user_convert_operand(function_operand,
                         conversion_type,
                         &arg_match_list->conversion,
                         (a_conv_descr *)NULL,
                         /*force_temp_for_class_bitwise_copy=*/FALSE,
                         /*is_explicit_cast=*/FALSE);
    /* See whether the conversion function returns a reference type. */
    if (arg_match_list->conversion.result_is_an_lvalue) {
      routine_type = conversion_type;
      if (is_pointer_type(routine_type)) {
        /* Deal with the case of a conversion function returning a
           reference to pointer to function. */
        routine_type = type_pointed_to(conversion_type);
        conv_lvalue_to_rvalue(function_operand);
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
    an_arg_operand_ptr arg_operand;
    /* This shouldn't happen for nonstatic member functions. */
    check_assertion(!have_selector);
    function_symbol = fundamental_symbol_of(function_symbol);
    start_call_argument_processing(routine_symbol_type(function_symbol),
                                   function_symbol->variant.routine.ptr,
                                   &arg_block);
    arg_block.closing_paren_position = *closing_paren_position;
    for (arg_operand = arg_operand_list;
         arg_operand != NULL;
         arg_operand = arg_operand->next) {
      process_call_argument(&arg_operand->operand, &arg_block);
    }  /* for */
    process_end_of_call_arguments(&arg_block);
    *arg_expr_list = arg_block.argument_head;
    /* Free the argument list. */
    free_arg_operand_list(arg_operand_list);
  }  /* if */
  db_exit();
  return routine_type;
}  /* select_and_prepare_to_call_overloaded_function */


static a_boolean is_object_pointer_type(a_type_ptr tp)
/*
Return TRUE if the given type is a pointer to an object type.
Instantiate the underlying type if necessary to make it a complete type.
*/
{
  a_boolean result = FALSE;

  if (is_pointer_type(tp)) {
    a_type_ptr underlying_type = type_pointed_to(tp);
    complete_type_is_needed(underlying_type);
    result = is_object_type(underlying_type);
  }  /* if */
  return result;
}  /* is_object_pointer_type */


static void try_conversion_function_match(
                            an_operand               *source_operand,
                            a_type_ptr               dest_type,
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
    builtin_types_allowed != BTK_NONE (if both (a) and (b) apply,
    (b) takes precedence, and dest_type is used only to guide the
    selection of template conversion functions).

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
  a_type_ptr                eff_this_param_type;
  an_arg_match_summary      this_match;
  an_arg_match_summary_ptr  this_match_ptr;
  a_std_conv_descr          std_conversion;
  a_boolean                 compatible;
  a_boolean                 result_is_an_lvalue;
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
    dest_type = bool_type();
    builtin_types_allowed = (a_builtin_type_kind_set)BTK_NONE;
  } else if (builtin_types_allowed == BTK_PTRDIFF_T) {
    /* There's only one type in the BTK_PTRDIFF_T category, so make this a
       conversion to a specific type so that templates can be used. */
    dest_type = integer_type(targ_ptrdiff_t_int_kind);
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
    if (debug_level >= 4) {
      db_symbol(conversion_symbol,
                "try_conversion_function_match: considering ", 2); 
    }  /* if */
#endif /* DEBUG */
    /* Set template_arg_list early so that, on goto to reject_function, we
       know what has to be freed. */
    template_arg_list = NULL;
    conversion_routine = NULL;
    result_is_an_lvalue = FALSE;
    this_match_ptr = NULL;
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
      /* The symbol is a function template. */
      /* Don't do type deduction if that would produce a conversion
         function that returns an abstract class type (which would be
         invalid). */
      if (is_abstract_class_type(dest_type)) goto reject_function;
      /* Do type deduction on the return type. */
      tssp = base_conversion_symbol->variant.template_info;
      conversion_routine = tssp->variant.function.routine;
      conv_routine_type = conversion_routine->type;
      return_type = return_type_of(conv_routine_type);
      if (!matches_template_type(is_reference_binding ?
                                          dest_type : skip_typerefs(dest_type),
                                 return_type,
                                 &template_arg_list,
                                 tssp->variant.function.decl_cache.
                                                         decl_info->parameters,
                                 MTT_NO_FLAGS)) {
        /* Type deduction failed, so the conversion function is not viable. */
        goto reject_function;
      }  /* if */
      /* Make a version of the routine type with the proper types/values
         substituted for the template parameters. */
      conv_routine_type = wrapup_function_template_argument_deduction(
                                                   template_arg_list, 
                                                   base_conversion_symbol,
                                                   (a_template_param_ptr)NULL);
      if (conv_routine_type == NULL) goto reject_function;
    }  /* if */
    /* Is the type returned by this routine a type we want? */
    compatible = FALSE;
    clear_std_conv_descr(&std_conversion);
    conv_routine_type = skip_typerefs(conv_routine_type);
    return_type = return_type_of(conv_routine_type);
    result_is_an_lvalue = is_reference_type(conv_routine_type->
                                                  variant.routine.return_type);
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
        if (result_is_an_lvalue &&
            (!is_reference_binding || !types_match_ignoring_qualifiers)) {
          /* If the conversion function returns a reference to an array or
             function type, account for the type decay that follows. */
          if (is_array_type(return_type)) {
            return_type =
                       type_after_array_to_pointer_transformation(return_type);
            result_is_an_lvalue = FALSE;
          } else if (is_function_type(return_type)) {
            return_type =
             type_after_function_to_pointer_transformation(return_type,
                                                           (an_operand *)NULL);
            result_is_an_lvalue = FALSE;
          }  /* if */
          if (!result_is_an_lvalue) {
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
      if (((builtin_types_allowed & BTK_INTEGRAL) != 0 &&
                                              is_integral_type(return_type)) ||
          ((builtin_types_allowed & BTK_ENUM) != 0 &&
                                              is_enum_type(return_type)) ||
          ((builtin_types_allowed & BTK_BOOL) != 0 &&
                                              is_bool_type(return_type)) ||
          ((builtin_types_allowed & BTK_FLOATING) != 0 &&
                                              is_floating_type(return_type)) ||
          ((builtin_types_allowed & BTK_POINTER) != 0 &&
                                              is_pointer_type(return_type)) ||
          ((builtin_types_allowed & BTK_OBJECT_POINTER) != 0 &&
                                        is_object_pointer_type(return_type)) ||
          ((builtin_types_allowed & BTK_FUNCTION_POINTER) != 0 &&
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
                                   /*selector_is_object_pointer=*/FALSE,
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


static a_symbol_ptr find_conversion_function(a_type_ptr class_type,
                                             a_type_ptr dest_type)
/*
See if there is a conversion function that converts class_type (a class type)
to dest_type.  If so, return a pointer to the symbol.  If not, return NULL.

Note:  This looks like a general-purpose routine, but it's not.  It won't
deal with differences like

  operator int() const;
  operator int();

For which one needs to know the exact type of the operand to be converted.
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
       slep != NULL;
       slep = slep->next) {
    conversion_symbol = slep->symbol;
    reduce_projection_symbol_to_fundamental_symbol(conversion_symbol);
    conv_routine_type = routine_symbol_type(conversion_symbol);
    return_type = return_type_of(conv_routine_type);
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
          /* "!" takes an arithmetic, pointer, or pointer-to-member operand. */
          operand_type_pattern = "a;P;M";
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
           Also, if overloading on enums is enabled, matching enum types 
           (as of June 1997, that's an extension over the WP, but it
           seems necessary). */
        if (cfront_2_1_mode) {
          /* cfront 2.1 is confused and allows pointers to members on this
             case (they get rejected if chosen). */
          operand_type_pattern = "AA;=PP;=MM";
        } else if (microsoft_bugs) {
          /* Microsoft considers only arithmetic types, not pointers. */
          operand_type_pattern = "AA";
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
           enum types (as of June 1997, that's an extension over the WP,
           but it seems necessary). */
        if (operator_overloading_on_enums_enabled && !microsoft_mode) {
          operand_type_pattern = "AA;=PP;=MM;=EE";
        } else {
          operand_type_pattern = "AA;=PP;=MM";
        }  /* if */
        break;
      case onk_and_and:
      case onk_or_or:
        if (bool_is_keyword) {
          /* "&&" and "||" take bool operands. */
          operand_type_pattern = "BB";
        } else {
          /* "&&" and "||" take arithmetic, pointer, or pointer-to-member
             operands, but they can be mixed. */
          operand_type_pattern = "aa;aP;aM;Pa;PP;PM;Ma;MP;MM";
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
        operand_type_pattern = "OD;DO";
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
    case PROMOTED_ARITH_TYPE_CODE:
      matches = is_arithmetic_or_enum_type(type);
      break;
    case POINTER_TYPE_CODE:
      matches = is_pointer_type(type);
      break;
    case OBJECT_POINTER_TYPE_CODE:
      matches = is_object_pointer_type(type);
      break;
    case FUNCTION_POINTER_TYPE_CODE:
      matches = is_pointer_type(type) &&
                is_function_type(type_pointed_to(type));
      break;
    case PTR_TO_MEMBER_TYPE_CODE:
      matches = is_ptr_to_member_type(type);
      break;
    case BOOL_TYPE_CODE:
      /* Arithmetic includes bool. */
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
    case OBJECT_POINTER_TYPE_CODE:
      builtin_types_allowed = BTK_OBJECT_POINTER;
      break;
    case FUNCTION_POINTER_TYPE_CODE:
      builtin_types_allowed = BTK_FUNCTION_POINTER;
      break;
    case PTR_TO_MEMBER_TYPE_CODE:
      builtin_types_allowed = BTK_PTR_TO_MEMBER;
      break;
    case BOOL_TYPE_CODE:
      builtin_types_allowed = BTK_BOOL;
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
            if (!types_are_compatible(promoted_type, operand_type)) {
              /* The type gets changed by promotion, so the cost is a
                 promotion. */
              match_level = aml_promotion;
            }  /* if */
          }  /* if */
        }  /* if */
      } else if (is_floating_type(operand_type)) {
        /* The operand has a floating type. */
        if (type_code == PROMOTED_ARITH_TYPE_CODE) {
          /* A promoted type is required.  See if the type gets changed by
             promotion. */
          a_type_ptr promoted_type = default_argument_promotion(operand_type);
          if (!types_are_compatible(promoted_type, operand_type)) {
            /* The type gets changed by promotion, so the cost is a
               promotion. */
            match_level = aml_promotion;
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
  a_boolean                ambiguous, need_lvalue_result;
#if DEBUG
  unsigned long            narg;
#endif /* DEBUG */

  /* This routine is similar to try_overloaded_function_match (but it only
     looks at one type pattern per call). */
#if DEBUG
  if (debug_level >= 4) {
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
    if (debug_level >= 4) {
      fprintf(f_debug, "try_builtin_operands_match: operand %lu\n", narg);
    }  /* if */
#endif /* DEBUG */
    /* Get the operand type. */
    operand_type = arg_operand->operand.type;
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
        }  /* if */
      }  /* if */
    } else {
      /* A specific type is required.  Check that the operand can be
         converted to the type passed in. */
      a_type_ptr eff_specific_type = specific_type;
      if (*type_pattern_position == OBJECT_POINTER_TYPE_CODE &&
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
                                         /*is_copy_initialization=*/TRUE,
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
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (microsoft_mode && !source_is_constant) {
          /* Microsoft mode allows some expressions as null pointer
             constants. */
          adjust_constant_operand_info_for_microsoft_null_pointer_test(
                                                         &arg_operand->operand,
                                                         &source_is_constant,
                                                         &source_constant);
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
                                  a_type_ptr specific_type,
                                  a_type_ptr class_type,
                                  a_type_ptr previous_class_type_considered,
                                  a_type_ptr previous_specific_type_considered)
/*
Helper routine for try_corresp_builtin_operands_match.  Return TRUE if
specific_type has already been tried as a target specific type.
class_type, if non-NULL, indicates the current operand class type.
previous_class_type_considered, if non-NULL, indicates the type of a
previous class operand already considered; previous_specific_type_considered,
if non-NULL, indicates the type of a previous non-class operand already
considered.
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
    if (class_type == previous_class_type_considered ||
        find_conversion_function(previous_class_type_considered,
                                 specific_type) != NULL) {
      /* This specific type was tried when the first operand was
         processed, so do not try it again (if we did, it would
         look like an ambiguity). */
      previously_handled = TRUE;
    }  /* if */
  }  /* if */
  return previously_handled;
}  /* specific_type_previously_handled */


static void adjust_specific_type_for_previous_specific_type(
                                            a_type_ptr  *specific_type,
                                            a_type_ptr  previous_specific_type)
/*
Helper function for adjust_specific_type_for_previous_operand.  If
previous_specific_type is a pointer type, look for a pointer type that
both *specific_type and previous_specific_type can be converted to,
e.g., by creating a type with the union of the cv-qualifiers on the
two types.
*/
{
  if (is_pointer_type(previous_specific_type)) {
    /* Look for the usual pointer cases. */
    a_type_ptr composite =
                     multilevel_composite_pointer_type(*specific_type,
                                                       previous_specific_type);
    if (composite != NULL) {
      *specific_type = composite;
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
        a_type_ptr underlying_type = type_pointed_to(*specific_type);
        a_type_ptr other_underlying_type =
                                     type_pointed_to(previous_specific_type);
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
}  /* adjust_specific_type_for_previous_specific_type */


static void adjust_specific_type_for_previous_operand(
                              a_type_ptr     *specific_type,
                              a_type_ptr     class_type,
                              an_opname_kind kind,
                              a_type_ptr     previous_class_type_considered,
                              a_type_ptr     previous_specific_type_considered)
/*
We are considering the type given by *specific_type as the operation
type for a built-in operation.  If *specific_type is the result type of a
conversion function, class_type indicates the class type of the operand;
otherwise, it is NULL.  The kind of operation is indicated by kind.
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
            a_type_ptr   return_type = return_type_of(conv_routine_type);
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
                                          *specific_type, class_type,
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
    if (*type_pattern_position == OBJECT_POINTER_TYPE_CODE &&
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
        return_type = return_type_of(conv_routine_type);
        if (type_matches_type_code(return_type, *type_pattern_position)) {
          /* We've found a conversion function to an appropriate type.  Make
             sure it's not a type we've already checked while examining a
             previous operand.  If it is, ignore it. */
          specific_type = return_type;
          if (!specific_type_previously_handled(
                                          specific_type, class_type,
                                          previous_class_type_considered,
                                          previous_specific_type_considered)) {
            /* Try matching the operands, with the chosen specific type. */
            any_approp_conversion_function_this_operand = TRUE;
            adjust_specific_type_for_previous_operand(
                                            &specific_type, class_type,
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
      if (type_matches_type_code(operand_type, *type_pattern_position)) {
        /* The operand has an appropriate type. */
        specific_type = operand_type;
        /* If the type has been previously handled, ignore it. */
        if (!specific_type_previously_handled(
                                          specific_type, (a_type_ptr)NULL,
                                          previous_class_type_considered,
                                          previous_specific_type_considered)) {
          adjust_specific_type_for_previous_operand(
                                            &specific_type, (a_type_ptr)NULL,
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
       checked later.  Note that we need not check for a modifiable
       lvalue, because the processing for the built-in operator will do
       that if necessary.  In fact, the processing for the built-in
       operator will do full checking, so the checking here is just
       looking for obvious mismatches. */
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
For unconventional "this" arguments, convert the selector to an object
pointer and cast it to a base class if necessary.  This is needed for
conversion functions, but not for function calls using the usual notation
or for operator functions (in those cases, the "catch up" processing
does the base class cast).  operand gives the selector, and routine_type
gives the type of the routine being called.
*/
{
  a_type_ptr       this_param_type, this_class_type, operand_class_type;
  a_base_class_ptr bcp;

  conv_class_operand_to_object_pointer(operand);
  /* If the function is const, change the reference kinds on the selector. */
  change_refs_on_selector_if_const_function(routine_type, operand);
  this_param_type = implicit_this_param_type_of(routine_type);
  this_class_type = routine_type->variant.routine.extra_info->this_class;
  if (is_pointer_type(operand->type)) {
    operand_class_type = f_skip_typerefs(type_pointed_to(operand->type));
    if (operand_class_type != this_class_type &&
        is_immediate_class_type(operand_class_type) &&
        (bcp = find_base_class_of(operand_class_type, this_class_type))!=NULL){
      /* Do the cast to a base class.  Access checking is suppressed on this
         cast, because the cast is really necessary only because the function
         is inherited from a base class.  This is not clear from the ARM,
         but cfront and Borland do it this way. */
      base_class_cast_operand(operand, bcp, (a_boolean *)NULL,
                              /*check_cast_access=*/FALSE,
                              /*is_implicit_cast=*/TRUE,
                              /*implicit_in_naming=*/FALSE,
                              /*is_object_pointer=*/TRUE);
    }  /* if */
  }  /* if */
  /* The cast here handles const/volatile differences and error cases. */
  cast_operand(this_param_type, operand, /*check_cast_access=*/TRUE,
               /*is_implicit_cast=*/TRUE, /*is_reinterpret_cast=*/FALSE,
               /*reinterpret_semantics=*/FALSE);
}  /* prep_special_selector_operand */


static void adjust_class_object_type(an_operand       *operand,
                                     a_type_ptr       dest_type,
                                     a_base_class_ptr bcp)
/*
*operand is a class lvalue or rvalue.  Adjust its type to dest_type,
which may differ from the current type in being a base class or having
different cv-qualifiers.  bcp is non-NULL to indicate the base class
case.  This adjustment does not make a new object; it merely adjusts
the operand type to access the same class object with a new type.
On return, the operand is an lvalue.
*/
{
  /* Convert to a pointer to the object. */
  conv_class_operand_to_object_pointer(operand);
  if (bcp != NULL) {
    /* Cast the pointer to the proper base class. */
    base_class_cast_operand(operand, bcp,
                            (a_boolean *)NULL,
                            /*check_cast_access=*/TRUE,
                            /*is_implicit_cast=*/TRUE,
                            /*implicit_in_naming=*/FALSE,
                            /*is_object_pointer=*/TRUE);
  }  /* if */
  /* Adjust cv-qualifiers. */
  cast_operand(make_pointer_type(dest_type), operand,
               /*check_cast_access=*/TRUE, /*is_implicit_cast=*/TRUE,
               /*is_reinterpret_cast=*/FALSE, /*reinterpret_semantics=*/FALSE);
  /* Make an address (an lvalue) for the adjusted class object. */
  conv_object_pointer_to_lvalue(operand);
}  /* adjust_class_object_type */


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
                             /*force_temp_for_class_bitwise_copy=*/FALSE,
                             /*is_explicit_cast=*/FALSE);
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
          pos_ty_start_error(ec_ambiguous_conversion_to_builtin,
                             &operand->position, operand->type);
          diagnose_overload_ambiguity(ambiguity_list,
                                      (an_operand *)NULL,
                                      (an_arg_operand_ptr)NULL,
                                      (an_opname_kind)onk_none);
          free_candidate_function_list(ambiguity_list);
        }  /* if */
        conv_to_error_operand(operand);
      }  /* if */
    } else {
      /* A specific type is wanted.  Convert to the type indicated in
         candidate_function. */
      if (type_code == OBJECT_POINTER_TYPE_CODE &&
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
                                &arg_match->conversion,
                                /*is_copy_initialization=*/TRUE,
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
                                          an_opname_kind    kind,
                                          a_boolean         unary_operator,
                                          an_operand        *operand_1,
                                          an_operand        *operand_2,
                                          an_operand        *result,
                                          a_source_position *operator_position)
/*
Make an operand for a "generic" operation, i.e., one on template-dependent
operands in a prototype instantiation.  kind indicates the operation,
and unary_operator is TRUE if the operation is a unary operation.
operand_1 and operand_2 are the operands (operand_2 is needed only
for non-unary operations).  The result operand is returned in *result.
*operator_position gives the source position of the operator.
*/
{
  an_expr_operator_kind generic_op =
                        generic_operator_for_opname_kind(kind, unary_operator);

  if (unary_operator) {
    if (generic_op == (an_expr_operator_kind)eok_address) {
      /* For unary "&", do some special processing. */
      /* If the operand is a member of a nonreal class, e.g., &T::x,
         make an lvalue for the member instead of the previous assumption
         that it is a constant. */
      change_nonreal_member_constant_operand_to_lvalue(operand_1);
      if (curr_expr_kind_is_const()) {
        /* In a constant expression, we can check that the entity is
           an lvalue.  (In non-constant expressions in prototype
           instantiations, the lvalue-ness of operands is sometimes
           unknowable.) */
        if (!is_an_lvalue(operand_1) &&
            !is_a_function_designator(operand_1) &&
            !is_error_operand(operand_1)) {
          error_in_operand(ec_expr_not_an_lvalue_or_function_designator,
                           operand_1);
        }  /* if */
      }  /* if */
    }  /* if */
    template_unary_operation(generic_op, operand_1, result,
                             operator_position);
  } else {
    /* Two-operand operation. */
    template_binary_operation(generic_op, operand_1, operand_2,
                              result, operator_position);
  }  /* if */
}  /* make_generic_operation_operand */


void check_for_operator_overloading(
                               an_opname_kind          kind,
                               a_boolean               unary_operator,
                               a_boolean               must_be_member_function,
                               a_boolean               try_conversions,
                               a_boolean               has_predef_meaning,
                               an_operand              *operand_1,
                               an_operand              *operand_2,
                               a_source_position       *operator_position,
                               a_token_sequence_number operator_tok_seq_number,
                               an_operand              *result,
                               a_boolean               *processed)
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
the operator.  This routine also checks for template-dependent operands
in a prototype instantiation, and builds a generic expression for
such cases (where operator overloading might apply, but we can't tell).
*/
{
  an_arg_operand_ptr       arg_operand_list, arg_operand_list2, arg_operand;
  an_expr_node_ptr         arg_expr_list, end_arg_expr_list;
  a_symbol_ptr             nonmember_functions_symbol;
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

  db_enter(4, "check_for_operator_overloading");
  *processed = FALSE;
  /* Check for template-dependent operands in a prototype instantiation. */
  if (is_template_dependent_context() &&
      (is_template_dependent_type(operand_1->type) ||
       (!unary_operator && is_template_dependent_type(operand_2->type)))) {
    /* There is at least one template-dependent operand, so we cannot
       check for operator overloading.  Just build an expression with a
       generic operator. */
    make_generic_operation_operand(kind, unary_operator, operand_1, operand_2,
                                   result, operator_position);
    *processed = TRUE;
  } else if (!curr_expr_kind_is_const()) {
    /* Check for operator overloading (but not in constant expressions). */
    if (is_error_operand(operand_1) || 
        (!unary_operator && is_error_operand(operand_2))) {
      /* One or both of the operands is an error operand. */
      if ((opname_symbol_table[kind] != NULL &&
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
                   depth_innermost_instantiation_scope != NO_SCOPE_DEPTH) {
          /* In a real (not prototype) instantiation, and doing dependent
             name processing.  Look up this call to see whether it was a
             dependent call in the prototype instantiation.  If it was a
             nondependent call, it was recorded, along with (usually) the
             symbol chosen by overload resolution. */
          a_nondependent_call_info_ptr ndcall_info;
          ndcall_info = get_nondependent_call_info(operator_tok_seq_number);
          dependent_call = (ndcall_info == NULL);
          function_symbol = NULL;
          if (!dependent_call) function_symbol = ndcall_info->symbol;
          if (function_symbol != NULL) {
            /* We know the function selected for this nondependent call
               during the prototype instantiation.  Use that without going
               through overload resolution. */
            a_boolean is_member = routine_type_is_nonstatic_member_function(
                                         routine_symbol_type(function_symbol));
            if (is_member) {
              member_functions_symbol = function_symbol;
            } else {
              nonmember_functions_symbol = function_symbol;
            }  /* if */
            try_overloaded_function_match(
                                         function_symbol,
                                         /*is_template_id=*/FALSE,
                                         (a_template_arg_ptr)NULL,
                                         is_member ? arg_operand_list2 :
                                                     arg_operand_list,
                                         /*have_selector=*/is_member,
                                         is_member ? operand_1 :
                                                     (an_operand *)NULL,
                                         /*selector_is_object_pointer=*/FALSE,
                                         /*ctor_conversion_case=*/FALSE,
                                         /*effects_copy_initialization=*/FALSE,
                                         /*allow_udc_on_arguments=*/TRUE,
                                         /*from_arg_dep_lookup=*/FALSE,
                                         /*dependent_call=*/FALSE,
                                         /*known_to_be_visible=*/TRUE,
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
                                         /*selector_is_object_pointer=*/FALSE,
                                         /*ctor_conversion_case=*/FALSE,
                                         /*effects_copy_initialization=*/FALSE,
                                         /*allow_udc_on_arguments=*/TRUE,
                                         /*from_arg_dep_lookup=*/FALSE,
                                         dependent_call,
                                         /*known_to_be_visible=*/TRUE,
                                         &candidate_functions,
                                         &matched_except_for_missing_selector,
                                         &matched_except_for_selector);
          }  /* if */
        }  /* if */
        /* Find any non-member function for the operator. */
        if (!must_be_member_function) {
          a_symbol_ptr            normal_sym;
          a_symbol_locator        locator;
          a_type_list_entry_ptr   type_list = NULL;
          a_symbol_list_entry_ptr symbol_list, slep;
          /* If the second operand has a template class type, try to
             instantiate it to expose any friend functions it declares. */
          if (!unary_operator && is_class_struct_union_type(operand_2->type)) {
            instantiate_template_class(operand_2->type);
          }  /* if */
          /* Do the normal id lookup on the operator name, e.g., for "+"
             look up "operator +".  Ignore member functions, which were
             covered above. */
          make_opname_locator(kind, &locator, operator_position);
          normal_sym = normal_id_lookup(&locator, IDL_SKIP_CLASS_SCOPES);
          if (normal_sym != NULL &&
              !is_function_or_template_symbol(normal_sym)) {
            /* Ignore error symbols and like. */
            normal_sym = NULL;
          }  /* if */
          if (!strict_ansi_mode && normal_sym != NULL &&
              is_local_symbol(normal_sym)) {
            /* If the symbol found is a block extern or using-declaration,
               skip the argument-dependent processing.  This is not in the
               standard, but at the Nov. 98 standards committee meeting there
               was strong sentiment for altering the rule to do it this way. */
          } else {
            /* Build a list of the argument types, to be used to do
               argument-dependent lookup below. */
            add_operand_to_arg_dependent_lookup_list(operand_1, &type_list);
            if (!unary_operator) {
              add_operand_to_arg_dependent_lookup_list(operand_2, &type_list);
            }  /* if */
          }  /* if */
          /* Do argument-dependent lookup, producing a list of symbols to
             be considered as candidate functions. */
          symbol_list = argument_dependent_lookup(normal_sym, &locator,
                                                  &type_list);
          for (slep = symbol_list; slep != NULL; slep = slep->next) {
            nonmember_functions_symbol = slep->symbol;
            if (is_template_dependent_context() &&
                is_block_extern_symbol(nonmember_functions_symbol)) {
              /* A block extern declaration in a prototype instantiation
                 can be dependent (e.g., it can have dependent parameter
                 types or dependent default argument expressions), so we
                 can't do overload resolution. */
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
                                         /*selector_is_object_pointer=*/TRUE,
                                         /*ctor_conversion_case=*/FALSE,
                                         /*effects_copy_initialization=*/FALSE,
                                         /*allow_udc_on_arguments=*/TRUE,
                                         /*from_arg_dep_lookup=*/
                                                 (slep != symbol_list ||
                                                  nonmember_functions_symbol !=
                                                  normal_sym),
                                         dependent_call,
                                         /*known_to_be_visible=*/FALSE,
                                         &candidate_functions,
                                         &matched_except_for_missing_selector,
                                         &matched_except_for_selector);
            }  /* if */
          }  /* for */
          free_list_of_symbol_list_entries(symbol_list);
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
                                         result, operator_position);
          check_assertion(!dependent_call);
          if (is_prototype_instantiation_context()) {
            /* Make sure this call is treated as a nondependent call in
               a real instantiation. */
            record_nondependent_call((a_symbol_ptr)NULL,
                                     operator_tok_seq_number);
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
            pos_st_start_error(ec_no_matching_operator_function,
                               operator_position,
                               opname_names[(int)kind]);
            display_operand_types(arg_operand_list, kind);
            end_error();
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
          pos_st_start_error(ec_ambiguous_operator_function, operator_position,
                             opname_names[(int)kind]);
          diagnose_overload_ambiguity(candidate_functions,
                                      (an_operand *)NULL,
                                      arg_operand_list,
                                      kind);
          make_error_operand(result);
          arg_operand_list_not_used = TRUE;
        } else {
          /* Exactly one function applies and is best. */
          proj_function_symbol = candidate_functions->function_symbol;
          arg_match = candidate_functions->arg_matches;
          if (proj_function_symbol == NULL) {
            a_boolean op_1_inside_conditional = FALSE,
                      op_2_inside_conditional = FALSE;
            /* A built-in operator was selected. */
#if DEBUG
            if (debug_level >= 4) {
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
            if (debug_level >= 4) {
              db_symbol(proj_function_symbol,
                        "check_for_operator_overloading: selected ", 2);
            }  /* if */
#endif /* DEBUG */
            *processed = TRUE;
            function_symbol = fundamental_symbol_of(proj_function_symbol);
            routine_type = routine_symbol_type(function_symbol);
            if (do_dependent_name_processing &&
                is_prototype_instantiation_context()) {
              /* Record the outcome of overload resolution for a nondependent
                 call in a prototype instantiation.  Dependent calls in such
                 a context don't get here. */
              check_assertion(!dependent_call &&
                              operator_tok_seq_number != 0);
              record_nondependent_call(function_symbol,
                                       operator_tok_seq_number);
            }  /* if */
            /* Check for the builtin operator=. */
            if (kind == (an_opname_kind)onk_assign &&
                function_symbol->kind == (a_symbol_kind)sk_member_function &&
                function_symbol->variant.routine.ptr->compiler_generated &&
                symbol_supplement_for_class(
                                       function_symbol->parent.class_type)->
                                          assignment_by_bitwise_copy_allowed) {
              /* This function is the default bitwise copy assignment
                 operator, so generate an assignment instead of a call. */
#if DEBUG
              if (debug_level >= 4) {
                fprintf(f_debug,
                 "check_for_operator_overloading: bitwise operator=\n");
              }  /* if */
#endif /* DEBUG */
              bitwise_assignment = TRUE;
            }  /* if */
            arg_operand = arg_operand_list;
            bound_function_selector = NULL;
            member_is_best_match = 
                       routine_type_is_nonstatic_member_function(routine_type);
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
              a_type_ptr       result_type= function_symbol->parent.class_type;
              an_expr_node_ptr assign_node, lhs_node, rhs_node;

              /* Make a pointer for the selector, and adjust its type if
                 necessary. */
              prep_special_selector_operand(bound_function_selector,
                                            routine_type);
              change_some_ref_kinds(bound_function_selector->ref_entries_list,
                                    SRK_ADDRESS_TAKEN,
                                    SRK_MODIFICATION);
              /* Cast the source operand to the right type. */
              prep_assignment_operand(&arg_operand->operand,
                                      result_type,
                                      ec_incompatible_param,
                                      operator_position);
              rhs_node = make_node_from_operand(&arg_operand->operand);
              lhs_node = make_node_from_operand(bound_function_selector);
              lhs_node->next = rhs_node;
              assign_node = make_operator_node(
                                            (an_expr_operator_kind)eok_sassign,
                                            lhs_node->type, lhs_node);
              assign_node->variant.operation.
                                 returns_lvalue_instead_of_usual_rvalue = TRUE;
              make_expression_operand(assign_node, result_type, result);
              result->state = (an_operand_state)os_lvalue;
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
                /* Convert the selector to a pointer. */
                conv_class_operand_to_object_pointer(bound_function_selector);
              }  /* if */
              /* Do the things that would have been done to the symbol but
                 weren't because the specific symbol was not known, and build
                 an operand for the function. */
              make_resolved_overloaded_function_operand(
                                                 proj_function_symbol,
                                                 member_is_best_match ?
                                                    member_functions_symbol :
                                                    nonmember_functions_symbol,
                                                 &have_selector,
                                                 bound_function_selector,
                                                 /*is_qualified_name=*/FALSE,
                                                 operator_position,
                                                 operator_position,
                                                 &function_operand);
              /* Make the call node and an operand for it. */
              assemble_function_call(&function_operand,
                                     bound_function_selector,
                                     arg_expr_list,
                                     /*compiler_generated=*/TRUE,
                                     /*is_conversion=*/FALSE,
                                     operator_position, result);
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
  if (*processed) result->position = *operator_position;
  db_exit();
}  /* check_for_operator_overloading */


a_boolean conversion_to_class_possible(
                            an_operand               *source_operand,
                            a_type_ptr               dest_type,
                            a_boolean                try_bitwise_copy,
                            a_boolean                is_copy_initialization,
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
try_bitwise_copy is TRUE.  If is_copy_initialization is TRUE, the
initialization is copy-initialization ("="-form initialization); if
FALSE, it's direct-initialization ("()"-form initialization).
User-defined conversions on constructor arguments are considered only
for direct-initialization.  If is_reference_binding is TRUE, the
result will be bound to a reference, so also consider conversions to
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

  db_enter(4, "conversion_to_class_possible");
  /* Note that this routine is like a simplified version of
     select_overloaded_function that works for user-defined conversion
     functions (no arguments, just a "this" parameter). */
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
     has a reference-to-const parameter and cannot copy a volatile object). */
  cctor_is_bitwise_copy = cssp->construction_by_bitwise_copy_allowed;
  bitwise_copy_okay = try_bitwise_copy &&
                      cctor_is_bitwise_copy &&
                      !any_qualifier_in_set_missing(TQ_CONST,
                                                    source_qualifiers);
  if (bitwise_copy_okay && type_is_same) {
    /* The source and destination types are the same class type, and a
       bitwise copy is allowed on that type.  That means there are no
       copy constructors, and therefore the bitwise copy is the best
       match. */
    conversion->class_identity_or_bitwise_copy = TRUE;
    okay = TRUE;
  } else if (is_template_dependent_context() &&
             (is_template_dependent_type(source_type) ||
              class_type->variant.class_struct_union.is_nonreal_class)) {
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
                                    /*selector_is_object_pointer=*/FALSE,
                                    /*ctor_conversion_case=*/TRUE,
                                    /*effects_copy_initialization=*/
                                                        is_copy_initialization,
                                    /*allow_udc_on_arguments=*/
                                              !adjusted_is_copy_initialization,
                                    /*from_arg_dep_lookup=*/FALSE,
                                    /*dependent_call=*/FALSE,
                                    /*known_to_be_visible=*/FALSE,
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
        if (any_cfront_mode() || sun_mode) {
          /* Cfront and the Sun 5.0 compiler do this in a nonstandard way. */
        } else {
          try_as_arg_of_bitwise_cctor = TRUE;
        }  /* if */
      } else {
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
  if (debug_level >= 4) {
    fprintf(f_debug, "conversion_to_class_possible: %s\n",
                     okay ? "okay" : "not okay");
  }  /* if */
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
    builtin_types_allowed != BTK_NONE (if both (a) and (b) apply,
    (b) takes precedence, and dest_type is used only to guide the
    selection of template conversion functions),

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

  db_enter(4, "conversion_from_class_possible");
  /* This routine is similar to select_overloaded_function. */
  clear_conv_descr(conversion);
  if (is_template_dependent_context() &&
      (f_skip_typerefs(source_operand->type)->
                                 variant.class_struct_union.is_nonreal_class ||
       (dest_type != NULL && is_template_dependent_type(dest_type)))) {
    /* Assume a conversion to or from an unknown type in a prototype
       instantiation is allowed. */
    okay = TRUE;
    conversion->unknown_dependent_conversion = TRUE;
  } else {
    candidate_functions = NULL;
    /* Find any viable conversion functions. */
    try_conversion_function_match(source_operand, dest_type,
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
  if (debug_level >= 4) {
    fprintf(f_debug, "conversion_from_class_possible: %s\n",
                     okay ? "okay" : "not okay");
  }  /* if */
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
                           /*force_temp_for_class_bitwise_copy=*/FALSE,
                           /*is_explicit_cast=*/FALSE);
      *processed = TRUE;
    } else if (ambiguous) {
      /* There is more than one possible conversion to a built-in type. */
      /* A NULL ambiguity_list indicates a case that was undecidable because
         of an error (no additional error is needed). */
      if (ambiguity_list != NULL) {
        pos_ty_start_error(ec_ambiguous_conversion_to_builtin,
                           &operand->position, operand->type);
        diagnose_overload_ambiguity(ambiguity_list,
                                    (an_operand *)NULL,
                                    (an_arg_operand_ptr)NULL,
                                    (an_opname_kind)onk_none);
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
                                        a_boolean    is_copy_initialization,
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
If is_copy_initialization is TRUE, this is copy-initialization
("="-form initialization); if FALSE, it's direct-initialization
("()"-form initialization).  User-defined conversions on constructor
arguments are considered only for direct-initialization.  If
is_reference_binding is TRUE, the result will be bound to a reference,
so also consider conversions to derived classes of dest_type.  If
ctor_arg_conversion is non-NULL, return a description of the
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
                                     is_copy_initialization,
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
        pos_ty_error(ec_converting_to_incomplete_class,
                     &source_operand->position, diag_dest_type);
      } else {
        /* Put out the usual message (which has already been chosen to
           describe the problem). */
        if (single_type_message) {
          /* Single-type case. */
          pos_ty_error(err_code, &source_operand->position, class_type);
          conv_to_error_operand(source_operand);
        } else {
          /* Normal double-type case. */
          type2_error_in_operand(err_code, source_operand,
                                 source_type, diag_dest_type);
        }  /* if */
      }  /* if */
    } else {
      /* More than one conversion applies (ambiguity). */
      /* A NULL ambiguity_list indicates a case that was undecidable because
         of an error (no additional error is needed). */
      if (ambiguity_list != NULL) {
        if (single_type_message) {
          pos_ty_start_error(err_code, &source_operand->position, class_type);
        } else {
          pos_ty2_start_error(err_code, &source_operand->position,
                              source_type, diag_dest_type);
        }  /* if */
        diagnose_overload_ambiguity(ambiguity_list,
                                    (an_operand *)NULL,
                                    (an_arg_operand_ptr)NULL,
                                    (an_opname_kind)onk_none);
        free_candidate_function_list(ambiguity_list);
      }  /* if */
    }  /* if */
    conv_to_error_operand(source_operand);
  }  /* if */
  return okay;
}  /* user_defined_conversion_possible */


static a_boolean conversion_possible(
                                   an_operand        *source_operand,
                                   a_type_ptr        dest_type,
                                   a_type_ptr        orig_dest_type,
                                   a_boolean         need_lvalue_result,
                                   a_boolean         is_copy_initialization,
                                   a_boolean         is_reference_binding,
                                   an_error_code     incompatible_err,
                                   a_source_position *err_pos,
                                   a_conv_descr      *conversion)
/*
Check whether or not the source operand can be converted to the
destination type, implicitly in an initialization.  If so, set
*conversion to describe the conversion, and return TRUE.  If not, issue
the error incompatible_err at the position err_pos, change the operand
to an error operand, and return FALSE.  The result of the conversion
must be an lvalue if need_lvalue_result is TRUE.  If
is_copy_initialization is TRUE, this is copy-initialization
("="-form); otherwise, it's direct-initialization ("()"-form).  If
is_reference_binding is TRUE, the result will be bound to a reference,
so also consider conversions to derived classes of dest_type.  See
3.3.16.1 in the ANSI C standard and 12.3 in the ARM.  Note that this
routine should only be called when the conversion must be done, not
when we're just wondering if it can be done, because it does operand
transformations on source_operand and issues errors.  The destination
type must not be a reference type (the caller should have rewritten
that case).  orig_dest_type is the original destination type (not
rewritten) for use in error messages.
*/
{
  a_boolean          okay = FALSE, failed = FALSE, ambiguous;
  a_type_ptr         source_type;
  a_type_ptr         unqual_dest_type = skip_typerefs(dest_type);
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
                                       is_copy_initialization,
                                       is_reference_binding,
                                       conversion, (a_conv_descr *)NULL,
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
                                                           &source_constant);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (is_indefinite_function_operand(source_operand)) {
      /* The source is an indefinite function, i.e., the address of an
         overloaded function.  It can be converted to an appropriate
         pointer or pointer-to-member type (WP [over.over], ARM 13.3). */
      a_boolean unknown_dependent_function;

      if (find_addr_of_overloaded_function_match(
                                           source_operand->variant.symbol,
                                           (a_boolean)source_operand->
                                                           is_template_id,
                                           source_operand->template_arg_list,
                                           dest_type,
                                           /*is_cast=*/FALSE,
                                           &match_level,
                                           &conversion->std,
                                           &unknown_dependent_function,
                                           &ambiguous) != NULL) {
        okay = TRUE;
        if (conversion->std.exception_spec_incompatibility) {
          /* In assignments and initializations, exception specifications
             under pointers-to-functions and pointers-to-member-functions
             must obey certain rules, but they don't in this case. */
          pos_error(ec_incompatible_exception_specs, err_pos);
        }  /* if */
      } else if (unknown_dependent_function) {
        okay = TRUE;
        conversion->unknown_dependent_conversion = TRUE;
      } else if (ambiguous) {
        /* More than one function matches. */
        pos_sy_error(ec_ambiguous_ptr_to_overloaded_function, err_pos,
                     source_operand->variant.symbol);
        conv_to_error_operand(source_operand);
      } else {
        /* No match. */
        if (!is_error_type(dest_type)) {
          pos_sy_error(ec_no_match_for_addr_of_overloaded_function, err_pos,
                       source_operand->variant.symbol);
        }  /* if */
        conv_to_error_operand(source_operand);
      }  /* if */
    } else if (C_dialect != C_dialect_cplusplus &&
               is_class_struct_union_type(dest_type) &&
               types_are_compatible(source_type, unqual_dest_type)) {
      /* In C, a struct or union is compatible with the same struct or union.
         Type qualifiers on the destination are ignored because they
         can be added on the conversion. */
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
      if (std_conv.exception_spec_incompatibility) {
        /* In assignments and initializations, exception specifications
           under pointers-to-functions and pointers-to-member-functions
           must obey certain rules, but they don't in this case. */
        pos_error(ec_incompatible_exception_specs, err_pos);
      }  /* if */
      /* Warn on oddball conversions. */
      if (std_conv.warning_suggested != ec_no_error) {
        /* The "opt_ty2" routine puts in the types if the specific error
           message has fill-ins for them, and otherwise ignores the types. */
        pos_opt_ty2_warning(std_conv.warning_suggested, err_pos,
                            source_type, orig_dest_type);
        conversion->std.warning_suggested = ec_no_error;
      }  /* if */
    } else {
      /* The conversion is not legal. */
      /* The "opt_ty2" routine puts in the types if the specific error
         message has fill-ins for them, and otherwise ignores the types. */
      pos_opt_ty2_error(incompatible_err, err_pos,
                        source_type, orig_dest_type);
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
                                            a_type_ptr dest_type,
                                            a_boolean  conv_to_rvalue)
/*
source_operand is to be copied bitwise to an entity of type dest_type.
Both have class types.  Adjust source_operand if necessary, specifically
for the case where the source type is a derived class of dest_type.
This is used for initialization.  See ARM 8.4.1 (aggregate initialization).
This routine does not do the bitwise copy; it just prepares the operand
for it.  Note also that this routine is called for the identity case
where the class type is already correct and nothing should be done to it.
conv_to_rvalue is TRUE if the result should be forced to be an rvalue.
*/
{
  a_type_ptr       source_type = source_operand->type;
  a_base_class_ptr bcp;

  if (C_mode()) {
    /* In C mode, the types will always be the same, ignoring cv-qualifiers.
       We don't want to adjust the cv-qualifiers of the operand to match
       the destination type, since rvalues don't have cv-qualifiers in C. */
  } else if (identical_types(source_type, dest_type)) {
    /* The source and destination types are the same type, so no conversion
       is necessary. */
  } else {
    /* Adjust the class object type. */
    if (types_are_compatible_ignoring_qualifiers(source_type, dest_type)) {
      bcp = NULL;
    } else {
      bcp = find_base_class_of(source_type, dest_type);
#if CHECKING
      if (bcp == NULL) {
        internal_error(
                      "prep_class_bitwise_copy_operand: base class not found");
      }  /* if */
#endif /* CHECKING */
    }  /* if */
    adjust_class_object_type(source_operand, dest_type, bcp);
  }  /* if */
  if (conv_to_rvalue) {
    /* Make the source an rvalue. */
    do_operand_transformations(source_operand, TOPT_NO_OPTIONS);
  }  /* if */
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
      pos_warning(ec_const_function_anachronism, &operand->position);
      /* prep_special_selector_operand (call below) will drop the const. */
    }  /* if */
  }  /* if */
  /* Convert the operand to the proper type to be an argument of the
     conversion function. */
  prep_special_selector_operand(operand, routine_type);
  /* Make an expression for the argument. */
  *arg_expr_list = make_node_from_operand(operand);
}  /* set_up_for_conversion_function_call */


static void set_up_for_constructor_call(an_operand       *operand,
                                        a_routine_ptr    ctor_routine,
                                        a_conv_descr     *ctor_arg_conversion,
                                        an_expr_node_ptr *arg_expr_list)
/*
Prepare for generating a call of a one-argument constructor (i.e.,
a copy constructor or a constructor used as a conversion function),
but do not actually create the call.  *operand is the argument for
the call.  Check accessibility of the routine and adjust the
operand type if necessary so that it will be appropriate for the call.
If ctor_arg_conversion is non-NULL, it points to the conversion to be
used for the constructor argument.  Return an argument list for the
call in *arg_expr_list.  This routine is used only in C++ mode.
*/
{
  a_symbol_ptr     ctor_symbol;
  a_type_ptr       routine_type;
  a_param_type_ptr param_list;
  a_routine_type_supplement_ptr
                   rtsp;

  /* Check that the constructor is accessible and mark it as referenced. */
  ctor_symbol = (a_symbol_ptr)(ctor_routine->source_corresp.assoc_info);
  expr_reference_to_implicitly_invoked_function(ctor_symbol,
                                                &operand->position,
                                                ctor_routine->source_corresp.
                                                             parent.class_type,
                                                /*honor_virtual=*/FALSE);
  routine_type = skip_typerefs(ctor_routine->type);
  /* Convert the operand to the proper type to be an argument of the
     constructor. */
  rtsp = routine_type->variant.routine.extra_info;
  param_list = rtsp->param_type_list;
#if CHECKING
  if (param_list == NULL && !rtsp->has_ellipsis) {
    internal_error("set_up_for_constructor_call: no first parameter");
  }  /* if */
#endif  /* CHECKING */
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
}  /* set_up_for_constructor_call */


void make_constructor_dynamic_init(a_routine_ptr     ctor_routine,
                                   an_expr_node_ptr  arg_expr_list,
                                   a_type_ptr        temp_type,
                                   a_boolean         result_is_addr,
                                   a_boolean         is_explicit_cast,
                                   a_source_position *position,
                                   an_operand        *result)
/*
Create an enk_temp_init node that calls the constructor ctor_routine with
the argument list arg_expr_list.  Return an operand for the value (if
result_is_addr == FALSE) or address (if result_is_addr == TRUE) of the
temporary in *result.  The argument list has already been prepared for
the call (default arguments have been added, the argument types have
been adjusted, etc.).  temp_type is the type of the temporary; its
cv-unqualified version must be the class of which the constructor is
a member.  If it is NULL, the class type is used.  ctor_routine can
be NULL to indicate that the constructor is unknown because one or
more of the arguments is template-dependent in a prototype instantiation.
temp_type must be non-NULL in that case.  is_explicit_cast is TRUE if
this node represents an explicit cast.  *position gives the source
position.
*/
{
  a_dynamic_init_ptr dip;
  an_expr_node_ptr   temp_init_node;

  if (ctor_routine == NULL) {
    check_assertion(temp_type != NULL);
  } else {
    a_type_ptr class_type;
    check_assertion_str(ctor_routine->special_kind ==
                                      (a_special_function_kind)sfk_constructor,
                     "make_constructor_dynamic_init: routine not constructor");
    class_type = ctor_routine->source_corresp.parent.class_type;
    if (temp_type == NULL) {
      temp_type = class_type;
    } else {
      check_assertion_str(types_are_compatible_ignoring_qualifiers(class_type,
                                                                   temp_type),
                          "make_constructor_dynamic_init: bad temp_type");
    }  /* if */
  }  /* if */
  /* Create the dynamic initialization entry and the enk_temp_init node. */
  temp_init_node = create_expr_temporary(temp_type,
                                         result_is_addr,
                                         is_explicit_cast,
                                         /*suppress_abstract_test=*/FALSE,
                                         position);
  dip = temp_init_node->variant.init.dynamic_init;
  /* Use a dik_constructor to call the constructor routine. */
  set_dynamic_init_kind(dip, (a_dynamic_init_kind)dik_constructor);
  dip->variant.constructor.ptr = ctor_routine;
  dip->variant.constructor.args = arg_expr_list;
  /* Make an operand for the overall expression. */
  make_expression_operand(temp_init_node, temp_init_node->type, result);
}  /* make_constructor_dynamic_init */


static void temp_init_by_bitwise_copy_from_operand(an_operand *operand,
                                                   a_boolean  result_is_addr,
                                                   a_boolean  is_explicit_cast)
/*
Create a temporary and initialize it by bitwise copy from the given operand.
Create an enk_temp_init node for the initialization, and update *operand
to refer to that node.  The result is the address of the temporary if
result_is_addr is TRUE.  is_explicit_cast is TRUE if this node represents
an explicit cast.
*/
{
  a_dynamic_init_ptr dip;
  an_expr_node_ptr   temp_init_node;

  /* Allocate the dynamic initialization entry and the enk_temp_init node. */
  temp_init_node = create_expr_temporary(operand->type,
                                         result_is_addr,
                                         is_explicit_cast,
                                         /*suppress_abstract_test=*/FALSE,
                                         &operand->position);
  dip = temp_init_node->variant.init.dynamic_init;
  conv_lvalue_to_rvalue(operand);
  set_dynamic_init_kind(dip, (a_dynamic_init_kind)dik_expression);
  dip->variant.expression = make_node_from_operand(operand);
  /* Make an operand for the overall expression. */
  make_expression_operand(temp_init_node, temp_init_node->type, operand);
}  /* temp_init_by_bitwise_copy_from_operand */


void user_convert_operand(an_operand   *operand,
                          a_type_ptr   dest_type,
                          a_conv_descr *conversion,
                          a_conv_descr *ctor_arg_conversion,
                          a_boolean    force_temp_for_class_bitwise_copy,
                          a_boolean    is_explicit_cast)
/*
Do the user-defined conversion indicated by *conversion to convert
*operand to dest_type.  dest_type may be NULL to indicate that
no additional conversion is needed after the conversion function is called.
If ctor_arg_conversion is non-NULL, it describes the conversion to be done
on the argument of the user-defined conversion, which in that case will
be a constructor call.  Note that this routine converts the operand to
a destination type, but does not copy it anywhere; that's up to the caller.
That's particularly significant when the "conversion" is a class bitwise
copy: the adjustment here changes the operand to access the same class
object with the new type, but does not copy it to a temporary.  However,
if force_temp_for_class_bitwise_copy is TRUE, a temporary will be created
in that case.  is_explicit_cast is TRUE if this conversion is due to
an explicit cast.
*/
{
  an_expr_node_ptr  rout_node, arg_expr_list;
  an_operand        orig_operand;
  a_routine_ptr     conversion_routine;

  orig_operand = *operand;
  conversion_routine = conversion->routine;
#if CHECKING
  if (conversion->unusable) {
    /* The conversion was unusable.  That should have been figured out
       again and shouldn't get here. */
    internal_error("user_convert_operand: unusable conversion");
  }  /* if */
#endif /* CHECKING */
  if (conversion->class_identity_or_bitwise_copy) {
    /* Bitwise copy of a class. */
    a_boolean conv_to_rvalue = !conversion->result_is_an_lvalue;
    prep_class_bitwise_copy_operand(operand, dest_type, conv_to_rvalue);
    if (force_temp_for_class_bitwise_copy) {
      /* Make a copy of the class object in a temporary. */
      check_assertion(conv_to_rvalue);
      temp_init_by_bitwise_copy_from_operand(operand,
                                             /*result_is_addr=*/FALSE,
                                             is_explicit_cast);
    }  /* if */
  } else if (conversion->unknown_dependent_conversion) {
    /* Conversion from or to a template-dependent type in a prototype
       instantiation.  Render as a cast. */
    if (dest_type == NULL) dest_type = type_of_unknown_templ_param_nontype;
    generic_cast_operand(operand, dest_type,
                         (an_expr_operator_kind)eok_cast,
                         !is_explicit_cast, /*is_reference_cast=*/FALSE);
  } else if (conversion_routine->special_kind ==
                                     (a_special_function_kind)sfk_conversion) {
    /* Conversion function. */
    set_up_for_conversion_function_call(operand, conversion_routine,
                                        conversion->routine_symbol,
                                        &arg_expr_list);
    /* Conversion routines are called directly. */
    /* Make a node for the address of the function. */
    rout_node = function_addr_expr(conversion_routine,
                                   /*set_address_taken_flag=*/FALSE);
    rout_node->next = arg_expr_list;
    /* Make an operand for the call. */
    make_function_call(rout_node, conversion_routine->type,
                       (a_boolean)conversion_routine->is_virtual,
                       /*virtual_suppressed=*/FALSE,
                       /*compiler_generated=*/!is_explicit_cast,
                       /*is_conversion=*/TRUE,
                       &orig_operand.position, operand);
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
        cast_operand(dest_type, operand, /*check_cast_access=*/TRUE,
                     /*is_implicit_cast=*/TRUE,
                     /*is_reinterpret_cast=*/FALSE,
                     /*reinterpret_semantics=*/FALSE);
      }  /* if */
    }  /* if */
  } else {
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
                                ctor_arg_conversion, &arg_expr_list);
    make_constructor_dynamic_init(conversion_routine, arg_expr_list,
                                  dest_type, /*result_is_addr=*/FALSE,
                                  is_explicit_cast,
                                  &orig_operand.position, operand);
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
                         /*force_temp_for_class_bitwise_copy=*/FALSE,
                         /*is_explicit_cast=*/FALSE);
  } else {
    /* Cast the operand to the result type. */
    cast_operand(dest_type, source_operand, /*check_cast_access=*/TRUE,
                 /*is_implicit_cast=*/TRUE, /*is_reinterpret_cast=*/FALSE,
                 /*reinterpret_semantics=*/FALSE);
  }  /* if */
}  /* convert_operand */


static a_boolean conversion_usable_or_possible(
                                   an_operand        *source_operand,
                                   a_type_ptr        dest_type,
                                   a_type_ptr        orig_dest_type,
                                   a_boolean         need_lvalue_result,
                                   a_boolean         is_copy_initialization,
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
lvalue if need_lvalue_result is TRUE.  If is_copy_initialization is
TRUE, this is copy-initialization ("="-form); otherwise, it's
direct-initialization ("()"-form).  If is_reference_binding is TRUE,
the result will be bound to a reference, so also consider conversions
to derived classes of dest_type.  orig_dest_type is the destination
type before any rewriting, for use in error messages.
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
    possible = conversion_possible(source_operand, dest_type, orig_dest_type,
                                   need_lvalue_result,
                                   is_copy_initialization,
                                   is_reference_binding,
                                   incompatible_err, err_pos,
                                   *p_conversion);
  }  /* if */
  return possible;
}  /* conversion_usable_or_possible */


static void prep_conversion_operand(an_operand        *source_operand,
                                    a_type_ptr        dest_type,
                                    a_conv_descr      *conversion,
                                    a_boolean         is_copy_initialization,
                                    a_boolean         nontype_template_arg,
                                    an_error_code     incompatible_err,
                                    a_source_position *err_pos)
/*
Convert source_operand to dest_type if that is possible.  If not, issue
incompatible_err at *err_pos.  If is_copy_initialization is TRUE, this
is copy-initialization ("="-form); otherwise, it's direct-initialization
("()"-form).  If nontype_template_arg is TRUE, this is a nontype template
argument.  source_operand may be an rvalue or an lvalue.  On return, it
will always be an rvalue.  If conversion is non-NULL, the conversion has
previously been found to be acceptable, and *conversion describes it.
dest_type must not be a reference type.
*/
{
  a_conv_descr local_conversion;

#if CHECKING
  if (is_reference_type(dest_type)) {
    internal_error("prep_conversion_operand: dest_type is reference");
  }  /* if */
#endif /* CHECKING */
  /* See if the conversion is possible. */
  if (conversion_usable_or_possible(source_operand, dest_type, dest_type,
                                    /*need_lvalue_result=*/FALSE,
                                    is_copy_initialization,
                                    /*is_reference_binding=*/FALSE,
                                    incompatible_err, err_pos,
                                    &conversion,
                                    &local_conversion)) {
    /* Some conversions are not allowed on a nontype template argument. */
    if (nontype_template_arg &&
        !conversion_allowed_for_nontype_template_argument(&conversion->std)) {
      pos_ty2_diagnostic(es_discretionary_error, incompatible_err, err_pos,
                         source_operand->type, dest_type);
    }  /* if */
    /* The types are compatible.  Do the conversion. */
    /* Force the result to be an rvalue. */
    conversion->result_is_an_lvalue = FALSE;
    convert_operand(source_operand, dest_type, conversion);
  }  /* if */
}  /* prep_conversion_operand */


static void check_access_to_elided_copy_constructor(
                                                 a_type_ptr        source_type,
                                                 a_source_position *err_pos)
/*
A conversion from source_type (a possibly-qualified class type) is being done
by eliding a copy constructor.  Check that the copy constructor that would
have been referenced exists and is accessible (ARM 12.6.1) and callable
(we assume that the thing being copied is an rvalue because it's the result
of a constructor call).  Issue an error (or a warning, depending on the
mode) at *err_pos if not.
*/
{
  a_type_ptr   class_type = skip_typerefs(source_type);
  a_symbol_ptr cctor_sym;
  a_boolean    ambiguous, uncallable;
  a_boolean    class_bitwise_copy;

  /* The diagnostics here are issued only in strict mode. */
  /* Avoid problems when the source is an error. */
  if (strict_ansi_mode && !is_error_type(source_type)) {
    cctor_sym = select_overloaded_copy_constructor(
                                      class_type,
                                      get_type_qualifiers(source_type),
                                      /*source_is_rvalue=*/TRUE,
                                      err_pos,
                                      &ambiguous, &uncallable,
                                      &class_bitwise_copy);
    if (class_bitwise_copy) {
      /* A bitwise copy is allowed, so the "copy constructor" is accessible. */
    } else if (ambiguous) {
      /* More than one applicable copy constructor. */
      pos_ty_diagnostic(strict_ansi_discretionary_severity,
                        ec_ambiguous_copy_constructor, err_pos, class_type);
    } else if (uncallable) {
      /* The copy constructor that might have been used is uncallable,
         e.g., because its input parameter cannot be bound to an rvalue. */
      pos_sy_diagnostic(strict_ansi_discretionary_severity,
                        ec_uncallable_elided_cctor,
                        err_pos, cctor_sym);
    } else if (cctor_sym == NULL) {
      /* No applicable copy constructor. */
      pos_ty_diagnostic(strict_ansi_discretionary_severity,
                        ec_no_suitable_copy_constructor, err_pos, class_type);
    } else if (!have_access_to_symbol(cctor_sym)) {
      /* The copy constructor is inaccessible. */
      pos_sy_diagnostic(strict_ansi_discretionary_severity,
                        ec_inaccessible_elided_cctor, err_pos, cctor_sym);
    }  /* if */
  }  /* if */
}  /* check_access_to_elided_copy_constructor */


static a_boolean operand_is_temp_init(an_operand *operand)
/*
Return TRUE if the given operand is an expression operand for an enk_temp_init
(which represents an expression temporary).  Whether the enk_temp_init
returns the value or address of the temporary is immaterial.
*/
{
  a_boolean is_temp_init = FALSE;

  if (is_expression_operand(operand)) {
    an_expr_node_ptr node = operand->variant.expression;
    if (node->kind == (an_expr_node_kind)enk_temp_init) {
      /* The operand is an enk_temp_init for the value of a temporary. */
      is_temp_init = TRUE;
    }  /* if */
  }  /* if */
  return is_temp_init;
}  /* operand_is_temp_init */


static a_boolean is_temp_init_usable_in_optimization(
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
    temp_init_node = source_operand->variant.expression;
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
      temp_init_node->variant.init.static_temp = FALSE;
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
                                          a_boolean          fill_in_dtor,
                                          a_dynamic_init_ptr *p_dip,
                                          an_expr_node_ptr   *p_temp_init_node)
/*
An entity of type dest_type (a class type) is being initialized from
source_operand.  The constructor or conversion function required to do the
copy and/or conversion is given by *conversion.  Create a dynamic
initialization entry to do the initialization (and any required
destruction, if fill_in_dtor is TRUE) and return a pointer to
it in *dip (or return *dip == NULL for an error).  If
p_temp_init_node is non-NULL, create an enk_temp_init node (for the
address of a temporary) pointing to that dynamic initialization entry,
and return a pointer to it in *p_temp_init_node.  An error node is
returned for an error.  dest_type is allowed to be a class having
no constructors at all.  The initialization represented is an "="
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
  a_type_ptr         class_type = skip_typerefs(dest_type);
  a_type_ptr         elision_source_type;

  temp_init_node = NULL;
  conversion_routine = conversion->routine;
  class_bitwise_copy = conversion->class_identity_or_bitwise_copy;
  if (class_bitwise_copy) {
    /* The operation is a class bitwise copy. */
    if (skip_typerefs(source_operand->type) == class_type && !C_mode()) {
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
      a_type_qualifier_set qualifiers;
      if (skip_typerefs(source_operand->type) == class_type &&
          is_copy_constructor(conversion_routine, class_type, &qualifiers,
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
                           /*force_temp_for_class_bitwise_copy=*/FALSE,
                           /*is_explicit_cast=*/FALSE);
      /* See if the result of the conversion is already in a temporary
         of the right type. */
      if (identical_types(source_operand->type, dest_type) &&
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
          conversion_routine = select_copy_constructor(
                                skip_typerefs(source_operand->type),
                                get_type_qualifiers(source_operand->type),
                                &source_operand->position, class_type,
                                &class_bitwise_copy,
                                curr_expr_is_potentially_evaluated());
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (elision_done) {
    /* Copy constructor elision is being done.  Check access to the elided
       copy constructor. */
    check_access_to_elided_copy_constructor(elision_source_type,
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
    prep_class_bitwise_copy_operand(source_operand, dest_type,
                                    /*conv_to_rvalue=*/TRUE);
    dip = alloc_dynamic_init_possibly_with_dtor(
                                          (a_dynamic_init_kind)dik_expression,
                                          fill_in_dtor,
                                          class_type,
                                          &source_operand->position);
    dip->variant.expression = make_node_from_operand(source_operand);
  } else if (conversion->unknown_dependent_conversion) {
    /* Conversion to or from an unknown template-dependent type in a
       prototype instantiation. */
    prep_generic_operand(source_operand, /*lvalue_expected=*/FALSE);
    /* Set the dynamic init entry to represent "constructor" initialization,
       leaving the constructor pointer NULL. */
    dip = alloc_expr_dynamic_init((a_dynamic_init_kind)dik_constructor);
    dip->variant.constructor.ptr = NULL;
    dip->variant.constructor.args = make_node_from_operand(source_operand);
  } else if (conversion_routine != NULL) {
    /* conversion_routine is a constructor (copy or other). */
    set_up_for_constructor_call(source_operand, conversion_routine,
                                (a_conv_descr *)NULL, &arg_expr_list);
    /* Use a dik_constructor entry to call the constructor. */
    dip = alloc_dynamic_init_possibly_with_dtor(
                                          (a_dynamic_init_kind)dik_constructor,
                                          fill_in_dtor,
                                          class_type,
                                          &source_operand->position);
    dip->variant.constructor.ptr = conversion_routine;
    dip->variant.constructor.args = arg_expr_list;
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
    if (temp_init_node == NULL) {
      if (dip == NULL) {
        /* Some error.  Return an error node. */
        temp_init_node = error_node();
      } else {
        temp_init_node = alloc_temp_init_node(dest_type, dip,
                                              /*result_is_addr=*/TRUE,
                                              /*is_explicit_cast=*/FALSE);
      }  /* if */
    } else {
      /* Existing enk_temp_init; make sure we get the address of the
         temporary instead of its value.  Note that in this case dip
         is the dynamic init pointer extracted from that node. */
      if (!temp_init_node->variant.init.result_is_addr) {
        temp_init_node->variant.init.result_is_addr = TRUE;
        temp_init_node->type = make_pointer_type(temp_init_node->type);
      }  /* if */
      /* Put the dynamic initialization on a destruction list if
         appropriate. */
      set_temp_init_dynamic_init_lifetime(temp_init_node);
    }  /* if */
    *p_temp_init_node = temp_init_node;
  }  /* if */
  *p_dip = dip;
}  /* determine_dynamic_init_for_class_init */


void prep_elision_initializer_operand(an_operand         *source_operand,
                                      a_type_ptr         dest_type,
                                      a_boolean          fill_in_dtor,
                                      an_error_code      err_code,
                                      a_dynamic_init_ptr *dip)
/*
An entity of (class) type dest_type is being initialized from source_operand.
Convert it if necessary (issuing an error if the conversion cannot be done),
and build a dynamic initialization entry to describe the initialization.
The dynamic initialization entry will also indicate a destructor if
appropriate and if fill_in_dtor is TRUE.  Return a pointer to the dynamic
initialization entry in *dip (or NULL for an error).  err_code is the error
code to be used in case of error.  source_operand may be changed by this
routine.  This routine is used in both C and C++ mode, but it exists to
do copy constructor elision in C++ mode.  This is an initialization with
the "=" semantics (copy-initialization).
*/
{
  a_conv_descr conversion;
  an_operand   orig_operand;
  a_boolean    is_copy_initialization = TRUE;

  orig_operand = *source_operand;
  *dip = NULL;
  /* Microsoft VC++ treats copy-initialization as direct-initialization. */
  if (microsoft_mode) is_copy_initialization = FALSE;
  /* Look for a constructor to convert the expression to the required
     class type. */
  if (conversion_possible(source_operand, dest_type, dest_type,
                          /*need_lvalue_result=*/FALSE,
                          is_copy_initialization,
                          /*is_reference_binding=*/FALSE,
                          err_code,
                          &source_operand->position,
                          &conversion)) {
    /* The conversion is possible.  Determine the routine and argument
       list to return to the caller. */
    determine_dynamic_init_for_class_init(source_operand, dest_type,
                                          &conversion,
                                          fill_in_dtor,
                                          dip, (an_expr_node_ptr *)NULL);
  }  /* if */
  /* Restore the original source position, etc. */
  restore_operand_details(source_operand, &orig_operand);
}  /* prep_elision_initializer_operand */


void temp_init_from_operand(an_operand *operand)
/*
Create an enk_temp_init node that initializes a temporary to a copy of
the indicated operand.  The source operand can be an rvalue or an
lvalue.  On return, *operand will have been changed to an rvalue for
the address of the temporary.  Used only in C++ mode.
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
      /* A copy constructor must be used.  An error is issued if an appropriate
         one does not exist or is inaccessible. */
      cctor_routine = select_copy_constructor(
                                unqual_temp_type,
                                get_type_qualifiers(operand->type),
                                &operand->position, unqual_temp_type,
                                &class_bitwise_copy,
                                curr_expr_is_potentially_evaluated());
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
                                    (a_conv_descr *)NULL, &cctor_arg);
        make_constructor_dynamic_init(cctor_routine, cctor_arg, temp_type,
                                      /*result_is_addr=*/TRUE,
                                      /*is_explicit_cast=*/FALSE,
                                      &orig_operand.position,
                                      operand);
      }  /* if */
    }  /* if */
  }  /* if */
  if (!cctor_case) {
    /* Normal case -- use a dik_expression initialization to copy the
       operand into the temporary. */
    temp_init_by_bitwise_copy_from_operand(operand, /*result_is_addr=*/TRUE,
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
Convert source_operand to dest_type, put it into a newly-created temporary,
and return an rvalue for the address of the temporary in source_operand.
The caller will bind a reference to dest_type to the temporary returned, so
there is some latitude in the type of temporary created (i.e., it
could be of a derived class type, or could have fewer cv-qualifiers).
If the conversion is not possible, issue the error incompatible_err,
convert source_operand to an error operand, and return *err TRUE.
orig_dest_type is the destination (reference) type before any rewriting,
for use in error messages.  If conversion is non-NULL, the conversion
is already known to be possible, and *conversion describes it.
dest_type must not be a reference type.  Only used in C++.  This is
copy-initialization.
*/
{
  a_conv_descr local_conversion;
  an_operand   orig_operand;
  a_boolean    have_temp;

  *err = FALSE;
  orig_operand = *source_operand;
#if CHECKING
  if (is_reference_type(dest_type)) {
    internal_error("convert_operand_into_temp: dest_type is reference");
  }  /* if */
#endif /* CHECKING */
  /* See if the conversion is possible. */
  if (conversion_usable_or_possible(source_operand, dest_type, orig_dest_type,
                                    /*need_lvalue_result=*/FALSE,
                                    /*is_copy_initialization=*/TRUE,
                                    /*is_reference_binding=*/FALSE, /* sic */
                                    incompatible_err,
                                    &source_operand->position,
                                    &conversion,
                                    &local_conversion)) {
    /* Yes, the conversion is possible.  Do it. */
    if (conversion->class_object_adjustment_required) {
      /* The result of the conversion function is a class rvalue that can
         be bound to but has a slightly different type than dest_type
         (because of derived --> base issues or cv-qualifier differences).
         Do the conversion, but make the temporary have the type of the
         result of the conversion function rather than dest_type. */
      user_convert_operand(source_operand, /*dest_type=*/(a_type_ptr)NULL,
                           conversion, (a_conv_descr *)NULL,
                           /*force_temp_for_class_bitwise_copy=*/FALSE,
                           /*is_explicit_cast=*/FALSE);
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
    if (have_temp && is_class_struct_union_type(source_operand->type)) {
      /* The result of the conversion is already a class temporary.
         Convert the operand from the value of the temporary to the
         address. */
      conv_class_operand_to_object_pointer(source_operand);
    } else if (have_temp && is_an_lvalue(source_operand)) {
      /* The result of the conversion is already a non-class temporary
         that is an lvalue (in particular, this includes array lvalues).
         Convert to a pointer to the lvalue. */
      /* Note that if a standard conversion is needed after the
         conversion function, source_operand has previously been converted
         to an rvalue. */
      take_address_of_lvalue(source_operand);
    } else {
      /* Initialize a temporary with the converted value. */
      temp_init_from_operand(source_operand);
    }  /* if */
    /* Handle base class casts, cv-qualifier adjustments. */
    cast_operand(make_pointer_type(dest_type), source_operand,
                 /*check_cast_access=*/TRUE, /*is_implicit_cast=*/TRUE,
                 /*is_reinterpret_cast=*/FALSE,
                 /*reinterpret_semantics=*/FALSE);
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
It has already been massaged into the right type, and a temporary has
been generated if necessary.  If the top of the expression is a temporary,
ensure that the temporary will have an appropriate lifetime so it will
last as long as the reference.  If static_lifetime is TRUE, the reference
is static; otherwise, it is automatic.  This is needed for cases like

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
    /* Drop any field selections or implicit casts on top of the expression. */
    while (is_operation_node(node)) {
      an_expr_operator_kind op = node->variant.operation.kind;
      if (((op == (an_expr_operator_kind)eok_cast ||
            op == (an_expr_operator_kind)eok_base_class_cast) &&
           node->variant.operation.compiler_generated) ||
          op == (an_expr_operator_kind)eok_field) {
        node = node->variant.operation.operands;
      } else {
        break;
      }  /* if */
    }  /* while */
    if (node->kind == (an_expr_node_kind)enk_temp_init) {
      if (static_lifetime) node->variant.init.static_temp = TRUE;
      dip = node->variant.init.dynamic_init;
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


a_boolean direct_reference_binding_possible(
                                       an_operand   *source_operand,
                                       a_type_ptr   source_type,
                                       a_type_ptr   dest_type,
                                       a_boolean    *ref_to_const,
                                       a_boolean    *ref_to_const_volatile,
                                       a_boolean    *binding_to_rvalue_allowed,
                                       a_boolean    *dropping_qualifiers,
                                       a_symbol_ptr *function_symbol)
/*
See if it is possible to directly bind a reference of type dest_type
to source_operand.  If so, return TRUE.  source_operand can be NULL, in
which case source_type gives the operand type.  *ref_to_const is returned
TRUE if the reference is to const.  *ref_to_const_volatile is returned
TRUE if the reference is to const volatile.
*binding_to_rvalue_allowed is returned TRUE if the reference can be
bound to an rvalue.  *dropping_qualifiers is returned TRUE if the
reference binding would drop type qualifiers (i.e., the types are such
that the binding could be done except for the qualifiers).
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
  a_boolean  template_case = FALSE;
  a_type_ptr base_dest_type, unqual_dest_type, unqual_source_type;
                                           
  if (function_symbol != NULL) *function_symbol = NULL;                   
  if (source_operand != NULL) source_type = source_operand->type;
  base_dest_type = type_pointed_to(dest_type);
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
    a_std_conv_descr   std_conversion;
    a_boolean          ambiguous, unknown_dependent_function;

    *function_symbol =
        find_addr_of_overloaded_function_match(source_operand->variant.symbol,
                                               (a_boolean)source_operand->
                                                                is_template_id,
                                               source_operand->
                                                             template_arg_list,
                                               dest_type,
                                               /*is_cast=*/FALSE,
                                               &match_level,
                                               &std_conversion,
                                               &unknown_dependent_function,
                                               &ambiguous);
    if (ambiguous) {
      /* More than one function matches. */
      pos_sy_error(ec_ambiguous_ptr_to_overloaded_function,
                   &source_operand->position,
                   source_operand->variant.symbol);
      conv_to_error_operand(source_operand);
    } else if (*function_symbol != NULL || unknown_dependent_function) {
      type_is_correct_or_derived = TRUE;
    }  /* if */
  }  /* if */
  direct_binding_possible = type_is_correct_or_derived;
  /* Determine whether or not the reference is to a const type. */
  *ref_to_const = is_const_qualified_type(base_dest_type);
  *binding_to_rvalue_allowed = *ref_to_const;
  *ref_to_const_volatile = FALSE;
  if (!any_cfront_mode() && *ref_to_const &&
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
  *dropping_qualifiers = type_is_correct_or_derived && !template_case &&
                         any_qualifier_missing(base_dest_type,
                                               source_type);
  if (*dropping_qualifiers) {
    /* There are fewer qualifiers on the destination than on the source,
       so the initialization would involve dropping qualifiers. */
    direct_binding_possible = FALSE;
  }  /* if */
  if (type_is_correct_or_derived && *binding_to_rvalue_allowed &&
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
  return direct_binding_possible;
}  /* direct_reference_binding_possible */


static a_boolean underlying_entity_is_auto(an_expr_node_ptr expr,
                                           a_boolean        *is_temp)
/*
Return TRUE if the underlying entity of the indicated expression tree
(an lvalue address) has auto (or register) storage class.  *is_temp
is set to TRUE if the underlying entity is a temporary.  If the
true answer cannot be determined, the safe answer is FALSE.
*/
{
  a_boolean entity_is_auto = FALSE;

  *is_temp = FALSE;
  if (expr->kind == (an_expr_node_kind)enk_variable_address) {
    if (!has_static_storage_duration(expr->variant.variable->storage_class)) {
      /* Address of non-static variable. */
      entity_is_auto = TRUE;
    }  /* if */
  } else if (expr->kind == (an_expr_node_kind)enk_temp_init) {
    if (expr->variant.init.result_is_addr &&
        !expr->variant.init.static_temp) {
      /* Address of non-static temporary. */
      entity_is_auto = TRUE;
      *is_temp = TRUE;
    }  /* if */
  } else if (is_operation_node(expr)) {
    an_expr_operator_kind op = expr->variant.operation.kind;
    an_expr_node_ptr      operands = expr->variant.operation.operands;
    an_expr_node_ptr      check_operand = NULL;

    if (expr->variant.operation.returns_lvalue_instead_of_usual_rvalue) {
      /* Operations that return an lvalue. */
      if (op == (an_expr_operator_kind)eok_comma) {
        /* Continue with the second operand. */
        check_operand = operands->next;
      } else if (op == (an_expr_operator_kind)eok_question) {
        /* Check both the second and third operands. */
        entity_is_auto = underlying_entity_is_auto(operands->next, is_temp);
        if (!entity_is_auto) {
          entity_is_auto = underlying_entity_is_auto(operands->next->next,
                                                     is_temp);
        }  /* if */
      } else {
        /* Others, e.g., pre-increment, assignment.  Continue with the
           first operand. */
        check_operand = operands;
      }  /* if */
    } else if (((op == (an_expr_operator_kind)eok_cast ||
                 op == (an_expr_operator_kind)eok_base_class_cast) &&
                 expr->variant.operation.compiler_generated) ||
               op == (an_expr_operator_kind)eok_field) {
      /* Implicit cast or field selection.  Continue with first operand. */
      check_operand = operands;
    }  /* if */
    if (check_operand != NULL) {
      entity_is_auto = underlying_entity_is_auto(check_operand, is_temp);
    }  /* if */
  }  /* if */
  return entity_is_auto;
}  /* underlying_entity_is_auto */


static void check_for_returning_reference_to_local_entity(an_operand *operand)
/*
operand is the address being bound to a reference in a return statement.
Issue a warning if it is a local entity.
*/
{
  a_boolean is_temp;

  if (is_expression_operand(operand)) {
    if (underlying_entity_is_auto(operand->variant.expression, &is_temp)) {
      if (is_temp) {
        /* Returning a reference to a temporary. */
        pos_warning(ec_return_ref_init_requires_temp, &operand->position);
      } else {
        /* Returning a reference to a local variable. */
        pos_warning(ec_returning_ref_to_local_variable, &operand->position);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* check_for_returning_reference_to_local_entity */


static void prep_reference_initializer_operand(
                              an_operand    *source_operand,
                              a_type_ptr    dest_type,
                              a_conv_descr  *conversion,
                              a_boolean     initializing_return_value,
                              a_boolean     initializing_variable,
                              a_boolean     static_lifetime,
                              a_boolean     bitwise_assignment_param,
                              an_error_code incompatible_err)
/*
A reference of type dest_type is being initialized from the indicated
source_operand.  Check that the operand has the right type (and other
attributes), converting it if necessary.  On return, *source_operand
will contain an rvalue pointer to the object to which the reference
should be bound.  initializing_return_value is TRUE if the initialization
is being done to return a value in a return statement.
initializing_variable is TRUE if this initialization is for a variable.
In that case, static_lifetime is TRUE if the variable is static.
If bitwise_assignment_param is TRUE, this call is analyzing the parameter
of a notional generated copy assignment operator.  If the operand and
type are incompatible, the error incompatible_err is issued.
If conversion is non-NULL, the initializer has previously been found
to be acceptable, and *conversion describes it.
*/
{
  a_type_ptr   orig_dest_type = dest_type, result_ptr_type;
  a_type_ptr   orig_source_type = source_operand->type;
  an_operand   orig_operand;
  a_type_ptr   base_dest_type;
  a_boolean    err = FALSE, dropping_qualifiers, ambiguous;
  a_boolean    direct_binding_possible, binding_to_rvalue_allowed;
  a_boolean    direct_binding_conversion_possible = FALSE;
  a_boolean    ref_to_const, ref_to_const_volatile, operand_was_rvalue;
  a_boolean    warn = FALSE, template_case = FALSE;
  a_conv_descr conv_for_direct_binding;
  a_candidate_function_ptr
               ambiguity_list = NULL;
  a_symbol_ptr function_symbol;

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
                                                    &ref_to_const,
                                                    &ref_to_const_volatile,
                                                    &binding_to_rvalue_allowed,
                                                    &dropping_qualifiers,
                                                    &function_symbol);
    if (!direct_binding_possible && !curr_expr_kind_is_const() &&
        is_class_struct_union_type(source_operand->type)) {
      /* It might be possible to convert the source operand to an lvalue
         via a conversion function, and then bind the reference directly to
         the result. */
      if (conversion_for_direct_reference_binding_possible(
                                                      source_operand,
                                                      dest_type,
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
  /* Use a pointer type instead of a reference type on the destination. */
  if (!bitwise_assignment_param) {
    result_ptr_type = make_pointer_type(base_dest_type);
  } else {
    /* For the bitwise assignment case, the reference type is
       reference-to-const, but it's fabricated.  There's no real need to
       add "const" in the cases where the original object is bound to,
       so use the dest type with the original object cv-qualifiers. */
    if (skip_typerefs(base_dest_type) == skip_typerefs(orig_source_type)) {
      /* Preserve a typedef from the original source type. */
      result_ptr_type = make_pointer_type(orig_source_type);
    } else {
      result_ptr_type = make_pointer_type(
                            make_identically_qualified_type(base_dest_type,
                                                            orig_source_type));
    }  /* if */
  }  /* if */
  operand_was_rvalue = is_an_rvalue(source_operand);
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
      if (is_an_lvalue(source_operand)) {
        take_address_of_lvalue(source_operand);
      } else if (is_a_function_designator(source_operand)) {
        conv_function_designator_to_ptr_to_function(source_operand,
                                                    /*allow_ctor=*/FALSE);
      } else {
        /* Binding a reference to an rvalue in a constant expression. */
        if (!is_error_operand(source_operand)) {
          error_in_operand(ec_expr_not_an_lvalue_or_function_designator,
                           source_operand);
        }  /* if */
      }  /* if */
    }  /* if */
    generic_cast_operand(source_operand, result_ptr_type,
                         (an_expr_operator_kind)eok_cast,
                         /*is_implicit_cast=*/TRUE,
                         /*is_reference_cast=*/TRUE);
  } else if (direct_binding_conversion_possible) {
    /* The initial value can be converted to an lvalue of the right type
       through use of a conversion function returning a reference. */
    if (ambiguity_list != NULL) {
      /* The conversion is ambiguous.  Put out an error. */
      pos_ty2_start_error(ec_ambiguous_conversion_function,
                          &source_operand->position, orig_source_type,
                          base_dest_type);
      diagnose_overload_ambiguity(ambiguity_list,
                                  (an_operand *)NULL,
                                  (an_arg_operand_ptr)NULL,
                                  (an_opname_kind)onk_none);
      free_candidate_function_list(ambiguity_list);
      conv_to_error_operand(source_operand);
    } else {
      /* Do the conversion. */
      convert_operand(source_operand, base_dest_type, conversion);
      /* Convert the lvalue to an rvalue pointer to the object. */
      take_address_of_lvalue(source_operand);
    }  /* if */
  } else if (direct_binding_possible && is_an_lvalue(source_operand)) {
    /* The initial value is an lvalue of the right type; the initialization
       can be done directly. */
    /* Convert the lvalue to an rvalue pointer to the object. */
    take_address_of_lvalue(source_operand);
    if (ref_to_const) {
      /* For a reference to const, tone down the reference kinds to
         indicate the address is taken in a way that can't modify the
         entity. */
      change_some_ref_kinds(source_operand->ref_entries_list,
                            SRK_ADDRESS_TAKEN,
                            SRK_ADDRESS_TAKEN | SRK_CONST_ADDRESS_TAKEN);
    }  /* if */
    if (is_constant_operand(source_operand) &&
        constant_bool_value_known_at_compile_time(
                                          &source_operand->variant.constant) &&
        /* "false" means zero, i.e., a null pointer. */
        is_false_constant(&source_operand->variant.constant)) {
      /* Initializing a reference to NULL, which is not allowed:
           int &p = *(int *)0;
      */
      if (!strict_ansi_mode) {
        pos_warning(ec_null_reference, &source_operand->position);
      } else {
        error_in_operand(ec_null_reference, source_operand);
      }  /* if */
    } else if (curr_expr_kind_is(ek_template_arg) &&
               is_class_struct_union_type(base_dest_type) &&
               find_base_class_of(orig_source_type, base_dest_type) != NULL) {
      /* A derived-base binding is not allowed in a nontype template
         argument. */
      pos_ty2_diagnostic(es_discretionary_error, incompatible_err,
                         &source_operand->position, orig_source_type,
                         dest_type);
    }  /* if */
    /* Cast the operand to the result type. */
    cast_operand(result_ptr_type, source_operand, /*check_cast_access=*/TRUE,
                 /*is_implicit_cast=*/TRUE, /*is_reinterpret_cast=*/FALSE,
                 /*reinterpret_semantics=*/FALSE);
  } else if (direct_binding_possible &&
             is_a_function_designator(source_operand)) {
    /* The initial value is a function designator of the right type;
       the initialization can be done directly. */
    if (function_symbol != NULL) {
      /* The initial value is a set of overloaded functions, out of which
         one has been selected. */
      /* Do whatever would have been done with the function if we had
         known all along which function was intended.  Make an operand
         for the specific function's address. */
      a_boolean access_error_reported;

      check_assertion(is_indefinite_function_operand(source_operand));
      overloaded_function_catch_up(function_symbol,
                                   source_operand->variant.symbol,
                                   (a_boolean)
                                             source_operand->is_qualified_name,
                                   &orig_operand.position,
                                   &orig_operand.position,
                                   /*elided_reference=*/FALSE,
                                   /*address_taken=*/TRUE,
                                   source_operand,
                                   &access_error_reported);
    } else {
      /* Normal case (not an indefinite function). */
      if (exceptions_enabled) {
        /* Check compatibility of exception specifications.  Unlike the
           pointer-to-function case, the reference-to-function case must
           match exactly. */
        if (exception_spec_is_less_restrictive(source_operand->type,
                                               base_dest_type) ||
            exception_spec_is_less_restrictive(base_dest_type,
                                               source_operand->type)) {
          pos_diagnostic(es_discretionary_error,
                         ec_incompatible_exception_specs,
                         &source_operand->position);
        }  /* if */
      }  /* if */
      conv_function_designator_to_ptr_to_function(source_operand,
                                                  /*allow_ctor=*/FALSE);
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
    if (!dropping_qualifiers && !any_cfront_mode()) {
      /* [dcl.init.ref] of the WP requires that the copy constructor be
         callable whether or not it is actually called.  We never call
         it, but we must check it anyway. */
      check_access_to_elided_copy_constructor(orig_source_type,
                                              &source_operand->position);
    }  /* if */
    /* Convert the operand to a pointer to the class object. */
    conv_class_operand_to_object_pointer(source_operand);
    cast_operand(result_ptr_type, source_operand, /*check_cast_access=*/TRUE,
                 /*is_implicit_cast=*/TRUE, /*is_reinterpret_cast=*/FALSE,
                 /*reinterpret_semantics=*/FALSE);
    if (dropping_qualifiers) {
      /* Type qualifiers were dropped on this binding. */
      if (bitwise_assignment_param) {
        /* Use a different message for the bitwise operator= case.  The
           normal message is confusing to programmers. */
        pos_ty_error(ec_no_suitable_assignment_operator,
                     &source_operand->position,
                     f_skip_typerefs(base_dest_type));
      } else {
        pos_ty2_error(ec_qualifier_dropped_in_ref_init,
                      &source_operand->position,
                      orig_dest_type, orig_source_type);
      }  /* if */
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
      pos_diagnostic(err_severity,
                     ref_to_const_volatile ?
                                       ec_const_volatile_ref_init_from_rvalue :
                                       ec_nonconst_ref_init_from_rvalue,
                     &source_operand->position);
    }  /* if */
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
      pos_ty2_error(ec_qualifier_dropped_in_ref_init,
                    &source_operand->position,
                    orig_dest_type, orig_source_type);
      conv_to_error_operand(source_operand);
    } else if (!binding_to_rvalue_allowed &&
               !allow_anachronisms && !any_cfront_mode()) {
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
        pos_ty2_error(ref_to_const_volatile ?
                                       ec_bad_const_volatile_ref_init :
                                       ec_bad_nonconst_ref_init,
                      &source_operand->position,
                      orig_dest_type, orig_source_type);
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
               source_operand->variant.expression->variant.
                                 init.dynamic_init->kind ==
                                       (a_dynamic_init_kind)dik_constructor)) {
            /* In cfront mode we allow this also for a ref to non-const if
               we're passing an argument, or if we have a constructed
               temporary in 2.1 mode, or if we're initializing a non-global
               in 3.0 mode. */
            pos_warning(ref_to_const_volatile ?
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
              pos_ty2_error(ref_to_const_volatile ?
                                       ec_bad_const_volatile_ref_init :
                                       ec_bad_nonconst_ref_init,
                            &source_operand->position,
                            orig_dest_type, orig_source_type);
              conv_to_error_operand(source_operand);
            }  /* if */
            err = TRUE;
          }  /* if */
        } else {
          /* Allowed as an anachronism. */
          check_assertion(allow_anachronisms);
          pos_diagnostic(anachronism_error_severity,
                         ref_to_const_volatile ?
                                       ec_const_volatile_ref_init_anachronism :
                                       ec_nonconst_ref_init_anachronism,
                         &source_operand->position);
          if (anachronism_error_severity == es_error) {
            err = TRUE;
          } else {
            warn = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
      if (!err && !warn) {
        /* Let the user know a temp was used. */
        pos_remark(ec_temp_used_for_ref_init, &source_operand->position);
      }  /* if */
    }  /* if */
  }  /* if */
  if (initializing_variable) {
    /* If the top thing in the initializer is a temporary, make sure the
       temporary has a lifetime as long as the reference. */
    adjust_top_temporary_for_binding_to_reference(source_operand,
                                                  static_lifetime);
  } else if (initializing_return_value) {
    /* Check for returning a reference to a local entity. */
    check_for_returning_reference_to_local_entity(source_operand);
  }  /* if */
  /* Restore the original source position, etc. */
  restore_operand_details(source_operand, &orig_operand);
}  /* prep_reference_initializer_operand */


void prep_initializer_operand(an_operand    *source_operand,
                              a_type_ptr    dest_type,
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
*conversion describes it.
*/
{
  /* Microsoft VC++ treats
       return expr;
     and
       A x = expr;
     as direct-initialization.  Argument passing is not affected. */
  if (microsoft_mode &&
      (initializing_return_value || initializing_variable)) {
    is_copy_initialization = FALSE;
  }  /* if */
  if (is_error_operand(source_operand)) {
    /* Previous error.  Leave the operand alone. */
  } else if (is_reference_type(dest_type)) {
    /* Reference initialization. */
    prep_reference_initializer_operand(source_operand, dest_type,
                                       conversion, initializing_return_value,
                                       initializing_variable,
                                       static_lifetime,
                                       /*bitwise_assignment_param=*/FALSE,
                                       incompatible_err);
  } else {
    /* Normal case (not initializing a reference). */
    prep_conversion_operand(source_operand, dest_type, conversion,
                            is_copy_initialization,
                            nontype_template_arg,
                            incompatible_err,
                            &source_operand->position);
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
                                    param_type,
                                    /*need_lvalue_result=*/FALSE,
                                    /*is_copy_initialization=*/TRUE,
                                    /*is_reference_binding=*/FALSE,
                                    err_code, &source_operand->position,
                                    &conversion,
                                    &local_conversion)) {
    an_operand orig_operand;
    orig_operand = *source_operand;
    /* Yes.  Build an enk_temp_init node and a dynamic init entry that
       will initialize the temporary.  The temporary's address is passed
       to the called routine. */
    determine_dynamic_init_for_class_init(source_operand, param_type,
                                          conversion,
                                          /*fill_in_dtor=*/TRUE,
                                          &dip, &temp_init_node);
    make_expression_operand(temp_init_node, temp_init_node->type,
                            source_operand);
    restore_operand_details(source_operand, &orig_operand);
  }  /* if */
}  /* prep_arg_passed_via_copy_constructor */


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

  /* If the parameter is a template class, make sure it is instantiated so
     we know if a copy constructor should be used. */
  complete_type_is_needed(param_type);
  if (formal_param->passed_via_copy_constructor) {
    /* Argument is initialized by a copy constructor. */
    prep_arg_passed_via_copy_constructor(source_operand, param_type,
                                         conversion, err_code);
  } else {
    /* Normal argument. */
    if (microsoft_bugs && conversion != NULL &&
        is_reference_type(param_type) && is_an_rvalue(source_operand)) {
      /* In Microsoft bugs mode, a reference to non-const is sometimes
         allowed to bind to an rvalue.  If that's been done (which we
         know because conversion != NULL means that we've made it through
         overload resolution), change the reference type to reference
         to const so that the binding is valid. */
      a_type_ptr underlying_type = type_pointed_to(param_type);
      if (!is_const_qualified_type(underlying_type)) {
        underlying_type = make_qualified_type(underlying_type, TQ_CONST);
        param_type = make_reference_type(underlying_type);
      }  /* if */
    }  /* if */
    prep_initializer_operand(source_operand, param_type,
                             conversion,
                             /*initializing_return_value=*/FALSE,
                             /*initializing_variable=*/FALSE,
                             /*static_lifetime=*/FALSE,
                             /*is_copy_initialization=*/TRUE,
                             /*nontype_template_arg=*/FALSE,
                             err_code);
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
    check_assertion_str(symbol_supplement_for_class(class_type)->
                                            assignment_by_bitwise_copy_allowed,
                        "prep_assignment_operand: class not bitwise copyable");
    if ((strict_ansi_mode || microsoft_mode) && is_qualified_type(dest_type)) {
      /* The bitwise copy is defined in terms of a notional generated copy
         assignment operator which is not cv-qualified and therefore cannot
         assign into a cv-qualified left operand. */
      pos_ty_error(ec_no_suitable_assignment_operator,
                   err_pos, class_type);
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
                                         incompatible_err);
      /* Turn the pointer produced for the reference binding back into an
         rvalue for a class object. */
      conv_object_pointer_to_lvalue(source_operand);
      conv_lvalue_to_rvalue(source_operand);    
    }  /* if */
  } else {
    /* Nonclass assignment, and C mode struct assignment. */
    /* See if the source and destination types are compatible, and convert the
       source operand to the destination type. */
    prep_conversion_operand(source_operand, dest_type,
                            (a_conv_descr_ptr)NULL,
                            /*is_copy_initialization=*/TRUE,
                            /*nontype_template_arg=*/FALSE,
                            incompatible_err, err_pos);
  }  /* if */
}  /* prep_assignment_operand */


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
  if (is_an_lvalue(op2)) {
    a_boolean ref_to_const, ref_to_const_volatile;
    a_boolean binding_to_rvalue_allowed, dropping_qualifiers;
    /* op2 is an lvalue.  Attempt to convert op1 to an lvalue of the type
       of op2.  The standard defines this in terms of a notional
       conversion to "reference to op2_type". */
    conv_dest_type = make_reference_type(op2_type);
    if (direct_reference_binding_possible(op1,
                                          (a_type_ptr)NULL,
                                          conv_dest_type,
                                          &ref_to_const,
                                          &ref_to_const_volatile,
                                          &binding_to_rvalue_allowed,
                                          &dropping_qualifiers,
                                          (a_symbol **)NULL)) {
      possible = TRUE;
      conv->class_identity_or_bitwise_copy = TRUE;
      conv->result_is_an_lvalue = TRUE;
    } else if (!curr_expr_kind_is_const() &&
               is_class_struct_union_type(op1_type)) {
      /* It might be possible to convert the source operand to an lvalue
         via a conversion function, and then bind the reference directly to
         the result. */
      if (conversion_for_direct_reference_binding_possible(
                                           op1,
                                           conv_dest_type,
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
        if (any_qualifier_missing(op2_type, op1_type)) possible = FALSE;
        /* Check for an ambiguous base class. */
        if (bcp != NULL && bcp->ambiguous) {
          conv->unusable = TRUE;
          local_ambiguous = TRUE;
          if (ambiguous == NULL) {
            /* Issue the ambiguity error.  Note that the base class is
               always the underlying type of op2, never of op1, in this
               case. */
            pos_ty_error(ec_ambiguous_base_class, &op1->position,
                         skip_typerefs(op2->type));
            error_issued = TRUE;
          }  /* if */
        }  /* if */
        /* Set *conv to indicate the related-class conversion. */
        conv->std.cast_base_class = bcp;
        conv->std.nontrivial_conversion = (bcp != NULL);
        conv->class_identity_or_bitwise_copy = TRUE;
      }  /* if */
    } else {
      /* Not related classes.  See whether op1 can be converted to the
         type of op2 as an rvalue. */
      conv_dest_type = rvalue_type(op2_type);
      if (is_class_struct_union_type(op2_type)) {
        if (conversion_to_class_possible(op1,
                                         conv_dest_type,
                                         /*try_bitwise_copy=*/TRUE,
                                         /*is_copy_initialization=*/TRUE,
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
    *ambiguous = local_ambiguous;
  } else if (local_ambiguous) {
    /* The conversion is ambiguous.  Issue an error. */
    if (!error_issued) {
      pos_ty2_start_error(ec_ambiguous_user_defined_conversion,
                          &op1->position, op1->type, conv_dest_type);
      diagnose_overload_ambiguity(ambiguity_list,
                                  (an_operand *)NULL,
                                  (an_arg_operand_ptr)NULL,
                                  (an_opname_kind)onk_none);
      free_candidate_function_list(ambiguity_list);
    }  /* if */
    conv_to_error_operand(op1);
  }  /* if */
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
used only in C++ mode.
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
  if (uncallable != NULL) *uncallable = FALSE;
  *class_bitwise_copy = FALSE;
  *ambiguous = FALSE;
  class_type = skip_typerefs(class_type);
  cssp = symbol_supplement_for_class(class_type);
  if (cssp->construction_by_bitwise_copy_allowed ||
      class_type->variant.class_struct_union.is_nonreal_class) {
    /* A bitwise copy is allowed.  Also used when the class is nonreal,
       because we don't know about constructors in that case. */
    cctor_sym = NULL;
    if (!sun_mode && 
        any_qualifier_in_set_missing(TQ_CONST, required_qualifiers)) {
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
      if (debug_level >= 4) {
        db_symbol(sym, "select_overloaded_copy_constructor: considering ", 2); 
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
            !deduce_one_parameter(ptp->type, (an_operand *)NULL, arg_type,
                                  sym, &template_arg_list)) {
          /* Deduction failed. */
          goto reject_function;
        }  /* if */
        routine_type = wrapup_function_template_argument_deduction(
                                                   template_arg_list,
                                                   sym,
                                                   (a_template_param_ptr)NULL);
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
      if (!is_copy_constructor_type(routine_type, class_type, &qualifiers,
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
  return cctor_sym;
}  /* select_overloaded_copy_constructor */


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
#endif /* DEBUG */
}  /* overload_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
