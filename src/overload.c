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

overload.c -- Expression processing overload resolution.

*/

#include "basics.h"
#include "host_envir.h"
#include "overload.h"
#include "exprutil.h"
#include "symbol_tbl.h"
#include "symbol_ref.h"
#include "il.h"
#include "mem_manage.h"
#include "error.h"
#include "debug.h"
#include "templates.h"
#include "cmd_line.h"
#include "types.h"
#include "folding.h"
#include "class_decl.h"

/* Forward declarations required because of out-of-order references. */
static void prep_conversion_operand(an_operand    *source_operand,
                                    a_type_ptr    dest_type,
                                    a_conv_descr  *conversion,
                                    a_boolean     is_initialization,
                                    an_error_code incompatible_err,
                                    a_source_position *err_pos);
static a_boolean conversion_to_class_possible(
                                  an_operand               *source_operand,
                                  a_type_ptr               dest_type,
                                  a_boolean                need_lvalue_result,
                                  a_conv_descr             *conversion,
                                  a_boolean                *ambiguous,
                                  a_candidate_function_ptr *ambiguity_list);


static void clear_conv_descr(a_conv_descr_ptr conv)
/*
Clear a conversion description.
*/
{
  conv->routine                        = NULL;
  conv->class_identity_or_bitwise_copy = FALSE;
  conv->result_is_an_lvalue            = FALSE;
  conv->ambiguous                      = FALSE;
  clear_std_conv_descr(&conv->std);
}  /* clear_conv_descr */


a_symbol_ptr find_addr_of_overloaded_function_match(
                                               a_symbol_ptr       ovl_sym,
                                               a_type_ptr         dest_type,
                                               a_source_position  *source_pos,
                                               an_arg_match_level *match_level,
                                               a_std_conv_descr   *std_conv,
                                               a_boolean          *ambiguous)
/*
ovl_sym is the symbol from an indefinite function operand representing the
address of an overloaded function.  It is being converted to a destination type
dest_type.  If dest_type is a pointer or pointer-to-member type that could be
a pointer to one of the overloaded functions, return a pointer to that
function's symbol; otherwise, return NULL.  Also set *match_level to indicate
whether or not any conversion is needed after the coercion to a specific
function pointer, and set *std_conv to indicate any such conversion.
If more than one function matches, return NULL and *ambiguous TRUE.
source_pos is the source position of the reference.  See ARM 13.3,
"Address of Overloaded Function".
*/
{
  a_boolean        is_ptr = FALSE, is_ptr_to_member = FALSE;
  a_boolean        sym_is_list, any_function_templates;
  a_boolean        dest_type_has_type_qualifiers = FALSE;
  a_type_ptr       routine_type, dest_class, ptr_routine_type;
  a_type_ptr       dest_underlying_type;
  a_symbol_ptr     sym, match_sym = NULL, instance_sym;
  unsigned long    number_of_matches = 0;
  a_std_conv_descr std_conversion;

  db_enter(4, "find_addr_of_overloaded_function_match");
  clear_std_conv_descr(std_conv);
  *ambiguous = FALSE;
  if (is_pointer_type(dest_type)) {
    dest_class = NULL;
    is_ptr = TRUE;
    dest_underlying_type = type_pointed_to(dest_type);
  } else if (is_ptr_to_member_type(dest_type)) {
    dest_class = pm_class_type(dest_type);
    is_ptr_to_member = TRUE;
    dest_underlying_type = pm_member_type(dest_type);
  }  /* if */
  if (is_ptr || is_ptr_to_member) {
    /* dest_type is a pointer or pointer-to-member type, but the underlying
       type is not necessarily a function type. */
    dest_type_has_type_qualifiers = is_qualified_type(dest_underlying_type);
    dest_underlying_type = skip_typerefs(dest_underlying_type);
    reduce_projection_symbol_to_fundamental_symbol(ovl_sym);
    if (ovl_sym->kind == (a_symbol_kind)sk_function_template) {
      /* A single function template represents multiple instantiations of
         that template. */
      sym_is_list = FALSE;
    } else {
#if CHECKING
      if (ovl_sym->kind != (a_symbol_kind)sk_overloaded_function) {
        internal_error(
                "find_addr_of_overloaded_function_match: not overloaded func");
      }  /* if */
#endif /* CHECKING */
      /* A list of overloaded functions. */
      sym_is_list = TRUE;
      ovl_sym = ovl_sym->variant.overloaded_function.symbols;
    }  /* if */
    /* Check each function in the overload set to see if its type matches
       the one desired.  The algorithm is the one for template matching
       (ARM 14.4):
         (1)  Look for an exact match.
         (2)  Look for a function template that can yield a function with
              exactly the right type.
         (3)  Look for a match involving a conversion (this is possible only
              for pointers-to-members, because there are no implicit
              conversions defined on pointers).
       If there is more than one match at any level, the operation is
       ambiguous.  That's probably possible only when function templates
       are involved. */
    /* Check first for an exact match. */
    any_function_templates = FALSE;
    for (sym = ovl_sym;
         sym != NULL;
         sym = (sym_is_list ? sym->next : NULL)) {
      if (sym->kind == (a_symbol_kind)sk_function_template) {
        /* Function template.  Ignore on this pass, but enable a second pass
           to try matching it. */
        any_function_templates = TRUE;
      } else {
        /* Not a function template (i.e., a normal function). */
        routine_type = routine_symbol_type(sym);
        /* Note that the type qualifiers on both types have already been
           dropped. */
        if (identical_types(routine_type, dest_underlying_type) &&
            sym->class_of_which_a_member == dest_class) {
          /* Exact match. */
          match_sym = sym;
          *match_level = aml_exact;
          number_of_matches++;
        }  /* if */
      }  /* if */
    }  /* for */
    if (number_of_matches == 0 && any_function_templates &&
        is_function_type(dest_underlying_type)) {
      /* Try matching function templates.  Do not try if the underlying type
         is not a function type. */
      for (sym = ovl_sym;
           sym != NULL;
           sym = (sym_is_list ? sym->next : NULL)) {
        if (sym->kind == (a_symbol_kind)sk_function_template) {
          /* Function template. */
          instance_sym = matching_template_function(sym, dest_underlying_type,
                                                    source_pos);
          if (instance_sym != NULL) {
            /* Template match. */
            match_sym = instance_sym;
            *match_level = aml_exact;
            number_of_matches++;
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
    if (number_of_matches == 0) {
      /* Try matches involving an implicit conversion.  This is here
         primarily for the pointer-to-member case, but it makes sense to
         handle the normal pointer case too in case the implicit conversion
         rules change (also, it makes the error message clearer in the
         case where dest_type is "void *"). */
      for (sym = ovl_sym;
           sym != NULL;
           sym = (sym_is_list ? sym->next : NULL)) {
        if (sym->kind != (a_symbol_kind)sk_function_template) {
          /* Not a function template (i.e., a normal function). */
          routine_type = routine_symbol_type(sym);
          if (is_ptr) {
            ptr_routine_type = make_pointer_type(routine_type);
          } else {
            ptr_routine_type = ptr_to_member_type(routine_type, dest_class);
          }  /* if */
          if (impl_conversion_possible(ptr_routine_type,
                                       /*source_is_constant=*/FALSE,
                                       (a_constant_ptr)NULL,
                                       dest_type,
                                       /*suppress_extensions=*/TRUE,
                                       ec_no_error,
                                       &std_conversion)) {
            /* A match. */
            match_sym = sym;
            *match_level = aml_std_conversion;
            *std_conv = std_conversion;
            number_of_matches++;
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
    if (number_of_matches > 1) {
      /* Ambiguous case. */
      *ambiguous = TRUE;
      match_sym = NULL;
    }  /* if */
  } else {
    /* dest_type is not a pointer type, so no function can match. */
  }  /* if */
  if (match_sym != NULL) {
    /* If the pointer type we converted to has extra type qualifiers,
       set the tie-breaker flag in the standard conversion description. */
    if (dest_type_has_type_qualifiers) std_conv->type_qualifiers_added = TRUE;
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


static void clear_arg_match_summary(an_arg_match_summary_ptr amsp)
/*
Clear the fields of the indicated argument match summary entry to default
values.
*/
{
  amsp->next                       = NULL;
  amsp->match_level                = aml_none;
  amsp->const_anachronism          = FALSE;
  amsp->is_match_for_this_param    = FALSE;
  amsp->param_type                 = NULL;
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
  }  /* if */
  if (amsp->match_level == aml_user_conversion &&
      amsp->conversion.std.nontrivial_conversion) {
    fprintf(f_debug, " (plus nontrivial conversion)");
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
  cfp->template_arg_list = NULL;
  cfp->operand_type_pattern = NULL;
  cfp->is_user_conversion = FALSE;
  clear_conv_descr(&cfp->conversion);
  cfp->pointer_type = NULL;
  cfp->arg_matches = NULL;
  cfp->arg_operand_list = NULL;
  cfp->current_arg_match = NULL;
  cfp->prev_func_arg_match_with_same_match_level = NULL;
  cfp->in_best_match_set = FALSE;
  cfp->in_best_match_set_for_some_argument = FALSE;
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
  } else {
    /* Built-in operator case. */
    fprintf(f_debug, "Built-in %s", cfp->operand_type_pattern);
    if (cfp->pointer_type != NULL) {
      fprintf(f_debug, ", pointer_type = ");
      db_type(cfp->pointer_type);
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


static void add_builtin_operator_to_candidate_functions_list(
                                char                     *operand_type_pattern,
                                a_type_ptr               pointer_type,
                                an_arg_match_summary_ptr arg_matches,
                                a_candidate_function_ptr *candidate_functions)
/*
Add the built-in operator identified by operand_type_pattern and pointer_type
to the candidate_functions list.  arg_matches gives information about how well
the operands we have match the operator's required operand types.
*/
{
  a_candidate_function_ptr candidate;

  candidate = alloc_candidate_function();
  candidate->operand_type_pattern = operand_type_pattern;
  candidate->pointer_type = pointer_type;
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
                                 an_arg_match_summary_ptr arg_matches,
                                 an_arg_operand_ptr       arg_operand_list,
                                 a_candidate_function_ptr *candidate_functions)
/*
Add the function template identified by function_symbol to the front of the
candidate_functions list.  arg_matches gives information about how well
the actual arguments we have match the function's formal parameters (but
the entries are sometimes just place-holders).  arg_operand_list gives the
operand list.
*/
{
  a_candidate_function_ptr candidate;

  candidate = alloc_candidate_function();
  candidate->function_symbol = function_symbol;
  candidate->is_function_template = TRUE;
  candidate->arg_matches = arg_matches;
  candidate->arg_operand_list = arg_operand_list;
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
Type codes used in type patterns that describe built-in operators
for overload resolution.
*/
#define INTEGRAL_TYPE_CODE 'I'
#define ARITH_TYPE_CODE 'A'
#define POINTER_TYPE_CODE 'P'
#define CORRESP_POINTER_TYPE_CODE 'C'
#define PTR_TO_MEMBER_TYPE_CODE 'M'


static char *name_for_type_code(char type_code)
/*
Return a printable string describing a type code.
*/
{
  char *str;

  if (type_code == INTEGRAL_TYPE_CODE) {
    str = "integer";
  } else if (type_code == ARITH_TYPE_CODE) {
    str = "arithmetic";
  } else if (type_code == POINTER_TYPE_CODE ||
             type_code == CORRESP_POINTER_TYPE_CODE) {
    str = "pointer";
  } else {
#if CHECKING
    if (type_code != PTR_TO_MEMBER_TYPE_CODE) {
      internal_error("name_for_type_code: bad type code");
    }  /* if */
#endif /* CHECKING */
    str = "pointer-to-member";
  }  /* if */
  return str;
}  /* name_for_type_code */


static void diagnose_overload_ambiguity(
                                  a_candidate_function_ptr candidate_functions,
                                  an_opname_kind           kind)
/*
Issue the add-on diagnostics to describe an overloading ambiguity.
candidate_functions gives the list of functions in the best-match set.
kind gives the operator associated with any entries in the set for
built-in operators.  The start_error or equivalent has already been done.
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
      if (function_sym->kind == (a_symbol_kind)sk_projection &&
          function_sym->variant.projection.ambiguous) {
        /* Function symbol is ambiguous by inheritance.  Use a special
           message.  This happens for conversion functions inherited
           into a derived class. */
        err_code = ec_ambiguous_by_inheritance_add_on;
      } else {
        /* Normal case. */
        err_code = ec_ambiguous_function_add_on;
      }  /* if */
      sym_add_diag_info(err_code, function_sym);
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
      } else {
        /* Binary operator. */
        (void)sprintf(buf, "%s %s %s", name_for_type_code(pattern[0]), opname,
                                       name_for_type_code(pattern[1]));
      }  /* if */
      str_add_diag_info(ec_builtin_operator_add_on, buf);
    }  /* if */
  }  /* for */
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
  if (param_is_reference && !conversion->ambiguous) {
    /* For reference parameters, see if any type qualifiers were added under
       the reference relative to the output type of the conversion function.
       That serves as a tie-breaker in overload resolution. */
    param_type = type_pointed_to(param_type);
    check_assertion(arg_summary->conversion.routine != NULL);
    /* Get the return type of the conversion routine. */
    conversion_type = arg_summary->conversion.routine->type;
    conversion_type = f_skip_typerefs(conversion_type);
    conversion_type = conversion_type->variant.routine.return_type;
    conversion_type = f_skip_typerefs(conversion_type);
    if (is_reference_type(conversion_type)) {
      /* The function returns a reference type.  Drop that. */
      conversion_type = f_skip_typerefs(conversion_type);
      if (!arg_summary->conversion.result_is_an_lvalue) {
        /* The lvalue gets converted to an rvalue, so the type qualifiers
           are dropped. */
        conversion_type = f_skip_typerefs(conversion_type);
      }  /* if */
    }  /* if */
    if (any_qualifier_missing(conversion_type, param_type)) {
      /* Some type qualifiers are being added.  Remember that for use as a
         tie-breaker later. */
      arg_summary->conversion.std.type_qualifiers_added = TRUE;
    }  /* if */
  }  /* if */
}  /* set_arg_summary_for_user_conversion */


static void set_user_conversion_for_class_copy(an_operand   *arg_operand,
                                               a_conv_descr *conversion,
                                               a_type_ptr   param_type)
/*
arg_operand (of class type) is being passed as an argument to a parameter
of type param_type (also a class type, either the same one or a base type
thereof).  Set *conversion to indicate the conversion that is required
to do that (a bitwise copy or a copy constructor call).  Note that
conversion->std.cast_base_class is set already, and that value is preserved
in the bitwise copy case.
*/
{
  a_class_symbol_supplement_ptr cssp = symbol_supplement_for_class(param_type);
  a_boolean                     ambiguous;

  if (cssp->construction_by_bitwise_copy_allowed) {
    /* This is a bitwise copy. */
    /* Do NOT clear the conversion entry here.  We want to preserve the
       cast_base_class pointer. */
    conversion->class_identity_or_bitwise_copy = TRUE;
  } else {
    /* This case must require a copy constructor. */
    if (conversion_to_class_possible(arg_operand, param_type,
                                     /*need_lvalue_result=*/FALSE,
                                     conversion, &ambiguous,
                                     (a_candidate_function_ptr *)NULL) ||
        ambiguous) {
      /* Conversion is okay. */
#if CHECKING
    } else {
      internal_error("set_user_conversion_for_class_copy: conv not possible");
#endif /* CHECKING */
    }  /* if */
  }  /* if */
}  /* set_user_conversion_for_class_copy */


static a_boolean array_transformation_needed_on_reference_init(
                                                         a_type_ptr arg_type,
                                                         a_type_ptr param_type)
/*
Return TRUE if when initializing a parameter of type param_type (a reference
type) from an argument of type arg_type (an array type), the array -->
pointer transformation should be done.
*/
{
  a_boolean  transform_needed = FALSE;
  a_type_ptr base_param_type = type_pointed_to(param_type);

  /* The logic here must match conv_array_operand_to_pointer_operand. */
  /* The array --> pointer transformation is wanted only if initializing
     a reference to the right pointer type, as in
       char *const &r = "abc";
  */
  if (is_pointer_type(base_param_type)) {
    base_param_type = type_pointed_to(base_param_type);
    if (types_are_compatible_ignoring_qualifiers(base_param_type,
                                               array_element_type(arg_type))) {
      transform_needed = TRUE;
    }  /* if */
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
  a_boolean  transform_needed = FALSE;
  a_type_ptr base_param_type = type_pointed_to(param_type);

  /* The logic here must match conv_function_designator_to_ptr_to_function. */
  /* The function --> pointer transformation is wanted only if
     initializing a reference to the right pointer type, as in
       void f();
       void (&r)() = f;
  */
  if (is_pointer_type(base_param_type)) {
    base_param_type = type_pointed_to(base_param_type);
    if (types_are_compatible_ignoring_qualifiers(base_param_type, arg_type)) {
      transform_needed = TRUE;
    }  /* if */
  }  /* if */
  return transform_needed;
}  /* function_transformation_needed_on_reference_init */


void determine_arg_match_level(an_operand           *arg_operand,
                               a_type_ptr           arg_type,
                               a_type_ptr           param_type,
                               a_boolean            try_user_conversions,
                               an_arg_match_summary *arg_summary)
/*
Determine how well an actual argument matches a formal parameter with type
param_type.  The actual argument is usually given by arg_operand, but
if arg_type is non-NULL, it provides the argument type for an argument
about which nothing else is known (and arg_operand is ignored; this can
only be used for selector operands, i.e., those being matched up with
a "this" parameter).  arg_summary is set to indicate the level of match.
This is used in resolving overloaded function calls.  See ARM 13.2.
User-defined conversions will be attempted only if try_user_conversions
is TRUE; it must be FALSE if arg_type is non-NULL.
*/
{
  an_operand        *orig_arg_operand;
  a_boolean         param_is_reference;
  a_boolean         param_is_class_type, arg_is_class_type;
  a_boolean         ref_type_qualifiers_dropped, ref_type_qualifiers_added;
  a_std_conv_descr  std_conversion;
  a_base_class_ptr  bcp;
  a_boolean         ambiguous;
  a_boolean         arg_operand_is_constant;
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
    /* The parameter type is a reference.  Drop the reference and remember
       we have one.  This is the "T --> T&" case.  Note that we're dropping any
       qualifiers above the reference type, but that's okay; they don't really
       mean anything ("int &const a" is meaningless). */
    param_type = type_pointed_to(param_type);
    /* Check the type qualifiers to see if they can be reconciled by
       trivial conversions. */
    if (type_qualifiers_match(param_type, arg_type)) {
      /* The qualifiers are the same: okay. */
    } else if (any_qualifier_missing(param_type, arg_type)) {
      /* There are some type qualifiers on the argument type that do not
         appear on the parameter type, so some type qualifiers are being
         dropped. */
      ref_type_qualifiers_dropped = TRUE;
    } else {
      /* Some type qualifiers are being added.  That's okay, but it may
         be a tie-breaker later. */
      ref_type_qualifiers_added = TRUE;
    }  /* if */
  } else {
    /* The parameter type is not a reference, which means the argument would
       have to be converted from an lvalue to an rvalue.  In the process,
       it would lose its top-level type qualifiers.  That means the type
       qualifiers will be compatible. */
    arg_type = skip_typerefs(arg_type);
    /* Qualifiers on the parameter type are also not significant when dealing
       with rvalues.  One cannot distinguish f(int) and f(const int). */
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
            value_of_constant_var_lvalue_expr(arg_operand->variant.expression);
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
  /* If the type qualifiers are not okay, do not check for an exact match
     or a match with promotions.  Cases other than those do their own
     checking of type qualifiers. */
  if (!ref_type_qualifiers_dropped) {
    /* Check for an exact match.  This is case [1] in the ARM. */
    /* The "_ignoring_qualifiers" version is called here to deal with
       array types with qualifiers on the element type. */
    if (types_are_compatible_ignoring_qualifiers(unqual_arg_type,
                                                 unqual_param_type)) {
      /* There is an exact match, possibly involving trivial conversions. */
      arg_summary->match_level = aml_exact;
      if (!param_is_reference && is_class_struct_union_type(param_type)) {
        /* The argument and parameter are the same class type, so this
           qualifies as a class copy. */
        check_assertion(arg_operand != NULL);
        set_user_conversion_for_class_copy(arg_operand,
                                           &arg_summary->conversion,
                                           param_type);
      }  /* if */
      goto have_level;
    }  /* if */
    /* Check for another exact match case, for pointers involving addition
       of type qualifiers on the type pointed to (the "T* --> (qualified T)*"
       case). */
    if (is_pointer_type(param_type) && is_pointer_type(arg_type)) {
      a_type_ptr arg_type_pointed_to = type_pointed_to(arg_type);
      a_type_ptr param_type_pointed_to = type_pointed_to(param_type);
      if (types_are_compatible_ignoring_qualifiers(arg_type_pointed_to,
                                                   param_type_pointed_to)) {
        /* The types pointed to are compatible.  See if the qualifiers
           are okay.  Note that the case where the qualifiers are the same
           need not be checked for, since it would have been handled
           above in the normal exact-match case. */
        if (any_qualifier_missing(param_type_pointed_to,
                                  arg_type_pointed_to)) {
          /* There are some qualifiers being dropped, so the pointer types
             are not compatible. */
        } else {
          /* Some qualifiers are being added.  This is the
             "T* --> (qualified T)*" case, which should be remembered
             as a possible tie-breaker later. */
          arg_summary->match_level = aml_exact;
          arg_summary->conversion.std.type_qualifiers_added = TRUE;
          goto have_level;
        }  /* if */
      }  /* if */
    }  /* if */
    if (arg_operand != NULL && is_indefinite_function_operand(arg_operand)) {
      /* The source is an indefinite function, i.e., the address of an
         overloaded function.  It can be converted to an appropriate
         pointer (ARM 13.3) or pointer-to-member type (not mentioned in ARM,
         but sensible).  Note that in the pointer-to-member case standard
         conversions (to a derived class) may also be required.  Note
         that the operand can be either a function designator or a pointer
         to a function at this point; it doesn't matter. */
      if (find_addr_of_overloaded_function_match(arg_operand->variant.symbol,
                                                 param_type,
                                                 &arg_operand->position,
                                                 &arg_summary->match_level,
                                                 &std_conversion,
                                                 &ambiguous) || ambiguous) {
        /* There is a suitable indefinite function, or more than one.
           arg_summary->match_level has been set appropriately. */
        arg_summary->conversion.std = std_conversion;
        arg_summary->conversion.ambiguous = ambiguous;
        goto have_level;
      }  /* if */
    }  /* if */
    /* Try a match involving promotions.  This is case [2] in the ARM.
       Promotions are the default argument promotions (integral promotions
       and float --> double). */
    if (types_are_compatible(default_argument_promotion(unqual_arg_type),
                             unqual_param_type)) {
      arg_summary->match_level = aml_promotion;
      goto have_level;
    }  /* if */
  }  /* if */
  /* Try a match involving standard conversions.  This is case [3] in
     the ARM. */
  arg_operand_is_constant = FALSE;
  arg_operand_constant = NULL;
  if (arg_operand != NULL && is_an_rvalue(arg_operand)) {
    /* For a constant argument, get the constant value. */
    arg_operand_is_constant = is_constant_operand(arg_operand);
    if (arg_operand_is_constant) {
      arg_operand_constant = &arg_operand->variant.constant;
    }  /* if */
  }  /* if */
  if (impl_conversion_possible(arg_type,
                               arg_operand_is_constant,
                               arg_operand_constant,
                               param_type, /*suppress_extensions=*/TRUE,
                               ec_incompatible_param, &std_conversion)) {
    /* Match with standard conversions. */
    arg_summary->match_level = aml_std_conversion;
    arg_summary->conversion.std = std_conversion;
    if (cfront_2_1_mode && param_is_reference &&
        arg_summary->conversion.std.cast_base_class == NULL) {
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
  param_is_class_type = is_immediate_class_type(unqual_param_type);
  arg_is_class_type = is_immediate_class_type(unqual_arg_type);
  if (param_is_class_type && arg_is_class_type &&
      !ref_type_qualifiers_dropped &&
      (bcp = find_base_class_of(arg_type, param_type)) != NULL) {
    /* The argument is a derived class and the parameter is a base class,
       so the conversion can be done. */
    arg_summary->match_level = aml_std_conversion;
    arg_summary->conversion.std.cast_base_class = bcp;
    if (param_is_reference) {
      /* This case falls under the reference standard conversions (ARM 4.7). */
      /* The operand need not be forced to an rvalue. */
      check_assertion(arg_operand != NULL);
      arg_summary->conversion.result_is_an_lvalue = is_an_lvalue(arg_operand);
    } else {
      /* This case falls under the aggregate initialization rules (ARM 8.4.1)
         or the copy constructor rules (ARM 12.8).  Note that this case
         counts as a standard conversion even if a copy constructor is
         called. */
      check_assertion(arg_operand != NULL);
      set_user_conversion_for_class_copy(arg_operand,
                                         &arg_summary->conversion,
                                         param_type);
    }  /* if */
    goto have_level;
  }  /* if */
  if (try_user_conversions) {
    a_conv_descr conversion;
    /* Try a match involving user-defined conversions.  This is case [4]
       in the ARM.  Note that we use orig_arg_operand, i.e., the argument
       before any implicit transformations (like array --> pointer) for
       these tests, because the user-defined conversion routines may or
       may not want the transformations we've done. */
    check_assertion(orig_arg_operand != NULL);
    if (param_is_class_type &&
        (conversion_to_class_possible(orig_arg_operand, param_type,
                                      /*need_lvalue_result=*/FALSE,
                                      &conversion, &ambiguous,
                                      (a_candidate_function_ptr *)NULL) ||
         ambiguous)) {
      /* There is a constructor or conversion function (or several) that
         will convert the argument type to the parameter class type. */
      set_arg_summary_for_user_conversion(arg_summary, &conversion,
                                          orig_param_type, param_is_reference);
      goto have_level;
    } else if (arg_is_class_type &&
               (conversion_from_class_possible(orig_arg_operand, param_type,
                                               (a_builtin_type_kind_set)
                                                                      BTK_NONE,
                                               /*need_lvalue_result=*/FALSE,
                                               &conversion,
                                               &ambiguous,
                                           (a_candidate_function_ptr *)NULL) ||
                ambiguous)) {
      /* There is a conversion function (or several) that will convert the
         argument class type into the parameter type or to some type that
         can be converted to the parameter type via a standard conversion. */
      set_arg_summary_for_user_conversion(arg_summary, &conversion,
                                          orig_param_type, param_is_reference);
      goto have_level;
    }  /* if */
  }  /* if */
  /* Case [5] in the ARM, match with ellipsis, is handled by the caller. */
  /* No match is possible. */
  arg_summary->match_level = aml_none;
have_level:;
  if (ref_type_qualifiers_added &&
      (int)arg_summary->match_level < (int)aml_user_conversion) {
    /* Some type qualifiers were added under a reference.  This can serve as
       a tie-breaker later.  User-defined conversions and above work this out
       a different way. */
    arg_summary->conversion.std.type_qualifiers_added = TRUE;
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


void selector_match_with_this_param(
                               an_operand           *bound_function_selector,
                               a_boolean            selector_is_object_pointer,
                               a_boolean            conversion_function_case,
                               a_routine_ptr        rout,
                               a_type_ptr           routine_type,
                               an_arg_match_summary *this_match_summary)
/*
Determine how well the selector object indicated by *bound_function_selector
matches the "this" parameter of the routine type indicated by *routine_type.
If selector_is_object_pointer is TRUE, *bound_function_selector has
already been converted to object pointer form.  Otherwise, it's just
an object (lvalue or rvalue).  Return the match summary in *this_match_summary.
If the specific routine being called is known, rout points to the
routine entry; otherwise, rout is NULL.  rout must be non-NULL when
calling a constructor or destructor, so that those can be treated as a
special case: constructors and destructors can be called for const- and
volatile-qualified objects even though they themselves are not (and
cannot be) const- or volatile-qualified.  If conversion_function_case
is TRUE, the underlying type of the selector is assumed to be the proper
class or a derived class thereof (except for error cases).
*/
{
  a_type_ptr selector_type, this_param_type, this_param_base_type;
  a_type_ptr this_param_class_type, const_this_param_base_type;
  a_type_ptr ptr_selector_type, const_this_param_type;

  db_enter(4, "selector_match_with_this_param");
  if (rout != NULL &&
      (rout->special_kind == (a_special_function_kind)sfk_constructor ||
       rout->special_kind == (a_special_function_kind)sfk_destructor)) {
    /* The routine is a constructor or destructor, so the check is
       suppressed. */
    clear_arg_match_summary(this_match_summary);
    this_match_summary->match_level = aml_exact;
  } else {
    /* Get the "this" parameter type. */
    this_param_type = implicit_this_param_type_of(routine_type);
#if CHECKING
    if (this_param_type == NULL) {
      internal_error("selector_match_with_this_param: this_param_type NULL");
    }  /* if */
#endif /* CHECKING */
    this_param_base_type = type_pointed_to(this_param_type);
    this_param_class_type = skip_typerefs(this_param_base_type);
    /* Determine the effective selector type. */
    selector_type = bound_function_selector->type;
    if (!m_is_error_type(selector_type)) {
      if (selector_is_object_pointer) {
        selector_type = type_pointed_to(selector_type);
      }  /* if */
      /* For conversion functions projected into a derived class,
         the base class cast has not been done yet (and cannot be, since we
         are just investigating whether or not the function is appropriate).
         However, the cast to the base class is not supposed to be counted
         as a standard conversion in the overload resolution "cost", because
         the reference can be thought of as being to the projection of the
         function into the derived class.  Change the selector type to the
         properly-qualified version of the proper base class (the class of
         the function), thus in effect doing any required base class cast.
         The underlying class of the selector must already be the base
         class or a derived class, or the function would not have been
         found. */
      if (conversion_function_case) {
        selector_type = make_identically_qualified_type(this_param_class_type,
                                                        selector_type);
      }  /* if */
    }  /* if */
    ptr_selector_type = make_pointer_type(selector_type);
    /* See how well the selector type and the "this" parameter type
       match up. */
    determine_arg_match_level((an_operand *)NULL, ptr_selector_type,
                              this_param_type,
                              /*try_user_conversions=*/FALSE,
                              this_match_summary);
    if (cfront_2_1_mode &&
        this_match_summary->match_level == aml_none) {
      /* No match.  Try the cfront anachronism of calling a function that
         does not require a const "this" with a const selector.  See also
         set_up_for_conversion_function_call. */
      /* Make the type that the "this" parameter would have if the routine
         were const, and try again. */
      const_this_param_base_type = make_qualified_type(this_param_base_type,
                                                       /*is_const=*/TRUE,
                                                       /*is_volatile=*/FALSE);
      const_this_param_type = make_pointer_type(const_this_param_base_type);
      const_this_param_type = make_qualified_type(const_this_param_type,
                                                  /*is_const=*/TRUE,
                                                  /*is_volatile=*/FALSE);
      determine_arg_match_level((an_operand *)NULL, ptr_selector_type,
                                const_this_param_type,
                                /*try_user_conversions=*/FALSE,
                                this_match_summary);
      if (this_match_summary->match_level != aml_none) {
        /* Anachronism -- calling non-const function with const object. */
	this_match_summary->const_anachronism = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  this_match_summary->is_match_for_this_param = TRUE;
  db_exit();
}  /* selector_match_with_this_param */


static void try_overloaded_function_match(
                 a_symbol_ptr             overloaded_function_symbol,
                 an_arg_operand_ptr       arg_operand_list,
                 a_boolean                have_selector,
                 an_operand               *bound_function_selector,
                 a_boolean                selector_is_object_pointer,
                 a_boolean                user_conversion_case,
                 a_candidate_function_ptr *candidate_functions,
                 a_boolean                *matched_except_for_missing_selector)
/*
Find out how well the functions described by overloaded_function_symbol
match the argument list given by arg_operand_list and the selector given
(if have_selector is TRUE) by bound_function_selector.
overloaded_function_symbol may be an overloaded function, a simple
function, or a projection symbol for one of those.  bound_function_selector
is an object pointer if selector_is_object_pointer is TRUE, an object
otherwise.  Any viable functions are added to the candidate_functions
list along with information on the level of argument matches.  If a
match would have been found except for the absence of a selector, set
*matched_except_for_missing_selector TRUE; that allows a different
error message.  If user_conversion_case is TRUE, this analysis is
being done as part of resolving an implicit conversion: the functions
are constructors, have_selector is FALSE (sic; the "this" parameter
is not matched up); user-defined conversions are not tried on argument
matches, and the "conversion" field is set in any candidate function
entries created.
*/
{
  a_boolean                overloaded_function_case;
  a_symbol_ptr             function_symbol;
  a_type_ptr               routine_type;
  a_routine_type_supplement_ptr
                           rtsp;
  a_param_type_ptr         param;
  a_boolean                reached_ellipsis;
  an_arg_match_summary_ptr this_match, this_match_next;
  an_arg_match_summary_ptr arg_match, arg_match_list, end_arg_match_list;
  an_arg_operand_ptr       arg_operand;
  a_boolean                function_is_nonstatic_member_function;
  a_boolean                function_template_case;
  a_type_ptr               implicit_selector_type = NULL;
#if DEBUG
  unsigned long            narg;
#endif /* DEBUG */

  reduce_projection_symbol_to_fundamental_symbol(overloaded_function_symbol);
  /* Determine whether or not the symbol is an overloaded function. */
  overloaded_function_case = (overloaded_function_symbol->kind ==
                                        (a_symbol_kind)sk_overloaded_function);
  if (overloaded_function_case) {
    /* Overloaded functions. */
    function_symbol =
               overloaded_function_symbol->variant.overloaded_function.symbols;
  } else {
    /* Non-overloaded function. */
    function_symbol = overloaded_function_symbol;
  }  /* if */
  /* If we have no selector, see if any one of the functions requires one.
     If so, we will look to see if an implicit "this->" can be generated.
     Don't do this for the conversion case (the "this" parameter of the
     constructor is not used in the match). */
  if (!user_conversion_case && !have_selector) {
    a_boolean some_function_needs_selector = FALSE;
    /* Check the first or only function to see whether or not it requires
       a selector. */
    if (function_symbol->kind != (a_symbol_kind)sk_function_template) {
      routine_type = routine_symbol_type(function_symbol);
      if (routine_type_is_nonstatic_member_function(routine_type)) {
        some_function_needs_selector = TRUE;
      }  /* if */
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
      a_variable_ptr this_var;
      if (variable_this_exists(&this_var)) {
        /* An implicit selector can be generated. */
        have_selector = TRUE;
        implicit_selector_type =
                            make_pointer_type(type_pointed_to(this_var->type));
      }  /* if */
    }  /* if */
  }  /* if */
  /* Look at each instance of the overloaded function and see whether or
     not it can match the actual arguments, and if so, how well. */
  for (; function_symbol != NULL;
       function_symbol = overloaded_function_case ? function_symbol->next :
                                                    NULL) {
#if DEBUG
    if (debug_level >= 4) {
      db_symbol(function_symbol, "try_overloaded_function_match: considering ",
                2); 
    }  /* if */
    narg = 0;
#endif /* DEBUG */
    function_template_case = (function_symbol->kind ==
                                          (a_symbol_kind)sk_function_template);
    if (function_template_case) {
      /* The symbol is a function template. */
      routine_type = function_symbol->variant.template_info->
                                                variant.function.routine->type;
      routine_type = skip_typerefs(routine_type);      
    } else {
      /* The symbol is not a function template (i.e., it's a normal
         function). */
      routine_type = routine_symbol_type(function_symbol);
    }  /* if */
    /* Do a quick pass through the lists to eliminate a function with an
       obviously wrong number of parameters quickly.  This is not just a
       speed optimization; it avoids recursion loops on constructors
       that look like
         struct A { A(A, xxx, yyy); }
       which look viable as copy constructors on the first argument. */
    arg_match_list = end_arg_match_list = NULL;
    rtsp = routine_type->variant.routine.extra_info;
    param = rtsp->param_type_list;
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
         argument values. */
      if (!param->has_default_arg) goto reject_function;
#if DEBUG
      if (debug_level >= 4) {
        fprintf(f_debug, "try_overloaded_function_match: default arg match\n");
      }  /* if */
#endif /* DEBUG */
    }  /* if */
    /* The function looks okay from the standpoint of argument count. */
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
        fprintf(f_debug, "try_overloaded_function_match: arg %lu\n", narg);
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
                         "try_overloaded_function_match: no arg, no ellipsis");
        reached_ellipsis = TRUE;
        /* There is an ellipsis, so there is a match, but with a low
           desirability. */
        arg_match->match_level = aml_ellipsis;
#if DEBUG
        if (debug_level >= 4) {
          fprintf(f_debug, "try_overloaded_function_match: ellipsis match\n");
        }  /* if */
#endif /* DEBUG */
      } else {
        /* Both the actual argument and formal parameter are available.
           See how well they match.  Don't do this for arguments of
           function templates that involve template types (that's handled
           later by function_template_matches_operand_list). */
        if (!function_template_case || !param->type_involves_template_param) {
          /* Compare their types. */
          determine_arg_match_level(&arg_operand->operand, (a_type_ptr)NULL,
                                    param->type,
                                    !user_conversion_case,
                                    arg_match);
          /* If no match is possible, go on to the next function. */
          if (arg_match->match_level == aml_none) goto reject_function;
        }  /* if */
      }  /* if */
      /* Go on to the next parameter. */
      if (!reached_ellipsis) param = param->next;
    }  /* for */
    /* If param != NULL here, there are default arguments (because we
       got past the argument-count check above). */
    check_assertion_str(param == NULL || param->has_default_arg,
                    "try_overloaded_function_match: no param, no default arg");
    /* All the arguments can be made to match the parameters. */
    /* See if the "this" parameter, if any, matches. */
    /* Template functions do not have "this" parameters. */
    /* Do not process the "this" parameter for constructors in a conversion
       case. */
    if (!function_template_case && !user_conversion_case) {
      function_is_nonstatic_member_function =
                       routine_type_is_nonstatic_member_function(routine_type);
      if (have_selector) {
        /* We have a selector. */
        /* Put a match entry for it on the front of the match list. */
        this_match = alloc_arg_match_summary();
        this_match->is_match_for_this_param = TRUE;
        this_match->next = this_match_next = arg_match_list;
        arg_match_list = this_match;
        if (!function_is_nonstatic_member_function) {
          /* The function has no "this" parameter, so it does not need a
             selector.  We would discard it if this function is chosen,
             but we still need a match entry for it.  It counts as an
             exact match. */
          this_match->match_level = aml_exact;
        } else {
          /* The function requires a selector, and we have one. */
          if (implicit_selector_type != NULL) {
            /* The selector is an implicit "this->".  See how well it
               matches.  It might not match at all. */
            determine_arg_match_level((an_operand *)NULL,
                                      implicit_selector_type,
                                      rtsp->implicit_this_param_type,
                                      /*try_user_conversions=*/FALSE,
                                      this_match);
            this_match->is_match_for_this_param = TRUE;
            /* Set the "next" pointer again, because it is cleared by
               determine_arg_match_level. */
            this_match->next = this_match_next;
            if (this_match->match_level == aml_none) {
              /* Mismatch.  Remember this case to select a different
                 error message if it turns out no function matches. */
              *matched_except_for_missing_selector = TRUE;
              goto reject_function;
            }  /* if */
          } else {
            /* See how the selector expression matches the "this" parameter
               type. */
            selector_match_with_this_param(bound_function_selector,
                                           selector_is_object_pointer,
                                           /*conversion_function_case=*/FALSE,
                                          function_symbol->variant.routine.ptr,
                                           routine_type, this_match);
            /* Set the "next" pointer again, because it is cleared by
               selector_match_with_this_param. */
            this_match->next = this_match_next;
            if (this_match->match_level == aml_none) goto reject_function;
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
    /* The function is a viable candidate.  Add it to the candidates
       list. */
    if (function_template_case) {
      /* The symbol is a function template. */
      add_function_template_to_candidate_functions_list(function_symbol,
                                                        arg_match_list,
                                                        arg_operand_list,
                                                        candidate_functions);
    } else {
      /* The symbol is a normal function. */
      add_function_to_candidate_functions_list(function_symbol,
                                               arg_match_list,
                                               candidate_functions);
      if (user_conversion_case) {
        /* If we are analyzing a user-defined conversion routine to resolve
           an implicit conversion, set "conversion" appropriately.
           Note that this cannot happen for the template case. */
        a_candidate_function_ptr candidate = *candidate_functions;
        candidate->is_user_conversion = TRUE;
        candidate->conversion.routine = function_symbol->variant.routine.ptr;
      }  /* if */
    }  /* if */
    goto next_function;
reject_function:
    /* The function is not suitable. */
    /* Free any argument match summary entries built for it. */
    free_arg_match_summary_list(arg_match_list);
next_function:;
    /* Keep looping to try all the functions in the overload set. */
  }  /* for */
}  /* try_overloaded_function_match */


static int compare_arg_match_levels(an_arg_match_summary *arg_match1,
                                    an_arg_match_summary *arg_match2)
/*
Compare two argument match summary entries and return

  +1 if arg_match1 is a better match than arg_match2,
   0 if the two matches are equal, or
  -1 if arg_match1 is a worse match than arg_match2.

*/
{
  int              cmp;
  a_base_class_ptr bcp_1, bcp_2;

  /* Compare the gross match levels. */
  if ((int)arg_match1->match_level < (int)arg_match2->match_level) {
    /* arg_match1 is better. */
    cmp = 1;
  } else if ((int)arg_match1->match_level > (int)arg_match2->match_level) {
    /* arg_match2 is better. */
    cmp = -1;
  } else {
    /* The matches are equal in terms of match level.  One can still be
       better than the other in some cases. */
    /* They can only be compared if the user-defined part of the conversion
       (if any) is the same in both conversions. */
    if (arg_match1->conversion.routine == arg_match2->conversion.routine) {
      /* A conversion involving a user-defined conversion is better than a
         conversion involving the same user-defined conversion followed by
         a nontrivial conversion, e.g.,
           A->int
         versus
           A->int->float
         (see the commentary at the bottom of p. 317 of the ARM.)
      */
      if (arg_match1->conversion.routine != NULL) {
        /* We have two conversions using the same user-defined conversion. */
        if (arg_match1->conversion.std.nontrivial_conversion !=
            arg_match2->conversion.std.nontrivial_conversion) {
          /* Two user-defined conversions involving the same conversion
             routine.  One does not have a nontrivial conversion after the
             user-defined conversion and the other does, so the one without
             the nontrivial conversion is better. */
          if (arg_match1->conversion.std.nontrivial_conversion) {
            /* arg_match1 has the nontrivial conversion and arg_match2 does
               not, so arg_match2 is better. */
            cmp = -1;
            goto have_cmp;
          } else {
            /* arg_match2 has the nontrivial conversion and arg_match1 does
               not, so arg_match1 is better. */
            cmp = 1;
            goto have_cmp;
          }  /* if */
        }  /* if */
      }  /* if */
      /* A cast to a base class is better than a cast to further along the
         same base class derivation (see rule [3] in ARM 13.2):
           struct A {};
           struct B : public A {};
           struct C : public B {};
           void f(A*);
           void f(B*);
           main () {
             C c;
             f(&c);  // C* -> B* is better than C* -> B* -> A*
           }
         Similar processing applies for casts to derived classes, and for
         pointers-to-members.  Also, a cast to "void *" is considered worse
         that any cast to a base class.
      */
      bcp_1 = arg_match1->conversion.std.cast_base_class;
      bcp_2 = arg_match2->conversion.std.cast_base_class;
      if (bcp_1 != NULL && bcp_2 != NULL &&
          arg_match1->conversion.std.reversed_cast ==
          arg_match2->conversion.std.reversed_cast) {
        /* Both entries have related-class casts, so they can be compared.
           If one is a subsequence of the other, the shorter derivation is
           preferable. */
        if (bcp_1 == bcp_2) {
          /* The same cast in both cases, so the two are equally good.
             Keep going with subsequence checking. */
        } else if (!arg_match1->conversion.std.reversed_cast) {
          /* Normal case: derived --> base cast. */
          if (is_on_any_derivation_of(bcp_2, bcp_1)) {
            /* bcp_1 is a subsequence of bcp_2 and thus preferable. */
            cmp = 1;
            goto have_cmp;
          } else if (is_on_any_derivation_of(bcp_1, bcp_2)) {
            /* bcp_2 is a subsequence of bcp_1 and thus preferable. */
            cmp = -1;
            goto have_cmp;
          }  /* if */
          /* The two classes are unrelated, so no subsequence is possible. */
          goto end_subsequence_check;
        } else {
          /* Base --> derived case (used for pointers to members). */
          if (find_base_class_of(bcp_2->derived_class,
                                 bcp_1->derived_class) != NULL) {
            /* bcp_1's type is a base class of bcp_2's type, so it's a
               subsequence and thus preferable. */
            cmp = 1;
            goto have_cmp;
          } else if (find_base_class_of(bcp_1->derived_class,
                                        bcp_2->derived_class) != NULL) {
            /* bcp_2's type is a base class of bcp_1's type, so it's a
               subsequence and thus preferable. */
            cmp = -1;
            goto have_cmp;
          }  /* if */
          /* The two classes are unrelated, so no subsequence is possible. */
          goto end_subsequence_check;
        }  /* if */
      } else if (bcp_1 != NULL) {
        /* bcp_1 != NULL, bcp_2 == NULL.  We know that the source type must
           be a class pointer, and not a constant zero, which means the
           arg_match2 destination type must be "void *" (since an implicit
           conversion is possible).  A base class cast is preferable to a
           cast to "void *", so arg_match1 is better. */
        cmp = 1;
        goto have_cmp;
      } else if (bcp_2 != NULL) {
        /* bcp_1 != NULL, bcp_2 != NULL.  We know that the source type must
           be a class pointer, and not a constant zero, which means the
           arg_match1 destination type must be "void *" (since an implicit
           conversion is possible).  A base class cast is preferable to a
           cast to "void *", so arg_match2 is better. */
        cmp = -1;
        goto have_cmp;
      }  /* if */
end_subsequence_check:;
    }  /* if */
    /* No special case was found, so the matches are equal. */
    cmp = 0;
  }  /* if */
have_cmp:
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


static void check_template_arg_type_qualifiers(
                                             a_type_ptr *arg_type,
                                             a_type_ptr *param_type,
                                             a_boolean  *type_qualifiers_added)
/*
Check and process the type qualifiers on an argument type *arg_type and a
parameter type *param_type as part of trying to match a function template
to an argument list.  Adjust the types to remove qualifiers that need
not be considered further.  Set *type_qualifiers_added to TRUE if any
type qualifiers are added in the conversion from *arg_type to *param_type
(that serves as a tie-breaker in overload resolution).
*/
{
  /* All combinations of type qualifiers are allowed in one way or
     another.  For each qualifier (e.g., const, volatile,...):
       (a)  If both the argument and the parameter are so-qualified
            or not so-qualified, that's okay.
       (b)  If the parameter is so-qualified but the argument is not,
            that's okay, but it's remembered as a less desirable
            tie-breaker case.  For example:
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
    *type_qualifiers_added = TRUE;
    /* All the qualifiers on the parameter type are case (b) and can be
       removed from further consideration. */
    *param_type = skip_typerefs(*param_type);
  }  /* if */
}  /* check_template_arg_type_qualifiers */


static a_boolean function_template_matches_operand_list(
                                                  a_candidate_function_ptr cfp)
/*
Find out whether or not an instantiation of the function template
indicated in the candidate function entry *cfp can be made to match the
argument list recorded therein.  If so, return TRUE and set template_arg_list
in the candidate function entry to the template argument
list for the specific instance of the template.  Note that it has
already been determined that the function template has the right
number of parameters, and the non-template parameter matches have been
evaluated (but not checked to see if the match is good enough).
*/
{
  a_symbol_ptr       templ_sym;
  a_param_type_ptr   ptp;
  a_template_arg_ptr templ_arg_list = NULL;
  a_boolean          matches = FALSE;
  a_routine_ptr      routine;
  an_arg_operand_ptr arg_operand;
  a_routine_type_supplement_ptr
                     rtsp;
  a_type_ptr         param_type, arg_type, eff_param_type;
  a_base_class_ptr   base_class_conv_needed;
  an_arg_match_summary_ptr
                     arg_match;
  a_boolean          param_is_reference, type_qualifiers_added;
  a_boolean          class_copy_case, pointer_case;

  db_enter(4, "function_template_matches_operand_list");
  templ_sym = cfp->function_symbol;
#if CHECKING
  if (templ_sym->kind != (a_symbol_kind)sk_function_template) {
    internal_error("function_template_matches_operand_list: bad symbol");
  }  /* if */
#endif /* CHECKING */
  if (templ_sym->variant.template_info->variant.function.cannot_be_called) {
    /* The function parameters do not use all of the template parameters,
       so this function cannot be made to match.  An error was issued
       at the point of declaration. */
    goto done;
  }  /* if */
  routine = templ_sym->variant.template_info->variant.function.routine;
  rtsp = routine->type->variant.routine.extra_info;
  /* Compare the types of the arguments to the parameter types. */
  ptp = rtsp->param_type_list;
  arg_operand = cfp->arg_operand_list;
  arg_match = cfp->arg_matches;
  for (; ptp != NULL && arg_operand != NULL;
       ptp = ptp->next, arg_operand = arg_operand->next,
                                                 arg_match = arg_match->next) {
    /* Try to match up the parameter type and the argument type. */
    if (!ptp->type_involves_template_param) {
      /* A parameter not involving a template parameter type.  The argument
         is already known to match the parameter to some extent, but we
         need to check that the match is good enough.  The ARM requires
         an exact match without even trivial conversions, but we allow
         the usual "exact" match of overload resolution (determined already)
         and a cast to a base class (handled here, as an extension). */
      if (arg_match->match_level == aml_exact) {
        /* "Exact" match.  Okay. */
      } else if (arg_match->match_level == aml_error) {
        /* Error match.  Okay. */
      } else if (!strict_ansi_mode &&
                 arg_match->match_level == aml_std_conversion &&
                 (arg_match->conversion.std.cast_base_class != NULL ||
                  !is_null_user_conv_descr(&arg_match->conversion))) {
        /* A cast to a base class, or an object of a derived class passed
           to a parameter of a base class.  Okay as an extension. */
      } else {
        /* Other match: the template cannot be used. */
        goto done;
      }  /* if */
    } else {
      /* A parameter involving a template parameter type. */
      /* The ARM says the match must be exact, without even trivial
         conversions, but we allow some trivial conversions anyway (involving
         references, array and function type decay, and type qualifiers).
         It seems to be necessary, and cfront seems to allow those. */
      /* The code here must match determine_arg_match_level and
         overload_distinguishable. */
      /* An indefinite function cannot be made to match anything. */
      if (is_indefinite_function_operand(&arg_operand->operand)) goto done;
      /* arg_match->param_type is left NULL because subsequence checking does
         not apply for template cases. */
      param_type = ptp->type;
      arg_type = arg_operand->operand.type;
      /* An incomplete type operand cannot be made to match anything.
         This comes up for something like
           struct A *p;
           template<class T> void f(T);
           void m() { f(*p); }
      */
      if (is_incomplete_type(arg_type)) goto done;
      type_qualifiers_added = FALSE;
      pointer_case = FALSE;
      param_is_reference = is_reference_type(param_type);
      /* See if any implicit transformations (e.g., array --> pointer) should
         be done. */
      if (is_array_type(arg_type) &&
          (!param_is_reference ||
           array_transformation_needed_on_reference_init(arg_type,
                                                         param_type))) {
        /* Simulate the array --> pointer transformation.  */
        arg_type = type_after_array_to_pointer_transformation(arg_type);
      } else if (is_a_function_designator(&arg_operand->operand) &&
                 (!param_is_reference ||
                  function_transformation_needed_on_reference_init(arg_type,
                                                                param_type))) {
        /* Simulate the function --> pointer transformation. */
        arg_type = type_after_function_to_pointer_transformation(arg_type,
                                                        &arg_operand->operand);
      }  /* if */
      if (param_is_reference) {
        /* The parameter has a reference type. */
        /* Drop the reference type. */
        param_type = type_pointed_to(param_type);
        /* Check and adjust the top-level type qualifiers. */
        check_template_arg_type_qualifiers(&arg_type, &param_type,
                                           &type_qualifiers_added);
      } else {
        /* Not a reference. */
        /* The argument would be converted to an rvalue and would lose its
           top-level type qualifiers. */
        arg_type = skip_typerefs(arg_type);
        /* Top-level type qualifiers on the parameter type are also not
           important (this is not supported by the ARM, but it matches the
           handling in determine_arg_match_level). */
        param_type = skip_typerefs(param_type);
      }  /* if */
      if (is_pointer_type(arg_type) && is_pointer_type(param_type)) {
        /* Check for cases where type qualifiers are being added down one
           level in a pointer case, e.g., int * --> const int *.
           This is another trivial conversion.
           In general, remove one level of matching pointer types. */
        pointer_case = TRUE;
        arg_type = type_pointed_to(arg_type);
        param_type = type_pointed_to(param_type);
        /* Check and adjust the top-level type qualifiers. */
        check_template_arg_type_qualifiers(&arg_type, &param_type,
                                           &type_qualifiers_added);
      }  /* if */
      /* Note that we haven't checked that the underlying types are compatible.
         That happens later. */
      /* Try to develop a template argument list that will allow this
         function template to match the argument list. */
      /* As the matching is attempted, templ_arg_list is filled in with
         the bindings for the template arguments.  This is needed during the
         matching process to ensure that each argument is used consistently
         and also later in this routine to build the instantiation.  The
         allow_conversion argument is used to determine whether an
         argument requiring a conversion from Derived<T> to Base<T> should
         be considered as a matching type.  This conversion is accepted
         in normal mode but not in strict ANSI mode. */
      if (!matches_template_type(arg_type, param_type, &templ_arg_list,
                                 /*allow_conversion=*/!strict_ansi_mode,
                                 &base_class_conv_needed)) {
        /* Mismatch. */
        goto done;
      }  /* if */
      /* The argument can be made to match. */
      if (type_qualifiers_added) {
        /* The match is one that involves adding type qualifiers, which can be
           a tie-breaker later.  For example:
             template <class T> void f(T) {}
             template <class T> void f(const T&) {}
             void m() { int i; f(i); }
        */
        arg_match->conversion.std.type_qualifiers_added = TRUE;
      }  /* if */
      class_copy_case = FALSE;
      if (base_class_conv_needed != NULL) {
        /* The extension allowing a standard conversion of a derived class to
           a base class was used. */
        arg_match->match_level = aml_std_conversion;
        arg_match->conversion.std.cast_base_class = base_class_conv_needed;
        /* Save information needed to check whether or not a copy
           constructor is needed. */
        class_copy_case = TRUE;
        eff_param_type = base_class_conv_needed->type;
      } else {
        /* Normal case: exact match. */
        arg_match->match_level = aml_exact;
        if (is_class_struct_union_type(arg_type)) {
          /* Save information needed to check whether or not a copy
             constructor is needed. */
          class_copy_case = TRUE;
          eff_param_type = skip_typerefs(arg_type);
        }  /* if */
      }  /* if */
      if (class_copy_case && !param_is_reference && !pointer_case) {
        /* See if a copy constructor is needed for a class copy. */
        set_user_conversion_for_class_copy(&arg_operand->operand,
                                           &arg_match->conversion,
                                           eff_param_type);
      }  /* if */
    }  /* if */
  }  /* for */
  if (arg_operand != NULL) {
    /* We ran out of parameters, but we still have arguments.  There should
       be an ellipsis. */
#if CHECKING
    if (!rtsp->has_ellipsis) {
      internal_error(
                   "function_template_matches_operand_list: missing ellipsis");
    }  /* if */
#endif /* CHECKING */
    /* An ellipsis match is not an exact match, so this template cannot be
       used.  We could allow an extension in this area, but we don't. */
    goto done;
#if CHECKING
  } else if (ptp != NULL) {
    /* We ran out of arguments, but we still have parameters.  The parameter
       should have a default argument expression. */
    if (!ptp->has_default_arg) {
      internal_error(
           "function_template_matches_operand_list: missing default arg expr");
    }  /* if */
#endif /* CHECKING */
  }  /* if */
  /* The function template matches the operand list. */
  matches = TRUE;
  cfp->template_arg_list = templ_arg_list;
done:
  if (!matches) {
    /* Free the template argument list if we will not use it. */
    free_template_arg_list(templ_arg_list);
  }  /* if */
  db_exit();
  return matches;
}  /* function_template_matches_operand_list */


static int compare_argument_tiebreakers(a_candidate_function_ptr cfp1,
                                        a_candidate_function_ptr cfp2)
/*
Compare the argument matches of the indicated candidate function calls,
which are equally good overall so far, and look for tie-breakers.
Return 

  +1 if cfp1 is better than cfp2,
   0 if cfp1 and cfp2 are equally good, or
  -1 if cfp1 is worse than cfp2.

This checks for the const/volatile tie-breaker of rule [1] in ARM 13.2.
*/
{
  int                      cmp = 0;
  an_arg_match_summary_ptr arg1, arg2;
  a_type_ptr               param_type1, param_type2, under_type1, under_type2;

  /* We're looking for cases like
       void f(const int *);
       void f(      int *);
       int *p;
       main () {
         f(p);  // Picks f(int *); f(const int *) is worse because it adds
                // type qualifiers under a pointer or reference.
       }
  */
  /* Compare each argument. */
  for (arg1 = cfp1->arg_matches, arg2 = cfp2->arg_matches;
       arg1 != NULL;
       arg1 = arg1->next, arg2 = arg2->next) {
    check_assertion(arg2 != NULL);
    if (arg1->conversion.std.type_qualifiers_added !=
        arg2->conversion.std.type_qualifiers_added) {
      /* There is the possibility that a tie-breaker applies on this pair
         of arguments.  An additional test is needed: the tie-breaker
         really has to do with one sequence being a subsequence of the
         other, so make sure that the destination types are compatible. */
      /* Get the corresponding parameter types. */
      param_type1 = arg1->param_type;
      param_type2 = arg2->param_type;
      /* Some arguments have no parameter type (e.g., an ellipsis match). */
      if (param_type1 != NULL && param_type2 != NULL) {
        /* Note that the test here allows one to be a pointer, the other a
           reference, for some cases that compare a "this" parameter
           pointer match with a reference match.  This seems to be common
           practice for some other cases as well; there's a pointer/reference
           case in the NIH library. */
        if (is_ptr_or_ref_type(param_type1) &&
            is_ptr_or_ref_type(param_type2)) {
          /* Both parameters are pointers or references. */
          under_type1 = type_pointed_to(param_type1);
          under_type2 = type_pointed_to(param_type2);
          if (any_cfront_mode()) {
            /* In cfront mode, ignore arrays under references.  There's a
               case like that in the NIH libraries. */
            if (is_reference_type(param_type1) && is_array_type(under_type1)) {
              under_type1 = underlying_array_element_type(under_type1);
            }  /* if */
            if (is_reference_type(param_type2) && is_array_type(under_type2)) {
              under_type2 = underlying_array_element_type(under_type2);
            }  /* if */
          }  /* if */
          if (types_are_compatible_ignoring_qualifiers(under_type1,
                                                       under_type2)) {
            /* The underlying types are the same, so tie-breaker differences
               are subsequence differences. */
            int prev_cmp = cmp;
            if (arg1->conversion.std.type_qualifiers_added) {
              /* Argument 1 has added type qualifiers and argument 2 does not,
                 so arg_match2 is the better match. */
              cmp = -1;
            } else {
              /* Argument 2 has added type qualifiers and argument 1 does not,
                 so arg_match1 is the better match. */
              cmp = 1;
            }  /* if */
            /* This tie-breaker applies only if no other arguments contradict
               it, so keep going and look at the rest of the arguments. */
            if (prev_cmp != 0 && prev_cmp != cmp) {
              /* This contradicts a previous argument, so the tie-breaker does
                 not apply. */
              cmp = 0;
              break;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  return cmp;
}  /* compare_argument_tiebreakers */


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
  int cmp;

  /* Note that the tests here must be ordered from most significant
     to least significant. */
  if ((cmp = compare_argument_tiebreakers(cfp1, cfp2)) != 0) {
    /* There is something about one argument list that makes it better
       than the other. */
  } else if (cfp1->is_user_conversion &&
             cfp1->conversion.std.nontrivial_conversion !=
             cfp2->conversion.std.nontrivial_conversion) {
    /* The fact that a standard conversion is needed after a conversion
       function can serve as a tie-breaker. */
    if (cfp1->conversion.std.nontrivial_conversion) {
      /* A standard conversion is needed after cfp1 and none is needed
         after cfp2, so cfp2 is better. */
      cmp = -1;
    } else {
      /* A standard conversion is needed after cfp2 and none is needed
         after cfp1, so cfp1 is better. */
      cmp = 1;
    }  /* if */
  } else if (cfp1->is_user_conversion &&
             cfp1->conversion.std.type_qualifiers_added !=
             cfp2->conversion.std.type_qualifiers_added) {
    /* The fact that type qualifiers were added after a conversion
       function can serve as a tie-breaker. */
    if (cfp1->conversion.std.type_qualifiers_added) {
      /* Type qualifiers were added on cfp1 and not on cfp2, so cfp2 is
         better. */
      cmp = -1;
    } else {
      /* Type qualifiers were added on cfp2 and not on cfp1, so cfp1 is
         better. */
      cmp = 1;
    }  /* if */
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
  } else {
    /* Use of the const anachronism (calling a const function for a
       non-const object) can break a tie.  This must be tested last. */
    a_boolean const_anachr1 = FALSE, const_anachr2 = FALSE;

    if (cfp1->arg_matches != NULL) {
      const_anachr1 = cfp1->arg_matches->const_anachronism;
    }  /* if */
    if (cfp2->arg_matches != NULL) {
      const_anachr2 = cfp2->arg_matches->const_anachronism;
    }  /* if */
    if (const_anachr1 != const_anachr2) {
      if (const_anachr1) {
        /* cfp1 uses the const anachronism and cfp2 does not, so cfp2
           is better. */
        cmp = -1;
      } else {
        /* cfp2 uses the const anachronism and cfp1 does not, so cfp1
           is better. */
        cmp = 1;
      }  /* if */
    }  /* if */
  }  /* if */
  return cmp;
}  /* compare_candidate_functions */


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
    /* Skip the chosen function itself. */
    if (cfp != best_cfp) {
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
                        a_boolean                *undecidable_because_of_error)
/*
*candidate_functions is the list of viable functions for a particular
overloaded function call.  From that set, select the best functions
and set *candidate_functions to that set.  Other candidate functions
that do not make the "best" set are freed.  On return from this function,
the *candidate_functions list has no elements if there are no viable
functions, has more than one element if the call is ambiguous, and
has exactly one member if the call is valid.  *source_pos is the source
position of the reference.  If the best functions could not be selected
because there were error arguments in the matches,
*undecidable_because_of_error is returned TRUE and *candidate_functions
is set to NULL.
*/
{
  a_candidate_function_ptr candidates = *candidate_functions;
  a_candidate_function_ptr cfp, best_cfp, end_candidate_functions, cfp_next;
  unsigned long            number_in_best_match_set;
  an_arg_match_summary_ptr best_match_for_curr_arg, curr_arg;
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
  /* See if there are any function templates.  Try matching them to the
     arguments.  Remove those that cannot be made to match from the candidate
     functions list (by rebuilding the list as we go through it).  The rest
     go on to participate in the general algorithm below. */
  *candidate_functions = end_candidate_functions = NULL;
  for (cfp = candidates; cfp != NULL; cfp = cfp_next) {
    cfp_next = cfp->next;
    cfp->next = NULL;
    if (cfp->is_function_template &&
        !function_template_matches_operand_list(cfp)) {
      /* A function template that cannot be made to match.  Free it instead
         of keeping it on the list.  Note that this call only frees one
         entry because the "next" pointer has been cleared. */
      free_candidate_function_list(cfp);
    } else {
      /* A non-template function, or a template function that can be made
         to match the operands we have.  Keep it on the list. */
      if (end_candidate_functions == NULL) {
        *candidate_functions = cfp;
      } else {
        end_candidate_functions->next = cfp;
      }  /* if */
      end_candidate_functions = cfp;
    }  /* if */
  }  /* for */
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
    /* Note that templates that could be made to match above now participate
       in the general overload resolution.  There's a tie-breaker that makes
       them worse than an otherwise-equivalent non-template case. */
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
    }  /* for */
    /* Loop for each argument. */
    while (candidates->current_arg_match != NULL) {
      /* Find the best-match set for this argument. */
      best_match_for_curr_arg = NULL;
      /* Loop for each candidate function.  Look at the current argument
         under each function to find the best matches. */
      for (cfp = candidates; cfp != NULL; cfp = cfp->next) {
        curr_arg = cfp->current_arg_match;
        /* Ignore error matches in finding the best matches (but they get
           added to the best-match set below). */
        if (curr_arg->match_level == aml_error) {
          any_error_match = TRUE;
        } else {
          if (best_match_for_curr_arg == NULL) {
            /* First match considered for this argument.  It's the best
               so far by definition. */
            cmp = 1;
          } else {
            /* Compare the current argument match level against the best
               match so far. */
            cmp = compare_arg_match_levels(curr_arg, best_match_for_curr_arg);
          }  /* if */
          if (cmp < 0) {
            /* The argument match being examined is not as good as the best
               match so far.  Ignore it. */
            cfp->prev_func_arg_match_with_same_match_level = NULL;
          } else {
            /* The argument match being examined is at least as good as the
               best match so far. */
            if (cmp > 0) {
              /* The argument match being examined is better than any seen
                 so far.  Remember it as the best so far. */
              best_match_for_curr_arg = curr_arg;
            }  /* if */
            /* Remember that this argument match is a member of a best-match
               set, at least at the moment. */
            cfp->prev_func_arg_match_with_same_match_level =
                                                       best_match_for_curr_arg;
          }  /* if */
        }  /* if */
      }  /* for */
      /* Here, the current argument matches with 
         prev_func_arg_match_with_same_match_level == best_match_for_curr_arg
         are the best-match set for the current argument. */
      /* Loop through the functions and form the intersection of the
         best-match set for this argument and the overall best-match
         set to date. */
      for (cfp = candidates; cfp != NULL; cfp = cfp->next) {
        /* Also keep functions with error matches in the best-match set. */
        curr_arg = cfp->current_arg_match;
        if (cfp->prev_func_arg_match_with_same_match_level ==
                                                     best_match_for_curr_arg ||
            curr_arg->match_level == aml_error) {
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
  if (candidates != NULL &&
      candidates->is_function_template &&
      candidates->next == NULL) {
    /* A single candidate function template was unambiguously selected.
       Create the template function instance. */
    candidates->function_symbol =
                         find_template_function(candidates->function_symbol,
                                                &candidates->template_arg_list,
                                                source_pos);
    candidates->is_function_template = FALSE;
  }  /* if */
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "Return from select_best_candidate_functions: ");
    db_candidate_function_list(candidates);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* select_best_candidate_functions */


a_symbol_ptr select_overloaded_function(
                           a_symbol_ptr             overloaded_function_symbol,
                           a_boolean                have_selector,
                           an_operand               *bound_function_selector,
                           an_arg_operand_ptr       arg_operand_list,
                           an_error_code            err_none_applies,
                           an_error_code            err_ambiguous,
                           a_source_position        *call_position,
                           an_arg_match_summary_ptr *arg_match_list)
/*
Determine which of the functions under overloaded_function_symbol should
be called given an argument list arg_operand_list.  The symbol may be an
overloaded function, a simple member or nonmember function, or a projection
symbol for one of those.  If have_selector is TRUE, *bound_function_selector
is a selector object.  Note that, for constructor calls,
bound_function_selector can be NULL when have_selector is TRUE; we have a
selector, but it's not available.  That's okay for constructors, because
they cannot be const- or volatile-qualified, and the selector expression
is only needed for that discrimination.  call_position is the source
position of the call.  If an error of some sort is detected, issue an
error at that position and return NULL.  err_none_applies is the error
code to use when no function applies, and err_ambiguous is the error code
to use when more than one function applies.  If there is no error,
an argument match list is returned in *arg_match_list (the caller must
free this) and the symbol selected is returned.  This routine is called
only in C++ mode.
*/
{
  a_candidate_function_ptr candidate_functions;
  a_symbol_ptr             function_symbol;
  a_boolean                matched_except_for_missing_selector = FALSE;
  a_boolean                undecidable_because_of_error;

  db_enter(4, "select_overloaded_function");
  /* candidate_functions will contain the list of viable functions. */
  candidate_functions = NULL;
  /* Evaluate all matches in the function set. */
  try_overloaded_function_match(overloaded_function_symbol,
                                arg_operand_list,
                                have_selector,
                                bound_function_selector,
                                /*selector_is_object_pointer=*/TRUE,
                                /*user_conversion_case=*/FALSE,
                                &candidate_functions,
                                &matched_except_for_missing_selector);
  /* The candidate_functions list now contains all the viable functions.
     Find the best one(s). */
  select_best_candidate_functions(&candidate_functions, call_position,
                                  &undecidable_because_of_error);
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
    } else {
      /* Normal case. */
      pos_sy_error(err_none_applies, call_position,
                   overloaded_function_symbol);
    }  /* if */
  } else if (candidate_functions->next != NULL) {
    /* More than one function applies and is a best match -- ambiguity. */
#if DEBUG
    if (debug_level >= 4) {
      db_candidate_function_list(candidate_functions);
    }  /* if */
#endif /* DEBUG */
    pos_sy_start_error(err_ambiguous, call_position,
                       overloaded_function_symbol);
    diagnose_overload_ambiguity(candidate_functions, (an_opname_kind)onk_none);
  } else {
    /* Exactly one function applies and is best. */
    function_symbol = candidate_functions->function_symbol;
    *arg_match_list = candidate_functions->arg_matches;
    /* Prevent freeing of the arg_match_list when the candidate_functions
       list is freed. */
    candidate_functions->arg_matches = NULL;
#if DEBUG
    if (debug_level >= 4) {
      db_symbol(function_symbol, "select_overloaded_function: selected ", 2); 
    }  /* if */
#endif /* DEBUG */
  }  /* if */
  /* Free the candidate functions list. */
  free_candidate_function_list(candidate_functions);
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


static a_type_ptr operand_complete_object_type(an_operand *operand,
                                               a_boolean  call_case)
/*
Return the type of the complete object that contains the location indicated
by operand (an address), or NULL if no complete object can be determined.
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
    if (operand_complete_object_type(bound_function_selector,
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
                                  a_source_position *call_position,
                                  a_boolean         elided_reference,
                                  an_operand        *operand,
                                  a_boolean         *access_error_reported)
/*
We've just determined which specific function within a set of overloaded
functions is being referenced, i.e., function_symbol is being called
within the set given by overloaded_function_symbol.  (function_symbol
may not be an overloaded function or a projection symbol.)  Do whatever
would have been done with the function along the way if we had known all
along which specific function was intended.  That is, "catch up" with the
processing that would have been done up to this point for a non-overloaded
function.  While this is usually used for overloaded functions, it is
also used for a few cases where the function is not overloaded but it
is not convenient to note that fact before scanning the arguments (e.g.,
operator overloading).  Therefore, while overloaded_function_symbol is
typically an sk_overloaded_function containing function_symbol, it may
be the same as function_symbol, or it may be a projection symbol
for one of those.  Generate an operand for a pointer to the specific
function in *operand.  call_position is used as the source position for
that operand.  is_qualified_name is TRUE if a qualified name was used to
name the function (that suppresses the virtual-ness of the function).
Access control and ambiguity checking are always done, even if the
overloaded_function_symbol is a non-overloaded function.  operand can be
NULL if it is not necessary to generate the function designator operand.
elided_reference is TRUE if the routine was referenced in the program
but the reference is being elided in the intermediate language (operand
should be NULL in that case).  On return, *access_error_reported is TRUE
if an access control checking error was detected.
*/
{
  a_symbol_locator function_symbol_locator;
  a_ref_entry_ptr  rep;

#if CHECKING
  /* Overloaded functions and projection symbols are not allowed for
     function_symbol. */
  if (function_symbol->kind != (a_symbol_kind)sk_routine &&
      function_symbol->kind != (a_symbol_kind)sk_member_function) {
    internal_error("overloaded_function_catch_up: bad function_symbol");
  }  /* if */
#endif /* CHECKING */
  /* Check ambiguity and access. */
  if (fundamental_symbol_of(overloaded_function_symbol)->kind ==
                                       (a_symbol_kind)sk_overloaded_function) {
    /* Use a special routine for overloaded functions because (a) overloaded
       functions are considered always accessible when checked through the
       normal routine and (b) the projection symbol here may point to the
       overloaded function symbol rather than to the specific function
       symbol. */
    make_locator_for_symbol(function_symbol, &function_symbol_locator);
    function_symbol_locator.source_position = *call_position;
    overload_check_ambiguity_and_verify_access(&function_symbol_locator,
                                               overloaded_function_symbol);
  } else {
    /* Non-overloaded function; use the normal routine.
       overloaded_function_symbol is either the same as function_symbol
       or is a projection symbol for it. */
    make_locator_for_symbol(overloaded_function_symbol,
                            &function_symbol_locator);
    function_symbol_locator.source_position = *call_position;
    check_ambiguity_and_verify_access(&function_symbol_locator);
  }  /* if */
  *access_error_reported =
                         function_symbol_locator.access_control_error_reported;
  if (is_error_locator(function_symbol_locator)) {
    /* An error locator is returned for an ambiguous case. */
    if (operand != NULL) {
      make_error_operand(operand);
      operand->position = *call_position;
    }  /* if */
  } else {
    if (elided_reference) {
      /* The reference to the routine was elided (e.g., in a "new" where the
         "new" call can be folded into a constructor call).  Mark the symbol
         as referenced, but not the IL entry. */
      check_assertion(operand == NULL);
      record_symbol_reference(SRK_REFERENCE, function_symbol, call_position,
                              /*update_il_entry=*/FALSE);
    } else {
      /* The reference is not elided. */
      if (operand == NULL) {
        /* We don't want an operand, presumably because the function is
           being used in some unusual way and the caller will build the
           operand.  Mark the function as referenced.  Note that we are
           ignoring whether or not the function is virtual; we are assuming
           that the reference is to exactly that function. */
        if_evaluating_mark_routine_referenced(function_symbol->
                                                         variant.routine.ptr);
      } else {
        /* Normal case: build an operand for the function. */
        /* Record that the function was referenced, for cross-reference (etc.)
           purposes. */
        rep = ref_entry(function_symbol, call_position);
        make_function_designator_operand(function_symbol, is_qualified_name,
                                         call_position, rep, operand);
        /* Convert the operand to a function pointer. */
        conv_function_designator_to_ptr_to_function(operand);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* overloaded_function_catch_up */


static a_type_ptr underlying_selector_class(an_operand *selector)
/*
Extract and return the class type underlying the given selector.  If the
class type cannot be determined, return NULL.  If there is an error in
the selector, return an error type.  The "underlying type" is the class
type of the object or pointer used to make the selection; any implicit
base class casts on top of that object are ignored in determining the
underlying type.
*/
{
  a_type_ptr       underlying_type = NULL;
  an_expr_node_ptr expr;
  a_constant_ptr   con;

  switch (selector->kind) {
    case ok_expression:
      /* Expression.  Drop implicit base class casts. */
      for (expr = selector->variant.expression;
           is_operation_node(expr) &&
             expr->variant.operation.kind ==
                                  (an_expr_operator_kind)eok_base_class_cast &&
             expr->variant.operation.compiler_generated;
           expr = expr->variant.operation.operands) {}
      if (is_pointer_type(expr->type)) {
        underlying_type = type_pointed_to(expr->type);
      }  /* if */
      break;
    case ok_constant:
      /* Constant.  See if it is the address of a variable. */
      con = &selector->variant.constant;
      if (con_is_exact_addr_of_variable(con)) {
        underlying_type = con->variant.address.variant.variable->type;
      }  /* if */
      break;
    case ok_error:
      break;
#if CHECKING
    default:
      internal_error("underlying_selector_class: bad operand kind");
#endif /* CHECKING */
  }  /* switch */
  if (underlying_type != NULL) {
    underlying_type = skip_typerefs(underlying_type);
#if CHECKING
    if (!is_immediate_class_type(underlying_type) &&
        !is_error_type(underlying_type)) {
      internal_error("underlying_selector_class: bad underlying type");
    }  /* if */
#endif /* CHECKING */
  }  /* if */
  return underlying_type;
}  /* underlying_selector_class */


static void f_catch_up_check_protected_member_access(
                                   a_symbol_ptr      sym,
                                   an_operand        *bound_function_selector,
                                   a_source_position *err_pos)
/*
This routine implements the access control check mandated by ARM 11.5, which
requires that a protected member be accessed only through a pointer or
object of a type to which we have member access.  This routine is used
specifically for the case where an overloaded function is a protected
member.  After overloaded_function_catch_up, this routine is called to
do the catch-up on the protected check.  sym points to the symbol for
the function being referenced.  *bound_function_selector is the selector
being used to access the function.  *err_pos is the source position for
an error.
*/
{
  a_type_ptr       class_type;

  /* Get the underlying type from the selector. */
  class_type = underlying_selector_class(bound_function_selector);
  /* Do the access check. */
  f_check_protected_member_access(sym, err_pos, class_type);
}  /* f_catch_up_check_protected_member_access */


/*
If sym is a protected member, do the access check of ARM 11.5.  sym
is being accessed through the selector "selector".  *err_pos is the
source position for an error.  This is being done after
overloaded_function_catch_up.
*/
#define catch_up_check_protected_member_access(sym, selector, err_pos)\
{ if (access_for_symbol(fundamental_symbol_of(sym)) ==                \
                                (an_access_specifier)as_protected) {  \
    f_catch_up_check_protected_member_access(sym, selector, err_pos); \
  }  /* if */                                                         \
}  /* catch_up_check_protected_member_access */


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


void make_this_variable_operand(a_variable_ptr this_var,
                                an_operand     *result)
/*
Make an operand for the value of the "this" variable this_var.  The source
position of the operand is set to "pos_curr_token".  The operand is an
rvalue.
*/
{
  an_expr_node_ptr node;

  /* Make a variable value node for the variable. */
  node = var_rvalue_expr(this_var);
  /* Make an operand for the node. */
  make_expression_operand(node, node->type, result);
}  /* make_this_variable_operand */


a_boolean make_this_pointer_operand(a_symbol_ptr      member_sym,
                                    a_source_position *member_pos,
                                    a_boolean         check_cast_access,
                                    an_operand        *result)
/*
Make an operand for the "this" pointer of a C++ nonstatic member function.
The operand made is an rvalue for the value of the pointer.  If we are not
currently in a nonstatic member function, issue an error and return an
error operand.  member_sym is the member, or is the overloaded function
symbol that contains the member, or it can be a projection symbol
for either of those.  The "this" pointer is cast (if necessary)
to the base class in which that member is defined.  Access checking is
done on that cast if check_cast_access is TRUE.  If the symbol is a
member of an unrelated class, issue an error and return an error operand.
member_pos is the position of the member, for use in errors and as the
source position of the result operand.  Return TRUE if the "this"
operand was built without error.  This routine is called only in
C++ mode.
*/
{
  a_variable_ptr   this_var;
  a_type_ptr       member_class, this_class;
  a_boolean        is_arrow_operator = TRUE, okay;
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
      member_class = member_sym->class_of_which_a_member;
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
      }  /* if */
    }  /* if */
    if (!okay) {
      /* The "this" pointer cannot be used (it doesn't exist or it has
         no relationship to the member). */
      pos_error(ec_member_ref_requires_object, member_pos);
      make_error_operand(result);
    } else {
      /* The "this" pointer can be used to access the member. */
      /* Note that no ARM 11.5 protected member access check is needed, because
         an access through "this" is always acceptable under the rules in
         that section. */
      /* Make an operand for the value of the "this" pointer. */
      make_this_variable_operand(this_var, result);
      /* Get position right in case of errors below. */
      result->position = *member_pos;
      if (bcp != NULL) {
        /* Cast the pointer to the base class of the member. */
        base_class_cast_operand(result, bcp, &is_arrow_operator,
                                check_cast_access,
                                /*implicit_in_naming=*/FALSE);
      }  /* if */
      /* If the member symbol is a projection symbol (i.e., it's inherited
         into the class where it is being referenced), cast the left operand
         down to the base class in which the fundamental symbol is defined.
         There's no access check on this part of the cast because the access
         to the fundamental base class was checked as part of determining
         access to the symbol. */
      if (member_sym->kind == (a_symbol_kind)sk_projection) {
        bcp= member_sym->variant.projection.extra_info->fundamental_base_class;
        base_class_cast_operand(result, bcp, &is_arrow_operator,
                                /*check_cast_access=*/FALSE,
                                /*implicit_in_naming=*/TRUE);
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
                                 a_source_position  *call_position,
                                 an_operand         *function_operand)
/*
Overload resolution has been done, and it has been decided that, of the
functions in overloaded_function_symbol (which may be a projection symbol
and/or just a simple function), function_symbol is the specific function to
be called (and not a projection symbol).  Create a function designator
operand for the function in *function_operand.  The function was named with
a qualified name if is_qualified_name is TRUE.  The reference has an
associated selector object if *have_selector is TRUE; in that case,
bound_function_selector gives the object, and function_operand is bound to
that object.  Even when *have_selector is FALSE going in,
bound_function_selector must point at an operand that can be filled in
if an implicit selector is generated (*have_selector is set to TRUE for that
case).  call_position gives the source position of the call.
*/
{                                 
  a_boolean access_error_reported;

  /* Do whatever would have been done to the function had we known
     originally which specific function was intended. */
  overloaded_function_catch_up(function_symbol,
                               overloaded_function_symbol,
                               is_qualified_name,
                               call_position,
                               /*elided_reference=*/FALSE,
                               function_operand,
                               &access_error_reported);
  /* Change the kind of reference to the function from "address taken"
     to "reference". */
  change_some_ref_kinds(function_operand->ref_entries_list, SRK_ADDRESS_TAKEN,
                        SRK_REFERENCE);
  /* Check whether or not a selector is needed. */
  if (routine_type_is_nonstatic_member_function(
                                       routine_symbol_type(function_symbol))) {
    /* The function needs a selector. */
    if (!*have_selector) {
      /* Try to generate a selector. */
      if (make_this_pointer_operand(overloaded_function_symbol, /* sic */
                                    call_position,
                                    /*check_cast_access=*/
                                       !function_operand->
                                         access_control_error_reported,
                                    bound_function_selector)) {
        /* The selector was generated without problem. */
      } else {
        /* There was some problem in generating the selector. */
        conv_to_error_operand(function_operand);
      }  /* if */
      *have_selector = TRUE;
    } else {
      /* We have a selector. */
      if (!access_error_reported) {
        /* Do the ARM 11.5 access checking for the type of selector used
           to access a protected member. */
        catch_up_check_protected_member_access(function_symbol,
                                               bound_function_selector,
                                               call_position);
      }  /* if */
    }  /* if */
    /* Bind the function to the selector. */
    bind_member_function_operand_to_selector(function_operand,
                                             bound_function_selector);
  } else {
    /* The routine does not need a selector. */
    if (*have_selector) {
      /* Discard the selector provided. */
      discard_operand(bound_function_selector);
      *have_selector = FALSE;
    }  /* if */
  }  /* if */
}  /* make_resolved_overloaded_function_operand */


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
under an ellipsis or old-style function.
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
                                      a_param_type_ptr         param)
/*
arg_operand represents an argument to an overloaded function call (including
operator cases); the call has now been resolved to a specific function.
arg_match indicates how well the actual argument matches the formal parameter,
which is described by param.  Cast the argument value to the proper type,
convert it to expression form, and return a pointer to the expression.
arg_operand can be NULL to indicate that we've run out of actual
arguments (default argument values will be used).  param can be NULL
to indicate that we've run out of parameters (remaining arguments will
be processed under an ellipsis).
*/
{
  an_expr_node_ptr arg;

  if (arg_operand == NULL) {
    /* Match uses a default argument value.  Get it from the parameter type
       entry. */
#if CHECKING
    if (param == NULL) {
      internal_error(
    "node_for_arg_of_overloaded_function_call: missing param for default arg");
    }  /* if */
#endif /* CHECKING */
    arg = param->default_arg_expr;
    if (arg != NULL) {
      arg = copy_expr_tree(arg);
    } else {
      /* In cases where there was an error in the declaration of a function
         template (a parameter with an default argument expression was
         followed by one without), put in an error node for the default
         expression for the parameter without one. */
      arg = error_node();
    }  /* if */
  } else {
    /* Actual argument is present (normal case). */
    /* Issue any warning about the conversion detected while evaluating the
       alternatives. */
    issue_warning_from_arg_match_summary(arg_match,
                                         &arg_operand->operand.position);
    /* Cast the argument to the right type. */
    prep_possible_ellipsis_argument_operand(&arg_operand->operand, param,
                                            &arg_match->conversion);
    arg = make_node_from_operand(&arg_operand->operand);
  }  /* if */
  return arg;
}  /* node_for_arg_of_overloaded_function_call */


void adjust_overloaded_function_call_arguments(
                             a_symbol_ptr             function_symbol,
                             a_boolean                have_selector,
                             an_operand               *bound_function_selector,
                             an_arg_operand_ptr       arg_operand_list,
                             an_arg_match_summary_ptr arg_match_list,
                             an_expr_node_ptr         *arg_expr_list)
/*
Overload resolution has been done, and it has been decided that the function
identified by function_symbol is the specific function to be called for
the argument list given by arg_operand_list.  If have_selector is TRUE,
there is also a selector object, given by bound_function_selector (or,
as a special case, bound_function_selector can be NULL for a constructor
case; we have a selector, but we don't know what it is).  Adjust the
selector object and arguments to the proper types, issue any warnings
detected on those arguments during the overload resolution process, and
return a list of argument expressions in *arg_expr_list.  arg_match_list
gives the argument match summaries for the selector object and the
arguments.  arg_operand_list and arg_match_list are freed.
function_symbol can be NULL to indicate that the overload resolution
failed; in that case, this routine does nothing except for freeing the
lists.  This routine is used for cases that look like calls (i.e.,
they have argument lists in parentheses); it is not used for
overloaded operator cases.
*/
{
  a_type_ptr               routine_type;
  a_boolean                old_style_function;
  an_arg_match_summary_ptr arg_match;
  an_arg_operand_ptr       arg_operand;
  an_expr_node_ptr         arg, prev_arg;
  a_param_type_ptr         param;

  /* If there was an error, skip the processing except for freeing the
     lists. */
  if (function_symbol != NULL) {
    routine_type = routine_symbol_type(function_symbol);
    old_style_function = !routine_type->variant.routine.extra_info->prototyped;
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
      arg = node_for_arg_of_overloaded_function_call(arg_operand, arg_match,
                                                     param);
      /* If the function is an old-style unprototyped function (an anachronism;
         yes, they can participate in overloading), promote the argument
         value if necessary (e.g., short --> int). */
      if (old_style_function) {
        cast_node(&arg, default_argument_promotion(arg->type),
                  /*is_implicit_cast=*/TRUE, &arg_operand->operand.position);
      }  /* if */
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


a_symbol_ptr select_and_prepare_to_call_overloaded_function(
                           a_symbol_ptr             overloaded_function_symbol,
                           a_boolean                have_selector,
                           an_operand               *bound_function_selector,
                           an_arg_operand_ptr       arg_operand_list,
                           a_boolean                is_qualified_name,
                           an_error_code            err_none_applies,
                           an_error_code            err_ambiguous,
                           a_source_position        *call_position,
                           an_operand               *function_operand,
                           an_expr_node_ptr         *arg_expr_list)
/*
Determine which of the functions under overloaded_function_symbol should
be called given an argument list arg_operand_list.  The symbol may be an
overloaded function, a simple member or nonmember function, or a projection
symbol for one of those.  If have_selector is TRUE, *bound_function_selector
is a selector object.  Note that, for constructor calls,
bound_function_selector can be NULL when have_selector is TRUE; we have a
selector, but it's not available.  That's okay for constructors, because
they cannot be const- or volatile-qualified, and the selector expression
is only needed for that discrimination.  If have_selector is FALSE,
bound_function_selector must still point at an operand that can be filled
in if an implicit selector is generated is_qualified_name is TRUE if a
qualified name was used to name the function (that suppresses the
virtual-ness of the function).  arg_operand_list is freed by this
routine.  call_position is the source position of the call.  If an
error of some sort is detected, issue an error at that position and
return NULL.  err_none_applies is the error code to use when no
function applies, and err_ambiguous is the error code to use when more
than one function applies.  If there is no error, an operand for the
function is built in *function_operand, an expression-form argument
list is built and returned in *arg_expr_list (with the arguments cast
to the proper types), and the symbol selected is returned.  This
routine is called only in C++ mode.
*/
{
  an_arg_match_summary_ptr arg_match_list;
  a_symbol_ptr             function_symbol;

  db_enter(4, "select_and_prepare_to_call_overloaded_function");
  /* Select the best function out of the overload set. */
  function_symbol = select_overloaded_function(overloaded_function_symbol,
                                               have_selector,
                                               bound_function_selector,
                                               arg_operand_list,
                                               err_none_applies,
                                               err_ambiguous,
                                               call_position,
                                               &arg_match_list);
  *arg_expr_list = NULL;
  if (function_symbol != NULL) {
    /* There was no error, i.e., a best function was chosen. */
    /* Do the things that would have been done to the symbol but weren't
       because the specific symbol was not known, and build an operand
       for the function. */
    make_resolved_overloaded_function_operand(function_symbol,
                                              overloaded_function_symbol,
                                              &have_selector,
                                              bound_function_selector,
                                              is_qualified_name,
                                              call_position,
                                              function_operand);
  }  /* if */
  /* Build an expression-form argument list.  Convert the arguments on
     the argument list to the right types.  Free arg_operand_list
     and_arg_match_list (the call is done even when function_symbol
     is NULL so that the freeing will be done). */
  adjust_overloaded_function_call_arguments(function_symbol,
                                            have_selector,
                                            bound_function_selector,
                                            arg_operand_list,
                                            arg_match_list,
                                            arg_expr_list);
  db_exit();
  return function_symbol;
}  /* select_and_prepare_to_call_overloaded_function */


static void try_conversion_function_match(
                                an_operand               *source_operand,
                                a_type_ptr               dest_type,
                                a_builtin_type_kind_set  builtin_types_allowed,
                                a_boolean                need_lvalue_result,
                                a_candidate_function_ptr *candidate_functions)
/*
See if a class operand source_operand can be converted by a conversion function
to either

(a) dest_type, if dest_type is non-NULL (a standard conversion can be
    done after the conversion function, if necessary), and an lvalue
    of that type if need_lvalue_result is TRUE, or
(b) a built-in type in the set given by builtin_types_allowed, if
    dest_type is NULL.

If a conversion function to do that conversion exists, evaluate how
well it matches the arguments and add it to the candidate_functions list,
setting "conversion" in the candidate function entry.  This routine
is only used in C++ mode.
*/
{
  a_symbol_ptr              conversion_symbol, base_conversion_symbol;
  a_routine_ptr             conversion_routine;
  a_symbol_list_entry_ptr   slep;
  a_type_ptr                source_type, conv_routine_type, return_type;
  an_arg_match_summary      this_match;
  an_arg_match_summary_ptr  this_match_ptr;
  a_std_conv_descr          std_conversion;
  a_boolean                 compatible;
  a_boolean                 result_is_an_lvalue;
  a_candidate_function_ptr  candidate;

  db_enter(4, "try_conversion_function_match");
  /* This routine is similar to try_overloaded_function_match. */
  source_type = source_operand->type;
  /* Look at all the conversion functions for the source class. */
  for (slep = symbol_supplement_for_class(source_type)->conversion_list;
       slep != NULL;
       slep = slep->next) {
    conversion_symbol = slep->symbol;
#if DEBUG
    if (debug_level >= 4) {
      db_symbol(conversion_symbol,
                "try_conversion_function_match: considering ", 2); 
    }  /* if */
#endif /* DEBUG */
    base_conversion_symbol = fundamental_symbol_of(conversion_symbol);
    conv_routine_type = routine_symbol_type(base_conversion_symbol);
    /* Is the type returned by this routine a type we want? */
    compatible = FALSE;
    return_type = conv_routine_type->variant.routine.return_type;
    /* Drop type qualifiers for the normal case, when the return value
       is an rvalue. */
    return_type = skip_typerefs(return_type);
    clear_std_conv_descr(&std_conversion);
    result_is_an_lvalue = FALSE;
    /* If the conversion function returns a reference type, drop the 
       reference. */
    if (is_reference_type(return_type)) {
      return_type = type_pointed_to(return_type);
      result_is_an_lvalue = TRUE;
      /* In this case, the return value is an lvalue, and type qualifiers
         are not dropped.  But see the comment below. */
    }  /* if */
    if (dest_type != NULL) {
      /* We're looking for a specific type. */
      if (types_are_compatible_ignoring_qualifiers(dest_type, return_type)) {
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
           to an "int" rvalue. */
        compatible = TRUE;
        if (result_is_an_lvalue) {
          if (any_qualifier_missing(dest_type, return_type)) {
            /* The function returns a reference type and the referenced type
               has more qualifiers than necessary.  Force the conversion
               of the result to an rvalue to drop the type qualifiers. */
            result_is_an_lvalue = FALSE;
          }  /* if */
        }  /* if */
      } else if (impl_conversion_possible(return_type,
                                          /*source_is_constant=*/FALSE,
                                          (a_constant_ptr)NULL, dest_type,
                                          /*suppress_extensions=*/TRUE,
                                          ec_no_error, &std_conversion)) {
        /* This conversion function returns a type that can be converted
           via a standard conversion to the type we want. */
        compatible = TRUE;
        result_is_an_lvalue = FALSE;
      }  /* if */
    } else {
      /* We're looking for a built-in type described in general terms. */
#if 0
      /* Different test for enum? */
#endif /* 0 */
      if (((builtin_types_allowed & BTK_INTEGRAL) != 0 &&
                                              is_integral_type(return_type)) ||
          ((builtin_types_allowed & BTK_FLOATING) != 0 &&
                                              is_floating_type(return_type)) ||
          ((builtin_types_allowed & BTK_POINTER) != 0 &&
                                              is_pointer_type(return_type)) ||
          ((builtin_types_allowed & BTK_PTR_TO_MEMBER) != 0 &&
                                         is_ptr_to_member_type(return_type))) {
        /* This conversion function returns an acceptable built-in type. */
        compatible = TRUE;
        /* The result does not have to be forced to an rvalue. */
      }  /* if */
    }  /* if */
    if (need_lvalue_result && !result_is_an_lvalue) {
      /* We need an lvalue result but the conversion function does
         not return one. */
      compatible = FALSE;
    }  /* if */
    if (compatible) {
      /* This conversion function meets the requirements for result type.
          However, we must also see whether or not it can be called for this
          argument (i.e., are the type qualifiers okay), and how good the
          match is. */
      conversion_routine = base_conversion_symbol->variant.routine.ptr;
      selector_match_with_this_param(source_operand,
                                     /*selector_is_object_pointer=*/FALSE,
                                     /*conversion_function_case=*/TRUE,
                                     conversion_routine,
                                     conv_routine_type,
                                     &this_match);
      /* Ignore this function if it cannot be called for this argument. */
      if (this_match.match_level == aml_none) goto next_function;
      /* The routine is viable. */
      /* Add the conversion function to the candidate functions list. */
      this_match_ptr = alloc_arg_match_summary();
      *this_match_ptr = this_match;
      add_function_to_candidate_functions_list(conversion_symbol,
                                               this_match_ptr,
                                               candidate_functions);
      candidate = *candidate_functions;
      candidate->is_user_conversion = TRUE;
      candidate->conversion.routine = conversion_routine;
      candidate->conversion.std = std_conversion;
      candidate->conversion.result_is_an_lvalue = result_is_an_lvalue;
    }  /* if */
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
    return_type = conv_routine_type->variant.routine.return_type;
    if (is_reference_type(return_type)) {
      /* Drop a reference type; a conversion function that returns "const int&"
         can be used like one that returns "const int". */
      return_type = type_pointed_to(return_type);
    }  /* if */
    return_type = skip_typerefs(return_type);
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
by semicolons, e.g., "AA;PI;IP"; each pattern has one letter (for
unary operators) or two letters (for binary operators) giving the type
code for the associated operand:
  I  Integral
  A  Arithmetic
  P  Pointer
  C  Corresponding pointer, when two pointer operands must match in type
  M  Pointer to member
The first character of the overall string is "L" if the operator requires
an lvalue as its first operand, e.g., "LAA;PI;IP".
*/
{
  char *operand_type_pattern;

  if (unary_operator) {
    switch (kind) {
      case onk_plus:
      case onk_minus:
        /* Unary "+" and "-" take an arithmetic operand. */
        operand_type_pattern = "A";
        break;
      case onk_not:
        /* "!" takes an arithmetic, pointer, or pointer-to-member operand. */
        operand_type_pattern = "A;P;M";
        break;
      case onk_compl:
        /* "~" takes an integral operand. */
        operand_type_pattern = "I";
        break;
      case onk_star:
        /* "*" takes a pointer operand. */
        operand_type_pattern = "P";
        break;
      case onk_plus_plus:
      case onk_minus_minus:
        /* "++" and "--" (prefix) take an arithmetic or pointer lvalue.
           See below for postfix (which shows up as a two-operand operator). */
        operand_type_pattern = "LA;P";
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
        /* "+" takes arith+arith, pointer+int, or int+pointer. */
        operand_type_pattern = "AA;PI;IP";
        break;
      case onk_minus:
        /* "-" takes arith-arith, pointer-int, or pointer-pointer. */
        operand_type_pattern = "AA;PI;CC";
        break;
      case onk_lt:
      case onk_le:
      case onk_gt:
      case onk_ge:
        /* Relational operators take arithmetic or pointer operands. */
        operand_type_pattern = "AA;CC";
        break;
      case onk_eq:
      case onk_ne:
        /* Equality operators take arithmetic, pointer, or pointer-to-member
           operands. */
        operand_type_pattern = "AA;CC;MM";
        break;
      case onk_and_and:
      case onk_or_or:
        /* "&&" and "||" take arithmetic, pointer, or pointer-to-member
           operands, but they can be mixed. */
        operand_type_pattern = "AA;AP;AM;PA;PP;PM;MA;MP;MM";
        break;
      case onk_times_assign:
      case onk_divide_assign:
        /* "*=" and "/=" take arithmetic operands, the first an lvalue. */
        operand_type_pattern = "LAA";
        break;
      case onk_remainder_assign:
      case onk_shift_left_assign:
      case onk_shift_right_assign:
      case onk_and_assign:
      case onk_or_assign:
      case onk_excl_or_assign:
        /* "%=", "<<=", ">>=", "&=", "|=", and "^=" take integral operands,
           the first an lvalue. */
        operand_type_pattern = "LII";
        break;
      case onk_plus_assign:
        /* "+=" takes arith+arith or pointer+int, the first an lvalue. */
        operand_type_pattern = "LAA;PI";
        break;
      case onk_minus_assign:
        /* "-=" takes arith-arith or pointer-int, the first an lvalue. */
        operand_type_pattern = "LAA;PI";
        break;
      case onk_subscript:
        /* "[]" takes pointer[int] or int[pointer]. */
        operand_type_pattern = "PI;IP";
        break;
      case onk_plus_plus:
      case onk_minus_minus:
        /* "++" and "--" (postfix, which show up as two-operand operators)
           take an arithmetic or pointer lvalue.  A second implied
           operand is integer. */
        operand_type_pattern = "LAI;PI";
        break;
      case onk_question:
        /* "?" (which shows up here as a two-operand operator) takes
           two operands (really the second and third) of arithmetic or
           pointer type (the void and class cases are handled outside of
           this routine). */
        operand_type_pattern = "AA;CC";
        break;
#if CHECKING
      default:
        internal_error("operand_type_pattern_for_operator: bad binary op");
#endif /* CHECKING */
    }  /* switch */
  } /* if */
  return operand_type_pattern;
}  /* operand_type_pattern_for_operator */


static a_builtin_type_kind_set builtin_type_set_for_type_code(char type_code)
/*
Build and return the built-in type kind set that corresponds to the indicated
type_code.
*/
{
  a_builtin_type_kind_set builtin_types_allowed = BTK_NONE;

  if (type_code == INTEGRAL_TYPE_CODE) {
    builtin_types_allowed |= BTK_INTEGRAL;
  }  /* if */
  if (type_code == ARITH_TYPE_CODE) {
    builtin_types_allowed |= BTK_INTEGRAL | BTK_FLOATING;
  }  /* if */
  /* Note that this routine is never called with CORRESP_POINTER_TYPE_CODE. */
  check_assertion(type_code != CORRESP_POINTER_TYPE_CODE);
  if (type_code == POINTER_TYPE_CODE) {
    builtin_types_allowed |= BTK_POINTER;
  }  /* if */
  if (type_code == PTR_TO_MEMBER_TYPE_CODE) {
    builtin_types_allowed |= BTK_PTR_TO_MEMBER;
  }  /* if */
  return builtin_types_allowed;
}  /* builtin_type_set_for_type_code */


static void try_builtin_operands_match(
                       char                     *operand_type_pattern,
                       an_arg_operand_ptr       arg_operand_list,
                       a_candidate_function_ptr *candidate_functions,
                       char                     *pointer_type_pattern_position,
                       a_type_ptr               pointer_type)
/*
Subroutine for try_conversions_for_builtin_operator.  Check to see how
well the operand values given by arg_operand_list match the operand type
pattern string given by operand_type_pattern.  If they match, add
the built-in operator to the candidate_functions list.
This is used for the case where the pattern string contains no corresponding
pointer types, with pointer_type_pattern_position == pointer_type == NULL,
and with those set non-NULL for pattern strings containing corresponding
pointer types (they indicate the target pointer type to be used and, to
allow a speed optimization, the pattern position which suggested that
pointer type).
*/
{
  a_boolean                okay;
  char                     type_code;
  char                     *type_pattern_position;
  an_arg_operand_ptr       arg_operand;
  an_arg_match_summary_ptr arg_match, arg_match_list, end_arg_match_list;
  a_type_ptr               operand_type;
  a_conv_descr             conversion;
  a_boolean                ambiguous;
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
  /* Go through the operands and determine the match level on each operand. */
  for (type_pattern_position = operand_type_pattern,
         arg_operand = arg_operand_list;
       arg_operand != NULL;
       type_pattern_position++, arg_operand = arg_operand->next) {
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
    /* This is different than for function argument matching.  The ARM
       is not very explicit about this.  13.2, page 314: "It follows that
       the binary operands for built-in types do not obey these ambiguity
       rules ... The problem with the (C and C++) rules for conversion of
       arithmetic types is that a value of any arithmetic type can be
       implicitly converted into any other arithmetic type." */
    type_code = *type_pattern_position;
    if (type_code != CORRESP_POINTER_TYPE_CODE) {
      /* Arithmetic or non-specific pointer type required. */
      if (is_class_struct_union_type(operand_type)) {
        /* The operand has a class type, so see if it can be converted to
           an appropriate built-in type. */
        if (conversion_from_class_possible(&arg_operand->operand,
                                           (a_type_ptr)NULL,
                                     builtin_type_set_for_type_code(type_code),
                                           /*need_lvalue_result=*/FALSE,
                                           &conversion,
                                           &ambiguous,
                                           (a_candidate_function_ptr *)NULL) ||
            ambiguous) {
          /* The conversion can be done. */
          arg_match->match_level = aml_user_conversion;
          arg_match->conversion = conversion;
        }  /* if */
      } else {
        /* A non-class operand.  See if it has (or can be converted to)
           the required type. */
        /* Note that we do not try 0 --> pointer, because in this case it
           would require just making up a pointer type out of nowhere --
           there's no other operand that provides guidance on which pointer
           type is required. */
        /* Do array --> pointer and function --> pointer transformations. */
        operand_type = do_implicit_type_transformations(operand_type,
                                                        &arg_operand->operand);
#if 0
        /* Enum? */
#endif /* 0 */
        if ((type_code == INTEGRAL_TYPE_CODE && 
                                             is_integral_type(operand_type)) ||
            (type_code == ARITH_TYPE_CODE && 
                                           is_arithmetic_type(operand_type)) ||
            (type_code == POINTER_TYPE_CODE &&
                                              is_pointer_type(operand_type)) ||
            (type_code == PTR_TO_MEMBER_TYPE_CODE &&
                                        is_ptr_to_member_type(operand_type))) {
          /* The type is correct, within the range allowed by a standard
             conversion.  We count the cost as a standard conversion even if
             it might be more properly considered an exact match or
             promotion.  This is unspecified by the ARM.  cfront 2.1, Borland
             3.1, and Zortech 3.1 seem to do it this way.  cfront 3.0.1 and
             Microsoft do it differently. */
          arg_match->match_level = aml_std_conversion;
        }  /* if */
      }  /* if */
    } else {
      /* A specific pointer type is required.  Check that the operand
         can be converted to the pointer type passed in. */
      if (is_class_struct_union_type(operand_type)) {
        /* The operand has a class type, so see if it can be converted to
           the pointer type. */
        /* If this operand is the one that suggested this pointer type,
           we already know it is compatible.  However, we still have to
           call conversion_from_class_possible to get the conversion field
           set in the arg_match entry. */
        if (conversion_from_class_possible(&arg_operand->operand, pointer_type,
                                           (a_builtin_type_kind_set)BTK_NONE,
                                           /*need_lvalue_result=*/FALSE,
                                           &conversion,
                                           &ambiguous,
                                           (a_candidate_function_ptr *)NULL) ||
            ambiguous) {
          /* The conversion can be done with a conversion function. */
          arg_match->match_level = aml_user_conversion;
          arg_match->conversion = conversion;
          arg_match->param_type = pointer_type;
        }  /* if */
      } else {
        a_std_conv_descr std_conv;
        /* A non-class operand. */
        /* Do array --> pointer and function --> pointer transformations. */
        operand_type = do_implicit_type_transformations(operand_type,
                                                        &arg_operand->operand);
        /* If this operand is the one that suggested this pointer type,
           we already know it is compatible.  This is a speed optimization. */
        if (pointer_type_pattern_position == type_pattern_position ||
            impl_pointer_conversion(operand_type,
                                    is_constant_operand(&arg_operand->operand),
                                    &arg_operand->operand.variant.constant,
                                    pointer_type,
                                    /*check_as_operands_not_conversion=*/TRUE,
                                    /*suppress_extensions=*/TRUE,
                                    ec_no_error, /* arbitrary */
                                    &std_conv)) {
          /* The conversion can be done. */
          /* As noted above, any match here is considered a standard
             conversion. */
          arg_match->match_level = aml_std_conversion;
          arg_match->param_type = pointer_type;
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
                                                     pointer_type,
                                                     arg_match_list,
                                                     candidate_functions);
  } else {
    /* The built-in operator cannot be used. */
    free_arg_match_summary_list(arg_match_list);
  }  /* if */
}  /* try_builtin_operands_match */


static a_boolean pointer_type_previously_handled(
                                   a_type_ptr pointer_type,
                                   a_type_ptr class_type,
                                   a_type_ptr previous_class_type_considered,
                                   a_type_ptr previous_pointer_type_considered)
/*
Helper routine for try_pointer_builtin_operands_match.  Return TRUE if
pointer_type has already been tried as a target pointer type.
class_type, if non-NULL, indicates the current operand class type.
previous_class_type_considered, if non-NULL, indicates the class type of
a previous operand converted to pointer; previous_pointer_type_considered,
if non-NULL, indicates the pointer type of a previous non-class operand.
*/
{
  a_boolean previously_handled;

  previously_handled = FALSE;
  if (previous_pointer_type_considered != NULL &&
      identical_types(previous_pointer_type_considered, pointer_type)) {
    /* This type was the type of a previous non-class operand;
       it's already been considered. */
    previously_handled = TRUE;
  } else if (previous_class_type_considered != NULL) {
    /* Some pointer types were tried on a previous class operand, so
       check to see if the pointer type we're considering was
       already processed on the previous operand.  It was if the
       underlying class types of the operands are the same or if
       the previous class has a conversion function that converts
       to the pointer type we're considering. */
    if (class_type == previous_class_type_considered ||
        find_conversion_function(previous_class_type_considered,
                                 pointer_type) != NULL) {
      /* This pointer type was tried when the first operand was
         processed, so do not try it again (if we did, it would
        look like an ambiguity). */
      previously_handled = TRUE;
    }  /* if */
  }  /* if */
  return previously_handled;
}  /* pointer_type_previously_handled */


static void try_pointer_builtin_operands_match(
                                char                     *operand_type_pattern,
                                an_arg_operand_ptr       arg_operand_list,
                                a_candidate_function_ptr *candidate_functions)
/*
Subroutine for try_conversions_for_builtin_operator.  Check to see how
well the operand values given by arg_operand_list match the operand type
pattern string given by operand_type_pattern.  If they match, add
the built-in operator to the candidate_functions list.  This is used for the
case where the pattern string contains "CC", meaning two pointer operands
that must have the same type.
*/
{
  an_arg_operand_ptr       arg_operand;
  a_type_ptr               pointer_type, operand_type, class_type;
  char                     *type_pattern_position;
  a_symbol_ptr             conversion_symbol, base_conversion_symbol;
  a_symbol_list_entry_ptr  slep;
  a_type_ptr               conv_routine_type, return_type;
  a_type_ptr               previous_class_type_considered;
  a_type_ptr               previous_pointer_type_considered;
  a_boolean                any_ptr_conversion_function_this_operand;

  db_enter(4, "try_pointer_builtin_operands_match");
  /* The reason the pointer case is more complicated than other cases is
     that it is not sufficient to ask "can this class-type operand be
     converted to any pointer type?" -- we must ask whether both of the
     pointer operands can be converted to a specific pointer type or
     something compatible with it. */
  /* Loop through the two operands. */
  previous_class_type_considered = NULL;
  previous_pointer_type_considered = NULL;
  for (type_pattern_position = operand_type_pattern,
         arg_operand = arg_operand_list;
       arg_operand != NULL;
       type_pattern_position++, arg_operand = arg_operand->next) {
    operand_type = arg_operand->operand.type;
    if (is_class_struct_union_type(operand_type)) {
      /* This operand takes a pointer type and the operand value has a class
         type.  Look for conversion functions that convert the class type
         to any pointer type. */
      class_type = skip_typerefs(operand_type);
      any_ptr_conversion_function_this_operand = FALSE;
      /* Look at all the conversion functions for the source class. */
      for (slep = symbol_supplement_for_class(class_type)->conversion_list;
           slep != NULL;
           slep = slep->next) {
        conversion_symbol = slep->symbol;
        base_conversion_symbol = fundamental_symbol_of(conversion_symbol);
        conv_routine_type = routine_symbol_type(base_conversion_symbol);
        return_type = conv_routine_type->variant.routine.return_type;
        if (is_reference_type(return_type)) {
          /* Drop a reference type; a conversion function that returns
             "const int&" can be used like one that returns "const int". */
          return_type = type_pointed_to(return_type);
        }  /* if */
        return_type = skip_typerefs(return_type);
        if (is_pointer_type(return_type)) {
          /* We've found a conversion function to a pointer type.  Make
             sure it's not a type we've already checked while examining a
             previous operand.  If it is, ignore it. */
          pointer_type = return_type;
          if (!pointer_type_previously_handled(
                                           pointer_type, class_type,
                                           previous_class_type_considered,
                                           previous_pointer_type_considered)) {
            /* Try matching the operands, with the chosen pointer type
               as the target type for operands that must be pointers. */
            any_ptr_conversion_function_this_operand = TRUE;
            try_builtin_operands_match(operand_type_pattern,
                                       arg_operand_list,
                                       candidate_functions,
                                       type_pattern_position,
                                       pointer_type);
          }  /* if */
        }  /* if */
      }  /* for */
      /* Remember if we've processed any class types with pointer conversion
         functions. */
      if (any_ptr_conversion_function_this_operand) {
        previous_class_type_considered = class_type;
      }  /* if */
    } else {
      /* The operand requires a pointer and the value supplied does not
         have a class type.  If it has a pointer type try that type as the
         target type. */
      /* Do array --> pointer and function --> pointer transformations. */
      operand_type = do_implicit_type_transformations(operand_type,
                                                      &arg_operand->operand);
      operand_type = skip_typerefs(operand_type);
      if (is_pointer_type(operand_type)) {
        pointer_type = operand_type;
        /* If the type has been previously handled, ignore it. */
        if (!pointer_type_previously_handled(
                                           pointer_type, (a_type_ptr)NULL,
                                           previous_class_type_considered,
                                           previous_pointer_type_considered)) {
          previous_pointer_type_considered = pointer_type;
          /* Try matching the operands, with the chosen pointer type
             as the target type for operands that must be pointers. */
          try_builtin_operands_match(operand_type_pattern,
                                     arg_operand_list,
                                     candidate_functions,
                                     type_pattern_position,
                                     pointer_type);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  db_exit();
}  /* try_pointer_builtin_operands_match */


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

  db_enter(4, "try_conversions_for_builtin_operator");
  /* Determine the argument pattern for the operator, and whether or not
     the first operand must be an lvalue.  The pattern begins with "L"
     if the operator requires an lvalue as its first operand.  Following that
     are one or more semicolon-separated argument patterns, each one consisting
     of one letter (for unary operators) or two letters (for binary operators)
     indicating the allowed argument types.  As a concrete example, the
     pattern for "-=" is "LAA;PI;CC", indicating that the operator requires
     an lvalue and takes operands of types arith-arith, pointer-int, or
     corresponding pointer-pointer. */
  operand_type_pattern = operand_type_pattern_for_operator(kind,
                                                           unary_operator);
  if (*operand_type_pattern == 'L') {
    /* The operator requires an lvalue as its first operand.  Check that.
       If the operand has a class type it might be convertible to an lvalue
       via a conversion function returning a reference.  Note that we need not
       check for a modifiable lvalue, because the processing for the
       built-in operator will do that if necessary.  In fact, the
       processing for the built-in operator will do full checking, so
       the checking here is just looking for obvious mismatches. */
    first_operand = &arg_operand_list->operand;
    if (!is_an_lvalue(first_operand) &&
        !is_class_struct_union_type(first_operand->type)) {
      /* The first operand is not an lvalue so the built-in operator cannot
         be used.  Give up. */
      goto end_of_check;
    }  /* if */
    /* Advance past the "L". */
    operand_type_pattern++;
  }  /* if */
  /* Check the operands to see if they can be converted to the proper
     types. */
  /* Loop for each ";"-separated pattern in the string. */
  do {
    if (operand_type_pattern[0] == CORRESP_POINTER_TYPE_CODE) {
      /* Both operands must have the same pointer type.  This case is more
         complicated because it involves enumerating the pointer types that
         can be generated by the applicable conversion functions. */
      try_pointer_builtin_operands_match(operand_type_pattern,
                                         arg_operand_list,
                                         candidate_functions);
    } else {
      /* There are no corresponding pointer types in the argument pattern. */
      try_builtin_operands_match(operand_type_pattern,
                                 arg_operand_list,
                                 candidate_functions,
                                 (char *)NULL, (a_type_ptr)NULL);
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
operator functions and conversion functions, but not for function calls
using the usual notation (in those cases, the base class cast is done
as part of the "->" or ".").  operand gives the selector, and routine_type
gives the type of the routine being called.
*/
{
  a_type_ptr       this_param_type, this_class_type, operand_class_type;
  a_boolean        is_arrow_operator = TRUE;
  a_base_class_ptr bcp;

  conv_class_operand_to_object_pointer(operand);
  this_param_type = implicit_this_param_type_of(routine_type);
  this_class_type = f_skip_typerefs(type_pointed_to(this_param_type));
  if (is_pointer_type(operand->type)) {
    operand_class_type = f_skip_typerefs(type_pointed_to(operand->type));
    if (operand_class_type != this_class_type &&
        is_immediate_class_type(operand_class_type) &&
        (bcp = find_base_class_of(operand_class_type, this_class_type))!=NULL){
      /* Do the cast to a base class.  Access checking is suppressed on this
         cast, because the cast is really necessary only because the function
         is inherited from a base class.  This is not clear from the ARM,
         but cfront and Borland do it this way. */
      base_class_cast_operand(operand, bcp, &is_arrow_operator,
                              /*check_cast_access=*/FALSE,
                              /*implicit_in_naming=*/FALSE);
    }  /* if */
  }  /* if */
  /* The cast here handles const/volatile differences and error cases. */
  cast_operand(this_param_type, operand, /*is_implicit_cast=*/TRUE);
}  /* prep_special_selector_operand */


static void adjust_operand_for_builtin_operator(
                                   an_operand               *operand,
                                   a_candidate_function_ptr candidate_function,
                                   int                      operand_num,
                                   an_arg_match_summary_ptr arg_match)
/*
operand is the operand_num-th operand of a built-in operator described by
candidate function.  arg_match is the argument match entry for that
argument.  Adjust the operand type to match the type requirement.
*/
{
  char       type_code;
  a_boolean  processed;
  a_type_ptr pointer_type;

  if (!is_class_struct_union_type(operand->type)) {
    /* Non-class operands need not be adjusted here; the built-in operator
       processing will do it. */
  } else {
    /* Get the type code for this operand (see
       operand_type_pattern_for_operator). */
    type_code = candidate_function->operand_type_pattern[operand_num-1];
    if (type_code != CORRESP_POINTER_TYPE_CODE) {
      /* Non-pointer case.  The conversion function result type is the
         right type. */
      if (conv_usable(&arg_match->conversion)) {
        /* The conversion is usable.  Do it. */
        prep_for_known_possible_conversion(operand, &arg_match->conversion);
        user_convert_operand(operand, /*dest_type=*/(a_type_ptr)NULL,
                             &arg_match->conversion);
      } else {
        /* The conversion is not usable, e.g., because the conversion
           is ambiguous.  Redo the analysis of the conversion to get
           a detailed error message. */
        try_to_convert_class_operand_to_builtin_type(operand,
                                     builtin_type_set_for_type_code(type_code),
                                     &processed);
#if CHECKING
        if (!processed) {
          internal_error(
                     "adjust_operand_for_builtin_operator: conversion failed");
        }  /* if */
#endif /* CHECKING */
      }  /* if */
    } else {
      /* Pointer cases.  Convert to the pointer type indicated in
         candidate_function. */      
      pointer_type = candidate_function->pointer_type;
      check_assertion(pointer_type != NULL);
      prep_conversion_operand(operand, pointer_type,
                              &arg_match->conversion,
                              /*is_initialization=*/TRUE,
                              ec_no_error,
                              &operand->position);
    }  /* if */
  }  /* if */
}  /* adjust_operand_for_builtin_operator */


void check_for_operator_overloading(an_opname_kind     kind,
                                    a_boolean          unary_operator,
                                    a_boolean          must_be_member_function,
                                    a_boolean          try_conversions,
                                    a_boolean          has_predef_meaning,
                                    an_operand         *operand_1,
                                    an_operand         *operand_2,
                                    a_source_position  *operator_position,
                                    an_operand         *result,
                                    a_boolean          *processed)
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
appropriate.  In cases where none of the operands has a class type, the
operands are returned unchanged.  Note that this routine is called for
operator "?", with unary_operator FALSE; the two operands are the second
and third operands of the "?" ("?" cannot be overloaded, but conversion
functions could still apply).
*/
{
  an_arg_operand_ptr       arg_operand_list, arg_operand_list2, arg_operand;
  an_expr_node_ptr         arg_expr_list, end_arg_expr_list;
  a_symbol_ptr             nonmember_functions_symbol;
  a_symbol_ptr             member_functions_symbol;
  a_symbol_ptr             function_symbol;
  a_boolean                operand_1_is_class;
  an_operand               function_operand;
  a_candidate_function_ptr candidate_functions;
  an_arg_match_summary_ptr arg_match;
  a_boolean                matched_except_for_missing_selector = FALSE;
  a_boolean                member_is_best_match, have_selector;
  an_expr_node_ptr         arg;
  a_type_ptr               routine_type;
  a_param_type_ptr         param;
  an_operand               *bound_function_selector;
  a_boolean                undecidable_because_of_error;
  a_boolean                arg_operand_list_discarded;

  db_enter(4, "check_for_operator_overloading");
  *processed = FALSE;
  /* Operator overloading should not be tried in constant expressions. */
  if (!curr_expr_kind_is_const()) {
    if (is_error_operand(operand_1) || 
        (!unary_operator && is_error_operand(operand_2))) {
      /* One or both of the operands is an error operand. */
      if (opname_symbol_table[kind] != NULL) {
        /* There exists a function that overloads the operator.  Therefore,
           a match might have been possible.  However, we cannot tell.
           Assume there is a match and give up. */
        *processed = TRUE;
        make_error_operand(result);
      } else {
        /* The operator is not overloaded.  Return to the caller to do
           the built-in operator processing. */
      }  /* if */
    } else {
      /* At least one operand must have a class type.  An error operand for
         the second operand counts as a class operand. */
      operand_1_is_class = is_class_struct_union_type(operand_1->type);
      if (operand_1_is_class ||
          (!unary_operator &&
           (is_class_struct_union_type(operand_2->type) ||
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
        /* Find any member function for the operator. */
        if (operand_1_is_class) {
          member_functions_symbol = opname_member_function_symbol(kind,
                                               skip_typerefs(operand_1->type));
          if (member_functions_symbol != NULL) {
            /* There are member functions for this class type.  See how well
               they match up. */
#if CHECKING
            if (member_functions_symbol->class_of_which_a_member == NULL) {
              internal_error(
                            "check_for_operator_overloading: func not member");
            }  /* if */
#endif /* CHECKING */
            /* Use the first operand as the selector expression, and
               the second operand as the first actual argument . */
            try_overloaded_function_match(member_functions_symbol,
                                          arg_operand_list2,
                                          /*have_selector=*/TRUE,
                                          operand_1,
                                          /*selector_is_object_pointer=*/FALSE,
                                          /*user_conversion_case=*/FALSE,
                                          &candidate_functions,
                                         &matched_except_for_missing_selector);
          }  /* if */
        }  /* if */
        /* Find any non-member function for the operator. */
        if (!must_be_member_function) {
          nonmember_functions_symbol = opname_function_symbol(kind);
          /* There are non-member functions.  See how well they match up. */
          if (nonmember_functions_symbol != NULL) {
            try_overloaded_function_match(nonmember_functions_symbol,
                                          arg_operand_list,
                                          /*have_selector=*/FALSE,
                                          (an_operand *)NULL,
                                          /*selector_is_object_pointer=*/TRUE,
                                          /*user_conversion_case=*/FALSE,
                                          &candidate_functions,
                                         &matched_except_for_missing_selector);
          }  /* if */
        }  /* if */
        /* See if the built-in meaning of the operator can apply if we convert
           the class operand(s) to a built-in type through use of a conversion
           function. */
        if (try_conversions) {
          /* See if we can find user-defined conversions to built-in types that
             will make the built-in operator feasible.  The argument matches
             are compared to the best match so far from the above searches. */
          try_conversions_for_builtin_operator(kind, unary_operator,
                                               arg_operand_list,
                                               &candidate_functions);
        }  /* if */
        /* The candidate_functions list now contains all the viable
           functions.  Find the best. */
        select_best_candidate_functions(&candidate_functions,
                                        operator_position,
                                        &undecidable_because_of_error);
        function_symbol = NULL;
        arg_expr_list = NULL;
        arg_operand_list_discarded = FALSE;
        if (undecidable_because_of_error) {
          /* There was a previous error. */
          *processed = TRUE;
          arg_operand_list_discarded = TRUE;
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
            pos_error(ec_no_matching_operator_function, operator_position);
            make_error_operand(result);
            arg_operand_list_discarded = TRUE;
          }  /* if */
        } else if (candidate_functions->next != NULL) {
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
          diagnose_overload_ambiguity(candidate_functions, kind);
          make_error_operand(result);
          arg_operand_list_discarded = TRUE;
        } else {
          /* Exactly one function applies and is best. */
          function_symbol = candidate_functions->function_symbol;
          arg_match = candidate_functions->arg_matches;
          if (function_symbol == NULL) {
            /* A built-in operator was selected. */
#if DEBUG
            if (debug_level >= 4) {
              fprintf(f_debug, "check_for_operator_overloading: selected\n");
              db_candidate_function(candidate_functions);
            }  /* if */
#endif /* DEBUG */
            /* *processed is left FALSE so the caller will try the built-in
               meaning. */
            /* Convert the operands to the proper types. */
            adjust_operand_for_builtin_operator(operand_1,
                                                candidate_functions, 1,
                                                arg_match);
            if (!unary_operator) {
              adjust_operand_for_builtin_operator(operand_2,
                                                  candidate_functions, 2,
                                                  arg_match->next);
            }  /* if */
          } else {
            /* An operator function was selected. */
#if DEBUG
            if (debug_level >= 4) {
              db_symbol(function_symbol,
                        "check_for_operator_overloading: selected ", 2);
            }  /* if */
#endif /* DEBUG */
            *processed = TRUE;
            routine_type = routine_symbol_type(function_symbol);
            arg_operand = arg_operand_list;
            bound_function_selector = NULL;
            member_is_best_match = 
                       routine_type_is_nonstatic_member_function(routine_type);
            if (member_is_best_match) {
              /* The function selected is a non-static member function.
                 Therefore, the first argument is to be used as the selector
                 object.  We must convert it to an object pointer and then
                 cast it to a base class if necessary. */
              bound_function_selector = &arg_operand_list->operand;
              /* Issue any warning about the "this" parameter detected while
                 evaluating the alternatives. */
              issue_warning_from_arg_match_summary(
                                           arg_match,
                                           &bound_function_selector->position);
              /* Make a pointer for the selector, and cast it to a base class
                 if necessary. */
              prep_special_selector_operand(bound_function_selector,
                                            routine_type);
              /* The "real" argument list starts with the second argument. */
              arg_operand = arg_operand->next;
              arg_match = arg_match->next;
            }  /* if */
            /* Build an expression-form argument list.  Convert the arguments
               on the argument list to the right types.  Note that for the
               member function case we start at the second operand. */
            param = routine_type->variant.routine.extra_info->param_type_list;
            arg_expr_list = end_arg_expr_list = NULL;
            for (; arg_operand != NULL;
                 arg_operand = arg_operand->next,arg_match = arg_match->next) {
              arg = node_for_arg_of_overloaded_function_call(arg_operand,
                                                             arg_match,
                                                             param);
              if (arg_expr_list == NULL) {
                arg_expr_list = arg;
              } else {
                end_arg_expr_list->next = arg;
              }  /* if */
              end_arg_expr_list = arg;
              /* Advance to the next parameter unless we're at an ellipsis. */
              if (param != NULL) param = param->next;
            }  /* for */
            /* Do the things that would have been done to the symbol but
               weren't because the specific symbol was not known, and build an
               operand for the function. */
            have_selector = member_is_best_match;
            make_resolved_overloaded_function_operand(
                                                 function_symbol,
                                                 member_is_best_match ?
                                                    member_functions_symbol :
                                                    nonmember_functions_symbol,
                                                 &have_selector,
                                                 bound_function_selector,
                                                 /*is_qualified_name=*/FALSE,
                                                 operator_position,
                                                 &function_operand);
            /* Make the call node and an operand for it. */
            assemble_function_call(&function_operand, bound_function_selector,
                                   arg_expr_list, result);
          }  /* if */
        }  /* if */
        /* Free the candidate functions list. */
        free_candidate_function_list(candidate_functions);
        if (arg_operand_list_discarded) {
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


/*
If symbol points to a projection symbol that is ambiguous by inheritance,
set *ambiguous to TRUE.  Always set symbol to its fundamental symbol.
*/
#define check_symbol_ambiguous_by_inheritance(symbol, ambiguous)      \
{ if ((symbol)->kind == (a_symbol_kind)sk_projection) {               \
    if ((symbol)->variant.projection.ambiguous) *(ambiguous) = TRUE;  \
    symbol = (symbol)->variant.projection.extra_info->fundamental_symbol; \
  }  /* if */                                                         \
}  /* check_symbol_ambiguous_by_inheritance */


static a_boolean conversion_to_class_possible(
                                  an_operand               *source_operand,
                                  a_type_ptr               dest_type,
                                  a_boolean                need_lvalue_result,
                                  a_conv_descr             *conversion,
                                  a_boolean                *ambiguous,
                                  a_candidate_function_ptr *ambiguity_list)
/*
If source_operand can be converted to the class type dest_type
(via a constructor or conversion function) set *conversion to describe
the routine that can do the conversion and return TRUE.  Otherwise
return FALSE.  If need_lvalue_result is TRUE, the result must be an
lvalue.  If more than one function matches, set *ambiguous to TRUE
and return FALSE.  If ambiguity_list is non-NULL in that case, it is set
to point to a list describing the set of ambiguous functions; the caller must
free that list.  *ambiguity_list is set to NULL to indicate a case that
is undecidable because of an error.  Note that this routine does not
check for the possibility of bitwise copying (see class_bitwise_copy_possible).
This routine is only used in C++ mode.
*/
{
  a_boolean                     okay;
  a_type_ptr                    class_type;
  a_candidate_function_ptr      candidate_functions;
  a_boolean                     matched_except_for_missing_selector = FALSE;
  a_symbol_ptr                  class_symbol, constructor_symbol;
  a_symbol_ptr                  conversion_symbol;
  a_class_symbol_supplement_ptr cssp;
  an_arg_operand_ptr            arg_operand_list;
  a_boolean                     undecidable_because_of_error;

  db_enter(4, "conversion_to_class_possible");
  /* Note that this routine is like a simplified version of
     select_overloaded_function that works for user-defined conversion
     functions (no arguments, just a "this" parameter). */
  clear_conv_descr(conversion);
  class_type = skip_typerefs(dest_type);
  /* If the class is a template class, instantiate it so that its
     constructors are visible. */
  instantiate_template_class(class_type);
  class_symbol = (a_symbol_ptr)(class_type->source_corresp.assoc_info);
  cssp = class_symbol->variant.class_struct_union.extra_info;
  /* candidate_functions will contain the list of viable functions. */
  candidate_functions = NULL;
  /* Make an argument list with just the source operand. */
  arg_operand_list = alloc_arg_operand();
  copy_operand(source_operand, &arg_operand_list->operand);
  constructor_symbol = cssp->constructor;
  /* Constructors don't create lvalues, so don't try them if we need
     an lvalue result. */
  if (constructor_symbol != NULL && !need_lvalue_result) {
    /* The class has constructors. */
    /* Try all the constructors with that argument list. */
    try_overloaded_function_match(constructor_symbol,
                                  arg_operand_list,
                                  /*have_selector=*/FALSE, /* sic */
                                  (an_operand *)NULL,
                                  /*selector_is_object_pointer=*/FALSE,
                                  /*user_conversion_case=*/TRUE,
                                  &candidate_functions,
                                  &matched_except_for_missing_selector);
  }  /* if */
  if (is_class_struct_union_type(source_operand->type) &&
      cssp->target_of_conversion_function) {
    /* There is at least one conversion function that converts some other
       class into the desired class, and the source type is a class.
       See if there is a conversion function that does the job. */
    try_conversion_function_match(source_operand, dest_type,
                                  (a_builtin_type_kind_set)BTK_NONE,
                                  need_lvalue_result,
                                  &candidate_functions);
  }  /* if */
  /* The candidate_functions list now contains all the viable functions.
     Find the best ones. */
  select_best_candidate_functions(&candidate_functions,
                                  &source_operand->position,
                                  &undecidable_because_of_error);
  *ambiguous = FALSE;
  okay = FALSE;
  if (undecidable_because_of_error) {
    *ambiguous = TRUE;
    /* Note that candidate_functions is NULL (select_best_candidate_functions
       returns it that way in this case), so a NULL ambiguity_list will
       be returned to indicate "undecidable because of error". */
  } else if (candidate_functions == NULL) {
    /* No constructor or conversion function is suitable. */
  } else if (candidate_functions->next != NULL) {
    /* More than one constructor or conversion function matches at the same
       level.  Ambiguity. */
    *ambiguous = TRUE;
#if DEBUG
    if (debug_level >= 4) {
      db_candidate_function_list(candidate_functions);
    }  /* if */
#endif /* DEBUG */
  } else {
    /* Exactly one constructor or conversion function matches best. */
    conversion_symbol = candidate_functions->function_symbol;
    /* If the function is a conversion function that is inherited from a base
       class, check to see if it's ambiguous by inheritance. */
    check_symbol_ambiguous_by_inheritance(conversion_symbol, ambiguous);
    if (!*ambiguous) {
      okay = TRUE;
      /* Return information on how the conversion is to be done. */
      *conversion = candidate_functions->conversion;
    }  /* if */
  }  /* if */
  conversion->ambiguous = *ambiguous;
  if (*ambiguous && ambiguity_list != NULL) {
    /* Return the candidate functions list to the caller, for use in generating
       an ambiguity error.  The caller will free the list. */
    *ambiguity_list = candidate_functions;
  } else {
    /* Free the candidate functions list. */
    free_candidate_function_list(candidate_functions);
  }  /* if */
  free_arg_operand_list(arg_operand_list);
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
                               a_conv_descr             *conversion,
                               a_boolean                *ambiguous,
                               a_candidate_function_ptr *ambiguity_list)
/*
If the class operand source_operand can be converted by a conversion function
to either

(a) dest_type, if dest_type is non-NULL (a standard conversion can be
    done after the conversion function, if necessary), and an lvalue
    of that type if need_lvalue_result is TRUE, or
(b) a built-in type in the set given by builtin_types_allowed, if
    dest_type is NULL.

then set *conversion to describe the conversion, and return TRUE.
Otherwise return FALSE.  If more than one function matches, set
*ambiguous to TRUE and return FALSE.  If ambiguity_list is non-NULL in
that case, it is set to point to a list describing the set of ambiguous
functions; the caller must free that list.  *ambiguity_list is set to
NULL to indicate a case that is undecidable because of an error.  Note
that this routine does not look for constructors that can be used as
conversion functions (see conversion_to_class_possible) or for the
possibility of bitwise copying (see class_bitwise_copy_possible).
This routine is only used in C++ mode.
*/
{
  a_boolean                okay;
  a_symbol_ptr             conversion_symbol;
  a_candidate_function_ptr candidate_functions;
  a_boolean                undecidable_because_of_error;

  db_enter(4, "conversion_from_class_possible");
  /* This routine is similar to select_overloaded_function. */
  clear_conv_descr(conversion);
  candidate_functions = NULL;
  /* Find any viable conversion functions. */
  try_conversion_function_match(source_operand, dest_type,
                                builtin_types_allowed, need_lvalue_result,
                                &candidate_functions);
  /* Of the viable functions, select the best. */
  select_best_candidate_functions(&candidate_functions,
                                  &source_operand->position,
                                  &undecidable_because_of_error);
  *ambiguous = FALSE;
  okay = FALSE;
  if (undecidable_because_of_error) {
    *ambiguous = TRUE;
    /* Note that candidate_functions is NULL (select_best_candidate_functions
       returns it that way in this case), so a NULL ambiguity_list will
       be returned to indicate "undecidable because of error". */
  } else if (candidate_functions == NULL) {
    /* There are no viable conversion functions. */
  } else if (candidate_functions->next != NULL) {
    /* There are several equally desirable functions. */
    *ambiguous = TRUE;
#if DEBUG
    if (debug_level >= 4) {
      db_candidate_function_list(candidate_functions);
    }  /* if */
#endif /* DEBUG */
  } else {
    /* There is exactly one best conversion function. */
    conversion_symbol = candidate_functions->function_symbol;
    /* If the function is a conversion function that is inherited from a base
       class, check to see if it's ambiguous by inheritance. */
    check_symbol_ambiguous_by_inheritance(conversion_symbol, ambiguous);
    if (!*ambiguous) {
      okay = TRUE;
      /* Return information on how the conversion is to be done. */
      *conversion = candidate_functions->conversion;
    }  /* if */
  }  /* if */
  conversion->ambiguous = *ambiguous;
  if (*ambiguous && ambiguity_list != NULL) {
    /* Return the candidate functions list to the caller, for use in generating
       an ambiguity error.  The caller will free the list. */
    *ambiguity_list = candidate_functions;
  } else {
    /* Free the candidate functions list. */
    free_candidate_function_list(candidate_functions);
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
If so, convert it.  The result is always an rvalue.  Issue an error and
set *processed to TRUE if the conversion is ambiguous.
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
                                       &conversion,
                                       &ambiguous, &ambiguity_list)) {
      /* The conversion is possible -- do it. */
      /* Force the result to be an rvalue. */
      conversion.result_is_an_lvalue = FALSE;
      user_convert_operand(operand, /*dest_type=*/(a_type_ptr)NULL,
                           &conversion);
      *processed = TRUE;
    } else if (ambiguous) {
      /* There is more than one possible conversion to a built-in type. */
      /* A NULL ambiguity_list indicates a case that was undecidable because
         of an error (no additional error is needed). */
      if (ambiguity_list != NULL) {
        pos_ty_start_error(ec_ambiguous_conversion_to_builtin,
                           &operand->position, operand->type);
        diagnose_overload_ambiguity(ambiguity_list, (an_opname_kind)onk_none);
        free_candidate_function_list(ambiguity_list);
      }  /* if */
      conv_to_error_operand(operand);
      *processed = TRUE;
    }  /* if */
  }  /* if */
}  /* try_to_convert_class_operand_to_builtin_type */


static a_boolean class_bitwise_copy_possible(a_type_ptr source_type,
                                             a_type_ptr dest_type,
                                             a_boolean  is_initialization)
/*
Return TRUE if an entity of type dest_type (a class type) can be initialized
from (is_initialization == TRUE) or assigned from (is_initialization == FALSE)
an entity of type source_type using a bitwise copy.  This routine is
used only in C++ mode.
*/
{
  a_boolean                     bitwise_copy_allowed = FALSE;
  a_class_symbol_supplement_ptr cssp;

  dest_type = skip_typerefs(dest_type);
  cssp = symbol_supplement_for_class(dest_type);
  source_type = skip_typerefs(source_type);
  if (is_class_struct_union_type(source_type)) {
    if (is_initialization ? cssp->construction_by_bitwise_copy_allowed :
                            cssp->assignment_by_bitwise_copy_allowed) {
      /* The destination class can be set by a bitwise copy from something
         of the same type or a derived type thereof. */
      if (types_are_compatible(dest_type, source_type) ||
          find_base_class_of(source_type, dest_type) != NULL) {
        bitwise_copy_allowed = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return bitwise_copy_allowed;
}  /* class_bitwise_copy_possible */


a_boolean user_defined_conversion_possible(an_operand   *source_operand,
                                           a_type_ptr   dest_type,
                                           a_boolean    is_initialization,
                                           a_boolean    need_lvalue_result,
                                           a_conv_descr *conversion,
                                           a_boolean    *failed)
/*
Check whether or not the source operand can be converted to the
destination type by a user-defined conversion (constructor or
conversion routine), in an initialization (is_initialization == TRUE)
or assignment (is_initialization == FALSE).  If so, set *conversion
to describe the conversion and return TRUE.  If not, return FALSE.  If
a user-defined conversion is the only hope of converting the source
operand to the destination type (i.e., one or the other has a class
type), and no conversion was found, issue an error, change
source_operand to an error operand, set *failed to TRUE, and return
FALSE.  need_lvalue_result is TRUE if the result is required to be
an lvalue; otherwise, the result can be an lvalue or an rvalue.
Note that this routine should only be called when the conversion must
be done, not when we're just wondering if it can be done, because it
issues errors.  See 12.3 in the ARM.  This routine is only called in
C++ mode.  The destination type must not be a reference type (the
caller should have rewritten that case).
*/
{
  a_boolean                okay = FALSE, ambiguous, to_class;
  a_type_ptr               source_type;
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
  to_class = is_class_struct_union_type(dest_type);
  if (to_class) {
    /* The destination type is a class. */
    to_class = TRUE;
    if (class_bitwise_copy_possible(source_type, dest_type,
                                    is_initialization)) {
      /* A bitwise copy of the class is allowed. */
      conversion->class_identity_or_bitwise_copy = TRUE;
      okay = TRUE;
    } else if (conversion_to_class_possible(source_operand, dest_type,
                                            need_lvalue_result,
                                            conversion, &ambiguous,
                                            &ambiguity_list)) {
      /* A user-defined conversion (constructor or conversion function) is
         available to convert to the destination type. */
      okay = TRUE;
    } else {
      /* The conversion is not possible. */
      *failed = TRUE;
      /* Pick the right error code. */
      if (is_class_struct_union_type(source_type)) {
        /* Both the source and destination types are classes, so the
           message should indicate that both constructors and conversion
           functions were considered. */
        err_code = ambiguous ? ec_ambiguous_user_defined_conversion :
                               ec_no_user_defined_conversion;
      } else {
        /* The source type is a non-class and the destination type is a class,
           so the message should indicate that constructors were considered. */
        err_code = ambiguous ? ec_ambiguous_constructor_for_conversion :
                               ec_no_constructor_for_conversion;
      }  /* if */
    }  /* if */
  } else if (is_class_struct_union_type(source_type)) {
    /* The source type is a class (and the destination type is not a class). */
    if (conversion_from_class_possible(source_operand, dest_type,
                                       (a_builtin_type_kind_set)BTK_NONE,
                                       need_lvalue_result,
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
  }  /* if */
  if (*failed) {
    /* The conversion failed. */
    if (!ambiguous) {
      /* No conversion applies. */
      if (is_error_type(dest_type) || is_error_type(source_type)) {
        /* Some previous error. */
      } else if (is_incomplete_type(dest_type)) {
        /* Conversion to an incomplete type is not possible (in this
           case, anyway).  Use a different message for clarity. */
        pos_ty_error(ec_converting_to_incomplete_class,
                     &source_operand->position, dest_type);
      } else {
        /* Put out the usual message (which has already been chosen to
           describe the problem). */
        type2_error_in_operand(err_code, source_operand,
                               source_type, dest_type);
      }  /* if */
    } else {
      /* More than one conversion applies (ambiguity). */
      /* A NULL ambiguity_list indicates a case that was undecidable because
         of an error (no additional error is needed). */
      if (ambiguity_list != NULL) {
        pos_ty2_start_error(err_code, &source_operand->position,
                            source_type, dest_type);
        diagnose_overload_ambiguity(ambiguity_list, (an_opname_kind)onk_none);
        free_candidate_function_list(ambiguity_list);
      }  /* if */
    }  /* if */
    conv_to_error_operand(source_operand);
  }  /* if */
  return okay;
}  /* user_defined_conversion_possible */


static a_boolean conversion_possible(an_operand        *source_operand,
                                     a_type_ptr        dest_type,
                                     a_type_ptr        orig_dest_type,
                                     a_boolean         is_initialization,
                                     an_error_code     incompatible_err,
                                     a_source_position *err_pos,
                                     a_conv_descr      *conversion)
/*
Check whether or not the source operand can be converted to the
destination type, implicitly, in an initialization (is_initialization ==
TRUE) or assignment (is_initialization == FALSE).  If so, set
*conversion to describe the conversion, and return TRUE.  If not, issue
the error incompatible_err at the position err_pos, change the operand
to an error operand, and return FALSE.  See 3.3.16.1 in the ANSI C
standard and 12.3 in the ARM.  Note that this routine should only be
called when the conversion must be done, not when we're just wondering
if it can be done, because it does operand transformations on
source_operand and issues errors.  The destination type must not be a
reference type (the caller should have rewritten that case).
orig_dest_type is the original destination type (not rewritten) for use
in error messages.
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
  if (C_dialect == C_dialect_cplusplus &&
      user_defined_conversion_possible(source_operand, dest_type,
                                       is_initialization,
                                       /*need_lvalue_result=*/FALSE,
                                       conversion, &failed)) {
    /* A user-defined conversion can be done. */
    okay = TRUE;
  } else if (!failed) {
    /* No user-defined conversion applies. */
    /* Do the lvalue --> rvalue transformation et al. */
    do_operand_transformations(source_operand,
                               TOPT_SUPPRESS_CHECK_FOR_INDEFINITE_FUNCTION);
    /* Note that the source type is extracted after the lvalue to
       rvalue transformation. */
    source_type = source_operand->type;
    if (is_indefinite_function_operand(source_operand)) {
      /* The source is an indefinite function, i.e., the address of an
         overloaded function.  It can be converted to an appropriate
         pointer (ARM 13.3) or pointer-to-member type (not mentioned in ARM,
         but sensible). */
      a_std_conv_descr std_conversion;

      if (find_addr_of_overloaded_function_match(
                                           source_operand->variant.symbol,
                                           dest_type,
                                           &source_operand->position,
                                           &match_level,
                                           &std_conversion,
                                           &ambiguous) != NULL) {
        okay = TRUE;
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
               types_are_compatible(source_type,
                                    f_skip_typerefs(dest_type))) {
      /* In C, a struct or union is compatible with the same struct or union.
         Type qualifiers on the destination are ignored because they
         can be added on the conversion. */
      conversion->class_identity_or_bitwise_copy = TRUE;
      okay = TRUE;
    } else if (impl_conversion_possible(source_type,
                                        is_constant_operand(source_operand),
                                        &source_operand->variant.constant,
                                        dest_type,
                                        /*suppress_extensions=*/FALSE,
                                        incompatible_err,
                                        &std_conv)) {
      /* An implicit conversion is legal. */
      okay = TRUE;
      conversion->std = std_conv;
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
                                            a_type_ptr dest_type)
/*
source_operand is to be copied bitwise to an entity of type dest_type.
Both have class types.  Adjust source_operand if necessary, specifically
for the case where the source type is a derived class of dest_type.
This is used both for initialization and for assignment.  See ARM 8.4.1
(aggregate initialization) and 5.17 (assignment operators).  Note that
this routine does not do the bitwise copy; it just prepares the operand
for it.  Note also that this routine is called for the identity case
where the class type is already correct and nothing should be done to it.
*/
{
  a_type_ptr       source_type;
  a_boolean        is_arrow_operator = TRUE;
  a_base_class_ptr bcp;

  source_type = skip_typerefs(source_operand->type);
  dest_type = skip_typerefs(dest_type);
  if (types_are_compatible(source_type, dest_type)) {
    /* The source and destination types are the same type, so no conversion
       is necessary. */
  } else {
    /* An entity of a base class is being initialized from an object
       of a derived class.  Get its address, cast it to the base class,
       then indirect through that to get an object of the base class. */
    conv_class_operand_to_object_pointer(source_operand);
    bcp = find_base_class_of(source_type, dest_type);
#if CHECKING
    if (bcp == NULL) {
      internal_error("prep_class_bitwise_copy_operand: base class not found");
    }  /* if */
#endif /* CHECKING */
    base_class_cast_operand(source_operand, bcp, &is_arrow_operator,
                            /*check_cast_access=*/TRUE,
                            /*implicit_in_naming=*/FALSE);
    /* Make an address (an lvalue) for the base class object. */
    conv_object_pointer_to_lvalue(source_operand);
  }  /* if */
  /* Make the source an rvalue. */
  do_operand_transformations(source_operand, TOPT_NO_OPTIONS);
}  /* prep_class_bitwise_copy_operand */


static void set_up_for_conversion_function_call(
                                           an_operand       *operand,
                                           a_routine_ptr    conversion_routine,
                                           an_expr_node_ptr *arg_expr_list)
/*
Prepare for generating a call of a conversion function, but do not
actually create the call.  Check accessibility of the routine and adjust
the operand type if necessary so that it will be appropriate for the call.
Return an argument list for the call in *arg_expr_list.  This routine
is used only in C++ mode.
*/
{
  a_symbol_ptr conversion_symbol;
  a_type_ptr   this_param_type, routine_type;

  /* Check that the conversion function is accessible and mark it as
     referenced. */
  conversion_symbol =
                 (a_symbol_ptr)(conversion_routine->source_corresp.assoc_info);
  routine_type = conversion_routine->type;
  this_param_type = implicit_this_param_type_of(routine_type);
  reference_to_implicitly_invoked_function(conversion_symbol,
                                           &operand->position,
                                           operand->type,
                                           /*honor_virtual=*/FALSE,
                                           curr_expr_is_evaluated(),
                                           /*suppress_access_check=*/FALSE);
  /* Convert the operand to the proper type to be an argument of the
     conversion function. */
#if CHECKING
  if (this_param_type == NULL) {
    internal_error("set_up_for_conversion_function_call: no this parameter");
  }  /* if */
#endif  /* CHECKING */
  /* Check for the cfront anachronism that allows a non-const function to be
     called for a const selector (see selector_match_with_this_param). */
  if (cfront_2_1_mode &&
      is_const_qualified_type(operand->type) &&
      !is_const_qualified_type(type_pointed_to(this_param_type))) {
    pos_warning(ec_const_function_anachronism, &operand->position);
    /* prep_special_selector_operand (call below) will drop the const. */
  }  /* if */
  /* Make a pointer for the selector, and cast it to a base class
     if necessary. */
  prep_special_selector_operand(operand, routine_type);
  /* Make an expression for the argument. */
  *arg_expr_list = make_node_from_operand(operand);
}  /* set_up_for_conversion_function_call */


static void set_up_for_constructor_call(an_operand       *operand,
                                        a_routine_ptr    ctor_routine,
                                        an_expr_node_ptr *arg_expr_list)
/*
Prepare for generating a call of a one-argument constructor (i.e.,
a copy constructor or a constructor used as a conversion function),
but do not actually create the call.  *operand is the argument for
the call.  Check accessibility of the routine and adjust the
operand type if necessary so that it will be appropriate for the call.
Return an argument list for the call in *arg_expr_list.  This routine
is used only in C++ mode.
*/
{
  a_symbol_ptr     ctor_symbol;
  a_type_ptr       routine_type;
  a_param_type_ptr param_list;
  a_routine_type_supplement_ptr
                   rtsp;

  /* Check that the constructor is accessible and mark it as referenced. */
  ctor_symbol = (a_symbol_ptr)(ctor_routine->source_corresp.assoc_info);
  reference_to_implicitly_invoked_function(ctor_symbol,
                                           &operand->position,
                                           ctor_routine->source_corresp.
                                                       class_of_which_a_member,
                                           /*honor_virtual=*/FALSE,
                                           curr_expr_is_evaluated(),
                                           /*suppress_access_check=*/FALSE);
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
     through overload resolution.  We know that no user-defined conversion
     is going to be required; at most a normal cast is needed. */
  prep_possible_ellipsis_argument_operand(operand, param_list,
                                          (a_conv_descr_ptr)NULL);
  /* Make an expression for the argument. */
  *arg_expr_list = make_node_from_operand(operand);
  /* If the constructor has default arguments after the first, add
     arguments for them. */
  if (param_list != NULL) {
    (*arg_expr_list)->next = copy_default_arg_expr_list(param_list->next);
  }  /* if */
}  /* set_up_for_constructor_call */


void make_constructor_dynamic_init(a_routine_ptr     ctor_routine,
                                   an_expr_node_ptr  arg_expr_list,
                                   a_boolean         result_is_addr,
                                   a_source_position *position,
                                   an_operand        *result)
/*
Create an enk_temp_init node that calls the constructor ctor_routine with
the argument list arg_expr_list.  Return an operand for the value (if
result_is_addr == FALSE) or address (if result_is_addr == TRUE) of the
temporary in *result.  The argument list has already been prepared for
the call (default arguments have been added, the argument types have
been adjusted, etc.).
*/
{
  a_type_ptr         class_type;
  a_dynamic_init_ptr dip;
  an_expr_node_ptr   temp_init_node;

#if CHECKING
  if (ctor_routine->special_kind != (a_special_function_kind)sfk_constructor) {
    internal_error("make_constructor_dynamic_init: routine not constructor");
  }  /* if */
#endif /* CHECKING */
  class_type = ctor_routine->source_corresp.class_of_which_a_member;
  /* Create the dynamic initialization entry and the enk_temp_init node. */
  temp_init_node = create_expr_temporary(class_type, result_is_addr,
                                         curr_expr_is_evaluated(),
                                         (a_boolean)expr_stack->
                                                 in_return_by_cctor_expression,
                                         position);
  dip = temp_init_node->variant.init.dynamic_init;
  /* Use a dik_constructor to call the constructor routine. */
  set_dynamic_init_kind(dip, (a_dynamic_init_kind)dik_constructor);
  dip->variant.constructor.ptr = ctor_routine;
  dip->variant.constructor.args = arg_expr_list;
  /* Make an operand for the overall expression. */
  make_expression_operand(temp_init_node, temp_init_node->type, result);
}  /* make_constructor_dynamic_init */


void user_convert_operand(an_operand   *operand,
                          a_type_ptr   dest_type,
                          a_conv_descr *conversion)
/*
Do the user-defined conversion indicated by *conversion to convert
*operand to dest_type.  dest_type may be NULL to indicate that
no additional conversion is needed after the conversion function is called.
*/
{
  an_expr_node_ptr  rout_node, arg_expr_list;
  an_operand        orig_operand;
  a_routine_ptr     conversion_routine;

  orig_operand = *operand;
  conversion_routine = conversion->routine;
#if CHECKING
  if (conversion->ambiguous) {
    /* The conversion was ambiguous.  That should have been figured out
       again and shouldn't get here. */
    internal_error("user_convert_operand: ambiguous conversion");
  }  /* if */
#endif /* CHECKING */
  if (conversion->class_identity_or_bitwise_copy) {
    /* Bitwise copy of a class. */
    prep_class_bitwise_copy_operand(operand, dest_type);
  } else if (conversion_routine->special_kind ==
                                     (a_special_function_kind)sfk_conversion) {
    /* Conversion function. */
    set_up_for_conversion_function_call(operand, conversion_routine,
                                        &arg_expr_list);
    /* Conversion routines are called directly. */
    /* Make a node for the address of the function. */
    rout_node = function_addr_expr(conversion_routine);
    rout_node->next = arg_expr_list;
    /* Make an operand for the call. */
    make_function_call(rout_node, conversion_routine->type,
                       (a_boolean)conversion_routine->is_virtual,
                       &orig_operand.position, operand);
    if (!conversion->result_is_an_lvalue || 
        conversion->std.nontrivial_conversion) {
      /* The caller will not accept an lvalue, or a standard conversion
         must be done, so convert an lvalue to an rvalue.  The operand
         could only be an lvalue if the conversion function returns a
         reference. */
      conv_lvalue_to_rvalue(operand);
    }  /* if */
    /* Do any necessary standard or trivial conversion. */
    if (dest_type != NULL && is_an_rvalue(operand)) {
      cast_operand(dest_type, operand, /*is_implicit_cast=*/TRUE);
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
    set_up_for_constructor_call(operand, conversion_routine, &arg_expr_list);
    make_constructor_dynamic_init(conversion_routine, arg_expr_list,
                                  /*result_is_addr=*/FALSE,
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
*/
{
#if CHECKING
  if (conversion->ambiguous) {
    /* The conversion was ambiguous.  That should have been figured out
       again and shouldn't get here. */
    internal_error("convert_operand: ambiguous conversion");
  }  /* if */
#endif /* CHECKING */
  if (!is_null_user_conv_descr(conversion)) {
    /* Call a user-defined conversion routine. */
    user_convert_operand(source_operand, dest_type, conversion);
  } else {
    /* Cast the operand to the result type. */
    cast_operand(dest_type, source_operand, /*is_implicit_cast=*/TRUE);
  }  /* if */
}  /* convert_operand */


static a_boolean conversion_usable_or_possible(
                                    an_operand        *source_operand,
                                    a_type_ptr        dest_type,
                                    a_type_ptr        orig_dest_type,
                                    a_boolean         is_initialization,
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
the conversion information.  orig_dest_type is the destination type
before any rewriting, for use in error messages.
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
                                   is_initialization,
                                   incompatible_err, err_pos,
                                   *p_conversion);
  }  /* if */
  return possible;
}  /* conversion_usable_or_possible */


static void prep_conversion_operand(an_operand        *source_operand,
                                    a_type_ptr        dest_type,
                                    a_conv_descr      *conversion,
                                    a_boolean         is_initialization,
                                    an_error_code     incompatible_err,
                                    a_source_position *err_pos)
/*
Convert source_operand to dest_type if that is possible.  If not, issue
incompatible_err at *err_pos.  This routine is used for initialization
(is_initialization == TRUE) and assignment (is_initialization == FALSE).
source_operand may be an rvalue or an lvalue.  On return, it will
always be an rvalue.  If conversion is non-NULL, the conversion
has previously been found to be acceptable, and *conversion
describes it.  dest_type must not be a reference type.
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
                                    is_initialization,
                                    incompatible_err, err_pos,
                                    &conversion,
                                    &local_conversion)) {
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
have been referenced exists and is accessible (ARM 12.6.1).  Issue an error
at *err_pos if not.
*/
{
  a_type_ptr   class_type = skip_typerefs(source_type);
  a_symbol_ptr cctor_sym;
  a_boolean    ambiguous;
  a_boolean    class_bitwise_copy;

  cctor_sym = find_copy_constructor(class_type,
                                    is_const_qualified_type(source_type),
                                    is_volatile_qualified_type(source_type),
                                    &ambiguous, &class_bitwise_copy);
  if (class_bitwise_copy) {
    /* A bitwise copy is allowed, so the "copy constructor" is accessible. */
  } else if (cctor_sym == NULL) {
    if (!ambiguous) {
      /* No applicable copy constructor. */
      pos_ty_error(ec_no_suitable_copy_constructor, err_pos, class_type);
    } else {
      /* More than one applicable copy constructor. */
      pos_ty_error(ec_ambiguous_copy_constructor, err_pos, class_type);
    }  /* if */
  } else {
    if (!have_access_to_symbol(cctor_sym)) {
      if (strict_ansi_mode) {
        pos_sy_diagnostic(strict_ansi_error_severity,
                          ec_inaccessible_special_function,
                          err_pos, cctor_sym);
      } else {
        pos_sy_warning(ec_inaccessible_special_function, err_pos, cctor_sym);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* check_access_to_elided_copy_constructor */


static a_boolean operand_is_temp_init(an_operand *operand)
/*
Return TRUE if the given operand is an expression operand for an enk_temp_init
(which represents an expression temporary).
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
                                  a_boolean          initializing_return_value,
                                  an_expr_node_ptr   *p_temp_init_node,
                                  a_dynamic_init_ptr *p_dip)
/*
Determine whether or not source_operand is an enk_temp_init node that can be
used in a copy constructor elision optimization.  Return TRUE if so, and
also set *p_temp_init_node and *p_dip to the underlying expression node
and dynamic initialization entry.  If initializing_return_value is TRUE,
the initialization being done is for the value returned from a function.
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
    if (initializing_return_value && dip->destructor != NULL) {
      /* In a return, we don't want a dynamic initialization that specifies
         a destructor call (the caller does the destruction);
         clear the destructor field.  Note that even though the destructor
         field is filled in, the destructor routine has not been marked as
         referenced, because we're in a cctor return expression
         (see alloc_dtor_dynamic_init and fix_up_dynamic_init_dtors). */
      dip->destructor = NULL;
    }  /* if */
    is_usable_temp_init = TRUE;
    *p_temp_init_node = temp_init_node;
    *p_dip = dip;
  }  /* if */
  return is_usable_temp_init;
}  /* is_temp_init_usable_in_optimization */


static a_dynamic_init_ptr alloc_dynamic_init_possibly_with_dtor(
                                 a_dynamic_init_kind kind,
                                 a_boolean           initializing_return_value,
                                 a_type_ptr          temp_type,
                                 a_source_position   *position)
/*
Allocate a dynamic initialization entry of type kind and return a pointer
to it.  The entity to be initialized is of type temp_type.  *position
indicates the source position of the initialization.  initializing_return_value
is TRUE if the dynamic initialization is initializing the return value of
a function that returns its value via a copy constructor (the destructor
is suppressed in that case).
*/
{
  a_dynamic_init_ptr dip;

  if (initializing_return_value) {
    /* No destructor call if this is a return statement. */
    dip = alloc_dynamic_init(kind);
  } else {
    dip = alloc_dtor_dynamic_init(kind,
                                  temp_type, curr_expr_is_evaluated(),
                                  (a_boolean)expr_stack->
                                                 in_return_by_cctor_expression,
                                  position);
  }  /* if */
  return dip;
}  /* alloc_dynamic_init_possibly_with_dtor */


static void determine_dynamic_init_for_class_init(
                                  an_operand         *source_operand,
                                  a_type_ptr         dest_type,
                                  a_conv_descr       *conversion,
                                  a_boolean          initializing_return_value,
                                  a_dynamic_init_ptr *p_dip,
                                  an_expr_node_ptr   *p_temp_init_node)
/*
An entity of type dest_type (a class type) is being initialized from
source_operand.  The constructor or conversion function required to do the
copy and/or conversion is given by *conversion.  Create a dynamic
initialization entry to do the initialization (and any required
destruction, unless initializing_return_value is TRUE) and return a
pointer to it in *dip (or return *dip == NULL for an error).  If
p_temp_init_node is non-NULL, create an enk_temp_init node (for the
address of a temporary) pointing to that dynamic initialization entry,
and return a pointer to it in *p_temp_init_node.  dest_type is allowed
to be a class having no constructors at all.  The initialization
represented is an "=" initialization, i.e.,

  dest_type var = source_operand;

If initializing_return_value is TRUE, the initialization is one that
returns a value from a return statement.

This routine does copy constructor elision, i.e., it checks for cases
where a constructor or other routine can be called to generate its
result directly in the entity to be initialized.  If that can be
done, we have in effect optimized out a call of a copy constructor
(i.e., we have elided it).  The language requires that we still check
to see that the copy constructor we would have used exists and is
accessible.

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
    /* The operation is a class bitwise copy.  Do nothing now; the real
       work gets done below. */
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
      if (skip_typerefs(source_operand->type) == class_type) {
        /* The conversion routine is a copy constructor. */
        /* Look at the top of the expression that is the input to the copy
           constructor, to see if it is something that creates a temporary.
           If it is, the temporary and the copy constructor call can be
           optimized away. */
        if (is_temp_init_usable_in_optimization(source_operand,
                                                initializing_return_value,
                                                &temp_init_node,
                                                &dip)) {
          elision_done = TRUE;
          elision_source_type = source_operand->type;
        }  /* if */
      } else {
        /* The conversion routine is a non-copy constructor, so copy
           constructor elision is being done. */
        elision_done = TRUE;
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
      user_convert_operand(source_operand, /*dest_type=*/(a_type_ptr)NULL,
                           conversion);
      /* See if the result of the conversion is already in a temporary. */
      if (is_temp_init_usable_in_optimization(source_operand,
                                              initializing_return_value,
                                              &temp_init_node,
                                              &dip)) {
        elision_done = TRUE;
        elision_source_type = source_operand->type;
      } else {
        /* See if an appropriate copy constructor exists. */
        conversion_routine = select_copy_constructor(
                              class_type,
                              is_const_qualified_type(source_operand->type),
                              is_volatile_qualified_type(source_operand->type),
                              &source_operand->position, class_type,
                              &class_bitwise_copy, curr_expr_is_evaluated(),
                              /*suppress_access_check=*/FALSE);
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
    /* The dynamic initialization entry was already allocated above. */
  } else if (class_bitwise_copy) {
    /* The operation is a class bitwise copy, so use a dik_expression. */
    prep_class_bitwise_copy_operand(source_operand, dest_type);
    dip = alloc_dynamic_init_possibly_with_dtor(
                                          (a_dynamic_init_kind)dik_expression,
                                          initializing_return_value,
                                          class_type,
                                          &source_operand->position);
    dip->variant.expression = make_node_from_operand(source_operand);
  } else if (conversion_routine != NULL) {
    /* conversion_routine is a constructor (copy or other). */
    set_up_for_constructor_call(source_operand, conversion_routine,
                                &arg_expr_list);
    /* Use a dik_constructor entry to call the constructor. */
    dip = alloc_dynamic_init_possibly_with_dtor(
                                          (a_dynamic_init_kind)dik_constructor,
                                          initializing_return_value,
                                          class_type,
                                          &source_operand->position);
    dip->variant.constructor.ptr = conversion_routine;
    dip->variant.constructor.args = arg_expr_list;
  } else {
    /* Some error. */
    dip = NULL;
  }  /* if */
  /* Build an enk_temp_init node if one is needed and one did not exist
     already. */
  if (p_temp_init_node != NULL) {
    if (temp_init_node == NULL) {
      temp_init_node = alloc_temp_init_node(dest_type,
                                            /*result_is_addr=*/TRUE);
      temp_init_node->variant.init.dynamic_init = dip;
    } else {
      /* Existing enk_temp_init; make sure we get the address of the
         temporary instead of its value. */
      if (!temp_init_node->variant.init.result_is_addr) {
        temp_init_node->variant.init.result_is_addr = TRUE;
        temp_init_node->type = make_pointer_type(temp_init_node->type);
      }  /* if */
    }  /* if */
    *p_temp_init_node = temp_init_node;
  }  /* if */
  *p_dip = dip;
}  /* determine_dynamic_init_for_class_init */


void prep_elision_initializer_operand(an_operand         *source_operand,
                                      a_type_ptr         dest_type,
                                      a_dynamic_init_ptr *dip)
/*
An entity of (class) type dest_type is being initialized from source_operand.
Convert it if necessary (issuing an error if the conversion cannot be done),
and build a dynamic initialization entry to describe the initialization.
The dynamic initialization entry will also indicate a destructor if
appropriate.  Return a pointer to that entry in *dip (or NULL for an
error).  source_operand may be changed by this routine.  This routine
is used in both C and C++ mode, but it exists to do copy constructor
elision in C++ mode.
*/
{
  a_conv_descr conversion;

  *dip = NULL;
  /* Look for a constructor to convert the expression to the required
     class type. */
  if (conversion_possible(source_operand, dest_type, dest_type,
                          /*is_initialization=*/TRUE,
                          ec_bad_initializer_type,  /* Arbitrary. */
                          &source_operand->position,
                          &conversion)) {
    /* The conversion is possible.  Determine the routine and argument
       list to return to the caller. */
    determine_dynamic_init_for_class_init(source_operand, dest_type,
                                          &conversion,
                                          /*initializing_return_value=*/FALSE,
                                          dip, (an_expr_node_ptr *)NULL);
  }  /* if */
}  /* prep_elision_initializer_operand */


void temp_init_from_operand(an_operand *operand)
/*
Create an enk_temp_init node that initializes a temporary to a copy of the
indicated operand.  The source operand can be an rvalue or an lvalue.
On return, *operand will have been changed to an rvalue for the address
of the temporary.  Only used in C++ mode.
*/
{
  a_dynamic_init_ptr dip;
  an_expr_node_ptr   temp_init_node;
  a_boolean          cctor_case, class_bitwise_copy;
  a_type_ptr         temp_type;
  a_routine_ptr      cctor_routine;
  an_expr_node_ptr   cctor_arg;
  an_operand         orig_operand;

  orig_operand = *operand;
  /* The temporary has the type of the operand minus type qualifiers. */
  temp_type = skip_typerefs(operand->type);
  cctor_case = FALSE;
  if (is_class_struct_union_type(temp_type)) {
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
                                temp_type,
                                is_const_qualified_type(operand->type),
                                is_volatile_qualified_type(operand->type),
                                &operand->position, temp_type,
                                &class_bitwise_copy, curr_expr_is_evaluated(),
                                /*suppress_access_check=*/FALSE);
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
        set_up_for_constructor_call(operand, cctor_routine, &cctor_arg);
        make_constructor_dynamic_init(cctor_routine, cctor_arg,
                                      /*result_is_addr=*/TRUE,
                                      &orig_operand.position,
                                      operand);
      }  /* if */
    }  /* if */
  }  /* if */
  if (!cctor_case) {
    /* Normal case -- use a dik_expression initialization to copy the
       operand into the temporary. */
    /* Allocate the dynamic initialization entry and the enk_temp_init node. */
    temp_init_node = create_expr_temporary(temp_type, /*result_is_addr=*/TRUE,
                                           curr_expr_is_evaluated(),
                                           (a_boolean)expr_stack->
                                                 in_return_by_cctor_expression,
                                           &operand->position);
    dip = temp_init_node->variant.init.dynamic_init;
    conv_lvalue_to_rvalue(operand);
    set_dynamic_init_kind(dip, (a_dynamic_init_kind)dik_expression);
    dip->variant.expression = make_node_from_operand(operand);
    /* Make an operand for the overall expression. */
    make_expression_operand(temp_init_node, temp_init_node->type, operand);
  }  /* if */
  /* Restore the original source position, etc. */
  restore_operand_details(operand, &orig_operand);
}  /* temp_init_from_operand */


static void convert_operand_into_temp(an_operand    *source_operand,
                                      a_type_ptr    dest_type,
                                      a_type_ptr    orig_dest_type,
                                      a_conv_descr  *conversion,
                                      an_error_code incompatible_err,
                                      a_boolean     *err,
                                      a_boolean     *temporary_used)
/*
Convert source_operand to dest_type, put it into a newly-created temporary,
and return an rvalue for the address of the temporary in source_operand.
If the conversion is not possible, issue the error incompatible_err,
convert source_operand to an error operand, and return *err TRUE.
If a temporary is created or source_operand is already a temporary,
return *temporary_used TRUE.  orig_dest_type is the destination type
before any rewriting, for use in error messages.  If conversion
is non-NULL, the conversion is already known to be possible, and
*conversion describes it.  This routine is used to convert the
initial value in a reference initialization to a temporary that
the reference will point to.  dest_type must not be a reference type.
Only used in C++.
*/
{
  a_conv_descr  local_conversion;
  a_routine_ptr conversion_routine;
  an_operand    orig_operand;
  a_boolean     have_temp;

  *err = FALSE;
  *temporary_used = FALSE;
  orig_operand = *source_operand;
#if CHECKING
  if (is_reference_type(dest_type)) {
    internal_error("convert_operand_into_temp: dest_type is reference");
  }  /* if */
#endif /* CHECKING */
  /* See if the conversion is possible. */
  if (conversion_usable_or_possible(source_operand, dest_type, orig_dest_type,
                                    /*is_initialization=*/TRUE,
                                    incompatible_err,
                                    &source_operand->position,
                                    &conversion,
                                    &local_conversion)) {
    /* Yes, the conversion is possible.  Do it. */
    convert_operand(source_operand, dest_type, conversion);
    /* In some cases, the result is already in something that can be
       considered a temporary. */
    have_temp = FALSE;
    if (operand_is_temp_init(source_operand)) {
      /* The conversion routine returns its value into a temporary, so
         we already have a temporary. */
      have_temp = TRUE;
      *temporary_used = TRUE;
    }  /* if */
    if (!have_temp) {
      conversion_routine = conversion->routine;
      if (conversion_routine != NULL &&
          conversion_routine->special_kind ==
                                     (a_special_function_kind)sfk_conversion) {
        /* The conversion is done by a conversion function. */
        a_type_ptr routine_type = skip_typerefs(conversion_routine->type);
        if (is_reference_type(routine_type->variant.routine.return_type)) {
          /* The conversion function returns a reference, so there is
             already something we can point to.  This is not a temporary,
             but it can be used directly.  For example:
               struct B { B(const B&); };
               struct A {
                 operator B&();
               } a;
               const B& x = a;  // No temp needed
          */
          have_temp = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
    if (have_temp && is_class_struct_union_type(source_operand->type)) {
      /* The result of the conversion is already a class temporary.
         Convert the operand from the value of the temporary to the
         address. */
      conv_class_operand_to_object_pointer(source_operand);
      /* Handle base class casts. */
      cast_operand(make_pointer_type(dest_type), source_operand,
                                     /*is_implicit_cast=*/TRUE);
    } else if (have_temp && is_an_lvalue(source_operand)) {
      /* The result of the conversion is already a non-class temporary
         that is an lvalue (in particular, this includes array lvalues).
         Convert to a pointer to the lvalue. */
      /* Note that if a standard conversion is needed after the
         conversion function, source_operand has been converted to
         an rvalue. */
      take_address_of_lvalue(source_operand);
    } else {
      /* Initialize a temporary with the converted value. */
      temp_init_from_operand(source_operand);
      *temporary_used = TRUE;
    }  /* if */
  } else {
    /* The conversion is not possible.  The error has already been issued. */
    *err = TRUE;
  }  /* if */
  /* Restore the original source position, etc. */
  restore_operand_details(source_operand, &orig_operand);
}  /* convert_operand_into_temp */


static a_boolean is_field_selection_lvalue_operand(an_operand *operand)
/*
Return TRUE if the operand is a field selection lvalue.  This is used for a
limited loophole allowed in cfront compatibility mode.
*/
{
  a_boolean is_field_selection = FALSE;

  if (is_expression_operand(operand) && is_an_lvalue(operand)) {
    an_expr_node_ptr node = operand->variant.expression;
    if (is_operation_node(node)) {
      if (node->variant.operation.kind == (an_expr_operator_kind)eok_field) {
        is_field_selection = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return is_field_selection;
}  /* is_field_selection_lvalue_operand */


void prep_initializer_operand(an_operand    *source_operand,
                              a_type_ptr    dest_type,
                              a_conv_descr  *conversion,
                              a_boolean     initializing_return_value,
                              an_error_code incompatible_err)
/*
Check the operand for initializer compatibility against the type supplied.
Cast the operand if required to make it the right type.  Convert the
operand from an lvalue to an rvalue if necessary (it usually is).
initializing_return_value is TRUE if the initialization is being done
to return a value in a return statement.  If the operand and type are
incompatible, issue the error incompatible_err.  This routine is used for
initialization, function call arguments, and return expressions, i.e.,
for "="-type initializations.  It is not used when copy constructor
elision is possible; see prep_elision_initializer_operand.
If conversion is non-NULL, the initializer has previously been
found to be acceptable, and *conversion describes it.
*/
{
  a_type_ptr base_dest_type, base_source_type;
  a_type_ptr unqual_dest_type, unqual_source_type;
  a_boolean  type_is_correct_or_derived, err = FALSE, dropping_qualifiers;
  a_boolean  ref_to_const, temporary_used, warn = FALSE;
  an_operand orig_operand;

  orig_operand = *source_operand;
  base_source_type = source_operand->type;
  if (is_error_operand(source_operand)) {
     /* Previous error.  Leave the operand alone. */
  } else if (is_reference_type(dest_type)) {
    /* When initializing a reference T&, there are two cases (ARM 8.4.3):
         (1)  If the initializer is an lvalue of type T or of
              a type derived from T for which T is an accessible base,
              the initialization is done directly;
         (2)  Otherwise, the reference must be const.  A temporary of
              type T is created and initialized with the initializer,
              and the reference points to the temporary.
    */
    base_dest_type = type_pointed_to(dest_type);
    /* The "unqual" types are the unqualified versions of the base types,
       except that array types can still be qualified down at the element
       level. */
    unqual_source_type = skip_typerefs(base_source_type);
    unqual_dest_type = skip_typerefs(base_dest_type);
    /* See if the types are correct without conversion. */
    type_is_correct_or_derived = FALSE;
    if (types_are_compatible_ignoring_qualifiers(unqual_dest_type,
                                                 unqual_source_type)) {
      /* The type is correct, ignoring (first-level) qualifiers.
         Note that this handles qualified array cases. */
      type_is_correct_or_derived = TRUE;
    } else if (is_class_struct_union_type(unqual_dest_type) &&
               is_class_struct_union_type(unqual_source_type) &&
               find_base_class_of(unqual_source_type,
                                  unqual_dest_type) != NULL) {
      /* The initializer has a derived type. */
      type_is_correct_or_derived = TRUE;
    } else if (any_cfront_mode() &&
               is_pointer_type(unqual_dest_type) &&
               is_pointer_type(unqual_source_type) &&
               same_type_with_added_qualifiers(unqual_dest_type,
                                               unqual_source_type,
                                               /*ignore_qualifiers=*/FALSE)) {
      /* The type is a pointer type and is correct, except that the
         destination type has some qualifiers that are not present on
         the source type (at any level).  Standard C++ processing can
         only add type qualifiers at the top level. */
      type_is_correct_or_derived = TRUE;
    }  /* if */
    /* Determine whether or not the reference is to a const type. */
    ref_to_const = is_const_qualified_type(base_dest_type);
    /* The destination type must have no fewer type qualifiers than the source
       type to be usable without conversion (ARM 8.4.3). */
    dropping_qualifiers = type_is_correct_or_derived &&
                          any_qualifier_missing(base_dest_type,
                                                base_source_type);
    if (dropping_qualifiers) {
      /* There are fewer qualifiers on the destination than on the source,
         so the initialization would involve dropping qualifiers. */
      /* cfront makes a field selected from a const structure compatible
         with a non-const reference to the underlying type:
           struct A {};
           struct B {
             A a;
             B() {}
           };
           const B bb;
           A &r = bb.a;  // okay according to cfront, no warning
           const B *pb;
           A &rr = pb->a;  // okay according to cfront, warning
      */
      if (cfront_2_1_mode &&
          !ref_to_const && is_const_qualified_type(base_source_type) &&
          is_field_selection_lvalue_operand(source_operand)) {
        /* Okay.  Note that a temporary will not be used in these cases. */
        pos_warning(ec_cfront_nonconst_ref_init, &source_operand->position);
        dropping_qualifiers = FALSE;
      } else {
        /* Qualifiers are being dropped. */
        type_is_correct_or_derived = FALSE;
      }  /* if */
    }  /* if */
    if (type_is_correct_or_derived && ref_to_const &&
        is_bit_field_operand(source_operand)) {
      /* For a bit-field case like
           struct A { int i:2; } a;
           const int &r = a.i;
         force the use of a temporary.  This is not covered by the ARM
         but it makes sense and cfront does it that way.  Note that in
         the ref to nonconst case we leave the operand as it is to get
         a more specific error message about taking the address of a
         bit field. */
      conv_lvalue_to_rvalue(source_operand);
    }  /* if */
    if (type_is_correct_or_derived && is_an_lvalue(source_operand)) {
      /* The initial value is an lvalue of the right type; the initialization
         can be done directly. */
      /* Convert the lvalue to an rvalue pointer to the object. */
      take_address_of_lvalue(source_operand);
      if (is_constant_operand(source_operand) &&
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
      }  /* if */
      /* Use a pointer type instead of a reference type on the
         destination. */
      dest_type = make_pointer_type(base_dest_type);
      /* Cast the operand to the result type. */
      cast_operand(dest_type, source_operand, /*is_implicit_cast=*/TRUE);
    } else if (type_is_correct_or_derived &&
               is_a_function_designator(source_operand)) {
      /* The initial value is a function designator of the right type;
         the initialization can be done directly. */
      conv_function_designator_to_ptr_to_function(source_operand);
    } else if (type_is_correct_or_derived &&
               is_class_struct_union_type(base_dest_type)) {          
      /* The source is a class rvalue but otherwise has the right type.
         No temporary is required.  Get the address of the rvalue, then
         cast the pointer to the right type to handle the derived-class
         case. */
      conv_class_operand_to_object_pointer(source_operand);
      /* Use a pointer type instead of a reference type on the
         destination. */
      dest_type = make_pointer_type(base_dest_type);
      cast_operand(dest_type, source_operand, /*is_implicit_cast=*/TRUE);
      if (!ref_to_const) {
        /* This is a reference to non-const initialized from a class rvalue
           of the right type.  According to the ARM (8.4.3), this is an error.
           We allow it as an extension. */
        if (strict_ansi_mode) {
          pos_diagnostic(strict_ansi_error_severity,
                         ec_nonconst_ref_init_from_rvalue,
                         &source_operand->position);
        }  /* if */
      }  /* if */
    } else {
      /* The initialization cannot be done directly; a temporary must be
         used and/or an implicit conversion must be done. */
      if (curr_expr_kind_is_const()) {
        /* In a constant context (e.g., a nontype template argument),
           a temporary or conversion is not allowed. */
        error_in_operand(ec_init_needing_temp_not_allowed, source_operand);
      } else if (dropping_qualifiers) {
        /* Type qualifiers were dropped (and otherwise the type is okay).
           Note that testing this early means that an implicit conversion
           cannot be used to drop the qualifiers. */
        error_in_operand(ec_qualifier_dropped_in_ref_init, source_operand);
      } else {
        /* Allocate a temporary and copy the operand into it, converting
           if necessary.  source_operand is set to the address of the
           temporary. */
        /* The temp has the same type as the operand, but without
           type qualifiers. */
        convert_operand_into_temp(source_operand, unqual_dest_type, dest_type,
                                  conversion, incompatible_err, &err,
                                  &temporary_used);
        if (err) {
          /* The conversion could not be done.  An error has already been
             issued. */
        } else if (!temporary_used) {
          /* The conversion is doable and does not requires a temporary
             (e.g., it uses a conversion function that returns a reference). */
        } else if (!ref_to_const) {
          /* A reference to non-const is initialized in a way that requires a
             temporary.  This is an error according to the ARM (8.4.3),
             but we allow it as an anachronism. */
          if (allow_anachronisms) {
            pos_diagnostic(anachronism_error_severity,
                           ec_nonconst_ref_init_anachronism,
                           &source_operand->position);
            if (anachronism_error_severity == es_error) {
              err = TRUE;
            } else {
              warn = TRUE;
            }  /* if */
          } else {
            /* Anachronism is not allowed. */
            /* Use a different message for the case where the type is right but
               the operand is an rvalue (only non-class cases of that come
               here). */
            error_in_operand(type_is_correct_or_derived ?
                               ec_nonconst_ref_init_from_rvalue :
                               ec_bad_nonconst_ref_init,
                             source_operand);
            err = TRUE;
          }  /* if */
        }  /* if */
        if (!err && initializing_return_value) {
          /* A temporary should not be created to return a value, since
             what would happen immediately is that the address of the
             (stack-based) temporary would be returned to the caller. */
          pos_warning(ec_return_ref_init_requires_temp,
                      &source_operand->position);
          warn = TRUE;
        }  /* if */
        if (!err && !warn && temporary_used) {
          /* Let the user know a temp was used. */
          pos_remark(ec_temp_used_for_ref_init, &source_operand->position);
        }  /* if */
      }  /* if */
    }  /* if */
  } else {
    /* Normal case (not initializing a reference). */
    prep_conversion_operand(source_operand, dest_type, conversion,
                            /*is_initialization=*/TRUE,
                            incompatible_err,
                            &source_operand->position);
  }  /* if */
  /* Restore the original source position, etc. */
  restore_operand_details(source_operand, &orig_operand);
}  /* prep_initializer_operand */


void prep_argument_operand(an_operand       *source_operand,
                           a_param_type_ptr formal_param,
                           a_conv_descr     *conversion,
                           an_error_code    err_code)
/*
Check that *source_operand is acceptable as an actual argument for the
formal parameter described by formal_param.  If not, issue the error err_code.
If so, convert the operand to the formal parameter type.
If conversion is non-NULL, the argument has previously been found
to be acceptable, and *conversion describes it.
*/
{
  a_conv_descr       local_conversion;
  an_expr_node_ptr   temp_init_node;
  a_dynamic_init_ptr dip;

  if (formal_param->passed_via_copy_constructor) {
    /* Argument is initialized by a copy constructor. */
    /* See if the conversion is possible. */
    if (conversion_usable_or_possible(source_operand, formal_param->type,
                                      formal_param->type,
                                      /*is_initialization=*/TRUE,
                                      err_code, &source_operand->position,
                                      &conversion,
                                      &local_conversion)) {
      /* Yes.  Build an enk_temp_init node and a dynamic init entry that
         will initialize the temporary.  The temporary's address is passed
         to the called routine. */
      determine_dynamic_init_for_class_init(source_operand, formal_param->type,
                                            conversion,
                                           /*initializing_return_value=*/FALSE,
                                            &dip, &temp_init_node);
      make_expression_operand(temp_init_node, temp_init_node->type,
                              source_operand);
    }  /* if */
  } else {
    /* Normal argument. */
    prep_initializer_operand(source_operand, formal_param->type,
                             conversion,
                             /*initializing_return_value=*/FALSE,
                             err_code);
  }  /* if */
}  /* prep_argument_operand */


void prep_return_by_cctor_operand(an_operand         *source_operand,
                                  a_type_ptr         required_type,
                                  an_error_code      err_code,
                                  a_dynamic_init_ptr *dip)
/*
*source_operand is an operand for the expression of a return statement
in a routine that returns its value via a copy constructor.  The function
return type is required_type.  Build a dynamic initialization entry
to do the return, and return a pointer to it in *dip (or NULL if there is
an error).  err_code is the error code to be used in case of error.
*/
{
  an_operand   orig_operand;
  a_conv_descr conversion;

  orig_operand = *source_operand;
  *dip = NULL;
  /* See if the conversion is possible. */
  if (conversion_possible(source_operand, required_type, required_type,
                          /*is_initialization=*/TRUE,
                          err_code, &source_operand->position,
                          &conversion)) {
    /* Yes.  Build the dynamic init entry. */
    determine_dynamic_init_for_class_init(source_operand, required_type,
                                          &conversion,
                                          /*initializing_return_value=*/TRUE,
                                          dip, (an_expr_node_ptr *)NULL);
  }  /* if */
  /* Restore the original source position, etc. */
  restore_operand_details(source_operand, &orig_operand);
}  /* prep_return_by_cctor_operand */


void prep_assignment_operand(an_operand        *source_operand,
                             a_type_ptr        dest_type,
                             an_error_code     incompatible_err,
                             a_source_position *err_pos)
/*
Check the operand for assignment compatibility against the type supplied.
Cast the operand if required to make it the right type.  The operand is
an rvalue.  If the operand and type are incompatible, issue the error
incompatible_err at position *err_pos.  Note that for classes in C++, this
routine is only called for cases where bitwise copying applies.
*/
{
  /* See if the source and destination types are compatible, and convert the
     source operand to the destination type. */
  prep_conversion_operand(source_operand, dest_type,
                          (a_conv_descr_ptr)NULL,
                          /*is_initialization=*/FALSE,
                          incompatible_err, err_pos);
}  /* prep_assignment_operand */


void overload_init(void)
/* 
Initialize things related to overload resolution in expression scanning.
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
* Copyright 1988-1993 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
